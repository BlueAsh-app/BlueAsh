$ErrorActionPreference='Stop'
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
$html='<h2><span id="Key_Location">Key Location</span></h2><ul><li>In Jackets</li></ul><h3>Customs</h3><p>Inside a jacket. Guaranteed spawn every raid.</p><ul class="gallery"><li>Photo caption</li></ul><h2>Lock Location</h2><p>Not an acquisition location.</p>'
$text=[ModePulse.KeyWiki]::LocationText($html)
if($text -notmatch 'In Jackets' -or $text -notmatch 'Customs' -or $text -notmatch 'Guaranteed spawn' -or $text -match 'Photo caption|Not an acquisition location'){throw "Incorrect key location: $text"}
if([ModePulse.KeyWiki]::LocationText('<h2>Other</h2><p>No location.</p>') -ne ''){throw 'Missing location section must be empty'}
'PASS: key location text preserves spawn conditions and excludes gallery and lock locations'
