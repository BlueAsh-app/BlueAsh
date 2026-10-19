param([string]$Out=(Join-Path $PSScriptRoot 'assets/flare-sign-reference-preview.png'))
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
$b=New-Object Drawing.Bitmap(600,600);$g=[Drawing.Graphics]::FromImage($b)
try{$g.Clear([Drawing.Color]::FromArgb(22,28,39));[ModePulse.FlareMarker]::Draw($g,300,300,390,[ModePulse.ExitSign]::Green);$b.Save($Out,[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$b.Dispose()}
$Out
