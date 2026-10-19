# Builds relay/public/pubg: the official PUBG maps (pubg/api-assets, <Map>_Main_High_Res.png, 8192 px) at 4096 px,
# green channel, 8 bit raw (<Map>.u8, 16 MB each) + manifest.txt (id, width, size, SHA-256). BlueAsh downloads the one
# map in play on first deep zoom (pubg_map.h); nothing here is bundled into BlueAsh.exe.
#   powershell -ExecutionPolicy Bypass -File source/build-pubg-hires.ps1
# Source PNGs (about 1 GB) are downloaded into %TEMP%\blueash-pubg-src when missing.
# relay/public/pubg is git-ignored: run this before deploying the relay from a fresh checkout (release.ps1 checks for it).
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$src=Join-Path $env:TEMP 'blueash-pubg-src';New-Item -ItemType Directory -Force $src|Out-Null
$out=Join-Path $root 'relay\public\pubg';New-Item -ItemType Directory -Force $out|Out-Null
$maps='Erangel','Miramar','Taego','Deston','Vikendi','Rondo','Sanhok','Karakin','Paramo','Haven'
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;using System.Drawing;using System.Drawing.Imaging;using System.IO;using System.Runtime.InteropServices;
public static class PubgHires {
  // Green channel, averaged 2 x 2 (8192 -> 4096), same channel as the bundled 273/819 px references (the blue zone tints blue).
  public static void Convert(string png,string dst,int w){
    using(var b=new Bitmap(png)){if(b.Width%w!=0||b.Height!=b.Width)throw new Exception(png+": "+b.Width+"x"+b.Height);int f=b.Width/w;
      var d=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
      byte[] row=new byte[d.Stride*f];byte[] o=new byte[w*w];
      for(int y=0;y<w;y++){Marshal.Copy(new IntPtr(d.Scan0.ToInt64()+(long)y*f*d.Stride),row,0,d.Stride*f);
        for(int x=0;x<w;x++){int s=0;for(int dy=0;dy<f;dy++)for(int dx=0;dx<f;dx++)s+=row[dy*d.Stride+(x*f+dx)*4+1];o[y*w+x]=(byte)((s+f*f/2)/(f*f));}}
      b.UnlockBits(d);File.WriteAllBytes(dst,o);}}
}
'@
$lines=@()
foreach($m in $maps){
  $png=Join-Path $src "$m.png"
  if(-not (Test-Path $png) -or (Get-Item $png).Length -lt 1000000){Invoke-WebRequest -UseBasicParsing "https://media.githubusercontent.com/media/pubg/api-assets/master/Assets/Maps/${m}_Main_High_Res.png" -OutFile $png}
  $u8=Join-Path $out "$m.u8";[PubgHires]::Convert($png,$u8,4096)
  $h=(Get-FileHash $u8 -Algorithm SHA256).Hash.ToLower();$n=(Get-Item $u8).Length;$lines+="$m 4096 $n $h";Write-Host "$m $n $h"
}
[IO.File]::WriteAllText((Join-Path $out 'manifest.txt'),($lines -join "`n")+"`n")
Write-Host 'relay/public/pubg ready'
