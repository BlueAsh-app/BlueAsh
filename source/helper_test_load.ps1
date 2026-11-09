# Test shim: loads companion/BlueAshHelper.exe (build: source/build-helper.ps1) and gives the tests the helper's logic
# under the names the PowerShell helpers used, so each test checks the code that actually runs.
$helperExe=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../companion/BlueAshHelper.exe'))
if(!(Test-Path $helperExe)){throw 'companion/BlueAshHelper.exe missing: run source/build-helper.ps1'}
Add-Type -AssemblyName System.Windows.Forms,System.Drawing,System.Web.Extensions,System.Net.Http
[void][Reflection.Assembly]::LoadFrom((Resolve-Path $helperExe).Path)
function T([string]$text,[object[]]$values=@()){if($values.Count){[ModePulse.I18n]::Format($text,$values)}else{[ModePulse.I18n]::Text($text)}}
function ConvertTo-Pin($o){if($o -is [ModePulse.Pin]){return $o};$p=New-Object ModePulse.Pin;$p.Name=[string]$o.Name;if($o.Kind){$p.Kind=[string]$o.Kind};return $p}
function Match-Exits($Lines,$Pins){$l=New-Object 'System.Collections.Generic.List[string]';foreach($x in @($Lines)){$l.Add([string]$x)};$p=New-Object 'System.Collections.Generic.List[ModePulse.Pin]';foreach($x in @($Pins)){if($null -ne $x){$p.Add((ConvertTo-Pin $x))}};return @([ModePulse.ExitMatch]::Match($l,$p))}
function Valid-Webhook([string]$Url){[ModePulse.ExitMatch]::ValidWebhook($Url)}
function Classify-Notice([string]$Text){[ModePulse.ExitMatch]::ClassifyNotice($Text)}
function Normalize-Exit([string]$Text){[ModePulse.ExitMatch]::Normalize($Text)}
function Exit-Distance([string]$Line,[string]$Name){[ModePulse.ExitMatch]::Distance($Line,$Name)}
function Convert-EftKey([string]$Name){[ModePulse.EftControl]::ConvertKey($Name)}
function Read-EftExitBinding([string]$Json){[ModePulse.EftControl]::ReadExitBinding($Json)}
# The trigger state lives in the caller's hashtable, as before.
function Update-EftTrigger($State,[bool]$Down,[double]$Now,$Binding){if(!$State.ContainsKey('ts')){$State.ts=New-Object ModePulse.TriggerState};[ModePulse.EftControl]::UpdateTrigger($State.ts,$Down,$Now,$Binding)}
function New-RaidExitSchedule($State,[long]$Tick,[string]$Previous){$s=[ModePulse.RaidExits]::Schedule([string[]]@($State),$Tick,$Previous);if($s){return @{key=$s.Key;until=$s.Until;next=$s.Next}}}
function Get-LogMode([string]$Text){[ModeSwitchPrice.PriceLog]::LogMode($Text)}
function Read-LogMode([string]$Root){[ModeSwitchPrice.PriceLog]::ReadLogMode($Root)}
function Read-RaidPhase([string]$Path,[int]$ParentId){[ModeSwitchPrice.PriceLog]::RaidPhase($Path,$ParentId)}
function Get-PriceState($Item,[string]$Mode){[ModeSwitchPrice.PriceLog]::PriceState($Item,$Mode)}
# companion-settings.json defaults applied once (CompanionConfig.EnableMapDefaults) on a hashtable like the old config.
function Enable-MapDefaults([hashtable]$Config){$c=New-Object ModePulse.CompanionConfig;foreach($k in 'showQuests','showKeys','shotPos','shotDelete'){if($Config.ContainsKey($k)){$c.$k=[bool]$Config[$k]}};if($Config.ContainsKey('mapDefaults')){$c.mapDefaults=[int]$Config.mapDefaults}
 $r=$c.EnableMapDefaults();foreach($k in 'showQuests','showKeys','shotPos','shotDelete','mapDefaults'){$Config[$k]=$c.$k};return $r}
# The helper's own C# sources, for checks on how a feature is wired.
function Helper-Source{(Get-ChildItem (Join-Path $PSScriptRoot '../companion') -Filter '*.cs'|ForEach-Object {[IO.File]::ReadAllText($_.FullName)}) -join "`n"}
function Price-Source{[IO.File]::ReadAllText((Join-Path $PSScriptRoot '../price/PriceApp.cs'))}
