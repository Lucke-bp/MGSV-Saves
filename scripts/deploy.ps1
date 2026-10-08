# Copia o mod (dinput8.dll) para a pasta do jogo. Nao altera nenhum arquivo original do jogo.
# Uso: powershell -ExecutionPolicy Bypass -File scripts/deploy.ps1 [-Force]
#   -Force: sobrescreve uma dinput8.dll que nao foi instalada por este script (nem pelo mgsv-dlss)
param(
    [switch]$Force,
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\MGS_TPP'
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

if (-not (Test-Path (Join-Path $GameDir 'mgsvtpp.exe'))) { throw "mgsvtpp.exe nao encontrado em $GameDir" }
if (Get-Process -Name mgsvtpp, mgsvmgo -ErrorAction SilentlyContinue) { throw 'Feche o jogo antes do deploy.' }

$dll = Join-Path $Root 'build\dinput8.dll'
if (-not (Test-Path $dll)) { throw 'Mod nao compilado. Rode scripts/build.ps1.' }

# Nao sobrescrever uma dinput8.dll de outro mod (ex.: IHHook). Guardamos o hash do que instalamos; a do projeto
# anterior do Lucas (mgsv-dlss, arquivado) tambem pode ser substituida.
$modDir = Join-Path $GameDir 'mgsv_saves'
New-Item -ItemType Directory -Force $modDir | Out-Null
$marker = Join-Path $modDir 'installed_dinput8.sha256'
$target = Join-Path $GameDir 'dinput8.dll'
if ((Test-Path $target) -and -not $Force) {
    $current = (Get-FileHash $target -Algorithm SHA256).Hash
    $ours = foreach ($m in $marker, (Join-Path $GameDir 'mgsv_dlss\installed_dinput8.sha256')) { if (Test-Path $m) { (Get-Content $m -Raw).Trim() } }
    if ($ours -notcontains $current) { throw "Ja existe uma dinput8.dll que nao e deste mod em $GameDir. Use -Force para sobrescrever." }
}

Copy-Item $dll $target -Force
(Get-FileHash $target -Algorithm SHA256).Hash | Set-Content $marker -Encoding ascii
Write-Host "Copiado: dinput8.dll -> $GameDir"
