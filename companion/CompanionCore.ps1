# Apply these map defaults once; later saved opt-outs survive restarts and future updates.
function Enable-MapDefaults([hashtable]$Config){
 if([int]$Config.mapDefaults -ge 1){return $false}
 foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){$Config[$key]=$true}
 $Config.mapDefaults=1;return $true
}
# Exit names are English in the game's Korean locale, read by the Korean OCR engine: fold look-alike characters.
function Normalize-Exit([string]$text){$t=[regex]::Replace($text.ToLowerInvariant(),'[\s\p{P}\p{S}]','');return $t.Replace('0','o').Replace('1','l').Replace('i','l').Replace('|','l')}
# Fewest edits to turn $name into some substring of $line (approximate substring match).
function Exit-Distance([string]$line,[string]$name){
 $n=$name.Length;$prev=New-Object int[] ($n+1);for($j=0;$j -le $n;$j++){$prev[$j]=$j};$best=$n
 foreach($ch in $line.ToCharArray()){$cur=New-Object int[] ($n+1);$cur[0]=0
  for($j=1;$j -le $n;$j++){$cost=$(if($name[$j-1] -eq $ch){0}else{1});$cur[$j]=[Math]::Min([Math]::Min($prev[$j]+1,$cur[$j-1]+1),$prev[$j-1]+$cost)}
  if($cur[$n] -lt $best){$best=$cur[$n]};$prev=$cur}
 return $best
}
# In-game transit lines are Korean ("리저브로 이동"); Fandom names them "Transit to Reserve".
$transitMaps=[ordered]@{'리저브'='reserve';'공장'='factory';'팩토리'='factory';'인터체인지'='interchange';'해안선'='shoreline';'쇼어라인'='shoreline';'세관'='customs';'커스텀'='customs';'삼림'='woods';'숲'='woods';'등대'='lighthouse';'라이트하우스'='lighthouse';'연구소'='lab';'렙'='lab';'시내'='streets';'스트리트'='streets';'그라운드'='ground zero';'터미널'='terminal';'미궁'='labyrinth';'아이스브레이커'='icebreaker'}
function Match-Exits($Lines,$Pins){
 $result=New-Object 'System.Collections.Generic.HashSet[string]'
 foreach($line in $Lines){$text=[string]$line;$key=Normalize-Exit $text;$digits=[regex]::Replace($text,'[^0-9]','')
  # Digits misread as look-alikes within one word ("ZB-1011" read as "7+1이1": 이 = 0 1, o/ㅇ = 0, l/I/| = 1).
  $folded=@($text -split '\s+'|ForEach-Object {[regex]::Replace(($_.Replace('이','01') -creplace '[oOㅇ]','0' -creplace '[lI|]','1'),'[^0-9]','')}|Where-Object {$_})
  foreach($pin in $Pins){$raw=[string]$pin.Name
   # Transit: the Korean destination in the line names the map in "Transit to <map>".
   if($raw -like 'Transit to*'){if($key.Contains((Normalize-Exit $raw))){[void]$result.Add($raw);continue};if($text -match '이동'){foreach($k in $transitMaps.Keys){if($text.Contains($k) -and $raw.ToLowerInvariant().Contains($transitMaps[$k])){[void]$result.Add($raw);break}}};continue}
   # Numbered exits ("ZB-1013", read as "7+1013"): the digit run decides.
   $runs=@([regex]::Matches($raw,'\d{3,}')|ForEach-Object {$_.Value});if($runs.Count -and ($runs|Where-Object {$r=$_;$digits.Contains($r) -or @($folded|Where-Object {$_.Contains($r)}).Count}).Count -eq $runs.Count){[void]$result.Add($raw);continue}
   # Words: notes in brackets differ between the game and the wiki ("(조명탄 탈출구)" / "(Flare)"), so they are ignored.
   $name=Normalize-Exit ([regex]::Replace($raw,'\s*\([^)]*\)',''));if($name.Length -lt 3){continue}
   # One misread character allowed per 4 (none for short names like "Gate 3", which differ by one digit).
   if($key.Contains($name) -or ($name.Length -ge 7 -and (Exit-Distance $key $name) -le [Math]::Floor($name.Length/4))){[void]$result.Add($raw)}}
 };return @($result)
}function Classify-Notice([string]$Text){
 $t=$Text.ToLowerInvariant()
 if($t -notmatch 'maintenance|installation|update|patch'){return ''}
 if($t -match 'website|arena'){return ''}
 if($t -match 'complete|completed|available again'){return 'maintenance.completed'}
 if($t -match 'extended|extension'){return 'maintenance.extended'}
 if($t -match 'have started|has started|we are installing|installation has begun'){return 'maintenance.started'}
 return ''
}
function Valid-Webhook([string]$Url){return $Url -cmatch '^https://discord\.com/api(?:/v[0-9]+)?/webhooks/[0-9]+/[A-Za-z0-9_-]+$'}
