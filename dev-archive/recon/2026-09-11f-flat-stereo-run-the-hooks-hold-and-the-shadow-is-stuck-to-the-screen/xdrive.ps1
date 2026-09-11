# XIII (2003) /lm harness, 2026-09-11: launch via Steam, find the game window,
# BitBlt capture (never PrintWindow), graceful WM_CLOSE.
param(
  [Parameter(Mandatory=$true)][string]$Action,
  [string]$Out = "",
  [int]$Seconds = 15,
  [string]$Keys = "",
  [int]$Hold = 80,
  [int]$Gap = 400,
  [int]$X = 0,
  [int]$Y = 0
)
$ErrorActionPreference = "Stop"
if (-not ("XW" -as [type])) {
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class XW {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
  public static void Key(byte vk, int holdMs, bool ext) {
    byte sc = (byte)MapVirtualKey(vk, 0);
    uint f = ext ? 1u : 0u;
    keybd_event(vk, sc, f, UIntPtr.Zero);
    System.Threading.Thread.Sleep(holdMs);
    keybd_event(vk, sc, f | 2u, UIntPtr.Zero);
  }
  public static void Click(IntPtr h, int cx, int cy) {
    POINT p; p.X = cx; p.Y = cy; ClientToScreen(h, ref p);
    SetCursorPos(p.X, p.Y); System.Threading.Thread.Sleep(80);
    mouse_event(2, 0, 0, 0, UIntPtr.Zero); System.Threading.Thread.Sleep(60);
    mouse_event(4, 0, 0, 0, UIntPtr.Zero);
  }
}
"@
}
function Get-GameWindows {
  $p = Get-Process XIII -ErrorAction SilentlyContinue
  if (-not $p) { return @() }
  $pids = @($p | ForEach-Object { $_.Id })
  $found = New-Object System.Collections.ArrayList
  $cb = [XW+EnumProc]{
    param($h,$l)
    $wpid = 0
    [void][XW]::GetWindowThreadProcessId($h, [ref]$wpid)
    if ($pids -contains [int]$wpid) {
      $sb = New-Object System.Text.StringBuilder 512
      [void][XW]::GetWindowTextW($h, $sb, 512)
      $r = New-Object XW+RECT
      [void][XW]::GetWindowRect($h, [ref]$r)
      [void]$found.Add([pscustomobject]@{ H=$h; Title=$sb.ToString(); Vis=[XW]::IsWindowVisible($h); X=$r.L; Y=$r.T; W=($r.R-$r.L); Ht=($r.B-$r.T) })
    }
    return $true
  }
  [void][XW]::EnumWindows($cb, [IntPtr]::Zero)
  return $found
}
function Main-Window { Get-GameWindows | Where-Object { $_.Vis -and $_.W -gt 200 -and $_.Ht -gt 200 } | Sort-Object { $_.W * $_.Ht } -Descending | Select-Object -First 1 }
switch ($Action) {
  "launch" {
    if (Get-Process XIII -ErrorAction SilentlyContinue) { "ALREADY RUNNING"; break }
    Start-Process "steam://rungameid/1170760"; "LAUNCHED via steam://rungameid/1170760 at $(Get-Date -Format HH:mm:ss)"
  }
  "windows" { Get-GameWindows | Format-Table -AutoSize | Out-String -Width 200 }
  "shot" {
    $w = Main-Window
    if (-not $w) { "SHOT: no visible game window"; break }
    [void][XW]::SetForegroundWindow($w.H); Start-Sleep -Milliseconds 300
    Add-Type -AssemblyName System.Drawing
    $bmp = New-Object System.Drawing.Bitmap $w.W, $w.Ht
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($w.X, $w.Y, 0, 0, (New-Object System.Drawing.Size $w.W, $w.Ht)); $g.Dispose()
    if (-not $Out) { $Out = Join-Path $env:TEMP "xiii-shot.png" }
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    "SHOT: $Out ($($w.W)x$($w.Ht)) title='$($w.Title)'"
  }
  "close" {
    $w = Main-Window
    if ($w) { [void][XW]::PostMessage($w.H, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero); "SENT WM_CLOSE"
      for ($i=0; $i -lt $Seconds; $i++) { Start-Sleep -Seconds 1; if (-not (Get-Process XIII -ErrorAction SilentlyContinue)) { "EXITED after $i s"; break } } }
    else { "no window to close" }
    if (Get-Process XIII -ErrorAction SilentlyContinue) { "STILL RUNNING - not killed" }
  }
  "keys" {
    # -Keys "ENTER,DOWN,NP7,W:1500" ; name[:holdms]
    $w = Main-Window; if (-not $w) { "no window"; break }
    [void][XW]::SetForegroundWindow($w.H); Start-Sleep -Milliseconds 250
    $map = @{ ENTER=0x0D; ESC=0x1B; UP=0x26; DOWN=0x28; LEFT=0x25; RIGHT=0x27; SPACE=0x20; TAB=0x09; BACK=0x08;
      NP0=0x60; NP1=0x61; NP2=0x62; NP3=0x63; NP4=0x64; NP5=0x65; NP6=0x66; NP7=0x67; NP8=0x68; NP9=0x69;
      SHIFT=0x10; CTRL=0x11; F2=0x71; TILDE=0xC0 }
    $ext = @('UP','DOWN','LEFT','RIGHT')
    foreach ($k in ($Keys -split ',')) {
      $parts = $k.Split(':'); $n = $parts[0].Trim().ToUpper(); $h = $Hold; if ($parts.Count -gt 1) { $h = [int]$parts[1] }
      if ($n -eq 'WAIT') { Start-Sleep -Milliseconds $h; continue }
      if ($map.ContainsKey($n)) { $vk = $map[$n] } elseif ($n.Length -eq 1) { $vk = [int][char]$n } else { "unknown key $n"; continue }
      [XW]::Key([byte]$vk, $h, ($ext -contains $n)); Start-Sleep -Milliseconds $Gap
    }
    "KEYS SENT: $Keys"
  }
  "click" {
    $w = Main-Window; if (-not $w) { "no window"; break }
    [void][XW]::SetForegroundWindow($w.H); Start-Sleep -Milliseconds 250
    [XW]::Click($w.H, $X, $Y); "CLICKED client $X,$Y"
  }
  "state" { $p = Get-Process XIII -ErrorAction SilentlyContinue; if ($p) { "RUNNING pid=$($p.Id) responding=$($p.Responding)" } else { "NOT RUNNING" } }
}
