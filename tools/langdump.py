# Searches the game's language tables in memory (LANG v3 blocks: "LANG", version 3, "BE", count, values offset,
# keys offset; keys = big-endian (hash, value offset) pairs sorted by hash; values = u16 color + UTF-8 + NUL).
# Usage: python -I tools/langdump.py text "VOLTAR À MISSÃO"     (texts containing it, with their keys)
#        python -I tools/langdump.py key 27b5469d               (the texts of a key in every block)
import struct
import sys

sys.argv, args = sys.argv[:1], sys.argv[1:]
exec(open(__file__.replace('langdump.py', 'memtext.py'), encoding='utf-8').read().split("if sys.argv[1] == 'find':")[0])

def blocks(handle):
    for base, size in regions(handle):
        if size > 0x40000000:
            continue
        data = read(handle, base, size)
        p = data.find(b'LANG\x03\x00\x00\x00BE')
        while p != -1:
            count, voff, koff = struct.unpack_from('<III', data, p + 12)
            if 0 < count < 100000 and p + koff + 8 * count <= len(data):
                yield base, data, p, count, voff, koff
            p = data.find(b'LANG\x03\x00\x00\x00BE', p + 4)

def entries(data, p, count, voff, koff):
    for i in range(count):
        key, off = struct.unpack_from('>II', data, p + koff + 8 * i)
        q = p + voff + off
        end = data.find(b'\0', q + 2)
        yield key, q, data[q + 2:end].decode('utf-8', 'replace')

handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, game_pid())
mode, value = args[0], args[1]
found = 0
for base, data, p, count, voff, koff in blocks(handle):
    for key, q, text in entries(data, p, count, voff, koff):
        if (mode == 'text' and value in text) or (mode == 'key' and key == int(value, 16)):
            print(f'chave 0x{key:08x} em 0x{base + q + 2:x} ({len(text.encode())} bytes): {text[:120]}')
            found += 1
print(f'{found} resultado(s)')
