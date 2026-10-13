# Builds app\ctm-usbip.ico: the DualSense outline on a flat-panel television.
#
# ---------------------------------------------------------------------------
# THE PIPELINE. Both halves live in this folder and either can be re-run:
#
#     icon-source.png  --[ trace-silhouette.py ]-->  controller-path.txt
#     controller-path.txt  --[ this script ]-->  ..\app\ctm-usbip.ico
#
# IMPORTANT: ONE FILE DRESSES THE WHOLE APP. app\ctm-usbip.rc links the .ico as
# `1 ICON` for the exe, taskbar and Alt-Tab, and AGAIN as `101 RCDATA` so the
# identical bytes are served at /favicon.ico -- which is where the Controller
# Configs window's title bar gets its icon. Both .iss files aim SetupIconFile
# at it, and the tray and window load it from the exe's own resources. Replace
# this file and every surface follows; there is nowhere else to edit.
#
# WARNING: make-icon-ds5.ps1 AND make-icon.ps1 ARE RETIRED and must not be run.
# They build the icon from installer\brand.png, which is Ciprian's bridge art,
# and running either silently puts the old black bridge back with no error and
# no diff anyone would read. They are kept only because brand.png is upstream's
# and the history is worth preserving.
#
# -Check WRITES NOTHING. It builds the icon in memory and compares it with the
# committed file byte for byte, exiting 1 if they differ. That is the whole of
# "a generated file must have a generator": the claim is only true while this
# passes.
# ---------------------------------------------------------------------------
#
# WHY THE FRAMES ARE SUPERSAMPLED. Filling a Bezier path straight into a 16x16
# bitmap leans entirely on GDI+'s own antialiasing, which is coarse on tight
# curves -- the grips come out notched. Each frame is drawn at four times its
# size and scaled down with HighQualityBicubic instead, so every edge pixel is
# an average of sixteen. This is the same reasoning as scale-then-treat in the
# script this one replaces, one step further on.

param([switch]$Check)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$pathFile = Join-Path $PSScriptRoot 'controller-path.txt'
$dst = Join-Path $PSScriptRoot '..\app\ctm-usbip.ico'

# ONE FRAME FOR EVERY SIZE WINDOWS ASKS FOR, at every stock display scale from
# 100% to 300%. Windows has a small icon, 16 units, and a big one, 32 units,
# and a unit is one pixel at 100%:
#
#     scale    100  125  150  175  200  225  250  300
#     small     16   20   24   28   32   36   40   48
#     big       32   40   48   56   64   72   80   96
#
# WARNING: a size missing here is not an error anywhere. Windows stretches the
# nearest frame and says nothing, so the icon is simply soft at that one scale.
# 128 and 256 are for Explorer's large views.
# NOTE: src\app\window_icon_rule.inl works the same sizes out for the configs
# window's taskbar icon, and its test carries this table. Keep them in step.
$sizes = 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 72, 80, 96, 128, 256

$SS = 4                     # supersample factor

# --- the design, in a 100 x 100 canvas -------------------------------------
# Screen 96 x 66 is 1.45:1, read off a flat-panel reference. The pedestal is
# 66 wide (69% of the screen) and 5 tall, floating 3 below it; at small sizes
# that gap closes and the bar reads as an attached stand, which is what a
# television looks like anyway.
$NAVY = [System.Drawing.Color]::FromArgb(255, 17, 24, 39)      # #111827
$WHITE = [System.Drawing.Color]::White

$SCREEN_X = 2.0;  $SCREEN_W = 96.0;  $SCREEN_H = 66.0;  $SCREEN_R = 8.0
$GAP = 3.0
$BAR_W = 66.0;    $BAR_H = 5.0;      $BAR_R = 2.5
$CTRL_S = 0.78                                                 # controller scale

$TOP = (100.0 - ($SCREEN_H + $GAP + $BAR_H)) / 2.0
$SCREEN_Y = $TOP
$BAR_X = (100.0 - $BAR_W) / 2.0
$BAR_Y = $TOP + $SCREEN_H + $GAP

# --- read the traced outline -----------------------------------------------
if (-not (Test-Path $pathFile)) {
    throw "missing $pathFile -- run trace-silhouette.py first"
}
$height = $null
$start = $null
$beziers = New-Object System.Collections.Generic.List[double[]]
foreach ($line in Get-Content $pathFile) {
    $t = $line.Trim()
    if ($t.Length -eq 0 -or $t.StartsWith('#')) { continue }
    $f = $t -split '\s+'
    switch ($f[0]) {
        'height' { $height = [double]$f[1] }
        'start'  { $start = @([double]$f[1], [double]$f[2]) }
        'bez'    {
            $v = New-Object double[] 6
            for ($i = 0; $i -lt 6; $i++) { $v[$i] = [double]$f[$i + 1] }
            $beziers.Add($v)
        }
    }
}
if ($null -eq $height -or $null -eq $start -or $beziers.Count -eq 0) {
    throw "$pathFile is not a traced outline"
}

# The controller is centred on the screen. Its frame is 100 wide, so at scale
# s it is 100*s wide and sits (100 - 100*s)/2 from the left.
$ctrlDX = (100.0 - 100.0 * $CTRL_S) / 2.0
$ctrlDY = $SCREEN_Y + $SCREEN_H / 2.0 - ($height * $CTRL_S) / 2.0

function New-RoundedRect {
    param([double]$X, [double]$Y, [double]$W, [double]$H, [double]$R, [double]$K)
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [float](2.0 * $R * $K)
    $x = [float]($X * $K); $y = [float]($Y * $K)
    $w = [float]($W * $K); $h = [float]($H * $K)
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-ControllerPath {
    param([double]$K)
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $cx = $start[0]; $cy = $start[1]
    foreach ($b in $beziers) {
        $p.AddBezier(
            [float]((($cx * $CTRL_S) + $ctrlDX) * $K), [float]((($cy * $CTRL_S) + $ctrlDY) * $K),
            [float]((($b[0] * $CTRL_S) + $ctrlDX) * $K), [float]((($b[1] * $CTRL_S) + $ctrlDY) * $K),
            [float]((($b[2] * $CTRL_S) + $ctrlDX) * $K), [float]((($b[3] * $CTRL_S) + $ctrlDY) * $K),
            [float]((($b[4] * $CTRL_S) + $ctrlDX) * $K), [float]((($b[5] * $CTRL_S) + $ctrlDY) * $K))
        $cx = $b[4]; $cy = $b[5]
    }
    $p.CloseFigure()
    return $p
}

function New-Frame {
    param([int]$Size)
    $big = $Size * $SS
    $k = $big / 100.0
    $bmp = New-Object System.Drawing.Bitmap($big, $big,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    $navyBrush = New-Object System.Drawing.SolidBrush($NAVY)
    $whiteBrush = New-Object System.Drawing.SolidBrush($WHITE)

    $screen = New-RoundedRect $SCREEN_X $SCREEN_Y $SCREEN_W $SCREEN_H $SCREEN_R $k
    $g.FillPath($navyBrush, $screen); $screen.Dispose()

    $bar = New-RoundedRect $BAR_X $BAR_Y $BAR_W $BAR_H $BAR_R $k
    $g.FillPath($navyBrush, $bar); $bar.Dispose()

    $ctrl = New-ControllerPath $k
    $g.FillPath($whiteBrush, $ctrl); $ctrl.Dispose()

    $navyBrush.Dispose(); $whiteBrush.Dispose(); $g.Dispose()

    # Down to the real size. Premultiplied averaging is what makes the curves
    # clean; HighQualityBicubic on a 32bppArgb source does that for us.
    $out = New-Object System.Drawing.Bitmap($Size, $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g2 = [System.Drawing.Graphics]::FromImage($out)
    $g2.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g2.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g2.Clear([System.Drawing.Color]::Transparent)
    $g2.DrawImage($bmp, (New-Object System.Drawing.Rectangle(0, 0, $Size, $Size)))
    $g2.Dispose(); $bmp.Dispose()
    return $out
}

$frames = @()
foreach ($s in $sizes) {
    $bmp = New-Frame $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $frames += , ($ms.ToArray())
    $ms.Dispose()
    Write-Host ("  {0,3}x{1,-3} {2,7} bytes" -f $s, $s, $frames[-1].Length)
}

# --- assemble the ICO ------------------------------------------------------
$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($out)
$bw.Write([uint16]0)              # reserved
$bw.Write([uint16]1)              # type: icon
$bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $dim = if ($s -ge 256) { 0 } else { $s }   # 0 means 256 in the ICO directory
    $bw.Write([byte]$dim)
    $bw.Write([byte]$dim)
    $bw.Write([byte]0)            # palette count
    $bw.Write([byte]0)            # reserved
    $bw.Write([uint16]1)          # colour planes
    $bw.Write([uint16]32)         # bits per pixel
    $bw.Write([uint32]$frames[$i].Length)
    $bw.Write([uint32]$offset)
    $offset += $frames[$i].Length
}
foreach ($f in $frames) { $bw.Write($f) }
$bw.Flush()
$bytes = $out.ToArray()
$out.Dispose()

if ($Check) {
    if (-not (Test-Path $dst)) {
        Write-Host "DRIFT: no committed icon at $dst"
        exit 1
    }
    $have = [System.IO.File]::ReadAllBytes((Resolve-Path $dst).Path)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $made = [System.BitConverter]::ToString($sha.ComputeHash($bytes))
    $kept = [System.BitConverter]::ToString($sha.ComputeHash($have))
    $sha.Dispose()
    if ($made -eq $kept) {
        Write-Host ("OK: the committed icon is what this script makes ({0} bytes, {1} sizes)" -f $bytes.Length, $sizes.Count)
        exit 0
    }
    Write-Host "DRIFT: the committed icon is NOT what this script makes"
    Write-Host ("  committed   {0} bytes" -f $have.Length)
    Write-Host ("  this script {0} bytes" -f $bytes.Length)
    exit 1
}

[System.IO.File]::WriteAllBytes($dst, $bytes)
Write-Host ("Wrote {0} ({1} sizes, {2} bytes)" -f (Resolve-Path $dst).Path, $sizes.Count, $bytes.Length)
