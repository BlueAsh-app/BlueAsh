param([string]$Out=(Join-Path $PSScriptRoot 'assets/flare-sign-preview.png'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
$b=New-Object Drawing.Bitmap(960,520);$g=[Drawing.Graphics]::FromImage($b);$g.Clear([Drawing.Color]::FromArgb(12,16,24));$g.SmoothingMode='AntiAlias';$g.TextRenderingHint='AntiAliasGridFit'
$title=New-Object Drawing.Font('Malgun Gothic',19,[Drawing.FontStyle]::Bold,[Drawing.GraphicsUnit]::Pixel);$body=New-Object Drawing.Font('Malgun Gothic',14,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
$dim=New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(167,180,204));$panel=New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(22,28,39))
try{
 $g.DrawString('BlueAsh · 플레어 탈출구',$title,[Drawing.Brushes]::White,28,22)
 $g.DrawString('참고 그림의 플레어건 형태 · 굵은 화살표와 중심축 정렬',$body,$dim,28,56)
 $g.FillRectangle($panel,28,100,284,206);$g.FillRectangle($panel,328,100,604,206)
 [ModePulse.FlareMarker]::Draw($g,170,180,104,[ModePulse.ExitSign]::Green)
 $g.DrawString('플레어 탈출',$title,[Drawing.Brushes]::White,108,258)
 $g.DrawString('실제 지도 표시 크기',$body,$dim,352,118)
 $sizes=@(24,32,48);for($i=0;$i -lt 3;$i++){$x=400+110*$i;[ModePulse.FlareMarker]::Draw($g,$x,184,$sizes[$i],[ModePulse.ExitSign]::Green);$g.DrawString(([string]$sizes[$i]+' px'),$body,$dim,$x-19,230)}
 [ModePulse.FlareMarker]::Draw($g,812,184,40,[Drawing.Color]::FromArgb(128,128,128));$g.DrawString('사용 불가',$body,$dim,782,230)
 $map=[Drawing.Image]::FromFile((Join-Path $PSScriptRoot '../data/maps/sandbox.jpg'))
 try{$target=[Drawing.RectangleF]::new(28,334,904,160);$src=[Drawing.RectangleF]::new($map.Width*.045,$map.Height*.177,$map.Width*.38,$map.Height*.0673);$g.InterpolationMode='HighQualityBicubic';$g.DrawImage($map,$target,$src,[Drawing.GraphicsUnit]::Pixel);$x=28+(.16908-.045)/.38*904;$y=334+(.2263-.177)/.0673*160;[ModePulse.FlareMarker]::Draw($g,$x,$y,32,[ModePulse.ExitSign]::Green)
  $g.FillRectangle([Drawing.Brushes]::Black,($x+24),($y-10),188,30);$g.DrawString('Mira Ave · 플레어 탈출',$body,[Drawing.Brushes]::White,($x+31),($y-5))
 }finally{$map.Dispose()}
 $b.Save($Out,[Drawing.Imaging.ImageFormat]::Png)
}finally{$g.Dispose();$b.Dispose();$title.Dispose();$body.Dispose();$dim.Dispose();$panel.Dispose()}
$Out
