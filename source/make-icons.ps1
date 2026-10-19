# Builds the BlueAsh icon resources from source/assets/blueash (the BlueAsh icon pack, 2026-10-12):
#   app.ico    = BlueAsh.ico (black tile: exe, taskbar, Alt+Tab, sidebar logo)
#   window.ico = BlueAsh-Window-Tray.ico (transparent cobalt symbol: title bar)
#   normal.ico = the cobalt symbol (tray, normal mode and before any mode is applied)
#   fps.ico    = the cobalt symbol with a faint green light around its outline (tray in game mode)
#   powershell -ExecutionPolicy Bypass -File source/make-icons.ps1
$ErrorActionPreference='Stop';Add-Type -AssemblyName System.Drawing
$src=Join-Path $PSScriptRoot 'assets\blueash';$out=Join-Path $PSScriptRoot 'assets'
Copy-Item (Join-Path $src 'BlueAsh.ico') (Join-Path $out 'app.ico') -Force
Copy-Item (Join-Path $src 'BlueAsh-Window-Tray.ico') (Join-Path $out 'window.ico') -Force
Copy-Item (Join-Path $src 'BlueAsh-Window-Tray.ico') (Join-Path $out 'normal.ico') -Force
# PNG-compressed ICO entries (Vista+ format, what the pack uses); 256 is written as 0.
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;
namespace BlueAshIcons { public static class Glow {
  // Writes into dst a soft halo in (r,g,b): the alpha of src blurred by radius (3 box passes), outside src only, at most max opacity.
  public static void Outer(Bitmap src,Bitmap dst,int radius,int r,int g,int b,double max){
   int w=src.Width,h=src.Height;var a=new double[w*h];var t=new double[w*h];var inside=new double[w*h];
   for(int y=0;y<h;y++)for(int x=0;x<w;x++){a[y*w+x]=src.GetPixel(x,y).A/255.0;inside[y*w+x]=a[y*w+x];}
   for(int pass=0;pass<3;pass++){
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){double s=0;int n=0;for(int d=-radius;d<=radius;d++){int xx=x+d;if(xx<0||xx>=w)continue;s+=a[y*w+xx];n++;}t[y*w+x]=s/n;}
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){double s=0;int n=0;for(int d=-radius;d<=radius;d++){int yy=y+d;if(yy<0||yy>=h)continue;s+=t[yy*w+x];n++;}a[y*w+x]=s/n;}}
   for(int y=0;y<h;y++)for(int x=0;x<w;x++){double v=Math.Min(1,a[y*w+x]*1.6)*max*(1-inside[y*w+x]);dst.SetPixel(x,y,Color.FromArgb((int)Math.Round(v*255),r,g,b));}
  }}}
'@
$sizes=16,20,24,32,40,48,64,96,128,256;$images=@()
foreach($n in $sizes){$b=[Drawing.Bitmap]::FromFile((Join-Path $src "blueash-symbol-$n.png"));$c=New-Object Drawing.Bitmap($b.Width,$b.Height,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
 # A faint green light only around the mark: its silhouette blurred (Glow.Outer), nothing drawn over the mark itself.
 $mark=New-Object Drawing.Bitmap($n,$n,[Drawing.Imaging.PixelFormat]::Format32bppArgb);$mg=[Drawing.Graphics]::FromImage($mark);$mg.InterpolationMode='HighQualityBicubic';$k=0.86;$m=$n*(1-$k)/2;$mg.DrawImage($b,[single]$m,[single]$m,[single]($n*$k),[single]($n*$k));$mg.Dispose()
 [BlueAshIcons.Glow]::Outer($mark,$c,[Math]::Max(1,[int][Math]::Round($n/14)),46,214,118,0.55)
 $g=[Drawing.Graphics]::FromImage($c);$g.DrawImage($mark,0,0,$n,$n);$g.Dispose();$mark.Dispose()
 $ms=New-Object IO.MemoryStream;$c.Save($ms,[Drawing.Imaging.ImageFormat]::Png);$images+=,@($n,$ms.ToArray());$b.Dispose();$c.Dispose()}
$f=New-Object IO.MemoryStream;$w=New-Object IO.BinaryWriter($f);$w.Write([uint16]0);$w.Write([uint16]1);$w.Write([uint16]$images.Count)
$offset=6+16*$images.Count
foreach($i in $images){$n=$i[0];$d=$i[1];$w.Write([byte]($n%256));$w.Write([byte]($n%256));$w.Write([byte]0);$w.Write([byte]0);$w.Write([uint16]1);$w.Write([uint16]32);$w.Write([uint32]$d.Length);$w.Write([uint32]$offset);$offset+=$d.Length}
foreach($i in $images){$w.Write([byte[]]$i[1])};$w.Flush();[IO.File]::WriteAllBytes((Join-Path $out 'fps.ico'),$f.ToArray())
# Preview of both tray icons (16/24/32 px, 4x) for checking by eye.
if($env:BLUEASH_ICON_PREVIEW){$pv=New-Object Drawing.Bitmap(400,140);$pg=[Drawing.Graphics]::FromImage($pv);$pg.Clear([Drawing.Color]::FromArgb(32,32,36));$pg.InterpolationMode='NearestNeighbor';$x=10
 foreach($file in 'normal.ico','fps.ico'){foreach($sz in 16,24,32){$ic=New-Object Drawing.Icon((Join-Path $out $file),$sz,$sz);$bm=$ic.ToBitmap();$pg.DrawImage($bm,$x,10,$sz*3,$sz*3);$x+=$sz*3+8;$bm.Dispose();$ic.Dispose()}};$pv.Save($env:BLUEASH_ICON_PREVIEW)}
'icons written'
