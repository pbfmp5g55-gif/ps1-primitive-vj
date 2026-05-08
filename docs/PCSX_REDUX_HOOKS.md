# PCSX-Redux hook 候補レポート

調査対象: `grumpycoders/pcsx-redux@main`(2026-05-08 時点 shallow clone)
依拠仕様: `../../ps1_primitive_vj_design.md` §15

## 結論

**第一候補(§15.2 の "GPU command → renderer primitive 変換直後")を採用**。
`src/core/gpu.cc` の 3 箇所(Poly / Line / Rect の `processWrite`)に
`addNode()` と `write0()` の間で1行ずつ挿入する。

## アーキテクチャ理解

### Logged 階層

`PCSX::GPU` 内に `struct Logged`(基底)があり、
全 GPU コマンドのライフサイクル単位がここから派生する(`gpu.h:156`)。

primitive を生む派生クラスは:

| 派生クラス | 仕様書の libvj 概念 | 場所 |
|---|---|---|
| `Poly<Shading, Shape, Textured, Blend, Modulation>` | 三角/四角ポリゴン | `gpu.h:527` |
| `Line<Shading, LineType, Blend>` | ライン | `gpu.h:594` |
| `Rect<Size, Textured, Blend, Modulation>` | スプライト/矩形 | `gpu.h:646` |
| `FastFill` | 矩形塗り(高速)| `gpu.h:311` |

`Logged` に `execute(GPU*)` 仮想関数があり、
具象クラスは `gpu->write0(this)` を呼んで renderer 側にディスパッチする(`gpu.h:305`他)。

### dispatch 層

`PCSX::GPU` には primitive 型ごとの `virtual void write0(...)` が網羅的に並ぶ(`gpu.h:883-`)。
`OpenGL_GPU/gpu_opengl.cc` と `gpu/soft/gpu.cc` が **renderer ごとにこれを override**。
=== ここに分岐の壁 ===

つまり `write0` 直前に介入すれば **renderer 非依存で全 primitive を捕捉できる**。

### addNode = 既存の傍受機構

`g_emulator->m_gpuLogger->addNode(*this, origin, value, length)` が
**既に PCSX-Redux 内で全 primitive 投入前に呼ばれている**(`gpulogger.h:42` テンプレート)。
GPULogger は debug overlay 用に primitive を蓄積するためのもので、
**libvj が乗るべきレールはこれと同じ場所**。

## 推奨 hook 挿入点

`src/core/gpu.cc` の 3 箇所、**現状こうなっている**:

```cpp
// Poly::processWrite (gpu.cc:97-98 付近)
g_emulator->m_gpuLogger->addNode(*this, origin, origvalue, length);
m_gpu->write0(this);

// Line::processWrite (gpu.cc:155 付近)
g_emulator->m_gpuLogger->addNode(*this, origin, origvalue, length);

// Rect::processWrite (gpu.cc:223 付近)
g_emulator->m_gpuLogger->addNode(*this, origin, origvalue, length);
m_gpu->write0(this);
```

**ここに 1 行ずつ挟む**:

```cpp
g_emulator->m_gpuLogger->addNode(*this, origin, origvalue, length);
if (g_emulator->m_vjInterceptor) {
    g_emulator->m_vjInterceptor->intercept(*this);   // ← 追加
}
m_gpu->write0(this);
```

`intercept()` の中で:
1. PCSX 側の `Poly<...>` 等を libvj の `vj::Primitive` に詰め替える
2. `vj::PrimitiveInterceptor::onPrimitive()` を呼んで判定/改変させる
3. 改変後の値を **元の `Poly<...>` メンバに書き戻す**(`x[]/y[]/u[]/v[]/colors[]`)
4. drop 判定なら `write0` を呼ばせない(`return false` などで親側で skip)

drop を実現したい場合は挿入点を:

```cpp
g_emulator->m_gpuLogger->addNode(*this, origin, origvalue, length);
if (g_emulator->m_vjInterceptor && !g_emulator->m_vjInterceptor->intercept(*this)) {
    // skipped this frame
} else {
    m_gpu->write0(this);
}
```

としたほうが綺麗。

## libvj `Primitive` への詰め替えスキーマ

PCSX 側の field と libvj の対応(仕様書 §6 / `vj/include/vj/Primitive.h` 参照):

| libvj `Primitive` member | PCSX 側 source |
|---|---|
| `vertices[i].x, .y` | `Poly::x[i] + offset.x`, `Poly::y[i] + offset.y` |
| `vertices[i].u, .v` | `Poly::u[i], Poly::v[i]`(`textured==Yes` のみ)|
| `vertices[i].color` | `Poly::colors[i]` |
| `vertexCount` | `Poly::count`(Tri=3, Quad=4)、Line は `x.size()`、Rect は固定2/4|
| `textured` | テンプレ引数 `Textured::Yes ? true : false` |
| `semiTransparent` | `Blend::Semi ? true : false` |
| `shape` | Poly→Polygon, Line→Line, Rect→Sprite |
| `hostTag` | 下記参照 |

## `hostTag` (uint64) パッキング案

`Primitive::hostTag` は libvj 側で opaque な per-primitive renderer state round-trip 用。
PCSX-Redux 用の packing 案:

```
bit  0..23 : tpage.raw      (TPage 内 24bit field)
bit 24..43 : twindow.raw    (TWindow 内 20bit field)
bit 44..59 : clutraw        (CLUT 16bit)
bit 60..62 : shape tag      (0=Poly, 1=Line, 2=Rect, 3=FastFill)
bit 63     : reserved
```

`intercept()` 内で詰め、`write0()` 直前に取り出して `Poly::tpage` 等に書き戻す
(libvj が明示的に書き換えていなければ no-op)。

## ライフサイクル(beginFrame / endFrame) — 調査完了 2026-05-08

### 結論: `Events::GPU::VSync` を EventBus 経由で listen する

PCSX-Redux には既に **EventBus に lifecycle イベントが流れている**。これに乗るのが
最もクリーン(GPULogger も同じ手法)。

### Event 定義

`src/core/system.h:70-72`:

```cpp
namespace Events {
    namespace GPU {
        struct VSync {};
    }
}
```

### Event 発火点

`src/core/psxemulator.cc:177-181`:

```cpp
void PCSX::Emulator::vsync() {
    m_gpu->vblank();                                            // renderer present
    g_system->m_eventBus->signal<Events::GPU::VSync>({});       // ← 我々が listen するイベント
    g_system->update(true);
}
```

**重要**: VSync signal は **renderer の `vblank()` (=present) の後** に発火する。
つまり「フレームが完成して画面に出た直後」のタイミング。

### libvj 側のマッピング

| libvj ライフサイクル | 呼ぶタイミング |
|---|---|
| `interceptor->beginFrame(params, estPrimCount)` | サブシステム初期化時に1回 + VSync イベント毎 |
| `interceptor->interceptAndSubmit(prim)` | gpu.cc の primitive hook 3 箇所 |
| `interceptor->endFrame()` | VSync イベント毎(beginFrame の直前) |

VSync ハンドラ内擬似コード:

```cpp
m_listener.listen<Events::GPU::VSync>([this](auto event) {
    g_vjInterceptor->endFrame();                                // 旧フレーム終了
    Params p = pollMidi();                                      // 最新 MIDI スナップショット
    g_vjInterceptor->beginFrame(p, m_lastFramePrimCount);       // 新フレーム開始
    m_lastFramePrimCount = m_thisFramePrimCount;                // 計測値繰り越し
    m_thisFramePrimCount = 0;
});
```

### depth delay の意味付けの注意

VSync は present の **後** に発火するので、`endFrame()` で flush される
DepthDelayQueue 内の primitive は **次フレーム** の描画キューに乗る(depth delay の本来の挙動と一致)。
仕様書 §10.4 の「depth として primitive submission 連番を使う」案と整合。

### 既存リスナーとの並走

VSync を listen している既存箇所(参考):

- `src/core/gpulogger.cc:64` — frame counter +1
- `src/gui/widgets/memory_observer.cc:36` — メモリ可視化更新
- `src/core/eventslua.cc:189` — Lua bindings 経由

新規追加する libvj リスナーは独立、相互干渉なし。

### Phase 4 着手時の具体作業

1. `psxemulator.h` に `std::unique_ptr<vj::PrimitiveInterceptor> m_vjInterceptor` 追加
2. `psxemulator.h` に `EventBus::Listener m_vjListener` 追加 (or `vj_bridge.cc` 内に static で持つ)
3. `psxemulator.cc` のコンストラクタで `m_vjInterceptor = std::make_unique<vj::PrimitiveInterceptor>();`
4. 同 `m_vjListener.listen<Events::GPU::VSync>(...)` で endFrame/beginFrame 配線
5. `vj_bridge.cc` を Phase 3 logging 実装から Phase 4 PCSX→vj 詰め替え実装に差し替え
6. 各 `onPrimitive(*this)` 呼び出し点で interceptor を経由して書き戻し

## VJ サブシステムのインスタンス化箇所

GPULogger は `PCSX::Emulator` のメンバ `m_gpuLogger`(`unique_ptr` 多分)として
ライフタイム管理されている。同じパターンで `m_vjInterceptor` を追加するのが自然:

- `src/core/psxemulator.h` に `std::unique_ptr<vj::PrimitiveInterceptor> m_vjInterceptor;` を追加
- `src/core/psxemulator.cc` のコンストラクタで `make_unique` する
- 設定で有効化/無効化できるようにフラグ追加(`m_vjEnabled`)

## OpenGL renderer 側での補助介入(オプション)

第二候補(§15.2 の "OpenGL draw 直前")も併用可能:
`gpu_opengl.cc:write0(Poly<...>*)` の各 override 内で、
シェーダー uniform に MASTER 値を渡してフラグメント側で色化けエフェクトをかける、
等の用途に使える。ただし最初の実装フェーズではスキップしてよい。

## 開いている疑問

1. **drop した primitive の補完**: `write0` を呼ばない場合、
   m_gpuLogger には既に登録済 → debug overlay 上で消えた primitive が
   "実行された" と見える。これは GPULogger の `node->enabled` を false にすれば回避可能?
   (`Logged::enabled` フィールドあり、`gpu.h:183`)
2. **DepthDelayQueue の "depth"** に PS1 の何を割り当てるか: PS1 GPU はZ無し、
   FIFO 順しかない。「primitive submission の連番」を depth として使うのが妥当。
3. **PS1 内部の matrix/coord 系**: 仕様書 §6.4 で「PS1 native: 11 bit signed range」と
   書いてある通り、`Poly::x[i]` は既に `signExtend<int, 11>` 通過済 11bit signed。
   libvj 側の geometry shift もこの range を超えないようにクランプする必要あり。

## 次アクション

1. PCSX-Redux を fork(GitHub 上で grumpycoders/pcsx-redux → 自アカウント)
2. fork を `external/pcsx-redux` に再 clone(authenticated remote 付き)
3. `third_party/libvj` として本リポを submodule で取り込む
4. `src/core/gpu.cc` の 3 箇所に hook 1 行追加 + `intercept()` の橋渡し実装
5. `psxemulator.{h,cc}` に `m_vjInterceptor` メンバ追加
6. 最初は **MASTER=0、すべて pass-through** で動作確認(本来の挙動から逸脱しない)
7. その後 GEOMETRY だけ有効化 → 頂点ずらしが画面に出ることを確認
