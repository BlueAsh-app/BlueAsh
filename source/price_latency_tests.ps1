$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
Add-Type -Path ($repo+'/price/PriceHelper.dll');[ModeSwitchPrice.Language]::Code='ko'
. ($repo+'/price/PriceCore.ps1')
function Assert($ok,$why){if(!$ok){throw $why}}
$t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile(($repo+'/price/PriceWorker.ps1'),[ref]$t,[ref]$e)
Assert (!$e.Count) 'Worker syntax'
Invoke-Expression ($ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Show-Price'},$true).Extent.Text)
function P($s,$v){if($v){return $s -f $v};return $s};function Status($s){};function Covered($s){return $false}
$script:historyReads=0
function Done-Quests($mode){$script:historyReads++;Start-Sleep -Milliseconds 700;return $null}
function Hideout-Levels($mode){return $null};function Edition-Skip{return $null}
$watch=[Diagnostics.Stopwatch]::StartNew();$script:keyAt=-1
function Tag{$v=New-Object PSObject;$v|Add-Member ScriptMethod Hide {};$v|Add-Member ScriptMethod ShowAt {};$v|Add-Member ScriptMethod ShowLines {};return $v}
$key=Tag;$key|Add-Member ScriptMethod ShowFor {$script:keyAt=$watch.ElapsedMilliseconds}
$catalog=New-Object PSObject;$catalog|Add-Member ScriptMethod DisplayName {return 'test key'}
$task=New-Object PSObject;$task|Add-Member ScriptMethod RefreshIfStale {};$task|Add-Member ScriptMethod For {return ,([string[]]@())};$task|Add-Member ScriptMethod Hideout {return ,@()}
$history=New-Object PSObject -Property @{Ready=$false};$history|Add-Member ScriptMethod RefreshIfStale {}
$histories=@{pve=$history};$catalogs=@{pve=$catalog};$tasks=@{pve=$task};$taskAge=[TimeSpan]::FromHours(6);$keysPath='unused'
$hit=@{item=(New-Object ModeSwitchPrice.Item -Property @{Id='test';Name='test';Mode='pve';Price=5;Offers=1;Category='keys'});mode='pve';th=20;left=100;right=500;top=100;bottom=600}
$slot=@{price=(Tag);quest=(Tag);hide=(Tag);key=$key;z=0}
$watch.Restart();Show-Price $hit $slot
Assert ($script:keyAt -ge 0 -and $script:keyAt -lt 200) ('Key button must not wait for cold quest history; was '+$script:keyAt+' ms')
Assert ($script:historyReads -eq 0) 'Pending quest history must not be read synchronously by inspect display'
$history.Ready=$true;$watch.Restart();Show-Price $hit $slot
Assert ($script:keyAt -lt 200 -and $script:historyReads -eq 1) 'Show key first; include quest metadata when its snapshot is ready'
Write-Output 'PASS: pending history never delays price/key overlay; ready metadata retained; key shown before optional tags'
