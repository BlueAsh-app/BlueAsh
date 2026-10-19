param([string]$Screenshot='')
# Windows-only OCR/WGC integration checks. Keeps all generated files; never deletes user data.
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $PSScriptRoot ('.bundle/deploy-check-'+[Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($stage)
$compiler=Get-Command g++.exe -ErrorAction SilentlyContinue
$gpp=$(if($compiler){$compiler.Source}else{(Get-ChildItem "$env:LOCALAPPDATA/Microsoft/WinGet/Packages" -Directory -Filter 'BrechtSanders.WinLibs.POSIX.UCRT*'|Select-Object -First 1).FullName+'/mingw64/bin/g++.exe'})
function Check([string]$Source,[string[]]$Libraries,[string[]]$Arguments=@()){
 $exe=Join-Path $stage ($Source+'.exe')
 & $gpp -std=c++17 -O2 -Wall -Wextra -Werror (Join-Path $PSScriptRoot ($Source+'.cpp')) @Libraries -o $exe
 if($LASTEXITCODE){throw ('Compile failed: '+$Source)}
 & $exe @Arguments
 if($LASTEXITCODE){throw ('Check failed: '+$Source)}
}
Check 'raid_deploy_tests' @()
Check 'raid_deploy_capture_check' @('-lole32','-luuid','-luser32','-lgdi32','-ld3d11','-ldxgi','-lruntimeobject')
if($Screenshot){Check 'raid_deploy_ocr_check' @('-municode','-lruntimeobject','-lole32','-lgdiplus') @([IO.Path]::GetFullPath($Screenshot))}
