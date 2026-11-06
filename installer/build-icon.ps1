# Builds the app's two icons: a DualSense on a flat-panel television, once for
# light backgrounds and once for dark ones.
#
# ---------------------------------------------------------------------------
# THE PIPELINE. Both halves live in this folder and either can be re-run:
#
#     icon-source.png  --[ trace-silhouette.py ]-->  controller-path.txt
#     controller-path.txt  --[ this script ]-->  ..\app\ctm-usbip.ico
#                                                ..\app\ctm-usbip-dark.ico
#
# TWO FILES DRESS THE WHOLE APP (rhoquinn8217, 2026-11-05: a white base on a
# dark background, a black one on a light background, and the light one's
# colours swapped for the dark one):
#
#   ctm-usbip.ico       FOR LIGHT BACKGROUNDS: a black television, a black
#                       controller outlined in white, white inner handles, and
#                       a glare across the glass. It is the exe's own icon
#                       (`1 ICON` in app\ctm-usbip.rc), so it is what File
#                       Explorer, the Start menu, shortcuts and Alt-Tab show,
#                       and both .iss files aim SetupIconFile at it (their
#                       word: "bottom", the light-background one, for every
#                       place that can only show one).
#   ctm-usbip-dark.ico  FOR DARK BACKGROUNDS: the same with its two colours
#                       swapped, and no glare. `2 ICON`.
#
# Both are linked a second time as raw bytes, `101 RCDATA` and `102 RCDATA`,
# and served at /favicon.ico for the Controller Configs window's title bar. The
# listener picks between them by Windows' light or dark mode
# (src\app\icon_theme.inl): the tray and the window's taskbar button follow the
# taskbar's mode, the title bar the apps' mode. Replace these two files and
# every surface follows; there is nowhere else to edit.
#
# WARNING: make-icon-ds5.ps1 AND make-icon.ps1 ARE RETIRED and must not be run.
# They build the icon from installer\brand.png, which is Ciprian's bridge art,
# and running either silently puts the old black bridge back with no error and
# no diff anyone would read. They are kept only because brand.png is upstream's
# and the history is worth preserving.
#
# -Png <folder> WRITES TWO PICTURES AND NOTHING ELSE: each version as one PNG
# at -PngSize (512 by default), for the TV app's webOS icon, which aurora-tv
# draws its badge from (rhoquinn8217, 2026-11-05: "Pass the icon to
# aurora-rhqn to update the app icon on webOS"). The .ico files are untouched.
#
# -Check WRITES NOTHING. It builds both icons in memory and compares each with
# the committed file byte for byte, exiting 1 if either differs. That is the
# whole of "a generated file must have a generator": the claim is only true
# while this passes.
# ---------------------------------------------------------------------------
#
# WHY THE FRAMES ARE SUPERSAMPLED. Filling a Bezier path straight into a 16x16
# bitmap leans entirely on GDI+'s own antialiasing, which is coarse on tight
# curves -- the grips come out notched. Each frame is drawn at four times its
# size and scaled down with HighQualityBicubic instead, so every edge pixel is
# an average of sixteen.
#
# ASCII only: PowerShell 5.1 reads a .ps1 with no BOM as ANSI.

param([switch]$Check, [string]$Png = '', [int]$PngSize = 512)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$pathFile = Join-Path $PSScriptRoot 'controller-path.txt'
$dstLight = Join-Path $PSScriptRoot '..\app\ctm-usbip.ico'
$dstDark = Join-Path $PSScriptRoot '..\app\ctm-usbip-dark.ico'

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
# Every part of it was asked for by rhoquinn8217 on 2026-11-05, over eighteen
# mock-ups: a dark edge round the television; the base thicker and close under
# it, then a third thinner; the controller with a line over the touchpad, its
# touchpad and stick area dark, its inner handles in a band of their own and
# shoulder buttons on top; a straight glare through the middle, slanted 15
# degrees from upright; then the two versions above.
# NOTE: the touchpad's shape is back at their word. The first icon left it out
# on purpose, as the most recognisable part of the controller's design.
$WHITE = [System.Drawing.Color]::White
$DARK = [System.Drawing.Color]::FromArgb(255, 17, 24, 39)      # #111827

$SCREEN_X = 2.0;  $SCREEN_W = 96.0;  $SCREEN_H = 66.0;  $SCREEN_R = 8.0
$GAP = 1.0                  # the base close under the screen
$BAR_W = 66.0;    $BAR_H = 5.5;      $BAR_R = 2.75
$CTRL_S = 0.78              # the controller's scale on the screen
$OUTLINE = 3.5              # the television's edge, inside the screen
$LINE_W = 1.2               # every line on the controller (controller units);
                            # never under one pixel of the finished frame
$PAD_RISE = 2.0             # the touchpad's top edge: its curve's control points
$STRIP_IN = 3.6             # the inner handles' band, from their edge
$BUMP_L = 13.0; $BUMP_H = 3.6
# Each shoulder button: its centre (controller units) and its slope, along
# the top edge of its shoulder, half of it behind the controller.
$BUMPS = @(@(20.5, 4.28, -8.5), @(79.5, 4.03, 8.5))
$GLARE_MIN = 40             # no glare below this size: it muddies the small frames
$GLARE_TILT = 15.0          # degrees from upright
$GLARE_BAND = 30.0          # how far the lit side fades out
$GLARE_ALPHA = 140          # its brightest, along the edge

$TOP = (100.0 - ($SCREEN_H + $GAP + $BAR_H)) / 2.0
$SCREEN_Y = $TOP
$BAR_X = (100.0 - $BAR_W) / 2.0
$BAR_Y = $TOP + $SCREEN_H + $GAP

# --- the traced outline ----------------------------------------------------
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
# The places on the outline this script draws along, by the number of the
# bezier that ENDS there (the outline runs clockwise from the touchpad's top
# left): 1 to 11 are the touchpad's notch, 43 to 46 the left handle's inner
# side down round its foot, 28 to 31 the right handle's, from its foot up.
if ($beziers.Count -ne 63) {
    throw "$pathFile has $($beziers.Count) beziers where this script expects 63: the numbers below are positions on that outline"
}

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

# A point of the controller (its own units) on the frame, and one of the canvas.
# NOTE: not `CP`, which PowerShell already has as an alias of Copy-Item.
function ToPt([double]$X, [double]$Y, [double]$K) {
    return New-Object System.Drawing.PointF([float]((($X * $CTRL_S) + $ctrlDX) * $K), [float]((($Y * $CTRL_S) + $ctrlDY) * $K))
}
function DesignPt([double]$X, [double]$Y, [double]$K) {
    return New-Object System.Drawing.PointF([float]($X * $K), [float]($Y * $K))
}
function BezStart([int]$I) {
    if ($I -eq 1) { return @($start[0], $start[1]) }
    $p = $beziers[$I - 2]
    return @($p[4], $p[5])
}

function New-ControllerPath {
    param([double]$K)
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $cx = $start[0]; $cy = $start[1]
    foreach ($b in $beziers) {
        $p.AddBezier((ToPt -X $cx -Y $cy -K $K), (ToPt -X $b[0] -Y $b[1] -K $K),
                     (ToPt -X $b[2] -Y $b[3] -K $K), (ToPt -X $b[4] -Y $b[5] -K $K))
        $cx = $b[4]; $cy = $b[5]
    }
    $p.CloseFigure()
    return $p
}

# Points along the outline's beziers $From..$To, each moved $D inside the
# controller along the outline's own normal: a line that follows its curve.
function Get-OffsetLine([int]$From, [int]$To, [double]$D, [double]$K) {
    $pts = New-Object System.Collections.Generic.List[System.Drawing.PointF]
    for ($i = $From; $i -le $To; $i++) {
        $p0 = BezStart -I $i
        $b = $beziers[$i - 1]
        for ($n = 0; $n -le 12; $n++) {
            if ($i -gt $From -and $n -eq 0) { continue }
            $t = $n / 12.0; $u = 1.0 - $t
            $x = $u*$u*$u*$p0[0] + 3*$u*$u*$t*$b[0] + 3*$u*$t*$t*$b[2] + $t*$t*$t*$b[4]
            $y = $u*$u*$u*$p0[1] + 3*$u*$u*$t*$b[1] + 3*$u*$t*$t*$b[3] + $t*$t*$t*$b[5]
            $dx = 3*$u*$u*($b[0]-$p0[0]) + 6*$u*$t*($b[2]-$b[0]) + 3*$t*$t*($b[4]-$b[2])
            $dy = 3*$u*$u*($b[1]-$p0[1]) + 6*$u*$t*($b[3]-$b[1]) + 3*$t*$t*($b[5]-$b[3])
            $len = [Math]::Sqrt($dx*$dx + $dy*$dy)
            if ($len -lt 1e-6) { continue }
            # The outline runs clockwise on screen, so inside is to its right.
            $pts.Add((ToPt -X ($x - $dy / $len * $D) -Y ($y + $dx / $len * $D) -K $K))
        }
    }
    return ,$pts.ToArray()
}

# A handle's inner band: the offset line, then back up the outline itself.
function New-StripPath([int]$From, [int]$To, [double]$K) {
    $pts = Get-OffsetLine -From $From -To $To -D $STRIP_IN -K $K
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddLines($pts)
    $end = BezStart -I ($To + 1)
    $p.AddLine($pts[$pts.Length - 1], (ToPt -X $end[0] -Y $end[1] -K $K))
    for ($i = $To; $i -ge $From; $i--) {
        $p0 = BezStart -I $i
        $b = $beziers[$i - 1]
        $p.AddBezier((ToPt -X $b[4] -Y $b[5] -K $K), (ToPt -X $b[2] -Y $b[3] -K $K),
                     (ToPt -X $b[0] -Y $b[1] -K $K), (ToPt -X $p0[0] -Y $p0[1] -K $K))
    }
    $p.CloseFigure()
    return $p
}

# A shoulder button: a pill centred at (Cx, Cy) in controller units, turned to
# lie along its shoulder.
function New-BumpPath([double]$Cx, [double]$Cy, [double]$Angle, [double]$K) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddArc([float](-$BUMP_L / 2), [float](-$BUMP_H / 2), [float]$BUMP_H, [float]$BUMP_H, 90, 180)
    $p.AddArc([float]($BUMP_L / 2 - $BUMP_H), [float](-$BUMP_H / 2), [float]$BUMP_H, [float]$BUMP_H, 270, 180)
    $p.CloseFigure()
    $m = New-Object System.Drawing.Drawing2D.Matrix
    $m.Rotate([float]$Angle, [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $m.Translate([float]$Cx, [float]$Cy, [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $m.Scale([float]($CTRL_S * $K), [float]($CTRL_S * $K), [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $m.Translate([float]($ctrlDX * $K), [float]($ctrlDY * $K), [System.Drawing.Drawing2D.MatrixOrder]::Append)
    $p.Transform($m)
    $m.Dispose()
    return $p
}

# The touchpad: the notch in the outline, closed by a curved top edge.
function New-PadPath([double]$K, [double]$YEnd) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $cx = $start[0]; $cy = $start[1]
    for ($i = 1; $i -le 11; $i++) {
        $b = $beziers[$i - 1]
        $p.AddBezier((ToPt -X $cx -Y $cy -K $K), (ToPt -X $b[0] -Y $b[1] -K $K),
                     (ToPt -X $b[2] -Y $b[3] -K $K), (ToPt -X $b[4] -Y $b[5] -K $K))
        $cx = $b[4]; $cy = $b[5]
    }
    $p.AddLine((ToPt -X $cx -Y $cy -K $K), (ToPt -X 71.0 -Y $YEnd -K $K))
    $p.AddBezier((ToPt -X 71.0 -Y $YEnd -K $K), (ToPt -X 57.0 -Y $PAD_RISE -K $K),
                 (ToPt -X 42.0 -Y $PAD_RISE -K $K), (ToPt -X 28.0 -Y $YEnd -K $K))
    $p.CloseFigure()
    return $p
}

# One frame. $Light: the version for light backgrounds.
function New-Frame {
    param([int]$Size, [bool]$Light)
    # The light-background version's colours; the dark one has them swapped.
    if ($Light) { $cMain = $DARK; $cLine = $WHITE } else { $cMain = $WHITE; $cLine = $DARK }

    $big = $Size * $SS
    $k = $big / 100.0
    $bmp = New-Object System.Drawing.Bitmap($big, $big,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)
    $main = New-Object System.Drawing.SolidBrush($cMain)
    $lineBrush = New-Object System.Drawing.SolidBrush($cLine)
    $thin = [float]([Math]::Max($LINE_W * $CTRL_S * $k, 1.0 * $SS))

    # The television: screen, its edge (inside it), and the base. The screen,
    # its edge and the base are one colour; the edge matters where the screen
    # meets the background, at the small sizes.
    $screen = New-RoundedRect $SCREEN_X $SCREEN_Y $SCREEN_W $SCREEN_H $SCREEN_R $k
    $g.FillPath($main, $screen)
    $g.SetClip($screen)
    $pen = New-Object System.Drawing.Pen($cMain, [float](2.0 * $OUTLINE * $k))
    $g.DrawPath($pen, $screen); $pen.Dispose()
    $g.ResetClip(); $screen.Dispose()
    $bar = New-RoundedRect $BAR_X $BAR_Y $BAR_W $BAR_H $BAR_R $k
    $g.FillPath($main, $bar); $bar.Dispose()

    # The shoulder buttons first, so the controller covers their lower half and
    # its own outline is the seam.
    foreach ($bump in $BUMPS) {
        $bp = New-BumpPath -Cx $bump[0] -Cy $bump[1] -Angle $bump[2] -K $k
        $g.FillPath($main, $bp)
        $g.SetClip($bp)
        $pen = New-Object System.Drawing.Pen($cLine, [float](2.0 * $thin))
        $g.DrawPath($pen, $bp); $pen.Dispose()
        $g.ResetClip(); $bp.Dispose()
    }

    # The controller, its touchpad and stick area in its own colour, and a band
    # of the line colour inside each handle's inner side.
    $ctrl = New-ControllerPath -K $k
    $g.FillPath($main, $ctrl)
    $g.SetClip($ctrl)
    foreach ($span in @(@(43, 46), @(28, 31))) {
        $sp = New-StripPath -From $span[0] -To $span[1] -K $k
        $g.FillPath($lineBrush, $sp); $sp.Dispose()
    }
    $g.ResetClip()
    $yEnd = $start[1] + $LINE_W / 2.0
    $pad = New-PadPath -K $k -YEnd $yEnd
    $g.FillPath($main, $pad)
    # Its edge stroked as well: two fills that meet leave a faint seam.
    $pen = New-Object System.Drawing.Pen($cMain, [float]2.0)
    $g.DrawPath($pen, $pad); $pen.Dispose(); $pad.Dispose()

    # The outline all round, inside the edge, and the touchpad's top edge.
    $g.SetClip($ctrl)
    $pen = New-Object System.Drawing.Pen($cLine, [float](2.0 * $thin))
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $g.DrawPath($pen, $ctrl); $pen.Dispose()
    $g.ResetClip()
    $pen = New-Object System.Drawing.Pen($cLine, $thin)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $g.DrawBezier($pen, (ToPt -X 28.0 -Y $yEnd -K $k), (ToPt -X 42.0 -Y $PAD_RISE -K $k),
                  (ToPt -X 57.0 -Y $PAD_RISE -K $k), (ToPt -X 71.0 -Y $yEnd -K $k))
    $pen.Dispose(); $ctrl.Dispose()

    # The glare, last and over everything on the glass, on the light-background
    # version only: a straight edge through the middle of the screen, 15
    # degrees from upright, the left side lit and fading out.
    if ($Light -and $Size -ge $GLARE_MIN) {
        $iL = $SCREEN_X + $OUTLINE; $iT = $SCREEN_Y + $OUTLINE
        $iW = $SCREEN_W - 2 * $OUTLINE; $iH = $SCREEN_H - 2 * $OUTLINE
        $glass = New-RoundedRect $iL $iT $iW $iH ($SCREEN_R - $OUTLINE) $k
        $g.SetClip($glass)
        $mx = $iL + $iW / 2.0; $my = $iT + $iH / 2.0
        $rad = $GLARE_TILT * [Math]::PI / 180.0
        $ex = [Math]::Sin($rad); $ey = - [Math]::Cos($rad)       # along the edge, up and right
        $nx = - [Math]::Cos($rad); $ny = - [Math]::Sin($rad)     # toward the lit side
        $far = 120.0
        $band = New-Object System.Drawing.Drawing2D.GraphicsPath
        $band.AddPolygon([System.Drawing.PointF[]]@(
            (DesignPt ($mx - $ex * $far) ($my - $ey * $far) $k),
            (DesignPt ($mx + $ex * $far) ($my + $ey * $far) $k),
            (DesignPt ($mx + $ex * $far + $nx * $GLARE_BAND) ($my + $ey * $far + $ny * $GLARE_BAND) $k),
            (DesignPt ($mx - $ex * $far + $nx * $GLARE_BAND) ($my - $ey * $far + $ny * $GLARE_BAND) $k)))
        $lit = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
            (DesignPt ($mx - $nx * 0.5) ($my - $ny * 0.5) $k),
            (DesignPt ($mx + $nx * ($GLARE_BAND + 0.5)) ($my + $ny * ($GLARE_BAND + 0.5)) $k),
            [System.Drawing.Color]::FromArgb($GLARE_ALPHA, 255, 255, 255),
            [System.Drawing.Color]::FromArgb(0, 255, 255, 255))
        $g.FillPath($lit, $band)
        $lit.Dispose(); $band.Dispose()
        $g.ResetClip(); $glass.Dispose()
    }

    $main.Dispose(); $lineBrush.Dispose(); $g.Dispose()

    # Down to the real size.
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

# One .ico, every size, PNG frames.
function Build-Ico([bool]$Light) {
    $frames = @()
    foreach ($s in $sizes) {
        $bmp = New-Frame -Size $s -Light $Light
        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $frames += , ($ms.ToArray())
        $ms.Dispose()
    }
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
    return ,$bytes
}

if ($Png) {
    New-Item -ItemType Directory -Force -Path $Png | Out-Null
    foreach ($v in @(@{ light = $true; name = 'ds5-usbip-icon-light' }, @{ light = $false; name = 'ds5-usbip-icon-dark' })) {
        $bmp = New-Frame -Size $PngSize -Light $v.light
        $file = Join-Path $Png ('{0}-{1}.png' -f $v.name, $PngSize)
        $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host ('Wrote {0}' -f $file)
    }
    exit 0
}

$made = @(
    @{ name = 'for light backgrounds'; path = $dstLight; bytes = (Build-Ico -Light $true) },
    @{ name = 'for dark backgrounds'; path = $dstDark; bytes = (Build-Ico -Light $false) })

if ($Check) {
    $drift = $false
    $sha = [System.Security.Cryptography.SHA256]::Create()
    foreach ($m in $made) {
        if (-not (Test-Path $m.path)) {
            Write-Host ("DRIFT: no committed icon at {0}" -f $m.path)
            $drift = $true
            continue
        }
        $have = [System.IO.File]::ReadAllBytes((Resolve-Path $m.path).Path)
        $a = [System.BitConverter]::ToString($sha.ComputeHash($m.bytes))
        $b = [System.BitConverter]::ToString($sha.ComputeHash($have))
        if ($a -eq $b) {
            Write-Host ("OK: {0} is what this script makes, {1} ({2} bytes, {3} sizes)" -f (Split-Path $m.path -Leaf), $m.name, $m.bytes.Length, $sizes.Count)
        } else {
            Write-Host ("DRIFT: {0} is NOT what this script makes (committed {1} bytes, this script {2})" -f (Split-Path $m.path -Leaf), $have.Length, $m.bytes.Length)
            $drift = $true
        }
    }
    $sha.Dispose()
    if ($drift) { exit 1 }
    exit 0
}

foreach ($m in $made) {
    [System.IO.File]::WriteAllBytes($m.path, $m.bytes)
    Write-Host ("Wrote {0}, {1} ({2} sizes, {3} bytes)" -f (Resolve-Path $m.path).Path, $m.name, $sizes.Count, $m.bytes.Length)
}
