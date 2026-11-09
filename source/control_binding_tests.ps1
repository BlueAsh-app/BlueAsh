$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'helper_test_load.ps1')
function Assert($ok,$label){if(!$ok){throw $label}}
function Fixture($key,$type='Press',$timeout=.3){return @{DoubleClickTimeout=$timeout;keyBindings=@(@{keyName='WatchTimerAndExits';pressType=$type;variants=@(@{keyCode=@($key)},@{keyCode=@()})})}|ConvertTo-Json -Depth 8}
$b=Read-EftExitBinding (Fixture 'F4');Assert ($b.Variants[0].Codes[0] -eq 115) 'F4'
$s=@{};Assert (!(Update-EftTrigger $s $false 0 $b)) 'initial';Assert (Update-EftTrigger $s $true 100 $b) 'press';Assert (!(Update-EftTrigger $s $true 120 $b)) 'held no repeat'
$b=Read-EftExitBinding (Fixture 'O' 'DoubleClick' .3);$s=@{};$null=Update-EftTrigger $s $false 0 $b;Assert (!(Update-EftTrigger $s $true 100 $b)) 'first tap';$null=Update-EftTrigger $s $false 150 $b;Assert (Update-EftTrigger $s $true 350 $b) 'second tap inside game timeout';$null=Update-EftTrigger $s $false 400 $b;Assert (!(Update-EftTrigger $s $true 450 $b)) 'third tap no triple trigger'
$b=Read-EftExitBinding (Fixture 'F4' 'Release');$s=@{};$null=Update-EftTrigger $s $false 0 $b;Assert (!(Update-EftTrigger $s $true 10 $b)) 'release not press';Assert (Update-EftTrigger $s $false 20 $b) 'release'
$b=Read-EftExitBinding (Fixture @('LeftControl','F4'));Assert ($b.Variants[0].Codes.Count -eq 2) 'chord'
foreach($json in @((Fixture 'Mouse5'),(Fixture 'O' 'Continuous'),'{}','{bad')){$failed=$false;try{$null=Read-EftExitBinding $json}catch{$failed=$true};Assert $failed 'unsupported must fail closed'}
'PASS: F4, chords, press/release/double-tap timing, no repeat, unsupported inputs'
