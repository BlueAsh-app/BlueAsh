$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
. (Join-Path $PSScriptRoot '../companion/ExitCapture.ps1')
function T($s){return $s}
function Status($s){$script:errorText=$s}
function Apply-ExitLines($lines){$script:readLines=@($lines)}
Add-Type -TypeDefinition @'
using System;using System.Threading.Tasks;
public class ExitTestResource:IDisposable {public int Closed;public void Dispose(){Closed++;}}
public static class ExitTestTasks {
 public static TaskCompletionSource<object> Pending(){return new TaskCompletionSource<object>();}
 public static Task<object> Done(object v){return Task.FromResult(v);}
 public static Task<object> Failed(){var t=Pending();t.SetException(new Exception("test-ocr-failure"));return t.Task;}
}
'@
$loading=@('12','woods','PMC','raid','loading','1','10000','0')
Assert (!(New-RaidExitSchedule $loading 10000 '')) 'No OCR during loading/countdown'
$started=@('12','woods','PMC','raid','started','1','10000','10000')
$a=New-RaidExitSchedule $started 10500 ''
Assert ($a -and $a.until-10500 -eq 3800 -and $a.next -eq 10650) 'Bounded 3.8-second capture window'
Assert (!(New-RaidExitSchedule $started 10600 $a.key)) 'No rearming on duplicate start / role refresh'
Assert (!(New-RaidExitSchedule $started 13000 '')) 'Ignore stale started state when helper attaches mid-raid'
Assert (!(New-RaidExitSchedule $started 9000 '')) 'Reject future start time'
$started[5]='2';$started[7]='14000'
Assert ([bool](New-RaidExitSchedule $started 14100 $a.key)) 'New raid can rearm'
$started[4]='ended';Assert (!(New-RaidExitSchedule $started 14100 '')) 'Ended raids never arm'
$script:exitGeneration=7;$script:mapId='woods_PMC';$script:mapsOn=$true
function Job($task,$stage='recognize'){@{task=$task;stage=$stage;generation=7;map='woods_PMC';until=[DateTime]::UtcNow.AddSeconds(8);discard=$false;bitmap=(New-Object ExitTestResource);stream=(New-Object ExitTestResource)}}
$pending=[ExitTestTasks]::Pending();$script:exitRead=Job $pending.Task
$watch=[Diagnostics.Stopwatch]::StartNew();$keysPolled=0
1..1000|ForEach-Object {Poll-ExitRead;$keysPolled++}
Assert ($watch.ElapsedMilliseconds -lt 1000 -and $keysPolled -eq 1000 -and $script:exitRead) 'Pending OCR returns immediately so map input continues'
$pending.SetResult([pscustomobject]@{Lines=@([pscustomobject]@{Text='Crossroads'})})
$buffers=$script:exitRead;Poll-ExitRead
Assert (!$script:exitRead -and $script:readLines[0] -eq 'Crossroads' -and $buffers.bitmap.Closed -eq 1 -and $buffers.stream.Closed -eq 1) 'Completed captured frame applies without requiring game foreground'
$script:readLines=@();$script:exitRead=Job ([ExitTestTasks]::Done([pscustomobject]@{Lines=@([pscustomobject]@{Text='old raid'})}));$script:exitGeneration=8;Poll-ExitRead
Assert (!$script:exitRead -and !$script:readLines.Count) 'Discard previous raid result'
$script:exitGeneration=7;$script:exitRead=Job ([ExitTestTasks]::Failed());Poll-ExitRead
Assert (!$script:exitRead -and $script:errorText -match 'test-ocr-failure') 'OCR errors release job and allow retry'
# Discarded bitmap completion still owns a new SoftwareBitmap: dispose that result as well.
$b=New-Object ExitTestResource;$script:exitRead=Job ([ExitTestTasks]::Done($b)) 'bitmap';$script:exitRead.discard=$true;Poll-ExitRead
Assert ($b.Closed -eq 1) 'Discarded decoder result releases its bitmap'
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions,System.Runtime.WindowsRuntime
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
$pin=New-Object ModePulse.Pin;$pin.Name='Scav Camp (Co-Op)';Assert ([ModePulse.ExitSign]::Cooperative($pin)) 'Co-op name'
$pin.Name='Boiler Room Basement';$pin.Condition='Requires PMC + Scav';Assert ([ModePulse.ExitSign]::Cooperative($pin)) 'Co-op requirement'
$pin.Name='Scav Checkpoint';$pin.Condition='';Assert (![ModePulse.ExitSign]::Cooperative($pin)) 'Regular scav extract stays single-person'
$pin.Name='Dorms V-Ex';$pin.Condition='Requires 20,000 Roubles';Assert (![ModePulse.ExitSign]::Cooperative($pin)) 'Vehicle extract stays single-person'
# Exercise the actual WinRT stages on a synthetic exit-list image, without starting a game or helper UI.
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../companion/Companion.ps1'),[ref]$tokens,[ref]$errors)
$init=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Init-Ocr'},$true)
. ([scriptblock]::Create($init.Extent.Text));Init-Ocr
$im=New-Object Drawing.Bitmap(760,140);$g=[Drawing.Graphics]::FromImage($im);$g.Clear([Drawing.Color]::White);$font=New-Object Drawing.Font('Arial',34);$g.DrawString('Crossroads',$font,[Drawing.Brushes]::Black,36,36);$memory=New-Object IO.MemoryStream
try{$im.Save($memory,[Drawing.Imaging.ImageFormat]::Png);$bytes=$memory.ToArray()}finally{$g.Dispose();$font.Dispose();$im.Dispose();$memory.Dispose()}
$job=Job $null 'store';$job.bitmap=$null;$job.stream=[Windows.Storage.Streams.InMemoryRandomAccessStream]::new();$job.writer=[Windows.Storage.Streams.DataWriter]::new($job.stream);$job.writer.WriteBytes($bytes)
Set-ExitOperation $job ($job.writer.StoreAsync()) ([uint32]) 'store';$script:exitRead=$job;$script:readLines=@();$watch.Restart();$heartbeats=0
while($script:exitRead -and $watch.ElapsedMilliseconds -lt 10000){Poll-ExitRead;$heartbeats++;Start-Sleep -Milliseconds 15}
Assert (!$script:exitRead -and ($script:readLines -join ' ') -match 'Crossroads') ('Real WinRT pipeline: '+$script:errorText)
Assert ($heartbeats -ge 4) 'Input loop runs between all WinRT stages'
# The screen capture is a pool-thread task too (stage 'capture'): from its PNG through every WinRT stage.
$job=Job ([System.Threading.Tasks.Task]::FromResult([byte[]]$bytes)) 'capture';$job.bitmap=$null;$job.stream=$null;$script:exitRead=$job;$script:readLines=@();$watch.Restart()
while($script:exitRead -and $watch.ElapsedMilliseconds -lt 10000){Poll-ExitRead;Start-Sleep -Milliseconds 15}
Assert (!$script:exitRead -and ($script:readLines -join ' ') -match 'Crossroads') ('Capture stage feeds the OCR stages: '+$script:errorText)
# A frame captured while the map window opened is dropped (it may show the map, not the list).
$map=[pscustomobject]@{Visible=$true};$script:exitRead=Job ([System.Threading.Tasks.Task]::FromResult([byte[]]$bytes)) 'capture';$script:readLines=@();Poll-ExitRead
Assert (!$script:exitRead -and !$script:readLines.Count) 'Capture finished under an open map is discarded';$map=$null
# The real capture call returns at once and finishes on its own thread (shell window as the target).
Add-Type -Path (Join-Path $PSScriptRoot '../price/PriceHelper.dll');Add-Type -Name ShellWin -Namespace ExitTest -MemberDefinition '[DllImport("user32.dll")] public static extern IntPtr GetShellWindow();'
$watch.Restart();$task=[ModeSwitchPrice.Desktop]::ExitListPngAsync([ExitTest.ShellWin]::GetShellWindow());$started=$watch.ElapsedMilliseconds
Assert ($started -lt 50) ('Capture does not block the caller: '+$started+' ms');$null=$task.Wait(10000);Assert ($task.Status -eq 'RanToCompletion' -and $task.Result.Length -gt 1000) 'Capture task produces a PNG'
. (Join-Path $PSScriptRoot '../companion/CompanionCore.ps1')
$apply=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Apply-ExitLines'},$true)
. ([scriptblock]::Create($apply.Extent.Text))
function Remember-Exits($lines){}
$exit=New-Object ModePulse.Pin;$exit.Name='Crossroads';$transit=New-Object ModePulse.Pin;$transit.Name='Transit to Factory';$transit.Kind='transit'
$canvas=[pscustomobject]@{Pins=@($exit,$transit);Available=(New-Object 'System.Collections.Generic.HashSet[string]');TaxiStates=(New-Object 'System.Collections.Generic.Dictionary[string,ModePulse.TaxiState]')};$canvas|Add-Member ScriptMethod Invalidate {}
$map=[pscustomobject]@{Canvas=$canvas;Role=[pscustomobject]@{SelectedItem='PMC'};State=[pscustomobject]@{Text=''};Visible=$true};$catalog=@();$script:lastFrame=@()
Apply-ExitLines @('EXFIL01 Crossroads','Transit to Factory');Assert ($canvas.Available.Count -eq 0) 'One frame cannot confirm exits'
Apply-ExitLines @('EXFIL01 Crossroads','Transit to Factory');Assert ($canvas.Available.Count -eq 2) 'Two matching frames confirm exit and transit while map is visible'
'PASS: fresh raid scheduling, responsive polling, stale-result cleanup, co-op classification, actual Windows OCR stages'
