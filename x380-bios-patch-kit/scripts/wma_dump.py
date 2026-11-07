import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

p = sys.argv[1]
d = open(p, 'rb').read()

e = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, e + 6)[0]
opt_size = struct.unpack_from('<H', d, e + 20)[0]
opt = e + 24
so = opt + opt_size
secs = []
for i in range(nsec):
    o = so + i * 40
    nm = d[o:o + 8].rstrip(b'\0').decode('latin1')
    vsize, vaddr, rawsize, rawptr = struct.unpack_from('<IIII', d, o + 8)
    secs.append((nm, vaddr, vsize, rawptr, rawsize))

def off(rva):
    for nm, vaddr, vsize, rawptr, rawsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            return rawptr + rva - vaddr

def hexdump(rva, n):
    o = off(rva)
    for i in range(0, n, 16):
        chunk = d[o + i:o + i + 16]
        print('%#06x  %-47s %s' % (rva + i, chunk.hex(' '), ''.join(chr(c) if 32 <= c < 127 else '.' for c in chunk)))

print('=== data 0x1fc0..0x20b0 ===')
hexdump(0x1fc0, 0xf0)
print('=== qwords pointing into 0x1d00..0x2200 ===')
o = off(0x1de0)
for i in range(0, 0x300, 8):
    q = struct.unpack_from('<Q', d, o + i)[0]
    if 0x1d00 <= q <= 0x2200:
        print('%#06x -> %#x' % (0x1de0 + i, q))

text = [s for s in secs if s[0] == '.text'][0]
code = d[text[3]:text[3] + text[2]]
md = Cs(CS_ARCH_X86, CS_MODE_64)
print('=== disasm 0x950..0xc00 ===')
for ins in md.disasm(code, text[1]):
    if 0x950 <= ins.address < 0xc00:
        print('%#06x  %-24s %s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
