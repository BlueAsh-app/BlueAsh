# GPL-3.0 corresponding source of one release: every file git tracks, as it is in the working tree the release was built
# from, without the maintainers' working notes (AGENTS.md, CLAUDE.md, .claude/, .codex/, docs/), the relay server (it is
# not part of the program users get) and BlueAsh.exe itself. Written to <Out>/BlueAsh-<version>-source.zip and to
# <Out>/BlueAsh-source-latest.zip; release.ps1 calls it so the relay serves the source next to every update
# (NOTICE.txt, README.txt and the download page link it). Build steps for readers: source/BUILDING.txt.
param([Parameter(Mandatory=$true)][string]$Version,[Parameter(Mandatory=$true)][string]$Out)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$root=Split-Path $PSScriptRoot -Parent
$files=@(& git -C $root -c core.quotepath=off ls-files)|Where-Object{$_ -notmatch '^(AGENTS\.md|CLAUDE\.md|\.claude/|\.codex/|docs/|relay/|brand/|BlueAsh\.exe$|t\.exe$)'}
if($LASTEXITCODE -or $files.Count -lt 50){throw 'git ls-files를 읽지 못했습니다.'}
[void][IO.Directory]::CreateDirectory($Out);$target=Join-Path $Out "BlueAsh-$Version-source.zip";if(Test-Path -LiteralPath $target){Remove-Item -LiteralPath $target}
$zip=[IO.Compression.ZipFile]::Open($target,'Create')
try{foreach($f in $files){$src=Join-Path $root $f;if(Test-Path -LiteralPath $src -PathType Leaf){[void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$src,"BlueAsh-$Version-source/$f",'Optimal')}}}finally{$zip.Dispose()}
$size=(Get-Item -LiteralPath $target).Length
# Cloudflare serves static files up to 25 MiB.
if($size -gt 25MB){Remove-Item -LiteralPath $target;throw "소스 ZIP이 $([Math]::Round($size/1MB,1)) MB로 25 MB 제한을 넘습니다."}
Copy-Item -LiteralPath $target -Destination (Join-Path $Out 'BlueAsh-source-latest.zip') -Force
"소스: $target ($([Math]::Round($size/1MB,1)) MB, 파일 $($files.Count)개)"
