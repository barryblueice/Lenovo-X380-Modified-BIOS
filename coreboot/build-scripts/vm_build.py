"""SSH helper pinned to the user's Ubuntu build VM and supplied host fingerprint."""
from pathlib import Path
import base64,hashlib,subprocess,sys

ROOT=Path(__file__).resolve().parent
HOST='192.168.50.133'
USER='barryblueice'
EXPECTED='SHA256:N3qDSoR8L+KJlApTDI8sztk2dxkzHLtLVVe7208/sgo'
BIN=Path('D:/Git/usr/bin')
KNOWN=ROOT/'known_hosts'
if not KNOWN.exists():
    scan=subprocess.run([str(BIN/'ssh-keyscan.exe'),'-T','10','-t','ed25519',HOST],capture_output=True,text=True)
    lines=[x for x in scan.stdout.splitlines() if x and not x.startswith('#')]
    if not lines:
        raise RuntimeError('No host key returned: '+scan.stderr)
    for line in lines:
        fields=line.split()
        fp='SHA256:'+base64.b64encode(hashlib.sha256(base64.b64decode(fields[2])).digest()).decode().rstrip('=')
        if fp!=EXPECTED:raise RuntimeError('Host fingerprint mismatch: '+fp)
    KNOWN.write_text('\n'.join(lines)+'\n',encoding='ascii')
    print('Verified host fingerprint '+EXPECTED,flush=True)
opts=['-o','BatchMode=yes','-o','ConnectTimeout=12','-o','StrictHostKeyChecking=yes',
      '-o','IdentitiesOnly=yes','-o','UserKnownHostsFile='+KNOWN.as_posix(),'-i',(ROOT/'id_ed25519').as_posix()]
action=sys.argv[1]
if action=='probe':
    cmd="id; uname -a; nproc; df -h /; command -v make gcc gnatmake nasm iasl; find /root /home /opt -maxdepth 4 -type d \\( -name coreboot -o -name xgcc -o -name edk2 -o -name edk2.git \\) -print 2>/dev/null | head -60"
    result=subprocess.run([str(BIN/'ssh.exe'),*opts,USER+'@'+HOST,cmd])
elif action=='run':
    result=subprocess.run([str(BIN/'ssh.exe'),*opts,USER+'@'+HOST,sys.argv[2]])
elif action=='script':
    result=subprocess.run([str(BIN/'ssh.exe'),*opts,USER+'@'+HOST,'bash -s'],input=Path(sys.argv[2]).read_bytes())
elif action in ('upload','download'):
    local,remote=sys.argv[2:4]
    args=[local,USER+'@'+HOST+':'+remote] if action=='upload' else [USER+'@'+HOST+':'+remote,local]
    result=subprocess.run([str(BIN/'scp.exe'),*opts,*args])
else:raise ValueError('Unknown action')
sys.exit(result.returncode)
