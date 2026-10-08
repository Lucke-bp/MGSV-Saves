# Finds 8-byte little-endian values (e.g. function pointers in vtables) in all sections of a PE file.
# Usage: python -I tools/find_qword.py <exe> <value_hex> [...]
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
    name = data[o:o + 8].rstrip(b'\0').decode(errors='replace')
    vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
    secs.append((name, va, raw, rsize))
for value in sys.argv[2:]:
    needle = struct.pack('<Q', int(value, 16))
    p = data.find(needle)
    while p != -1:
        for name, va, raw, rsize in secs:
            if raw <= p < raw + rsize:
                print(f'{value} em {name} {hex(base + va + p - raw)}')
        p = data.find(needle, p + 1)
