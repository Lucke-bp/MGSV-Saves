# Dumps a vtable from a PE file: each entry, and the first call/jmp target inside the function it points to.
# Usage: python -I tools/dump_vtable.py <exe> <vtable_va_hex> <count> [names.txt: "va_hex name" lines]
import struct, sys

data = open(sys.argv[1], 'rb').read()
pe = struct.unpack_from('<I', data, 0x3C)[0]
count = struct.unpack_from('<H', data, pe + 6)[0]
opt_size = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]
table = pe + 24 + opt_size
secs = []
for i in range(count):
    o = table + i * 40
    vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
    secs.append((base + va, raw, rsize))

def off(v):
    for va, raw, rsize in secs:
        if va <= v < va + rsize:
            return raw + v - va
    return None

names = {}
if len(sys.argv) > 4:
    for line in open(sys.argv[4]):
        parts = line.split()
        if len(parts) >= 2:
            names[int(parts[0], 16)] = parts[1]

start, n = int(sys.argv[2], 16), int(sys.argv[3])
for k in range(n):
    p = off(start + 8 * k)
    fn = struct.unpack_from('<Q', data, p)[0]
    q = off(fn)
    target = None
    if q is not None:
        for j in range(40):
            if data[q + j] in (0xE8, 0xE9):
                t = fn + j + 5 + struct.unpack_from('<i', data, q + j + 1)[0]
                if 0x140000000 <= t < 0x14A01D000:
                    target = t
                    break
    label = names.get(target, '') if target else ''
    print(f'[{k:3}] {hex(start + 8 * k)}: {hex(fn)} -> {hex(target) if target else "-":>12} {label}')
