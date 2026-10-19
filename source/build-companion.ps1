# Windows PowerShell 5.1 / .NET Framework 4.8. Never remove an existing build.
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $PSScriptRoot ('.bundle/helpers-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stage)|Out-Null
$root=Join-Path $repo 'companion'
$out=Join-Path $stage 'CompanionUI.dll'
Add-Type -Path @((Join-Path $root 'CompanionUI.cs'),(Join-Path $root 'MapSearch.cs')) -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.Core,System.Net.Http,System.Web.Extensions,(Join-Path $root 'Microsoft.Web.WebView2.Core.dll'),(Join-Path $root 'Microsoft.Web.WebView2.WinForms.dll') -OutputAssembly $out -OutputType Library
Copy-Item -LiteralPath $out -Destination (Join-Path $root 'CompanionUI.dll') -Force
'CompanionUI.dll built'
