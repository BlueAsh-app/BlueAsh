$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
Add-Type -Path (Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
Assert ([ModePulse.TaxiMarker]::Colour([ModePulse.TaxiState]::Unknown).ToArgb() -eq [ModePulse.ExitSign]::Green.ToArgb()) 'Unconfirmed taxi must be visible green'
$canvas=New-Object ModePulse.MapCanvas;$canvas.Size=[Drawing.Size]::new(800,600);$canvas.MapImage=New-Object Drawing.Bitmap(1600,1200)
Assert ($canvas.PSObject.Methods.Name -contains 'FocusKey') 'Map supports locating a registered key'
$a=New-Object ModePulse.Pin;$a.Kind='key';$a.Ref='key-a';$a.X=.25;$a.Y=.3
$b=New-Object ModePulse.Pin;$b.Kind='key';$b.Ref='key-a';$b.X=.6;$b.Y=.65
$canvas.Pins.Add($a);$canvas.Pins.Add($b)
Assert ($canvas.FocusKey('key-a') -and $canvas.HighlightRef -eq 'key-a') 'Click locates all uses and starts highlight'
Add-Type -TypeDefinition 'using System;using System.Drawing;using System.Reflection;using System.Windows.Forms;public static class KeyTestUI{public static void RowEvent(ModePulse.KeyPanel p,int row,string method){var b=(ListBox)p.GetType().GetField("list",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(p);var r=b.GetItemRectangle(row);typeof(Control).GetMethod(method,BindingFlags.Instance|BindingFlags.NonPublic).Invoke(b,new object[]{new MouseEventArgs(MouseButtons.Left,1,r.Left+15,r.Top+10,0)});}public static PointF Screen(ModePulse.MapCanvas c,ModePulse.Pin p){return (PointF)c.GetType().GetMethod("ToScreen",BindingFlags.Instance|BindingFlags.NonPublic).Invoke(c,new object[]{p});}}' -ReferencedAssemblies System.Drawing,System.Windows.Forms,(Join-Path $PSScriptRoot '../companion/CompanionUI.dll')
foreach($p in @($a,$b)){$at=[KeyTestUI]::Screen($canvas,$p);Assert ($at.X -ge 55 -and $at.X -le 745 -and $at.Y -ge 55 -and $at.Y -le 545) 'Every use stays visible with room for the pulse'}
$zoom=$canvas.Zoom;Assert (!$canvas.FocusKey('missing') -and $canvas.Zoom -eq $zoom) 'Unknown key cannot move map'
$root=Join-Path $PSScriptRoot ('.bundle/key-guidance-test-'+[Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($root)
$data=Join-Path $root 'data.json';$mine=Join-Path $root 'mine.json'
[IO.File]::WriteAllText($data,'{"keys":{"key-a":{"name":"Test key","description":"A localized key description.","card":false,"maps":{"woods":2}},"key-b":{"name":"Other key","card":true,"maps":{"bigmap":1,"woods":1}}},"mapNames":{"woods":"Woods","bigmap":"Customs"}}')
[IO.File]::WriteAllText($mine,'{"pve":{"key-a":"Test key","key-b":"Other key"}}')
$panel=New-Object ModePulse.KeyPanel;$panel.Size=[Drawing.Size]::new(290,600);$panel.Load($data,$mine,'pve','bigmap')
$script:located=$null;$panel.add_LocateRequested({param($sender,$e)$script:located=$e})
Assert ($panel.Locate('key-a') -and $script:located.MapId -eq 'woods' -and $script:located.Id -eq 'key-a') 'Off-map key requests its actual use map'
Assert ($panel.ActiveRef -eq 'key-a' -and $panel.HoverDescription -eq 'A localized key description.') 'Selected row exposes cached localized description'
$panel.Locate('key-b')|Out-Null;Assert ($script:located.MapId -eq 'bigmap') 'Multi-map key prefers the map already open'
Assert (!$panel.Locate('not-owned')) 'Unregistered key cannot navigate'
[KeyTestUI]::RowEvent($panel,0,'OnMouseClick');Assert ([bool]$panel.ActiveRef) 'Actual row click starts a preview'
[KeyTestUI]::RowEvent($panel,1,'OnMouseMove');Assert (!$panel.ActiveRef -and $panel.Hover) 'Moving to another key stops the previous preview'
$panel.ClearPreview();Assert (!$panel.ActiveRef -and !$panel.Hover) 'Leaving row clears selection and tooltip'
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../companion/Companion.ps1'),[ref]$tokens,[ref]$errors)
$locate=$ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Locate-Key'},$true);. ([scriptblock]::Create($locate.Extent.Text))
$map=[pscustomobject]@{Role=[pscustomobject]@{SelectedItem='SCAV'};Maps=[pscustomobject]@{SelectedItem='Customs'};Canvas=$canvas};$catalog=@([pscustomobject]@{id='laboratory';role='PMC';displayName='Labs'})
function Load-Map{$script:mapId='laboratory_PMC'}
Locate-Key $null ([pscustomobject]@{MapId='laboratory';Id='key-a'});Assert ($map.Maps.SelectedItem -eq 'Labs' -and $map.Role.SelectedItem -eq 'PMC') 'PMC-only use map switches role and selects the correct catalog entry'
$canvas.SetKeyTip('Long title '+('title '*40),('description '*500),'Woods',590)
$bitmap=New-Object Drawing.Bitmap(800,600);$canvas.DrawToBitmap($bitmap,[Drawing.Rectangle]::new(0,0,800,600));$box=$canvas.KeyTipBounds
Assert ($box.Width -gt 0 -and $box.Left -ge 0 -and $box.Top -ge 0 -and $box.Right -le 800 -and $box.Bottom -le 600) 'Large tooltip remains inside map canvas'
$canvas.Size=[Drawing.Size]::new(160,100);$small=New-Object Drawing.Bitmap(160,100);$canvas.DrawToBitmap($small,[Drawing.Rectangle]::new(0,0,160,100));$box=$canvas.KeyTipBounds
Assert ($box.Left -ge 0 -and $box.Top -ge 0 -and $box.Right -le 160 -and $box.Bottom -le 100) 'Tooltip remains bounded at tiny sizes'
$small.Dispose();$bitmap.Dispose();$panel.Dispose();$canvas.Dispose()
'PASS: green taxi, key navigation, all-use framing, cached descriptions, leave cancellation, bounded tooltips'
