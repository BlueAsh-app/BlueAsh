$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$root=Join-Path $repo 'price'
$probe=@'
using System;using System.Windows.Forms;
public class KeyButtonProbe:ModeSwitchPrice.KeyButton {
 public void MouseMessage(int msg){var m=Message.Create(Handle,msg,IntPtr.Zero,IntPtr.Zero);base.WndProc(ref m);}
}
'@
$work=Join-Path $PSScriptRoot ('.bundle/key-button-close-test-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($work)
$probePath=Join-Path $work 'KeyButtonProbe.cs'
[IO.File]::WriteAllText($probePath,$probe)
Add-Type -Path @((Join-Path $root 'PriceCore.cs'),(Join-Path $root 'PriceWindow.cs'),(Join-Path $root 'PriceCatalog.cs'),(Join-Path $root 'PriceLanguage.cs'),(Join-Path $root '../companion/UiFont.cs'),$probePath) -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.Net.Http,System.Web.Extensions
[ModeSwitchPrice.Language]::Code='ko'
function Assert($ok,$why){if(!$ok){throw $why}}
# The worker's close path (PriceApp.HideSlot): the price tag (with its linked quest/hideout tags) and the key button hide together.
$src=[IO.File]::ReadAllText((Join-Path $root 'PriceApp.cs'))
Assert ($src.Contains('void HideSlot(Slot s){s.Price.Hide();s.Key.Hide();s.Shown=null;s.Covered=false;')) 'Worker hides price and key button together'
function Hide-Slot($s){$s.price.Hide();$s.key.Hide();$s.shown=$null;$s.covered=$false}
$script:zc=0
$keys=Join-Path $work 'my-keys.json'
$price=New-Object ModeSwitchPrice.PriceTag;$quest=New-Object ModeSwitchPrice.PriceTag;$hideout=New-Object ModeSwitchPrice.PriceTag;$key=New-Object KeyButtonProbe
$price.Linked.Add($quest);$price.Linked.Add($hideout)
$slot=@{price=$price;quest=$quest;hide=$hideout;key=$key;shown=@{item='key'};covered=$false}
try{
 $price.ShowAt('price',[Drawing.Color]::White,-10000,-10000,16)
 $quest.ShowAt('quest',[Drawing.Color]::White,-10000,-10000,16)
 $hideout.ShowAt('hideout',[Drawing.Color]::White,-10000,-10000,16)
 $key.ShowFor($keys,'pve','test-key','test key',-10000,-10000,16)
 Assert ($key.Visible) 'Inspect explicitly shows the register button'
 $key.MouseMessage(0x200) # Hover before the inspect window closes.
 Hide-Slot $slot
 $key.MouseMessage(0x2A3) # WM_MOUSELEAVE can arrive after Hide.
 Assert (!$key.Visible) 'Closed inspect: delayed mouse-leave must not resurrect the key button'
 Assert (!$price.Visible -and !$quest.Visible -and !$hideout.Visible -and !$slot.shown) 'All inspect overlays close together'
 $key.MouseMessage(0x200);$key.MouseMessage(0x202)
 Assert (!$key.Visible -and ![ModeSwitchPrice.MyKeys]::Has($keys,'pve','test-key')) 'Hidden mouse events neither show nor register a key'
 $draw=[ModeSwitchPrice.KeyButton].GetMethod('Draw',[Reflection.BindingFlags]'NonPublic,Instance')
 $draw.Invoke($key,@())
 Assert (!$key.Visible) 'A delayed redraw never shows a hidden button'
 $key.ShowFor($keys,'pve','test-key','test key',-10000,-10000,16)
 Assert ($key.Visible -and !$key.Registered) 'A newly opened inspect restores the button'
 $key.MouseMessage(0x200);$key.MouseMessage(0x202)
 Assert ($key.Visible -and $key.Registered -and [ModeSwitchPrice.MyKeys]::Has($keys,'pve','test-key')) 'Visible button still registers a key'
 $key.MouseMessage(0x202)
 Assert (!$key.Registered -and ![ModeSwitchPrice.MyKeys]::Has($keys,'pve','test-key')) 'Visible button still unregisters a key'
 $key.MouseMessage(0x200);$key.Hide();$key.MouseMessage(0x2A3)
 $hot=[ModeSwitchPrice.KeyButton].GetField('hot',[Reflection.BindingFlags]'NonPublic,Instance')
 $tracking=[ModeSwitchPrice.KeyButton].GetField('tracking',[Reflection.BindingFlags]'NonPublic,Instance')
 Assert (!$key.Visible -and !$hot.GetValue($key) -and !$tracking.GetValue($key)) 'Hiding resets hover and mouse tracking'
 Write-Output 'PASS: inspect-close lifecycle, delayed/hidden mouse events, reopen, visible register/unregister, hover reset'
}finally{foreach($form in $price,$quest,$hideout,$key){$form.Dispose()}}
