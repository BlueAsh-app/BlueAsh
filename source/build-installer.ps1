# Makes BlueAsh-Setup-<version>.exe from installer/BlueAsh.iss (run after sh source/build.sh):
#   powershell -ExecutionPolicy Bypass -File source/build-installer.ps1 [-Out <folder>] [-Exe <BlueAsh.exe built elsewhere>]
# Needs Inno Setup 6 (winget install --id JRSoftware.InnoSetup --scope user). Default output: the Desktop.
param([string]$Out=[Environment]::GetFolderPath('Desktop'),[string]$Exe='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'update_ui.h')),'appVersion=L"(\d+\.\d+\.\d+)"').Groups[1].Value
if(!$version){throw 'update_ui.h에서 appVersion을 찾지 못했습니다.'}
$iscc=@("$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe","${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe","$env:ProgramFiles\Inno Setup 6\ISCC.exe")|Where-Object{Test-Path -LiteralPath $_}|Select-Object -First 1
if(!$iscc){throw 'Inno Setup 6이 없습니다: winget install --id JRSoftware.InnoSetup --scope user'}
$exePath=$(if($Exe){(Resolve-Path -LiteralPath $Exe).Path}else{Join-Path $root 'BlueAsh.exe'})
$exeVersion=(Get-Item -LiteralPath $exePath).VersionInfo.ProductVersion
if($exeVersion -ne $version){throw "BlueAsh.exe 버전($exeVersion)이 update_ui.h($version)와 다릅니다. 먼저 sh source/build.sh"}
# Microsoft's WebView2 Runtime bootstrapper: setup runs it only on a PC without the runtime (quest card pictures on the map).
$wv2=Join-Path $root 'installer\MicrosoftEdgeWebview2Setup.exe'
if(!(Test-Path -LiteralPath $wv2)){Invoke-WebRequest -UseBasicParsing 'https://go.microsoft.com/fwlink/p/?LinkId=2124703' -OutFile $wv2}
$sig=Get-AuthenticodeSignature -LiteralPath $wv2;if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation'){Remove-Item -LiteralPath $wv2;throw 'WebView2 설치 도구 서명이 올바르지 않습니다.'}
& $iscc /Qp "/DAppVersion=$version" "/DRoot=$root" "/DExeSource=$exePath" "/DOutputDir=$Out" (Join-Path $root 'installer\BlueAsh.iss')
if($LASTEXITCODE){throw "ISCC 실패 ($LASTEXITCODE)"}
$setup=Join-Path $Out "BlueAsh-Setup-$version.exe"
"$setup ($([Math]::Round((Get-Item -LiteralPath $setup).Length/1MB,1)) MB)"
