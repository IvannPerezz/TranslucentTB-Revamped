param(
    # optional: also write the icon as a standalone PNG of this size, e.g. for a README or a store listing
    [string]$PreviewPath = '',
    [int]$PreviewSize = 1024
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

# Same tile, inset, stroke weights and frame sizes as Resolution Switcher's and
# AirPlay Receiver's icons so the three programs look like a set, in orange.
# Writes the window icon for every build type and the package logos.

$resources = $PSScriptRoot
$package = Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'AppPackage'

function New-RoundedRectanglePath {
    param(
        [System.Drawing.RectangleF]$Rectangle,
        [float]$Radius
    )

    $diameter = $Radius * 2.0
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc($Rectangle.X, $Rectangle.Y, $diameter, $diameter, 180.0, 90.0)
    $path.AddArc($Rectangle.Right - $diameter, $Rectangle.Y, $diameter, $diameter, 270.0, 90.0)
    $path.AddArc($Rectangle.Right - $diameter, $Rectangle.Bottom - $diameter, $diameter, $diameter, 0.0, 90.0)
    $path.AddArc($Rectangle.X, $Rectangle.Bottom - $diameter, $diameter, $diameter, 90.0, 90.0)
    $path.CloseFigure()
    return $path
}

# A screen whose bottom strip is a see-through taskbar with three apps, on a 16x16 grid.
function Add-Glyph {
    param(
        [System.Drawing.Graphics]$Graphics,
        [System.Drawing.RectangleF]$Box
    )

    [float]$u = $Box.Width / 16.0
    [float]$w = [Math]::Max(1.0, [Math]::Round($u * 1.1))

    $screen = [System.Drawing.RectangleF]::new(
        $Box.X + 1.0 * $u + $w / 2.0,
        $Box.Y + 3.0 * $u + $w / 2.0,
        14.0 * $u - $w,
        10.0 * $u - $w)

    $band = [System.Drawing.RectangleF]::new($screen.X, $Box.Y + 9.6 * $u, $screen.Width, $screen.Bottom - ($Box.Y + 9.6 * $u))
    $screenPath = New-RoundedRectanglePath $screen (1.5 * $u)
    $translucent = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(115, 255, 255, 255))
    try {
        $Graphics.SetClip($screenPath, [System.Drawing.Drawing2D.CombineMode]::Intersect)
        $Graphics.FillRectangle($translucent, $band)
        $Graphics.ResetClip()
    }
    finally {
        $translucent.Dispose()
    }

    $white = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::White)
    $pen = [System.Drawing.Pen]::new([System.Drawing.Color]::White, $w)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    try {
        [float]$app = 1.7 * $u
        # centred in the visible strip, between its top edge and the inside of the frame
        [float]$centerY = ($band.Y + $screen.Bottom - $w / 2.0) / 2.0
        foreach ($centerX in @(5.6, 8.0, 10.4)) {
            $tile = [System.Drawing.RectangleF]::new($Box.X + $centerX * $u - $app / 2.0, $centerY - $app / 2.0, $app, $app)
            $tilePath = New-RoundedRectanglePath $tile ($app * 0.3)
            try {
                $Graphics.FillPath($white, $tilePath)
            }
            finally {
                $tilePath.Dispose()
            }
        }

        $Graphics.DrawPath($pen, $screenPath)
    }
    finally {
        $screenPath.Dispose()
        $pen.Dispose()
        $white.Dispose()
    }
}

function New-IconBitmap {
    param([int]$Size)

    $bitmap = [System.Drawing.Bitmap]::new(
        $Size,
        $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    try {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $tileRect = [System.Drawing.RectangleF]::new(0.5, 0.5, $Size - 1, $Size - 1)
        $tilePath = New-RoundedRectanglePath $tileRect ($Size * 0.22)
        $tileBrush = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
            $tileRect,
            [System.Drawing.Color]::FromArgb(255, 172, 48),
            [System.Drawing.Color]::FromArgb(232, 88, 12),
            90.0)

        try {
            $graphics.FillPath($tileBrush, $tilePath)
            $graphics.SetClip($tilePath)
            [float]$inset = $Size * 0.16
            Add-Glyph $graphics ([System.Drawing.RectangleF]::new($inset, $inset, $Size - 2 * $inset, $Size - 2 * $inset))
        }
        finally {
            $tileBrush.Dispose()
            $tilePath.Dispose()
        }
    }
    finally {
        $graphics.Dispose()
    }

    return $bitmap
}

function Get-PngBytes {
    param([int]$Size)

    $bitmap = New-IconBitmap $Size
    $stream = [System.IO.MemoryStream]::new()
    try {
        $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        return ,$stream.ToArray()
    }
    finally {
        $stream.Dispose()
        $bitmap.Dispose()
    }
}

function Write-Icon {
    param([string]$Path)

    $sizes = @(16, 20, 24, 32, 40, 48, 64, 256)
    $frames = foreach ($size in $sizes) {
        [PSCustomObject]@{
            Size = $size
            Bytes = [byte[]](Get-PngBytes $size)
        }
    }

    $iconStream = [System.IO.MemoryStream]::new()
    $writer = [System.IO.BinaryWriter]::new($iconStream)
    try {
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]$frames.Count)

        [UInt32]$offset = 6 + 16 * $frames.Count
        foreach ($frame in $frames) {
            $dimension = if ($frame.Size -eq 256) { 0 } else { $frame.Size }
            $writer.Write([byte]$dimension)
            $writer.Write([byte]$dimension)
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([UInt16]1)
            $writer.Write([UInt16]32)
            $writer.Write([UInt32]$frame.Bytes.Length)
            $writer.Write($offset)
            $offset += [UInt32]$frame.Bytes.Length
        }

        foreach ($frame in $frames) {
            $writer.Write([byte[]]$frame.Bytes)
        }
        $writer.Flush()
        [System.IO.File]::WriteAllBytes($Path, $iconStream.ToArray())
    }
    finally {
        $writer.Dispose()
        $iconStream.Dispose()
    }
}

function Write-Png {
    param([string]$Path, [int]$Size)

    [System.IO.File]::WriteAllBytes($Path, [byte[]](Get-PngBytes $Size))
}

# The tile carries its own background, so plated and unplated logos are the same image.
$logos = [ordered]@{
    'Square44x44Logo.png' = 44
    'Square44x44Logo.altform-unplated.png' = 44
    'Square44x44Logo.altform-lightunplated.png' = 44
    'Square44x44Logo.targetsize-256.png' = 256
    'Square44x44Logo.targetsize-256_altform-unplated.png' = 256
    'Square44x44Logo.targetsize-256_altform-lightunplated.png' = 256
    'Square150x150Logo.png' = 150
    'Square150x150Logo.scale-400.png' = 600
    'SmallTile.png' = 71
    'SmallTile.scale-400.png' = 284
    'StoreLogo.png' = 50
    'StoreLogo.scale-400.png' = 200
}

foreach ($buildType in @('Release', 'Canary', 'Dev')) {
    Write-Icon (Join-Path $resources "TranslucentTB.$($buildType.ToLowerInvariant()).ico")

    $assets = Join-Path $package "Assets-$buildType"
    foreach ($logo in $logos.GetEnumerator()) {
        Write-Png (Join-Path $assets $logo.Key) $logo.Value
    }
}

if ($PreviewPath) {
    Write-Png $PreviewPath $PreviewSize
}
