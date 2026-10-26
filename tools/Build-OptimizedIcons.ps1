param(
    [string]$SourceDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'assets\icons-source'),
    [string]$OutputDirectory = (Join-Path (Split-Path $PSScriptRoot -Parent) 'assets\icons'),
    [string[]]$Names = @()
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

if (-not ('AtlasIconBuilder' -as [type])) {
    Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;

public static class AtlasIconBuilder
{
    public static void Build(string sourcePath, string destinationPath, int size)
    {
        using (var source = new Bitmap(sourcePath))
        {
            int left = source.Width, top = source.Height, right = -1, bottom = -1;
            for (int y = 0; y < source.Height; ++y)
            {
                for (int x = 0; x < source.Width; ++x)
                {
                    if (source.GetPixel(x, y).A <= 8) continue;
                    if (x < left) left = x;
                    if (x > right) right = x;
                    if (y < top) top = y;
                    if (y > bottom) bottom = y;
                }
            }

            if (right < left || bottom < top)
                throw new InvalidOperationException("The source icon has no visible pixels: " + sourcePath);

            int visibleWidth = right - left + 1;
            int visibleHeight = bottom - top + 1;
            int cropSide = Math.Max(visibleWidth, visibleHeight);
            cropSide = Math.Min(Math.Max(cropSide + Math.Max(4, cropSide / 14), 1), Math.Max(source.Width, source.Height));
            float centerX = (left + right + 1) * 0.5f;
            float centerY = (top + bottom + 1) * 0.5f;
            float cropX = centerX - cropSide * 0.5f;
            float cropY = centerY - cropSide * 0.5f;

            if (cropX < 0) cropX = 0;
            if (cropY < 0) cropY = 0;
            if (cropX + cropSide > source.Width) cropX = source.Width - cropSide;
            if (cropY + cropSide > source.Height) cropY = source.Height - cropSide;

            using (var output = new Bitmap(size, size, PixelFormat.Format32bppArgb))
            using (var graphics = Graphics.FromImage(output))
            using (var attributes = new ImageAttributes())
            {
                graphics.Clear(Color.Transparent);
                graphics.CompositingMode = CompositingMode.SourceCopy;
                graphics.CompositingQuality = CompositingQuality.HighQuality;
                graphics.InterpolationMode = InterpolationMode.HighQualityBicubic;
                graphics.PixelOffsetMode = PixelOffsetMode.HighQuality;
                graphics.SmoothingMode = SmoothingMode.HighQuality;
                attributes.SetWrapMode(WrapMode.TileFlipXY);
                graphics.DrawImage(source,
                    new Rectangle(0, 0, size, size),
                    cropX, cropY, cropSide, cropSide,
                    GraphicsUnit.Pixel, attributes);
                output.Save(destinationPath, ImageFormat.Png);
            }
        }
    }
}
'@
}

if ($Names.Count -eq 0) {
    $Names = Get-ChildItem -LiteralPath $SourceDirectory -Filter '*.png' | Select-Object -ExpandProperty BaseName
}
foreach ($size in 64, 128) {
    $target = Join-Path $OutputDirectory $size
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    foreach ($name in $Names) {
        $source = Join-Path $SourceDirectory ($name + '.png')
        $destination = Join-Path $target ($name + '.png')
        [AtlasIconBuilder]::Build($source, $destination, $size)
    }
}

Write-Output "Generated optimized Atlas icons in $OutputDirectory"
