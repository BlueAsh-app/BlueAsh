$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
$repo=Split-Path $PSScriptRoot -Parent
$assembly=[Reflection.Assembly]::LoadFrom($repo+'/companion/CompanionUI.dll')
Assert ($null -ne $assembly.GetType('ModePulse.FlareMarker',$false)) 'Flare exits need a dedicated marker'
$field=[ModePulse.FandomText].GetField('words',[Reflection.BindingFlags]'NonPublic,Static')
$words=New-Object 'System.Collections.Generic.Dictionary[string,string]';$field.SetValue($null,$words)
$p=New-Object ModePulse.Pin
foreach($name in 'Mira Ave (Flare)','Klimov Street (Flare)','Power Line Passage (Flare)','Path to River (Flare)','Railroad Passage (Flare)','철도 통로 (조명탄 탈출구)'){$p.Name=$name;Assert ([ModePulse.FlareMarker]::IsFlare($p)) ('Flare name: '+$name)}
foreach($name in 'Old Gas Station','ZB-016','Courtyard','Pier Boat'){$p.Name=$name;$p.Condition='Green flare = Open';Assert (![ModePulse.FlareMarker]::IsFlare($p)) ('Open-status flare is not a required shot: '+$name)}
$source='Requires a green flare to be shot into the sky while inside the signal flare area.'
$row=([IO.File]::ReadAllLines($repo+'/data/companion/FandomText.tsv')|Where-Object {$_.StartsWith($source+[char]9)}) -split [char]9
Assert ($row.Count -eq 13) 'Flare requirement has all supported translations'
foreach($description in $row){$words=New-Object 'System.Collections.Generic.Dictionary[string,string]';$words[$source]=$description;$field.SetValue($null,$words);$p.Name='Helicopter';$p.Condition=$description;Assert ([ModePulse.FlareMarker]::IsFlare($p)) 'Requirement-only exit is recognized in each language'}
$p.Kind='transit';Assert (![ModePulse.FlareMarker]::IsFlare($p)) 'Transit is never replaced by a flare sign'
$p.Kind='quest';Assert (![ModePulse.FlareMarker]::IsFlare($p)) 'Other map marker kinds remain unchanged'
Assert (![ModePulse.FlareMarker]::IsFlare($null)) 'Missing marker is ignored'
$words=New-Object 'System.Collections.Generic.Dictionary[string,string]';$field.SetValue($null,$words)
$count=0;foreach($map in [ModePulse.FandomMarkers]::Titles.Keys){foreach($pin in [ModePulse.FandomMarkers]::Pins($map,'PMC','',$repo+'/data/maps/markers.json')){if([ModePulse.FlareMarker]::IsFlare($pin)){$count++}}};Assert ($count -eq 6) 'All six shipped flare-requirement exits are classified; indicator lights are excluded'
foreach($size in 24,32,48,96){$b=New-Object Drawing.Bitmap(($size*2),($size*2));$g=[Drawing.Graphics]::FromImage($b);try{$g.Clear([Drawing.Color]::Transparent);[ModePulse.FlareMarker]::Draw($g,$size,$size,$size,[ModePulse.ExitSign]::Green);$white=0;$green=0;for($y=0;$y -lt $b.Height;$y++){for($x=0;$x -lt $b.Width;$x++){$c=$b.GetPixel($x,$y);if($c.A -gt 200 -and $c.R -gt 240 -and $c.G -gt 240 -and $c.B -gt 240){$white++};if($c.A -gt 200 -and $c.G -gt 100 -and $c.R -lt 50 -and $c.B -lt 100){$green++}}};Assert ($white -gt 20 -and $green -gt 80) ('Readable white glyph and green tile at '+$size+'px');Assert ($b.GetPixel(0,0).A -eq 0 -and $b.GetPixel(($b.Width-1),($b.Height-1)).A -eq 0) 'Sign stays within its canvas';Assert ($g.Transform.IsIdentity) 'Drawing preserves the caller transform'}finally{$g.Dispose();$b.Dispose()}}
'PASS: flare-only classification, six shipped exits, 13 languages, normal exit/transit preservation, 24–96px vector rendering'

# The normal, unavailable and faded map branches use the same production vector.
$draw=[ModePulse.MapCanvas].GetMethod('DrawMarker',[Reflection.BindingFlags]'NonPublic,Static')
$faded=[ModePulse.MapCanvas].GetMethod('DrawFaded',[Reflection.BindingFlags]'NonPublic,Static')
foreach($kind in 'exit','grey'){
 $expected=New-Object Drawing.Bitmap(80,80);$actual=New-Object Drawing.Bitmap(80,80);$eg=[Drawing.Graphics]::FromImage($expected);$ag=[Drawing.Graphics]::FromImage($actual);$edge=New-Object Drawing.Pen([Drawing.Color]::Black)
 try{$color=if($kind -eq 'grey'){[Drawing.Color]::FromArgb(128,128,128)}else{[ModePulse.ExitSign]::Green};[ModePulse.FlareMarker]::Draw($eg,40,40,24,$color);$null=$draw.Invoke($null,@($ag,$kind,[single]40,[single]40,[single]7.5,[Drawing.Brushes]::White,$edge.PSObject.BaseObject,$false,$true));for($y=0;$y -lt 80;$y++){for($x=0;$x -lt 80;$x++){Assert ($expected.GetPixel($x,$y).ToArgb() -eq $actual.GetPixel($x,$y).ToArgb()) ('Map branch retains flare shape and colour: '+$kind)}}
 }finally{$eg.Dispose();$ag.Dispose();$edge.Dispose();$expected.Dispose();$actual.Dispose()}
}
$b=New-Object Drawing.Bitmap(80,80);$g=[Drawing.Graphics]::FromImage($b);$edge=New-Object Drawing.Pen([Drawing.Color]::Black)
try{$null=$faded.Invoke($null,@($g,'exit',[single]40,[single]40,[single]7.5,[Drawing.Brushes]::White,$edge.PSObject.BaseObject,$false,$false,[ModePulse.TaxiState]::Unknown,$true));$max=0;for($y=0;$y -lt 80;$y++){for($x=0;$x -lt 80;$x++){$max=[Math]::Max($max,$b.GetPixel($x,$y).A)}};Assert ($max -ge 100 -and $max -le 103) 'Faded flare keeps the existing 40 percent opacity'}finally{$g.Dispose();$edge.Dispose();$b.Dispose()}
'PASS: actual map renderer preserves flare glyph in available, unavailable and faded branches'

# The user's reference has four pointed diamond vertices and transparent corners.
$b=New-Object Drawing.Bitmap(192,192);$g=[Drawing.Graphics]::FromImage($b)
try{
 [ModePulse.FlareMarker]::Draw($g,96,96,96,[ModePulse.ExitSign]::Green)
 foreach($at in @(@(96,37),@(155,96),@(96,155),@(37,96))){Assert ($b.GetPixel($at[0],$at[1]).A -gt 100) 'Diamond has centered top/right/bottom/left vertices'}
 foreach($at in @(@(51,51),@(141,51),@(141,141),@(51,141))){Assert ($b.GetPixel($at[0],$at[1]).A -eq 0) 'Diamond corners remain transparent'}
 $minX=192;$minY=192;$maxX=0;$maxY=0
 for($y=0;$y -lt 192;$y++){for($x=0;$x -lt 192;$x++){if($b.GetPixel($x,$y).A -gt 100){$minX=[Math]::Min($minX,$x);$minY=[Math]::Min($minY,$y);$maxX=[Math]::Max($maxX,$x);$maxY=[Math]::Max($maxY,$y)}}}
 Assert ([Math]::Abs(($maxX-$minX)-($maxY-$minY)) -le 1 -and [Math]::Abs(($minX+$maxX)/2.0-96) -le 1 -and [Math]::Abs(($minY+$maxY)/2.0-96) -le 1) 'Diamond is square and stays centered on the exact map location'
}finally{$g.Dispose();$b.Dispose()}
'PASS: reference diamond silhouette, transparent corners and centered map anchor'

# The foreground fills the diamond evenly, with a centered downward arrow and clear edge padding.
$b=New-Object Drawing.Bitmap(192,192);$g=[Drawing.Graphics]::FromImage($b)
try{
 [ModePulse.FlareMarker]::Draw($g,96,96,96,[ModePulse.ExitSign]::Green)
 $minX=192;$minY=192;$maxX=0;$maxY=0
 for($y=0;$y -lt 192;$y++){for($x=0;$x -lt 192;$x++){$c=$b.GetPixel($x,$y);if($c.A -gt 240 -and $c.R -gt 240 -and $c.G -gt 240 -and $c.B -gt 240){$minX=[Math]::Min($minX,$x);$minY=[Math]::Min($minY,$y);$maxX=[Math]::Max($maxX,$x);$maxY=[Math]::Max($maxY,$y);Assert (([Math]::Abs($x-96)+[Math]::Abs($y-96)) -lt 58) 'Foreground keeps clear padding from every diamond edge'}}}
 Assert (($maxX-$minX) -ge 68 -and ($maxY-$minY) -ge 68) 'Gun and arrow fill the diamond at a larger readable size'
 Assert ([Math]::Abs(($minX+$maxX)/2.0-96) -le 2 -and [Math]::Abs(($minY+$maxY)/2.0-96) -le 2) 'Foreground bounds have balanced horizontal and vertical margins'
 $rows=0;for($y=116;$y -le 130;$y++){$left=192;$right=0;for($x=55;$x -le 137;$x++){$c=$b.GetPixel($x,$y);if($c.R -gt 240 -and $c.G -gt 240 -and $c.B -gt 240){$left=[Math]::Min($left,$x);$right=[Math]::Max($right,$x)}};if($right -ge $left){Assert ([Math]::Abs(($left+$right)/2.0-96) -le 1) 'Downward arrow stays symmetric about the diamond vertical centerline';$rows++}}
 Assert ($rows -ge 8) 'Arrow remains visible as a separate centered shape'
}finally{$g.Dispose();$b.Dispose()}
'PASS: larger foreground, balanced margins, clean border padding and symmetric centered arrow'

# The updated reference has a substantial separate arrow under the flare pistol.
$b=New-Object Drawing.Bitmap(192,192);$g=[Drawing.Graphics]::FromImage($b)
try{[ModePulse.FlareMarker]::Draw($g,96,96,96,[ModePulse.ExitSign]::Green);$left=192;$right=0;$top=192;$bottom=0
 for($y=0;$y -lt 192;$y++){for($x=0;$x -lt 192;$x++){$c=$b.GetPixel($x,$y);if($c.R -gt 240 -and $c.G -gt 240 -and $c.B -gt 240){$top=[Math]::Min($top,$y);$bottom=[Math]::Max($bottom,$y);if($y -eq 120){$left=[Math]::Min($left,$x);$right=[Math]::Max($right,$x)}}}}
 Assert (($bottom-$top) -ge 86 -and ($right-$left) -ge 36) 'Reference arrow has a broad head and fills the lower diamond with the pistol above'
 Assert ([Math]::Abs(($left+$right)/2.0-96) -le 1) 'Broad arrow remains symmetric on the vertical centerline'
}finally{$g.Dispose();$b.Dispose()}
'PASS: reference proportions and broad centered arrow'
