# Decodes the exe's table of pause menu items (records of 0x18 bytes: name key, info key, pointer to the item's id
# string) and shows each key's text from the game's language tables in memory.
# Usage: python -I tools/pause_items.py [table file offset, default 0x2a45cd0] [records, default 24]
import struct
import sys

sys.argv, item_args = sys.argv[:1], sys.argv[1:]
__file__ = __file__.replace('pause_items.py', 'langdump.py') # langdump.py loads memtext.py next to itself
exec(open(__file__, encoding='utf-8').read().split('handle = k32.OpenProcess')[0])

EXE = r'C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP\mgsvtpp.exe'
exe = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', exe, 0x3C)[0]
sections_count = struct.unpack_from('<H', exe, pe + 6)[0]
optional_size = struct.unpack_from('<H', exe, pe + 20)[0]
image_base = struct.unpack_from('<Q', exe, pe + 24 + 24)[0]
sections = []
for i in range(sections_count):
    s = pe + 24 + optional_size + 40 * i
    name = exe[s:s + 8].rstrip(b'\0').decode()
    vsize, va, rsize, raw = struct.unpack_from('<IIII', exe, s + 8)
    sections.append((name, va, max(vsize, rsize), raw, rsize))

def file_offset(address):
    rva = address - image_base
    for name, va, size, raw, rsize in sections:
        if va <= rva < va + size and rva - va < rsize:
            return raw + rva - va
    return None

def c_string(address):
    off = file_offset(address)
    if off is None:
        return '?'
    end = exe.find(b'\0', off)
    return exe[off:min(end, off + 80)].decode('latin-1')

start = int(item_args[0], 16) if item_args else 0x2a45cd0
count = int(item_args[1]) if len(item_args) > 1 else 24
records = []
for i in range(count):
    name_key, _, info_key, _, pointer = struct.unpack_from('<IIIIQ', exe, start + 0x18 * i)
    records.append((name_key, info_key, pointer))

wanted = {key for record in records for key in record[:2]}
texts = {}
handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, game_pid())
for base, data, p, count_, voff, koff in blocks(handle):
    for key, q, text in entries(data, p, count_, voff, koff):
        if key in wanted and key not in texts:
            texts[key] = text.replace('\n', ' / ')
for i, (name_key, info_key, pointer) in enumerate(records):
    print(f'{i:2} {c_string(pointer):40} nome 0x{name_key:08x} {texts.get(name_key, "-")[:40]!r:44} info 0x{info_key:08x} {texts.get(info_key, "-")[:60]!r}')
