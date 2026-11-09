$ErrorActionPreference = 'Stop'
$root=Join-Path $PSScriptRoot ('bench\feeder-install-test-'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $root 'stage';$game=Join-Path $root 'game';$archive=Join-Path $root 'archive'
New-Item -ItemType Directory -Path $stage,$game,$archive | Out-Null
[IO.File]::WriteAllText((Join-Path $game 'example.exe'),'fixture executable')
$body='fixture';[IO.File]::WriteAllText((Join-Path $stage 'dxgi.dll'),$body)
$hash=(Get-FileHash -LiteralPath (Join-Path $stage 'dxgi.dll') -Algorithm SHA256).Hash
@{files=@(@{path='dxgi.dll';sha256=$hash})} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'blueash-feeder-stage.json') -Encoding UTF8
$installer=Join-Path $PSScriptRoot 'feeder_install.ps1'
$failed=$false;try { & $installer -Action Install -StageDirectory $stage -GameDirectory $game -GameExecutable (Join-Path $root 'outside.exe') -ArchiveDirectory $archive } catch { $failed=$true }
if(-not $failed) {throw 'An executable outside the game directory was not rejected.'}
& $installer -Action Install -StageDirectory $stage -GameDirectory $game -GameExecutable (Join-Path $game 'example.exe') -ArchiveDirectory $archive
if(-not (Test-Path -LiteralPath (Join-Path $game 'dxgi.dll'))) {throw 'Install did not copy the staged file.'}
$failed=$false;try { & $installer -Action Install -StageDirectory $stage -GameDirectory $game -GameExecutable (Join-Path $game 'example.exe') -ArchiveDirectory $archive } catch { $failed=$true }
if(-not $failed) {throw 'Existing installation was not rejected.'}
[IO.File]::WriteAllText((Join-Path $game 'dxgi.dll'),'user modification')
$failed=$false;try { & $installer -Action Restore -StageDirectory $stage -GameDirectory $game -GameExecutable (Join-Path $game 'example.exe') -ArchiveDirectory $archive } catch { $failed=$true }
if(-not $failed) {throw 'Modified file was not preserved.'}
if([IO.File]::ReadAllText((Join-Path $game 'dxgi.dll')) -ne 'user modification') {throw 'Modified file was altered.'}
$game2=Join-Path $root 'game2';$archive2=Join-Path $root 'archive2'
New-Item -ItemType Directory -Path $game2,$archive2 | Out-Null
[IO.File]::WriteAllText((Join-Path $game2 'example.exe'),'fixture executable')
& $installer -Action Install -StageDirectory $stage -GameDirectory $game2 -GameExecutable (Join-Path $game2 'example.exe') -ArchiveDirectory $archive2
& $installer -Action Restore -StageDirectory (Join-Path $root 'missing-stage') -GameDirectory $game2 -GameExecutable (Join-Path $game2 'example.exe') -ArchiveDirectory $archive2
if(Test-Path -LiteralPath (Join-Path $game2 'dxgi.dll')) {throw 'Restored file remains in the game directory.'}
if([IO.File]::ReadAllText((Join-Path $archive2 'preserved\dxgi.dll')) -ne $body) {throw 'Restored file was not preserved.'}
$badStage=Join-Path $root 'bad-stage';$game3=Join-Path $root 'game3';$archive3=Join-Path $root 'archive3'
New-Item -ItemType Directory -Path $badStage,$game3,$archive3 | Out-Null
[IO.File]::WriteAllText((Join-Path $game3 'example.exe'),'fixture executable')
@{files=@(@{path='..\outside.dll';sha256=$hash})} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $badStage 'blueash-feeder-stage.json') -Encoding UTF8
$failed=$false;try { & $installer -Action Install -StageDirectory $badStage -GameDirectory $game3 -GameExecutable (Join-Path $game3 'example.exe') -ArchiveDirectory $archive3 } catch { $failed=$true }
if(-not $failed) {throw 'Path traversal was not rejected.'}
if(Test-Path -LiteralPath (Join-Path $archive3 'feeder-install.json')) {throw 'Invalid request created a journal.'}
$inside=Join-Path $game3 'nested-archive';New-Item -ItemType Directory -Path $inside | Out-Null
$failed=$false;try { & $installer -Action Install -StageDirectory $stage -GameDirectory $game3 -GameExecutable (Join-Path $game3 'example.exe') -ArchiveDirectory $inside } catch { $failed=$true }
if(-not $failed) {throw 'Archive inside the game directory was not rejected.'}
$stage4=Join-Path $root 'stage4';$game4=Join-Path $root 'game4';$archive4=Join-Path $root 'archive4'
New-Item -ItemType Directory -Path $stage4,$game4,$archive4 | Out-Null
[IO.File]::WriteAllText((Join-Path $game4 'example.exe'),'fixture executable');[IO.File]::WriteAllText((Join-Path $stage4 'ReShade.ini'),'original configuration')
$hash4=(Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $stage4 'ReShade.ini')).Hash
@{files=@(@{path='ReShade.ini';sha256=$hash4})} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage4 'blueash-feeder-stage.json') -Encoding UTF8
& $installer -Action Install -StageDirectory $stage4 -GameDirectory $game4 -GameExecutable (Join-Path $game4 'example.exe') -ArchiveDirectory $archive4
[IO.File]::WriteAllText((Join-Path $game4 'ReShade.ini'),'changed configuration')
& $installer -Action Restore -StageDirectory $stage4 -GameDirectory $game4 -GameExecutable (Join-Path $game4 'example.exe') -ArchiveDirectory $archive4
if([IO.File]::ReadAllText((Join-Path $archive4 'preserved\ReShade.ini')) -ne 'changed configuration') {throw 'Changed configuration was not preserved.'}
'Feeder installation safety tests passed.'
