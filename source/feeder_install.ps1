# Experimental file transaction only. It does not determine game compatibility or launch a game.
param(
    [Parameter(Mandatory=$true)][ValidateSet('Install','Restore')][string]$Action,
    [Parameter(Mandatory=$true)][string]$StageDirectory,
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [Parameter(Mandatory=$true)][string]$GameExecutable,
    [Parameter(Mandatory=$true)][string]$ArchiveDirectory
)
$ErrorActionPreference='Stop'
$stage=[IO.Path]::GetFullPath($StageDirectory).TrimEnd('\')
$game=[IO.Path]::GetFullPath($GameDirectory).TrimEnd('\')
$archive=[IO.Path]::GetFullPath($ArchiveDirectory).TrimEnd('\')
foreach($dir in @($game,$archive)) { if(-not (Test-Path -LiteralPath $dir -PathType Container)) { throw ('Directory not found: '+$dir) } }
if($Action -eq 'Install' -and -not (Test-Path -LiteralPath $stage -PathType Container)) {throw ('Directory not found: '+$stage)}
if($stage -ieq $game -or $archive -ieq $game -or $stage -ieq $archive) {throw 'Directories must be distinct.'}
foreach($a in @($stage,$archive)) {if($a.StartsWith($game+'\',[StringComparison]::OrdinalIgnoreCase) -or $game.StartsWith($a+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Staging and archive directories must be outside the game directory.'}}
$exe=[IO.Path]::GetFullPath($GameExecutable)
if(-not $exe.StartsWith($game+'\',[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $exe -PathType Leaf)) {throw 'The game executable must exist inside the game directory.'}
$processName=[IO.Path]::GetFileNameWithoutExtension($exe)
if(Get-Process -Name $processName -ErrorAction SilentlyContinue) {throw 'Close the game before installing or restoring.'}
$journal=Join-Path $archive 'feeder-install.json'
function SafePath([string]$root,[string]$relative) {
    if([string]::IsNullOrWhiteSpace($relative) -or [IO.Path]::IsPathRooted($relative) -or $relative.Contains(':')) {throw 'Invalid file path.'}
    $parts=$relative -split '[\\/]'
    foreach($part in $parts) { if([string]::IsNullOrWhiteSpace($part) -or $part -eq '.' -or $part -eq '..' -or $part.EndsWith('.') -or $part.EndsWith(' ')) {throw 'Invalid file path component.'} }
    $path=[IO.Path]::GetFullPath((Join-Path $root ($parts -join '\')))
    if(-not $path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) {throw 'Path outside target directory.'}
    $parent=Split-Path -Parent $path
    while($parent.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) {
        if(Test-Path -LiteralPath $parent) {if(((Get-Item -LiteralPath $parent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Reparse points are not supported.'}}
        if($parent -ieq $root){break};$parent=Split-Path -Parent $parent
    }
    return $path
}
if($Action -eq 'Install') {
    if(Test-Path -LiteralPath $journal) {throw 'An installation journal already exists.'}
    $data=Get-Content -LiteralPath (Join-Path $stage 'blueash-feeder-stage.json') -Raw | ConvertFrom-Json
    if(-not $data.files -or $data.files.Count -eq 0) {throw 'Staging manifest is empty.'}
    $seen=@{};$rows=@()
    foreach($f in $data.files) {
        $src=SafePath $stage ([string]$f.path);$dst=SafePath $game ([string]$f.path)
        if($seen.ContainsKey($dst.ToLowerInvariant())) {throw 'Duplicate installation target.'};$seen[$dst.ToLowerInvariant()]=$true
        if(Test-Path -LiteralPath $dst) {throw ('Existing game file is preserved: '+$dst)}
        if(-not (Test-Path -LiteralPath $src -PathType Leaf) -or (Get-FileHash -Algorithm SHA256 -LiteralPath $src).Hash -ne $f.sha256) {throw ('Staged file verification failed: '+$src)}
        $rows+=@{path=[string]$f.path;sha256=[string]$f.sha256}
    }
    @{game=$game;files=$rows} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $journal -Encoding UTF8
    foreach($f in $rows) {
        $src=SafePath $stage $f.path;$dst=SafePath $game $f.path;$parent=Split-Path -Parent $dst
        if(-not (Test-Path -LiteralPath $parent)) {New-Item -ItemType Directory -Path $parent -Force | Out-Null}
        if(Test-Path -LiteralPath $dst) {throw ('File appeared during installation: '+$dst)}
        Copy-Item -LiteralPath $src -Destination $dst
        if((Get-FileHash -Algorithm SHA256 -LiteralPath $dst).Hash -ne $f.sha256) {throw ('Copy verification failed: '+$dst)}
    }
} else {
    $data=Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
    if($data.game -ine $game) {throw 'Journal does not match game directory.'}
    $saved=Join-Path $archive 'preserved'
    foreach($f in $data.files) {
        $src=SafePath $game ([string]$f.path);$dst=SafePath $saved ([string]$f.path)
        if(Test-Path -LiteralPath $src) {
            $mutable=@('reshade.ini','reshadepreset.ini') -contains ([string]$f.path).ToLowerInvariant()
            if(-not $mutable -and (Get-FileHash -Algorithm SHA256 -LiteralPath $src).Hash -ne $f.sha256) {throw ('Modified game file is preserved: '+$src)}
            if(Test-Path -LiteralPath $dst) {throw ('Archive file already exists: '+$dst)}
        }
    }
    foreach($f in $data.files) {
        $src=SafePath $game ([string]$f.path);$dst=SafePath $saved ([string]$f.path)
        if(Test-Path -LiteralPath $src) {$parent=Split-Path -Parent $dst;if(-not (Test-Path -LiteralPath $parent)){New-Item -ItemType Directory -Path $parent -Force | Out-Null};Move-Item -LiteralPath $src -Destination $dst}
    }
}
