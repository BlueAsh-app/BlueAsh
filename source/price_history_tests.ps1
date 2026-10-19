$ErrorActionPreference='Stop';function Assert($ok,$why){if(!$ok){throw $why}}
$repo=Split-Path $PSScriptRoot -Parent
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Net.Http,System.Web.Extensions
Add-Type -Path ($repo+'/companion/CompanionUI.dll')
$work=Join-Path $PSScriptRoot ('.bundle/history-test-'+[Guid]::NewGuid().ToString('N'));$logs=Join-Path $work 'Logs';$session=Join-Path $logs 'log_1';[void][IO.Directory]::CreateDirectory($session)
$nl=[string][char]10;function Unix($s){return [long]([DateTime]::Parse($s).ToUniversalTime()-[DateTime]'1970-01-01').TotalSeconds}
[IO.File]::WriteAllText(($session+'/application_0.log'),('2026-10-15 12:00:00.100|1.2.0|Info|application|Session mode: Pve'+$nl+'2026-10-15 12:10:00.100|1.2.0|Info|application|Session mode: PvpSeason'+$nl))
$a='aaaaaaaaaaaaaaaaaaaaaaaa';$b='bbbbbbbbbbbbbbbbbbbbbbbb'
[IO.File]::WriteAllText(($session+'/push-notifications_0.log'),(('{"type": 12, "dt": '+(Unix '2026-10-15 12:05:00')+', "text": "completed", "templateId": "'+$a+' description"},')+$nl+('{"type": 12, "dt": '+(Unix '2026-10-15 12:12:00')+', "text": "completed", "templateId": "'+$b+' description"},')))
[IO.File]::WriteAllText(($session+'/backend_0.log'),('2026-10-15 12:02:00.100|1.2.0|Info|backend|<--- Response HTTPS: 200, GET, /client/prestige/obtain'+$nl))
[ModePulse.Quests]::CacheFile=$work+'/index.json'
$history=New-Object ModePulse.QuestHistory($logs,'pve')
$gate=[ModePulse.QuestHistory].GetField('gate',[Reflection.BindingFlags]'NonPublic,Static').GetValue($null)
[Threading.Monitor]::Enter($gate)
try{
 $watch=[Diagnostics.Stopwatch]::StartNew();$history.RefreshIfStale()
 Assert ($watch.ElapsedMilliseconds -lt 200) 'Starting history never waits for the log-reader lock'
 $watch.Restart();$changed=$history.Poll()
 Assert (!$changed -and !$history.Ready -and $watch.ElapsedMilliseconds -lt 100) 'Pending history is explicitly unavailable and polling never blocks'
}finally{[Threading.Monitor]::Exit($gate)}
function Ready($h){$until=[DateTime]::UtcNow.AddSeconds(10);while(!$h.Poll() -and !$h.Ready){if([DateTime]::UtcNow -ge $until){throw 'History did not finish'};Start-Sleep -Milliseconds 10}}
Ready $history
Assert ($history.Completed.Count -eq 1 -and $history.Completed.Contains($a) -and !$history.Completed.Contains($b)) 'Background snapshot retains completed quests in the correct mode'
Assert ($history.ResetAt -eq (Unix '2026-10-15 12:02:00')-60) 'Background snapshot retains prestige reset time'
$old=$history.Completed;$history.RefreshIfStale()
Assert (!$history.Poll() -and [object]::ReferenceEquals($old,$history.Completed)) 'Fresh snapshot is reused without another read'
$pvp=New-Object ModePulse.QuestHistory($logs,'pvp-season');$pvp.RefreshIfStale();Ready $pvp
Assert ($pvp.Completed.Count -eq 1 -and $pvp.Completed.Contains($b) -and $pvp.ResetAt -eq 0) 'Per-mode snapshots keep PvE/PvP completion and resets separate'
Write-Output 'PASS: nonblocking pending/start/poll, background completion, mode separation, prestige reset, fresh snapshot reuse'
