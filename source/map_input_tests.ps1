$ErrorActionPreference='Stop';function Assert($ok,$why){if(!$ok){throw $why}}
$repo=Split-Path $PSScriptRoot -Parent
Assert (Test-Path ($repo+'/companion/MapInput.ps1')) 'Map opening needs a cancellable search-screen check'
. ($repo+'/companion/ExitCapture.ps1');. ($repo+'/companion/MapInput.ps1')
Add-Type -TypeDefinition @'
using System;using System.Threading.Tasks;
public class MapTestResource:IDisposable{public int Closed;public void Dispose(){Closed++;}}
public static class MapTestTask{public static TaskCompletionSource<object> Pending(){return new TaskCompletionSource<object>();}public static Task<object> Done(bool blocked){return Task.FromResult<object>(blocked);}}
'@
$script:mapsOn=$true;$script:opened=0;$script:closed=0
$map=New-Object PSObject -Property @{Visible=$false};$map|Add-Member ScriptMethod ShowOver {$script:opened++};$map|Add-Member ScriptMethod HideMap {$script:closed++}
function Load-Map{};function Status($s){};function Get-MapReadBlocked($job,$result){return [bool]$result}
function Job($task){@{task=$task;stage='recognize';game=[IntPtr]12;until=[DateTime]::UtcNow.AddSeconds(2);discard=$false;image=(New-Object MapTestResource);bitmap=(New-Object MapTestResource);stream=(New-Object MapTestResource)}}
$pending=[MapTestTask]::Pending();$script:mapRead=Job $pending.Task;$watch=[Diagnostics.Stopwatch]::StartNew()
1..1000|ForEach-Object {Poll-MapRead ([IntPtr]12)}
Assert ($watch.ElapsedMilliseconds -lt 1000 -and $script:mapRead -and !$script:opened) 'Pending OCR never blocks the helper or opens the map early'
$buffers=$script:mapRead;$pending.SetResult($true);Poll-MapRead ([IntPtr]12)
Assert (!$script:mapRead -and !$script:opened -and $buffers.image.Closed -eq 1 -and $buffers.bitmap.Closed -eq 1) 'Flea/item search blocks only this map request and releases capture buffers'
$script:mapRead=Job ([MapTestTask]::Done($false));Poll-MapRead ([IntPtr]12)
Assert ($script:opened -eq 1) 'Ordinary inventory/trader/quest screens still open the map'
$script:mapRead=Job ([MapTestTask]::Done($false));Poll-MapRead ([IntPtr]13)
Assert ($script:opened -eq 1) 'Switching away from the captured game discards the delayed request'
$script:mapsOn=$false;$script:mapRead=Job ([MapTestTask]::Done($false));Poll-MapRead ([IntPtr]12)
Assert ($script:opened -eq 1) 'Disabled maps cannot open from a pending result'
$script:mapsOn=$true;$script:mapRead=Job ([MapTestTask]::Done($false));$script:mapRead.until=[DateTime]::UtcNow.AddSeconds(-1);Poll-MapRead ([IntPtr]12)
Assert ($script:opened -eq 1 -and !$script:mapRead) 'An expired result cannot open a stale map'
$map.Visible=$true;Request-MapOpen ([IntPtr]12)
Assert ($script:closed -eq 1) 'Closing a visible map never waits for screen recognition'
Write-Output 'PASS: nonblocking pending OCR, search-only block, normal menu allowance, foreground/disabled/timeout cancellation, immediate map close'
