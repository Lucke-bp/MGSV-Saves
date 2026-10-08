# Finds direct calls/jumps (E8/E9 rel32) to the given addresses in the code sections of a PE file.
# Usage: python -I tools/find_calls.py <exe> <target_va_hex> [...]
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
    for p in range(raw, raw + rsize - 5):
        op = data[p]
        if op != 0xE8 and op != 0xE9:
            continue
        rel = struct.unpack_from('<i', data, p + 1)[0]
        site = base + va + (p - raw)
        if site + 5 + rel in targets:
            print(f"{'call' if op == 0xE8 else 'jmp '} em {hex(site)} -> {hex(site + 5 + rel)}")
