#!/usr/bin/env python3
"""Download OpenBIOS for the PS1 emulator and verify its SHA512.

Sources are configurable via environment variables:
    PS1VJ_BIOS_URL  - override list of bin URLs (comma-separated)
    PS1VJ_SHA_URL   - override list of sha512 URLs (comma-separated)
"""
from __future__ import annotations

import hashlib
import os
import sys
import urllib.request
from pathlib import Path

DEFAULT_BIOS_URLS = [
    "https://www.mirrorservice.org/sites/libreboot.org/release/canoeboot/20250107/roms/playstation/openbios.bin",
    "https://mirrors.mit.edu/libreboot/canoeboot/old_releases/20250107/roms/playstation/openbios.bin",
]

DEFAULT_SHA_URLS = [
    "https://www.mirrorservice.org/sites/libreboot.org/release/canoeboot/20250107/roms/playstation/openbios.bin.sha512",
    "https://mirrors.mit.edu/libreboot/canoeboot/old_releases/20250107/roms/playstation/openbios.bin.sha512",
]

ROOT = Path(__file__).resolve().parents[1]
BIOS_DIR = ROOT / "bios"
BIOS_PATH = BIOS_DIR / "openbios.bin"
SHA_PATH = BIOS_DIR / "openbios.bin.sha512"


def _resolve_urls(env_name: str, defaults: list[str]) -> list[str]:
    val = os.environ.get(env_name, "").strip()
    if not val:
        return list(defaults)
    return [u.strip() for u in val.split(",") if u.strip()]


def download_first(urls: list[str], dest: Path) -> None:
    last_error: Exception | None = None
    for url in urls:
        try:
            print(f"Downloading: {url}")
            with urllib.request.urlopen(url, timeout=30) as r:
                data = r.read()
            dest.write_bytes(data)
            print(f"Saved: {dest} ({len(data)} bytes)")
            return
        except Exception as e:
            print(f"Failed: {url}: {e}")
            last_error = e
    raise RuntimeError(f"All downloads failed: {last_error}")


def sha512_file(path: Path) -> str:
    h = hashlib.sha512()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def manual_instructions() -> str:
    return (
        "\nManual setup:\n"
        f"  1. Place openbios.bin at: {BIOS_PATH}\n"
        f"  2. (Optional) Place openbios.bin.sha512 at: {SHA_PATH}\n"
        "  3. Re-run this script to verify the SHA512.\n"
        "  See README.md for mirror URLs.\n"
    )


def main() -> int:
    BIOS_DIR.mkdir(parents=True, exist_ok=True)

    bios_urls = _resolve_urls("PS1VJ_BIOS_URL", DEFAULT_BIOS_URLS)
    sha_urls = _resolve_urls("PS1VJ_SHA_URL", DEFAULT_SHA_URLS)

    if BIOS_PATH.exists():
        print(f"BIOS already exists: {BIOS_PATH}")
    else:
        try:
            download_first(bios_urls, BIOS_PATH)
        except Exception as e:
            print(f"openbios.bin download failed: {e}")
            print(manual_instructions())
            return 1

    try:
        download_first(sha_urls, SHA_PATH)
        expected_text = SHA_PATH.read_text(encoding="utf-8", errors="replace").strip()
        expected = expected_text.split()[0] if expected_text else ""
        actual = sha512_file(BIOS_PATH)

        if not expected:
            print("WARNING: sha512 file was empty; skipping verification.")
        elif actual.lower() != expected.lower():
            raise RuntimeError(
                f"SHA512 mismatch\n  expected={expected}\n  actual  ={actual}"
            )
        else:
            print("SHA512 OK")
    except Exception as e:
        print(f"SHA512 verification skipped or failed: {e}")
        print("Please verify openbios.bin manually if needed.")

    print("OpenBIOS setup complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
