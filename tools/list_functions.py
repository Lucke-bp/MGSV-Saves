# Lists function starts in a code range of a PE file (a start = first byte after int3/ret padding), with the first
# instructions as raw bytes. Usage: python -I tools/list_functions.py <exe> <start_va_hex> <end_va_hex>
import struct, sys

data = open(sys.argv[1], 'rb').read()
start, end = int(sys.argv[2], 16), int(sys.argv[3], 16)
pe = struct.unpack_from('<I', data, 0x3C)[0]
count = struct.unpack_from('<H', data, pe + 6)[0]
opt_size = struct.unpack_from('<H', data, pe + 20)[0]
base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]
table = pe + 24 + opt_size
for i in range(count):
    o = table + i * 40
    vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
    if base + va <= start < base + va + rsize:
        def at(v):
            return raw + (v - base - va)
        v = start
        while v < end:
            b = data[at(v)]
            prev = data[at(v) - 1]
            if b != 0xCC and prev == 0xCC:
                print(hex(v), data[at(v):at(v) + 24].hex(' '))
            v += 1
        break
