$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Web.Extensions
$dll=Join-Path $PSScriptRoot '../companion/CompanionUI.dll';Add-Type -Path $dll
Add-Type -TypeDefinition @'
using System;using System.Drawing;using System.Reflection;
public static class KeyBounceTest {
 public static Bitmap Frame(string kind,int ms){using(var c=new ModePulse.MapCanvas()){c.Size=new Size(400,400);c.MapImage=new Bitmap(400,400);c.Font=new Font("Segoe UI",12,GraphicsUnit.Pixel);var p=new ModePulse.Pin{Kind=kind,Ref="key",X=.5f,Y=.5f};var b=new Bitmap(400,400);using(var g=Graphics.FromImage(b)){g.Clear(Color.Transparent);g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;var draw=typeof(ModePulse.MapCanvas).GetMethod("DrawFocusedKey",BindingFlags.Instance|BindingFlags.NonPublic);draw.Invoke(c,draw.GetParameters().Length==3?new object[]{g,p,ms}:new object[]{g,p});}return b;}}
 public static float Centre(Bitmap b,string kind){double sum=0;int n=0;for(int y=100;y<220;y++)for(int x=140;x<260;x++){var c=b.GetPixel(x,y);if(c.A<250)continue;bool ink=kind=="key"?c.R>210&&c.G>130&&c.B<120:c.R<120&&c.G>150&&c.B>210;if(ink){sum+=y;n++;}}if(n==0)throw new Exception("No key glyph");return (float)(sum/n);}
 static bool Ink(Color c,string kind){return c.A>=250&&(kind=="key"?c.R>210&&c.G>130&&c.B<120:c.R<120&&c.G>150&&c.B>210);}
 public static int Width(Bitmap b,string kind){int left=400,right=0;for(int y=100;y<220;y++)for(int x=140;x<260;x++)if(Ink(b.GetPixel(x,y),kind)){left=Math.Min(left,x);right=Math.Max(right,x);}return right-left+1;}
 public static int Difference(Bitmap a,Bitmap b,string kind){int n=0;for(int y=100;y<220;y++)for(int x=140;x<260;x++)if(Ink(a.GetPixel(x,y),kind)!=Ink(b.GetPixel(x,y),kind))n++;return n;}
 public static int Interval(bool active){using(var c=new ModePulse.MapCanvas()){c.HighlightRef="test";if(!active)c.HighlightRef="";return ((System.Windows.Forms.Timer)typeof(ModePulse.MapCanvas).GetField("pulse",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(c)).Interval;}}
 public static int Below(Bitmap b){int n=0;for(int y=202;y<260;y++)for(int x=130;x<270;x++)if(b.GetPixel(x,y).A>12)n++;return n;}
 public static int Rings(Bitmap b){int n=0;bool on=false;for(int x=201;x<270;x++){bool ink=b.GetPixel(x,202).A>12;if(ink&&!on)n++;on=ink;}return n;}
 public static int Badge(Bitmap b){int n=0;for(int y=120;y<240;y++)for(int x=140;x<260;x++){var c=b.GetPixel(x,y);if(c.A>200&&c.R==17&&c.G==23&&c.B==35)n++;}return n;}
}
'@ -ReferencedAssemblies System.Drawing,System.Windows.Forms,$dll
Assert ([KeyBounceTest]::Interval($true) -le 17) 'Focused key requests smooth animation frames'
Assert ([KeyBounceTest]::Interval($false) -eq 50) 'Leaving key restores existing timer cadence'
$work=Join-Path $PSScriptRoot '.bundle/key-jelly-2.16.4';[void][IO.Directory]::CreateDirectory($work)
$sheet=New-Object Drawing.Bitmap(1400,340);$g=[Drawing.Graphics]::FromImage($sheet);$g.Clear([Drawing.Color]::FromArgb(22,28,39));$font=New-Object Drawing.Font('Segoe UI',11)
try{
 $row=0
 foreach($kind in 'key','keycard'){
  $ground=[KeyBounceTest]::Frame($kind,0);$top=[KeyBounceTest]::Frame($kind,450);$land=[KeyBounceTest]::Frame($kind,900);$wave=[KeyBounceTest]::Frame($kind,1150);$next=[KeyBounceTest]::Frame($kind,1400);$soft=[KeyBounceTest]::Frame($kind,880);$jelly=[KeyBounceTest]::Frame($kind,945);$rebound=[KeyBounceTest]::Frame($kind,1030);$settled=[KeyBounceTest]::Frame($kind,1390)
  try{
   Assert (([KeyBounceTest]::Centre($ground,$kind)-[KeyBounceTest]::Centre($top,$kind)) -ge 18) ($kind+': enlarged icon jumps upward')
   Assert ([Math]::Abs([KeyBounceTest]::Centre($ground,$kind)-[KeyBounceTest]::Centre($land,$kind)) -lt 1) ($kind+': lands at the same ground position')
   Assert ([KeyBounceTest]::Badge($ground) -eq 0 -and [KeyBounceTest]::Badge($top) -eq 0) ($kind+': no circular background badge')
   Assert ([KeyBounceTest]::Below($ground) -eq 0 -and [KeyBounceTest]::Below($top) -eq 0) ($kind+': no ripple before landing')
   Assert ([KeyBounceTest]::Below($land) -gt 0 -and [KeyBounceTest]::Below($wave) -gt 0) ($kind+': ripple expands after landing')
   Assert ([KeyBounceTest]::Rings($wave) -eq 1) ($kind+': one clean water ripple')
   Assert ([Math]::Abs([KeyBounceTest]::Centre($soft,$kind)-[KeyBounceTest]::Centre($land,$kind)) -lt .8) ($kind+': eased descent reaches the ground gently')
   Assert ([KeyBounceTest]::Width($jelly,$kind) -gt [KeyBounceTest]::Width($land,$kind)) ($kind+': soft squash widens icon on landing')
   $deformed=[KeyBounceTest]::Difference($jelly,$land,$kind);Assert ($deformed -gt 5) ($kind+': landing visibly deforms and rocks the glyph')
   Assert ([KeyBounceTest]::Difference($settled,$land,$kind) -lt $deformed) ($kind+': jelly motion settles before next jump')
   Assert ([KeyBounceTest]::Below($next) -eq 0) ($kind+': ripple ends before next jump')
   $i=0;foreach($b in @($ground,$top,$land,$jelly,$rebound,$settled,$next)){$g.DrawImage($b,[Drawing.Rectangle]::new($i*200,$row*165+18,200,130),[Drawing.Rectangle]::new(100,115,200,130),[Drawing.GraphicsUnit]::Pixel);$g.DrawString(($kind+' '+@(0,450,900,945,1030,1390,1400)[$i]+'ms'),$font,[Drawing.Brushes]::White,$i*200+12,$row*165+145);$i++}
  }finally{$ground.Dispose();$top.Dispose();$land.Dispose();$wave.Dispose();$next.Dispose();$soft.Dispose();$jelly.Dispose();$rebound.Dispose();$settled.Dispose()};$row++
 }
 $sheet.Save((Join-Path $work 'motion-frames.png'),[Drawing.Imaging.ImageFormat]::Png)
}finally{$g.Dispose();$sheet.Dispose();$font.Dispose()}
'PASS: key/card jump height, stable landing point, no badge, landing-only ripple, one water ring, eased descent, damped landing jelly and focus-only smooth cadence, next-cycle reset'
