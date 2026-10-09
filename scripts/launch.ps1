# Abre o MGSV pelo Steam e mostra o log do mod por alguns segundos.
# Uso: powershell -ExecutionPolicy Bypass -File scripts/launch.ps1 [-Seconds 60]
param(
    [int]$Seconds = 60,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP'
)
$ErrorActionPreference = 'Stop'
if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue) { throw 'O jogo ja esta aberto.' }

$log = Join-Path $GameDir 'mgsv_saves\mgsv_saves.log'
$started = Get-Date
Start-Process 'steam://rungameid/287700'

$seen = $false
$deadline = $started.AddSeconds($Seconds)
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if (Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue) { $seen = $true }
    elseif ($seen) { break }  # abriu e fechou
}

$running = [bool](Get-Process -Name mgsvtpp -ErrorAction SilentlyContinue)
Write-Host "Processo visto: $seen | rodando agora: $running"
if ((Test-Path $log) -and (Get-Item $log).LastWriteTime -ge $started) {
    Get-Content $log
} else {
    Write-Warning 'O log do mod nao foi escrito nesta execucao (a msacm32.dll nao carregou?).'
}
