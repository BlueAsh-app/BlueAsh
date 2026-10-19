param([Parameter(Mandatory=$true)][int]$ParentId)
$ErrorActionPreference='Stop'
$store=Join-Path $env:LOCALAPPDATA 'ModeSwitch'
[void][System.IO.Directory]::CreateDirectory($store)
$statusPath=Join-Path $store 'price-status.txt'
function Status([string]$s){[System.IO.File]::WriteAllText($statusPath,$s,[System.Text.Encoding]::UTF8)}
function P([string]$Source,[object[]]$Values=@()){return [ModeSwitchPrice.Language]::T($Source,$Values)}
$slots=$null
try {
 Add-Type -AssemblyName System.Windows.Forms,System.Drawing,System.Net.Http,System.Runtime.WindowsRuntime,System.Web.Extensions
 Add-Type -Path (Join-Path $PSScriptRoot 'PriceHelper.dll')
 [ModeSwitchPrice.Language]::Reload()
 Status (P '켜는 중 · 글자 인식(OCR)을 준비하고 있습니다.')
 # Quest history from the game's logs (ModePulse.Quests): completed quests are left out of the quest tag.
 Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
 . (Join-Path $PSScriptRoot 'PriceCore.ps1')
 [void][ModeSwitchPrice.Desktop]::SetProcessDPIAware()
 $null=[Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime]
 $null=[Windows.Globalization.Language,Windows.Globalization,ContentType=WindowsRuntime]
 $null=[Windows.Storage.Streams.InMemoryRandomAccessStream,Windows.Storage.Streams,ContentType=WindowsRuntime]
 $null=[Windows.Storage.Streams.DataWriter,Windows.Storage.Streams,ContentType=WindowsRuntime]
 $null=[Windows.Graphics.Imaging.BitmapDecoder,Windows.Graphics.Imaging,ContentType=WindowsRuntime]
 $null=[Windows.Graphics.Imaging.SoftwareBitmap,Windows.Graphics.Imaging,ContentType=WindowsRuntime]
 $null=[Windows.Media.Ocr.OcrResult,Windows.Foundation,ContentType=WindowsRuntime]
 # Titles are read in the game's own text language (Game.ini "Language"), matched against names in that language.
 $gameIni=Join-Path $env:APPDATA 'Battlestate Games\Escape from Tarkov\Settings\Game.ini'
 $gameLang=[ModeSwitchPrice.GameLanguage]::Api($gameIni)
 function Ocr-Engine([string]$tag){foreach($l in [Windows.Media.Ocr.OcrEngine]::AvailableRecognizerLanguages){if($l.LanguageTag -eq $tag -or $l.LanguageTag.StartsWith($tag+'-')){return [Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage($l)}};return $null}
 $main=Ocr-Engine ([ModeSwitchPrice.GameLanguage]::OcrTag($gameLang));$eng=Ocr-Engine 'en'
 # Titles are read first by BlueAsh's own PaddleOCR model for this language (downloaded once in the background from
 # BlueAsh's server, nothing for the user to install); Windows OCR is the second reader and the fallback until then.
 $paddleModel=[ModeSwitchPrice.OcrModels]::ModelFor($gameLang);$script:paddle=$null;$script:paddleOff=$false;$script:paddleLoad=$null
 function Paddle{if($script:paddle -or $script:paddleOff -or !$paddleModel){return $script:paddle}
  if(!$script:paddleLoad){$ocrRoot=[ModeSwitchPrice.OcrModels]::Ready($store,$paddleModel);if(!$ocrRoot){return $null};$script:paddleLoad=[ModeSwitchPrice.PaddleRec]::LoadAsync($ocrRoot,$paddleModel);return $null}
  if(!$script:paddleLoad.IsCompleted){return $null}
  if($script:paddleLoad.IsFaulted){$script:paddleOff=$true;Log-Miss 'paddle-error' @($script:paddleLoad.Exception.GetBaseException().Message)}else{$script:paddle=$script:paddleLoad.Result};return $script:paddle}
 [void][ModeSwitchPrice.OcrModels]::Ready($store,$paddleModel)
 # Only a language with no BlueAsh model and no Windows OCR is offered for install (ocr-missing.txt -> BlueAsh's one-click
 # install); a missing English pack is no longer asked for.
 $missing=@();if($null -eq $main -and !$paddleModel){$missing+=[ModeSwitchPrice.GameLanguage]::Capability($gameLang)+"`t"+$gameLang}
 try{[IO.File]::WriteAllLines((Join-Path $store 'ocr-missing.txt'),[string[]]$missing,(New-Object Text.UTF8Encoding($false)))}catch{}
 if($null -eq $main){$main=$eng};if($null -eq $main){$main=[Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()}
 if($null -eq $main -and !$paddleModel){throw (P 'Windows 글자 인식(OCR) 언어가 없습니다. BlueAsh 알림을 눌러 설치하세요.')}
 if($null -eq $eng){$eng=$main}
 # Second reader for titles the game-language OCR misses (English model names inside Korean titles, etc.).
 $alt=if([object]::ReferenceEquals($eng,$main)){$null}else{$eng}
 $logRoot=[string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name RaidLogs -ErrorAction SilentlyContinue).RaidLogs
 $raidState=Join-Path $store 'raid-state.txt'
 $asTask=[System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {$_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetGenericArguments().Count -eq 1 -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'} | Select-Object -First 1
 if($null -eq $asTask){throw (P 'Windows OCR 비동기 연결을 만들지 못했습니다.')}
 $parentProcess=[Diagnostics.Process]::GetProcessById($ParentId)
 function Pump {
  [System.Windows.Forms.Application]::DoEvents()
  if($parentProcess.HasExited){throw (P 'BlueAsh 종료')}
 }
 function Wait-Task($Task){$watch=[Diagnostics.Stopwatch]::StartNew();while(!$Task.IsCompleted){Pump;Start-Sleep -Milliseconds 5;if($watch.Elapsed.TotalSeconds -gt 25){throw (P '처리 시간 초과')}};return $Task.GetAwaiter().GetResult()}
 function Await($Operation,[Type]$Type){$task=$asTask.MakeGenericMethod($Type).Invoke($null,@($Operation));return Wait-Task $task}
 function Ocr([byte[]]$Bytes,$Engine){
  if($null -eq $Engine){return ''}
  $stream=[Windows.Storage.Streams.InMemoryRandomAccessStream]::new();$writer=$null;$bitmap=$null
  try {
   $writer=[Windows.Storage.Streams.DataWriter]::new($stream);$writer.WriteBytes($Bytes)
   $null=Await ($writer.StoreAsync()) ([uint32]);$null=$writer.DetachStream();$stream.Seek(0)
   $decoder=Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
   $bitmap=Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
   $r=Await ($Engine.RecognizeAsync($bitmap)) ([Windows.Media.Ocr.OcrResult]);return [string]$r.Text
  } finally {if($null -ne $bitmap){$bitmap.Dispose()};if($null -ne $writer){$writer.Dispose()};$stream.Dispose()}
 }
 # Price tables: disk cache is usable at once; downloads run on a background thread every 10 minutes.
 $catalogs=@{};foreach($m in [ModeSwitchPrice.Catalog]::Modes){$c=New-Object ModeSwitchPrice.Catalog($m,$store,$gameLang);[void]$c.LoadCache();$catalogs[$m]=$c}
 $refreshAge=[TimeSpan]::FromMinutes(10)
 $mode='';try{$mode=Read-LogMode $logRoot}catch{};if($mode){$catalogs[$mode].RefreshIfStale($refreshAge)}
 # Quest needs per item: cached, refreshed every 6 h in the background.
 $tasks=@{};foreach($m in [ModeSwitchPrice.Catalog]::Modes){$t=New-Object ModeSwitchPrice.TaskIndex($m,$store);[void]$t.LoadCache();$tasks[$m]=$t};$taskAge=[TimeSpan]::FromHours(6);if($mode){$tasks[$mode].RefreshIfStale($taskAge)}
 # Player's hideout levels (entered in the map/Discord window), per mode; re-read when the file changes.
 $levelsPath=Join-Path $store 'hideout-levels.json';$levelsStamp=-1;$levels=@{}
 # Completed quests/reset history is read in the background, at most every 30 s per mode.
 [ModePulse.Quests]::CacheFile=Join-Path $store 'quest-log-index.json'
 $histories=@{};foreach($m in [ModeSwitchPrice.Catalog]::Modes){$histories[$m]=New-Object ModePulse.QuestHistory($logRoot,$m)}
 if($mode){$histories[$mode].RefreshIfStale()}
 function Done-Quests($mode){return ,$histories[$mode].Completed}
 # Stash already maxed by the game edition (Unheard / Edge of Darkness): its upgrade items are not listed.
 $script:editionAt=[DateTime]::MinValue;$script:editionSkip=$null
 function Edition-Skip{$now=[DateTime]::UtcNow;if($now -ge $script:editionAt){$script:editionAt=$now.AddMinutes(1);$set=New-Object 'System.Collections.Generic.HashSet[string]';if([ModeSwitchPrice.TaskIndex]::StashMaxedByEdition($gameIni)){[void]$set.Add([ModeSwitchPrice.TaskIndex]::StashId)};if([ModeSwitchPrice.TaskIndex]::CultistByEdition($gameIni)){[void]$set.Add([ModeSwitchPrice.TaskIndex]::CultistId)};$script:editionSkip=$set};return ,$script:editionSkip}
 $script:wipeAt=@{}
# Every minute per mode: a prestige / season wipe in the logs after the levels were saved clears them (the file then changes and is read again).
function Hideout-Levels($mode){$now=[DateTime]::UtcNow;if(!$script:wipeAt.ContainsKey($mode) -or $now -ge $script:wipeAt[$mode]){$script:wipeAt[$mode]=$now.AddMinutes(1);try{[void][ModeSwitchPrice.TaskIndex]::ClearIfOlder($levelsPath,$mode,$histories[$mode].ResetAt)}catch{}};$t=if(Test-Path $levelsPath){(Get-Item $levelsPath).LastWriteTimeUtc.Ticks}else{0};if($t -ne $script:levelsStamp){$script:levelsStamp=$t;$script:levels=@{}};if(!$script:levels.ContainsKey($mode)){$script:levels[$mode]=[ModeSwitchPrice.TaskIndex]::ReadLevels($levelsPath,$mode)};return ,$script:levels[$mode]}

 # Titles read but not found, newest last (kept to 200 lines) so real misses can be diagnosed later.
 $missPath=Join-Path $store 'price-misses.log';$lastCatalogErrors=@{}
 function Log-Miss([string]$mode,$texts){
  $texts=@($texts|Where-Object {$_ -match '[A-Za-z가-힣]'});if(!$texts.Count){return}
  try{$line=(Get-Date -Format 'MM-dd HH:mm:ss')+"`t"+$mode+"`t"+(($texts|ForEach-Object {$_ -replace '\s+',' '}) -join ' | ')
   $old=@();if(Test-Path $missPath){$old=@([IO.File]::ReadAllLines($missPath,[Text.Encoding]::UTF8)|Select-Object -Last 199)}
   [IO.File]::WriteAllLines($missPath,[string[]]($old+$line),(New-Object Text.UTF8Encoding($false)))}catch{}
 }
 # Yellow "현재 시세 : 8,500루블" in the bottom-right corner of the inspect window's picture panel.
 function Show-Price($hit,$slot){
  if($slot.z -and (Covered $slot)){$slot.price.Hide();$slot.key.Hide();$slot.covered=$true;return}
  $item=$hit.item;$mode=$hit.mode;$state=Get-PriceState $item $mode
  if($state -eq 'invalid'){throw (P '가격 데이터 모드 불일치')}
  $modeLabel=@{'pve'='PvE';'pvp-season'=(P 'PvP S · 시즌');'regular'='PvP'}[$mode]
  # A range appears only for several items sharing this exact name with clearly different prices.
  if($state -eq 'price'){$value=([double]$item.Price).ToString('N0',[ModeSwitchPrice.Language]::Culture);if($item.PriceMax){$value+='~'+([double]$item.PriceMax).ToString('N0',[ModeSwitchPrice.Language]::Culture)};$line=P '현재 시세 : {0}루블' @($value);$color=[Drawing.Color]::FromArgb(255,214,64)}
  elseif($state -eq 'banned'){$line=P '플리마켓 판매 금지';$color=[Drawing.Color]::FromArgb(235,96,80)}
  else{$line=P '플리마켓 물량 없음';$color=[Drawing.Color]::FromArgb(205,205,205)}
  # No flea number (banned or no offers): the best trader buy-back price instead, so a number is always shown.
  if($state -ne 'price' -and $item.TraderPrice -gt 0){$who=$catalogs[$mode].TraderName($item.TraderId);$tp=([double]$item.TraderPrice).ToString('N0',[ModeSwitchPrice.Language]::Culture)
   $line+=' · '+$(if($who){P '상인 {0}루블 ({1})' @($tp,$who)}else{P '상인 {0}루블' @($tp)})}
  $th=$hit.th;$slot.price.ShowAt($line,$color,[int]($hit.right-$th*0.45),[int]($hit.bottom-$th*0.3),[int]($th*0.55))
  # Keys and keycards: a button at the picture's top-left puts the key on the map (or takes it off): the green
  # "✓ 지도에 등록됨" shows at a glance that this key is already on the map.
  if([ModeSwitchPrice.MyKeys]::IsKey($item)){$slot.key.ShowFor($keysPath,$mode,$item.Id,$item.Name,[int]($hit.left+$th*0.4),[int]($hit.top+$th*1.35),[int]($th*0.42))}else{$slot.key.Hide()}
  # Optional quest/hideout tags follow only after the background history snapshot is ready.
  $history=$histories[$mode];$history.RefreshIfStale()
  $tq=$tasks[$mode];$tq.RefreshIfStale($taskAge)
  if($history.Ready){
  # Quest use (left, orange): which quests need this item or key; nothing when none.
  $q=$tq.For($(if($item.Ids){$item.Ids}else{$item.Id}),(Done-Quests $mode))
  if($q.Count){$slot.quest.ShowLines($q,[Drawing.Color]::FromArgb(236,168,84),[int]($hit.left+$th*1.35),[int]($hit.bottom-$th*0.3),[int]($th*0.45))}else{$slot.quest.Hide()}
  # Hideout upgrades (centre, light blue): "발전기 V2 (3개 필요)".
  $hz=@($tq.Hideout($(if($item.Ids){$item.Ids}else{$item.Id}),(Hideout-Levels $mode),(Edition-Skip)))
  if($hz.Count){$slot.hide.ShowLines([string[]]@($hz|ForEach-Object {$_.Text}),[Drawing.Color[]]@($hz|ForEach-Object {if($_.Now){[Drawing.Color]::FromArgb(130,192,236)}else{[Drawing.Color]::FromArgb(120,128,140)}}),[int](($hit.left+$hit.right)/2),[int]($hit.bottom-$th*0.3),[int]($th*0.45),$true)}else{$slot.hide.Hide()}

  }else{$slot.quest.Hide();$slot.hide.Hide()}
  Status ($modeLabel+' · '+$catalogs[$mode].DisplayName($item)+' · '+$line+' · tarkov.dev '+$item.Updated)
 }
 # Name lookup; the magnifier icon left of the title often reads as a stray letter ("P Military cable ...").
 function Find-Title($catalog,[string]$text){
  $item=$catalog.Find($text);if($null -eq $item -and $text -match '^\S{1,2}\s+(.+)$'){$item=$catalog.Find($Matches[1])};return $item
 }
 # One capture: find inspect title bars, read the one nearest the click first, stop at the first catalog hit.
 # $deep: the costly second chances (category line, binarized re-read, English OCR) only on the later attempts;
 # the early ones are quick so a slowly drawn window is caught as soon as it is readable.
 function Scan($game,[int]$clickX,[int]$clickY,$skip,[bool]$deep=$true){
  $frame=$null;$old=$null
  try {
   $mode='';try{$mode=Read-LogMode $logRoot}catch{}
   $frame=[ModeSwitchPrice.Desktop]::Grab($game)
   if(!$mode){
    # Mode footer: lower-left 40% width / 4% height, enlarged for small grey text.
    $modeBytes=[ModeSwitchPrice.Desktop]::CropPng($frame,0,[int]($frame.Height*.96),[int]($frame.Width*.40),[int]($frame.Height*.04),2)
    $mode=[ModeSwitchPrice.Core]::Mode((Ocr $modeBytes $eng))
   }
   if(!$mode){Status (P '게임 모드(PvE·PvP)를 알 수 없습니다 · 레이드 시작 알림 설정에서 로그 폴더를 확인하세요.');return $null}
   $catalog=$catalogs[$mode];$catalog.RefreshIfStale($refreshAge)
   $w=0;$h=0;$pixels=[ModeSwitchPrice.Desktop]::Sample($frame,2048,[ref]$w,[ref]$h)
   $bars=@([ModeSwitchPrice.Core]::TitleBars($pixels,$w,$h));$pixels=$null;$script:lastBars=$bars.Count
   if($bars.Count -eq 0){return $null}
   if(!$catalog.Ready){Status (P '{0} · 시세 데이터를 받는 중입니다. 잠시 뒤 다시 조사해 주세요.' @($mode));return $null}
   $f=$frame.Width/$w;$bounds=[ModeSwitchPrice.Desktop]::Bounds($game);$cx=($clickX-$bounds.X)/$f;$cy=($clickY-$bounds.Y)/$f
   $bars=@($bars|Sort-Object {[Math]::Pow($_.X+$_.W/2-$cx,2)+[Math]::Pow($_.Y+$_.H/2-$cy,2)})
   $texts=New-Object 'System.Collections.Generic.List[string]'
   foreach($bar in $bars){
    # Only the grey title strip (about 2/3 of the detected band); the line below it confuses OCR.
    $png=[ModeSwitchPrice.Desktop]::TitlePng($frame,[int]($bar.X*$f),[int]($bar.Y*$f),[int]($bar.W*$f),[int]($bar.H*$f*0.67))
    $item=$null;$text='';$pr=Paddle
    if($pr){try{$conf=0.0;$text=$pr.Recognize($frame,(New-Object Drawing.Rectangle([int]($bar.X*$f),[int]($bar.Y*$f),[int]($bar.W*$f),[int]($bar.H*$f*0.67))),[ref]$conf);if($text){$texts.Add('['+$paddleModel.Split('_')[0]+'] '+$text);$item=Find-Title $catalog $text}}catch{Log-Miss 'paddle-error' @($_.Exception.Message)}}
    if($null -eq $item -and ($deep -or !$pr)){$w2=Ocr $png $main;if($w2){$texts.Add($w2);$item=Find-Title $catalog $w2;$text=$w2}}
    if(!$text){continue}
    if($null -eq $item -and $deep -and $null -ne $alt){$t2=Ocr $png $alt;if($t2){$texts.Add($t2);$item=Find-Title $catalog $t2}}
    # Any reading with a letter or digit gets the second chance: garbled titles like '入 타 p F-I 丁五' have no two in a row.
    if($null -eq $item -and !($text -match '[A-Za-z가-힣0-9]')){$texts.Add((P '(2차 건너뜀)'))}
    if($null -eq $item -and $deep -and $text -match '[A-Za-z가-힣0-9]'){
     # Second chance, only on a miss (costs one or two more OCR passes, ~20-100 ms each): read the category
     # line under the title ("Weapons > Throwables") and match within that category; then a binarized re-read.
     $bx=[int]($bar.X*$f);$by=[int]($bar.Y*$f);$bw=[int]($bar.W*$f);$bh=[int]($bar.H*$f);$category='';$again=''
     try{
      $category=Ocr ([ModeSwitchPrice.Desktop]::TextPng($frame,$bx,$by+[int]($bh*0.8),[int]($bw*0.6),[int]($bh*0.75),330,2,$true)) $eng
      $item=$catalog.FindInCategory($text,$category)
      if($null -eq $item){$again=Ocr ([ModeSwitchPrice.Desktop]::TextPng($frame,$bx,$by,$bw,[int]($bh*0.67),450,3,$true)) $main
       if($again){$texts.Add($again);$item=Find-Title $catalog $again;if($null -eq $item){$item=$catalog.FindInCategory($again,$category)}}}
     }catch{Log-Miss 'ocr-error' @($_.Exception.Message);$texts.Add((P '2차 오류: {0}' @([ModeSwitchPrice.Language]::Error($_.Exception))))}
     if($null -eq $item){$texts.Add('['+([string]$category).Trim()+'] bar '+$bx+','+$by+' '+$bw+'x'+$bh)
      # Keep what was actually read (title + category line, game pixels) for diagnosing misses: price-miss-debug.png.
      try{$png=[ModeSwitchPrice.Desktop]::CropPng($frame,$bx-[int]($bh*0.8),$by-[int]($bh*0.3),$bw+[int]($bh*2.5),[int]($bh*2.4),1);[IO.File]::WriteAllBytes((Join-Path $store 'price-miss-debug.png'),$png)}catch{}}
    }
    if($null -ne $item){
     # Window geometry in frame pixels: right edge = title text end + close button; panel bottom = top of the button row.
     $th=[int]($bar.H*$f);$top=[int]($bar.Y*$f);$right=[int](($bar.X+$bar.W)*$f+$th*1.07)
     $bottom=[ModeSwitchPrice.Desktop]::PanelBottom($frame,$right,$top+$th,$th);if($bottom -lt 0){$bottom=$top+[int]($th*10.8)}
     # Close button: TitleBars ends the title 5 sampled px before the red button starts; centre is ~1/3 bar height in.
     $start=$bounds.X+[int](($bar.X+$bar.W+5)*$f)
     $hit=@{item=$item;mode=$mode;th=$th;right=$bounds.X+$right;left=$bounds.X+[int]($bar.X*$f);bottom=$bounds.Y+$bottom;top=$bounds.Y+$top;start=$start
      cx=$start+[int]($th*0.33);cy=$bounds.Y+$top+[int]($th*0.33);r=[Math]::Max(6,[int]($th*0.2));fullOnly=$false;old=$false;w=[int]($bar.W*$f)}
     # A window whose price is already shown (same item, same place): look for the newly opened one first.
     $isOld=$false;foreach($o in @($skip)){if($o -and $item.Id -eq $o.item.Id -and [Math]::Abs($hit.top-$o.top) -le $o.th -and [Math]::Abs($start-$o.start) -le $o.th){$isOld=$true}}
     if($isOld){$hit.old=$true;$old=$hit;continue}
     return $hit}
   }
   if($old){return $old}
   # Early quick attempts often see a window still being drawn: only the later (deep) ones report a miss.
   if($deep){Status (P '{0} · 아이템 이름을 읽지 못했습니다: {1}' @($mode,($texts -join ' / ')));if($texts.Count){Log-Miss $mode $texts}};return $null
  } finally {if($null -ne $frame){$frame.Dispose()}}
 }

 # Full search: is an inspect title bar still at the shown window's place (within one bar height)?
 function Find-Window($game,$shown,[ref]$count){
  $frame=$null
  try{
   $frame=[ModeSwitchPrice.Desktop]::Grab($game);$w=0;$h=0;$px=[ModeSwitchPrice.Desktop]::Sample($frame,2048,[ref]$w,[ref]$h);$f=$frame.Width/$w;$bounds=[ModeSwitchPrice.Desktop]::Bounds($game)
   $bars=@([ModeSwitchPrice.Core]::TitleBars($px,$w,$h));$count.Value=$bars.Count;$moved=$null
   foreach($bar in $bars){
    $th=[int]($bar.H*$f);$top=$bounds.Y+[int]($bar.Y*$f);$start=$bounds.X+[int](($bar.X+$bar.W+5)*$f)
    if([Math]::Abs($top-$shown.top) -le $shown.th -and [Math]::Abs($start-$shown.start) -le $shown.th){return @{cx=$start+[int]($th*0.33);cy=$top+[int]($th*0.33)}}
    # Dragged window: the same title (same bar width and height) elsewhere. No OCR needed, the tags just move.
    if(!$moved -and $shown.w -and [Math]::Abs([int]($bar.W*$f)-$shown.w) -le [int]($f*1.5)+1 -and [Math]::Abs($th-$shown.th) -le [int]$f+1){$moved=@{dx=$start-$shown.start;dy=$top-$shown.top}}
   }
   return $moved
  }finally{if($null -ne $frame){$frame.Dispose()}}
 }
 # Nothing recognised after an inspect click: the screen (reduced to 1600 px) goes to price-miss-frame.jpg, with a
 # log line, at most every 20 s. Local only; it shows what the title-bar search or OCR missed.
 $script:missFrameAt=[DateTime]::MinValue;$script:lastBars=0
 function Save-MissFrame($game,[string]$why){
  $now=[DateTime]::UtcNow;if($now -lt $script:missFrameAt){return};$script:missFrameAt=$now.AddSeconds(20)
  $frame=$null;$small=$null
  try{$frame=[ModeSwitchPrice.Desktop]::Grab($game);$k=[Math]::Min(1.0,1600.0/$frame.Width);$small=New-Object Drawing.Bitmap($frame,[int]($frame.Width*$k),[int]($frame.Height*$k))
   $small.Save((Join-Path $store 'price-miss-frame.jpg'),[Drawing.Imaging.ImageFormat]::Jpeg);Log-Miss 'inspect' @($why)}catch{}finally{if($small){$small.Dispose()};if($frame){$frame.Dispose()}}
 }
 # The quick close check misjudged an open window: keep the evidence (grab around the button) for diagnosis.
 function Log-CloseMiss($shown,$again){
  try{[ModeSwitchPrice.Desktop]::SaveGrab($shown.cx-4*$shown.r,$shown.cy-2*$shown.r,6*$shown.r,4*$shown.r,(Join-Path $store 'price-close-debug.png'))
   Log-Miss 'close-check' @((P '{0} 기대 위치 {1},{2} → 실제 {3},{4}' @($shown.item.Name,$shown.cx,$shown.cy,$again.cx,$again.cy)))}catch{}
 }
 # Up to three inspect windows at once (compare two items side by side): each slot has its own price / quest /
 # hideout tags, close check and drag follow. A new inspect click keeps the others and looks for the new window.
 $keysPath=Join-Path $store 'my-keys.json'
 $slots=@();foreach($i in 1..3){$p=New-Object ModeSwitchPrice.PriceTag;$qt=New-Object ModeSwitchPrice.PriceTag;$ht=New-Object ModeSwitchPrice.PriceTag;$p.Linked.Add($qt);$p.Linked.Add($ht);$kb=New-Object ModeSwitchPrice.KeyButton;$slots+=@{price=$p;quest=$qt;hide=$ht;key=$kb;shown=$null;at=0;next=0;z=0;covered=$false}}
 # Overlapping inspect windows: the one opened or clicked last is on top (the game raises a clicked window).
 # A window under another one that overlaps it shows no tags (they would sit on top of the upper window) until it is uncovered.
 $script:zc=0
 function Window-Rect($h){return [Drawing.Rectangle]::FromLTRB($h.left-[int]($h.th*0.3),$h.top,$h.right,$h.bottom+[int]($h.th*1.2))}
 function Covered($s){if(!$s.shown){return $false};$a=Window-Rect $s.shown;foreach($o in $slots){if(!($o -eq $s) -and $o.shown -and $o.z -gt $s.z -and $a.IntersectsWith((Window-Rect $o.shown))){return $true}};return $false}
 function Update-Covers{foreach($s in $slots){if(!$s.shown){continue};$c=Covered $s;if($c -ne $s.covered){if($c){$s.price.Hide();$s.key.Hide();$s.covered=$true}else{$s.covered=$false;Show-Price $s.shown $s}}}}
 function Raise-At([int]$x,[int]$y){$best=$null;foreach($s in $slots){if($s.shown -and (Window-Rect $s.shown).Contains($x,$y) -and (!$best -or $s.z -gt $best.z)){$best=$s}}
  if($best){$top=0;foreach($s in $slots){if($s.shown -and $s.z -gt $top){$top=$s.z}};if($best.z -lt $top){$script:zc++;$best.z=$script:zc;Update-Covers}}}
 function Same-Place($a,$b){return [Math]::Abs($a.top-$b.top) -le $a.th -and [Math]::Abs($a.start-$b.start) -le $a.th}
 function Hide-Slot($s){$s.price.Hide();$s.key.Hide();$s.shown=$null;$s.covered=$false;if($script:zc){Update-Covers}}
 function Shown-Hits{return ,@($slots|Where-Object {$_.shown}|ForEach-Object {$_.shown})}
 # The window's own slot (same place: a new window opened over an old one replaces it), else a free one, else the oldest.
 function Place($hit,$now){
  $target=$null;foreach($s in $slots){if($s.shown -and (Same-Place $s.shown $hit)){$target=$s;break}}
  if(!$target){foreach($s in $slots){if(!$s.shown){$target=$s;break}}};if(!$target){$target=@($slots|Sort-Object {$_.at})[0]}
  $script:zc++;$target.z=$script:zc;$target.covered=$false;$target.shown=$hit;$target.at=$now;$target.next=$now+400;Show-Price $hit $target;Update-Covers
 }
 $trigger=New-Object ModeSwitchPrice.ClickTrigger;$trigger.Reset()
 $clock=$trigger.Clock;$game=[IntPtr]::Zero;$nextGame=0;$phase='unknown';$nextPhase=0
 # Scheduled captures: inspect (find the newly opened window), relocate (a slot's window moved or was covered), restore (after Alt+Tab).
 $jobs=New-Object System.Collections.ArrayList;$lost=@();$lostUntil=0
 Status (P '켜짐 · 아이템을 우클릭 → 조사하면 시세를 띄웁니다.')
 while($true){
  # This PowerShell loop wakes about 4x/s; the 20 ms mouse sampling happens inside ClickTrigger.Wait (C#).
  Pump;$now=$clock.ElapsedMilliseconds;if(!$script:paddle){[void](Paddle)}
  $historyChanged=@{};foreach($m in $histories.Keys){if($histories[$m].Poll()){$historyChanged[$m]=$true}}
  if($now -ge $nextGame){$nextGame=$now+250;$was=$game;$game=[ModeSwitchPrice.Desktop]::Game()
   if($game -eq [IntPtr]::Zero){$hits=Shown-Hits;if($hits.Count){$lost=$hits;$lostUntil=$now+600000};foreach($s in $slots){if($s.shown){Hide-Slot $s}};$jobs.Clear();$trigger.Reset()}
   elseif($was -eq [IntPtr]::Zero -and $lost.Count){if($now -lt $lostUntil){foreach($l in $lost){foreach($d in 400,1000){[void]$jobs.Add(@{at=$now+$d;kind='restore';x=[int](($l.left+$l.right)/2);y=$l.top})}}};$lost=@()}}
  if($game -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 250;continue}
  if($now -ge $nextPhase){$nextPhase=$now+1000;$phase=Read-RaidPhase $raidState $ParentId}
  # Closing the inventory (Tab) or a window gives no click to react to: watch each window's close button.
  # A failed quick check is confirmed with a full title-bar search: a moved window is followed, a covered or
  # replaced one is read again where it was, and only when no title bar is left is the slot hidden.
  foreach($s in $slots){$shown=$s.shown;if(!$shown -or $now -lt $s.next){continue}
   $s.next=$now+$(if($shown.fullOnly){600}else{250})
   if(!$shown.fullOnly -and [ModeSwitchPrice.Desktop]::StillOpen($shown.cx,$shown.cy,$shown.r)){continue}
   $again=$null;$bars=0;try{$again=Find-Window $game $shown ([ref]$bars)}catch{}
   if($again -and $null -ne $again.dx){
    foreach($k in 'left','right','start','cx'){$shown[$k]+=$again.dx};foreach($k in 'top','bottom','cy'){$shown[$k]+=$again.dy}
    $shown.fullOnly=$false;Show-Price $shown $s;Update-Covers
   }elseif($again){
    if(!$shown.fullOnly){Log-CloseMiss $shown $again}
    $shown.cx=$again.cx;$shown.cy=$again.cy;$shown.fullOnly=![ModeSwitchPrice.Desktop]::StillOpen($shown.cx,$shown.cy,$shown.r)
   }elseif($bars -gt 0){[void]$jobs.Add(@{at=$now;kind='relocate';slot=$s;x=[int](($shown.left+$shown.right)/2);y=$shown.top});$s.next=$now+600}
   else{Hide-Slot $s}}
  foreach($s in $slots){if($s.shown -and $historyChanged.ContainsKey($s.shown.mode)){Show-Price $s.shown $s}}
  # Wait for a click no longer than the next job or close check (at most 250 ms).
  $wait=250;foreach($j in $jobs){$wait=[Math]::Min($wait,$j.at-$now)};foreach($s in $slots){if($s.shown){$wait=[Math]::Min($wait,$s.next-$now)}}
  $ev=$trigger.Wait($phase -eq 'started',[int][Math]::Max(0,$wait));$now=$clock.ElapsedMilliseconds
  if($ev -eq [ModeSwitchPrice.ClickTrigger]::Inspect){
   # Inspect window appears a few frames after the menu click; slow first-time model loads can delay it:
   # keep trying up to 2.8 s, stopping at the first window that is not already shown.
   foreach($j in @($jobs)){if($j.kind -eq 'inspect'){$jobs.Remove($j)}}
   $dbl=$trigger.Double;foreach($d in 90,150,300,450,650,900,1200,1600,2200,2800){[void]$jobs.Add(@{at=$now+$d;kind='inspect';x=$trigger.X;y=$trigger.Y;dbl=$dbl;deep=($d -ge 900)})}
  }elseif($ev -eq [ModeSwitchPrice.ClickTrigger]::Click){
   # A click may have closed a window: check every shown one soon. It also brings the clicked window to the front.
   Raise-At $trigger.X $trigger.Y
   foreach($s in $slots){if($s.shown){$s.next=[Math]::Min($s.next,$now+250)}}
  }elseif($ev -eq [ModeSwitchPrice.ClickTrigger]::Escape){foreach($s in $slots){if($s.shown){Hide-Slot $s}};$jobs.Clear()}
  $due=$null;foreach($j in $jobs){if($j.at -le $now -and (!$due -or $j.at -lt $due.at)){$due=$j}};if(!$due){continue}
  [void]$jobs.Remove($due)
  try {
   if($due.kind -eq 'relocate'){
    $hit=Scan $game $due.x $due.y $null
    if([ModeSwitchPrice.Desktop]::Game() -ne $game){continue}
    $s=$due.slot;if(!$s.shown){continue}
    # The window there now belongs to another slot (this one was covered): drop this slot; otherwise follow.
    $other=$false;if($hit){foreach($o in $slots){if(!($o -eq $s) -and $o.shown -and (Same-Place $o.shown $hit)){$other=$true}}}
    if(!$hit -or $other){Hide-Slot $s}else{$s.shown=$hit;$s.next=$now+400;Show-Price $hit $s}
    continue
   }
   $hit=Scan $game $due.x $due.y (Shown-Hits) ($due.kind -ne 'inspect' -or $due.deep)
   if([ModeSwitchPrice.Desktop]::Game() -ne $game){$jobs.Clear();continue}
   # A double-click on a rig, backpack or case (items with their own grid) opens that container's window, which has
   # the same title bar: no price for it.
   if($hit -and !$hit.old -and $due.dbl -and $hit.item.Grid){foreach($j in @($jobs)){if($j.kind -eq $due.kind){$jobs.Remove($j)}};continue}
   if($hit -and !$hit.old){foreach($j in @($jobs)){if($j.kind -eq $due.kind){$jobs.Remove($j)}};Place $hit $now}
   elseif($due.kind -eq 'inspect' -and !@($jobs|Where-Object {$_.kind -eq 'inspect'}).Count){Save-MissFrame $game $(if($script:lastBars -eq 0){P '조사 후 상세보기 제목줄을 찾지 못함'}else{P '조사 후 아이템 이름 인식 실패'})}
  }catch{Log-Miss 'error' @($_.Exception.Message);Status (P '시세 인식 오류: {0}' @([ModeSwitchPrice.Language]::Error($_.Exception)));$jobs.Clear()}
  foreach($c in $catalogs.Values){if($c.Error){if($lastCatalogErrors[$c.Mode] -ne $c.ErrorDetail){Log-Miss 'refresh-error' @($c.ErrorDetail);$lastCatalogErrors[$c.Mode]=$c.ErrorDetail};Status $c.Error}}
 }
}catch{if(Get-Command Log-Miss -ErrorAction SilentlyContinue){Log-Miss 'error' @($_.Exception.Message)};Status (P '시세 인식 오류: {0}' @([ModeSwitchPrice.Language]::Error($_.Exception)))}
finally{if($slots){foreach($s in $slots){foreach($t in $s.price,$s.quest,$s.hide,$s.key){if($null -ne $t){$t.Dispose()}}}}}
