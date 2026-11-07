from pathlib import Path
import hashlib
import json
import tarfile

r = Path(__file__).resolve().parent
usb = r.parent / 'ec-usb-update'
proven = usb / 'src'
assert hashlib.sha256((usb / 'ec-update.efi').read_bytes()).hexdigest() == 'e29d893b3ff9aec8670dc4eb776b3bea232a8b6cc453c0857e154eb070152df5'
same = ['ec_flash.c', 'ec_flash.h', 'platform.h', 'ec_image.h', 'test_ec.c', 'test_readback.c']
for name in same:
    assert (r / 'ec-app' / name).read_bytes() == (proven / name).read_bytes(), name
expected = (proven / 'X380EcUpdate.c').read_text().replace('X380 EC USB updater', 'X380 EC firmware updater')
expected = expected.replace('        L"Read/entry/exit tested; erase/program not yet hardware validated.\\n");', '        );')
assert (r / 'ec-app/X380EcUpdate.c').read_text() == expected
assert (r / 'ec-app/X380EcUpdate.inf').read_text() == (proven / 'X380EcUpdate.inf').read_text().replace('582380A05707', '582380A05703')
manifest = {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted((r / 'ec-app').iterdir()) if f.is_file()}
(r / 'evidence/ec-source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
with tarfile.open(r / 'apps.tar.gz', 'w:gz') as archive:
    for directory, module in [('app', 'X380Autopatcher'), ('ec-app', 'X380EcUpdate')]:
        for f in sorted((r / directory).iterdir()):
            if f.is_file():
                archive.add(f, arcname=module + '/' + f.name)
print('PASS: proven engine/platform unchanged; only application title/status and integrated GUID differ')
