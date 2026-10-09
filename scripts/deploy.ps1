# Copia o mod (msacm32.dll + scripts Lua) para a pasta do jogo. Nao altera nenhum arquivo original do jogo.
# Uso: powershell -ExecutionPolicy Bypass -File scripts/deploy.ps1 [-Force]
#   -Force: sobrescreve uma msacm32.dll que nao foi instalada por este script
param(
    [switch]$Force,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP'
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path (Join-Path $GameDir 'mgsvtpp.exe'))) { throw "mgsvtpp.exe nao encontrado em $GameDir" }
if (Get-Process -Name mgsvtpp, mgsvmgo -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes do deploy.' }

$dll = Join-Path $Root 'build\msacm32.dll'
if (-not (Test-Path $dll)) { throw 'Mod nao compilado. Rode scripts/build.ps1.' }

$modDir = Join-Path $GameDir 'mgsv_saves'
New-Item -ItemType Directory -Force $modDir | Out-Null

function Get-Marker([string]$path) { if (Test-Path $path) { (Get-Content $path -Raw).Trim() } }

# Nao sobrescrever uma msacm32.dll de outro programa: guardamos o hash do que instalamos.
$marker = Join-Path $modDir 'installed_msacm32.sha256'
$target = Join-Path $GameDir 'msacm32.dll'
if ((Test-Path $target) -and -not $Force) {
    if ((Get-FileHash $target -Algorithm SHA256).Hash -ne (Get-Marker $marker)) {
        throw "Ja existe uma msacm32.dll que nao e deste mod em $GameDir. Use -Force para sobrescrever."
    }
}

# Versoes antigas do mod eram dinput8.dll: remove-se so a nossa (a do IHHook/tpp-mod fica).
$oldMarker = Join-Path $modDir 'installed_dinput8.sha256'
$oldDll = Join-Path $GameDir 'dinput8.dll'
if ((Test-Path $oldDll) -and (Get-FileHash $oldDll -Algorithm SHA256).Hash -eq (Get-Marker $oldMarker)) {
    Remove-Item $oldDll
    Write-Host 'Removida a dinput8.dll de uma versao antiga do mod'
}
if (Test-Path $oldMarker) { Remove-Item $oldMarker }

Copy-Item $dll $target -Force
(Get-FileHash $target -Algorithm SHA256).Hash | Set-Content $marker -Encoding ascii
# The mod's Lua scripts
$scripts = Get-ChildItem (Join-Path $Root 'lua') -Filter *.lua -ErrorAction SilentlyContinue
foreach ($s in $scripts) { Copy-Item $s.FullName $modDir -Force }
Write-Host "Copiado: msacm32.dll + $($scripts.Count) script(s) Lua -> $GameDir"
