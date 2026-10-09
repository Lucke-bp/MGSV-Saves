# Presses a key in the foreground window by SendInput (the game reads the keyboard by Raw Input, which sees it).
# Usage: powershell -File tools/press_key.ps1 [-Key 0x1B] [-HoldMs 60]
#        powershell -File tools/press_key.ps1 -WaitFor "teste: reabrir" [-DelayMs 300]   (presses when the mod's log
#        gets a line containing the text; waits up to 10 minutes)
param([int]$Key = 0x1B, [int]$HoldMs = 60, [string]$WaitFor = '', [int]$DelayMs = 300)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class KeyPress {
    [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort wVk, wScan; public uint dwFlags, time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit, Size = 40)] struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public KEYBDINPUT ki; }
    [DllImport("user32.dll")] static extern uint SendInput(uint count, INPUT[] inputs, int size);
    [DllImport("user32.dll")] static extern uint MapVirtualKey(uint code, uint mapType);
    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint process);
    public static uint ForegroundProcess() { uint p; GetWindowThreadProcessId(GetForegroundWindow(), out p); return p; }
    public static uint Send(ushort vk, bool up) {
        var input = new INPUT[1];
        input[0].type = 1;
        input[0].ki.wVk = vk;
        input[0].ki.wScan = (ushort)MapVirtualKey(vk, 0);
        input[0].ki.dwFlags = up ? 2u : 0u;
        return SendInput(1, input, Marshal.SizeOf(typeof(INPUT)));
    }
}
'@
if ($WaitFor) {
    $log = 'C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP\mgsv_saves\mgsv_saves.log'
    $seen = (Get-Content $log).Count
    $deadline = (Get-Date).AddMinutes(10)
    while ((Get-Date) -lt $deadline) {
        $lines = Get-Content $log
        if (($lines | Select-Object -Skip $seen) -match [regex]::Escape($WaitFor)) { break }
        $seen = $lines.Count
        Start-Sleep -Milliseconds 100
    }
    if ((Get-Date) -ge $deadline) { 'tempo esgotado'; exit 1 }
    Start-Sleep -Milliseconds $DelayMs
}
$game = (Get-Process mgsvtpp -ErrorAction SilentlyContinue).Id
$front = [KeyPress]::ForegroundProcess()
if ($front -ne $game) { "janela em primeiro plano nao e o jogo (pid $front)"; exit 1 }
[void][KeyPress]::Send($Key, $false)
Start-Sleep -Milliseconds $HoldMs
[void][KeyPress]::Send($Key, $true)
"tecla 0x{0:X2} enviada" -f $Key
