#!/usr/bin/env bash
set -euo pipefail
previous=/home/barryblueice/codex-x380-autopatcher-menu-20261102
root=/home/barryblueice/codex-x380-autopatcher-v10-20261102
src=$previous/src
workspace=$src/payloads/external/edk2/workspace
edk=$workspace/mrchromebox
mkdir -p "$root/results"
if ! test -e "$root/v9-source-before-fix.tar.gz"; then
 tar -czf "$root/v9-source-before-fix.tar.gz" -C "$src" .config payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/Application/X380Autopatcher
fi
mkdir -p "$root/regression-v9"
for name in X380Autopatcher.c autopatcher.c autopatcher.h; do
 tar -xOf "$root/v9-source-before-fix.tar.gz" "payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/Application/X380Autopatcher/$name" > "$root/regression-v9/$name"
done
tar -xOf /home/barryblueice/x380-ap-v10-app.tar.gz X380Autopatcher/test_host.c | sed 's/X380-Release-v10/X380-Release-v9/g' > "$root/regression-v9/test_host.c"
gcc -fshort-wchar -ffunction-sections -fdata-sections -I "$edk/MdePkg/Include" -I "$edk/MdePkg/Include/X64" \
 "$root/regression-v9/test_host.c" "$root/regression-v9/autopatcher.c" \
 -Wl,--gc-sections -o "$root/regression-v9/test-host"
if "$root/regression-v9/test-host" > "$root/results/v9-regression.txt"; then
 echo 'Expected old v9 to reject unavailable SPI'; exit 10
fi
grep -Fx 'FAIL: CheckPlatform() && Calls==0' "$root/results/v9-regression.txt"
tar -xzf /home/barryblueice/x380-ap-v10-app.tar.gz -C "$edk/UefiPayloadPkg/Application"
cd "$edk"
gcc -fshort-wchar -ffunction-sections -fdata-sections -I MdePkg/Include -I MdePkg/Include/X64 \
 UefiPayloadPkg/Application/X380Autopatcher/test_host.c UefiPayloadPkg/Application/X380Autopatcher/autopatcher.c \
 -Wl,--gc-sections -o "$root/test-host"
"$root/test-host" | tee "$root/results/host-test.txt"
python3 - "$previous/build.log" "$root/edk2-command.sh" <<'PY'
from pathlib import Path
import shlex,sys
args=['build','-a','IA32','-a','X64','-b','RELEASE','-p','UefiPayloadPkg/UefiPayloadPkg.dsc','-t','GCC','-q','-s','-D','BUILD_ARCH=X64']
for line in Path(sys.argv[1]).read_text().splitlines():
 if line.startswith('   Option:'):
  args.extend(['-D',line.split(':',1)[1].strip()])
 if line.startswith('   Pcd:'):
  args.extend(['--pcd',line.split(':',1)[1].strip()])
assert 'VARIABLE_SUPPORT=SMMSTORE' in args and 'BOOTLOADER=COREBOOT' in args
Path(sys.argv[2]).write_text(shlex.join(args)+'\n')
PY
cd "$workspace"
export WORKSPACE="$workspace" PACKAGES_PATH="$edk" EDK_TOOLS_PATH="$edk/BaseTools"
set +u
source "$edk/edksetup.sh"
set -u
if ! bash "$root/edk2-command.sh" > "$root/edk2-build.log" 2>&1; then tail -80 "$root/edk2-build.log"; exit 1; fi
cd "$src"
export PATH="$src/util/crossgcc/xgcc/bin:$PATH"
sed -i 's/X380-Release-v9/X380-Release-v10/' .config
make olddefconfig UPDATED_SUBMODULES=1 > "$root/config.log" 2>&1
cp "$workspace/Build/UefiPayloadPkgX64/RELEASE_GCC/FV/UEFIPAYLOAD.fd" build/UEFIPAYLOAD.fd
set +e
make -j8 UPDATED_SUBMODULES=1 > "$root/build.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$root/build.exit"
if test "$status" -ne 0; then tail -80 "$root/build.log"; exit "$status"; fi
cp .config "$root/results/resolved.config"
cp build/coreboot.rom build/UEFIPAYLOAD.fd build/smm/smm.elf build/dsdt.aml "$root/results/"
cp build/mainboard/lenovo/sklkbl_thinkpad/static.c "$root/results/"
build/util/cbfstool/cbfstool build/coreboot.rom print > "$root/results/cbfs.txt"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n fallback/ramstage -m x86 -f "$root/results/ramstage.elf"
for name in IntelGopDriver.efi vbt.bin spd.bin; do
 build/util/cbfstool/cbfstool build/coreboot.rom extract -n "$name" -f "$root/results/$name"
done
git diff --ignore-submodules=all --binary > "$root/results/coreboot-tracked.patch"
git -C "$edk" diff --ignore-submodules=all --binary > "$root/results/edk2-tracked.patch"
cp "$workspace/Build/UefiPayloadPkgX64/RELEASE_GCC/X64/X380Autopatcher.efi" "$root/results/"
cp "$root/build.log" "$root/build.exit" "$root/edk2-build.log" "$root/results/"
nm build/smm/smm.elf > "$root/results/smm-symbols.txt"
if grep -E ' (ec_read|ec_write|ec_query|h8_get_sense_ready|h8_get_fn_key)$' "$root/results/smm-symbols.txt"; then exit 9; fi
tar -czf "$root/results.tar.gz" -C "$root" results
echo READY
