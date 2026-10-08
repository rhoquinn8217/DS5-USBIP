# Builds DS5-USBIP's installer: the Release exe, then Inno Setup, leaving
# out\installer\DS5-USBIP-Setup.exe.
#
#   .\installer\build-ds5-installer.ps1
#   .\installer\build-ds5-installer.ps1 -SkipBuild     (compile the .iss only)
#
# ============================================================================
# WHY THIS EXISTS BESIDE ciprianmisaila's build-installer.ps1
# ============================================================================
#
# His is left where it is. It builds his product: it publishes CipriansBridge
# with dotnet, compiles ctm-usbip.iss (the service installer), and copies the
# result to D:\Work\CTM\dist, a path on his machine. None of that is ours.
#
# ============================================================================
# THE VERSION IS READ, NEVER TYPED
# ============================================================================
#
# include\ctm\product.h is the one place the version is written, and the window
# title, --version and the exe's own fields all read from it. A number typed
# into the installer would drift from the binary and then name the wrong build
# with complete confidence. release.ps1 reads it the same way, on purpose, so
# the zip and the installer cannot disagree about what they hold.
#
# ============================================================================
# IT REFUSES WHILE A PERSON'S FILES WOULD LAND IN PROGRAM FILES
# ============================================================================
#
# The installer puts the program in C:\Program Files\DS5-USBIP, which a normal
# user cannot write to. The listener still resolves configs, device.log and the
# window state relative to its own folder, so an install made today would look
# fine and then silently fail to save anything a person changed. That is the
# worst kind of broken: nothing errors, the settings just never come back.
#
# So this stops. Pass -AllowUnwritableConfigs to build one anyway for testing,
# knowing it cannot keep settings.

param(
  [string]$Configuration = 'Release',
  [switch]$SkipBuild,
  [switch]$AllowUnwritableConfigs
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repo

# --- the version, from the one place it is written --------------------------
$productHeader = Join-Path $repo 'include\ctm\product.h'
$match = Select-String -Path $productHeader -Pattern '#define PRODUCT_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+)"'
if (-not $match) { throw "no PRODUCT_VERSION in $productHeader" }
$version = $match.Matches[0].Groups[1].Value
Write-Host "version: $version (from include/ctm/product.h)"

# --- refuse while a person's files would be unwritable -----------------------
# Read off the source rather than assumed. Crude on purpose: a wrong YES here
# ships an installer that loses settings, so it fails closed.
#
# IT LOOKS IN main.cpp, NOT IN home_folder.inl. This guard was first written
# expecting the fix to land in home_folder.inl, which composes paths. It did
# not: the answer turned out to be moving the WORKING DIRECTORY to the person's
# folder in settle_home, so that every relative path follows it and no file
# handling had to change. home_folder.inl still only finds the program's home
# and says nothing about where a person's files go, so grepping it would refuse
# for ever.
$settleHome = Join-Path $repo 'src\main.cpp'
$perUserHome = (Test-Path $settleHome) -and
               (Select-String -Path $settleHome -Pattern 'LOCALAPPDATA' -Quiet)
if (-not $perUserHome) {
    $msg = @(
        "The listener still keeps configs, device.log and the window state beside its exe."
        "An install into Program Files cannot write them, and nothing will say so:"
        "settings simply never come back."
        ""
        "Move the person's files to a per-user folder first, or pass"
        "-AllowUnwritableConfigs to build one for testing anyway."
    ) -join [Environment]::NewLine
    if (-not $AllowUnwritableConfigs) { throw $msg }
    Write-Warning $msg
}

# --- build ------------------------------------------------------------------
if (-not $SkipBuild) {
    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "build.ps1 failed" }
}

$exe = Join-Path $repo "out\x64\$Configuration\ctm-usbip.exe"
if (-not (Test-Path $exe)) { throw "ctm-usbip.exe not found at $exe -- build it first" }

# --- Inno Setup -------------------------------------------------------------
# winget puts it under LOCALAPPDATA; an installer run as admin puts it in
# Program Files. Both are looked for rather than one being assumed.
$iscc = @(
    (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
    (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    throw "Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup"
}
Write-Host "iscc:    $iscc"

& $iscc "/DAppVersion=$version" (Join-Path $repo 'installer\ds5-usbip.iss')
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }

$setup = Join-Path $repo 'out\installer\DS5-USBIP-Setup.exe'
if (-not (Test-Path $setup)) { throw "ISCC reported success but $setup is not there" }

$size = [math]::Round((Get-Item $setup).Length / 1MB, 1)
Write-Host ""
Write-Host "installer: $setup  ($size MB)"
Write-Host "NOTE: unsigned, so SmartScreen warns on first run."
