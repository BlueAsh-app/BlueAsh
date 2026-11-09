$ErrorActionPreference='Stop'
# Story helper: data integrity, Korean coverage, states, per-mode progress kept on disk, map placement, spoiler levels.
$repo=Split-Path $PSScriptRoot -Parent;$root=Join-Path $repo 'companion'
function Assert($ok,$name){if(!$ok){throw "FAIL: $name"}}
Add-Type -AssemblyName System.Web.Extensions,System.Windows.Forms,System.Drawing;Add-Type -Path (Join-Path $root 'CompanionUI.dll')
[ModePulse.Story]::Load($root)
$ch=[ModePulse.Story]::Chapters;Assert ($ch.Count -eq 10) 'ten chapters'
$names=@($ch|ForEach-Object Name);foreach($n in 'Tour','Falling Skies','The Ticket','Batya','Accidental Witness','Blue Fire','Boreas','The Labyrinth','The Unheard','They Are Already Here'){Assert ($names -contains $n) "chapter $n"}
$steps=0;foreach($c in $ch){$steps+=$c.Steps.Count};Assert ($steps -eq [ModePulse.Story]::Steps.Count -and $steps -gt 400) 'unique step ids'
# Every English line has a line in each translated language (Korean also checked for Hangul).
foreach($lang in 'zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'){$tr=@{};foreach($l in [IO.File]::ReadAllLines((Join-Path $root "story.$lang.tsv"),[Text.Encoding]::UTF8)){if($l -and !$l.StartsWith('#')){$c=$l.Split([char]9);$tr[$c[0]]=$c[1]}};foreach($c in $ch){foreach($s in $c.Steps){Assert ($tr.ContainsKey($s.Text) -and $tr[$s.Text].Trim().Length -gt 0) ("$lang"+': '+$s.Text)}}}
$ko=@{};foreach($l in [IO.File]::ReadAllLines((Join-Path $root 'story.ko.tsv'),[Text.Encoding]::UTF8)){if($l -and !$l.StartsWith('#')){$c=$l.Split([char]9);$ko[$c[0]]=$c[1]}}
foreach($c in $ch){foreach($s in $c.Steps){Assert ($ko.ContainsKey($s.Text) -and $ko[$s.Text] -match '[가-힣]') ('Korean: '+$s.Text)};foreach($t in $c.Activation,$c.Description,$c.Notes,$c.Rewards){if($t){Assert $ko.ContainsKey($t) ('Korean: '+$t)}}}
# Progress per mode, kept across runs.
$tmp=Join-Path ([IO.Path]::GetTempPath()) ('story-test-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $tmp|Out-Null
try{
 $file=Join-Path $tmp 'story-progress.json';[ModePulse.Story]::Use($file)
 $tour=[ModePulse.Story]::Find('tour');$fs=[ModePulse.Story]::Find('falling-skies')
 $first=$tour.Steps[0];Assert ([ModePulse.Story]::StateOf($first,'pve') -eq 'next') 'first Tour step is next'
 Assert ([ModePulse.Story]::ChapterState($fs,'pve') -eq 'locked') 'Falling Skies locked until Tour is done'
 Assert ([ModePulse.Story]::StateOf($fs.Steps[0],'pve') -eq 'locked') 'locked chapter step'
 [ModePulse.Story]::SetDone('pve',$first.Id,$true)
 Assert ([ModePulse.Story]::StateOf($first,'pve') -eq 'done') 'ticked step is done'
 $second=@($tour.Steps|Where-Object {!$_.Optional})[1];Assert ([ModePulse.Story]::StateOf($second,'pve') -eq 'next') 'next moves on'
 Assert ([ModePulse.Story]::StateOf($first,'regular') -eq 'next') 'PvP progress separate from PvE'
 [ModePulse.Story]::Spoiler='guide';[ModePulse.Story]::HideDone=$false;[ModePulse.Story]::Save()
 [ModePulse.Story]::Use((Join-Path $tmp 'other.json'));Assert (![ModePulse.Story]::IsDone('pve',$first.Id)) 'other file is empty'
 [ModePulse.Story]::Spoiler='hide';[ModePulse.Story]::Use($file)
 Assert ([ModePulse.Story]::IsDone('pve',$first.Id) -and [ModePulse.Story]::Spoiler -eq 'guide' -and ![ModePulse.Story]::HideDone) 'progress and settings reload'
 foreach($s in $tour.Steps){if(!$s.Optional){[ModePulse.Story]::SetDone('pve',$s.Id,$true)}}
 Assert ([ModePulse.Story]::ChapterState($tour,'pve') -eq 'done' -and [ModePulse.Story]::ChapterState($fs,'pve') -ne 'locked') 'finishing Tour opens Falling Skies'
 # Branch steps: the armoured-case choice in Falling Skies.
 $keep=@($fs.Steps|Where-Object Text -eq 'Keep the armored case for yourself')[0];Assert ([ModePulse.Story]::IsBranch($keep)) 'armored case is a branch'
 # Placement on maps (shipped transforms copied beside markers.json as at run time).
 $maps=Join-Path $tmp 'maps';New-Item -ItemType Directory $maps|Out-Null;Copy-Item (Join-Path $repo 'data/maps/markers.json'),(Join-Path $repo 'maps/fit.json'),(Join-Path $repo 'maps/levels.json') $maps
 $shipped=Join-Path $maps 'markers.json';[ModePulse.Story]::Only='';[ModePulse.Story]::HideDone=$true
 $un=New-Object 'System.Collections.Generic.List[ModePulse.Story+Step]';$pins=[ModePulse.Story]::Pins('shoreline',$shipped,'regular',$un)
 Assert ($pins.Count -gt 0) 'story markers on Shoreline'
 foreach($p in $pins){Assert ($p.Kind -eq 'story' -and $p.X -ge 0 -and $p.X -le 1 -and $p.Y -ge 0 -and $p.Y -le 1 -and $p.Story -in 'next','active') ('marker '+$p.Ref)}
 $un2=New-Object 'System.Collections.Generic.List[ModePulse.Story+Step]';$lab=[ModePulse.Story]::Pins('labyrinth',$shipped,'regular',$un2);Assert ($lab.Count -eq 0) 'no exact spot without a map transform'
 # Every chapter, every step: count positions that can be placed on their map.
 [ModePulse.Story]::Focus='';$placed=0;$unplaced=0;foreach($c in $ch){foreach($s in $c.Steps){foreach($m in $s.Marks){$ok=$true;for($i=0;$i+1 -lt $m.Pts.Length;$i+=2){$f=[ModePulse.FandomMarkers]::Floors($m.Map,$shipped,$m.Pts[$i],$m.Pts[$i+1]);if($f -ne $null -and $f.Count -gt 0){$ok=$false}elseif($f -eq $null -or $f.Count -eq 0){if($null -eq [ModePulse.FandomMarkers]::ToImage($m.Map,$shipped,$m.Pts[$i],$m.Pts[$i+1])){$ok=$false}}};if($ok){$placed++}else{$unplaced++}}}}
 "marks placed without floor doubt: $placed, needing a floor or a transform: $unplaced"
 # Spoiler levels shape the branch data.
 $fsIds=New-Object 'System.Collections.Generic.List[string]';$fsIds.Add($keep.Id);$name=[Func[string,string]]{param($i)$i}
 [ModePulse.Story]::Spoiler='hide';$d=[ModePulse.Story]::Card($fsIds,$keep.Id,'pve','bigmap',$name);Assert ($d['branches'].Count -eq 0 -and $d['chapter']['notes'] -eq '') 'hidden spoilers'
 [ModePulse.Story]::Spoiler='hint';$d=[ModePulse.Story]::Card($fsIds,$keep.Id,'pve','bigmap',$name);Assert ($d['branchCount'] -eq 2 -and $d['branches'][0]['sides'][0]['steps'].Count -eq 0) 'hint: count only'
 [ModePulse.Story]::Spoiler='guide';$d=[ModePulse.Story]::Card($fsIds,$keep.Id,'pve','bigmap',$name);Assert ($d['branches'][1]['sides'][1]['steps'].Count -ge 1 -and $d['chapter']['notes'].Length -gt 0) 'guide: sides and notes'
 $t=[ModePulse.Story]::Find('the-ticket');$e=@($t.Steps|Where-Object Ending)[0];$tid=New-Object 'System.Collections.Generic.List[string]';$tid.Add($e.Id)
 [ModePulse.Story]::Spoiler='hint';$d=[ModePulse.Story]::Card($tid,$e.Id,'pve','terminal',$name);Assert ($d['endings'].Count -eq 0 -and ($d['chapter']['steps']|Where-Object {$_['id'] -eq $e.Id})['text'] -ne $e.Text) 'hint: ending steps masked'
 [ModePulse.Story]::Spoiler='guide';$d=[ModePulse.Story]::Card($tid,$e.Id,'pve','terminal',$name);Assert ($d['endings'][0]['system'] -eq '') 'guide: ending names hidden'
 [ModePulse.Story]::Spoiler='all';$d=[ModePulse.Story]::Card($tid,$e.Id,'pve','terminal',$name);Assert ($d['endings'].Count -eq 4 -and $d['endings'][0]['system'].Length -gt 0) 'full reveal: endings'
}finally{Remove-Item -Recurse -Force $tmp}
'PASS: story chapters, Korean text, states, per-mode progress, placement, spoiler levels'
