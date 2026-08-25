# 作業継続メモ

## 現状

**ここに書いてある「次フェーズ」は 2026-05-13 までに全部終わっている。**
プロジェクト全体の現在地と再開手順は
[ps1-vj-mix の HANDOVER_ja.md](https://github.com/pbfmp5g55-gif/ps1-vj-mix/blob/main/HANDOVER_ja.md) を見ること。
このファイルは libvj 単体のビルド環境メモとして残している。

仕様書 `../ps1_primitive_vj_design.md` をもとに、`vj/` サブシステム一式を生成済み。
**ホストビルド検証完了**(2026-05-08)。`vj_demo.exe` が 60 フレーム回って
DebugOverlay の出力(MASTER/CHANCE/各エフェクト確率/CC値)が正常に出ることを確認。
Python script (`scripts/fetch_openbios.py`) も `python -m py_compile` 通過済み。

## ホスト toolchain (このPCでの構成)

- CMake 4.3.2 (winget `Kitware.CMake`、マシン範囲インストール)
- GCC 16.1.0 MinGW-w64 UCRT POSIX SEH (WinLibs ZIP を直 DL → 展開)
  - 場所: `C:\Users\yuho_shinkawa\AppData\Local\Programs\mingw64\bin`
  - User PATH に追加済み(新セッションは自動で拾う)
- WinLibs には CMake/Ninja/clang も同梱、必要なら `mingw64\bin` の方を優先

ビルドコマンド:
```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/vj_demo.exe
```

実行時: `mingw64\bin` が PATH に居ないと `libstdc++-6.dll` 等が
解決できず exit 0xC0000135 になる。User PATH 追加済みなので新規シェルはOK。

## 次フェーズ(2026-05-13 に全て完了済み・記録として残す)

仕様書 §15 の **PCSX-Redux fork 取り込み**:

1. PCSX-Redux を fork → 既存の `ps1-primitive-vj/` を `third_party/libvj/` 等として組み込む → 済
2. PCSX-Redux 内のプリミティブ送出箇所に `PrimitiveInterceptor::onPrimitive` を挟む → 済
3. host renderer 側で `Primitive::hostTag` を round-trip させる
   (TPage / CLUT / blend mode を opaque uint64 に詰めて取り出す) → 済
4. RtMidi 統合 (`RtMidiController`) → 済(`-DVJ_ENABLE_RTMIDI=ON` で有効)

## 既知の注意点

- `DepthDelayQueue::tickAndFlush` は deque 中間 erase の iterator 無効化を避けるため、
  新 deque へ移し替える方式に書き換え済み。
- `lowMasterSafety` は `beginFrame` で master に乗算して適用している。
  仕様書は「全体をさらに弱める」としか書いていないので、master 自体に効かせると
  すべてのエフェクトが連動して弱まる。意図と違ったら個別係数化に変更すること。
- `DebugOverlay.cpp` は `%zu` を使用。MinGW UCRT 16.1 では問題なし。MSVC 旧版では `%llu` 推奨。
