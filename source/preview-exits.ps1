param([string]$Out=(Join-Path $PSScriptRoot 'assets/exit-sign-preview.png'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
$image=New-Object Drawing.Bitmap(900,370)
$g=[Drawing.Graphics]::FromImage($image);$g.Clear([Drawing.Color]::FromArgb(14,19,27));$g.SmoothingMode='AntiAlias'
$font=New-Object Drawing.Font('Malgun Gothic',16);$small=New-Object Drawing.Font('Malgun Gothic',11)
$labels=@('탈출구','트랜짓','PMC + SCAV 협동 탈출구');$colors=@([ModePulse.ExitSign]::Green,[ModePulse.ExitSign]::Orange,[ModePulse.ExitSign]::Green)
try{
 $g.DrawString('BlueAsh · 지도 아이콘 미리보기',$font,[Drawing.Brushes]::White,24,18)
 for($i=0;$i -lt 3;$i++){
  $x=150+$i*300
  [ModePulse.ExitSign]::Draw($g,$x,116,96,$colors[$i],($i -eq 2))
  $sf=New-Object Drawing.StringFormat;$sf.Alignment='Center'
  $g.DrawString($labels[$i],$small,[Drawing.Brushes]::White,[Drawing.RectangleF]::new($x-145,179,290,36),$sf);$sf.Dispose()
  $g.DrawString('지도 크기 (24 / 32px)',$small,[Drawing.Brushes]::LightGray,$x-96,224)
  [ModePulse.ExitSign]::Draw($g,$x-32,278,24,$colors[$i],($i -eq 2));[ModePulse.ExitSign]::Draw($g,$x+32,278,32,$colors[$i],($i -eq 2))
  [ModePulse.ExitSign]::Draw($g,$x-32,330,24,[Drawing.Color]::Gray,($i -eq 2))
  $g.DrawString('미확인',$small,[Drawing.Brushes]::LightGray,$x-6,317)
 }
 $image.Save($Out,[Drawing.Imaging.ImageFormat]::Png)
}finally{$g.Dispose();$font.Dispose();$small.Dispose();$image.Dispose()}
$Out
