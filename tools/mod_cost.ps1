# Measures what the mod costs in the running game: CPU time of the mod's own threads (those that start inside its
# DLL: the CRT is linked statically, so std::thread starts there) and the DLL's memory.
# The Lua work (MgsvSaves.Tick, twice a second) runs on the game's main thread: see "lua MgsvSaves.Benchmark()".
# Usage: powershell -File tools/mod_cost.ps1
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ThreadInfo {
    [DllImport("kernel32.dll")] static extern IntPtr OpenThread(uint access, bool inherit, uint id);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("ntdll.dll")] static extern int NtQueryInformationThread(IntPtr thread, int infoClass, out IntPtr info, int size, IntPtr returned);
    public static long StartAddress(int id) {
        IntPtr thread = OpenThread(0x0040, false, (uint)id); // THREAD_QUERY_INFORMATION
        if (thread == IntPtr.Zero) return -1;
        IntPtr address;
        int status = NtQueryInformationThread(thread, 9, out address, IntPtr.Size, IntPtr.Zero); // ThreadQuerySetWin32StartAddress
        CloseHandle(thread);
        return status == 0 ? address.ToInt64() : -1;
    }
}
'@
$p = Get-Process mgsvtpp -ErrorAction Stop
$m = $p.Modules | Where-Object { $_.FileName -like '*MGS_TPP*msacm32.dll' }
if (-not $m) { 'O mod nao esta carregado'; exit 1 }
$lo = $m.BaseAddress.ToInt64()
$hi = $lo + $m.ModuleMemorySize
$uptime = ((Get-Date) - $p.StartTime).TotalSeconds
$sum = 0.0
foreach ($t in $p.Threads) {
    $start = [ThreadInfo]::StartAddress($t.Id)
    if ($start -ge $lo -and $start -lt $hi) {
        $cpu = $t.TotalProcessorTime.TotalSeconds
        $sum += $cpu
        'thread {0} (inicio +0x{1:X}): CPU {2:N3} s' -f $t.Id, ($start - $lo), $cpu
    }
}
'jogo aberto ha {0:N0} s; CPU do jogo {1:N1} s' -f $uptime, $p.TotalProcessorTime.TotalSeconds
'threads do mod: CPU {0:N3} s = {1:P3} da CPU do jogo, {2:N2} ms por segundo de jogo' -f $sum, ($sum / $p.TotalProcessorTime.TotalSeconds), (1000 * $sum / $uptime)
'DLL do mod: {0} KB de imagem; jogo: {1:N0} MB privados' -f [int]($m.ModuleMemorySize / 1KB), ($p.PrivateMemorySize64 / 1MB)
