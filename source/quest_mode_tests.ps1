# Quest pins follow the mode the game is in: a session that switches PvE -> PvP season keeps its earlier quest events in
# PvE, and the live mode comes from the newest log folder by creation time (names such as 2-10-43 vs 13-27-50 do not sort).
$ErrorActionPreference='Stop';function Assert($ok,$name){if(!$ok){throw $name}}
$work=Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($work)
try{
 $dll=Join-Path $work 'CompanionUI.dll'
 Add-Type -Path @((Join-Path $PSScriptRoot '../companion/CompanionUI.cs'),(Join-Path $PSScriptRoot '../companion/MapSearch.cs')) -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.Core,System.Net.Http,System.Web.Extensions,(Join-Path $PSScriptRoot '../companion/Microsoft.Web.WebView2.Core.dll'),(Join-Path $PSScriptRoot '../companion/Microsoft.Web.WebView2.WinForms.dll') -OutputAssembly $dll -OutputType Library
 Add-Type -Path $dll
 $logs=Join-Path $work 'Logs'
 function Unix($local){[long]([DateTime]::Parse($local).ToUniversalTime()-[DateTime]'1970-01-01').TotalSeconds}
 function Session($name,$created,$modes,$events,$prestige=''){$d=Join-Path $logs $name;[void][IO.Directory]::CreateDirectory($d)
  [IO.File]::WriteAllText((Join-Path $d "$name application_000.log"),(($modes|ForEach-Object {"$($_[0]).100|1.2.0.0.47965|Info|application|Session mode: $($_[1])"}) -join "`n")+"`n")
  [IO.File]::WriteAllText((Join-Path $d "$name push-notifications_000.log"),(($events|ForEach-Object {"`"type`": $(if($_.Count -gt 2){$_[2]}else{10}),`n    `"dt`": $(Unix $_[0]),`n    `"text`": `"quest started`",`n    `"templateId`": `"$($_[1]) description`","}) -join "`n"))
  if($prestige){[IO.File]::WriteAllText((Join-Path $d "$name backend_000.log"),"$prestige.100|1.2.0.0.47965|Info|backend|<--- Response HTTPS: 200, GET, /client/prestige/obtain`n")}
  [IO.Directory]::SetCreationTimeUtc($d,[DateTime]::Parse($created).ToUniversalTime())}
 $a='aaaaaaaaaaaaaaaaaaaaaaaa';$b='bbbbbbbbbbbbbbbbbbbbbbbb';$c='cccccccccccccccccccccccc'
 # Early morning PvE session, then an afternoon one that starts in PvE and switches to PvP season.
 Session 'log_2026.10.12_2-10-43_1.2.0.0.47965' '2026-10-12 02:10:43' @(,@('2026-10-12 02:10:58','Pve')) @(,@('2026-10-12 02:20:00',$a))
 Session 'log_2026.10.12_13-27-35_1.2.0.0.47965' '2026-10-12 13:27:35' @(@('2026-10-12 13:27:50','Pve'),@('2026-10-12 13:28:22','PvpSeason')) @(@('2026-10-12 13:28:00',$b),@('2026-10-12 13:40:00',$c))
 [ModePulse.Quests]::CacheFile=Join-Path $work 'index.json'
 Assert ([ModePulse.Quests]::CurrentMode($logs) -eq 'pvp-season') ('live mode: '+[ModePulse.Quests]::CurrentMode($logs))
 $pve=[ModePulse.Quests]::Active($logs,'pve');$pvp=[ModePulse.Quests]::Active($logs,'pvp-season')
 Assert ($pve.Contains($a) -and $pve.Contains($b) -and !$pve.Contains($c)) ('pve: '+(@($pve) -join ','))
 Assert ($pvp.Count -eq 1 -and $pvp.Contains($c)) ('pvp-season: '+(@($pvp) -join ','))
 # After a PvE prestige, taking up again quests finished before it is normal: it must not read as a wipe that hides
 # the quests taken since the prestige (real logs 2026-10-12: 6 of 47 PvE quests were left on the map).
 $logs=Join-Path $work 'Logs2';$q=1..4|ForEach-Object {([string]$_)*24}
 Session 'log_2025.11.18_2-55-00_1.0' '2025-11-18 02:55:00' @(,@('2025-11-18 02:55:10','Pve')) @(@('2025-11-18 03:00:00',$q[0]),@('2025-11-18 03:10:00',$q[0],12),@('2025-11-18 03:20:00',$q[1]),@('2025-11-18 03:30:00',$q[1],12),@('2025-11-18 03:40:00',$q[2]),@('2025-11-18 03:50:00',$q[2],12))
 Session 'log_2026.10.07_2-50-00_1.2' '2026-10-07 02:50:00' @(,@('2026-10-07 02:50:10','Pve')) @(,@('2026-10-07 03:30:00',$q[3])) '2026-10-07 03:04:00'
 Session 'log_2026.10.12_18-31-00_1.2' '2026-10-12 18:31:00' @(,@('2026-10-12 18:31:38','Pve')) @(@('2026-10-12 18:59:36',$q[0]),@('2026-10-12 18:59:46',$q[1]),@('2026-10-12 18:59:52',$q[2]))
 [ModePulse.Quests]::CacheFile=Join-Path $work 'index2.json'
 $pve=[ModePulse.Quests]::Active($logs,'pve')
 Assert (@($q|Where-Object {$pve.Contains($_)}).Count -eq 4) ('after prestige: '+(@($pve) -join ','))
 'PASS: live mode by folder creation time, quest events by the mode in force when they happened, restarts after a prestige are not a wipe'
}finally{Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue}
