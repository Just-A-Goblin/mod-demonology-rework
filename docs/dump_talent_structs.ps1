# dump_talent_structs.ps1 — read WoW.exe talent tab structs from memory (build 12340, base 0x400000).
# HOW TO USE:
#   1) In-game: open the Talent window. To reproduce the "bleed", click DESTRUCTION spec, then DEMONOLOGY.
#      (Leave the talent window open so the structs stay populated.)
#   2) In PowerShell (does NOT need admin on a local single-player setup, but run as admin if it can't open
#      the process):  powershell -ExecutionPolicy Bypass -File .\dump_talent_structs.ps1 > talentdump.txt
#   3) Send me talentdump.txt.
#
# It reads: player tab-count @0x00C2101C, tab-array @0x00C21020; per tab struct [+0]=cap [+4]=count
# [+8]=array; each entry 0x5C bytes (=Talent.dbc record: +0 ID, +4 TabID, +8 Tier, +0xC Column).

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class PM {
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(int a, bool i, int p);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr h, IntPtr a, byte[] b, int s, out int r);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
}
"@

$proc = Get-Process -Name Wow,WoW -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Output "Wow.exe not running"; exit 1 }
$base = $proc.MainModule.BaseAddress.ToInt64()
Write-Output ("Wow.exe pid={0} module base=0x{1:X}" -f $proc.Id, $base)
$delta = $base - 0x400000   # rebase if ASLR moved it (should be 0)

$h = [PM]::OpenProcess(0x10, $false, $proc.Id)   # PROCESS_VM_READ
if ($h -eq [IntPtr]::Zero) { Write-Output "OpenProcess failed (try running PowerShell as Administrator)"; exit 1 }

function RU32([int64]$addr) {
  $buf = New-Object byte[] 4; $read = 0
  $ok = [PM]::ReadProcessMemory([IntPtr]($addr), $buf, 4, [ref]$read)
  if (-not $ok -or $read -ne 4) { return $null }
  return [BitConverter]::ToUInt32($buf,0)
}

$TABCNT = 0x00C2101C + $delta
$TABARR = 0x00C21020 + $delta
$tabCount = RU32 $TABCNT
$tabArr   = RU32 $TABARR
Write-Output ("player tabCount={0}  tabArray=0x{1:X}" -f $tabCount, $tabArr)
if (-not $tabCount -or -not $tabArr) { Write-Output "structs not populated — open the Talent window first"; exit 1 }

for ($t = 0; $t -lt [Math]::Min($tabCount, 6); $t++) {
  $st = RU32 ($tabArr + $t*4)
  if (-not $st) { continue }
  $cap = RU32 ($st + 0); $cnt = RU32 ($st + 4); $arr = RU32 ($st + 8)
  Write-Output ""
  Write-Output ("=== tab index {0}: struct=0x{1:X}  capacity={2}  count={3}  array=0x{4:X} ===" -f $t,$st,$cap,$cnt,$arr)
  if (-not $arr) { continue }
  # dump entries up to count, plus 10 extra to catch stale/bleed beyond count
  $limit = [Math]::Min([int]$cnt + 10, 60)
  for ($j = 0; $j -lt $limit; $j++) {
    $e   = $arr + $j*0x5C
    $id  = RU32 ($e + 0); $tab = RU32 ($e + 4); $tier = RU32 ($e + 8); $col = RU32 ($e + 0xC)
    $r1  = RU32 ($e + 0x10)   # SpellRank[1]
    if ($id -eq $null) { break }
    $mark = if ($j -ge [int]$cnt) { " (beyond count)" } else { "" }
    Write-Output ("  [{0,2}] id={1} tab={2} tier={3} col={4} rank1spell={5}{6}" -f $j,$id,$tab,$tier,$col,$r1,$mark)
  }
}
[PM]::CloseHandle($h) | Out-Null
Write-Output ""
Write-Output "done. Send this whole output back."
