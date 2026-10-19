# Makes the ZIP handed to users (run after sh source/build.sh):
#   powershell -ExecutionPolicy Bypass -File source/package.ps1 [-Out <folder>]
# Only what a user needs: BlueAsh.exe (it carries price/companion/maps/updater) and the three text files.
# Nothing from the development folder (source, relay, agent notes) goes in. Default output: the Desktop.
param([string]$Out=[Environment]::GetFolderPath('Desktop'),[string]$Exe='') # -Exe: a build made elsewhere (see release.ps1)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$root=Split-Path $PSScriptRoot -Parent
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'update_ui.h')),'appVersion=L"(\d+\.\d+\.\d+)"').Groups[1].Value
if(!$version){throw 'update_ui.h에서 appVersion을 찾지 못했습니다.'}
$zipPath=Join-Path $Out "BlueAsh-$version.zip"
if(Test-Path -LiteralPath $zipPath){throw "$zipPath 이(가) 이미 있습니다. 옮기거나 지운 뒤 다시 실행하세요."}
$files='BlueAsh.exe','README.txt','LICENSE.txt','NOTICE.txt'
$zip=[IO.Compression.ZipFile]::Open($zipPath,'Create')
try{foreach($f in $files){$src=$(if($f -eq 'BlueAsh.exe'-and $Exe){$Exe}else{Join-Path $root $f});if(!(Test-Path -LiteralPath $src)){throw "$f 없음"};[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$src,'BlueAsh/'+$f,'Optimal')}}finally{$zip.Dispose()}
"$zipPath ($([Math]::Round((Get-Item -LiteralPath $zipPath).Length/1MB,1)) MB)"
