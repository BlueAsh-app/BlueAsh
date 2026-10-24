# Puts one release on GitHub (https://github.com/BlueAsh-app/BlueAsh): mirrors every file git tracks here into the public
# copy (default Desktop\BlueAsh-public, cloned if missing) without the maintainers' notes, the relay server and
# BlueAsh.exe (same list as source-zip.ps1), commits it as "BlueAsh <version>" with the public author, tags v<version>
# and pushes both. The tag starts the GitHub Actions build. release.ps1 -Promote / -Direct call it, so a release reaches
# the update server, the download site and GitHub together. Pushing uses Git's stored GitHub sign-in (Git Credential
# Manager asks once). -WhatIf only shows what would change.
param([Parameter(Mandatory=$true)][string]$Version,[string]$Public=(Join-Path ([Environment]::GetFolderPath('Desktop')) 'BlueAsh-public'),[switch]$WhatIf)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent;$repo='https://github.com/BlueAsh-app/BlueAsh.git'
function G([string]$dir){& git.exe -C $dir @args;if($LASTEXITCODE){throw "git $($args -join ' ') 실패 ($LASTEXITCODE)"}}
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
if($WhatIf){return}
G $Public add -A
& git -C $Public diff --cached --quiet;if($LASTEXITCODE){G $Public commit -q -m "BlueAsh $Version"}else{'GitHub: 바뀐 파일 없음'}
& git -C $Public rev-parse -q --verify "refs/tags/v$Version" *> $null;if($LASTEXITCODE){G $Public tag -a "v$Version" -m "BlueAsh $Version"}
G $Public push origin main;G $Public push origin "v$Version"
"GitHub: v$Version 올림 (https://github.com/BlueAsh-app/BlueAsh)"
