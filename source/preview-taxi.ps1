param([string]$Out=(Join-Path $PSScriptRoot 'assets/taxi-sign-preview.png'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
$b=New-Object Drawing.Bitmap(900,390);$g=[Drawing.Graphics]::FromImage($b);$g.Clear([Drawing.Color]::FromArgb(14,19,27));$g.SmoothingMode='AntiAlias'
$f=New-Object Drawing.Font('Malgun Gothic',16);$small=New-Object Drawing.Font('Malgun Gothic',11)
try{
 $g.DrawString('BlueAsh · 택시 탈출구 아이콘',$f,[Drawing.Brushes]::White,24,18)
 $states=@([ModePulse.TaxiState]::Normal,[ModePulse.TaxiState]::Using,[ModePulse.TaxiState]::Gone);$labels=@('기본 · 입금 표시 없음','출발 카운트다운 감지','출발 완료 · 사용 불가')
 for($i=0;$i -lt 3;$i++){$x=150+300*$i;[ModePulse.TaxiMarker]::Draw($g,$x,116,96,$states[$i]);$sf=New-Object Drawing.StringFormat;$sf.Alignment='Center';$g.DrawString($labels[$i],$small,[Drawing.Brushes]::White,[Drawing.RectangleF]::new($x-145,180,290,40),$sf);$sf.Dispose();$g.DrawString('지도 크기 (24 / 32px)',$small,[Drawing.Brushes]::LightGray,$x-94,234);[ModePulse.TaxiMarker]::Draw($g,$x-32,288,24,$states[$i]);[ModePulse.TaxiMarker]::Draw($g,$x+32,288,32,$states[$i])}
 [ModePulse.TaxiMarker]::Draw($g,64,348,24,[ModePulse.TaxiState]::Unknown);$g.DrawString('미확인',$small,[Drawing.Brushes]::LightGray,92,334)
 $g.DrawString('기본 표시는 차량이 실제로 있는지 보장하지 않습니다.',$small,[Drawing.Brushes]::LightGray,340,334)
 $b.Save($Out,[Drawing.Imaging.ImageFormat]::Png)
}finally{$g.Dispose();$f.Dispose();$small.Dispose();$b.Dispose()}
$Out
