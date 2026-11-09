# Builds companion/BlueAshHelper.exe (the map, notification, hideout and price helpers) with the .NET Framework 4.8 C#
# compiler that ships with Windows (C# 5). Windows OCR comes from the system's own WinMetadata (no SDK needed).
# Build into a unique folder, then overwrite the output; BlueAsh must not be running the helper (file in use).
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $PSScriptRoot ('.bundle/helpers-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stage)|Out-Null
$fx=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319';$meta=Join-Path $env:WINDIR 'System32\WinMetadata'
$companion=Join-Path $repo 'companion';$price=Join-Path $repo 'price'
$sources=@('CompanionUI.cs','MapSearch.cs','UiFont.cs','CompanionLogic.cs','CompanionApp.cs','CompanionApp.Map.cs','CompanionApp.Hideout.cs','CompanionApp.Net.cs','CompanionApp.Run.cs','CompanionApp.Loop.cs','HelperMain.cs'|ForEach-Object {Join-Path $companion $_})
$sources+=@('PriceCore.cs','PriceWindow.cs','PriceCatalog.cs','PriceLanguage.cs','PaddleOcr.cs','PriceApp.cs'|ForEach-Object {Join-Path $price $_})
$refs=@('System.dll','System.Core.dll','System.Drawing.dll','System.Windows.Forms.dll','System.Net.Http.dll','System.Web.Extensions.dll','System.Security.dll',(Join-Path $fx 'System.Runtime.dll'),(Join-Path $fx 'System.Runtime.WindowsRuntime.dll'))
$refs+=@('Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll'|ForEach-Object {Join-Path $companion $_})
$refs+=@('Windows.Foundation.winmd','Windows.Media.winmd','Windows.Storage.winmd','Windows.Graphics.winmd','Windows.Globalization.winmd'|ForEach-Object {Join-Path $meta $_})
$out=Join-Path $stage 'BlueAshHelper.exe'
$args=@('/nologo','/target:winexe','/platform:x64','/optimize+','/codepage:65001','/utf8output',"/out:$out","/win32icon:$(Join-Path $PSScriptRoot 'assets\app.ico')")+($refs|ForEach-Object {"/r:$_"})+$sources
& (Join-Path $fx 'csc.exe') @args
if($LASTEXITCODE -ne 0){throw "BlueAshHelper.exe build failed ($LASTEXITCODE)"}
Copy-Item -LiteralPath $out -Destination (Join-Path $companion 'BlueAshHelper.exe') -Force
'BlueAshHelper.exe built'
# companion/CompanionUI.dll: the same UI classes as a library for the tests and tools (preview-*.ps1, build-maps.ps1) that
# load them on their own. Not shipped (build.sh leaves it out of the bundle).
$lib=Join-Path $stage 'CompanionUI.dll'
$libArgs=@('/nologo','/target:library','/optimize+','/codepage:65001','/utf8output',"/out:$lib")+(@('System.dll','System.Core.dll','System.Drawing.dll','System.Windows.Forms.dll','System.Net.Http.dll','System.Web.Extensions.dll')+@('Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll'|ForEach-Object {Join-Path $companion $_})|ForEach-Object {"/r:$_"})+@('CompanionUI.cs','MapSearch.cs','UiFont.cs'|ForEach-Object {Join-Path $companion $_})
& (Join-Path $fx 'csc.exe') @libArgs
if($LASTEXITCODE -ne 0){throw "CompanionUI.dll build failed ($LASTEXITCODE)"}
Copy-Item -LiteralPath $lib -Destination (Join-Path $companion 'CompanionUI.dll') -Force
'CompanionUI.dll built'
