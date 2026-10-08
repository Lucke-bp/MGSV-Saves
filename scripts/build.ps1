# Compila build\dinput8.dll (proxy + mod) com o MSVC (x64, Release, CRT estatico).
# Uso: powershell -ExecutionPolicy Bypass -File scripts/build.ps1
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build'
$Obj = Join-Path $Build 'obj'
New-Item -ItemType Directory -Force $Obj | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio com C++ (x64) nao encontrado.' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"  # o vcvars chama o vswhere

$sources = (Get-ChildItem (Join-Path $Root 'src') -Filter *.cpp | ForEach-Object { "`"$($_.FullName)`"" }) -join ' '
$cflags = '/nologo /std:c++20 /O2 /MT /EHsc /W4 /permissive- /utf-8 /MP /Zi /DNDEBUG /DUNICODE /D_UNICODE'
$libs = 'user32.lib advapi32.lib shell32.lib version.lib'
$out = Join-Path $Build 'dinput8.dll'

$cmd = "`"$vcvars`" >nul && cl $cflags $sources /Fo`"$Obj\\`" /Fd`"$Obj\\`" /LD /link /DEBUG /OPT:REF /OPT:ICF /OUT:`"$out`" /PDB:`"$Build\dinput8.pdb`" /IMPLIB:`"$Obj\dinput8.lib`" $libs"
Push-Location $Build
try { cmd /c $cmd } finally { Pop-Location }
if ($LASTEXITCODE -ne 0) { throw "Build falhou (codigo $LASTEXITCODE)" }
Write-Host "OK: $out"
