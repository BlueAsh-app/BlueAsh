$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
function T($s){$s}
function Status($s){$script:errorText=$s}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions,System.Runtime.WindowsRuntime
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
. (Join-Path $PSScriptRoot '../companion/CompanionCore.ps1')
. (Join-Path $PSScriptRoot '../companion/ExitCapture.ps1')
foreach($n in 'Dorms V-Ex','Bridge V-Ex','Police Cordon V-Ex','Road to North V-Ex','Primorsky Ave Taxi V-Ex','Power Station V-Ex','Road to Military Base V-Ex'){$p=New-Object ModePulse.Pin;$p.Name=$n;Assert ([ModePulse.TaxiMarker]::IsTaxi($p)) ('Vehicle name: '+$n)}
foreach($n in 'Cargo Elevator','Scav Camp (Co-Op)','Car repair shop'){$p=New-Object ModePulse.Pin;$p.Name=$n;Assert (![ModePulse.TaxiMarker]::IsTaxi($p)) ('Not vehicle: '+$n)}
foreach($n in '0:00:58','00:05','0:01:00','00:00'){Assert ([ModePulse.TaxiVision]::Countdown($n)) ('Countdown: '+$n)}
foreach($n in '0.50','2477 RUB','??:??:??','0:18:09','0:60'){Assert (![ModePulse.TaxiVision]::Countdown($n)) ('Not countdown: '+$n)}
$car=New-Object ModePulse.Pin;$car.Name='Bridge V-Ex'
$canvas=[pscustomobject]@{Pins=@($car);Available=(New-Object 'System.Collections.Generic.HashSet[string]');TaxiStates=(New-Object 'System.Collections.Generic.Dictionary[string,ModePulse.TaxiState]');MeShown=$true};$canvas|Add-Member ScriptMethod Invalidate {}
$map=[pscustomobject]@{Canvas=$canvas};$script:mapId='woods_PMC';$script:mapsOn=$true;$script:exitGeneration=1;$script:taxiLastFrame=@{}
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
Update-TaxiStates @{$car.Name='Using'};Assert (!$canvas.TaxiStates.ContainsKey($car.Name)) 'One frame cannot change taxi state'
Update-TaxiStates @{$car.Name='Using'};Assert ($canvas.TaxiStates[$car.Name] -eq [ModePulse.TaxiState]::Using) 'Two frames confirm use'
Update-TaxiStates @{};Update-TaxiStates @{$car.Name='Unknown'};Assert ($canvas.TaxiStates[$car.Name] -eq [ModePulse.TaxiState]::Using) 'Missing or ambiguous reads retain confirmed state'
[void]$canvas.Available.Add($car.Name);Update-TaxiStates @{$car.Name='Gone'};Update-TaxiStates @{$car.Name='Gone'};Assert ($canvas.TaxiStates[$car.Name] -eq [ModePulse.TaxiState]::Gone -and !$canvas.Available.Contains($car.Name)) 'Departure clears availability'
Update-TaxiStates @{$car.Name='Normal'};Update-TaxiStates @{$car.Name='Normal'};Assert ($canvas.TaxiStates[$car.Name] -eq [ModePulse.TaxiState]::Gone) 'Departed car cannot resurrect within raid'
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../companion/Companion.ps1'),[ref]$tokens,[ref]$errors)
$manual=$ast.Find({param($n)$n -is [Management.Automation.Language.IfStatementAst] -and $n.Clauses[0].Item1.Extent.Text -eq '$triggered'},$true)
Assert ([bool]$manual) 'Manual extract key branch exists'
$canvas.TaxiStates.Clear();$script:taxiLastFrame=@{$car.Name='Using'};$triggered=$true;$now=[DateTime]::Now
. ([scriptblock]::Create($manual.Extent.Text))
Update-TaxiStates @{$car.Name='Using'};Assert (!$canvas.TaxiStates.Count) 'Manual key clears evidence and needs two fresh captures'
Update-TaxiStates @{$car.Name='Using'};Assert ($canvas.TaxiStates[$car.Name] -eq [ModePulse.TaxiState]::Using) 'Manual key session confirms after second capture'
foreach($name in 'Reset-Raid','Init-Ocr'){$f=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true);. ([scriptblock]::Create($f.Extent.Text))}
Reset-Raid;Assert (!$canvas.TaxiStates.Count -and !$script:taxiLastFrame.Count) 'New raid clears vehicle evidence'
# Optional primary game screenshots stay in ignored research staging, never in the distributed bundle.
$research=Join-Path $PSScriptRoot '.bundle/taxi-research'
if(Test-Path (Join-Path $research 'normal.jpg')){
 foreach($c in @(@('normal',34,118,92,36,'white'),@('active',35,178,84,30,'green'),@('gone',35,179,83,26,'red'),@('paid',1424,74,95,28,'green'))){$bytes=[IO.File]::ReadAllBytes((Join-Path $research ($c[0]+'.jpg')));$got=[ModePulse.TaxiVision]::Colour($bytes,[Drawing.RectangleF]::new($c[1],$c[2],$c[3],$c[4]),$true);Assert ($got -eq $c[5]) ('Actual game identifier '+$c[0]+': '+$got)}
 Init-Ocr
 Add-Type -Path (Join-Path $PSScriptRoot '../price/PriceHelper.dll')
 function Apply-ExitLines($lines,$evidence){$script:ocrTaxi=$evidence;$script:ocrText=$lines}
 foreach($kind in 'paid','gone','active'){
  $im=[Drawing.Bitmap]::FromFile((Join-Path $research ($kind+'.jpg')))
  try{if($kind -eq 'paid'){$bytes=[ModeSwitchPrice.Desktop]::CropPng($im,1410,65,510,65,2)}else{$bytes=[ModeSwitchPrice.Desktop]::CropPng($im,26,172,480,68,2)}}finally{$im.Dispose()}
  $job=@{png=$bytes;generation=$script:exitGeneration;map=$script:mapId;until=[DateTime]::UtcNow.AddSeconds(8);discard=$false;stream=[Windows.Storage.Streams.InMemoryRandomAccessStream]::new();writer=$null;bitmap=$null;decoder=$null;op=$null;task=$null;stage=''}
  $job.writer=[Windows.Storage.Streams.DataWriter]::new($job.stream);$job.writer.WriteBytes($bytes);Set-ExitOperation $job ($job.writer.StoreAsync()) ([uint32]) 'store';$script:exitRead=$job;$script:ocrTaxi=@{};$script:ocrText=@();$watch=[Diagnostics.Stopwatch]::StartNew()
  while($script:exitRead -and $watch.ElapsedMilliseconds -lt 10000){Poll-ExitRead;Start-Sleep -Milliseconds 15}
  $expected=@{paid='Using';gone='Gone';active='Unknown'}[$kind];Assert ($script:ocrTaxi[$car.Name] -eq $expected) ('Actual Windows OCR '+$kind+': '+($script:ocrText -join ' / ')+'; '+$script:errorText)
 }
}
'PASS: taxi classification, row colours/countdown, no proximity false payment, two-frame confirmation, raid reset, real game screenshot OCR'
