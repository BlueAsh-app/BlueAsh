param($src,$dst,[string[]]$maps)
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Imaging;using System.IO;using System.Runtime.InteropServices;
public static class G8192 { public static void Dump(string png,string dst){using(var b=new Bitmap(png)){var d=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
 byte[] row=new byte[d.Stride];byte[] o=new byte[(long)b.Width*b.Height];for(int y=0;y<b.Height;y++){Marshal.Copy(new IntPtr(d.Scan0.ToInt64()+(long)y*d.Stride),row,0,d.Stride);for(int x=0;x<b.Width;x++)o[(long)y*b.Width+x]=row[x*4+1];}
 b.UnlockBits(d);File.WriteAllBytes(dst,o);}}}
'@
foreach($m in ($maps -split ",")){[G8192]::Dump((Join-Path $src "$m.png"),(Join-Path $dst "$m.8192"))}
