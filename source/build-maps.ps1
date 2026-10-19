# Builds the map images and marker snapshot from the Escape from Tarkov Fandom interactive maps (run by hand; output is committed):
#   maps/<id>.jpg       the interactive map's base image (Map:<title> page "mapImage"), longest side scaled to $MaxSide
#   maps/markers.json   exits (PMC/Scav), transits, PMC spawns, bosses, cultists only, positions normalised 0..1 (top-left),
#                       plus "transform": game x,z -> image, fitted from exits shared with tarkov.dev (screenshot position)
#   maps/catalog.json   file names switched to <id>.jpg
# At run time the companion refreshes the markers from the same pages at most daily (ModePulse.FandomMarkers);
# the images only change when this script is run again. Fandom content: CC BY-SA, see NOTICE.txt.
param([int]$MaxSide=3000)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent;$maps=Join-Path $repo 'maps';$pack=Join-Path $repo 'data/maps' # pictures and markers.json go to the data pack (source/build-datapack.ps1), not git
Add-Type -AssemblyName PresentationCore,WindowsBase,System.Web.Extensions
Add-Type -Path (Join-Path $repo 'companion/CompanionUI.dll')
$js=New-Object Web.Script.Serialization.JavaScriptSerializer;$js.MaxJsonLength=[int]::MaxValue
$client=New-Object Net.WebClient
function Fetch([string]$url){$client.Headers['User-Agent']='Mozilla/5.0 (Windows NT 10.0; Win64; x64) ModePulse';$client.Headers['Referer']='https://escapefromtarkov.fandom.com/';return $client.DownloadData($url)}
# Base images come as PNG, JPEG or WebP; WIC (Windows' codecs) decodes all three, already scaled down.
function Save-Jpeg([byte[]]$bytes,[string]$out,[int]$w,[int]$h){
 $img=New-Object Windows.Media.Imaging.BitmapImage;$img.BeginInit();$img.StreamSource=New-Object IO.MemoryStream(,$bytes);$img.CacheOption='OnLoad'
 if($w -ge $h){$img.DecodePixelWidth=[Math]::Min($w,$MaxSide)}else{$img.DecodePixelHeight=[Math]::Min($h,$MaxSide)};$img.EndInit()
 $enc=New-Object Windows.Media.Imaging.JpegBitmapEncoder;$enc.QualityLevel=85;$enc.Frames.Add([Windows.Media.Imaging.BitmapFrame]::Create($img))
 $f=[IO.File]::Create($out);try{$enc.Save($f)}finally{$f.Close()};return "$($img.PixelWidth)x$($img.PixelHeight)"
}
$catalogPath=Join-Path $maps 'catalog.json';$catalog=@(Get-Content $catalogPath -Raw -Encoding UTF8|ConvertFrom-Json|ForEach-Object {$_})
$snapshot=[ordered]@{}
# tarkov.dev exits/transits with game coordinates, names in English (the same names the Fandom markers use).
# JSON objects keyed by id come back as dictionaries: iterate their values.
function Seq($o){if($o -is [Collections.IDictionary]){return @($o.Values)};return @($o)}
$devMaps=Seq ($js.DeserializeObject([ModePulse.FandomMarkers]::Get('https://json.tarkov.dev/regular/maps'))['data']['maps'])
$devEn=$js.DeserializeObject([ModePulse.FandomMarkers]::Get('https://json.tarkov.dev/regular/maps_en'))['data']
function En($key){if($key -and $devEn.ContainsKey([string]$key)){return [string]$devEn[[string]$key]};return [string]$key}
foreach($entry in [ModePulse.FandomMarkers]::Titles.GetEnumerator()){
 $id=$entry.Key;$title=$entry.Value
 $page=[ModePulse.FandomMarkers]::ParsePage([ModePulse.FandomMarkers]::Get([ModePulse.FandomMarkers]::PageUrl($title)))
 $info=$js.DeserializeObject([ModePulse.FandomMarkers]::Get('https://escapefromtarkov.fandom.com/api.php?action=query&prop=imageinfo&iiprop=url&format=json&titles='+[Uri]::EscapeDataString('File:'+$page['image'])))
 $url=$null;foreach($p in $info['query']['pages'].Values){$url=$p['imageinfo'][0]['url']}
 if(!$url){"$id : 기본 이미지 주소 없음 · 건너뜀";continue}
 $size=Save-Jpeg (Fetch $url) (Join-Path $pack ($id+'.jpg')) ([int]$page['w']) ([int]$page['h'])
 foreach($row in $catalog){if($row.id -eq $id){$row.file=$id+'.jpg'}}
 # Game position -> image: pair the page's exits/transits with tarkov.dev's (game x,z) by name and fit an affine map.
 $fitText='위치 변환 없음'
 $dev=$null;foreach($m in $devMaps){if(([string]$m['nameId']).ToLower() -eq $id){$dev=$m;break}}
 if($dev){
  $game=@();foreach($e in (Seq $dev['extracts'])){if($e['position']){$game+=,@([ModePulse.FandomMarkers]::Key((En $e['name'])),[double]$e['position']['x'],[double]$e['position']['z'])}}
  foreach($t in (Seq $dev['transits'])){if($t['position']){$game+=,@([ModePulse.FandomMarkers]::Key((En $t['description'])),[double]$t['position']['x'],[double]$t['position']['z'])}}
  $pairs=New-Object 'System.Collections.Generic.List[double[]]'
  foreach($m in $page['markers']){if($m['k'] -notlike 'exfil_*'){continue};$k=[ModePulse.FandomMarkers]::Key($m['n']);if($k.Length -lt 4){continue}
   foreach($g in $game){if($g[0].Length -ge 4 -and ($g[0] -eq $k -or $g[0].Contains($k) -or $k.Contains($g[0]))){$pairs.Add([double[]]@($g[1],$g[2],[double]$m['x'],[double]$m['y']));break}}}
  $t=[ModePulse.FandomMarkers]::Fit($pairs)
  if($t){$page['transform']=[double[]]$t[0..5];$fitText="위치 변환: 짝 $($pairs.Count)개 중 $([int]$t[6])개 사용, 오차 $([Math]::Round($t[7]*100,2))%"
   # Local correction: spawn markers paired with tarkov.dev spawns add control points; the radius (sigma) is the
   # one with the lowest leave-one-out error on the exits, and kept only when it beats the plain affine fit.
   $marks=New-Object 'System.Collections.Generic.List[double[]]';foreach($m in $page['markers']){if($m['k'] -like 'spawn_*'){$marks.Add([double[]]@([double]$m['x'],[double]$m['y']))}}
   $spawnGame=New-Object 'System.Collections.Generic.List[double[]]';foreach($sp in (Seq $dev['spawns'])){if($sp['position']){$spawnGame.Add([double[]]@([double]$sp['position']['x'],[double]$sp['position']['z']))}}
   $spawnPairs=[ModePulse.FandomMarkers]::PairNearest($page['transform'],$marks,$spawnGame)
   [double]$x0=1e9;[double]$x1=-1e9;[double]$z0=1e9;[double]$z1=-1e9;foreach($q in $pairs){$x0=[Math]::Min($x0,$q[0]);$x1=[Math]::Max($x1,$q[0]);$z0=[Math]::Min($z0,$q[1]);$z1=[Math]::Max($z1,$q[1])};[double]$extent=[Math]::Max($x1-$x0,$z1-$z0)
   $best=$null;foreach($f in 0.04,0.06,0.09,0.13){$r=[ModePulse.FandomMarkers]::LeaveOneOut($page['transform'],$pairs,$spawnPairs,$extent*$f);if(!$best -or $r[1] -lt $best[1]){$best=@(($extent*$f),$r[1],$r[0],$r[2])}}
   if($best -and $best[1] -lt $best[2]){$all=New-Object 'System.Collections.Generic.List[double[]]';$all.AddRange($pairs);$all.AddRange($spawnPairs);$page['ctrl']=[ModePulse.FandomMarkers]::Controls($page['transform'],$all);$page['sigma']=[double]$best[0]}
   if($best){$fitText+=" · 지역 보정: 기준점 $($pairs.Count)+$($spawnPairs.Count)개, 검증 오차 $([Math]::Round($best[2]*100,2))% → $([Math]::Round($best[1]*100,2))%"+$(if($best[1] -lt $best[2]){''}else{' (미사용)'})}
  }else{$fitText="위치 변환 실패 (짝 $($pairs.Count)개)"}
 }
 $snapshot[$id]=$page
 $count=@{};foreach($m in $page['markers']){$count[$m['k']]=1+[int]$count[$m['k']]}
 "$id ($title) : $size · "+(($count.GetEnumerator()|Sort-Object Name|ForEach-Object {"$($_.Name) $($_.Value)"}) -join ', ')+" · $fitText"
}
$utf8=New-Object Text.UTF8Encoding($true)
[IO.File]::WriteAllText((Join-Path $pack 'markers.json'),$js.Serialize([ordered]@{fetched=[DateTime]::UtcNow.ToString('o');maps=$snapshot}),$utf8)
[IO.File]::WriteAllText($catalogPath,(ConvertTo-Json -InputObject $catalog -Compress),$utf8)
'완료: maps/<id>.jpg, maps/markers.json, maps/catalog.json'
