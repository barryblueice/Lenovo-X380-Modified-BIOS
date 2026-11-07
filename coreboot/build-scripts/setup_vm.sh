set -euo pipefail
work="$HOME/codex-x380-edk2-cf7c35d"
cd "$work"
if [ ! -d src/.git ]; then
    git init src
fi
if ! git -C src cat-file -e cf7c35d73bd2845e053e741b7b03e1cb1baccfcf 2>/dev/null; then
    printf '%s\n' cf7c35d73bd2845e053e741b7b03e1cb1baccfcf > src/.git/shallow
    git -C src fetch --update-shallow ../coreboot-cf7c35d.bundle HEAD
fi
printf '%s\n' cf7c35d73bd2845e053e741b7b03e1cb1baccfcf > src/.git/shallow
git -C src checkout --detach cf7c35d73bd2845e053e741b7b03e1cb1baccfcf
if ! git -C src remote get-url origin >/dev/null 2>&1; then
    git -C src remote add origin https://github.com/coreboot/coreboot.git
fi
test "$(git -C src rev-parse HEAD)" = cf7c35d73bd2845e053e741b7b03e1cb1baccfcf
tar -xzf x380-build-inputs.tar.gz -C src
cd src
if [ -f AGENTS.md ]; then cat AGENTS.md; fi
if [ -f .site-local/AGENTS.md ]; then cat .site-local/AGENTS.md; fi
if [ ! -e util/crossgcc/xgcc ]; then
    ln -s "$HOME/coreboot-ic847-plus/util/crossgcc/xgcc" util/crossgcc/xgcc
fi
util/crossgcc/xgcc/bin/x86_64-elf-gcc --version | head -1
util/crossgcc/xgcc/bin/iasl -v | head -4
cp x380-edk2.defconfig .config
make olddefconfig
printf '\nResolved board and payload:\n'
grep -E '^CONFIG_(BOARD_LENOVO_X380_YOGA|MAINBOARD_PART_NUMBER|PAYLOAD_EDK2|EDK2_|SMMSTORE|ROM_SIZE|CBFS_SIZE|FSP_|HAVE_.*BIN|.*BIN_PATH|MAINBOARD_USE_LIBGFXINIT)=' .config || true
printf '\nPinned submodules:\n'
git submodule status 3rdparty/fsp 3rdparty/blobs 3rdparty/libgfxinit 3rdparty/libhwbase 3rdparty/vboot 3rdparty/intel-microcode
