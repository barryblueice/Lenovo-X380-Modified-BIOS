set -euo pipefail
work="$HOME/codex-x380-edk2-cf7c35d"
test "$(cat "$work/build.exit")" = 0
cd "$work/src"
mkdir -p "$work/results"
result="$work/results"
cp build/coreboot.rom "$result/coreboot-gop-mxic.rom"
cp build/UEFIPAYLOAD.fd "$result/UEFIPAYLOAD.fd"
cp .config "$result/resolved.config"
cp x380-edk2-gop.defconfig "$result/defconfig"
cp "$work/build.log" "$result/build.log"
cp "$work/toolchain.log" "$result/toolchain.log"
cp "$work/build.exit" "$result/build.exit"
build/util/cbfstool/cbfstool build/coreboot.rom print > "$result/cbfs.txt"
build/util/cbfstool/cbfstool build/coreboot.rom layout > "$result/fmap.txt"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n IntelGopDriver.efi -f "$result/IntelGopDriver.extracted.efi"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n vbt.bin -f "$result/vbt.extracted.bin"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n spd.bin -f "$result/spd.extracted.bin"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n fallback/payload -f "$result/payload.extracted.elf" -m x86
cmp binaries/IntelGopDriver.efi "$result/IntelGopDriver.extracted.efi"
cmp binaries/vbt.bin "$result/vbt.extracted.bin"
git diff --binary -- payloads/external/edk2/Makefile > "$result/coreboot-build-only.patch"
{
    printf 'coreboot: '; git rev-parse HEAD
    printf 'edk2: '; git -C payloads/external/edk2/workspace/mrchromebox rev-parse HEAD
    printf '\ncoreboot submodules:\n'; git submodule status 3rdparty/blobs 3rdparty/fsp 3rdparty/libgfxinit 3rdparty/libhwbase 3rdparty/vboot 3rdparty/intel-microcode
    printf '\nEDK2 direct submodules:\n'; git -C payloads/external/edk2/workspace/mrchromebox submodule status
    printf '\nCross compiler:\n'; util/crossgcc/xgcc/bin/i386-elf-gcc --version | head -1
    printf '\nCoreboot source modifications:\n'; git diff --stat
} > "$result/sources.txt"
sha256sum "$result/coreboot-gop-mxic.rom" "$result/UEFIPAYLOAD.fd" "$result/IntelGopDriver.extracted.efi" "$result/vbt.extracted.bin" "$result/spd.extracted.bin" "$result/payload.extracted.elf"
tar -czf "$work/x380-coreboot-edk2-results.tar.gz" -C "$work" results
ls -lh "$work/x380-coreboot-edk2-results.tar.gz"
