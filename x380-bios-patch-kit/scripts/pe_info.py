import sys, struct, re

p = sys.argv[1]
d = open(p, 'rb').read()
print('file', p, 'size', len(d), 'magic', d[:2])

e_lfanew = struct.unpack_from('<I', d, 0x3c)[0]
machine = struct.unpack_from('<H', d, e_lfanew + 4)[0]
nsec = struct.unpack_from('<H', d, e_lfanew + 6)[0]
opt_size = struct.unpack_from('<H', d, e_lfanew + 20)[0]
opt = e_lfanew + 24
magic = struct.unpack_from('<H', d, opt)[0]
entry = struct.unpack_from('<I', d, opt + 16)[0]
if magic == 0x20b:
    base = struct.unpack_from('<Q', d, opt + 24)[0]
else:
    base = struct.unpack_from('<I', d, opt + 28)[0]
print('machine %#x nsec %d optmagic %#x entryRVA %#x base %#x' % (machine, nsec, magic, entry, base))

so = opt + opt_size
secs = []
for i in range(nsec):
    off = so + i * 40
    name = d[off:off + 8].rstrip(b'\0').decode('latin1')
    vsize, vaddr, rawsize, rawptr = struct.unpack_from('<IIII', d, off + 8)
    secs.append((name, vaddr, vsize, rawptr, rawsize))
    print('%-8s vaddr %#x vsize %#x rawptr %#x rawsize %#x' % (name, vaddr, vsize, rawptr, rawsize))

def rva2off(rva):
    for name, vaddr, vsize, rawptr, rawsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            return rawptr + (rva - vaddr)
    return None

for s in ['Unauthorized', '1802', 'WWAN', 'WLAN', 'Whitelist', 'whitelist', 'Policy', 'Lenovo']:
    for enc in ['ascii', 'utf-16le']:
        pat = s.encode(enc)
        idx = [m.start() for m in re.finditer(re.escape(pat), d, re.I)]
        if idx:
            print('STR %-14s %-8s %s' % (s, enc, [hex(x) for x in idx[:6]]))

off = rva2off(entry)
print('entry file offset', hex(off) if off else None)
if off:
    print('entry bytes', d[off:off + 32].hex())
