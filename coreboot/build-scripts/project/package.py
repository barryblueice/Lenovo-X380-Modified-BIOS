from pathlib import Path
import hashlib,json,re,shutil,struct,subprocess,tarfile,uuid

out=Path(__file__).resolve().parent
project=out.parent
old=project/'coreboot-autopatcher-v10-20261102'
sha=lambda b:hashlib.sha256(b).hexdigest()
baseline=(old/'coreboot.bin').read_bytes()
assert sha(baseline)=='de84e7d102c8da3d66959395d9859bce185ae6979e22e9d50913b008296e9cf3'
with tarfile.open(out/'results.tar.gz') as archive:
    for item in archive.getmembers():
        assert (out/item.name).resolve().is_relative_to(out)
        assert item.isdir() or item.isfile()
    archive.extractall(out,filter='data')
r=out/'results'
assert (r/'build.exit').read_text().strip()=='0'
raw=(r/'coreboot.rom').read_bytes()
assert len(raw)==0x1000000 and raw[0x790000:0x790008]==b'__FMAP__'
cfg=(r/'resolved.config').read_text()
assert 'CONFIG_LOCALVERSION="X380-Coreboot"' in cfg and 'CONFIG_X380_AUTOPATCHER=y' in cfg
assert 'CONFIG_CONSOLE_SERIAL=y' not in cfg
before=(old/'results/resolved.config').read_text().splitlines()
after=cfg.splitlines()
changes={'removed':[x for x in before if x not in after],'added':[x for x in after if x not in before]}
allowed=('CONFIG_LOCALVERSION=','CONFIG_INTEL_GMA_VBT_FILE=','CONFIG_EDK2_GOP_FILE=')
assert all(x.startswith(allowed) for x in changes['removed']+changes['added']),changes
static=(r/'static.c').read_text()
assert '.gen3_dec = LPC_IO(0x700, 0x8),' in static
assert static.replace('\t.gen3_dec = LPC_IO(0x700, 0x8),\n','')==(old/'results/static.c').read_text()
for name in ['IntelGopDriver.efi','vbt.bin','spd.bin']:
    assert (r/name).read_bytes()==(old/'results'/name).read_bytes(),name
for name in ['autopatcher.c','autopatcher.h','autopatcher_ec.c']:
    assert (out/'app'/name).read_bytes()==(old/'app'/name).read_bytes(),name
assert (r/'autopatcher-tests.txt').read_text().startswith('PASS:')
assert (r/'ec-tests.txt').read_text().startswith('PASS:')
assert (r/'readback-tests.txt').read_text().startswith('PASS:')
assert (r/'inspect-tests.txt').read_text().startswith('PASS:')
manifest=json.loads((out/'evidence/ec-source-manifest.json').read_text())
remote_manifest={line.split()[-1].split('/')[-1]:line.split()[0]
                 for line in (r/'ec-source-sha256.txt').read_text().splitlines()}
assert remote_manifest==manifest, 'Compiled source must match the audited local source'
for name,digest in manifest.items():
    assert sha((out/'ec-app'/name).read_bytes())==digest,name
for name in ['ec_flash.c','ec_flash.h','platform.h','ec_image.h']:
    assert (out/'ec-app'/name).read_bytes()==(project/'ec-usb-update/src'/name).read_bytes()
assert len(json.loads((out/'evidence/vendor-primitives.json').read_text()))==18
assert not re.search(r' (ec_read|ec_write|ec_query|h8_get_sense_ready|h8_get_fn_key)$',
    (r/'smm-symbols.txt').read_text(),re.M)
ec=(out/'binaries/ec.bin').read_bytes()
app=(r/'X380EcUpdate.efi').read_bytes()
assert app[:2]==b'MZ' and ec in app and hashlib.sha256(ec).digest() in app
assert b'CBET4000 X380-Coreboot' in app
assert 'UPDATE'.encode('utf-16le') in app
for text in ['X380 EC firmware updater','command gate before/enabled/restored=',
             'preserved-data differences=', 'halt step=', 'read passes=',
             'EC code already matches target. Nothing programmed.']:
    assert text.encode('utf-16le') in app,text
assert 'not yet hardware validated'.encode('utf-16le') not in app
ap=(r/'X380Autopatcher.efi').read_bytes()
assert ap[:2]==b'MZ' and b'CBET4000 X380-Coreboot' in ap and b'X380-Release-v10' not in ap
assert b'CBET4000 X380-Coreboot' in (r/'ramstage.elf').read_bytes()
payload=(r/'UEFIPAYLOAD.fd').read_bytes()
assert app in payload and ap in payload, 'Final payload must contain these exact compiled applications'
assert payload in (r/'payload.elf').read_bytes(), 'Final ROM payload must include the newly built firmware volume'
for guid in ['c140da7e-722b-4c81-a383-582380a05701','c140da7e-722b-4c81-a383-582380a05703']:
    assert uuid.UUID(guid).bytes_le in payload
patch=(r/'edk2-tracked.patch').read_text()
assert 'X380 EC update (manual)' in patch and patch.count('LOAD_OPTION_ACTIVE | LOAD_OPTION_CATEGORY_APP')>=2
image=baseline[:0x790000]+raw[0x790000:]
assert image[0x3000:0x700000]==baseline[0x3000:0x700000]
assert image[:0x790000]==baseline[:0x790000]
assert all(int.from_bytes(image[x:x+4],'little')==0xffffff00 for x in [0x80,0x84,0x88,0x90])
(out/'coreboot.bin').write_bytes(image)
digest=sha(image)
script=(old/'flash.sh').read_text().replace(sha(baseline),digest)
script=script.replace('X380-Release-v10','X380-Coreboot')
(out/'flash.sh').write_text(script,newline='\n')
shutil.copyfile(old/'backup.sh',out/'backup.sh')
shutil.copyfile(old/'test_delivery.py',out/'test_delivery.py')
(out/'SHA256SUMS.txt').write_text(digest+'  coreboot.bin\n')
for f in ['flash.sh','backup.sh']:
    subprocess.run(['D:/Git/bin/bash.exe','-n',(out/f).as_posix()],check=True)
microcode=(r/'cpu_microcode_blob.bin').read_bytes(); microcodes=[]; pos=0
while pos+48<=len(microcode):
    h=struct.unpack_from('<12I',microcode,pos);size=h[8] or 2048
    assert h[0]==1 and size%4==0 and pos+size<=len(microcode)
    assert sum(struct.unpack_from('<%dI'%(size//4),microcode,pos))&0xffffffff==0
    microcodes.append({'cpuid':hex(h[3]),'revision':hex(h[1]),'date':hex(h[2]),'size':size});pos+=size
assert pos==len(microcode)
report={
 'status':'BUILT_VALIDATED_NOT_FLASHED','image':str(out/'coreboot.bin'),
 'sha256':digest,'size':len(image),'firmware_label':'X380-Coreboot',
 'custom_release_number_embedded':False,
 'source_package':json.loads((out/'evidence/source.json').read_text()),
 'coreboot_commit':'4f3d2b564c8675a05d555d27740d62e0257cbd5c',
 'edk2_commit':'6c14abf865e093452edc775651df61775bf99c9f',
 'configuration_changes':changes,'microcodes':microcodes,
 'me_version':'11.6.0.1126 Consumer LP deguard + HAP',
 'me_sha256':sha(image[0x3000:0x700000]),'me_identical_to_working_v10':True,
 'descriptor_gbe_mrc_nvram_identical_to_v10_build':True,
 'graphics_and_spd_identical_to_working_v10':True,
 'autopatcher_manual_entry_preserved':True,
 'ec_update_entry':'ESC -> Boot Manager -> X380 EC update (manual)',
 'ec_update_confirmation':'U then UPDATE; I inspects without erase/program',
 'ec_image_sha256':sha(ec),'ec_program_scope':'0x1000..0x1efff, changed 128-byte rows',
 'ec_preserved_ranges':['0x0000..0x0fff','0x1f000..0x1ffff'],
 'ec_compatibility_gate':'all 2432 boot bytes outside 0x200..0x87f; target slot must be FF; reject 64KiB alias; recheck under reset before writing',
 'ec_checks':['embedded SHA256','board/firmware/LPC/SPI','LPC EC window','AC and battery presence',
              'pre-OS exclusive access','bounded vendor handshakes','row readback','full 128KiB comparison including preserved data'],
 'ec_engine_hardware_tested_via_usb':True,'ec_postboot_functions_user_confirmed':True,
 'ec_integrated_app_hardware_tested':False,'bios_hardware_tested':False,'flashed':False,
 'ec_source_identical_to_proven_usb_engine':True,'ec_application_sha256':sha(app),
 'ec_source_manifest':manifest,
 'vendor_machine_code_emulation':'18 flash primitive cases and 4 platform mode functions',
 'ec_update_performed':False,'smm_no_known_ec_users':True,
 'native_internal_scope':['bios'],'usb_delivery':'pending',
 'limitations':['Proven USB engine now integrated; this rebuilt BIOS itself has not yet been flashed or hardware tested.',
                'No boot-block update; incompatible boot code is refused.',
                'This ports required blobs and EC updater; vendor UEFI fixes are not automatically converted into coreboot code.']}
(out/'validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['status','sha256','size','me_sha256','microcodes','flashed','ec_update_performed']},indent=2))
