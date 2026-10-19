param([int]$ParentId,[switch]$Hidden,[string]$Page='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'I18n.ps1')
Add-Type -AssemblyName System.Windows.Forms,System.Drawing,System.Net.Http,System.Runtime.WindowsRuntime
Add-Type -Path (Join-Path $PSScriptRoot '../price/PriceHelper.dll')
Add-Type -Path (Join-Path $PSScriptRoot 'CompanionUI.dll')
. (Join-Path $PSScriptRoot 'CompanionCore.ps1')
. (Join-Path $PSScriptRoot 'ControlBinding.ps1')
$null=[ModeSwitchPrice.Desktop]::SetProcessDPIAware()
$store=Join-Path $env:LOCALAPPDATA 'ModeSwitch';[void][IO.Directory]::CreateDirectory($store)
$settingsPath=Join-Path $store 'companion-settings.json';$pinsPath=Join-Path $store 'map-pins.json'
$config=@{phoneCode='';showQuests=$true;showKeys=$true;mapKeyIgnore='';shotPos=$true;shotDelete=$true;mapDefaults=0;maps=$true;discord=$false;maintenance=$false;idle=20;delay=1100;mapKey=77;exitKey=79;webhook='';relayKey=''}
if(Test-Path $settingsPath){try{$old=Get-Content $settingsPath -Raw -Encoding UTF8|ConvertFrom-Json;foreach($k in @($config.Keys)){if($null -ne $old.$k){$config[$k]=$old.$k}}}catch{}}
# Enable every map checkbox on the first upgraded/new run, then respect saved choices.
if(Enable-MapDefaults $config){
 if(!(Test-Path 'HKCU:\Software\ModeSwitch')){$null=New-Item 'HKCU:\Software\ModeSwitch'}
 Set-ItemProperty 'HKCU:\Software\ModeSwitch' 'MapScreenshotKey' ([string][char][int]$config.mapKey)
 $config|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8
}
$pins=@{};if(Test-Path $pinsPath){try{$obj=Get-Content $pinsPath -Raw -Encoding UTF8|ConvertFrom-Json;foreach($p in $obj.PSObject.Properties){$pins[$p.Name]=@($p.Value)}}catch{}}
# PS 5.1 ConvertFrom-Json emits a JSON array as ONE pipeline object; ForEach-Object unrolls it into rows.
$catalog=@(Get-Content (Join-Path $PSScriptRoot '../maps/catalog.json') -Raw -Encoding UTF8|ConvertFrom-Json|ForEach-Object {$_})
foreach($row in $catalog){$row|Add-Member -NotePropertyName displayName -NotePropertyValue (T $row.name)}
# Windows, each opened from its own BlueAsh menu entry (page request: -Page or companion-page.txt):
#   map@<hwnd>     - 게임별 기능 > 타르코프 > 인게임 지도 ⚙ (a page inside BlueAsh's main window)
#   discord@<hwnd> - 디스코드 알림 (sidebar page; shared by every game)
#   hideout        - 게임별 기능 > 타르코프 > 은신처 진행도 (its own resizable window)
$fontName=[ModePulse.I18n]::FontName;$uiFont=New-Object Drawing.Font($fontName,10)
# BlueAsh.exe (the parent) gives every extra window its mark on the title bar and taskbar (CompanionUI Brand).
$appExe='';try{$appExe=(Get-Process -Id $ParentId -ErrorAction Stop).Path}catch{}
# The two pages match BlueAsh's own pages: 16 px text, 14 px hints, bold section headings, 704x590 inside the page box.
# Pixel sizes: the pages are created DPI-unaware inside BlueAsh's window, and point sizes would make the native
# text boxes (HFONT from the process's system DPI) larger than the labels.
$px=[Drawing.GraphicsUnit]::Pixel;$pageFont=New-Object Drawing.Font($fontName,16,[Drawing.FontStyle]::Regular,$px);$hintFont=New-Object Drawing.Font($fontName,14,[Drawing.FontStyle]::Regular,$px);$sectionFont=New-Object Drawing.Font($fontName,16,[Drawing.FontStyle]::Bold,$px)
function New-Panel($title){
 $f=New-Object ModePulse.PageForm;$f.SuspendLayout();$f.Text=$title;$f.ClientSize=New-Object Drawing.Size(704,590);$f.StartPosition='CenterScreen';$f.BackColor=[ModePulse.Theme]::Bg;$f.ForeColor=[ModePulse.Theme]::Ink;$f.Font=$pageFont;$f.FormBorderStyle='FixedDialog';$f.MaximizeBox=$false
 [ModePulse.Brand]::Apply($f,$appExe)
 $f.add_FormClosing({param($s,$e)if($e.CloseReason -eq 'UserClosing'){$e.Cancel=$true;$s.Hide()}});return $f
}
$mapForm=New-Panel (T 'BlueAsh · 인게임 중 지도 보기');[ModePulse.Embed]::Identify($mapForm,'Map')
$discordForm=New-Panel (T 'BlueAsh · 디스코드 알림');[ModePulse.Embed]::Identify($discordForm,'Discord')
# The helpers below add to whichever window $form points at. Section/Hint are restyled after the theme is applied.
$script:sections=@();$script:hints=@();$script:primary=@()
function Label($text,$x,$y,$width,$height=30){$c=New-Object Windows.Forms.Label;$c.Text=$text;$c.SetBounds($x,$y,$width,$height);$form.Controls.Add($c);return $c}
function Section($text,$y){$c=Label $text 8 $y 688 28;$script:sections+=$c;return $c}
function Hint($text,$x,$y,$width,$height=24){$c=Label $text $x $y $width $height;$script:hints+=$c;return $c}
function Check($text,$y,$value){$c=New-Object Windows.Forms.CheckBox;$c.Text=$text;$c.SetBounds(8,$y,688,30);$c.Checked=$value;$form.Controls.Add($c);return $c}
function Button($text,$x,$y,$width,[switch]$Primary){$c=New-Object ModePulse.ThemeButton;$c.Text=$text;$c.SetBounds($x,$y,$width,40);$form.Controls.Add($c);if($Primary){$script:primary+=$c};return $c}
# Status lines are plain text (they looked like a box to type into); long text wraps inside the given height.
function StatusBox($y,$height,[switch]$Compact){$c=New-Object Windows.Forms.Label;$c.AutoSize=$false;$c.UseMnemonic=$false;$c.SetBounds(8,$y,688,$height);$c.Text=(T '준비 중');$c.Font=$hintFont;$form.Controls.Add($c);return $c}

# ---- 인게임 지도 ----
$form=$mapForm
$null=Section (T '키') 12
$keyLabel=Label (T '지도 키') 8 50 130;$mapKey=New-Object Windows.Forms.ComboBox;$mapKey.DropDownStyle='DropDownList';foreach($i in 65..90){$null=$mapKey.Items.Add([string][char]$i)};$form.Controls.Add($mapKey)
$mapKey.SetBounds(144,46,90,30);$mapKey.SelectedItem=[string][char][int]$config.mapKey
$keysLabel=Label (T '탈출구 확인 키') 266 50 160
# Read from the game's Control.ini, not typed here: shown as text.
$bindingLabel=New-Object Windows.Forms.Label;$bindingLabel.AutoSize=$false;$bindingLabel.UseMnemonic=$false;$bindingLabel.TextAlign='MiddleLeft';$bindingLabel.SetBounds(430,46,266,30);$form.Controls.Add($bindingLabel)
$bindingDetails=Hint (T '게임 키 설정(Control.ini)을 확인하는 중') 8 86 688
$binding=$null;$bindingStamp='';$nextBindingCheck=[DateTime]::MinValue;$triggerStates=@{}
$null=Section (T '지도에 표시') 126
$questCheck=Check (T '진행 중인 퀘스트 목표') 158 $config.showQuests;$questCheck.Width=330
# The keys and keycards the player listed beside the map (my-keys.json): where they open, as small gold keys and cyan cards.
$keyCheck=Check (T '내 열쇠·카드키 목록 (지도 오른쪽에서 넣고 빼기)') 158 $config.showKeys;$keyCheck.SetBounds(346,158,350,30)
# Position from the game's own screenshot names (the player presses the screenshot key; nothing is automated).
$shotCheck=Check (T '내 위치 (게임 스크린샷을 찍으면 그 자리로 갱신)') 192 $config.shotPos
# The map key also as the game's second screenshot key (BlueAsh writes Control.ini while the game is closed:
# tarkov_keys.h, registry MapScreenshotKey). One press: the game saves its own screenshot, BlueAsh opens the map.
$shotOnMap=Check (T '지도 키를 누를 때 내 위치도 갱신') 226 ([string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name MapScreenshotKey -ErrorAction SilentlyContinue).MapScreenshotKey -ne '')
$shotDelete=Check (T '위치를 읽은 스크린샷은 자동 삭제') 260 $config.shotDelete
$null=Section (T '지도와 탈출구') 306
$mapBtn=Button (T '지도 열기 · 탈출구 편집') 8 338 250
$resetBtn=Button (T '이번 레이드 표시 지우기') 270 338 250
$keysClear=Button (T '내 열쇠 목록 비우기') 532 338 164
$keysClear.add_Click({$p=Join-Path $store 'my-keys.json';try{if(Test-Path $p){[IO.File]::WriteAllText($p,'{}')};Status (T '내 열쇠 목록을 비웠습니다. 시세 창으로 열쇠를 다시 조사하면 다시 표시됩니다.')}catch{}})
$status=StatusBox 392 126
$closeMapBtn=Button (T '닫기') 424 540 120;$saveMapBtn=Button (T '저장') 556 540 140 -Primary

# ---- 디스코드 알림 (모든 게임 공통) ----
$form=$discordForm
# Secrets (relay key, webhook) are DPAPI-encrypted for this Windows user.
function Protect([string]$s){if($s){ConvertFrom-SecureString (ConvertTo-SecureString $s -AsPlainText -Force)}else{''}}
function Unprotect([string]$s){if(!$s){return ''};try{(New-Object Net.NetworkCredential('',(ConvertTo-SecureString $s))).Password}catch{''}}
# Relay URL ships with the build (companion/relay.txt). Empty = one-click connect not set up; webhook still works.
$relay='';$relayFile=Join-Path $PSScriptRoot 'relay.txt';if(Test-Path $relayFile){$relay=([IO.File]::ReadAllText($relayFile)).Trim().TrimEnd('/');if($relay -notmatch '^https://[A-Za-z0-9.-]+(/[A-Za-z0-9._/-]*)?$'){$relay=''}}
[ModePulse.QuestWiki]::Relay=$relay
$script:relayKey=Unprotect $config.relayKey
# Phone alerts (web push: rings even while Discord is open on the PC). Server status alerts are sent by the relay
# itself and switched on the phone. Discord DMs and the webhook were dropped from the page (2026-10-12): their controls
# still exist, hidden, because DiscordTransport.ps1 reads them; a build without the relay keeps the webhook route.
$phoneTitle=Section (T '휴대폰 알림') 12
$phoneLabel=Hint '' 8 50 466 28
$phoneBtn=Button (T '휴대폰 연결') 486 44 210 -Primary;$phoneBtn.Visible=[bool]$relay
$phoneHelp=Hint (T "휴대폰 연결을 누르면 PC에 QR이 뜹니다. 휴대폰 카메라로 찍고 알림 허용을 누르세요.`r`n게임이 시작될 때 PC에 움직임이 없으면 자리 비움으로 보고 휴대폰으로 바로 알려 드립니다.") 8 84 688 48
$linkLabel=Hint '' 8 180 466 28
$connectBtn=Button (T 'Discord 연결') 486 174 210
$helpBtn=Button (T '웹후크 만드는 법') 486 174 210
$helpBtn.add_Click({[void][Windows.Forms.MessageBox]::Show((T "1. Discord에서 알림 받을 서버의 채널(예: #모드펄스)을 정합니다. 내 서버가 없으면 왼쪽 + 로 서버를 만드세요.`r`n2. 채널 이름 옆 톱니바퀴(채널 편집) → 연동 → 웹후크 → 새 웹후크`r`n3. 웹후크 URL 복사 → 여기 웹후크 주소 칸에 붙여넣기 → 저장`r`n4. 테스트 알림 보내기로 도착 확인`r`n`r`n웹후크 주소는 이 PC에만 암호화해 저장합니다. 다른 사람에게 알려 주지 마세요."),(T 'BlueAsh · 디스코드 웹후크'))})
$hookLabel=Label (T '웹후크 주소') 8 146 130
$hook=New-Object Windows.Forms.TextBox;$hook.UseSystemPasswordChar=$true;$hook.SetBounds(140,142,556,30);$form.Controls.Add($hook)
$hook.Text=Unprotect $config.webhook
foreach($c in $linkLabel,$connectBtn){$c.Visible=$false}
$webhookRoute=!$relay;foreach($c in $helpBtn,$hookLabel,$hook){$c.Visible=$webhookRoute}
$top=$(if($webhookRoute){196}else{142})
$alertTitle=Section (T '알림 받을 상황') $top
$discordCheck=Check (T '타르코프 · 자리를 비운 사이 레이드가 시작되면') ($top+32) $config.discord
$maintenanceCheck=Check '' 0 $false;$maintenanceCheck.Visible=$false
$idleLabel=Label (T '자리 비움 기준') 8 ($top+78) 130
# The seconds are kept in a NumericUpDown that is never shown (its white spin buttons did not fit the app);
# the page shows  −  15  +  in the app's own buttons instead, 5 seconds a step.
$idle=New-Object Windows.Forms.NumericUpDown;$idle.Minimum=10;$idle.Maximum=600;$idle.Value=[Math]::Max(10,[Math]::Min(600,[int]$config.idle))
$idleBox=New-Object Windows.Forms.Panel;$idleBox.Size=New-Object Drawing.Size(132,32);$form.Controls.Add($idleBox)
$idleMinus=New-Object ModePulse.ThemeButton;$idleMinus.Text='−';$idleMinus.SetBounds(0,0,32,32);$idleBox.Controls.Add($idleMinus)
$idleValue=New-Object Windows.Forms.Label;$idleValue.AutoSize=$false;$idleValue.TextAlign='MiddleCenter';$idleValue.SetBounds(34,0,64,32);$idleValue.Text=[string]$idle.Value;$idleBox.Controls.Add($idleValue)
$idlePlus=New-Object ModePulse.ThemeButton;$idlePlus.Text='+';$idlePlus.SetBounds(100,0,32,32);$idleBox.Controls.Add($idlePlus)
$idleMinus.add_Click({$idle.Value=[Math]::Max($idle.Minimum,$idle.Value-5);$idleValue.Text=[string]$idle.Value})
$idlePlus.add_Click({$idle.Value=[Math]::Min($idle.Maximum,$idle.Value+5);$idleValue.Text=[string]$idle.Value})
$idleHint=Hint (T '초 동안 입력이 없으면 자리 비움으로 봅니다.') 240 ($top+80) 456
$serverHint=Hint (T '타르코프 점검·서버 알림은 휴대폰 BlueAsh 화면에서 켜고 끕니다. PC가 꺼져 있어도 옵니다.') 8 ($top+118) 688
$discordStatus=StatusBox ($top+154) 64 -Compact
$testBtn=Button (T '테스트 알림 보내기') 8 540 240
$hideBtn=Button (T '닫기') 424 540 120;$saveBtn=Button (T '저장') 556 540 140 -Primary

# ---- 은신처 레벨 (아이템 시세 표시에서 사용) ----
# Hideout levels per game mode, used by the price overlay to show only upgrades still ahead.
# Stations come from the price worker's cached tarkov.dev data (tasks-<mode>.tsv).
function Show-HideoutLevels {
 . (Join-Path $PSScriptRoot '../price/PriceCore.ps1')
 $levelsPath=Join-Path $store 'hideout-levels.json';$logs=[string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name RaidLogs -ErrorAction SilentlyContinue).RaidLogs
 $mode='pve';try{$m=Read-LogMode $logs;if($m){$mode=$m}}catch{}
 $edition=[ModeSwitchPrice.TaskIndex]::StashMaxedByEdition((Join-Path $env:APPDATA 'Battlestate Games\Escape from Tarkov\Settings\Game.ini'))
 # One card per facility with a button per level: one click sets and saves it (no spinners, no save button).
 # The window can be resized: the number of card columns, the card width and the level buttons follow it ($relayout).
 $f=New-Object Windows.Forms.Form;$f.Text=(T 'BlueAsh · 은신처 진행도');[ModePulse.Brand]::Apply($f,$appExe);$f.StartPosition='CenterScreen';$f.FormBorderStyle='Sizable';$f.MaximizeBox=$true;$f.MinimizeBox=$false;$f.Font=$uiFont;$f.TopMost=$true
 $f.MinimumSize=New-Object Drawing.Size(560,420);$f.ClientSize=New-Object Drawing.Size(720,700)
 $sizeKey='HKEY_CURRENT_USER\Software\ModeSwitch';$sz=[string][Microsoft.Win32.Registry]::GetValue($sizeKey,'HideoutWindow',$null)
 if($sz -match '^(\d+),(\d+)$'){$f.ClientSize=New-Object Drawing.Size([Math]::Max(540,[int]$Matches[1]),[Math]::Max(380,[int]$Matches[2]))}
 $bold=New-Object Drawing.Font($fontName,10.5,[Drawing.FontStyle]::Bold);$small=New-Object Drawing.Font($fontName,9)
 $top=New-Object Windows.Forms.Panel;$top.Dock='Top';$top.Height=112;$f.Controls.Add($top)
 $title=New-Object Windows.Forms.Label;$title.Text=(T '은신처 진행도');$title.Font=New-Object Drawing.Font($fontName,15,[Drawing.FontStyle]::Bold);$title.AutoSize=$true;$title.Location=New-Object Drawing.Point(18,12);$top.Controls.Add($title)
 $modeButtons=@{};$modeOrder=@();foreach($pair in @(@('pve','PvE'),@('pvp-season',(T 'PvP 시즌')),@('regular',(T 'PvP 일반')))){$b=New-Object ModePulse.ThemeButton;$b.Text=$pair[1];$b.Tag=$pair[0];$b.Size=New-Object Drawing.Size(112,32);$top.Controls.Add($b);$modeButtons[$pair[0]]=$b;$modeOrder+=$b}
 $hint=New-Object Windows.Forms.Label;$hint.Text=(T '시설마다 지금 지어진 레벨을 누르세요 (0 = 아직 안 지음). 바로 저장되고, 모드마다 따로 기억합니다.')+"`r`n"+(T '아무것도 누르지 않으면 시세 창에 모든 레벨의 재료를 보여 줍니다.')
 $hint.Location=New-Object Drawing.Point(18,56);$top.Controls.Add($hint)
 $flow=New-Object Windows.Forms.FlowLayoutPanel;$flow.Dock='Fill';$flow.AutoScroll=$true;$flow.Padding=New-Object Windows.Forms.Padding(12,2,0,8);$f.Controls.Add($flow);$flow.BringToFront()
 $bottom=New-Object Windows.Forms.Panel;$bottom.Dock='Bottom';$bottom.Height=56;$f.Controls.Add($bottom)
 $note=New-Object Windows.Forms.Label;$note.Location=New-Object Drawing.Point(18,18);$note.AutoEllipsis=$true;$bottom.Controls.Add($note)
 $clear=New-Object ModePulse.ThemeButton;$clear.Text=(T '입력 지우기');$clear.Size=New-Object Drawing.Size(140,34);$bottom.Controls.Add($clear)
 $close=New-Object ModePulse.ThemeButton;$close.Text=(T '닫기');$close.Size=New-Object Drawing.Size(112,34);$bottom.Controls.Add($close)
 $script:hideoutIx=@{};$wait=New-Object Windows.Forms.Timer;$wait.Interval=400;$wait.add_Tick({$ix=$script:hideoutIx[$script:hideoutMode];if(!$ix -or !$ix.Refreshing){$wait.Stop();& $fill}})
 $script:hideoutMode=$mode;$script:hideoutLevels=@{};$script:hideoutButtons=@{};$script:hideoutCards=@();$script:hideoutEntered=$false
 # Everything that depends on the window width: header and footer positions, cards per row, card width, level buttons.
 $relayout={
  # Header: mode buttons beside the title when they fit, else on the next line; the hint as tall as its wrapped text.
  $w=$f.ClientSize.Width;$bw=3*112+2*6;$x=$w-18-$bw;$by=14;if($x -lt $title.Right+16){$x=18;$by=$title.Bottom+8}
  foreach($b in $modeOrder){$b.Location=New-Object Drawing.Point($x,$by);$x+=118}
  $hy=[Math]::Max($title.Bottom,$by+32)+8;$hw=[Math]::Max(200,$w-36);$hh=[Windows.Forms.TextRenderer]::MeasureText($hint.Text,$hint.Font,(New-Object Drawing.Size($hw,0)),[Windows.Forms.TextFormatFlags]::WordBreak).Height
  $hint.SetBounds(18,$hy,$hw,$hh+4);$top.Height=$hy+$hh+12
  $close.Location=New-Object Drawing.Point(($w-18-112),11);$clear.Location=New-Object Drawing.Point(($w-18-112-8-140),11);$note.Size=New-Object Drawing.Size([Math]::Max(120,$w-18-112-8-140-30),26)
  $avail=$flow.ClientSize.Width-$flow.Padding.Left-[Windows.Forms.SystemInformation]::VerticalScrollBarWidth-4
  $cols=[Math]::Max(1,[int][Math]::Floor(($avail+12)/(300+12)));$cw=[int][Math]::Floor(($avail+12)/$cols)-12
  $flow.SuspendLayout()
  foreach($c in $script:hideoutCards){$c.Card.Width=$cw;$c.Max.Location=New-Object Drawing.Point(($cw-12-$c.Max.PreferredWidth),10);$c.Name.Width=[Math]::Max(60,$cw-24-$c.Max.PreferredWidth-8)
   if($c.Lock){$c.Lock.SetBounds(12,36,$cw-24,28);$c.Lock.BringToFront();continue}
   $n=$c.Buttons.Count;$step=[Math]::Min(56,[int][Math]::Floor(($cw-24+6)/$n));$bx=12;foreach($b in $c.Buttons){$b.SetBounds($bx,36,$step-6,28);$bx+=$step}}
  $flow.ResumeLayout()
 }
 $paint={
  foreach($k in $modeButtons.Keys){$b=$modeButtons[$k];$on=$k -eq $script:hideoutMode;$b.BackColor=$(if($on){[ModePulse.Theme]::Accent}else{[ModePulse.Theme]::Field});$b.ForeColor=[ModePulse.Theme]::Ink}
  foreach($id in $script:hideoutButtons.Keys){foreach($b in $script:hideoutButtons[$id]){$on=$script:hideoutEntered -and [int]$b.Tag.v -eq [int]$script:hideoutLevels[$id];$b.BackColor=$(if($on){[ModePulse.Theme]::Accent}else{[ModePulse.Theme]::Field});$b.ForeColor=[ModePulse.Theme]::Ink}}
  foreach($l in $script:hideoutLocks){$l.BackColor=[ModePulse.Theme]::Accent;$l.ForeColor=[Drawing.Color]::White}
  $built=@($script:hideoutLevels.Values|Where-Object {$_ -gt 0}).Count
  $note.Text=$(if($script:hideoutEntered){(T '저장됨 · 지어진 시설 {0} / {1}' @($built,$script:hideoutLevels.Count))}else{(T '입력 안 함 · 필요한 레벨 모두 표시 중')})
 }
 $save={
  $d=New-Object 'System.Collections.Generic.Dictionary[string,int]';foreach($k in $script:hideoutLevels.Keys){$d[$k]=[int]$script:hideoutLevels[$k]}
  try{[ModeSwitchPrice.TaskIndex]::WriteLevels($levelsPath,$script:hideoutMode,$d);return $true}catch{$note.Text=(T '저장 실패: ')+$_.Exception.Message;return $false}
 }
 $pick={
  $t=$this.Tag;$script:hideoutLevels[$t.id]=[int]$t.v;$script:hideoutEntered=$true
  if(& $save){& $paint}
 }
 $fill={
  $flow.SuspendLayout();foreach($c in @($flow.Controls)){$c.Dispose()};$flow.Controls.Clear();$script:hideoutButtons=@{};$script:hideoutLevels=@{};$script:hideoutCards=@();$script:hideoutLocks=@()
  # Station lists come from the price worker's cache, which only holds the modes played so far: a missing (or old format) one is fetched here.
  $ix=$script:hideoutIx[$script:hideoutMode];if(!$ix){$ix=New-Object ModeSwitchPrice.TaskIndex($script:hideoutMode,$store);[void]$ix.LoadCache();$script:hideoutIx[$script:hideoutMode]=$ix}
  $have=$ix.Ready;if(!$have){$ix.RefreshIfStale([TimeSpan]::FromHours(6));if($ix.Refreshing){$wait.Start()}}
  # A prestige or season wipe seen in the logs after the levels were saved starts the hideout over.
  $wiped=$false;try{$wiped=[ModeSwitchPrice.TaskIndex]::ClearIfOlder($levelsPath,$script:hideoutMode,[ModePulse.Quests]::ResetAt($(if($logs){$logs}else{'C:\Battlestate Games\Escape from Tarkov\Logs'}),$script:hideoutMode))}catch{}
  $saved=[ModeSwitchPrice.TaskIndex]::ReadLevels($levelsPath,$script:hideoutMode);$script:hideoutEntered=$null -ne $saved
  $stations=@($ix.Stations)
  if(!$have -or $stations.Count -eq 0){$flow.ResumeLayout();& $paint;$note.Text=$(if($ix.Refreshing){(T '시설 목록을 받아오는 중...')}elseif($ix.Error){(T '시설 목록을 받지 못했습니다 · 인터넷 연결을 확인하세요')}else{(T '시설 목록이 없습니다')});return}
  $stashAuto=$false
  # Unheard comes with the Cultist Circle built: not something to track here (and its materials are skipped in the price tags).
  $cultistBuilt=[ModeSwitchPrice.TaskIndex]::CultistByEdition((Join-Path $env:APPDATA 'Battlestate Games\Escape from Tarkov\Settings\Game.ini'))
  foreach($s in $stations){
   $isStash=$s.Id -eq [ModeSwitchPrice.TaskIndex]::StashId
   # Edition-given stations: the stash (Unheard/EOD) and the cultist circle (Unheard only) are fixed at max.
   $fixed=($isStash -and $edition) -or ($cultistBuilt -and $s.Id -eq [ModeSwitchPrice.TaskIndex]::CultistId)
   $card=New-Object ModePulse.ThemeCard;$card.Size=New-Object Drawing.Size(300,74);$card.Margin=New-Object Windows.Forms.Padding(0,0,12,10)
   $name=New-Object Windows.Forms.Label;$name.Text=$s.Name;$name.Font=$bold;$name.SetBounds(12,8,200,26);$card.Controls.Add($name)
   $max=New-Object Windows.Forms.Label;$max.Text=(T '최대 Lv.')+$s.MaxLevel;$max.Font=$small;$max.AutoSize=$true;$card.Controls.Add($max)
   $v=0;if($saved -and $saved.ContainsKey($s.Id)){$v=[Math]::Min($s.MaxLevel,$saved[$s.Id])}
   # Edition-given stations start at their last level: set (and save) it, so they count as built.
   if($fixed -and $v -lt $s.MaxLevel){$v=$s.MaxLevel;$stashAuto=$true}
   $script:hideoutLevels[$s.Id]=$v
   # Fixed by the edition: a badge instead of level buttons.
   if($fixed){$lock=New-Object ModePulse.ThemeButton;$lock.TabStop=$false;$lock.Cursor=[Windows.Forms.Cursors]::Default;$lock.Text='Lv.'+$s.MaxLevel+(T ' 고정 · 에디션 기본');$card.Controls.Add($lock);$script:hideoutLocks+=$lock
    $script:hideoutCards+=[pscustomobject]@{Card=$card;Name=$name;Max=$max;Buttons=@();Lock=$lock};$flow.Controls.Add($card);continue}
   $n=[Math]::Max(1,$s.MaxLevel)+1;$list=@();for($lv=0;$lv -lt $n;$lv++){$b=New-Object ModePulse.ThemeButton;$b.Text=[string]$lv;$b.Tag=@{id=$s.Id;v=$lv};$b.add_Click($pick);$card.Controls.Add($b);$list+=$b}
   $script:hideoutButtons[$s.Id]=$list;$script:hideoutCards+=[pscustomobject]@{Card=$card;Name=$name;Max=$max;Buttons=$list;Lock=$null};$flow.Controls.Add($card)}
  if($stashAuto){$script:hideoutEntered=$true;[void](& $save)}
  Apply-Theme $f;$flow.BackColor=[ModePulse.Theme]::Bg;$flow.ResumeLayout();& $relayout;& $paint
  if($wiped){$note.Text=(T '로그에서 프레스티지/시즌 초기화를 확인해 레벨을 처음부터 시작합니다')}
 }
 foreach($b in $modeButtons.Values){$b.add_Click({$script:hideoutMode=[string]$this.Tag;& $fill})}
 $clear.add_Click({[ModeSwitchPrice.TaskIndex]::WriteLevels($levelsPath,$script:hideoutMode,$null);& $fill})
 $close.add_Click({$f.Close()})
 $f.add_Resize({& $relayout});$f.add_FormClosed({$wait.Stop();$wait.Dispose()})
 $f.add_FormClosing({if($f.WindowState -eq 'Normal'){[Microsoft.Win32.Registry]::SetValue($sizeKey,'HideoutWindow',"$($f.ClientSize.Width),$($f.ClientSize.Height)")}})
 Apply-Theme $f;& $fill;& $relayout;[void]$f.ShowDialog();$f.Dispose()
}
function Open-Page([string]$page){
 switch($page){
  'discord'{$discordForm.Show();$discordForm.WindowState='Normal';$discordForm.Activate()}
  'hideout'{Show-HideoutLevels}
  # discord@<main window>: the 디스코드 알림 page inside BlueAsh's main window (sidebar).
  # name@<main window>: a page inside BlueAsh's main window, in the page box under its header (main-window units).
  {$_ -like 'discord@*'}{if(![ModePulse.Embed]::Attach($discordForm,[IntPtr][long]$_.Substring(8),260,118,852,622)){$discordForm.Show()};break}
  {$_ -like 'map@*'}{if(![ModePulse.Embed]::Attach($mapForm,[IntPtr][long]$_.Substring(4),260,118,852,600)){$mapForm.Show()};Check-MapKeyConflicts;break}
  default{$mapForm.Show();$mapForm.WindowState='Normal';$mapForm.Activate();Check-MapKeyConflicts}
 }
}
# ---- 지도 키 충돌 (타르코프 키 설정) ----
# The map key alone (no Ctrl/Alt/Shift) must not also do something in the game. When Control.ini binds it, ask once
# per key whether to move those game bindings; chosen keys go to TarkovKeyMoves, which BlueAsh writes into
# Control.ini while the game is closed (tarkov_keys.h). "No" is remembered for that key (mapKeyIgnore).
$gameKeyNames=@{WeaponMounting=(T '무기 거치');Recorder=(T '녹음기');ToggleBipods=(T '양각대');Walk=(T '걷기');Sprint=(T '달리기');Jump=(T '점프');Vaulting=(T '넘어가기');Duck=(T '앉기');Prone=(T '엎드리기');StepRight=(T '오른쪽 기울이기');StepLeft=(T '왼쪽 기울이기');LeanLockRight=(T '오른쪽 기울이기 고정');LeanLockLeft=(T '왼쪽 기울이기 고정');Shoot=(T '사격');Aim=(T '조준');ReloadWeapon=(T '재장전');QuickReloadWeapon=(T '빠른 재장전');ShootingMode=(T '사격 모드');CheckFireMode=(T '사격 모드 확인');NextMagazine=(T '다음 탄창');PreviousMagazine=(T '이전 탄창');UnloadMagazine=(T '탄창 비우기');CheckAmmo=(T '탄약 확인');CheckChamber=(T '약실 확인');ChamberUnload=(T '약실 비우기');ChangeAimScope=(T '조준경 변경');ChangeAimScopeMagnification=(T '배율 변경');ExamineWeapon=(T '무기 살펴보기');FoldStock=(T '개머리판 접기');PrimaryWeaponFirst=(T '주무기 1');PrimaryWeaponSecond=(T '주무기 2');Knife=(T '칼');SecondaryWeapon=(T '권총');QuickSecondaryWeapon=(T '빠른 권총');QuickKnife=(T '빠른 칼');ThrowGrenade=(T '수류탄');ToggleGoggles=(T '고글');ToggleVoip=(T '음성 채팅');PushToTalk=(T '음성 채팅 말하기');Tactical=(T '전술 장비');NextTacticalDevice=(T '다음 전술 장비');ToggleHeadLight=(T '헤드라이트');SwitchHeadLight=(T '헤드라이트 모드');Nidnod=(T '고개 끄덕이기');WatchTime=(T '시간 확인');Interact=(T '상호작용');ThrowItem=(T '아이템 버리기');DropBackpack=(T '가방 내려놓기');Breath=(T '숨 참기');Mumble=(T '제스처');WatchTimerAndExits=(T '탈출구 확인');Inventory=(T '인벤토리');Slot4=(T '퀵슬롯 4');Slot5=(T '퀵슬롯 5');Slot6=(T '퀵슬롯 6');Slot7=(T '퀵슬롯 7');Slot8=(T '퀵슬롯 8');Slot9=(T '퀵슬롯 9');Slot0=(T '퀵슬롯 0');MakeScreenshot=(T '스크린샷');ToggleInfo=(T '정보 표시');Console=(T '콘솔');ChangePointOfView=(T '시점 변경');TogglePinMode=(T '고정 모드');ToggleLockMode=(T '잠금 모드')}
# Unity key names offered as replacements, with what the player sees.
$gameKeyChoices=[ordered]@{Comma=',';Period='.';Slash='/';Semicolon=';';Quote='''';LeftBracket='[';RightBracket=']';Minus='-';Equals='=';BackQuote='`';Backslash='\'}
foreach($c in 65..90){$gameKeyChoices[[string][char]$c]=[string][char]$c};foreach($d in 0..9){$gameKeyChoices['Alpha'+$d]=[string]$d};foreach($f in 1..11){$gameKeyChoices['F'+$f]='F'+$f}
foreach($d in 0..9){$gameKeyChoices['Keypad'+$d]=(T '숫자패드 ')+$d}
# Keys on movement axes (W/A/S/D...) live in axisBindings, not keyBindings.
function Game-AxisKeys {
 $path=Join-Path $env:APPDATA 'Battlestate Games\Escape from Tarkov\Settings\Control.ini';$keys=@{}
 try{foreach($a in @((Get-Content $path -Raw -Encoding UTF8|ConvertFrom-Json).axisBindings|ForEach-Object {$_})){foreach($pair in @($a.pairs|ForEach-Object {$_})){foreach($side in $pair.positive,$pair.negative){foreach($k in @($side.keyCode|ForEach-Object {$_})){$keys[[string]$k]=1}}}}}catch{}
 return $keys
}
function Game-Bindings {
 $path=Join-Path $env:APPDATA 'Battlestate Games\Escape from Tarkov\Settings\Control.ini';if(!(Test-Path $path)){return ,@()}
 try{return ,@((Get-Content $path -Raw -Encoding UTF8|ConvertFrom-Json).keyBindings|ForEach-Object {$_})}catch{return ,@()}
}
# Game bindings whose single key is the map key; ones with a move already queued are left out.
function Map-KeyConflicts($bindings){
 $key=[string][char][int]$config.mapKey;$pending=[string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name TarkovKeyMoves -ErrorAction SilentlyContinue).TarkovKeyMoves
 $out=@();foreach($b in $bindings){if($b.keyName -eq 'MakeScreenshot' -or $pending -match ('(^|;)'+[regex]::Escape($b.keyName)+':')){continue}
  foreach($v in @($b.variants|ForEach-Object {$_})){$codes=@($v.keyCode|ForEach-Object {$_});if($codes.Count -eq 1 -and $codes[0] -eq $key){$out+=$b.keyName;break}}}
 return ,$out
}
function Check-MapKeyConflicts([switch]$Asked){
 $bindings=Game-Bindings;$conflicts=Map-KeyConflicts $bindings;$key=[string][char][int]$config.mapKey
 if(!$conflicts.Count -or (!$Asked -and $config.mapKeyIgnore -eq $key)){return}
 $names=($conflicts|ForEach-Object {if($gameKeyNames[$_]){$gameKeyNames[$_]}else{$_}}) -join ', '
 $answer=[Windows.Forms.MessageBox]::Show($mapForm,(T '현재 지도 단축키 {0} 가 타르코프 안에서 ''{1}''(으)로 지정되어 있습니다.\r\n그대로 두면 지도를 열 때 게임에서도 같이 동작합니다.\r\n\r\n타르코프 안에 지정되어 있는 키를 변경하시겠습니까?' @($key,$names)),(T 'BlueAsh · 지도 키 충돌'),'YesNo','Warning')
 if($answer -ne 'Yes'){$config.mapKeyIgnore=$key;try{$config|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8}catch{};return}
 # Keys free in the game (no single-key binding uses them) and not BlueAsh's own keys.
 $used=Game-AxisKeys;foreach($b in $bindings){foreach($v in @($b.variants|ForEach-Object {$_})){$codes=@($v.keyCode|ForEach-Object {$_});if($codes.Count -eq 1){$used[$codes[0]]=1}}}
 $free=@($gameKeyChoices.Keys|Where-Object {!$used.ContainsKey($_) -and $_ -ne $key})
 $f=New-Object Windows.Forms.Form;$f.Text=(T 'BlueAsh · 타르코프 키 변경');[ModePulse.Brand]::Apply($f,$appExe);$f.StartPosition='CenterParent';$f.FormBorderStyle='FixedDialog';$f.MaximizeBox=$false;$f.MinimizeBox=$false;$f.Font=$uiFont;$f.TopMost=$true
 $f.ClientSize=New-Object Drawing.Size(620,(138+44*$conflicts.Count))
 $l=New-Object Windows.Forms.Label;$l.Text=(T '게임에서 쓰지 않는 키만 보입니다. 게임이 꺼져 있을 때 자동으로 바뀝니다.');$l.SetBounds(18,14,584,50);$f.Controls.Add($l)
 $boxes=@{};$y=76;$i=0
 foreach($c in $conflicts){$n=New-Object Windows.Forms.Label;$n.Text=$(if($gameKeyNames[$c]){$gameKeyNames[$c]}else{$c})+"  ($key →)";$n.SetBounds(18,$y+4,340,26);$f.Controls.Add($n)
  $box=New-Object Windows.Forms.ComboBox;$box.DropDownStyle='DropDownList';foreach($k in $free){[void]$box.Items.Add($gameKeyChoices[$k])};$box.SelectedIndex=[Math]::Min($i,$box.Items.Count-1);$box.SetBounds(380,$y,220,30);$f.Controls.Add($box);$boxes[$c]=$box;$y+=44;$i++}
 $ok=New-Object ModePulse.ThemeButton;$ok.Text=(T '변경');$ok.SetBounds(380,$y+8,105,34);$ok.DialogResult='OK';$f.Controls.Add($ok);$f.AcceptButton=$ok
 $no=New-Object ModePulse.ThemeButton;$no.Text=(T '취소');$no.SetBounds(495,$y+8,105,34);$no.DialogResult='Cancel';$f.Controls.Add($no);$f.CancelButton=$no
 Apply-Theme $f;$result=$f.ShowDialog($mapForm)
 if($result -eq 'OK'){
  $picked=@{};$moves='';foreach($c in $conflicts){$label=[string]$boxes[$c].SelectedItem;$unity=@($gameKeyChoices.Keys|Where-Object {$gameKeyChoices[$_] -eq $label})[0]
   if(!$unity -or $picked.ContainsKey($unity)){[void][Windows.Forms.MessageBox]::Show($mapForm,(T '기능마다 서로 다른 키를 골라 주세요.'),'BlueAsh');$f.Dispose();return}
   $picked[$unity]=1;$moves+="$($c):$key>$unity;"}
  $old=[string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name TarkovKeyMoves -ErrorAction SilentlyContinue).TarkovKeyMoves
  Set-ItemProperty 'HKCU:\Software\ModeSwitch' 'TarkovKeyMoves' ($old+$moves)
  Status ((T '타르코프 키 변경 예약: ')+(($conflicts|ForEach-Object {$(if($gameKeyNames[$_]){$gameKeyNames[$_]}else{$_})+' → '+[string]$boxes[$_].SelectedItem}) -join ', ')+(T ' · 게임이 꺼져 있을 때 자동 적용'))
 }
 $f.Dispose()
}
$pagePath=Join-Path $store 'companion-page.txt'
$map=New-Object ModePulse.MapWindow
foreach($name in @($catalog.displayName|Select-Object -Unique)){$null=$map.Maps.Items.Add($name)};$map.Maps.SelectedIndex=0
$map.MapKey=[int]$config.mapKey
$script:activeUrl=$hook.Text.Trim()
$script:mapId='';$script:seen=@();$script:lastFrame=@();$script:pendingAt=[DateTime]::MaxValue;$script:captures=0;$script:ocrReady=$false;$script:ocrBlocked=$false;$script:exitRead=$null;$script:exitGeneration=0;$script:mapRead=$null;$script:autoExits=$null;$script:autoExitKey='';$script:taxiLastFrame=@{}
# Each page has its own status box: Status for the map page, DStatus for the Discord page.
function Status($text){$status.Text=$text}
function DStatus($text){$discordStatus.Text=$text}
# Map markers (exits, transits, PMC spawns, bosses, cultists) from the Fandom interactive maps: the snapshot shipped
# in maps/markers.json, replaced by a copy refreshed at most daily in the background. The user's own pin edits
# (map-pins.json) still win per map.
$markerShipped=Join-Path $PSScriptRoot '../maps/markers.json';$markerCache=Join-Path $store 'fandom-markers.json'
[ModePulse.FandomMarkers]::RefreshIfStale($markerCache,[TimeSpan]::FromHours(24))
# Accepted quests (from the game's push-notification logs) and their objective places (tarkov.dev, refreshed daily).
$questLogs=[string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name RaidLogs -ErrorAction SilentlyContinue).RaidLogs;if(!$questLogs){$questLogs='C:\Battlestate Games\Escape from Tarkov\Logs'}
[ModePulse.Quests]::CacheFile=Join-Path $store 'quest-log-index.json'
# First log scan now (seconds once ever, then cached on disk), so opening the map only re-reads the newest logs.
try{$mode=[ModePulse.Quests]::CurrentMode($questLogs);[ModePulse.Quests]::RefreshIfStale($store,$mode,[TimeSpan]::FromHours(24))}catch{}
# Exit names read from the in-game exit list (F4), per map, offered as names in the pin editor.
$seenPath=Join-Path $store 'seen-exits.json';$seenExits=@{}
if(Test-Path $seenPath){try{$obj=Get-Content $seenPath -Raw -Encoding UTF8|ConvertFrom-Json;foreach($p in $obj.PSObject.Properties){$seenExits[$p.Name]=@($p.Value|ForEach-Object {[string]$_})}}catch{}}
$extracts=$null;try{$extracts=Get-Content (Join-Path $PSScriptRoot '../maps/extracts.json') -Raw -Encoding UTF8|ConvertFrom-Json}catch{}
function Save-Pins{ConvertTo-Json -InputObject $pins -Depth 6|Set-Content $pinsPath -Encoding UTF8}
function Pins-For($key){if($pins.ContainsKey($key)){return ,@($pins[$key])};return ,@()}
# Name suggestions: names seen in the in-game list first, then tarkov.dev's exits for this side and transits (with conditions).
function Set-Suggestions($id,$role){
 $names=New-Object 'System.Collections.Generic.List[string]';$conds=New-Object 'System.Collections.Generic.List[string]'
 foreach($n in @($seenExits[$id+'_'+$role])){if($n){$names.Add($n);$conds.Add('')}}
 $m=$null;if($extracts){$m=$extracts.$id}
 if($m){$want=if($role -eq 'SCAV'){'scav'}else{'pmc'}
  foreach($e in @($m.extracts)){if($e.faction -in @($want,'shared')){$names.Add([string]$e.name);$conds.Add($(if($e.switch){(T '스위치 작동 필요')}else{''}))}}
  foreach($t in @($m.transits)){if($t.ko){$names.Add([string]$t.ko);$conds.Add([string]$t.conditions)}}}
 $map.SetSuggestions($names.ToArray(),$conds.ToArray())
}
function Remember-Exits($lines){
 if(!$script:mapId){return};$list=New-Object 'System.Collections.Generic.List[string]';foreach($n in @($seenExits[$script:mapId])){if($n){$list.Add($n)}}
 $changed=$false;foreach($l in $lines){$t=([string]$l).Trim();if($t.Length -ge 3 -and $t -match '[A-Za-z가-힣]{2}' -and !$list.Contains($t)){$list.Add($t);$changed=$true}}
 if($changed){while($list.Count -gt 60){$list.RemoveAt(0)};$seenExits[$script:mapId]=$list.ToArray();try{ConvertTo-Json -InputObject $seenExits -Depth 4|Set-Content $seenPath -Encoding UTF8}catch{}}
}
function Load-Map {
 $row=@($catalog|Where-Object {$_.displayName -eq [string]$map.Maps.SelectedItem -and $_.role -eq [string]$map.Role.SelectedItem})
 if($row.Count -ne 1){$map.Canvas.Visible=$false;$map.State.Text=(T '이 역할의 지도는 제공되지 않았습니다.');return}
 $key=$row[0].id+'_'+$row[0].role
 # The picture comes with the data pack (downloaded after BlueAsh starts): until it is there, say so and try again next time.
 $picture=Join-Path $PSScriptRoot ('../maps/'+$row[0].file);if(!(Test-Path -LiteralPath $picture)){$map.Canvas.Visible=$false;$map.State.Text=(T '지도 그림을 받는 중입니다. 잠시 뒤 다시 열어 주세요.');$script:mapId='';return}
 if($key -ne $script:mapId){$script:mapId=$key;$script:seen=@();$script:lastFrame=@();$map.Canvas.Available.Clear();$map.Canvas.TaxiStates.Clear();$script:taxiLastFrame=@{};$map.Canvas.LoadMap((Join-Path $PSScriptRoot ('../maps/'+$row[0].file)))}
 $map.Canvas.Visible=$true;$map.Canvas.Pins.Clear();$map.Canvas.Selected=$null;$map.MapId=$row[0].id;$map.QuestLogs=$questLogs;$map.Store=$store;$map.MyKeys=(Join-Path $store 'my-keys.json')
 # The player's own exit edits replace the Fandom exits/transits only; spawns, bosses and quests stay live.
 $own=Pins-For $key
 foreach($p in $own){if($null -eq $p){continue};$pin=New-Object ModePulse.Pin;$pin.Name=[string]$p.Name;$pin.X=[single]$p.X;$pin.Y=[single]$p.Y;$pin.Taxi=[bool]$p.Taxi;$pin.Destination=[string]$p.Destination;$pin.Condition=[string]$p.Condition;$pin.Kind=$(if($p.Kind -eq 'transit'){'transit'}else{'exit'});$map.Canvas.Pins.Add($pin)}
 foreach($pin in [ModePulse.FandomMarkers]::Pins($row[0].id,$row[0].role,$markerCache,$markerShipped)){if(!$own.Count -or $pin.Kind -notin 'exit','transit'){$map.Canvas.Pins.Add($pin)}}
 # Bosses with this mode's spawn chance, map texts in the UI language; then my keys and the active quests.
 try{$mode=[ModePulse.Quests]::CurrentMode($questLogs);[ModePulse.Quests]::RefreshIfStale($store,$mode,[TimeSpan]::FromHours(24))}catch{}
 try{[ModePulse.Quests]::Decorate($map.Canvas.Pins,$row[0].id,$questLogs,$store)}catch{}
 # Top-left box: every boss this mode's game data lists on this map, with its chance (also ones the map has no marker for).
 $map.Canvas.Info='';try{$map.Canvas.Info=[ModePulse.Quests]::BossSummary($row[0].id,$questLogs,$store,$map.Canvas.Pins)}catch{}
 if($config.showKeys){try{foreach($pin in [ModePulse.Quests]::KeyPins($row[0].id,$questLogs,$store,$markerShipped,(Join-Path $store 'my-keys.json'))){$map.Canvas.Pins.Add($pin)}}catch{}}
 # The key list beside the map (add / remove there; its Changed event redraws the map).
 $map.KeyList.Visible=[bool]$config.showKeys
 if($config.showKeys){try{$map.KeyList.Load([ModePulse.Quests]::CachePath($store,$mode),(Join-Path $store 'my-keys.json'),$mode,$row[0].id)}catch{}}
 if($config.showQuests){try{foreach($pin in [ModePulse.Quests]::Pins($row[0].id,$questLogs,$store,$markerShipped)){$map.Canvas.Pins.Add($pin)}}catch{}}
 Set-Suggestions $row[0].id $row[0].role
 $exits=@($map.Canvas.Pins|Where-Object {$_.Kind -in 'exit','transit'}).Count
 $map.Canvas.Invalidate();$map.State.Text=T '탈출구 {0}곳 · 레이드 시작 시 자동 확인 · 확인 키로 재확인\n초록 비상구: 탈출구 · 두 사람: 협동 탈출구 · 주황 비상구: 트랜짓 · 미확인: 회색 · 신호탄 권총: 플레어 탈출' @($exits)
}
function Locate-Key($sender,$e){
 $choices=@($catalog|Where-Object {$_.id -eq $e.MapId});$target=@($choices|Where-Object {$_.role -eq [string]$map.Role.SelectedItem}|Select-Object -First 1)
 if(!$target.Count){$target=@($choices|Where-Object role -eq 'PMC'|Select-Object -First 1)};if(!$target.Count){return}
 $map.Role.SelectedItem=$target[0].role;$map.Maps.SelectedItem=$target[0].displayName
 if($script:mapId -ne ($target[0].id+'_'+$target[0].role)){Load-Map}
 if(!$map.Canvas.FocusKey([string]$e.Id)){$map.State.Text=T '이 지도의 열쇠 사용처 좌표를 찾지 못했습니다.'}
}
$map.KeyList.add_LocateRequested({param($sender,$e)Locate-Key $sender $e})
$map.KeyList.add_Changed({Load-Map});$map.Maps.add_SelectedIndexChanged({Load-Map});$map.Role.add_SelectedIndexChanged({Load-Map})
$map.ClearPins.add_Click({if([Windows.Forms.MessageBox]::Show((T '이 지도에서 직접 고친 탈출구를 지우고 기본(공유) 데이터로 되돌릴까요?'),'BlueAsh','YesNo') -eq 'Yes'){$pins.Remove($script:mapId);Save-Pins;$script:mapId='';Load-Map}})
function Reset-Raid {$script:exitGeneration++;$script:autoExits=$null;$map.Canvas.MeShown=$false;$script:seen=@();$script:lastFrame=@();$script:captures=0;$script:pendingAt=[DateTime]::MaxValue;$map.Canvas.Available.Clear();$map.Canvas.TaxiStates.Clear();$script:taxiLastFrame=@{};$map.Canvas.Invalidate()}
$resetBtn.add_Click({Reset-Raid;Status (T '이번 레이드 표시를 지웠습니다.')})
$mapBtn.add_Click({Load-Map;$map.ShowOver([ModeSwitchPrice.Desktop]::Game())})
$hideBtn.add_Click({$discordForm.Hide()});$closeMapBtn.add_Click({$mapForm.Hide()})
$saveMapBtn.add_Click({$before=[string]$config.mapKey;if(Save-Config){Status (T '저장했습니다.');if([string]$config.mapKey -ne $before){$config.mapKeyIgnore=''};Check-MapKeyConflicts}})
function Save-Config {
 $url=$hook.Text.Trim()
 if($url -and !(Valid-Webhook $url)){$m=(T '웹후크 주소가 올바르지 않습니다. 비워 두거나 discord.com 웹후크 주소를 넣어 주세요.');Status $m;DStatus $m;return $false}
 if($discordCheck.Checked -and !$script:phonePaired -and !$url){$m=$(if($relay){(T '자리 비움 알림을 켜려면 먼저 휴대폰 연결(또는 Discord 연결)을 해 주세요.')}else{(T '디스코드 알림을 켜려면 먼저 웹후크 주소를 넣어 주세요. (웹후크 만드는 법 참고)')});Status $m;DStatus $m;return $false}
 if($mapKey.SelectedIndex -lt 0){Status (T '지도 키를 골라 주세요.');return $false}
 $config.showQuests=$questCheck.Checked;$config.showKeys=$keyCheck.Checked;$config.shotPos=$shotCheck.Checked;$config.shotDelete=$shotDelete.Checked;[ModePulse.ScreenshotPosition]::Watch($config.shotPos)
 $config.discord=$discordCheck.Checked;$config.maintenance=$maintenanceCheck.Checked;$config.idle=[int]$idle.Value;$config.mapKey=[int][char][string]$mapKey.SelectedItem
 $config.webhook=Protect $url;$config.relayKey=Protect $script:relayKey
 $script:activeUrl=$url;$map.MapKey=[int]$config.mapKey
 $config|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8
 # Never New-Item -Force an existing registry key: it deletes every BlueAsh setting in it.
 if(!(Test-Path 'HKCU:\Software\ModeSwitch')){$null=New-Item 'HKCU:\Software\ModeSwitch'};Set-ItemProperty 'HKCU:\Software\ModeSwitch' 'MapScreenshotKey' $(if($shotOnMap.Checked -and $config.shotPos){[string][char][int]$config.mapKey}else{''});Set-ItemProperty 'HKCU:\Software\ModeSwitch' 'CompanionEnabled' ([string][int]($config.maps -or $config.discord -or $config.maintenance))
 $script:ocrBlocked=$false;Update-Link;return $true
}
$saveBtn.add_Click({if(Save-Config){DStatus (T '저장했습니다.')}})
. (Join-Path $PSScriptRoot 'DiscordTransport.ps1')
$script:nextParentCheck=[DateTime]::MinValue
function Pump {[Windows.Forms.Application]::DoEvents();Network-Pump;if([DateTime]::UtcNow -ge $script:nextParentCheck){$script:nextParentCheck=[DateTime]::UtcNow.AddSeconds(1);if(!(Get-Process -Id $ParentId -ErrorAction SilentlyContinue)){throw 'parent-exit'}}}
function Init-Ocr {
 if($script:ocrReady){return}
 $null=[Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime];$null=[Windows.Globalization.Language,Windows.Globalization,ContentType=WindowsRuntime]
 $null=[Windows.Storage.Streams.InMemoryRandomAccessStream,Windows.Storage.Streams,ContentType=WindowsRuntime];$null=[Windows.Storage.Streams.DataWriter,Windows.Storage.Streams,ContentType=WindowsRuntime]
 $null=[Windows.Graphics.Imaging.BitmapDecoder,Windows.Graphics.Imaging,ContentType=WindowsRuntime];$null=[Windows.Graphics.Imaging.SoftwareBitmap,Windows.Graphics.Imaging,ContentType=WindowsRuntime];$null=[Windows.Media.Ocr.OcrResult,Windows.Foundation,ContentType=WindowsRuntime]
 # Exit names are English in the game's list: English OCR reads them best, then Korean, then any installed language.
 foreach($tag in 'en','ko'){if($script:engine){break};foreach($l in [Windows.Media.Ocr.OcrEngine]::AvailableRecognizerLanguages){if($l.LanguageTag -eq $tag -or $l.LanguageTag.StartsWith($tag+'-')){$script:engine=[Windows.Media.Ocr.OcrEngine]::TryCreateFromLanguage($l);break}}}
 if(!$script:engine){$script:engine=[Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()}
 if(!$script:engine){throw (T 'Windows 한국어 OCR 언어가 필요합니다.')}
 $script:asTask=[System.WindowsRuntimeSystemExtensions].GetMethods()|Where-Object {$_.Name -eq 'AsTask' -and $_.IsGenericMethod -and $_.GetParameters().Count -eq 1 -and $_.GetGenericArguments().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1'}|Select-Object -First 1
 $script:ocrReady=$true
}
function Apply-ExitLines($lines,$taxiEvidence=@{}){
  Remember-Exits $lines;$hits=@(Match-Exits $lines @($map.Canvas.Pins|Where-Object {$_.Kind -in 'exit','transit'}))
  # The exit list also tells the side: when PMC/Scav was not detected and the other side's exits match clearly
  # better (transits count for both), switch to it; Load-Map then shows that side's exits.
  if($script:mapId){$id=$script:mapId.Substring(0,$script:mapId.LastIndexOf('_'));$cur=[string]$map.Role.SelectedItem;$other=$(if($cur -eq 'SCAV'){'PMC'}else{'SCAV'})
   if(@($catalog|Where-Object {$_.id -eq $id -and $_.role -eq $other}).Count){
    $alt=@(Match-Exits $lines @([ModePulse.FandomMarkers]::Pins($id,$other,$markerCache,$markerShipped)|Where-Object {$_.Kind -eq 'exit'}))
    $now1=@(Match-Exits $lines @($map.Canvas.Pins|Where-Object {$_.Kind -eq 'exit'}))
    if($alt.Count -ge 2 -and $alt.Count -gt $now1.Count+1){$map.Role.SelectedItem=$other;$hits=@(Match-Exits $lines @($map.Canvas.Pins|Where-Object {$_.Kind -in 'exit','transit'}));$script:lastFrame=$hits}}}
  # Shown once a name is read in two frames of the same check (any two of them: a misread frame in between no longer
  # resets it); a new check (exit key, raid start) starts over.
  foreach($name in $hits){if($script:lastFrame -contains $name){[void]$map.Canvas.Available.Add($name)}};$script:lastFrame=@(@($script:lastFrame)+$hits|Select-Object -Unique);Update-TaxiStates $taxiEvidence;$map.Canvas.Invalidate()
  $map.State.Text=(T '확인된 탈출구: ')+(@($map.Canvas.Available) -join ', ')
  Status ((T '탈출구 OCR 읽음: ')+($lines -join ' / ')+(T "`r`n등록 좌표와 일치한 이름만 2회 확인 후 표시합니다."))
}
. (Join-Path $PSScriptRoot 'ExitCapture.ps1')
. (Join-Path $PSScriptRoot 'MapInput.ps1')
$nextStateCheck=[DateTime]::MinValue;$stateStamp='';$lastRaid='';$lastSerial='';$pendingNotice=$null;$announced=New-Object 'System.Collections.Generic.HashSet[string]';$sentFile=Join-Path $store 'discord-raids.json';if(Test-Path $sentFile){try{foreach($id in @(Get-Content $sentFile -Raw|ConvertFrom-Json|ForEach-Object {$_})){[void]$announced.Add([string]$id)}}catch{}};$keys=@{};$lastO=[DateTime]::MinValue;$lastM=[DateTime]::MinValue
$noticeTask=$null;$noticeNext=[DateTime]::MinValue;$noticeInitialized=$false;$noticeSeen=New-Object 'System.Collections.Generic.HashSet[string]'
function Apply-Theme($parent){[ModePulse.Theme]::Apply($parent)}
Apply-Theme $mapForm;Apply-Theme $discordForm;Apply-Theme $map
# Same look as BlueAsh pages: graphite surfaces, bold headings, muted hints, cobalt primary actions.
foreach($f in $mapForm,$discordForm){$f.BackColor=[ModePulse.Theme]::Panel}
foreach($c in $script:sections){$c.Font=$sectionFont};foreach($c in $script:hints){$c.Font=$hintFont;$c.ForeColor=[ModePulse.Theme]::Muted}
foreach($b in $script:primary){$b.BackColor=[ModePulse.Theme]::Accent;$b.FlatAppearance.BorderColor=[ModePulse.Theme]::Accent}
# Rows retain their original gaps. Longer translations wrap and scroll at the native page size.
function Fit-Page($f){
 $actions=@($f.Controls|Where-Object {$_.Top -ge 540});$footer=New-Object Windows.Forms.Panel;$footer.Dock='Bottom';$footer.Height=56;$footer.Width=$f.ClientSize.Width;$footer.BackColor=[ModePulse.Theme]::Panel
 foreach($c in $actions){$footer.Controls.Add($c);$c.Top=6;if($c.Left -ge 400){$c.Anchor='Top, Right'}}
 $body=New-Object Windows.Forms.Panel;$body.Dock='Fill';$body.Width=$f.ClientSize.Width;$body.BackColor=[ModePulse.Theme]::Panel;$body.AutoScroll=$true
 foreach($c in @($f.Controls)){$body.Controls.Add($c);if($c.Right -ge 660){$c.Width=[Math]::Max(40,$f.ClientSize.Width-16-$c.Left);$c.Anchor='Top, Left, Right'}}
 $f.Controls.Add($body);$f.Controls.Add($footer);$body.BringToFront()
 $skip=@($maintenanceCheck,$linkLabel,$connectBtn);if(!$webhookRoute){$skip+=@($helpBtn,$hookLabel,$hook)};if(!$relay){$skip+=@($phoneBtn)}
 $rows=@();foreach($c in @($body.Controls|Where-Object {$skip -notcontains $_}|Sort-Object Top)){
  if(!$rows.Count -or $c.Top-$rows[-1].Top -gt 8){$rows+=@{Top=$c.Top;Controls=@($c)}}else{$rows[-1].Controls+=@($c)}
 }
 $shift=0;$end=0;$originalEnd=0
 foreach($row in $rows){$oldEnd=($row.Controls|ForEach-Object Bottom|Measure-Object -Maximum).Maximum
  if($end){$shift=[Math]::Max($shift,$end+[Math]::Max(4,$row.Top-$originalEnd)-$row.Top)}
  foreach($c in $row.Controls){
   if($c -is [Windows.Forms.Label] -and $c.Text){$h=[Windows.Forms.TextRenderer]::MeasureText($c.Text,$c.Font,(New-Object Drawing.Size($c.Width,0)),[Windows.Forms.TextFormatFlags]::WordBreak).Height+4;$c.Height=[Math]::Max($c.Height,$h)}
   $c.Top+=$shift
  }
  $end=($row.Controls|ForEach-Object Bottom|Measure-Object -Maximum).Maximum;$originalEnd=$oldEnd
 }
 $body.AutoScrollMinSize=New-Object Drawing.Size(0,($end+10))
}
# The notifications page uses two measured blocks instead of a status box that fills the unused height.
# Control variables and event handlers stay the same, including the relay-free webhook route.
function Fit-Notifications {
 $discordForm.BackColor=[ModePulse.Theme]::Panel
 $script:discordBody=New-Object Windows.Forms.Panel;$script:discordBody.Dock='Fill';$script:discordBody.BackColor=[ModePulse.Theme]::Panel;$script:discordBody.AutoScroll=$true
 $script:connectionCard=New-Object ModePulse.ThemeCard;$script:connectionCard.BackColor=[ModePulse.Theme]::Panel
 $script:alertCard=New-Object ModePulse.ThemeCard;$script:alertCard.BackColor=[ModePulse.Theme]::Panel
 $script:discordActions=New-Object Windows.Forms.Panel;$script:discordActions.BackColor=[ModePulse.Theme]::Panel
 foreach($c in $phoneTitle,$phoneLabel,$phoneBtn,$phoneHelp,$linkLabel,$connectBtn,$helpBtn,$hookLabel,$hook,$discordStatus){$script:connectionCard.Controls.Add($c)}
 foreach($c in $alertTitle,$discordCheck,$maintenanceCheck,$idleLabel,$idleBox,$idleHint,$serverHint){$script:alertCard.Controls.Add($c)}
 foreach($c in $testBtn,$hideBtn,$saveBtn){$script:discordActions.Controls.Add($c);$c.Top=0}
 foreach($c in $script:connectionCard,$script:alertCard,$script:discordActions){$script:discordBody.Controls.Add($c)}
 $discordForm.Controls.Add($script:discordBody)
 $discordForm.add_Resize({Layout-Notifications});$script:discordBody.add_SizeChanged({Layout-Notifications});$phoneLabel.add_TextChanged({Layout-Notifications})
 Layout-Notifications
}
function Text-Height($c,[int]$width,[int]$minimum=24){
 $flags=[Windows.Forms.TextFormatFlags]::WordBreak -bor [Windows.Forms.TextFormatFlags]::NoPrefix
 return [Math]::Max($minimum,[Windows.Forms.TextRenderer]::MeasureText($c.Text,$c.Font,(New-Object Drawing.Size($width,0)),$flags).Height+4)
}
function Layout-Notifications {
 if($script:notificationLayout -or !$script:discordBody){return};$script:notificationLayout=$true
 try{
  $body=$script:discordBody;$body.SuspendLayout();$w=[Math]::Max(420,$body.ClientSize.Width-16);$inner=$w-32;$y=16
  $phoneTitle.SetBounds(16,$y,$inner,28);$y+=36
  $buttonWidth=[Math]::Min(224,[Math]::Max(210,[Windows.Forms.TextRenderer]::MeasureText($phoneBtn.Text,$phoneBtn.Font).Width+24))
  $labelWidth=$(if($relay){$inner-$buttonWidth-16}else{$inner});$rowHeight=Text-Height $phoneLabel $labelWidth 40
  $phoneLabel.SetBounds(16,$y,$labelWidth,$rowHeight);$phoneLabel.TextAlign='MiddleLeft';$phoneBtn.SetBounds(($w-16-$buttonWidth),$y,$buttonWidth,40);$y+=$rowHeight+12
  $hintHeight=Text-Height $phoneHelp $inner 42;$phoneHelp.SetBounds(16,$y,$inner,$hintHeight);$y+=$hintHeight+12
  if($webhookRoute){
   $labelWidth=[Math]::Max(130,[Windows.Forms.TextRenderer]::MeasureText($hookLabel.Text,$hookLabel.Font).Width+8)
   $hookLabel.SetBounds(16,($y+4),$labelWidth,30);$hook.SetBounds((16+$labelWidth+12),$y,($inner-$labelWidth-12),30);$y+=42
   $helpBtn.SetBounds(($w-16-224),$y,224,40);$y+=52
  }
  $discordStatus.SetBounds(16,$y,$inner,64);$y+=80;$script:connectionCard.SetBounds(8,12,$w,$y)
  $y=16;$alertTitle.SetBounds(16,$y,$inner,28);$y+=36
  $checkHeight=Text-Height $discordCheck ($inner-24) 30;$discordCheck.SetBounds(16,$y,$inner,$checkHeight);$y+=$checkHeight+14
  $labelWidth=[Math]::Max(130,[Windows.Forms.TextRenderer]::MeasureText($idleLabel.Text,$idleLabel.Font).Width+8)
  $hintWidth=$inner-$labelWidth-90-32;$rowHeight=Text-Height $idleHint $hintWidth 30
  $idleLabel.SetBounds(16,($y+4),$labelWidth,$rowHeight);$idleBox.Location=New-Object Drawing.Point((16+$labelWidth+12),($y-1));$idleHint.SetBounds((16+$labelWidth+160),($y+3),($hintWidth-42),$rowHeight);$y+=$rowHeight+14
  $hintHeight=Text-Height $serverHint $inner 28;$serverHint.SetBounds(16,$y,$inner,$hintHeight);$y+=$hintHeight+16
  $script:alertCard.SetBounds(8,($script:connectionCard.Bottom+12),$w,$y)
  $script:discordActions.SetBounds(8,($script:alertCard.Bottom+16),$w,40)
  $testWidth=[Math]::Min(280,[Math]::Max(240,[Windows.Forms.TextRenderer]::MeasureText($testBtn.Text,$testBtn.Font).Width+24))
  $testBtn.SetBounds(0,0,$testWidth,40);$saveBtn.SetBounds(($w-140),0,140,40);$hideBtn.SetBounds(($w-272),0,120,40)
  $body.AutoScrollMinSize=New-Object Drawing.Size(0,($script:discordActions.Bottom+12))
 }finally{$body.ResumeLayout($true);$script:notificationLayout=$false}
}
Fit-Page $mapForm;Fit-Notifications
$mapForm.ResumeLayout($true);$discordForm.ResumeLayout($true)
# New in-raid screenshots: their names carry position and facing. Shown on the current map when it has a
# game->image transform (maps/markers.json); used screenshots are deleted only when the player chose so.
$script:shotDeletes=New-Object System.Collections.ArrayList;$script:nextShotWatch=[DateTime]::MinValue;$script:mapKeyAt=[DateTime]::MinValue;$script:raidPhase=''
function Read-Screenshots {
 $now=[DateTime]::UtcNow
 $watch=$config.shotPos -or $shotOnMap.Checked
 if($watch -and !([ModePulse.ScreenshotPosition]::Watching) -and $now -ge $script:nextShotWatch){$script:nextShotWatch=$now.AddSeconds(10);[ModePulse.ScreenshotPosition]::Watch($true)}
 while($file=[ModePulse.ScreenshotPosition]::Next()){
  # The map key is also the game's screenshot key ("지도 오픈 시 위치 갱신"), so the game saves one on every press,
  # menus and hideout included. Outside a raid such a screenshot is not wanted: remove it (PrintScreen ones stay).
  if(($now-$script:mapKeyAt).TotalSeconds -lt 3 -and $script:raidPhase -ne 'started'){[void]$script:shotDeletes.Add(@{path=$file;at=$now.AddSeconds(2);until=$now.AddSeconds(20)});continue}
  $pos=[ModePulse.ScreenshotPosition]::Parse($file);if(!$pos -or !$config.shotPos -or !$script:mapId){continue}
  $id=$script:mapId.Substring(0,$script:mapId.LastIndexOf('_'));$t=[ModePulse.FandomMarkers]::Transform($id,$markerShipped)
  if(!$t -or !$map.Canvas.MapImage){Status (T '이 지도는 스크린샷 위치 표시를 지원하지 않습니다 (탈출구 좌표가 부족한 지도).');continue}
  $c=$map.Canvas;$img=[ModePulse.FandomMarkers]::ToImage($id,$markerShipped,$pos[0],$pos[1],$pos[2]);$c.MeX=[single]$img[0];$c.MeY=[single]$img[1]
  $c.MeDirX=[single](($t[0]*$pos[3]+$t[1]*$pos[4])*$c.MapImage.Width);$c.MeDirY=[single](($t[3]*$pos[3]+$t[4]*$pos[4])*$c.MapImage.Height);$c.MeShown=$true;$c.Invalidate()
  Status ((T '내 위치 표시 · ')+(Get-Date -Format 'HH:mm:ss')+(T ' 스크린샷 기준 · 지도 키로 확인하세요.'))
  if($config.shotDelete){[void]$script:shotDeletes.Add(@{path=$file;at=$now.AddSeconds(2);until=$now.AddSeconds(20)})}
 }
 # The game may still be writing the PNG: delete a little later, retrying briefly.
 foreach($d in @($script:shotDeletes)){if($now -lt $d.at){continue};try{Remove-Item -LiteralPath $d.path -ErrorAction Stop;$script:shotDeletes.Remove($d)}catch{if(!(Test-Path -LiteralPath $d.path) -or $now -gt $d.until){$script:shotDeletes.Remove($d)}else{$d.at=$now.AddSeconds(1)}}}
}
Load-Map;if(Test-Path $pagePath){try{$Page=[IO.File]::ReadAllText($pagePath).Trim();Remove-Item $pagePath}catch{}};if($Page){Open-Page $Page}elseif(!$Hidden){$mapForm.Show()};Status (T '대기 중 · 레이드에서 탈출구 확인 키를 누르면 쓸 수 있는 탈출구가 지도에 초록으로 표시됩니다.');DStatus $(if($config.discord){(T '대기 중 · 자리를 비운 사이 레이드가 시작되면 알려 드립니다.')}else{(T '알림이 꺼져 있습니다. 받을 상황을 고르고 저장하세요.')})
$script:mapsOn=$true;$script:mapsCheck=[DateTime]::MinValue
$script:fleaPending=New-Object System.Collections.ArrayList;$script:fleaSince=$null;$script:fleaSentAt=[DateTime]::MinValue;$script:fleaNext=[DateTime]::MinValue
# Item names for sale alerts: the price helper's catalogue for the current mode (price-<mode>.tsv: id TAB name ...).
function Flea-Names {$mode=[ModePulse.Quests]::CurrentMode($questLogs);$f=Join-Path $store ('price-'+$mode+'.tsv');$h=@{};if(Test-Path $f){foreach($l in [IO.File]::ReadLines($f)){$c=$l.Split([char]9);if($c.Count -ge 2 -and $c[0].Length -eq 24){$h[$c[0]]=$c[1]}}};return $h}
# The quest card's browser (WebView2) starts hidden 15 s after the helper (it starts with the game), so the first card opens at once.
$script:warmAt=[DateTime]::UtcNow.AddSeconds(15)
try{while($true){try{
 Pump
 Read-Screenshots
 # Menu entries in BlueAsh ask for a window by writing companion-page.txt (map / discord / hideout).
 if(Test-Path $pagePath){try{$req=[IO.File]::ReadAllText($pagePath).Trim();Remove-Item $pagePath;Open-Page $req}catch{}}
 # Sleep in C# until the map key / exit-check key changes, the next exit capture is due, or 250 ms pass.
 # The map switch lives in BlueAsh (게임별 기능 > 타르코프, registry MapEnabled), read every 2 s.
 if([DateTime]::UtcNow -ge $script:mapsCheck){$script:mapsCheck=[DateTime]::UtcNow.AddSeconds(2);$script:mapsOn=[string](Get-ItemProperty 'HKCU:\Software\ModeSwitch' -Name MapEnabled -ErrorAction SilentlyContinue).MapEnabled -ne '0';if(!$script:mapsOn -and $map.Visible){$map.HideMap()}}
 $codes=New-Object 'System.Collections.Generic.List[int]';if($script:mapsOn){$codes.Add([int]$config.mapKey);if($binding){foreach($v in $binding.Variants){foreach($vk in $v.Codes){$codes.Add([int]$vk)}}}}
 $waitMs=250;if($script:captures -gt 0 -and !$script:exitRead){$waitMs=[Math]::Max(15,[Math]::Min(250,($script:pendingAt-[DateTime]::UtcNow).TotalMilliseconds))};if($script:exitRead -or $script:autoExits -or $script:mapRead){$waitMs=[Math]::Min($waitMs,50)};if($script:claimState -or $script:sendTask){$waitMs=[Math]::Min($waitMs,100)}
 [ModePulse.Input]::WaitKeys($codes.ToArray(),[int]$waitMs)
 $now=[DateTime]::UtcNow;$game=[ModeSwitchPrice.Desktop]::Game()
 # Pin edits in the map editor: save this map's full list as the user's own version.
 if($map.Canvas.Dirty){$map.Canvas.Dirty=$false;if($script:mapId){$pins[$script:mapId]=@($map.Canvas.Pins|Where-Object {$_.Kind -in 'exit','transit'}|ForEach-Object {[ordered]@{Name=$_.Name;X=$_.X;Y=$_.Y;Taxi=$_.Taxi;Destination=$_.Destination;Condition=$_.Condition;Kind=$_.Kind}});Save-Pins}}
 if($now -ge $nextBindingCheck){
  $nextBindingCheck=$now.AddSeconds(5)
  try{
   $path=Find-EftControlFile;$file=Get-Item -LiteralPath $path
   if($file.Length -gt 2097152){throw (T 'Control.ini 크기 제한 초과')}
   $stamp=$path+':'+$file.LastWriteTimeUtc.Ticks+':'+$file.Length
   if($stamp -ne $bindingStamp -or !$binding){
    $binding=Read-EftExitBinding ([IO.File]::ReadAllText($path));$bindingStamp=$stamp;$triggerStates=@{};$script:captures=0
    $bindingLabel.Text=$binding.Label;$bindingDetails.Text=(T '게임 키 설정(Control.ini)에서 자동으로 읽습니다. 게임에서 키를 바꾸면 따라 바뀝니다.')
   }
  }catch{$binding=$null;$triggerStates=@{};$script:captures=0;$bindingLabel.Text=(T '읽지 못함');$bindingDetails.Text=(T '게임 키 설정을 읽지 못했습니다: ')+$_.Exception.Message}
 }
 if($script:mapsOn){
  # Presses come from a background key watcher: a tap while this loop reads the exit list is not lost.
  $k=[int]$config.mapKey;[ModePulse.Input]::Watch(@($k));$edge=[ModePulse.Input]::TakePress($k);if($edge){$script:mapKeyAt=$now}
  $conflict=$false;if($binding){foreach($v in $binding.Variants){if($v.Codes -contains $k){$conflict=$true}}}
  if($edge -and !$conflict){if($map.Visible){if(!$map.Typing){$map.HideMap()}}elseif($game -ne [IntPtr]::Zero){Request-MapOpen $game}}
  if($game -eq [IntPtr]::Zero){$triggerStates=@{}}elseif($binding){
   $triggered=$false
   for($i=0;$i -lt $binding.Variants.Count;$i++){
    $v=$binding.Variants[$i];$held=$true;foreach($vk in $v.Codes){if([ModePulse.Input]::GetAsyncKeyState([int]$vk) -ge 0){$held=$false}}
    if(!$triggerStates.ContainsKey($i)){$triggerStates[$i]=@{}}
    if(Update-EftTrigger $triggerStates[$i] $held ($now.Ticks/10000.0) $binding){$triggered=$true}
   }
   # The exit list fades in after the key: read 0.6 s later, then 4 more times 0.4 s apart (an exit shows after two matching reads).
   if($triggered){$script:pendingAt=$now.AddMilliseconds(600);$script:captures=5;$script:lastFrame=@();$script:taxiLastFrame=@{}}
  }
  $tick=[long][ModePulse.Input]::GetTickCount64()
  if($script:autoExits -and $tick -ge $script:autoExits.until){$script:autoExits=$null}
  $autoDue=$script:autoExits -and $tick -ge $script:autoExits.next
  $manualDue=$script:captures -gt 0 -and $now -ge $script:pendingAt
  if(($autoDue -or $manualDue) -and !$script:exitRead -and $game -ne [IntPtr]::Zero -and !$map.Visible -and !$script:ocrBlocked){
   if($manualDue){$script:captures--;$script:pendingAt=$now.AddMilliseconds(400)}
   if($autoDue){$script:autoExits.next=$tick+400}
   try{Read-Exits $game}catch{Status $_.Exception.Message;$script:captures=0;$script:autoExits=$null}
  }
 }else{[ModePulse.Input]::Watch([int[]]@())}
 if($script:warmAt -and [DateTime]::UtcNow -ge $script:warmAt){$script:warmAt=$null;if($script:mapsOn){try{$map.Warm()}catch{}}}
 # Flea market sales from the game's log (every 3 s): sent to the phone together, at most once a minute (the relay's limit).
 if($now -ge $script:fleaNext){$script:fleaNext=$now.AddSeconds(3);foreach($sale in [ModePulse.FleaSales]::Poll($questLogs)){[void]$script:fleaPending.Add($sale);if(!$script:fleaSince){$script:fleaSince=$now}}
  if(!$script:phonePaired){$script:fleaPending.Clear();$script:fleaSince=$null}
  if($script:fleaPending.Count -and ($now-$script:fleaSince).TotalSeconds -ge 10 -and $now -ge $script:fleaSentAt.AddSeconds(61)){$fl=@($script:fleaPending);$names=Flea-Names
   $first=$fl[0];$name=$(if($names.ContainsKey($first.Item)){$names[$first.Item]}else{'?'});$fmt={param($n,$c)if($c -eq '₽'){('{0:N0}' -f $n)+'₽'}else{$c+('{0:N0}' -f $n)}}
   if($fl.Count -eq 1){$sent=Phone-Send 'tarkov.flea.sold' @($name,$first.Count,(& $fmt $first.Amount $first.Currency))}
   else{$tot=@($fl|Group-Object Currency|ForEach-Object {& $fmt ($_.Group|Measure-Object Amount -Sum).Sum $_.Name}) -join ' + ';$sent=Phone-Send 'tarkov.flea.sold.many' @($name,$first.Count,(& $fmt $first.Amount $first.Currency),($fl.Count-1),$tot)}
   if($sent){$script:fleaPending.Clear();$script:fleaSince=$null;$script:fleaSentAt=$now}}}
 Poll-ExitRead
 Poll-MapRead $game
 # A key added or removed from an inspect window (price helper, my-keys.json): redraw the map and its key list.
 if($now -ge $script:nextKeysCheck){$script:nextKeysCheck=$now.AddSeconds(1);$kf=Join-Path $store 'my-keys.json';$ks=0;if(Test-Path $kf){$ks=(Get-Item $kf).LastWriteTimeUtc.Ticks}
  $kd=[ModePulse.Quests]::CachePath($store,[ModePulse.Quests]::CurrentMode($questLogs));$ks=[string]$ks+':'+$(if(Test-Path $kd){(Get-Item $kd).LastWriteTimeUtc.Ticks}else{0})
  if($null -eq $script:keysStamp){$script:keysStamp=$ks}elseif($ks -ne $script:keysStamp){$script:keysStamp=$ks;try{Load-Map}catch{}}}
 $stateFile=Join-Path $store 'raid-state.txt'
 if($now -ge $nextStateCheck -and (Test-Path $stateFile)){$nextStateCheck=$now.AddMilliseconds(100);$stamp=(Get-Item $stateFile).LastWriteTimeUtc.Ticks;if($stamp -ne $stateStamp){$stateStamp=$stamp;try{
  $state=@(Get-Content $stateFile -Encoding UTF8);if($state.Count -ge 7 -and $state[0] -eq [string]$ParentId){
   $serial=$state[0]+':'+$state[5];$raid=$state[3];$phase=$state[4];$script:raidPhase=$phase
   if($serial -ne $lastSerial -or ($raid -and $raid -ne $lastRaid) -or $phase -eq 'ended'){Reset-Raid;$pendingNotice=$null;$lastSerial=$serial;$lastRaid=$raid}
   $mapName=$state[1];if($mapName -eq 'factory4_night'){$mapName='factory4_day'};if($mapName -eq 'sandbox_high'){$mapName='sandbox'}
   $found=@($catalog|Where-Object id -eq $mapName|Select-Object -First 1);if($found.Count){$map.Maps.SelectedItem=$found[0].displayName}
   if($state[2] -in @('PMC','SCAV')){$map.Role.SelectedItem=$state[2]}else{$map.State.Text=(T '역할 미확정 · PMC/SCAV 직접 선택 필요')}
   $schedule=New-RaidExitSchedule $state ([long][ModePulse.Input]::GetTickCount64()) $script:autoExitKey
   if($schedule){$script:autoExitKey=$schedule.key;if($script:mapsOn){$script:autoExits=$schedule;$script:lastFrame=@()}}
   if($config.discord -and $phase -eq 'started' -and $raid -and !$announced.Contains($raid)){
    [void]$announced.Add($raid);ConvertTo-Json -InputObject @($announced|Select-Object -Last 200)|Set-Content $sentFile -Encoding UTF8
    if($config.discord -and [ModePulse.Input]::Idle() -ge ([int]$config.idle*1000)){$pendingNotice=@{id=$raid;due=$now.AddSeconds(5);idle=[ModePulse.Input]::Idle()}}
   }
  }
 }catch{Status (T '레이드 상태 읽기 대기')}}}
 if($pendingNotice){if([ModePulse.Input]::Idle() -lt $pendingNotice.idle){$pendingNotice=$null}elseif($now -ge $pendingNotice.due){if($config.discord){[void](Phone-Send 'raid.started.away');if($webhookRoute){Enqueue 'tarkov' 'raid.started.away' $pendingNotice.id (T '타르코프 레이드가 시작되었습니다. PC 무입력 상태가 감지되었습니다.')}};$pendingNotice=$null}}
 if($webhookRoute -and $config.maintenance -and !$noticeTask -and $now -ge $noticeNext){$noticeNext=$now.AddMinutes(5);$noticeTask=$http.GetAsync('https://t.me/s/escapefromtarkovEN')}
 if($noticeTask -and $noticeTask.IsCompleted){try{$r=$noticeTask.GetAwaiter().GetResult();try{
  if(!$r.IsSuccessStatusCode){throw (T '공식 공지 조회 실패 · 점검 상태를 추정하지 않습니다.')}
  $html=$r.Content.ReadAsStringAsync().GetAwaiter().GetResult();$matches=[regex]::Matches($html,'(?s)data-post="escapefromtarkovEN/(\d+)".*?<div class="tgme_widget_message_text[^\"]*"[^>]*>(.*?)</div>')
  if($matches.Count -eq 0){throw (T '공식 공지 형식 미확인 · 알림 보류')}
  foreach($m in $matches){$id=$m.Groups[1].Value;$text=[Net.WebUtility]::HtmlDecode([regex]::Replace($m.Groups[2].Value,'<[^>]+>',' '));$new=$noticeSeen.Add($id);$kind=Classify-Notice $text
   if($new -and $noticeInitialized -and $kind -and $config.maintenance){$label=@{'maintenance.started'=(T '점검 시작');'maintenance.extended'=(T '점검 연장');'maintenance.completed'=(T '점검 완료')}[$kind];Enqueue 'tarkov' $kind $id ((T '타르코프 공식 공지 · ')+$label+"`n"+$text.Substring(0,[Math]::Min(1200,$text.Length))+"`nhttps://t.me/escapefromtarkovEN/"+$id)}
  };$noticeInitialized=$true
 }finally{$r.Dispose()}}catch{DStatus $_.Exception.Message};$noticeTask=$null}
# One bad iteration (game focus change, OCR error...) must not kill the helper: report it and keep going.
}catch{if($_.Exception.Message -eq 'parent-exit'){throw};$m=(T '도우미 오류 (계속 실행 중): ')+$_.Exception.Message;Status $m;DStatus $m}}}catch{if($_.Exception.Message -ne 'parent-exit'){[void][Windows.Forms.MessageBox]::Show($_.Exception.Message,(T 'BlueAsh 도우미 오류'))}}finally{if($script:mapRead -and $script:mapRead.task.IsCompleted){Dispose-MapRead $script:mapRead};if($script:exitRead -and $script:exitRead.task.IsCompleted){Dispose-ExitRead $script:exitRead};$map.Dispose();$mapForm.Dispose();$discordForm.Dispose();$http.Dispose();$handler.Dispose()}
