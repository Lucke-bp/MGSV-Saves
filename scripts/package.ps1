# Monta o pacote de lancamento (Nexus Mods): dist\MGSV_Saves_<versao>.zip
#   MSACM32.dll, mgsv_saves\mgsv_saves.lua, mgsv_saves\mgsv_saves_lang.lua, README.txt, LICENSE.txt
# Sem o marcador .dev (o console command.txt fica desligado para os jogadores), sem .ini, log ou saves.
# Uso: powershell -ExecutionPolicy Bypass -File scripts/package.ps1
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

$header = Get-Content (Join-Path $Root 'src\common.hpp') -Raw
if ($header -notmatch '#define MOD_VERSION "([^"]+)"') { throw 'MOD_VERSION nao encontrado em src\common.hpp' }
$version = $Matches[1]

& (Join-Path $PSScriptRoot 'build.ps1')

$dist = Join-Path $Root 'dist'
$stage = Join-Path $dist "MGSV_Saves_$version"
if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage 'mgsv_saves') | Out-Null

Copy-Item (Join-Path $Root 'build\msacm32.dll') (Join-Path $stage 'MSACM32.dll')
foreach ($script in 'mgsv_saves.lua', 'mgsv_saves_lang.lua') {
    Copy-Item (Join-Path $Root "lua\$script") (Join-Path $stage "mgsv_saves\$script")
}
# Text files with Windows line endings, so any editor shows them right
$utf8 = New-Object Text.UTF8Encoding $false
foreach ($text in 'README.txt', 'LICENSE.txt') {
    $content = [IO.File]::ReadAllText((Join-Path $Root "package\$text")).Replace("`r`n", "`n").Replace("`n", "`r`n")
    [IO.File]::WriteAllText((Join-Path $stage $text), $content.Replace('{VERSION}', $version), $utf8)
}

$zip = Join-Path $dist "MGSV_Saves_$version.zip"
if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
# Entries with '/' (the zip format's separator): Compress-Archive in PowerShell 5.1 writes '\', which some extractors
# (and mod managers) take as part of the file name.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem $stage -Recurse -File) {
        $entry = $file.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry, [IO.Compression.CompressionLevel]::Optimal)
    }
} finally { $archive.Dispose() }
Write-Host "OK: $zip"
Get-ChildItem $stage -Recurse -File | ForEach-Object { '  {0} ({1:N0} bytes)' -f $_.FullName.Substring($stage.Length + 1), $_.Length }
