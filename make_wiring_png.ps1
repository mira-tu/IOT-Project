Add-Type -AssemblyName System.Drawing

$width = 1400
$height = 850
$outPath = Join-Path $PSScriptRoot "esp32_cam_buttons_wiring.png"

$bmp = New-Object System.Drawing.Bitmap $width, $height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::FromArgb(248, 251, 244))

function Brush($hex) {
  return New-Object System.Drawing.SolidBrush ([System.Drawing.ColorTranslator]::FromHtml($hex))
}

function Pen($hex, $w) {
  $p = New-Object System.Drawing.Pen ([System.Drawing.ColorTranslator]::FromHtml($hex), $w)
  $p.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
  $p.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
  $p.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
  return $p
}

function FontA($size, $style = [System.Drawing.FontStyle]::Regular) {
  return New-Object System.Drawing.Font "Arial", $size, $style
}

function FillRoundRect($x, $y, $w, $h, $r, $fill, $stroke = $null, $sw = 1) {
  $path = New-Object System.Drawing.Drawing2D.GraphicsPath
  $d = $r * 2
  $path.AddArc($x, $y, $d, $d, 180, 90)
  $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
  $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
  $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
  $path.CloseFigure()
  $g.FillPath((Brush $fill), $path)
  if ($stroke) { $g.DrawPath((Pen $stroke $sw), $path) }
  $path.Dispose()
}

function DrawText($text, $x, $y, $size, $color = "#173629", $bold = $false, $center = $false) {
  $style = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }
  $font = FontA $size $style
  $brush = Brush $color
  if ($center) {
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = [System.Drawing.StringAlignment]::Center
    $g.DrawString($text, $font, $brush, [single]$x, [single]$y, $fmt)
    $fmt.Dispose()
  } else {
    $g.DrawString($text, $font, $brush, [single]$x, [single]$y)
  }
  $font.Dispose()
  $brush.Dispose()
}

function DrawWire($points, $color, $width = 8) {
  $pen = Pen $color $width
  $pts = @()
  for ($i = 0; $i -lt $points.Count; $i += 2) {
    $pts += New-Object System.Drawing.PointF ([single]$points[$i]), ([single]$points[$i + 1])
  }
  $g.DrawCurve($pen, $pts)
  $pen.Dispose()
}

DrawText "ESP32-CAM buttons to breadboard" 70 48 30 "#173629" $true
DrawText "Keep the MB board for USB power/programming. Wire each button from its GPIO pin to GND." 70 88 16 "#173629" $false

# ESP32-CAM MB board
FillRoundRect 70 145 360 590 22 "#121b18" "#2d3b35" 4
DrawText "ESP32-CAM-MB" 135 178 20 "#fff8e8" $true
DrawText "USB cable powers board" 132 212 15 "#cce8d4" $true
FillRoundRect 182 660 140 52 10 "#f1f1f1" "#a9a9a9" 3
FillRoundRect 205 675 94 22 4 "#d1d1d1"

FillRoundRect 92 250 42 320 8 "#26312d"
FillRoundRect 366 250 42 320 8 "#26312d"
for ($i = 0; $i -lt 8; $i++) {
  FillRoundRect 101 (265 + $i * 40) 24 24 4 "#d8dde0"
  FillRoundRect 375 (265 + $i * 40) 24 24 4 "#d8dde0"
}

FillRoundRect 300 285 86 34 6 "#2f7a54"
DrawText "IO13" 318 293 15 "#fff8e8" $true
FillRoundRect 300 350 86 34 6 "#c79b43"
DrawText "IO14" 318 358 15 "#fff8e8" $true
FillRoundRect 300 415 86 34 6 "#2368a2"
DrawText "IO15" 318 423 15 "#fff8e8" $true
FillRoundRect 300 535 86 34 6 "#333333"
DrawText "GND" 322 543 15 "#fff8e8" $true

# Breadboard
FillRoundRect 520 130 805 640 18 "#f7f7f7" "#cfcfcf" 4
FillRoundRect 545 155 755 60 10 "#ffffff" "#dedede" 1
$g.DrawLine((Pen "#d84a3f" 4), 565, 175, 1280, 175)
$g.DrawLine((Pen "#2368a2" 4), 565, 198, 1280, 198)
DrawText "Top rail: use the blue line as GND" 565 134 13 "#173629" $true
DrawText "GND rail" 1220 201 13 "#173629" $true
FillRoundRect 545 700 755 45 10 "#ffffff" "#dedede" 1
$g.DrawLine((Pen "#d84a3f" 4), 565, 720, 1280, 720)
$g.DrawLine((Pen "#2368a2" 4), 565, 738, 1280, 738)

FillRoundRect 570 240 700 430 12 "#ffffff" "#dedede" 1
$g.FillRectangle((Brush "#eef0f0"), 908, 240, 24, 430)
$dashPen = Pen "#c4c8c8" 3
$dashPen.DashStyle = [System.Drawing.Drawing2D.DashStyle]::Dash
$g.DrawLine($dashPen, 920, 240, 920, 670)
$dashPen.Dispose()
DrawText "center gap" 872 688 12 "#173629" $true

# Breadboard holes
$holeBrush = Brush "#d1d5d5"
foreach ($x in @(620,650,680,710,740,770,1065,1095,1125,1155,1185,1215)) {
  foreach ($y in @(265,315,365,415,465,515,565,615)) {
    $g.FillEllipse($holeBrush, $x - 4, $y - 4, 8, 8)
  }
}
$holeBrush.Dispose()

# Buttons
function DrawButton($y, $title, $signalColor) {
  FillRoundRect 830 $y 180 85 18 "#f6f3e9" "#555555" 4
  $g.FillEllipse((Brush "#e7e4d9"), 890, ($y + 12), 60, 60)
  $g.DrawEllipse((Pen "#999999" 3), 890, ($y + 12), 60, 60)
  $g.FillEllipse((Brush $signalColor), 782, ($y + 18), 16, 16)
  $g.FillEllipse((Brush "#333333"), 1042, ($y + 18), 16, 16)
  DrawText $title 920 ($y + 106) 19 "#173629" $true $true
}

DrawButton 265 "Start Stream" "#2f7a54"
DrawButton 415 "Capture && Identify" "#c79b43"
DrawButton 565 "Clear" "#2368a2"

# Wires
DrawWire @(386,302,470,302,530,285,790,285) "#2f7a54" 8
DrawWire @(386,367,490,367,540,435,790,435) "#c79b43" 8
DrawWire @(386,432,500,432,560,585,790,585) "#2368a2" 8
DrawWire @(386,552,470,552,500,198,565,198) "#222222" 8
DrawWire @(1050,285,1120,285,1160,198,1160,198) "#222222" 5
DrawWire @(1050,435,1180,435,1195,198,1195,198) "#222222" 5
DrawWire @(1050,585,1240,585,1230,198,1230,198) "#222222" 5

DrawText "IO13" 550 260 15 "#2f7a54" $true
DrawText "IO14" 555 402 15 "#9c7020" $true
DrawText "IO15" 575 547 15 "#2368a2" $true
DrawText "GND" 470 168 15 "#173629" $true

FillRoundRect 70 760 1255 58 14 "#fff6df" "#d9c7a4" 2
DrawText "Do not connect the button wires to 5V or 3V3. Each button connects one GPIO pin to GND when pressed." 95 779 17 "#9a342b" $true

$bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Output $outPath
