# Lists the texts of the game's language tables stored near the text of a key (the values area keeps related texts
# together), in memory order.
# Usage: python -I tools/langnear.py 27b5469d [bytes around, default 0x600]
import sys

sys.argv, near_args = sys.argv[:1], sys.argv[1:]
__file__ = __file__.replace('langnear.py', 'langdump.py') # langdump.py loads memtext.py next to itself
exec(open(__file__, encoding='utf-8').read().split('handle = k32.OpenProcess')[0])

handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, game_pid())
target = int(near_args[0], 16)
around = int(near_args[1], 16) if len(near_args) > 1 else 0x600
for base, data, p, count, voff, koff in blocks(handle):
    items = list(entries(data, p, count, voff, koff))
    hits = [q for key, q, text in items if key == target]
    for hit in hits:
        print(f'--- bloco em 0x{base + p:x}, {count} textos')
        for key, q, text in sorted(items, key=lambda item: item[1]):
            if abs(q - hit) <= around:
                mark = '>>' if q == hit else '  '
                print(f'{mark} 0x{key:08x} +0x{q - hit:+05x}: {text[:90]}')
