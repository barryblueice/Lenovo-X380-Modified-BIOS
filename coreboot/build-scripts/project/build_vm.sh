#!/usr/bin/env bash
set -euo pipefail
previous=/home/barryblueice/codex-x380-autopatcher-menu-20261102
root=/home/barryblueice/codex-x380-ec-integrated
src=$previous/src
workspace=$src/payloads/external/edk2/workspace
edk=$workspace/mrchromebox
mkdir -p "$root/results"
if ! test -e "$root/pre-refresh.tar.gz"; then
 tar -czf "$root/pre-refresh.tar.gz" -C "$src" .config binaries \
  src/mainboard/lenovo/sklkbl_thinkpad/variants/x380_yoga/overridetree.cb \
  payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/Application/X380Autopatcher \
  payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/Application/X380EcUpdate \
  payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/UefiPayloadPkg.dsc \
  payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/UefiPayloadPkg.fdf \
  payloads/external/edk2/workspace/mrchromebox/UefiPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c
fi
tar -xzf /home/barryblueice/x380-integrated-apps.tar.gz -C "$edk/UefiPayloadPkg/Application"
tar -xzf /home/barryblueice/x380-integrated-blobs.tar.gz -C "$root"
mkdir -p "$src/refresh-binaries"
cp "$root/binaries/IntelGopDriver.efi" "$root/binaries/vbt.bin" "$src/refresh-binaries/"
python3 - "$src" "$edk" <<'PY'
from pathlib import Path
import sys,re
s,e=map(Path,sys.argv[1:])
p=s/'.config'; c=p.read_text();c=re.sub(r'CONFIG_LOCALVERSION="[^"]*"','CONFIG_LOCALVERSION="X380-Coreboot"',c)
c=c.replace('="binaries/IntelGopDriver.efi"','="refresh-binaries/IntelGopDriver.efi"').replace('="binaries/vbt.bin"','="refresh-binaries/vbt.bin"');p.write_text(c)
p=s/'src/mainboard/lenovo/sklkbl_thinkpad/variants/x380_yoga/overridetree.cb';c=p.read_text()
if 'register "gen3_dec"' not in c:
 c=c.replace('chip soc/intel/skylake\n','chip soc/intel/skylake\n\t# Indexed ENE EC update window; LPC and DMI decode set by coreboot.\n\tregister "gen3_dec" = "LPC_IO(0x700, 0x8)"\n',1)
p.write_text(c)
for name in ['UefiPayloadPkg.dsc','UefiPayloadPkg.fdf']:
 p=e/'UefiPayloadPkg'/name;c=p.read_text();old='UefiPayloadPkg/Application/X380Autopatcher/X380Autopatcher.inf';new='UefiPayloadPkg/Application/X380EcUpdate/X380EcUpdate.inf'
 if new not in c:
  c=c.replace(old,old+'\n'+('INF ' if name.endswith('fdf') else '  ')+new)
 p.write_text(c)
p=e/'UefiPayloadPkg/Library/PlatformBootManagerLib/PlatformBootManager.c';c=p.read_text()
if 'X380 EC update (manual)' not in c:
 anchor='  //\n  // Register iPXE if the binary is present in the firmware volume.'
 assert anchor in c
 c=c.replace(anchor,'''  {
    EFI_GUID EcGuid = {0xc140da7e,0x722b,0x4c81,{0xa3,0x83,0x58,0x23,0x80,0xa0,0x57,0x03}};
    PlatformRegisterFvBootOption (&EcGuid, L"X380 EC update (manual)",
                                 LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP);
  }

'''+anchor)
p.write_text(c)
PY
cd "$edk"
gcc -fshort-wchar -ffunction-sections -fdata-sections -I MdePkg/Include -I MdePkg/Include/X64 \
 UefiPayloadPkg/Application/X380Autopatcher/test_host.c UefiPayloadPkg/Application/X380Autopatcher/autopatcher.c \
 -Wl,--gc-sections -o "$root/test-autopatcher"
"$root/test-autopatcher" | tee "$root/results/autopatcher-tests.txt"
gcc -O2 -fshort-wchar -I MdePkg/Include -I MdePkg/Include/X64 \
 UefiPayloadPkg/Application/X380EcUpdate/test_ec.c UefiPayloadPkg/Application/X380EcUpdate/ec_flash.c \
 -o "$root/test-ec"
"$root/test-ec" | tee "$root/results/ec-tests.txt"
for pair in 'readback ec_flash' 'inspect ec_inspect_engine'; do
 test_name=${pair%% *}
 engine_name=${pair##* }
 gcc -O2 -fshort-wchar -I MdePkg/Include -I MdePkg/Include/X64 \
  "UefiPayloadPkg/Application/X380EcUpdate/test_$test_name.c" "UefiPayloadPkg/Application/X380EcUpdate/$engine_name.c" \
  -o "$root/test-$test_name"
 "$root/test-$test_name" | tee "$root/results/$test_name-tests.txt"
done
sha256sum UefiPayloadPkg/Application/X380EcUpdate/* > "$root/results/ec-source-sha256.txt"
python3 - "$previous/build.log" "$root/edk2-command.sh" <<'PY'
from pathlib import Path
import shlex,sys
args=['build','-a','IA32','-a','X64','-b','RELEASE','-p','UefiPayloadPkg/UefiPayloadPkg.dsc','-t','GCC','-q','-s','-D','BUILD_ARCH=X64']
for line in Path(sys.argv[1]).read_text().splitlines():
 if line.startswith('   Option:'):args.extend(['-D',line.split(':',1)[1].strip()])
 if line.startswith('   Pcd:'):args.extend(['--pcd',line.split(':',1)[1].strip()])
assert 'VARIABLE_SUPPORT=SMMSTORE' in args and 'BOOTLOADER=COREBOOT' in args
Path(sys.argv[2]).write_text(shlex.join(args)+'\n')
PY
cd "$workspace"
export WORKSPACE="$workspace" PACKAGES_PATH="$edk" EDK_TOOLS_PATH="$edk/BaseTools"
set +u
source "$edk/edksetup.sh"
set -u
if ! bash "$root/edk2-command.sh" > "$root/edk2-build.log" 2>&1; then tail -90 "$root/edk2-build.log"; exit 1; fi
cd "$src"
export PATH="$src/util/crossgcc/xgcc/bin:$PATH"
make olddefconfig UPDATED_SUBMODULES=1 > "$root/config.log" 2>&1
cp "$workspace/Build/UefiPayloadPkgX64/RELEASE_GCC/FV/UEFIPAYLOAD.fd" build/UEFIPAYLOAD.fd
set +e
make -j8 UPDATED_SUBMODULES=1 > "$root/build.log" 2>&1
status=$?
set -e
printf '%s\n' "$status" > "$root/build.exit"
if test "$status" -ne 0; then tail -90 "$root/build.log"; exit "$status"; fi
cp .config "$root/results/resolved.config"
cp build/coreboot.rom build/UEFIPAYLOAD.fd build/smm/smm.elf build/dsdt.aml "$root/results/"
cp build/mainboard/lenovo/sklkbl_thinkpad/static.c "$root/results/"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n fallback/payload -m x86 -f "$root/results/payload.elf"
build/util/cbfstool/cbfstool build/coreboot.rom print > "$root/results/cbfs.txt"
build/util/cbfstool/cbfstool build/coreboot.rom extract -n fallback/ramstage -m x86 -f "$root/results/ramstage.elf"
for name in IntelGopDriver.efi vbt.bin spd.bin cpu_microcode_blob.bin; do
 build/util/cbfstool/cbfstool build/coreboot.rom extract -n "$name" -f "$root/results/$name"
done
git diff --ignore-submodules=all --binary > "$root/results/coreboot-tracked.patch"
git -C "$edk" diff --ignore-submodules=all --binary > "$root/results/edk2-tracked.patch"
cp "$workspace/Build/UefiPayloadPkgX64/RELEASE_GCC/X64/UefiPayloadPkg/Application/X380Autopatcher/X380Autopatcher/OUTPUT/X380Autopatcher.efi" "$root/results/"
cp "$workspace/Build/UefiPayloadPkgX64/RELEASE_GCC/X64/UefiPayloadPkg/Application/X380EcUpdate/X380EcUpdate/OUTPUT/X380EcUpdate.efi" "$root/results/"
cp "$root/build.log" "$root/build.exit" "$root/edk2-build.log" "$root/results/"
nm build/smm/smm.elf > "$root/results/smm-symbols.txt"
if grep -E ' (ec_read|ec_write|ec_query|h8_get_sense_ready|h8_get_fn_key)$' "$root/results/smm-symbols.txt"; then exit 9; fi
tar -czf "$root/results.tar.gz" -C "$root" results
echo READY
