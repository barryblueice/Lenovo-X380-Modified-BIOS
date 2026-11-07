from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tarfile
import uuid

out=Path(__file__).resolve().parent
project=out.parent
old=project/'coreboot-autopatcher-v9-20261102'
sha=lambda b: hashlib.sha256(b).hexdigest()
baseline=(old/'coreboot.bin').read_bytes()
assert sha(baseline)=='e486efaafc5bdced884c41d3d05874c1e2ac0c578a41a146a94d24d2899048cd'
with tarfile.open(out/'results.tar.gz') as archive:
    for item in archive.getmembers():
        assert (out/item.name).resolve().is_relative_to(out)
        assert item.isdir() or item.isfile()
    archive.extractall(out,filter='data')
r=out/'results'
assert (r/'build.exit').read_text(encoding='utf-8').strip()=='0'
raw=(r/'coreboot.rom').read_bytes()
assert len(raw)==len(baseline)==0x1000000
assert raw[0x790000:0x790008]==b'__FMAP__'
cfg=(r/'resolved.config').read_text(encoding='utf-8')
assert 'CONFIG_X380_AUTOPATCHER=y' in cfg
assert 'CONFIG_LOCALVERSION="X380-Release-v10"' in cfg
for name in ['IntelGopDriver.efi','vbt.bin','spd.bin','static.c']:
    assert (r/name).read_bytes()==(old/'results'/name).read_bytes(), name
for name in ['autopatcher.c','autopatcher.h','autopatcher_ec.c']:
    assert (out/'app'/name).read_bytes()==(old/'app'/name).read_bytes(), name
assert (r/'v9-regression.txt').read_text().strip() == 'FAIL: CheckPlatform() && Calls==0'
assert 'PASS: real-machine unavailable SPI regression' in (r/'host-test.txt').read_text()
app=(r/'X380Autopatcher.efi').read_bytes()
assert b'MZ'==app[:2]
assert 'X380 Autopatcher - manual maintenance'.encode('utf-16le') in app
assert 'APPLY'.encode('utf-16le') in app
assert b'CBET4000 X380-Release-v10' in app
assert 'Detected LPC 00:1f.0:'.encode('utf-16le') in app
assert 'SPI PCI function is not visible; EC access uses LPC ports.'.encode('utf-16le') in app
assert b'CBET4000 X380-Release-v10' in (r/'ramstage.elf').read_bytes()
payload=(r/'UEFIPAYLOAD.fd').read_bytes()
assert uuid.UUID('c140da7e-722b-4c81-a383-582380a05701').bytes_le in payload
patch=(r/'edk2-tracked.patch').read_text(encoding='utf-8')
assert 'LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP' in patch
assert 'X380 Autopatcher (manual)' in patch
image=baseline[:0x790000]+raw[0x790000:]
assert all(int.from_bytes(image[x:x+4],'little')==0xffffff00 for x in [0x80,0x84,0x88,0x90])
(out/'coreboot.bin').write_bytes(image)
digest=sha(image)
script=(old/'flash.sh').read_text(encoding='utf-8').replace(sha(baseline),digest)
script=script.replace('IFD descriptor and BIOS regions','IFD BIOS region')
script=script.replace('Native internal programmer: descriptor + BIOS. ME/GbE are excluded.',
                      'Native internal programmer: BIOS only. Descriptor/ME/GbE are preserved.')
script=script.replace("printf 'Descriptor write protection may reject this operation; no force or BIOS-only fallback is used.\\n'",
'''probe_log="${log_file%.log}-probe.log"
"${run_as_root[@]}" flashrom -p internal -VV -o "$probe_log" || fail 'SPI probe failed; nothing written.'
grep -Eq 'FREG1: BIOS region .* is read-write\\.' "$probe_log" || fail 'BIOS region is not confirmed writable; nothing written.' ''')
script=script.replace('flashrom -p internal --ifd -i fd -i bios -N -w',
                      'flashrom -p internal --ifd -i bios -N -w')
(out/'flash.sh').write_text(script,newline='\n')
shutil.copyfile(old/'backup.sh',out/'backup.sh')
(out/'SHA256SUMS.txt').write_text(digest+'  coreboot.bin\n')
for f in ['flash.sh','backup.sh']:
    subprocess.run(['D:/Git/bin/bash.exe','-n',(out/f).as_posix()],check=True)
report={
 'status':'BUILT_VALIDATED_NOT_FLASHED','version':'X380-Release-v10',
 'sha256':digest,'size':len(image),'coreboot_commit':'4f3d2b564c8675a05d555d27740d62e0257cbd5c',
 'edk2_commit':'6c14abf865e093452edc775651df61775bf99c9f','edk2_rebuilt':True,
 'autopatcher_config_enabled':True,'autopatcher_runtime_entry':True,
 'autopatcher_entry':'ESC -> Boot Manager -> X380 Autopatcher (manual)',
 'automatic_execution':False,'confirmation':'type APPLY then Enter',
 'autopatcher_core_logic_unchanged':True,'ec_hardware_tested':False,
 'platform_regression':{'reported_board':'LENOVO / X380 Yoga',
                        'reported_lpc':'9D4E8086','reported_spi':'FFFFFFFF',
                        'old_v9_reproduced_rejection':True,'v10_host_regression_passed':True,
                        'spi_unavailable_allowed_only_with_exact_board_version_and_lpc':True},
 'ec_backup':'256-byte bank57, unique nonvolatile boot-service UEFI variable before write',
 'ec_verification':'all 256 bytes checked by reconstructed core library',
 'prefix_identical_to_v9':True,'descriptor_sha256':sha(image[:0x1000]),
 'me_version':'11.6.0.1126 Consumer LP deguard + HAP','full_ifd_master_permissions_preserved':True,
 'native_internal_scope':['bios'],'ch341_scope':None,
 'flash_method_for_this_fix':'Linux internal BIOS-only','flashed':False,
 'validation':'Compiled coreboot and EDK2; payload GUID and UI strings present; hardware configuration preserved',
}
(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
(out/'README.txt').write_text(
 'X380-Release-v10 — 手动 autopatcher 菜单版\n'
 '开机按 ESC -> Boot Manager -> X380 Autopatcher (manual)。\n'
 '进入后输入 APPLY 并回车才会执行；Esc 取消，不会自动执行或自动重启。\n'
 '按原程序模板覆盖 EC bank 57 的 256 字节，包括 SVP 及其他字段，不是 EC 固件更新。\n'
 '执行前将原 bank 保存在独立的非易失 UEFI 变量（Bank57-*）中；写入后检查256字节。\n'
 '原 bank 变量 GUID: c140da7e-722b-4c81-a383-582380a05702，可在UEFI Shell中用dmpstore查看。\n'
 'EC 实机执行尚未验证，菜单中会明确提示。\n'
 '保留 v7/v6 的硬件修复、自动 SATA/PCIe 选择、解锁 Descriptor、deguard ME。\n'
 'Linux 内刷脚本：bash flash.sh，只更新 BIOS 区，先检查写权限，保留写后校验。\n'
 'v10 accepts unavailable SPI only with exact board, v10 firmware and LPC ID; rejects unexpected visible SPI IDs.\n'
 f'SHA256: {digest}\n',encoding='utf8')
print(json.dumps(report,indent=2))
