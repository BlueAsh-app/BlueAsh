$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
. (Join-Path $PSScriptRoot 'helper_test_load.ps1')
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
$pin=New-Object ModePulse.Pin;$pin.Name='Scav Camp (Co-Op)';Assert ([ModePulse.ExitSign]::Cooperative($pin)) 'Co-op name'
$pin.Name='Boiler Room Basement';$pin.Condition='Requires PMC + Scav';Assert ([ModePulse.ExitSign]::Cooperative($pin)) 'Co-op requirement'
$pin.Name='Scav Checkpoint';$pin.Condition='';Assert (![ModePulse.ExitSign]::Cooperative($pin)) 'Regular scav extract stays single-person'
$pin.Name='Dorms V-Ex';$pin.Condition='Requires 20,000 Roubles';Assert (![ModePulse.ExitSign]::Cooperative($pin)) 'Vehicle extract stays single-person'
# The real Windows OCR chain (WinOcr: store -> decode -> bitmap -> recognize) on a synthetic exit-list image, on a pool thread.
$engine=[ModePulse.WinOcr]::Engine('en');if(!$engine){$engine=[ModePulse.WinOcr]::Engine('ko')};if(!$engine){$engine=[Windows.Media.Ocr.OcrEngine,Windows.Foundation,ContentType=WindowsRuntime]::TryCreateFromUserProfileLanguages()}
$im=New-Object Drawing.Bitmap(760,140);$g=[Drawing.Graphics]::FromImage($im);$g.Clear([Drawing.Color]::White);$font=New-Object Drawing.Font('Arial',34);$g.DrawString('Crossroads',$font,[Drawing.Brushes]::Black,36,36);$memory=New-Object IO.MemoryStream
try{$im.Save($memory,[Drawing.Imaging.ImageFormat]::Png);$bytes=$memory.ToArray()}finally{$g.Dispose();$font.Dispose();$im.Dispose();$memory.Dispose()}
$watch=[Diagnostics.Stopwatch]::StartNew();$task=[ModePulse.WinOcr]::Read($engine,$bytes);$started=$watch.ElapsedMilliseconds;$heartbeats=0
while(!$task.IsCompleted -and $watch.ElapsedMilliseconds -lt 10000){$heartbeats++;Start-Sleep -Milliseconds 15}
Assert ($task.Status -eq 'RanToCompletion' -and $task.Result.Text -match 'Crossroads' -and $task.Result.Lines[0].Words.Count -ge 1) ('Real WinRT pipeline: '+$task.Exception)
Assert ($started -lt 200) ('OCR returns a task at once so the input loop keeps running: '+$started+' ms')
# The real capture call returns at once and finishes on its own thread (shell window as the target).
Add-Type -Name ShellWin -Namespace ExitTest -MemberDefinition '[DllImport("user32.dll")] public static extern IntPtr GetShellWindow();'
$watch.Restart();$task=[ModeSwitchPrice.Desktop]::ExitListPngAsync([ExitTest.ShellWin]::GetShellWindow());$started=$watch.ElapsedMilliseconds
Assert ($started -lt 50) ('Capture does not block the caller: '+$started+' ms');$null=$task.Wait(10000);Assert ($task.Status -eq 'RanToCompletion' -and $task.Result.Length -gt 1000) 'Capture task produces a PNG'
# Two matching reads in one check confirm an exit (a misread frame in between does not reset it).
$seen=New-Object 'System.Collections.Generic.HashSet[string]';$available=New-Object 'System.Collections.Generic.HashSet[string]'
$exit=New-Object ModePulse.Pin;$exit.Name='Crossroads';$transit=New-Object ModePulse.Pin;$transit.Name='Transit to Factory';$transit.Kind='transit'
$pins=@($exit,$transit)
[ModePulse.ExitMatch]::Confirm($seen,[string[]]@(Match-Exits @('EXFIL01 Crossroads','Transit to Factory') $pins),$available);Assert ($available.Count -eq 0) 'One frame cannot confirm exits'
[ModePulse.ExitMatch]::Confirm($seen,[string[]]@(Match-Exits @('garbled') $pins),$available);Assert ($available.Count -eq 0) 'A misread frame adds nothing'
[ModePulse.ExitMatch]::Confirm($seen,[string[]]@(Match-Exits @('EXFIL01 Crossroads','Transit to Factory') $pins),$available);Assert ($available.Count -eq 2) 'Two matching frames confirm exit and transit'
# The helper drops a capture that finished while the map window was open, and stale results of an earlier raid/map.
$src=Helper-Source
Assert ($src -match 'job\.Recognize==null&&map\.Visible' -and $src -match 'job\.Generation!=exitGeneration\|\|job\.Map!=mapId') 'Stale or map-covered exit reads are discarded'
'PASS: fresh raid scheduling, co-op classification, actual Windows OCR stages off the input loop, two-frame confirmation'
