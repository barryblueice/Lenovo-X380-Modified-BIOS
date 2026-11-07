import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

p = sys.argv[1]
lo = int(sys.argv[2], 0)
hi = int(sys.argv[3], 0)
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
text = [s for s in secs if s[0] == '.text'][0]
code = d[text[3]:text[3] + text[2]]
md = Cs(CS_ARCH_X86, CS_MODE_64)
for ins in md.disasm(code, text[1]):
    if lo <= ins.address < hi:
        print('%#06x  %-24s %s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
