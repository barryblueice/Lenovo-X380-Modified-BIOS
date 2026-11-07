#!/usr/bin/env python3
"""Verify a patched ThinkPad X380 Yoga (R0SET57W) image.

Usage:
    python verify_patch.py --image dist/x380-R0SET57W-adv-wwan.bin --mode both

Checks:
  * image size is 16 MiB
  * LenovoWmaPolicyDxe PE32 at RVA 0x684 contains the patched bytes
  * SystemFormBrowserCoreDxe contains the new formset GUID

Requires UEFIExtract (UEFITool NE). Pass --uefiextract or place it at
tools/UEFIExtract_NE_A76_win64/UEFIExtract.exe inside the kit.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

KIT = Path(__file__).resolve().parent.parent
DEFAULT_UEFIEXTRACT = KIT / "tools" / "UEFIExtract_NE_A76_win64" / "UEFIExtract.exe"

PATCHED_684 = bytes.fromhex("31c0e9ad030000")
NEW_FORM_GUID = bytes.fromhex("778b1d826d24964e8e103467d56ab1ba")

IMAGE_SIZE = 16 * 1024 * 1024


def run_dump(uefiextract: Path, image: Path, cwd: Path) -> None:
    """Extract the whole image tree into <image>.dump next to the image."""
    cmd = [str(uefiextract), str(image), "all"]
    proc = subprocess.run(cmd, cwd=str(cwd), capture_output=True, text=True, errors="replace")
    if proc.returncode != 0:
        print(proc.stdout[-4000:])
        print(proc.stderr[-2000:], file=sys.stderr)
        raise RuntimeError("UEFIExtract failed")


def main() -> None:
    parser = argparse.ArgumentParser(description="Verify a patched X380 Yoga image")
    parser.add_argument("--image", required=True)
    parser.add_argument("--mode", choices=("both", "adv", "wwan"), default="both")
    parser.add_argument("--uefiextract", default=str(DEFAULT_UEFIEXTRACT))
    parser.add_argument("--work-dir", default=str(KIT),
                        help="directory for the temporary extraction (default: kit root)")
    parser.add_argument("--keep", action="store_true", help="keep the temporary extract directory")
    args = parser.parse_args()

    image = Path(args.image).expanduser().resolve()
    if not image.is_file():
        print(f"FAIL: image not found: {image}")
        raise SystemExit(1)

    uefiextract = Path(args.uefiextract).expanduser().resolve()
    if not uefiextract.is_file():
        print(f"FAIL: UEFIExtract not found: {uefiextract}")
        print("download UEFITool NE and pass --uefiextract")
        raise SystemExit(1)

    failures = []

    size = image.stat().st_size
    if size == IMAGE_SIZE:
        print(f"PASS: size {size}")
    else:
        print(f"FAIL: size {size}, expected {IMAGE_SIZE}")
        failures.append("size")

    # UEFIExtract may fail to create directories under the system temp on some
    # Windows setups, so default to a temp dir inside the kit.
    work = Path(tempfile.mkdtemp(prefix=".verify-", dir=args.work_dir))
    try:
        local = work / "image.bin"
        shutil.copyfile(image, local)
        run_dump(uefiextract, local, work)
        root = work / "image.bin.dump"
        if not root.is_dir():
            print("FAIL: extraction produced no dump directory")
            raise SystemExit(1)

        if args.mode in ("both", "wwan"):
            hits = [p for p in root.rglob("body.bin")
                    if "LenovoWmaPolicyDxe" in str(p) and "PE32 image section" in str(p)]
            if len(hits) != 1:
                print(f"FAIL: expected 1 LenovoWmaPolicyDxe PE32, found {len(hits)}")
                failures.append("wwan")
            else:
                data = hits[0].read_bytes()
                actual = data[0x684:0x68B]
                if actual == PATCHED_684:
                    print(f"PASS: LenovoWmaPolicyDxe @0x684 = {actual.hex()}")
                else:
                    print(f"FAIL: LenovoWmaPolicyDxe @0x684 = {actual.hex()}")
                    failures.append("wwan")

        if args.mode in ("both", "adv"):
            count = 0
            for p in root.rglob("body.bin"):
                if "SystemFormBrowserCoreDxe" in str(p) and NEW_FORM_GUID in p.read_bytes():
                    count += 1
            if count:
                print(f"PASS: SystemFormBrowserCoreDxe contains new GUID ({count} file(s))")
            else:
                print("FAIL: new formset GUID not found in SystemFormBrowserCoreDxe")
                failures.append("adv")
    finally:
        if args.keep:
            print(f"kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)

    if failures:
        print(f"RESULT: FAIL ({', '.join(failures)})")
        raise SystemExit(1)
    print("RESULT: PASS")


if __name__ == "__main__":
    main()
