$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot '../price'
Add-Type -Path @((Join-Path $root 'PriceCore.cs'),(Join-Path $root 'PriceWindow.cs'),(Join-Path $root 'PriceCatalog.cs'),(Join-Path $root 'PriceLanguage.cs')) -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.Net.Http,System.Web.Extensions
[ModeSwitchPrice.Language]::Code='ko'
. (Join-Path $root 'PriceCore.ps1')
function Assert($condition,$message){if(!$condition){throw $message}}
foreach($f in Get-ChildItem $root -Filter '*.ps1'){
 $t=$null;$errors=$null;$null=[System.Management.Automation.Language.Parser]::ParseFile($f.FullName,[ref]$t,[ref]$errors)
 Assert ($errors.Count -eq 0) ($errors | Out-String)
}
Assert ([ModeSwitchPrice.Core]::Mode('1.2.0 | B3FNNW | PvE') -eq 'pve') 'PvE mode'
Assert ([ModeSwitchPrice.Core]::Mode('1.2.0 | PvP S') -eq 'pvp-season') 'Season mode'
Assert ([ModeSwitchPrice.Core]::Mode('PvP') -eq '') 'Never substitute regular'
Assert ([ModeSwitchPrice.Core]::Mode('PvE PvP S') -eq '') 'Ambiguous mode'
$names=@('RB-ORB1 열쇠','RB-ORB2 열쇠','Oakley Mechanism heavy duty 배낭 (검은색)','35발, PPSh-41 7.62x25 탄창','HK G36 가늠쇠','Toothpaste 치약');$folded=[string[]]@($names|ForEach-Object {[ModeSwitchPrice.Core]::Fold($_)})
Assert ([ModeSwitchPrice.Core]::Closest('Oakley Mechanism heavy duty 배당 (검은색)',$folded) -eq 2) 'One Hangul misread'
Assert ([ModeSwitchPrice.Core]::Closest('35발, PPSh-41 7.62)(25 단상',$folded) -eq 3) 'x read as )( plus misreads'
Assert ([ModeSwitchPrice.Core]::Closest('HK 636 가늠쇠',$folded) -eq 4) 'G read as 6'
Assert ([ModeSwitchPrice.Core]::Closest('RB-ORBI 열쇠',$folded) -eq 0) 'I read for 1 resolves to the folded key'
Assert ([ModeSwitchPrice.Core]::Closest('RB-ORB 열쇠',$folded) -eq -1) 'Equally close keys are ambiguous'
Assert ([ModeSwitchPrice.Core]::Closest('상세 정보',$folded) -eq -1) 'Unrelated UI text'
Assert ([ModeSwitchPrice.Core]::Closest('',$folded) -eq -1) 'Empty OCR'
$parse={param($items,$locale,$mode)[ModeSwitchPrice.Catalog]::Parse($items,$locale,$mode)}
$caseRows=@(& $parse '{"data":{"items":{"5755383e24597772cb798966":{"id":"5755383e24597772cb798966","name":"n","types":["noFlea"],"lastLowPrice":null},"Front_plate":{},"front_plate":{}}}}' '{"data":{"n":"바셀린","Front_plate":"a","front_plate":"b"}}' 'regular')
Assert ($caseRows.Count -eq 1 -and $caseRows[0].Banned -and (Get-PriceState $caseRows[0] 'regular') -eq 'banned') 'Case-distinct JSON keys (ConvertFrom-Json rejects these), ban, regular mode'
$log="2026-10-06 23:38:34.522|1.2.0.0.47888|Info|application|Session mode: PvpSeason`n2026-10-06 23:42:28.688|1.2.0.0.47888|Info|application|Session mode: Pve`n"
Assert ((Get-LogMode $log) -eq 'pve') 'Latest log mode wins'
Assert ((Get-LogMode '2026|Info|application|Session mode: PvpSeason') -eq 'pvp-season') 'Season log mode'
Assert ((Get-LogMode '2026|Info|application|Session mode: Regular') -eq 'regular') 'Regular log mode has its own endpoint'
Assert ((Get-LogMode '2026|Info|application|Session mode: Arena') -eq '') 'Unknown log mode'
Assert ((Get-LogMode 'no marker') -eq '') 'No log mode'
$logDir=Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString('N'));$session=Join-Path $logDir 'log_2026.10.07_1'
try{[void][IO.Directory]::CreateDirectory($session);$path=Join-Path $session 'x application_000.log';[IO.File]::WriteAllText($path,$log)
 $held=New-Object IO.FileStream($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
 try{Assert ((Read-LogMode $logDir) -eq 'pve') 'Read log held open by the game'}finally{$held.Dispose()}
 Assert ((Read-LogMode (Join-Path $logDir 'missing')) -eq '') 'Missing log folder'
}finally{Remove-Item $logDir -Recurse -Force}
$rows=@(& $parse '{"data":{"items":{"5755383e24597772cb798966":{"id":"5755383e24597772cb798966","name":"item Name","types":["meds"],"lastLowPrice":12000,"lastOfferCount":5,"updated":"2026-10-06T00:00:00Z"},"5755383e24597772cb798967":{"id":"5755383e24597772cb798967","name":"dup","types":[],"lastLowPrice":1},"5755383e24597772cb798968":{"id":"5755383e24597772cb798968","name":"dup2","types":[],"lastLowPrice":2}}}}' '{"data":{"item Name":"Vaseline 바셀린 연고","dup":"같은 이름","dup2":"같은 이름"}}' 'pve')
Assert ($rows.Count -eq 3) 'Catalog translation'
$cache=Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($cache)
try{
 $cat=New-Object ModeSwitchPrice.Catalog('pve',$cache);Assert (!$cat.LoadCache()) 'No cache yet'
 $cat.Load([ModeSwitchPrice.Item[]]$rows,[DateTime]::UtcNow)
 Assert ($cat.Find('Vaseline 바셀린 연고').Id -eq '5755383e24597772cb798966') 'Exact title lookup'
 Assert ($cat.Find('VaseIine 바셀린 언고').Id -eq '5755383e24597772cb798966') 'Near title lookup'
 $dup=$cat.Find('같은 이름');Assert ($dup.Price -eq 1 -and $dup.PriceMax -eq 2) 'Items sharing a name merge into a price range'
 $same=[ModeSwitchPrice.Catalog]::Merge([ModeSwitchPrice.Item[]]@((New-Object ModeSwitchPrice.Item -Property @{Name='a';Price=100;Offers=3}),(New-Object ModeSwitchPrice.Item -Property @{Name='a';Price=110;Offers=2})))
 Assert ($same.Price -eq 100 -and $null -eq $same.PriceMax -and $same.Offers -eq 5) 'Close prices show the lowest, no range'
 $ban=[ModeSwitchPrice.Catalog]::Merge([ModeSwitchPrice.Item[]]@((New-Object ModeSwitchPrice.Item -Property @{Name='c';Banned=$true;Mode='pve'}),(New-Object ModeSwitchPrice.Item -Property @{Name='c';Banned=$true;Mode='pve'})))
 Assert ((Get-PriceState $ban 'pve') -eq 'banned') 'All banned stays banned'
 $cat.Load([ModeSwitchPrice.Item[]]@((New-Object ModeSwitchPrice.Item -Property @{Name='Twitch Winter 2025 case (Common) 아주 긴 이름의 케이스';Price=5;Mode='pve'})),[DateTime]::UtcNow)
 Assert ($cat.Find('Twitch Winter 2025 case (Common) 아주 긴...').Price -eq 5) 'Ellipsis-cut title matches its unique prefix'
 [IO.File]::WriteAllText((Join-Path $cache 'price-pve.tsv'),("v2`tpve`t"+[DateTime]::UtcNow.Ticks+"`n5755383e24597772cb798966`tVaseline 바셀린 연고`t0`t12000`t5`t2026`tmedicalsuppl1es`n"),(New-Object Text.UTF8Encoding($false)))
 $fresh=New-Object ModeSwitchPrice.Catalog('pve',$cache);Assert ($fresh.LoadCache() -and $fresh.Find('Vaseline 바셀린 연고').Price -eq 12000) 'Disk cache is usable immediately'
 Assert (!(New-Object ModeSwitchPrice.Catalog('pvp-season',$cache)).LoadCache()) 'Cache never crosses modes'
}finally{Remove-Item $cache -Recurse -Force}
$item=$rows[0]
Assert ((Get-PriceState $item 'pve') -eq 'price') 'Price'
Assert ((Get-PriceState $item 'pvp-season') -eq 'invalid') 'Cross-mode rejection'
$item.Offers=0
Assert ((Get-PriceState $item 'pve') -eq 'no-price') 'No offers'
$item.Banned=$true
Assert ((Get-PriceState $item 'pve') -eq 'banned') 'Ban takes precedence'
$failed=$false;try{& $parse '{}' '{}' 'pve'}catch{$failed=$true};Assert $failed 'Reject invalid API payload'
$state=Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString('N'))
try{[IO.File]::WriteAllText($state,"42`nbigmap`nPMC`nABC123`nstarted`n3`n99");Assert ((Read-RaidPhase $state 42) -eq 'started') 'Raid phase';Assert ((Read-RaidPhase $state 7) -eq 'unknown') 'Stale raid state from another process'}finally{Remove-Item $state}
# Title bar crossed by glyphs on the scan row: the whole bar is found, not just the part after the last glyph.
$fw=2048;$fh=1151;$px=New-Object byte[] ($fw*$fh*4);function Px($x,$y,$r,$g,$b){$i=($y*$fw+$x)*4;$px[$i]=$b;$px[$i+1]=$g;$px[$i+2]=$r;$px[$i+3]=255}
for($y=270;$y -lt 310;$y++){for($x=560;$x -lt 1440;$x++){if($x -lt 600){Px $x $y 90 90 90}elseif($x -lt 1405){Px $x $y 30 30 30}elseif($x -lt 1425){if($y -ge 280 -and $y -lt 296){Px $x $y 200 20 20}else{Px $x $y 30 30 30}}else{Px $x $y 90 90 90}}}
foreach($g0 in 640,700,760,830,900,980,1050,1120,1200,1290){for($y=278;$y -lt 296;$y++){for($x=$g0;$x -lt $g0+14;$x++){Px $x $y 230 230 230}}}
$tb=@([ModeSwitchPrice.Core]::TitleBars($px,$fw,$fh))
Assert ($tb.Count -eq 1 -and $tb[0].X -le 605 -and $tb[0].W -ge 790) ('Title bar across glyphs: '+(($tb|ForEach-Object {''+$_.X+','+$_.W}) -join ' '))
if(Test-Path '/tmp/eft-screen.bgra'){
 $bars=[ModeSwitchPrice.Core]::TitleBars([IO.File]::ReadAllBytes('/tmp/eft-screen.bgra'),2048,1151)
 $bars | Format-Table
 Assert (@($bars | Where-Object {$_.X -ge 590 -and $_.X -le 625 -and $_.Y -ge 275 -and $_.Y -le 300 -and $_.W -gt 780}).Count -eq 1) 'Supplied screenshot title bar detection'
}
$t=New-Object ModeSwitchPrice.ClickTrigger;$I=[ModeSwitchPrice.ClickTrigger]::Inspect;$C=[ModeSwitchPrice.ClickTrigger]::Click
Assert (($t.Left(1000,500,500)) -eq $C -and ($t.Left(1250,503,498)) -eq $I) 'Double-click opens inspect'
Assert (($t.Left(5000,500,500)) -eq $C -and ($t.Left(6000,500,500)) -eq $C) 'Two slow clicks are not a double-click'
Assert (($t.Left(9000,500,500)) -eq $C -and ($t.Left(9200,900,500)) -eq $C) 'Clicks on different items are not a double-click'
Assert (($t.Left(12000,500,500)) -eq $C -and ($t.Left(12200,500,500)) -eq $I -and ($t.Left(12400,500,500)) -eq $C) 'A triple click opens once'
# Category second chance with the real in-game misread of 'F-1 수류탄' and its category line.
$cc=New-Object ModeSwitchPrice.Catalog('pve',[IO.Path]::GetTempPath());$mk={param($n,$c,$p)New-Object ModeSwitchPrice.Item -Property @{Name=$n;Category=[ModeSwitchPrice.Catalog]::CategoryKey($c);Price=$p;Mode='pve'}}
$cc.Load([ModeSwitchPrice.Item[]]@((& $mk 'F-1 수류탄' 'throwables' 8999),(& $mk 'RGD-5 수류탄' 'throwables' 9000),(& $mk 'F-1 hand grenade (Reduced delay)' 'throwables' 96000),(& $mk 'M67 수류탄' 'throwables' 15000),(& $mk 'AR-15 F1 Firearms Skeletonized Style 1 권총 손잡이' 'pistol-grips' 23000)),[DateTime]::UtcNow)
Assert ($null -eq $cc.Find('入 타 p F-I 丁五“')) 'The garbled read alone does not match'
Assert ($cc.FindInCategory('入 타 p F-I 丁五“','eapons 〉 ThrowabIes').Price -eq 8999) 'Category narrows the garbled read to F-1'
Assert ($null -eq $cc.FindInCategory('入 타 p F-I 丁五“','Weapon parts & mods > Functional mods > Foregrips')) 'Wrong category gives no guess'
Assert ($null -eq $cc.FindInCategory('수류탄','Weapons > Throwables')) 'Ambiguous within the category gives no guess'
# Quest index: specific hand-ins, plants and needed keys are listed; 'any of many items' objectives are not.
$tj='{"data":{"tasks":{"t1":{"id":"t1","name":"t1 name","trader":"tr","kappaRequired":true,"objectives":[{"type":"giveItem","count":2,"foundInRaid":true,"items":["ledx"]},{"type":"giveItem","count":3,"foundInRaid":true,"items":["a","b","c","d","e","f"]}],"neededKeys":[{"keys":["key1"]}]},"t2":{"id":"t2","name":"t2 name","trader":"tr","objectives":[{"type":"plantItem","count":1,"items":["cam"]}]}}}}'
$tm=[ModeSwitchPrice.TaskIndex]::Build($tj,'{"data":{"t1 name":"위기","t2 name":"감시"}}','{"data":{"tr":{"name":"tr Nickname"}}}','{"data":{"tr Nickname":"테라피스트"}}')
$ti=New-Object ModeSwitchPrice.TaskIndex('pve',[IO.Path]::GetTempPath());$ti.Load($tm)
Assert (($ti.For('ledx') -join '') -eq '위기 (테라피스트) · 카파 · 2개 · 인레이드') 'Specific hand-in'
Assert (($ti.For('key1') -join '') -eq '위기 (테라피스트) · 카파 · 열쇠 필요') 'Needed key'
Assert (($ti.For('cam') -join '') -eq '감시 (테라피스트) · 설치') 'Plant item'
Assert ($ti.For('a').Count -eq 0) 'Any-of-many objectives are skipped'
Assert (($ti.For('zzz,cam') -join '') -eq '감시 (테라피스트) · 설치') 'Merged ids'
$done=New-Object 'System.Collections.Generic.HashSet[string]';[void]$done.Add('t1')
Assert ($ti.For('ledx',$done).Count -eq 0 -and ($ti.For('cam',$done) -join '') -eq '감시 (테라피스트) · 설치') 'Completed quests are left out'
$hm=[ModeSwitchPrice.TaskIndex]::BuildHideout('{"data":{"wb":{"id":"wb","name":"wb name","levels":[{"level":1,"itemRequirements":[{"item":"bolt","count":2,"attributes":{"foundInRaid":false}}]},{"level":2,"itemRequirements":[{"item":"bolt","count":3,"attributes":{"foundInRaid":false}},{"item":"ledx","count":1,"attributes":{"foundInRaid":true}}]},{"level":3,"itemRequirements":[{"item":"bolt","count":5,"attributes":{"foundInRaid":false}}]}]}}}','{"data":{"wb name":"작업대"}}')
$ti.LoadHideout($hm)
function HideText($ids,$lv){(@($ti.Hideout($ids,$lv))|ForEach-Object {($(if($_.Now){'+'}else{'-'}))+$_.Text}) -join ' / '}
Assert ((HideText 'bolt' $null) -eq '+작업대 Lv.1,2,3 (2·3·5개)') 'No levels entered: grouped by station';$skipWb=New-Object 'System.Collections.Generic.HashSet[string]';[void]$skipWb.Add('wb');Assert (@($ti.Hideout('bolt',$null,$skipWb)).Count -eq 0) 'Station finished by edition is left out'; Assert ((HideText 'ledx' $null) -eq '+작업대 Lv.2 (1개 필요) · 인레이드') 'Single level, found in raid'
$lv=New-Object 'System.Collections.Generic.Dictionary[string,int]';$lv['wb']=1;Assert ((HideText 'bolt' $lv) -eq '+작업대 Lv.2 (3개 필요) · 이후 Lv.3 (5개)') 'Next upgrade bright, later listed'
$lv['wb']=0;$lv2=New-Object 'System.Collections.Generic.Dictionary[string,int]';$lv2['wb']=2;Assert ((HideText 'bolt' $lv2) -eq '+작업대 Lv.3 (5개 필요)') 'Only the remaining level'; $lv2['wb']=3;Assert ((HideText 'bolt' $lv2) -eq '') 'Fully built: nothing'
$lv3=New-Object 'System.Collections.Generic.Dictionary[string,int]';Assert ((HideText 'ledx' $lv3) -eq '-작업대 Lv.2 (1개) · 나중에 · 인레이드') 'Not the next level: dim, later'
Write-Output 'PASS: script syntax, mode separation, catalog translations, price/no-price/ban distinction, captured title-bar detection, inspect click gestures, category second chance, quest index, hideout'
