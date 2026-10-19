$ErrorActionPreference='Stop'
function Assert($ok,$why){if(!$ok){throw $why}}
$repo=Split-Path $PSScriptRoot -Parent
$text=[IO.File]::ReadAllText($repo+'/companion/Companion.ps1')
$assignment=[regex]::Match($text,'(?m)^\$config=@\{[^\r\n]+').Value
Assert ($assignment.Length -gt 0) 'The helper supplies its initial settings'
Invoke-Expression $assignment
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){Assert ($config[$key] -eq $true) ('New install default: '+$key)}
. ($repo+'/companion/CompanionCore.ps1')
Assert ([bool](Get-Command Enable-MapDefaults -ErrorAction SilentlyContinue)) 'Existing users need a one-time map defaults migration'
$legacy=@{showQuests=$false;showKeys=$false;shotPos=$false;shotDelete=$false;mapKey=75;discord=$false;idle=47;webhook='keep-secret';phoneCode='keep-phone'}
Assert (Enable-MapDefaults $legacy) 'Existing settings migrate once'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){Assert ($legacy[$key] -eq $true) ('Migrated setting: '+$key)}
Assert ($legacy.mapKey -eq 75 -and !$legacy.discord -and $legacy.idle -eq 47 -and $legacy.webhook -eq 'keep-secret' -and $legacy.phoneCode -eq 'keep-phone') 'Unrelated preferences and chosen map key are preserved'
Assert ($legacy.mapDefaults -eq 1) 'Migration completion is persisted with the settings'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){$legacy[$key]=$false}
$reloaded=@{};($legacy|ConvertTo-Json|ConvertFrom-Json).PSObject.Properties|ForEach-Object {$reloaded[$_.Name]=$_.Value}
Assert (!(Enable-MapDefaults $reloaded)) 'Restart or a later version must not migrate again'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){Assert ($reloaded[$key] -eq $false) ('User can opt out permanently: '+$key)}
$reloaded.mapDefaults=2;Assert (!(Enable-MapDefaults $reloaded)) 'Future migration revisions must not be downgraded'
Assert ($text -match 'if\(Enable-MapDefaults \$config\)') 'The migration runs when the helper starts'
'PASS: all map defaults, one-time migration, retained opt-outs, unrelated settings, future revision'

# Execute the actual startup statements with isolated JSON and an in-memory registry boundary.
$fixture=Join-Path $PSScriptRoot ('.bundle/map-defaults-tests-'+[Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($fixture)
$settingsPath=Join-Path $fixture 'settings.json';$legacy.mapDefaults=0;$legacy|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8
$script:shotKey='';$script:registryWrites=0
function Test-Path($Path){if($Path -eq 'HKCU:\Software\ModeSwitch'){return $true};Microsoft.PowerShell.Management\Test-Path -LiteralPath $Path}
function Set-ItemProperty($Path,$Name,$Value){Assert ($Path -eq 'HKCU:\Software\ModeSwitch' -and $Name -eq 'MapScreenshotKey') 'Only the map screenshot binding is changed';$script:shotKey=$Value;$script:registryWrites++}
$tokens=$null;$parseErrors=$null;$ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$parseErrors)
$startup=($ast.EndBlock.Statements|Where-Object {$_.Extent.Text.StartsWith('$config=@{') -or $_.Extent.Text.StartsWith('if(Test-Path $settingsPath)') -or $_.Extent.Text.StartsWith('if(Enable-MapDefaults $config)')}|ForEach-Object {$_.Extent.Text}) -join [Environment]::NewLine
Invoke-Expression $startup
Assert ($script:shotKey -eq 'K' -and $script:registryWrites -eq 1) 'The fifth checkbox is enabled using the existing chosen map key'
$saved=Get-Content $settingsPath -Raw -Encoding UTF8|ConvertFrom-Json
Assert ($saved.mapDefaults -eq 1 -and $saved.showQuests -and $saved.showKeys -and $saved.shotPos -and $saved.shotDelete) 'All four JSON checkboxes and completion marker are saved before the watcher starts'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){$saved.$key=$false};$saved|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8;$script:shotKey=''
Invoke-Expression $startup
Assert (!$config.showQuests -and !$config.showKeys -and !$config.shotPos -and !$config.shotDelete -and $script:shotKey -eq '' -and $script:registryWrites -eq 1) 'An upgrade restart preserves all five user opt-outs without rewriting the binding'
$settingsPath=Join-Path $fixture 'fresh.json';Invoke-Expression $startup
Assert ($script:shotKey -eq 'M' -and $config.showKeys -and $config.shotDelete) 'Fresh installs persist all defaults and use the default M key'
'PASS: real startup JSON persistence, all five checkbox defaults, selected binding, restart opt-outs; fixtures retained'
