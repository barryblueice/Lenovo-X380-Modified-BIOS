import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_IMM

p = sys.argv[1]
d = open(p, 'rb').read()

e_lfanew = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, e_lfanew + 6)[0]
opt_size = struct.unpack_from('<H', d, e_lfanew + 20)[0]
opt = e_lfanew + 24
so = opt + opt_size
secs = []
for i in range(nsec):
    o = so + i * 40
    name = d[o:o + 8].rstrip(b'\0').decode('latin1')
    vsize, vaddr, rawsize, rawptr = struct.unpack_from('<IIII', d, o + 8)
    secs.append((name, vaddr, vsize, rawptr, rawsize))

text = [s for s in secs if s[0] == '.text'][0]
name, vaddr, vsize, rawptr, rawsize = text
code = d[rawptr:rawptr + vsize]
base = vaddr

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
insns = list(md.disasm(code, base))
print('insns', len(insns))

# find rip-relative references to data addresses of interest
targets = {}
for ins in insns:
    for op in ins.operands:
        if op.type == CS_OP_MEM and op.mem.base == 41:  # X86_REG_RIP
            tgt = ins.address + ins.size + op.mem.disp
            targets.setdefault(tgt, []).append(ins)
for t in sorted(targets):
    if 0x1d00 <= t <= 0x2400:
        refs = targets[t]
        print('REF %#x <- %s' % (t, ', '.join('%#x(%s %s)' % (i.address, i.mnemonic, i.op_str) for i in refs)))

print('--- disassembly 0x2a0..0x500 ---')
for ins in insns:
    if 0x2a0 <= ins.address < 0x500:
        print('%#06x  %-24s %s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str))
