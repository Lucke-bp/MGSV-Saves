# Finds RIP-relative references (disp32 as the last 4 bytes of an instruction) to the given addresses in code.
# Usage: python -I tools/find_rip_refs.py <exe> <target_va_hex> [...]
import struct, sys

data = open(sys.argv[1], 'rb').read()
targets = {int(t, 16) for t in sys.argv[2:]}
pe = struct.unpack_from('<I', data, 0x3C)[0]
count = struct.unpack_from('<H', data, pe + 6)[0]
opt_size = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]
table = pe + 24 + opt_size
for i in range(count):
    o = table + i * 40
    vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
    flags = struct.unpack_from('<I', data, o + 36)[0]
    if not flags & 0x20000000:
        continue
    for p in range(raw, raw + rsize - 4):
        rel = struct.unpack_from('<i', data, p)[0]
        site = base + va + (p - raw)
        t = site + 4 + rel
        if t in targets:
            print(f'{hex(site - 3)} (disp32 em {hex(site)}) -> {hex(t)}  bytes antes: {data[p-3:p].hex(" ")}')
