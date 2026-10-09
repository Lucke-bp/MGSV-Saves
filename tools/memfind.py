# Searches the running game's memory for values: 8-byte pointers and/or 4-byte numbers (little-endian), and prints
# where they are with the bytes around them.
# Usage: python -I tools/memfind.py q ed4dd450 [q ed4dd44e] [d 39df1083] ...   (q: qword, d: dword)
import struct
import sys

sys.argv, find_args = sys.argv[:1], sys.argv[1:]
__file__ = __file__.replace('memfind.py', 'memtext.py')
exec(open(__file__, encoding='utf-8').read().split("if sys.argv[1] == 'find':")[0])

needles = []
for kind, value in zip(find_args[0::2], find_args[1::2]):
    number = int(value, 16)
    needles.append((f'{kind} {value}', struct.pack('<Q' if kind == 'q' else '<I', number)))

handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, game_pid())
hits = 0
for base, size in regions(handle):
    if size > 0x40000000:
        continue
    data = read(handle, base, size)
    for name, needle in needles:
        p = data.find(needle)
        while p != -1 and hits < 60:
            if p % 4 == 0:
                around = data[max(0, p - 32):p + 40]
                print(f'{name} em 0x{base + p:x} (regiao 0x{base:x}+0x{p:x}): {around.hex(" ", 8)}')
                hits += 1
            p = data.find(needle, p + 1)
print(f'{hits} ocorrencia(s)')
