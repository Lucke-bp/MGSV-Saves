# Searches the running game's memory for a text (UTF-8 and UTF-16) and prints where it is, with its surroundings.
# Optionally replaces it in place (same or shorter length, NUL-padded) - for experiments with the game's texts.
# Usage: python -I tools/memtext.py find "VOLTAR À MISSÃO"
#        python -I tools/memtext.py replace <hex address> "SALVAR JOGO" <original byte length>
import ctypes
import ctypes.wintypes as w
import subprocess
import sys

PROCESS_QUERY_INFORMATION, PROCESS_VM_READ, PROCESS_VM_WRITE, PROCESS_VM_OPERATION = 0x400, 0x10, 0x20, 0x8
MEM_COMMIT = 0x1000
READABLE = {0x02, 0x04, 0x08, 0x20, 0x40, 0x80}  # READONLY, READWRITE, WRITECOPY, EXECUTE_READ, EXECUTE_READWRITE, EXECUTE_WRITECOPY

class MBI(ctypes.Structure):
    _fields_ = [('BaseAddress', ctypes.c_uint64), ('AllocationBase', ctypes.c_uint64), ('AllocationProtect', w.DWORD),
                ('PartitionId', w.WORD), ('RegionSize', ctypes.c_uint64), ('State', w.DWORD), ('Protect', w.DWORD), ('Type', w.DWORD)]

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
k32.OpenProcess.restype = w.HANDLE
k32.VirtualQueryEx.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.POINTER(MBI), ctypes.c_size_t]
k32.ReadProcessMemory.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
k32.WriteProcessMemory.argtypes = [w.HANDLE, ctypes.c_uint64, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]

def game_pid():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq mgsvtpp.exe', '/FO', 'CSV', '/NH'], capture_output=True, text=True).stdout
    return int(out.split(',')[1].strip('"'))

def regions(handle):
    address, mbi = 0, MBI()
    while k32.VirtualQueryEx(handle, address, ctypes.byref(mbi), ctypes.sizeof(mbi)):
        if mbi.State == MEM_COMMIT and (mbi.Protect & 0xFF) in READABLE and not mbi.Protect & 0x100:
            yield mbi.BaseAddress, mbi.RegionSize
        address = mbi.BaseAddress + mbi.RegionSize

def read(handle, address, size):
    buffer = ctypes.create_string_buffer(size)
    done = ctypes.c_size_t()
    if not k32.ReadProcessMemory(handle, address, buffer, size, ctypes.byref(done)):
        return b''
    return buffer.raw[:done.value]

def find(text):
    handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, False, game_pid())
    needles = [('utf-8', text.encode('utf-8')), ('utf-16', text.encode('utf-16-le'))]
    hits = 0
    for base, size in regions(handle):
        if size > 0x40000000:
            continue
        data = read(handle, base, size)
        for kind, needle in needles:
            p = data.find(needle)
            while p != -1 and hits < 40:
                before = data[max(0, p - 48):p]
                after = data[p:p + len(needle) + 48]
                print(f'{kind} 0x{base + p:x}: ...{before!r} | {after!r}')
                hits += 1
                p = data.find(needle, p + 1)
    print(f'{hits} ocorrencia(s)')

def replace(address, text, original_length):
    handle = k32.OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, False, game_pid())
    data = text.encode('utf-8')
    if len(data) > original_length:
        raise SystemExit(f'texto novo ({len(data)} bytes) maior que o original ({original_length})')
    data = data + b'\0' * (original_length - len(data))
    done = ctypes.c_size_t()
    ok = k32.WriteProcessMemory(handle, address, data, len(data), ctypes.byref(done))
    print('ok' if ok else f'falhou ({ctypes.get_last_error()})', read(handle, address, original_length + 8))

if sys.argv[1] == 'find':
    find(sys.argv[2])
elif sys.argv[1] == 'replace':
    replace(int(sys.argv[2], 16), sys.argv[3], int(sys.argv[4]))
