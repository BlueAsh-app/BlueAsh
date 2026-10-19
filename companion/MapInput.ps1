# Check only on a map-key request with a visible game cursor. Gameplay and map close stay immediate.
function Init-MapOcr {
 Init-Ocr;if($script:mapEngine){return}
 $ini=Join-Path $env:APPDATA 'Battlestate Games/Escape from Tarkov/Settings/Game.ini';$tag=[ModeSwitchPrice.GameLanguage]::OcrTag([ModeSwitchPrice.GameLanguage]::Api($ini))
 foreach($l in [Windows.Media.Ocr.OcrEngine]::AvailableRecognizerLanguages){if($l.LanguageTag -eq $tag -or $l.LanguageTag.StartsWith($tag+'-')){$script:mapEngine=[Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage($l);break}}
 if(!$script:mapEngine){$script:mapEngine=$script:engine}
}
function Dispose-MapRead($job){
 if($job.stage -eq 'bitmap' -and !$job.bitmap -and $job.task.IsCompleted -and !$job.task.IsFaulted -and !$job.task.IsCanceled){try{$job.bitmap=$job.task.GetAwaiter().GetResult()}catch{}}
 if($job.image -is [IDisposable]){$job.image.Dispose()};Dispose-ExitRead $job
}
function Read-MapScreen($frame,$game){
 Init-MapOcr;$job=@{game=$game;image=$null;stream=$null;writer=$null;bitmap=$null;decoder=$null;task=$null;op=$null;stage='';nav=0;discard=$false;until=[DateTime]::UtcNow.AddSeconds(2)}
 try{
  $w=0;$h=0;$px=[ModeSwitchPrice.Desktop]::Sample($frame,1600,[ref]$w,[ref]$h);$bars=New-Object 'System.Collections.Generic.List[Drawing.Rectangle]'
  foreach($b in [ModeSwitchPrice.Core]::TitleBars($px,$w,$h)){$bars.Add([Drawing.Rectangle]::new($b.X,$b.Y,$b.W,$b.H))}
  $nav=0;$job.image=[ModePulse.MapSearch]::Sheet($frame,$bars.ToArray(),[ref]$nav);$job.nav=$nav;$memory=New-Object IO.MemoryStream
  try{$job.image.Save($memory,[Drawing.Imaging.ImageFormat]::Png);$bytes=$memory.ToArray()}finally{$memory.Dispose()}
  $job.stream=[Windows.Storage.Streams.InMemoryRandomAccessStream]::new();$job.writer=[Windows.Storage.Streams.DataWriter]::new($job.stream);$job.writer.WriteBytes($bytes)
  Set-ExitOperation $job ($job.writer.StoreAsync()) ([uint32]) 'store';$script:mapRead=$job
 }catch{Dispose-MapRead $job;throw}
}
function Request-MapOpen($game){
 if($map.Visible){$map.HideMap();return}
 if($script:mapRead){$script:mapRead.discard=$true;return}
 if($script:raidPhase -eq 'started' -and ![ModeSwitchPrice.ClickTrigger]::CursorShown()){Load-Map;$map.ShowOver($game);return}
 $frame=$null;try{$frame=[ModeSwitchPrice.Desktop]::Grab($game);Read-MapScreen $frame $game}catch{Status $_.Exception.Message}finally{if($frame){$frame.Dispose()}}
}
function Get-MapReadBlocked($job,$result){
 $text=New-Object 'System.Collections.Generic.List[string]';$boxes=New-Object 'System.Collections.Generic.List[Drawing.RectangleF]'
 foreach($line in $result.Lines){foreach($word in $line.Words){$r=$word.BoundingRect;$text.Add([string]$word.Text);$boxes.Add([Drawing.RectangleF]::new($r.X,$r.Y,$r.Width,$r.Height))}}
 return [ModePulse.MapSearch]::Blocked($job.image,$job.nav,$text.ToArray(),$boxes.ToArray())
}
function Finish-MapRead($job,[bool]$blocked,$foreground){
 if(!$blocked -and !$job.discard -and $script:mapsOn -and !$map.Visible -and $foreground -eq $job.game -and $foreground -ne [IntPtr]::Zero){Load-Map;$map.ShowOver($foreground)}
}
function Poll-MapRead($foreground){
 $job=$script:mapRead;if(!$job){return}
 if(!$script:mapsOn -or $foreground -ne $job.game -or $map.Visible -or [DateTime]::UtcNow -gt $job.until){$job.discard=$true}
 if(!$job.task.IsCompleted){if($job.discard -and !$job.cancelled){$job.cancelled=$true;try{$job.op.Cancel()}catch{}};return}
 if($job.discard){Dispose-MapRead $job;$script:mapRead=$null;return}
 try{
  $result=$job.task.GetAwaiter().GetResult()
  switch($job.stage){
   'store' {$null=$job.writer.DetachStream();$job.stream.Seek(0);Set-ExitOperation $job ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($job.stream)) ([Windows.Graphics.Imaging.BitmapDecoder]) 'decode'}
   'decode' {$job.decoder=$result;Set-ExitOperation $job ($job.decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap]) 'bitmap'}
   'bitmap' {$job.bitmap=$result;Set-ExitOperation $job ($script:mapEngine.RecognizeAsync($job.bitmap)) ([Windows.Media.Ocr.OcrResult]) 'recognize'}
   'recognize' {$blocked=Get-MapReadBlocked $job $result;Finish-MapRead $job $blocked $foreground;Dispose-MapRead $job;$script:mapRead=$null}
  }
 }catch{Dispose-MapRead $job;$script:mapRead=$null;Status $_.Exception.Message}
}
