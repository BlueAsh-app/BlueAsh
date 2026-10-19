using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.Text;
namespace ModePulse {
 // Screen pixels only: an active flea navigation tab or the separate ITEM SEARCH window.
 public static class MapSearch {
  static string Fold(string s){var b=new StringBuilder();foreach(char c in (s??"").Normalize(NormalizationForm.FormD))if(char.IsLetterOrDigit(c)&&CharUnicodeInfo.GetUnicodeCategory(c)!=UnicodeCategory.NonSpacingMark)b.Append(char.ToUpperInvariant(c));return b.ToString().Normalize(NormalizationForm.FormC);}
  public static bool SearchTitle(string text){string n=Fold(text),want="ITEMSEARCH";if(n==want)return true;if(Math.Abs(n.Length-want.Length)>1||(!n.StartsWith("ITEM")&&!n.EndsWith("SEARCH")))return false;var d=new int[want.Length+1];for(int j=0;j<d.Length;j++)d[j]=j;for(int i=0;i<n.Length;i++){int before=d[0];d[0]=i+1;for(int j=1;j<d.Length;j++){int old=d[j];d[j]=Math.Min(Math.Min(d[j]+1,d[j-1]+1),before+(n[i]==want[j-1]?0:1));before=old;}}return d[want.Length]<=1;}
  public static bool FleaTitle(string text){string n=Fold(text);if(n.Length==4&&n.Substring(1)=="리마켓")return true;foreach(var s in new[]{"플리마켓","FLEA MARKET","Барахолка","フリーマーケット","跳蚤市场","跳蚤市場","FLOHMARKT","MARCHÉ AUX PUCES","PCHLI TARG","BIT PAZARI","BİT PAZARI","MERCADILLO","MERCADO","MERCATINO","MERCATO DELLE PULCI","BOLHAPIAC","BLŠÍ TRH","PIAȚĂ"})if(n==Fold(s))return true;return false;}
  // Surrounding backplate must be light; merely seeing an inactive flea button is not enough.
  public static bool Selected(Bitmap b,RectangleF r){
   int pad=Math.Max(3,(int)(r.Height*.32)),x0=Math.Max(0,(int)r.Left-pad),x1=Math.Min(b.Width,(int)r.Right+pad),y0=Math.Max(0,(int)r.Top-pad),y1=Math.Min(b.Height,(int)r.Bottom+pad),light=0,total=0;
   for(int y=y0;y<y1;y+=2)for(int x=x0;x<x1;x+=2){if(r.Contains(x,y))continue;var c=b.GetPixel(x,y);total++;if(Math.Min(c.R,Math.Min(c.G,c.B))>125&&Math.Max(c.R,Math.Max(c.G,c.B))-Math.Min(c.R,Math.Min(c.G,c.B))<65)light++;}
   return total>0&&light*100>total*55;
  }
  // Compact OCR sheet: navigation bands, then detected floating title bars. No item grids need OCR.
  public static Bitmap Sheet(Bitmap frame,Rectangle[] bars,out int navigationHeight){
   int w=Math.Min(1600,frame.Width),h=(int)Math.Round(frame.Height*(double)w/frame.Width),head=Math.Min(h,Math.Max(48,Math.Min(144,(int)(h*.13)))),foot=Math.Min(h-head,Math.Max(24,Math.Min(48,(int)(h*.05))));
   navigationHeight=head+foot+24;int extra=0;foreach(var r in bars)extra+=Math.Min(48,r.Height+10)+12;
   var sheet=new Bitmap(w,navigationHeight+extra);using(var small=new Bitmap(w,h))using(var g=Graphics.FromImage(small))using(var o=Graphics.FromImage(sheet)){
    g.InterpolationMode=InterpolationMode.HighQualityBilinear;g.DrawImage(frame,0,0,w,h);o.Clear(Color.FromArgb(24,26,28));o.DrawImage(small,new Rectangle(0,0,w,head),new Rectangle(0,0,w,head),GraphicsUnit.Pixel);o.DrawImage(small,new Rectangle(0,head+12,w,foot),new Rectangle(0,h-foot,w,foot),GraphicsUnit.Pixel);int y=navigationHeight;
    foreach(var r in bars){var crop=Rectangle.Intersect(new Rectangle(0,0,w,h),new Rectangle(Math.Max(0,r.X-12),Math.Max(0,r.Y-2),r.Width+24,Math.Min(48,r.Height+10)));if(crop.Width>0&&crop.Height>0)o.DrawImage(small,new Rectangle(0,y,crop.Width,crop.Height),crop,GraphicsUnit.Pixel);y+=Math.Min(48,r.Height+10)+12;}
   }var enlarged=new Bitmap((int)(sheet.Width*1.5),(int)(sheet.Height*1.5));using(var g=Graphics.FromImage(enlarged)){g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.DrawImage(sheet,0,0,enlarged.Width,enlarged.Height);}sheet.Dispose();navigationHeight=(int)(navigationHeight*1.5);return enlarged;
  }
  public static bool Blocked(Bitmap sheet,int navigationHeight,string[] text,RectangleF[] boxes){
   for(int i=0;i<text.Length;i++){string span="";var r=boxes[i];for(int j=i;j<Math.Min(text.Length,i+4);j++){
    if(j>i&&(Math.Abs((boxes[j].Top+boxes[j].Height/2)-(r.Top+r.Height/2))>Math.Max(r.Height,boxes[j].Height)*.65||boxes[j].Left-r.Right>r.Height*3||boxes[j].Left<r.Left))break;
    span+=text[j];r=RectangleF.Union(r,boxes[j]);if(SearchTitle(span))return true;if(r.Bottom<=navigationHeight&&FleaTitle(span)&&Selected(sheet,r))return true;
   }}return false;
  }
 }
}
