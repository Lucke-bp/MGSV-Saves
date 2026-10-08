# Sends commands to the mod (command.txt) and prints the log lines written during the next seconds.
# Usage: powershell -File tools/remote.ps1 "lua return vars.missionCode" [-Seconds 3] [-Filter regex]
param([Parameter(Mandatory)][string]$Command, [int]$Seconds = 3, [string]$Filter = '')
$dir = 'C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP\mgsv_saves'
$log = Join-Path $dir 'mgsv_saves.log'
$before = (Get-Content $log).Count
[IO.File]::WriteAllText((Join-Path $dir 'command.txt'), $Command, (New-Object Text.UTF8Encoding $false))
Start-Sleep -Seconds $Seconds
$lines = Get-Content $log -Encoding utf8 | Select-Object -Skip $before
if ($Filter) { $lines = $lines | Where-Object { $_ -match $Filter } }
$lines | ForEach-Object { if ($_.Length -gt 600) { $_.Substring(0, 600) } else { $_ } }
