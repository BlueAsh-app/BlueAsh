# Prepare a Feeder package in a NEW directory. This script never changes a game or deletes files.
# Installation and rollback integration are deliberately separate and are not implemented here.
param(
    [Parameter(Mandatory=$true)][string]$StageDirectory,
    [Parameter(Mandatory=$true)][string]$FeederArchive,
    [Parameter(Mandatory=$true)][string]$RhiDirectory,
    [Parameter(Mandatory=$true)][string]$NeuralRuntime,
    [ValidateSet('x86','x64')][string]$Architecture = 'x64',
    [ValidateSet('D3D11','D3D12')][string]$Api = 'D3D11'
)
$ErrorActionPreference = 'Stop'
if ($Architecture -eq 'x86' -and $Api -eq 'D3D12') { throw '32-bit Direct3D 12 is not supported.' }
if (Test-Path -LiteralPath $StageDirectory) { throw 'The staging directory already exists; existing files are preserved.' }
$hash=(Get-FileHash -Algorithm SHA256 -LiteralPath $FeederArchive).Hash
if ($hash -ne '9CC19E28F8FB4E216EA61621FDF6443E207E338EC639979AB335503C75F48F36') { throw 'Unexpected Feeder archive SHA-256.' }
$rhi=[IO.Path]::GetFullPath($RhiDirectory)
$needed=@(
    @{From=(Join-Path $rhi ('reshade\ReShade'+$(if($Architecture -eq 'x86'){'32'}else{'64'})+'.dll'));To='dxgi.dll'},
    @{From=(Join-Path $rhi 'reshade\Shaders\ReShade.fxh');To='reshade-shaders\Shaders\ReShade.fxh'},
    @{From=(Join-Path $rhi 'reshade\Shaders\ReShadeUI.fxh');To='reshade-shaders\Shaders\ReShadeUI.fxh'},
    @{From=(Join-Path $rhi 'reshade\Shaders\CrosireMaster\DrawText.fxh');To='reshade-shaders\Shaders\DrawText.fxh'},
    @{From=(Join-Path $rhi 'reshade\Shaders\LumeniteFX\lumenite_Kernel.fx');To='reshade-shaders\Shaders\lumenite_Kernel.fx'},
    @{From=(Join-Path $rhi 'reshade\Textures\LumeniteFX\lumenite_bluenoise256.png');To='reshade-shaders\Textures\lumenite_bluenoise256.png'}
)
$include=Join-Path $rhi 'reshade\Shaders\LumeniteFX\include'
foreach($n in @('lumenite_ColorManagement.fxh','lumenite_Compute.fxh','lumenite_Helpers.fxh','lumenite_Projections.fxh')) {
    $needed+=@{From=(Join-Path $include $n);To=('reshade-shaders\Shaders\include\'+$n)}
}
$prefix=if($Architecture -eq 'x86'){'host64\'}else{''}
$needed+=@(
    @{From=(Join-Path $rhi 'rdx5\renodx-dlss5.addon64');To=($prefix+'renodx-dlss5.addon64')},
    @{From=(Join-Path $rhi 'DLSS\310.9.16\nvngx_dlss.dll');To=($prefix+'nvngx_dlss.dll')},
    @{From=$NeuralRuntime;To=($prefix+'nvngx_dlssnr.dll')}
)
if($Architecture -eq 'x86') { $needed+=@{From=(Join-Path $rhi 'reshade\ReShade64.dll');To='host64\dxgi.dll'} }
foreach($item in $needed) { if(-not (Test-Path -LiteralPath $item.From -PathType Leaf)) { throw ('Missing component: '+$item.From) } }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip=[IO.Compression.ZipFile]::OpenRead([IO.Path]::GetFullPath($FeederArchive))
try {
    $addon=if($Architecture -eq 'x86'){'dlss5-feed.addon32'}else{'dlss5-feed.addon64'}
    $entries=@{ $addon=$addon; 'reshade-shaders\Shaders\DLSS5_Feed.fx'='reshade-shaders\Shaders\DLSS5_Feed.fx' }
    if($Architecture -eq 'x86') { $entries['host64\dlss5-feed-host64.exe']='host64\dlss5-feed-host64.exe' }
    foreach($name in $entries.Keys) { if(-not $zip.GetEntry($name)) { throw ('Missing archive entry: '+$name) } }
    New-Item -ItemType Directory -Path $StageDirectory | Out-Null
    $stage=[IO.Path]::GetFullPath($StageDirectory)
    foreach($item in $needed) {
        $to=Join-Path $stage $item.To; $parent=Split-Path -Parent $to
        if(-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
        Copy-Item -LiteralPath $item.From -Destination $to
    }
    foreach($name in $entries.Keys) {
        $to=Join-Path $stage $entries[$name]; $parent=Split-Path -Parent $to
        if(-not (Test-Path -LiteralPath $parent)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
        [IO.Compression.ZipFileExtensions]::ExtractToFile($zip.GetEntry($name),$to)
    }
    $reshade="[ADDON]`r`nAddonPath=.\`r`n[GENERAL]`r`nEffectSearchPaths=.\reshade-shaders\Shaders\**`r`nTextureSearchPaths=.\reshade-shaders\Textures\**`r`nPresetPath=.\ReShadePreset.ini`r`n[INPUT]`r`nKeyOverlay=45,0,0,0`r`n[OVERLAY]`r`nTutorialProgress=4`r`n"
    $preset="Techniques=Lumenite_Kernel@lumenite_Kernel.fx,DLSS5_Feed@DLSS5_Feed.fx`r`nTechniqueSorting=Lumenite_Kernel@lumenite_Kernel.fx,DLSS5_Feed@DLSS5_Feed.fx,DLSS5_Feed_Debug@DLSS5_Feed.fx`r`n[DLSS5_Feed.fx]`r`nDEBUG_VIEW=0`r`nMV_SCALE=1.000000`r`nMV_SIGN=1.000000,1.000000`r`nPreprocessorDefinitions=DLSS5_MV_PROVIDER=3`r`n"
    [IO.File]::WriteAllText((Join-Path $stage 'ReShade.ini'),$reshade,[Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $stage 'ReShadePreset.ini'),$preset,[Text.UTF8Encoding]::new($false))
    if($Architecture -eq 'x86') { [IO.File]::WriteAllText((Join-Path $stage 'host64\ReShade.ini'),"[GENERAL]`r`nEffectSearchPaths=.\`r`nTextureSearchPaths=.\`r`n",[Text.UTF8Encoding]::new($false)) }
    $files=Get-ChildItem -LiteralPath $stage -File -Recurse | Sort-Object FullName | ForEach-Object { @{path=$_.FullName.Substring($stage.Length+1);sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash} }
    @{feeder='1.19.0';architecture=$Architecture;api=$Api;files=@($files)} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $stage 'blueash-feeder-stage.json') -Encoding UTF8
} finally { $zip.Dispose() }
