set -euo pipefail
work="$HOME/codex-x380-edk2-cf7c35d"
target="$work/src/payloads/external/edk2/workspace/mrchromebox"
old="$HOME/coreboot-ic847-plus/payloads/external/edk2/workspace/mrchromebox"
pin=1d840d4e6ed0e9a13fee47936c330f4f0cbf6510
test "$(git -C "$old" rev-parse HEAD)" = "$pin"
git -C "$target" checkout --detach "$pin"
while read -r rootkey optional; do
    while read -r key path; do
        if [ "$optional/$path" = CryptoPkg/Library/MbedTlsLib/mbedtls/framework ]; then
            git -C "$target/$optional" config "${key%.path}.active" true
        else
            git -C "$target/$optional" config "${key%.path}.active" false
        fi
    done < <(git -C "$target/$optional" config --file .gitmodules --get-regexp '^submodule\..*\.path$' 2>/dev/null || true)
done < <(git -C "$target" config --file .gitmodules --get-regexp '^submodule\..*\.path$')
git -C "$target" -c submodule.recurse=false submodule update --init --checkout --jobs 6
test "$(git -C "$target" rev-parse HEAD)" = "$pin"
tar -xzf "$work/x380-gop-inputs.tar.gz" -C "$work/src"
cd "$work/src"
python3 - <<'PY'
from pathlib import Path
p=Path('x380-edk2-gop.defconfig')
s=p.read_text().replace('6c14abf865e093452edc775651df61775bf99c9f','1d840d4e6ed0e9a13fee47936c330f4f0cbf6510')
s+='\n# CONFIG_EDK2_USE_LVGL_RENDERER is not set\n'
p.write_text(s)
PY
cp x380-edk2-gop.defconfig .config
python3 - <<'PY'
from pathlib import Path
p=Path('payloads/external/edk2/Makefile')
s=p.read_text()
s=s.replace('git submodule update --checkout --recursive;', 'git -c submodule.recurse=false submodule update --checkout;')
s=s.replace('git submodule update --init --checkout --recursive', 'git -c submodule.recurse=false submodule update --init --checkout')
p.write_text(s)
PY
export PATH="$PWD/util/crossgcc/xgcc/bin:$PATH"
make olddefconfig UPDATED_SUBMODULES=1
grep -E '^CONFIG_(BOARD_LENOVO_X380_YOGA|NO_GFX_INIT|INTEL_GMA_ADD_VBT|INTEL_GMA_VBT_FILE|EDK2_GOP_DRIVER|EDK2_GOP_FILE|EDK2_TAG_OR_REV|VBT_CBFS_COMPRESSION_LZMA|SPI_FLASH_MACRONIX)=' .config
