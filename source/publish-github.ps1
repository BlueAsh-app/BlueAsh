# Puts one release on GitHub (https://github.com/BlueAsh-app/BlueAsh): mirrors every file git tracks here into the public
# copy (default Desktop\BlueAsh-public, cloned if missing) without the maintainers' notes, the relay server and
# BlueAsh.exe (same list as source-zip.ps1), commits it as "BlueAsh <version>" with the public author, tags v<version>
# and pushes both, then makes the GitHub Release v<version> with the same setup program the download site serves
# (relay\public\download\BlueAsh-Setup-<version>.exe, built by release.ps1) attached. The tag also starts the GitHub
# Actions build, which only checks that the public source builds; its exe is not the released one. release.ps1 -Promote /
# -Direct call it, so a release reaches the update server, the download site and GitHub together. Pushing uses Git's
# stored GitHub sign-in (Git Credential Manager asks once); the Release uses the GitHub CLI's sign-in (gh auth login,
# once). Safe to run again for the same version: the Release's file and notes are replaced. -WhatIf only shows what
# would change. -ReleaseOnly skips the public copy and the push (the tag must already be on GitHub) and only makes the Release.
param([Parameter(Mandatory=$true)][string]$Version,[string]$Public=(Join-Path ([Environment]::GetFolderPath('Desktop')) 'BlueAsh-public'),[switch]$WhatIf,[switch]$ReleaseOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$repo='https://github.com/BlueAsh-app/BlueAsh.git'
function G([string]$dir){& git.exe -C $dir @args;if($LASTEXITCODE){throw "git $($args -join ' ') 실패 ($LASTEXITCODE)"}}
# gh writes progress and "not found" to stderr, which Windows PowerShell turns into errors under 'Stop'.
$gh=@((Get-Command gh.exe -ErrorAction SilentlyContinue).Source)+@("$env:ProgramFiles\GitHub CLI\gh.exe")|Where-Object {$_ -and (Test-Path -LiteralPath $_)}|Select-Object -First 1
function Gh{$e=$ErrorActionPreference;$ErrorActionPreference='Continue';try{& $gh @args 2>&1|ForEach-Object {"$_"}}finally{$ErrorActionPreference=$e}}
$ghRepo='BlueAsh-app/BlueAsh';$setup=Join-Path $root "relay\public\download\BlueAsh-Setup-$Version.exe"
if(!(Test-Path -LiteralPath $setup)){throw "설치 파일이 없습니다: $setup (release.ps1이 베타 때 만듭니다)"}
if(!$gh){throw 'GitHub CLI(gh)가 없습니다. winget install --id GitHub.cli 후 gh auth login 을 한 번 하세요.'}
if(!$WhatIf){$null=Gh auth status --hostname github.com;if($LASTEXITCODE){throw 'GitHub CLI에 로그인되어 있지 않습니다. 터미널에서 gh auth login 을 한 번 하세요.'}}
if(!$ReleaseOnly){
if(!(Test-Path -LiteralPath (Join-Path $Public '.git'))){& git clone $repo $Public;if($LASTEXITCODE){throw 'GitHub 저장소를 받지 못했습니다.'}}
G $Public config user.name 'BlueAsh';G $Public config user.email '334943974+BlueAsh-app@users.noreply.github.com'
G $Public pull --ff-only origin main
$files=@(& git -C $root -c core.quotepath=off ls-files)|Where-Object{$_ -notmatch '^(AGENTS\.md|CLAUDE\.md|\.claude/|\.codex/|docs/|relay/|brand/|BlueAsh\.exe$|t\.exe$)'}
if($LASTEXITCODE -or $files.Count -lt 50){throw 'git ls-files를 읽지 못했습니다.'}
$keep=@{};foreach($f in $files){$keep[$f]=$true;$src=Join-Path $root $f;$dst=Join-Path $Public $f
 if(!(Test-Path -LiteralPath $src -PathType Leaf)){continue}
 if(!(Test-Path -LiteralPath $dst) -or (Get-FileHash -LiteralPath $src).Hash -ne (Get-FileHash -LiteralPath $dst).Hash){"변경: $f";if(!$WhatIf){[void][IO.Directory]::CreateDirectory((Split-Path $dst));Copy-Item -LiteralPath $src -Destination $dst -Force}}}
# Files no longer in the source leave the public copy too (git keeps them in its history).
foreach($f in @(& git -C $Public -c core.quotepath=off ls-files)){if(!$keep.ContainsKey($f)){"삭제: $f";if(!$WhatIf){G $Public rm -q -- $f}}}
if($WhatIf){$null=Gh release view "v$Version" --repo $ghRepo;"GitHub Release v$Version "+$(if($LASTEXITCODE){'새로 만듦'}else{'파일·설명 바꿈'})+" · $(Split-Path $setup -Leaf)";return}
G $Public add -A
& git -C $Public diff --cached --quiet;if($LASTEXITCODE){G $Public commit -q -m "BlueAsh $Version"}else{'GitHub: 바뀐 파일 없음'}
& git -C $Public rev-parse -q --verify "refs/tags/v$Version" *> $null;if($LASTEXITCODE){G $Public tag -a "v$Version" -m "BlueAsh $Version"}
G $Public push origin main;G $Public push origin "v$Version"
"GitHub: v$Version 올림 (https://github.com/BlueAsh-app/BlueAsh)"}
elseif($WhatIf){return}
# Release notes: the same "what changed" lines as the update server (manifest.json after -Promote/-Direct, else beta.json).
$notes=@();foreach($n in 'manifest.json','beta.json'){$m=Join-Path $root "relay\public\update\$n"
 if(Test-Path -LiteralPath $m){$j=Get-Content -LiteralPath $m -Raw -Encoding UTF8|ConvertFrom-Json;if($j.version -eq $Version){$notes=@(@($j.notes)|Where-Object {$_});break}}}
$sha=(Get-FileHash -LiteralPath $setup -Algorithm SHA256).Hash.ToLowerInvariant();$leaf=Split-Path $setup -Leaf
$body=@("**$leaf** 를 받아 실행하세요 (download and run **$leaf**). 다운로드 사이트 https://blueash.pages.dev 와 같은 파일입니다.",'',"SHA-256: ``$sha``",'')+
 @(if($notes){'### 바뀐 점';$notes|ForEach-Object {"- $_"};''})+
 @('아직 코드 서명이 없어 Windows SmartScreen이나 백신이 경고할 수 있습니다. (Not code-signed yet; SmartScreen or antivirus may warn.)','',
   'Source code (zip / tar.gz) = 이 버전의 공개 소스. 설치에는 필요 없습니다.')
$nf=Join-Path ([IO.Path]::GetTempPath()) "blueash-release-$Version.md";[IO.File]::WriteAllText($nf,($body -join "`n"),(New-Object Text.UTF8Encoding $false))
try{$null=Gh release view "v$Version" --repo $ghRepo
 if($LASTEXITCODE){Gh release create "v$Version" $setup --repo $ghRepo --verify-tag --latest --title "BlueAsh $Version" --notes-file $nf}
 else{Gh release upload "v$Version" $setup --repo $ghRepo --clobber;if(!$LASTEXITCODE){Gh release edit "v$Version" --repo $ghRepo --title "BlueAsh $Version" --notes-file $nf}}
 if($LASTEXITCODE){throw "GitHub Release v$Version 를 만들지 못했습니다 ($LASTEXITCODE). 다시: source/publish-github.ps1 -Version $Version"}}
finally{Remove-Item -LiteralPath $nf -ErrorAction SilentlyContinue}
"GitHub Release: https://github.com/$ghRepo/releases/tag/v$Version ($leaf)"
