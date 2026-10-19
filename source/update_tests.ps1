$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../updater/UpdateCore.ps1')
function Assert($ok,$name){if(!$ok){throw $name}}
foreach($f in Get-ChildItem (Join-Path $PSScriptRoot '../updater') -Filter '*.ps1'){$t=$null;$e=$null;$null=[Management.Automation.Language.Parser]::ParseFile($f.FullName,[ref]$t,[ref]$e);Assert ($e.Count -eq 0) ($e|Out-String)}
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$work=Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid().ToString('N'))
# Test ZIPs are made here: name -> contents ('MZ' for an exe).
function New-Zip($entries){$zip=Join-Path $work ([Guid]::NewGuid().ToString()+'.zip');[void][IO.Directory]::CreateDirectory($work);$z=[IO.Compression.ZipFile]::Open($zip,'Create')
 foreach($n in $entries.Keys){$s=New-Object IO.StreamWriter($z.CreateEntry($n).Open());$s.Write($entries[$n]);$s.Dispose()};$z.Dispose();return $zip}
try{
 # Online update layout (what 2.8.x updaters accept): ModePulse/ModePulse.exe is installed as BlueAsh.exe.
 $legacy=New-Zip ([ordered]@{'ModePulse/ModePulse.exe'='MZ new';'ModePulse/README.txt'='readme';'ModePulse/price/PriceWorker.ps1'='w';'ModePulse/price/PriceHelper.dll'='MZ'})
 $plan=@(Get-UpdatePlan $legacy (Join-Path $work 'stage'))
 $install=Join-Path $work 'install';$backup=Join-Path $work 'backup'
 $null=Apply-UpdatePlan $plan $install $backup
 Assert (Test-Path (Join-Path $install 'BlueAsh.exe')) 'install missing'
 Assert (!(Test-Path (Join-Path $install 'ModePulse.exe'))) 'old exe name installed'
 # The ZIP given to users: BlueAsh/BlueAsh.exe and text files only.
 $plan2=@(Get-UpdatePlan (New-Zip ([ordered]@{'BlueAsh/BlueAsh.exe'='MZ';'BlueAsh/LICENSE.txt'='l'})) (Join-Path $work 'stage2'))
 Assert (@($plan2|Where-Object Relative -eq 'BlueAsh.exe').Count -eq 1) 'BlueAsh layout not accepted'
 $test=Join-Path $install 'README.txt';[IO.File]::WriteAllText($test,'old contents')
 $subset=@($plan|Where-Object Relative -eq 'README.txt')
 $subset+= [pscustomobject]@{Relative='missing.txt';Staged=(Join-Path $work 'absent')}
 $failed=$false;try{Apply-UpdatePlan $subset $install (Join-Path $work 'rollback')}catch{$failed=$true}
 Assert $failed 'Expected failed copy';Assert ([IO.File]::ReadAllText($test) -eq 'old contents') 'Rollback did not restore'
 foreach($entries in @([ordered]@{'ModePulse/../evil'='x'},[ordered]@{'ModePulse/ModePulse.exe'='MZ';'ModePulse/BlueAsh.exe'='MZ'},[ordered]@{'Other/BlueAsh.exe'='MZ'},[ordered]@{'BlueAsh/README.txt'='r'})){
  $rejected=$false;try{$null=Get-UpdatePlan (New-Zip $entries) (Join-Path $work ('bad'+[Guid]::NewGuid().ToString('N')))}catch{$rejected=$true};Assert $rejected ('Unsafe ZIP accepted: '+($entries.Keys -join ','))
 }
 # The new version did not start: Restore-UpdatePlan puts replaced files back and removes the ones the update added.
 $old=Join-Path $work 'old';[void][IO.Directory]::CreateDirectory($old);[IO.File]::WriteAllText((Join-Path $old 'BlueAsh.exe'),'MZ old');[IO.File]::WriteAllText((Join-Path $old 'README.txt'),'old readme')
 $changed=Apply-UpdatePlan $plan $old (Join-Path $work 'backup2')
 Assert ([IO.File]::ReadAllText((Join-Path $old 'BlueAsh.exe')) -eq 'MZ new') 'update not applied'
 Assert (Restore-UpdatePlan $changed) 'restore reported a failure'
 Assert ([IO.File]::ReadAllText((Join-Path $old 'BlueAsh.exe')) -eq 'MZ old' -and [IO.File]::ReadAllText((Join-Path $old 'README.txt')) -eq 'old readme') 'old files not restored'
 Assert (!(Test-Path (Join-Path $old 'price/PriceWorker.ps1'))) 'added file not removed'
 'Update tests passed: legacy and BlueAsh layouts, install as BlueAsh.exe, rollback, restore after a failed start, traversal, duplicate exe, missing exe, script parsing.'
}finally{if(Test-Path $work){Remove-Item $work -Recurse -Force}}
