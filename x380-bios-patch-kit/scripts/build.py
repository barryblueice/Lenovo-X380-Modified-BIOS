#!/usr/bin/env python3
"""Build a patched ThinkPad X380 Yoga (R0SET57W / BIOS 1.41) image.

Usage:
    python build.py --input a.bin --mode both
    python build.py --input a.bin --mode wwan --out-dir dist

The script validates the image, applies the patch set with UEFIPatch,
optionally fixes the LNVBB marker, and writes a manifest with hashes.
It never flashes anything.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent
DEFAULT_UEFIPATCH = KIT / "tools" / "UEFIPatch_0.28.0" / "UEFIPatch.exe"
IMAGE_SIZE = 16 * 1024 * 1024
FD_SIGNATURE = bytes.fromhex("5AA5F00F")
BIOS_ID_RE = re.compile(rb"R0SET[0-9A-Z]{2}W")
EXPECTED_BIOS_ID = "R0SET57W"

MODES = {
    # mode: (patch file, label, expected number of replacements)
    "both": ("x380_custom_patches.txt", "adv-wwan", 2),
    "adv": ("adv_only.txt", "adv-only", 1),
    "wwan": ("wwan_only.txt", "wwan-only", 1),
}


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description="Build patched X380 Yoga BIOS image")
    parser.add_argument("--input", required=True, help="16 MB SPI dump")
    parser.add_argument("--mode", choices=sorted(MODES), default="both",
                        help="which patches to apply (default: both)")
    parser.add_argument("--out-dir", default=str(KIT / "dist"),
                        help="output directory (default: <kit>/dist)")
    parser.add_argument("--uefipatch", default=str(DEFAULT_UEFIPATCH),
                        help="path to UEFIPatch.exe")
    parser.add_argument("--lnvbb", choices=("auto", "off"), default="auto",
                        help="auto: replace 'LNVBBSEC'+FB with FF if present; off: never touch")
    parser.add_argument("--force", action="store_true",
                        help="continue even if size/BIOS ID does not match")
    args = parser.parse_args()

    src = Path(args.input).expanduser().resolve()
    if not src.is_file():
        fail(f"input not found: {src}")

    data = src.read_bytes()
    if len(data) != IMAGE_SIZE and not args.force:
        fail(f"unexpected size {len(data)}, expected {IMAGE_SIZE} (16 MiB)")
    if data[0x10:0x14] != FD_SIGNATURE and not args.force:
        fail("Flash Descriptor signature not found at 0x10; is this a full dump?")

    match = BIOS_ID_RE.search(data)
    bios_id = match.group().decode() if match else "unknown"
    print(f"input   : {src}")
    print(f"size    : {len(data)}")
    print(f"sha256  : {sha256(src)}")
    print(f"BIOS ID : {bios_id}")
    if bios_id != EXPECTED_BIOS_ID and not args.force:
        fail(f"BIOS ID is {bios_id}, expected {EXPECTED_BIOS_ID}; use --force to continue")

    patch_name, label, expected = MODES[args.mode]
    patch = KIT / "patches" / patch_name
    if not patch.is_file():
        fail(f"patch file not found: {patch}")

    uefipatch = Path(args.uefipatch).expanduser().resolve()
    if not uefipatch.is_file():
        fail(f"UEFIPatch not found: {uefipatch}")

    out_dir = Path(args.out_dir).expanduser().resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / f"x380-{bios_id}-{label}.bin"

    print(f"mode    : {args.mode} ({patch_name})")
    print(f"output  : {out}")

    cmd = [str(uefipatch), str(src), str(patch), "-o", str(out)]
    proc = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    output = (proc.stdout or "") + (proc.stderr or "")
    print(output.strip())

    if proc.returncode != 0:
        fail(f"UEFIPatch exited with {proc.returncode}")
    if not out.is_file():
        fail("UEFIPatch did not create the output file")
    if out.stat().st_size != len(data):
        fail("output size changed")

    if "No patches can be applied" in output:
        fail("no patches were applied; wrong BIOS version?")

    replaced = len(re.findall(r"patch:\s+replaced", output))
    if replaced != expected:
        fail(f"expected {expected} replacement(s), got {replaced}")
    print(f"applied : {replaced} replacement(s)")

    if args.lnvbb == "auto":
        blob = out.read_bytes()
        marker = b"LNVBBSEC"
        positions = []
        start = 0
        while True:
            i = blob.find(marker, start)
            if i < 0:
                break
            positions.append(i)
            start = i + 1
        changed = 0
        if positions:
            buf = bytearray(blob)
            for i in positions:
                j = i + len(marker)
                if j < len(buf) and buf[j] == 0xFB:
                    buf[j] = 0xFF
                    changed += 1
            if changed:
                out.write_bytes(bytes(buf))
                print(f"LNVBB   : {changed} marker(s) changed FB -> FF")
            else:
                print("LNVBB   : already FF, no change")
        else:
            print("LNVBB   : marker not found in raw image, skipped")

    out_hash = sha256(out)
    manifest = {
        "tool": "UEFIPatch 0.28.0",
        "created": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "mode": args.mode,
        "patch_file": patch.name,
        "replacements": replaced,
        "input": {
            "path": str(src),
            "size": len(data),
            "sha256": sha256(src),
            "bios_id": bios_id,
        },
        "output": {
            "path": str(out),
            "size": out.stat().st_size,
            "sha256": out_hash,
        },
    }
    manifest_path = out.with_suffix(".json")
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8")
    (out.with_suffix(".sha256")).write_text(f"{out_hash}  {out.name}\n", encoding="ascii")

    print()
    print("done")
    print(f"  image    : {out}")
    print(f"  sha256   : {out_hash}")
    print(f"  manifest : {manifest_path}")
    print()
    print("next: verify with verify_patch.py, then flash with a programmer.")


if __name__ == "__main__":
    main()
