import sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_MEM, CS_OP_IMM

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

text = [s for s in secs if s[0] == '.text'][0]
code = d[text[3]:text[3] + text[2]]
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = True
insns = list(md.disasm(code, text[1]))
byaddr = {i.address: i for i in insns}

calls = {}
for i in insns:
    if i.mnemonic == 'call' and i.operands and i.operands[0].type == CS_OP_IMM:
        t = i.operands[0].imm
        calls.setdefault(t, []).append(i.address)

starts = set([0x2a0]) | set(calls)
# function starts from int3 padding: instruction right after int3 run
prev = None
for i in insns:
    if prev is not None and prev.mnemonic == 'int3' and i.mnemonic != 'int3':
        starts.add(i.address)
    prev = i

def fstart(a):
    cands = [s for s in starts if s <= a]
    return max(cands) if cands else None

print('== functions referencing data 0x1d00..0x2300 ==')
funcs = {}
for i in insns:
    for op in i.operands:
        if op.type == CS_OP_MEM and op.mem.base == 41:
            t = i.address + i.size + op.mem.disp
            if 0x1d00 <= t <= 0x2300:
                fs = fstart(i.address)
                funcs.setdefault(fs, set()).add(t)
for fs in sorted(funcs):
    print('%#06x refs %s' % (fs, ' '.join('%#x' % t for t in sorted(funcs[fs]))))

print('== callers of functions that reference 0x1ff0/0x1ff8 ==')
for fs, ts in funcs.items():
    if ts & {0x1ff0, 0x1ff8}:
        print('func %#x called from %s' % (fs, ', '.join('%#x' % c for c in calls.get(fs, []))))

print('== data 0x20b0..0x21a0 ==')
o = off(0x20b0)
for i in range(0, 0xf0, 16):
    chunk = d[o + i:o + i + 16]
    print('%#06x  %-47s %s' % (0x20b0 + i, chunk.hex(' '), ''.join(chr(c) if 32 <= c < 127 else '.' for c in chunk)))
