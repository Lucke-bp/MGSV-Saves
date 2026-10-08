# Finds byte patterns ("48 8B ? 05", ? = any byte) in the code sections of a PE file.
# Usage: python -I tools/find_patterns.py <exe> < patterns.txt   (lines: name<TAB>pattern)
import re, struct, sys

def sections(data):
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    opt_size = struct.unpack_from('<H', data, pe + 20)[0]
    image_base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]
    table = pe + 24 + opt_size
    for i in range(count):
        o = table + i * 40
        name = data[o:o + 8].rstrip(b'\0').decode(errors='replace')
        vsize, va, rsize, raw = struct.unpack_from('<IIII', data, o + 8)
        flags = struct.unpack_from('<I', data, o + 36)[0]
        yield name, image_base + va, raw, rsize, flags

def to_regex(pattern):
    parts = []
    for token in pattern.split():
        parts.append(b'.' if token == '?' else re.escape(bytes([int(token, 16)])))
    return re.compile(b''.join(parts), re.DOTALL)

data = open(sys.argv[1], 'rb').read()
code = [s for s in sections(data) if s[4] & 0x20000000]  # IMAGE_SCN_MEM_EXECUTE
for line in sys.stdin:
    line = line.strip()
    if not line or '\t' not in line:
        continue
    name, pattern = line.split('\t', 1)
    hits = []
    rx = to_regex(pattern)
    for sname, va, raw, rsize, _ in code:
        for m in rx.finditer(data, raw, raw + rsize):
            hits.append(va + m.start() - raw)
    shown = ' '.join(hex(h) for h in hits[:4])
    print(f'{name:<24} {len(hits):>3} {shown}')
