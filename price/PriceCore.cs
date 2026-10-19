using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
namespace ModeSwitchPrice {
 public struct Box { public int X,Y,W,H; public Box(int x,int y,int w,int h){X=x;Y=y;W=w;H=h;} }
 public static class Core {
  // × counts as x (PaddleOCR reads "5.56x45" as "5.56×45").
  public static string Normalize(string s){return Regex.Replace((s??"").Normalize().Replace('×','x').ToLowerInvariant(),@"[^\p{L}\p{Nd}]","");}
  // OCR-tolerant key: x read as ")(", and I/l/|/! vs 1, O vs 0 collapse to one symbol.
  public static string Fold(string s){
   var n=Normalize((s??"").Replace(")(","x").Replace("|","1").Replace("!","1"));
   var c=n.ToCharArray();for(int i=0;i<c.Length;i++){if(c[i]=='l'||c[i]=='i')c[i]='1';else if(c[i]=='o')c[i]='0';}return new string(c);
  }
  static int Distance(string a,string b,int limit){
   if(Math.Abs(a.Length-b.Length)>limit)return limit+1;
   var prev=new int[b.Length+1];var cur=new int[b.Length+1];for(int j=0;j<=b.Length;j++)prev[j]=j;
   for(int i=1;i<=a.Length;i++){cur[0]=i;int low=cur[0];
    for(int j=1;j<=b.Length;j++){cur[j]=Math.Min(Math.Min(cur[j-1]+1,prev[j]+1),prev[j-1]+(a[i-1]==b[j-1]?0:1));if(cur[j]<low)low=cur[j];}
    if(low>limit)return limit+1;var t=prev;prev=cur;cur=t;}
   return prev[b.Length];
  }
  // Longest common subsequence length: how many of b's characters appear in a, in order.
  public static int CommonInOrder(string a,string b){
   var prev=new int[b.Length+1];var cur=new int[b.Length+1];
   for(int i=1;i<=a.Length;i++){for(int j=1;j<=b.Length;j++)cur[j]=a[i-1]==b[j-1]?prev[j-1]+1:Math.Max(prev[j],cur[j-1]);var t=prev;prev=cur;cur=t;}
   return prev[b.Length];
  }
  // Index of the single closest folded key, or -1 when nothing is close or the best is ambiguous.
  public static int Closest(string text,string[] folded){
   var f=Fold(text);if(f.Length<4)return -1;int limit=Math.Max(1,f.Length/6),best=-1,bestD=limit+1,second=limit+1;
   for(int i=0;i<folded.Length;i++){var k=folded[i];if(k==null)continue;int d=Distance(f,k,limit);
    if(d<bestD){second=bestD;bestD=d;best=i;}else if(d<second)second=d;}
   return bestD<=limit&&second>bestD?best:-1;
  }
  public static string Mode(string s){
   s=(s??"").ToUpperInvariant();
   bool e=Regex.IsMatch(s,@"\bPV\s*E\b");
   bool p=Regex.IsMatch(s,@"\bPV\s*P\s*S\b");
   if(e==p)return "";return e?"pve":"pvp-season";
  }
  static bool Red(byte[] b,int p){return b[p+2]>=40&&b[p+2]>b[p+1]*2&&b[p+2]>b[p]*2&&b[p+1]<65&&b[p]<65;}
  static bool Similar(byte[] b,int p,int q){return Math.Abs(b[p]-b[q])<9&&Math.Abs(b[p+1]-b[q+1])<9&&Math.Abs(b[p+2]-b[q+2])<9;}
  // Input is a tightly packed BGRA frame, scaled to <=2048px. Locate the red
  // close button and the flat title bar to its left; no fixed window position.
  public static Box[] TitleBars(byte[] b,int w,int h){
   var result=new List<Box>();if(w<100||h<100||b.Length!=w*h*4)return result.ToArray();
   for(int y=60;y<h-80;y+=2){
    for(int x=120;x<w-5;x++){
     if(!Red(b,(y*w+x)*4))continue;int start=x;
     while(x<w&&Red(b,(y*w+x)*4))x++;
     int rw=x-start;if(rw<9||rw>55)continue;
     bool seen=false;foreach(var r in result)if(Math.Abs(r.Y-y)<24&&Math.Abs(r.X+r.W-start)<12)seen=true;
     if(seen)continue;
     int anchor=(y*w+start-5)*4;
     if(b[anchor]>65||b[anchor+1]>65||b[anchor+2]>65)continue;
     // The title text can cross this row: step over short breaks (glyphs, up to 22 sampled px) and stop at a
     // long one (the window edge). Stopping at the first glyph used to keep only the last word ("Commando").
     int left=start-5,gap=0;
     for(int xx=start-5;xx>0;xx--){if(Similar(b,(y*w+xx)*4,anchor)){left=xx;gap=0;}else if(++gap>22)break;}
     left--;
     int width=start-left-8;if(width<180||width>w*0.85)continue;
     int top=Math.Max(0,y-3),height=Math.Min(32,h-top);
     result.Add(new Box(left+3,top,width,height));
     if(result.Count>=6)return result.ToArray();
    }
   }
   return result.ToArray();
  }
 }
}
