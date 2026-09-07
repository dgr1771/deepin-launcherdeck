Add-Type -AssemblyName System.Drawing
$W = 256; $H = 256
$bmp = New-Object System.Drawing.Bitmap($W, $H)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush((New-Object System.Drawing.Point(0,0)), (New-Object System.Drawing.Point($W,$H)), [System.Drawing.Color]::FromArgb(255,35,43,82), [System.Drawing.Color]::FromArgb(255,18,24,48))
$rr = New-Object System.Drawing.Drawing2D.GraphicsPath
$rr.AddRoundedRect(8, 8, 240, 240, 40, 40)
$g.FillPath($bg, $rr)
$pen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255,88,101,242), 8)
$g.DrawPath($pen, $rr)
$gold = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,254,231,92))
$cx = 128.0; $cy = 128.0; $R = 80.0
$star = New-Object System.Drawing.Drawing2D.GraphicsPath
$star.AddEllipse($cx-$R, $cy-$R, $R*2, $R*2)
$g.FillPath($gold, $star)
$dark = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255,17,19,27))
$star2 = New-Object System.Drawing.Drawing2D.GraphicsPath
$star2.AddEllipse($cx-$R*0.7, $cy-$R*0.7, $R*1.4, $R*1.4)
$g.FillPath($dark, $star2)
$g.FillPath($gold, $star)
$g.Dispose()
$bmp.Save("C:\Users\67842\ZCodeProject\deepin-launcherdeck\assets\launcher-deck.png", [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "icon saved"
