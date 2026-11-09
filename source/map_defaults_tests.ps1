$ErrorActionPreference='Stop'
function Assert($ok,$why){if(!$ok){throw $why}}
. (Join-Path $PSScriptRoot 'helper_test_load.ps1')
$fresh=New-Object ModePulse.CompanionConfig
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){Assert ($fresh.$key -eq $true) ('New install default: '+$key)}
Assert ($fresh.mapKey -eq 77 -and $fresh.idle -eq 20 -and !$fresh.discord) 'Other defaults (M key, 20 s, alerts off)'
$legacy=@{showQuests=$false;showKeys=$false;shotPos=$false;shotDelete=$false;mapKey=75;discord=$false;idle=47;webhook='keep-secret';phoneCode='keep-phone'}
Assert (Enable-MapDefaults $legacy) 'Existing settings migrate once'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){Assert ($legacy[$key] -eq $true) ('Migrated setting: '+$key)}
Assert ($legacy.mapKey -eq 75 -and !$legacy.discord -and $legacy.idle -eq 47 -and $legacy.webhook -eq 'keep-secret' -and $legacy.phoneCode -eq 'keep-phone') 'Unrelated preferences and chosen map key are preserved'
Assert ($legacy.mapDefaults -eq 1) 'Migration completion is persisted with the settings'
# The real settings file: written by the old PowerShell helper (ConvertTo-Json, UTF-8 with BOM), read and saved by the new one.
$fixture=Join-Path $PSScriptRoot ('.bundle/map-defaults-tests-'+[Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($fixture)
$path=Join-Path $fixture 'companion-settings.json'
@{showQuests=$false;showKeys=$false;shotPos=$false;shotDelete=$false;mapKey=75;discord=$true;idle=47;webhook='keep-secret';phoneCode='keep-phone';mapDefaults=0;maps=$true;maintenance=$false;delay=1100;exitKey=79;relayKey='';mapKeyIgnore=''}|ConvertTo-Json|Set-Content $path -Encoding UTF8
$c=[ModePulse.CompanionConfig]::Load($path)
Assert ($c.mapKey -eq 75 -and $c.discord -and $c.idle -eq 47 -and $c.webhook -eq 'keep-secret' -and $c.phoneCode -eq 'keep-phone' -and !$c.showKeys) 'Old PowerShell settings read as they were'
Assert ($c.EnableMapDefaults()) 'The upgrade migrates once';$c.Save($path)
$saved=Get-Content $path -Raw -Encoding UTF8|ConvertFrom-Json
Assert ($saved.mapDefaults -eq 1 -and $saved.showQuests -and $saved.showKeys -and $saved.shotPos -and $saved.shotDelete -and $saved.mapKey -eq 75 -and $saved.webhook -eq 'keep-secret' -and $saved.idle -eq 47) 'Saved JSON reads back in PowerShell with every value'
Assert ([IO.File]::ReadAllBytes($path)[0] -eq 0xEF) 'Saved with a BOM like before (BlueAsh.exe reads the phone code from it)'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){$saved.$key=$false};$saved|ConvertTo-Json|Set-Content $path -Encoding UTF8
$c=[ModePulse.CompanionConfig]::Load($path)
Assert (!$c.EnableMapDefaults()) 'Restart or a later version must not migrate again'
foreach($key in 'showQuests','showKeys','shotPos','shotDelete'){Assert ($c.$key -eq $false) ('User can opt out permanently: '+$key)}
$c.mapDefaults=2;Assert (!$c.EnableMapDefaults()) 'Future migration revisions must not be downgraded'
$src=Helper-Source
Assert ($src.Contains('if(config.EnableMapDefaults()){SetReg("MapScreenshotKey",((char)config.mapKey).ToString());config.Save(settingsPath);}')) 'The migration runs when the helper starts and turns on the fifth switch with the chosen map key'
'PASS: all map defaults, one-time migration, retained opt-outs, unrelated settings, old settings file, future revision'
