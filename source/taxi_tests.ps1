$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
. (Join-Path $PSScriptRoot 'helper_test_load.ps1')
foreach($n in 'Dorms V-Ex','Bridge V-Ex','Police Cordon V-Ex','Road to North V-Ex','Primorsky Ave Taxi V-Ex','Power Station V-Ex','Road to Military Base V-Ex'){$p=New-Object ModePulse.Pin;$p.Name=$n;Assert ([ModePulse.TaxiMarker]::IsTaxi($p)) ('Vehicle name: '+$n)}
foreach($n in 'Cargo Elevator','Scav Camp (Co-Op)','Car repair shop'){$p=New-Object ModePulse.Pin;$p.Name=$n;Assert (![ModePulse.TaxiMarker]::IsTaxi($p)) ('Not vehicle: '+$n)}
foreach($n in '0:00:58','00:05','0:01:00','00:00'){Assert ([ModePulse.TaxiVision]::Countdown($n)) ('Countdown: '+$n)}
foreach($n in '0.50','2477 RUB','??:??:??','0:18:09','0:60'){Assert (![ModePulse.TaxiVision]::Countdown($n)) ('Not countdown: '+$n)}
$car=New-Object ModePulse.Pin;$car.Name='Bridge V-Ex'
$states=New-Object 'System.Collections.Generic.Dictionary[string,ModePulse.TaxiState]';$available=New-Object 'System.Collections.Generic.HashSet[string]';$pins=New-Object 'System.Collections.Generic.List[ModePulse.Pin]';$pins.Add($car);$script:last=$null
function Ocr($rows){$o=New-Object ModePulse.OcrText;foreach($r in $rows){$l=New-Object ModePulse.OcrLine;$l.Text=$r.Text;foreach($w in $r.Words){$x=New-Object ModePulse.OcrWord;$x.Text=$w.Text;$x.Box=[Drawing.RectangleF]::new($w.BoundingRect.X,$w.BoundingRect.Y,$w.BoundingRect.Width,$w.BoundingRect.Height);$l.Words.Add($x)};$o.Lines.Add($l)};return $o}
function Get-TaxiObservations($result,$png){[ModePulse.TaxiRead]::Observe((Ocr $result.Lines),$png,$pins)}
function Update-TaxiStates($o){$d=New-Object 'System.Collections.Generic.Dictionary[string,string]';foreach($k in $o.Keys){$d[$k]=$o[$k]};$script:last=[ModePulse.TaxiRead]::Update($states,$available,$pins,$d,$script:last)}
function Word($t,$x,$y,$w=85,$h=26){[pscustomobject]@{Text=$t;BoundingRect=[pscustomobject]@{X=$x;Y=$y;Width=$w;Height=$h}}}
function Scene([Drawing.Color]$color,$time='00:58',$clock='0:18:09',$otherY=80){
 $b=New-Object Drawing.Bitmap(800,240);$g=[Drawing.Graphics]::FromImage($b);$g.Clear([Drawing.Color]::FromArgb(20,22,22));$f=New-Object Drawing.Font('Arial',20,[Drawing.FontStyle]::Bold,[Drawing.GraphicsUnit]::Pixel);$ink=New-Object Drawing.SolidBrush($color);$ms=New-Object IO.MemoryStream
 try{$g.DrawString('EXIT03',$f,$ink,40,80);$g.DrawString('Bridge V-Ex',$f,[Drawing.Brushes]::White,175,80);$g.DrawString($clock,$f,[Drawing.Brushes]::White,605,20);if($time){$g.DrawString($time,$f,[Drawing.Brushes]::White,605,$otherY)};$b.Save($ms,[Drawing.Imaging.ImageFormat]::Png);$bytes=$ms.ToArray()}finally{$g.Dispose();$f.Dispose();$ink.Dispose();$b.Dispose();$ms.Dispose()}
 $rows=@([pscustomobject]@{Text='EXIT03 Bridge V-Ex';Words=@((Word 'EXIT03' 40 80),(Word 'Bridge' 175 80),(Word 'V-Ex' 248 80 55))},[pscustomobject]@{Text=$clock;Words=@((Word $clock 605 20 130))});if($time){$rows+=@([pscustomobject]@{Text=$time;Words=@((Word $time 605 $otherY 130))})};[pscustomobject]@{Png=$bytes;Result=[pscustomobject]@{Lines=$rows}}
}
$green=[Drawing.Color]::FromArgb(134,205,10);$red=[Drawing.Color]::FromArgb(220,25,20)
$s=Scene $green;$e=Get-TaxiObservations $s.Result $s.Png;Assert ($e[$car.Name] -eq 'Using') 'Green identifier plus countdown confirms use'
$s=Scene $green '';Assert ((Get-TaxiObservations $s.Result $s.Png)[$car.Name] -eq 'Unknown') 'Green proximity/ruble display without countdown is not payment'
$s=Scene $green '00:58' '0:18:09' 140;Assert ((Get-TaxiObservations $s.Result $s.Png)[$car.Name] -eq 'Unknown') 'Countdown on another row cannot activate car'
$s=Scene ([Drawing.Color]::White) '';Assert ((Get-TaxiObservations $s.Result $s.Png)[$car.Name] -eq 'Normal') 'White identifier is baseline, not confirmed vehicle presence'
$s=Scene $red '';Assert ((Get-TaxiObservations $s.Result $s.Png)[$car.Name] -eq 'Gone') 'Red identifier signals departed/unavailable'
Update-TaxiStates @{$car.Name='Using'};Assert (!$states.ContainsKey($car.Name)) 'One frame cannot change taxi state'
Update-TaxiStates @{$car.Name='Using'};Assert ($states[$car.Name] -eq [ModePulse.TaxiState]::Using) 'Two frames confirm use'
Update-TaxiStates @{};Update-TaxiStates @{$car.Name='Unknown'};Assert ($states[$car.Name] -eq [ModePulse.TaxiState]::Using) 'Missing or ambiguous reads retain confirmed state'
[void]$available.Add($car.Name);Update-TaxiStates @{$car.Name='Gone'};Update-TaxiStates @{$car.Name='Gone'};Assert ($states[$car.Name] -eq [ModePulse.TaxiState]::Gone -and !$available.Contains($car.Name)) 'Departure clears availability'
Update-TaxiStates @{$car.Name='Normal'};Update-TaxiStates @{$car.Name='Normal'};Assert ($states[$car.Name] -eq [ModePulse.TaxiState]::Gone) 'Departed car cannot resurrect within raid'
# The exit-check key starts a fresh read session (taxi evidence cleared), and a new raid clears the vehicle states.
$src=Helper-Source
Assert ($src.Contains('if(triggered){pendingAt=now.AddMilliseconds(600);captures=5;lastFrame.Clear();taxiLastFrame=new Dictionary<string,string>();}')) 'Manual key clears evidence and needs two fresh captures'
Assert ($src.Contains('map.Canvas.TaxiStates.Clear();taxiLastFrame=new Dictionary<string,string>();map.Canvas.Invalidate();}')) 'New raid clears vehicle evidence'
$states.Clear();$script:last=$null;Update-TaxiStates @{$car.Name='Using'};Assert (!$states.Count) 'After a reset one frame cannot confirm'
Update-TaxiStates @{$car.Name='Using'};Assert ($states[$car.Name] -eq [ModePulse.TaxiState]::Using) 'Fresh session confirms after second capture'
# Optional primary game screenshots stay in ignored research staging, never in the distributed bundle.
$research=Join-Path $PSScriptRoot '.bundle/taxi-research'
if(Test-Path (Join-Path $research 'normal.jpg')){
 foreach($c in @(@('normal',34,118,92,36,'white'),@('active',35,178,84,30,'green'),@('gone',35,179,83,26,'red'),@('paid',1424,74,95,28,'green'))){$bytes=[IO.File]::ReadAllBytes((Join-Path $research ($c[0]+'.jpg')));$got=[ModePulse.TaxiVision]::Colour($bytes,[Drawing.RectangleF]::new($c[1],$c[2],$c[3],$c[4]),$true);Assert ($got -eq $c[5]) ('Actual game identifier '+$c[0]+': '+$got)}
 $engine=[ModePulse.WinOcr]::Engine('en');if(!$engine){$engine=[ModePulse.WinOcr]::Engine('ko')}
 foreach($kind in 'paid','gone','active'){
  $im=[Drawing.Bitmap]::FromFile((Join-Path $research ($kind+'.jpg')))
  try{if($kind -eq 'paid'){$bytes=[ModeSwitchPrice.Desktop]::CropPng($im,1410,65,510,65,2)}else{$bytes=[ModeSwitchPrice.Desktop]::CropPng($im,26,172,480,68,2)}}finally{$im.Dispose()}
  $r=[ModePulse.WinOcr]::Read($engine,$bytes).GetAwaiter().GetResult();$obs=[ModePulse.TaxiRead]::Observe($r,$bytes,$pins)
  $expected=@{paid='Using';gone='Gone';active='Unknown'}[$kind];Assert ($obs[$car.Name] -eq $expected) ('Actual Windows OCR '+$kind+': '+$r.Text)
 }
}
'PASS: taxi classification, row colours/countdown, no proximity false payment, two-frame confirmation, raid reset, real game screenshot OCR'
