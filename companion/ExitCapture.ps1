# One WinRT OCR operation at a time. The caller polls it alongside map keys; no UI-thread waits.
function New-RaidExitSchedule($state,[long]$tick,[string]$previous){
 if($state.Count -lt 8 -or $state[4] -ne 'started'){return}
 $start=0L;if(![long]::TryParse([string]$state[7],[ref]$start) -or $start -le 0){return}
 $key=$state[0]+':'+$state[5]+':'+$start
 if($key -eq $previous -or $tick -lt $start -or $tick-$start -gt 2500){return}
 return @{key=$key;until=$tick+3800;next=$tick+150}
}
function Set-ExitOperation($job,$op,$type,$stage){
 $job.op=$op;$job.task=$script:asTask.MakeGenericMethod($type).Invoke($null,@($op));$job.stage=$stage
}
function Dispose-ExitRead($job){
 foreach($key in 'bitmap','decoder','writer','stream'){if($job[$key] -is [IDisposable]){try{$job[$key].Dispose()}catch{}}}
}
# The capture itself runs on a pool thread (stage 'capture'), so the map key is answered while the list is read.
function Read-Exits($game){
 Init-Ocr;$job=@{stream=$null;writer=$null;bitmap=$null;decoder=$null;task=$null;op=$null;stage='';generation=$script:exitGeneration;map=$script:mapId;png=$null;until=[DateTime]::UtcNow.AddSeconds(8);discard=$false}
 $job.task=[ModeSwitchPrice.Desktop]::ExitListPngAsync($game);$job.stage='capture';$script:exitRead=$job
}
function Poll-ExitRead {
 $job=$script:exitRead;if(!$job){return}
 # Cancellation can complete asynchronously. Keep buffers alive until that task has finished.
 if(!$job.task.IsCompleted){
  if(!$job.discard -and [DateTime]::UtcNow -gt $job.until){$job.discard=$true;try{$job.op.Cancel()}catch{};Status (T 'OCR 처리 시간 초과')}
  return
 }
 # A frame taken while the map window opened may show the map instead of the list: dropped.
 if($job.discard -or $job.generation -ne $script:exitGeneration -or $job.map -ne $script:mapId -or !$script:mapsOn -or ($job.stage -eq 'capture' -and $map.Visible)){if($job.stage -eq 'bitmap'){try{$job.bitmap=$job.task.GetAwaiter().GetResult()}catch{}};Dispose-ExitRead $job;$script:exitRead=$null;return}
 try{
  $result=$job.task.GetAwaiter().GetResult()
  switch($job.stage){
   'capture' {$job.png=$result;$job.stream=[Windows.Storage.Streams.InMemoryRandomAccessStream]::new();$job.writer=[Windows.Storage.Streams.DataWriter]::new($job.stream);$job.writer.WriteBytes($result);Set-ExitOperation $job ($job.writer.StoreAsync()) ([uint32]) 'store'}
   'store' {$null=$job.writer.DetachStream();$job.stream.Seek(0);Set-ExitOperation $job ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($job.stream)) ([Windows.Graphics.Imaging.BitmapDecoder]) 'decode'}
   'decode' {$job.decoder=$result;Set-ExitOperation $job ($job.decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap]) 'bitmap'}
   'bitmap' {$job.bitmap=$result;Set-ExitOperation $job ($script:engine.RecognizeAsync($job.bitmap)) ([Windows.Media.Ocr.OcrResult]) 'recognize'}
   'recognize' {$evidence=Get-TaxiObservations $result $job.png;Apply-ExitLines @($result.Lines|ForEach-Object {$_.Text}) $evidence;Dispose-ExitRead $job;$script:exitRead=$null}
  }
 }catch{Dispose-ExitRead $job;$script:exitRead=$null;$script:captures=0;$script:autoExits=$null;Status $_.Exception.Message}
}


# Match the car name first, then inspect that row's identifier colour and countdown only.
function Get-TaxiObservations($result,$png){
 $observations=@{};if(!$png -or !$map){return $observations}
 $taxi=@($map.Canvas.Pins|Where-Object {[ModePulse.TaxiMarker]::IsTaxi($_)});if(!$taxi.Count){return $observations}
 foreach($line in $result.Lines){
  $words=New-Object 'System.Collections.Generic.List[object]';foreach($word in $line.Words){$words.Add($word)}
  if(!$words.Count){continue};$names=@(Match-Exits @($line.Text) $taxi);if(!$names.Count){continue}
  $r=$words[0].BoundingRect;$rect=[Drawing.RectangleF]::new([single]$r.X,[single]$r.Y,[single]$r.Width,[single]$r.Height)
  $identifier=[string]$words[0].Text -match '\d{1,2}$';$colour=[ModePulse.TaxiVision]::Colour($png,$rect,$identifier);$band=New-Object 'System.Collections.Generic.List[string]'
  foreach($other in $result.Lines){foreach($word in $other.Words){$b=$word.BoundingRect;if([Math]::Abs(($b.Y+$b.Height/2)-($r.Y+$r.Height/2)) -le [Math]::Max($r.Height,$b.Height)*0.6){$band.Add([string]$word.Text)}}}
  $countdown=[ModePulse.TaxiVision]::Countdown(($band -join ' '))
  $state=$(if($colour -eq 'red'){'Gone'}elseif($colour -eq 'green' -and $countdown){'Using'}elseif($colour -eq 'white'){'Normal'}else{'Unknown'})
  foreach($name in $names){if($observations.ContainsKey($name) -and $observations[$name] -ne $state){$observations[$name]='Unknown'}else{$observations[$name]=$state}}
 }
 return $observations
}
function Update-TaxiStates($observations){
 foreach($name in $observations.Keys){
  if(!@($map.Canvas.Pins|Where-Object {$_.Name -eq $name -and [ModePulse.TaxiMarker]::IsTaxi($_)}).Count){continue}
  $state=[Enum]::Parse([ModePulse.TaxiState],[string]$observations[$name])
  if($state -ne [ModePulse.TaxiState]::Unknown -and $script:taxiLastFrame[$name] -eq $observations[$name]){
   if(!$map.Canvas.TaxiStates.ContainsKey($name) -or $map.Canvas.TaxiStates[$name] -ne [ModePulse.TaxiState]::Gone){$map.Canvas.TaxiStates[$name]=$state}
  }
 }
 $script:taxiLastFrame=$observations
 # Departure is terminal for this raid; a failed read or white/green false match cannot resurrect a car.
 foreach($name in $map.Canvas.TaxiStates.Keys){if($map.Canvas.TaxiStates[$name] -eq [ModePulse.TaxiState]::Gone){[void]$map.Canvas.Available.Remove($name)}}
}
