"""Exercise native flashing-script guards with fake DMI/flashrom; no hardware access."""
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

out = Path(__file__).resolve().parent
results = []
with tempfile.TemporaryDirectory(prefix='flash-script-check-', dir=out) as td:
    temp = Path(td)
    stub = temp / 'bin'
    stub.mkdir()
    for name in ('coreboot.bin', 'flash.sh'):
        shutil.copyfile(out / name, temp / name)
    dmi = temp / 'dmi'
    dmi.mkdir()
    (dmi / 'sys_vendor').write_text('LENOVO\n')
    (dmi / 'product_name').write_text('X380 Yoga\n')
    script_text = (temp / 'flash.sh').read_text()
    assert script_text.count('dmi_dir=/sys/class/dmi/id') == 1
    script_text = script_text.replace('dmi_dir=/sys/class/dmi/id', 'dmi_dir="' + dmi.as_posix() + '"')
    (temp / 'flash.sh').write_text(script_text, newline='\n')
    (stub / 'sudo').write_text('#!/usr/bin/env bash\nif [[ "$1" == -v ]]; then exit 0; fi\nexec "$@"\n', newline='\n')
    (stub / 'flashrom').write_text('''#!/usr/bin/env bash
printf '%s\\n' "$*" >> "$CALLS"
log=
write=0
while (( $# )); do
    case "$1" in
        -o) log=$2; shift 2 ;;
        -w) write=1; shift 2 ;;
        *) shift ;;
    esac
done
if (( write )); then
    if [[ "$TEST_MODE" == write_fail ]]; then exit 7; fi
    echo 'VERIFIED.' > "$log"
else
    if [[ "$TEST_MODE" == probe_fail ]]; then exit 3; fi
    if [[ "$TEST_MODE" == locked ]]; then
        echo 'FREG1: BIOS region (0x00700000-0x00ffffff) is read-only.' > "$log"
    else
        echo 'FREG1: BIOS region (0x00700000-0x00ffffff) is read-write.' > "$log"
    fi
fi
''', newline='\n')
    for path in stub.iterdir():
        path.chmod(0o755)
    env = os.environ.copy()
    env['PATH'] = str(stub) + os.pathsep + env['PATH']
    calls = temp / 'calls.txt'
    env['CALLS'] = calls.as_posix()
    for mode, script, expected_rc, expect_write in (
        ('ok', 'flash.sh', 0, True),
        ('write_fail', 'flash.sh', 7, True),
        ('wrong_model', 'flash.sh', 1, False),
        ('locked', 'flash.sh', 1, False),
        ('probe_fail', 'flash.sh', 1, False),
    ):
        if calls.exists(): calls.unlink()
        env['TEST_MODE'] = mode
        (dmi / 'product_name').write_text('Other Model\n' if mode == 'wrong_model' else 'X380 Yoga\n')
        run = subprocess.run(['D:/Git/bin/bash.exe', (temp / script).as_posix()],
                             env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
        invoked = calls.read_text() if calls.exists() else ''
        assert run.returncode == expected_rc, (mode, script, run.returncode, run.stdout, run.stderr)
        assert (' -w ' in invoked) == expect_write, (mode, invoked)
        assert '--force' not in invoked and ' -n ' not in invoked
        if expect_write:
            assert len(invoked.splitlines()) == 2, invoked
            assert '-p internal --ifd -i bios -N -w ' in invoked, invoked
            assert 'ch341' not in invoked
        results.append({'case': script + ':' + mode, 'status': 'passed', 'write_called_in_mock': expect_write})
    # A hash mismatch must abort before any programmer is probed.
    if calls.exists(): calls.unlink()
    with (temp / 'coreboot.bin').open('r+b') as f:
        f.write(b'\0')
    run = subprocess.run(['D:/Git/bin/bash.exe', (temp / 'flash.sh').as_posix()],
                         env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
    assert run.returncode == 1 and not calls.exists(), (run.stdout, run.stderr)
    results.append({'case': 'tampered-image', 'status': 'passed', 'write_called_in_mock': False})
(out / 'script-validation.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
