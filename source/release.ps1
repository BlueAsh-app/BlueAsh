# Publishes a built BlueAsh.exe as an update, beta first (run after sh source/build.sh):
#   powershell -ExecutionPolicy Bypass -File source/release.ps1 -Notes '바뀐 점 1|바뀐 점 2'   -> beta (beta.json)
#   powershell -ExecutionPolicy Bypass -File source/release.ps1 -Promote                       -> everyone (manifest.json)
# Beta reaches only installs with registry HKCU\Software\ModeSwitch UpdateChannel=beta (the developer's PC): update
# there, check the build, then -Promote copies beta.json to manifest.json. -Direct skips beta (emergency fixes only).
# Before anything is uploaded the exe checks itself (BlueAsh.exe --selftest: unpacks its bundle into a scratch folder,
# parses every script, loads both DLLs) and the version must be newer than what is already published.
# Makes ModePulse-<version>.zip (the layout updater/UpdateCore.ps1 accepts), splits it into 8 MiB parts under
# relay/public/update/<version>/ (static assets are limited to 25 MiB a file), writes the channel file and deploys
# the relay. Older version folders are left in place.
# -Promote and -Direct also put the release on GitHub (publish-github.ps1: public copy, commit, tag v<version>, GitHub Release with the setup .exe) unless
# -NoGitHub or -NoDeploy; the download site (blueash.pages.dev) reads check.txt, so it follows the relay by itself.
# -Exe: publish a build made elsewhere (BLUEASH_OUT=... sh source/build.sh), e.g. while the running copy locks BlueAsh.exe.
param([string[]]$Notes=@(),[switch]$NoDeploy,[string]$Exe='',[switch]$Promote,[switch]$Direct,[switch]$NoGitHub)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$root=Split-Path $PSScriptRoot -Parent
$updates=Join-Path $root 'relay\public\update'
$stablePath=Join-Path $updates 'manifest.json';$betaPath=Join-Path $updates 'beta.json'
function Deploy{CheckFiles;if($NoDeploy){return}
 # The OCR models in relay/public/ocr and the PUBG deep-zoom maps in relay/public/pubg are git-ignored: deploying without them would remove them from the server.
 if(!(Test-Path (Join-Path $root 'relay/public/ocr/manifest.json'))){throw 'relay/public/ocr가 없습니다. 먼저 node source/ocr-models.mjs 를 실행하세요.'};if(!(Test-Path (Join-Path $root 'relay/public/pubg/manifest.txt'))){throw 'relay/public/pubg가 없습니다. 먼저 powershell -File source/build-pubg-hires.ps1 을 실행하세요.'};Push-Location (Join-Path $root 'relay');try{& npx --yes wrangler@4 deploy;if($LASTEXITCODE){throw 'wrangler deploy 실패'}}finally{Pop-Location}}
 # The data pack (source/build-datapack.ps1; git-ignored too) must be there, or the deploy removes the one this build downloads.
 $pack=[regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'datapack_id.h')),'DATAPACK_FILE L"([^"]+)"').Groups[1].Value;if(!$pack -or !(Test-Path (Join-Path $root "relay/public/data/$pack"))){throw "relay/public/data/$pack 가 없습니다. 먼저 powershell -File source/build-datapack.ps1 을 실행하세요."}
# The app's regular update check reads these static files (served without running the worker, so free and unlimited):
# check.txt = manifest.json, check-beta.txt = beta.json (or manifest.json without one), in /update/check's key=value text.
function CheckText($path){$m=Get-Content -LiteralPath $path -Raw -Encoding UTF8|ConvertFrom-Json;$l=@("version=$($m.version)","date=$($m.date)","size=$($m.size)")+@(if(Test-Path -LiteralPath (Join-Path $root "relay\public\download\BlueAsh-Setup-$($m.version).exe")){"setup=/download/BlueAsh-Setup-$($m.version).exe"})+@(@($m.notes)|Where-Object {$_}|ForEach-Object {'note='+($_ -replace "[`r`n]+",' ')});return ($l -join "`n")+"`n"}
function CheckFiles{$u=New-Object Text.UTF8Encoding($false);if(Test-Path -LiteralPath $stablePath){[IO.File]::WriteAllText((Join-Path $updates 'check.txt'),(CheckText $stablePath),$u)}
 $b=$(if(Test-Path -LiteralPath $betaPath){$betaPath}else{$stablePath});if(Test-Path -LiteralPath $b){[IO.File]::WriteAllText((Join-Path $updates 'check-beta.txt'),(CheckText $b),$u)}}
function Published($path){if(Test-Path -LiteralPath $path){return [version](Get-Content -LiteralPath $path -Raw -Encoding UTF8|ConvertFrom-Json).version};return [version]'0.0.0'}

function GitHub($v){if($NoDeploy -or $NoGitHub){return};& (Join-Path $PSScriptRoot 'publish-github.ps1') -Version $v}

if($Promote){
 if(!(Test-Path -LiteralPath $betaPath)){throw '베타로 올린 버전이 없습니다. 먼저 release.ps1 -Notes ... 로 베타를 올리세요.'}
 $beta=Get-Content -LiteralPath $betaPath -Raw -Encoding UTF8|ConvertFrom-Json
 foreach($p in @($beta.parts)){if(!(Test-Path -LiteralPath (Join-Path $updates $p))){throw "베타 조각 파일이 없습니다: $p"}}
 if([version]$beta.version -le (Published $stablePath)){throw "베타($($beta.version))가 지금 배포된 버전보다 새롭지 않습니다."}
 Copy-Item -LiteralPath $betaPath -Destination $stablePath -Force;Remove-Item -LiteralPath $betaPath
 "BlueAsh $($beta.version) → 모두에게 배포"
 Deploy;GitHub $beta.version;return
}

# -File hands the list over as one string, so changes are separated by |.
$Notes=@($Notes|ForEach-Object {$_ -split '\|'}|ForEach-Object {$_.Trim()}|Where-Object {$_})
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'update_ui.h')),'appVersion=L"(\d+\.\d+\.\d+)"').Groups[1].Value
if(!$version){throw 'update_ui.h에서 appVersion을 찾지 못했습니다.'}
$exe=$(if($Exe){$Exe}else{Join-Path $root 'BlueAsh.exe'})
$vi=[Diagnostics.FileVersionInfo]::GetVersionInfo($exe);$fileVersion="$($vi.FileMajorPart).$($vi.FileMinorPart).$($vi.FileBuildPart)"
if($fileVersion -ne $version){throw "BlueAsh.exe 파일 버전($fileVersion)이 appVersion($version)과 다릅니다. 버전을 올린 뒤 다시 빌드하세요."}
$latest=@((Published $stablePath),(Published $betaPath)|Sort-Object)[-1]
if([version]$version -le (Published $stablePath)){throw "$version 은(는) 이미 배포된 $(Published $stablePath) 보다 새롭지 않습니다. 버전을 올리세요."}
if(!$Direct -and [version]$version -lt $latest){throw "$version 은(는) 베타 $latest 보다 오래된 버전입니다."}

# Self-test of the exact exe being published, in a scratch folder (the running BlueAsh's own files stay untouched).
$check=Join-Path ([IO.Path]::GetTempPath()) ('blueash-selftest-'+[Guid]::NewGuid().ToString('N'))
$p=Start-Process -FilePath $exe -ArgumentList ('--selftest "'+$check+'"') -Wait -PassThru
$report=Join-Path $check 'selftest.txt';$detail=$(if(Test-Path -LiteralPath $report){(Get-Content -LiteralPath $report -Encoding UTF8) -join ' / '}else{'(보고 없음)'})
Remove-Item -LiteralPath $check -Recurse -Force -ErrorAction SilentlyContinue
if($p.ExitCode -ne 0){throw "자체 점검 실패(코드 $($p.ExitCode)): $detail — 배포하지 않았습니다."}
"자체 점검 통과: $detail"

$out=Join-Path $updates $version;[void][IO.Directory]::CreateDirectory($out)
$zipPath=Join-Path ([IO.Path]::GetTempPath()) "ModePulse-$version-$([Guid]::NewGuid().ToString('N')).zip"
# Entries are written by hand: ModePulse/<path> with forward slashes. The online ZIP keeps the pre-rename layout
# (ModePulse/ModePulse.exe) because 2.8.x updaters accept nothing else; BlueAsh's updater installs it as BlueAsh.exe and
# an exe that starts as ModePulse.exe renames itself. price/PriceWorker.ps1 and PriceHelper.dll are what 2.8.1's
# updater insists on; the exe carries everything else.
$files='BlueAsh.exe','README.txt','NOTICE.txt','LICENSE.txt','price/PriceWorker.ps1','price/PriceHelper.dll'
$zip=[IO.Compression.ZipFile]::Open($zipPath,'Create')
try{foreach($f in $files){$src=$(if($f -eq 'BlueAsh.exe'){$exe}else{Join-Path $root $f});if(!(Test-Path -LiteralPath $src)){throw "$f 없음"};[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$src,'ModePulse/'+$(if($f -eq 'BlueAsh.exe'){'ModePulse.exe'}else{$f}),'Optimal')}}finally{$zip.Dispose()}
$bytes=[IO.File]::ReadAllBytes($zipPath);Remove-Item -LiteralPath $zipPath
$sha=[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($bytes)).Replace('-','')
$partSize=8MB;$parts=@()
for($i=0;$i*$partSize -lt $bytes.Length;$i++){$n=[Math]::Min($partSize,$bytes.Length-$i*$partSize);$chunk=New-Object byte[] $n;[Array]::Copy($bytes,$i*$partSize,$chunk,0,$n);[IO.File]::WriteAllBytes((Join-Path $out "part$i"),$chunk);$parts+="$version/part$i"}
$manifest=[ordered]@{version=$version;date=(Get-Date -Format 'yyyy-MM-dd');notes=@($Notes);size=$bytes.Length;sha256=$sha;url="https://modepulse-relay.modepulse.workers.dev/update/BlueAsh-$version.zip";parts=$parts}
$target=$(if($Direct){$stablePath}else{$betaPath})
[IO.File]::WriteAllText($target,($manifest|ConvertTo-Json -Depth 4),(New-Object Text.UTF8Encoding($false)))
if($Direct-and(Test-Path -LiteralPath $betaPath)){Remove-Item -LiteralPath $betaPath}
"BlueAsh $version · $([Math]::Round($bytes.Length/1MB,1)) MB · $($parts.Count) parts · sha256 $sha · "+$(if($Direct){'모두에게 바로 배포'}else{'베타로 배포 (확인 뒤 release.ps1 -Promote)'})
# GPL-3.0: the source of this exact build goes out with it (relay/public/source, linked from NOTICE.txt and README.txt).
# The download site offers the setup program (an .exe), not the update ZIP (which keeps the old ModePulse layout for 2.8.x updaters).
& (Join-Path $PSScriptRoot 'build-installer.ps1') -Out (Join-Path $root 'relay\public\download') -Exe $exe
& (Join-Path $PSScriptRoot 'source-zip.ps1') -Version $version -Out (Join-Path $root 'relay\public\source')
Deploy
if($Direct){GitHub $version}
