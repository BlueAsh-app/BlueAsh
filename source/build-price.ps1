# Windows PowerShell 5.1 / .NET Framework 4.8. Build into a unique folder, then overwrite the output.
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$stage=Join-Path $PSScriptRoot ('.bundle/helpers-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($stage)|Out-Null
$root=Join-Path $repo 'price'
$out=Join-Path $stage 'PriceHelper.dll'
Add-Type -Path @((Join-Path $root 'PriceCore.cs'),(Join-Path $root 'PriceWindow.cs'),(Join-Path $root 'PriceCatalog.cs'),(Join-Path $root 'PriceLanguage.cs'),(Join-Path $root 'PaddleOcr.cs')) -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.Net.Http,System.Web.Extensions -OutputAssembly $out -OutputType Library
Copy-Item -LiteralPath $out -Destination (Join-Path $root 'PriceHelper.dll') -Force
'PriceHelper.dll built'
