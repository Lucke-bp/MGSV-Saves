# Captures the game window (even behind other windows) with PrintWindow(PW_RENDERFULLCONTENT) and saves a PNG.
# Usage: powershell -File tools/game_window_shot.ps1 <out.png> [-Scale 0.5]
param([Parameter(Mandatory)][string]$Out, [double]$Scale = 0.5)
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class GameWindow {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
  // The largest visible top-level window of the process.
  public static IntPtr Find(uint pid) {
    IntPtr best = IntPtr.Zero; long bestArea = 0;
    EnumWindows((h, l) => {
      uint p; GetWindowThreadProcessId(h, out p);
      RECT r;
      if (p == pid && IsWindowVisible(h) && GetClientRect(h, out r)) {
        long area = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
        if (area > bestArea) { bestArea = area; best = h; }
      }
      return true;
    }, IntPtr.Zero);
    return best;
  }
}
'@
$p = Get-Process mgsvtpp -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p) { throw 'Jogo nao esta aberto.' }
$hwnd = [GameWindow]::Find([uint32]$p.Id)
if ($hwnd -eq [IntPtr]::Zero) { throw 'Janela do jogo nao encontrada.' }
$r = New-Object GameWindow+RECT
[GameWindow]::GetClientRect($hwnd, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc()
$ok = [GameWindow]::PrintWindow($hwnd, $dc, 3)  # PW_CLIENTONLY | PW_RENDERFULLCONTENT
$g.ReleaseHdc($dc); $g.Dispose()
$small = New-Object System.Drawing.Bitmap $bmp, ([int]($w * $Scale)), ([int]($h * $Scale))
$small.Save($Out); $bmp.Dispose(); $small.Dispose()
"PrintWindow: $ok ($w x $h) -> $Out"
