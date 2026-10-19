using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Windows.Forms;
namespace ModeSwitchPrice {
 public class Overlay:Form {
  public readonly Label TextBox=new Label();
  public Overlay(){FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;TopMost=true;BackColor=Color.FromArgb(22,24,31);Opacity=.94;Size=new Size(430,135);TextBox.Dock=DockStyle.Fill;TextBox.Padding=new Padding(12);TextBox.Font=new Font(Language.FontName,10);TextBox.ForeColor=Color.FromArgb(242,244,248);Controls.Add(TextBox);var accent=new Panel();accent.Dock=DockStyle.Left;accent.Width=3;accent.BackColor=Color.FromArgb(52,92,255);Controls.Add(accent);}
  protected override bool ShowWithoutActivation {get{return true;}}
  protected override CreateParams CreateParams {get{var c=base.CreateParams;c.ExStyle|=0x08000000|0x20|0x80;return c;}}
 }
 // One price line drawn inside the inspect window's picture panel (bottom-right corner), no backdrop:
 // a per-pixel-alpha layered window holding only the outlined text. Click-through, never takes focus.
 // Latin/digits use Bahnschrift SemiBold (DIN-like, close to Tarkov's Bender); Hangul falls back to Malgun Gothic Bold.
 public class PriceTag:Form {
  [StructLayout(LayoutKind.Sequential)] struct SIZE{public int cx,cy;}
  [StructLayout(LayoutKind.Sequential,Pack=1)] struct BLEND{public byte Op,Flags,Alpha,Format;}
  [DllImport("user32.dll")] static extern bool UpdateLayeredWindow(IntPtr h,IntPtr dst,ref Desktop.POINT pos,ref SIZE size,IntPtr src,ref Desktop.POINT srcPos,int key,ref BLEND blend,int flags);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int ht,uint flags);
  [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h,IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc,IntPtr o);
  [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr o);
  public PriceTag(){FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;TopMost=true;StartPosition=FormStartPosition.Manual;}
  protected override bool ShowWithoutActivation {get{return true;}}
  // NOACTIVATE | LAYERED | TRANSPARENT (click-through) | TOOLWINDOW | TOPMOST
  protected override CreateParams CreateParams {get{var c=base.CreateParams;c.ExStyle|=0x08000000|0x80000|0x20|0x80|0x8;return c;}}
  // All text uses Malgun Gothic Bold (user preference: cleaner than Bahnschrift digits). Set false to put
  // Latin/digits back in the Latin family.
  public static bool KoreanOnly=true;
  static bool Hangul(char c){return KoreanOnly||(c>=0xAC00&&c<=0xD7A3)||(c>=0x3130&&c<=0x318F);}
  public static string Latin="Bahnschrift SemiBold",Korean=Language.FontName;
  // Renders the text with an outline into a premultiplied-alpha bitmap sized to fit.
  public static Bitmap Render(string text,Color color,int textHeight){
   float em=Math.Max(12,textHeight);int outline=Math.Max(2,textHeight/9),pad=outline+2;
   // Split into Hangul / non-Hangul runs: GraphicsPath.AddString does no font fallback.
   var runs=new System.Collections.Generic.List<string>();foreach(var ch in text){if(runs.Count==0||Hangul(runs[runs.Count-1][0])!=Hangul(ch))runs.Add(ch.ToString());else runs[runs.Count-1]+=ch;}
   using(var path=new System.Drawing.Drawing2D.GraphicsPath())using(var fmt=(StringFormat)StringFormat.GenericTypographic.Clone()){
    fmt.FormatFlags|=StringFormatFlags.MeasureTrailingSpaces;float x=0;
    using(var probe=new Bitmap(1,1))using(var pg=Graphics.FromImage(probe))using(var latin=new FontFamily(Latin))using(var korean=new FontFamily(Korean)){
     // Hangul is optically larger: shrink it slightly. Both scripts sit on one baseline (fonts' ascents differ).
     Func<bool,float> sizeOf=k=>k?em*0.9f:em;Func<bool,FontStyle> styleOf=k=>k?FontStyle.Bold:FontStyle.Regular;
     Func<bool,float> baseOf=k=>{var f=k?korean:latin;var s=styleOf(k);return sizeOf(k)*f.GetCellAscent(s)/f.GetEmHeight(s);};
     float baseline=Math.Max(baseOf(true),baseOf(false));
     foreach(var run in runs){
      bool k=Hangul(run[0]);var family=k?korean:latin;var style=styleOf(k);float size=sizeOf(k);
      using(var font=new Font(family,size,style,GraphicsUnit.Pixel)){path.AddString(run,family,(int)style,size,new PointF(x,baseline-baseOf(k)),fmt);x+=pg.MeasureString(run,font,PointF.Empty,fmt).Width;}
     }
    }
    var box=path.GetBounds();var bmp=new Bitmap((int)Math.Ceiling(box.Width)+2*pad,(int)Math.Ceiling(box.Height)+2*pad,PixelFormat.Format32bppArgb);
    using(var g=Graphics.FromImage(bmp)){g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;g.Clear(Color.Transparent);
     g.TranslateTransform(pad-box.X,pad-box.Y);
     using(var pen=new Pen(Color.FromArgb(225,0,0,0),outline*2){LineJoin=System.Drawing.Drawing2D.LineJoin.Round})g.DrawPath(pen,path);
     using(var brush=new SolidBrush(color))g.FillPath(brush,path);}
    return bmp;
   }
  }
  // A tag that disappears together with this one (the quest lines follow the price).
  public readonly System.Collections.Generic.List<PriceTag> Linked=new System.Collections.Generic.List<PriceTag>();
  protected override void OnVisibleChanged(EventArgs e){base.OnVisibleChanged(e);if(!Visible)foreach(var t in Linked)if(t.Visible)t.Hide();}
  // Several lines stacked upwards from bottom, left-aligned at left (quest lines, bottom-left of the picture panel).
  public void ShowLines(string[] lines,Color color,int left,int bottom,int textHeight){ShowLines(lines,color,left,bottom,textHeight,false);}
  // centered: x is the horizontal centre (hideout lines in the middle of the panel); otherwise the left edge.
  public void ShowLines(string[] lines,Color color,int x,int bottom,int textHeight,bool centered){
   var colors=new Color[lines==null?0:lines.Length];for(int i=0;i<colors.Length;i++)colors[i]=color;ShowLines(lines,colors,x,bottom,textHeight,centered);
  }
  // One color per line (hideout: bright for the next upgrade, dim for later ones).
  public void ShowLines(string[] lines,Color[] colors,int x,int bottom,int textHeight,bool centered){
   if(lines==null||lines.Length==0){Hide();return;}
   var parts=new System.Collections.Generic.List<Bitmap>();try{
    for(int i=0;i<lines.Length;i++)parts.Add(Render(lines[i],colors[Math.Min(i,colors.Length-1)],textHeight));int w=0,h=0;foreach(var p in parts){w=Math.Max(w,p.Width);h+=p.Height-2;}
    using(var bmp=new Bitmap(w,h+2,PixelFormat.Format32bppArgb)){using(var g=Graphics.FromImage(bmp)){g.Clear(Color.Transparent);int y=0;foreach(var p in parts){g.DrawImageUnscaled(p,centered?(w-p.Width)/2:0,y);y+=p.Height-2;}}
     Blit(bmp,centered?x-w/2:x,bottom-bmp.Height);}
   }finally{foreach(var p in parts)p.Dispose();}
  }
  void Blit(Bitmap bmp,int x,int y){
   if(!Visible)Show();
   var screen=GetDC(IntPtr.Zero);var mem=CreateCompatibleDC(screen);var hbmp=bmp.GetHbitmap(Color.FromArgb(0));var old=SelectObject(mem,hbmp);
   try{var size=new SIZE{cx=bmp.Width,cy=bmp.Height};var pos=new Desktop.POINT{X=x,Y=y};var src=new Desktop.POINT();var blend=new BLEND{Op=0,Flags=0,Alpha=255,Format=1};UpdateLayeredWindow(Handle,screen,ref pos,ref size,mem,ref src,0,ref blend,2);}
   finally{SelectObject(mem,old);DeleteObject(hbmp);DeleteDC(mem);ReleaseDC(IntPtr.Zero,screen);}
   SetWindowPos(Handle,new IntPtr(-1),0,0,0,0,0x2|0x1|0x10|0x40);
  }
  // right/bottom: screen pixels of the corner to align to; textHeight: pixel height of the text.
  public void ShowAt(string value,Color color,int right,int bottom,int textHeight){
   if(!Visible)Show();
   using(var bmp=Render(value,color,textHeight)){
    var screen=GetDC(IntPtr.Zero);var mem=CreateCompatibleDC(screen);var hbmp=bmp.GetHbitmap(Color.FromArgb(0));var old=SelectObject(mem,hbmp);
    try{var size=new SIZE{cx=bmp.Width,cy=bmp.Height};var pos=new Desktop.POINT{X=right-bmp.Width,Y=bottom-bmp.Height};var src=new Desktop.POINT();
     var blend=new BLEND{Op=0,Flags=0,Alpha=255,Format=1};UpdateLayeredWindow(Handle,screen,ref pos,ref size,mem,ref src,0,ref blend,2);}
    finally{SelectObject(mem,old);DeleteObject(hbmp);DeleteDC(mem);ReleaseDC(IntPtr.Zero,screen);}
   }
   // Re-assert topmost over the borderless game every time (SWP_NOMOVE|NOSIZE|NOACTIVATE|SHOWWINDOW).
   SetWindowPos(Handle,new IntPtr(-1),0,0,0,0,0x2|0x1|0x10|0x40);
  }
 }
 // "지도에 열쇠 등록" on a key's inspect window (top-left of the picture panel): one click puts the key in my-keys.json
 // (the map then marks every lock it opens), and the button turns green "✓ 지도에 등록됨"; hovering a registered key
 // shows "누르면 빼기" and a click takes it out again. Unlike the price tags it takes clicks, but never focus
 // (NOACTIVATE, MA_NOACTIVATE), so the game stays in front and keeps its cursor.
 public class KeyButton:Form {
  [StructLayout(LayoutKind.Sequential)] struct SIZE{public int cx,cy;}
  [StructLayout(LayoutKind.Sequential,Pack=1)] struct BLEND{public byte Op,Flags,Alpha,Format;}
  [StructLayout(LayoutKind.Sequential)] struct TRACK{public int Size;public int Flags;public IntPtr Window;public int Hover;}
  [DllImport("user32.dll")] static extern bool UpdateLayeredWindow(IntPtr h,IntPtr dst,ref Desktop.POINT pos,ref SIZE size,IntPtr src,ref Desktop.POINT srcPos,int key,ref BLEND blend,int flags);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int ht,uint flags);
  [DllImport("user32.dll")] static extern bool TrackMouseEvent(ref TRACK t);
  [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h,IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr CreateCompatibleDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern bool DeleteDC(IntPtr dc);
  [DllImport("gdi32.dll")] static extern IntPtr SelectObject(IntPtr dc,IntPtr o);
  [DllImport("gdi32.dll")] static extern bool DeleteObject(IntPtr o);
  string path,mode,id,name;int x,y,textHeight;bool hot,tracking;
  public bool Registered{get;private set;}
  public event EventHandler Toggled;
  public KeyButton(){FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;TopMost=true;StartPosition=FormStartPosition.Manual;Cursor=Cursors.Hand;}
  protected override bool ShowWithoutActivation {get{return true;}}
  // NOACTIVATE | LAYERED | TOOLWINDOW | TOPMOST (no TRANSPARENT: this one is clicked)
  protected override CreateParams CreateParams {get{var c=base.CreateParams;c.ExStyle|=0x08000000|0x80000|0x80|0x8;return c;}}
  public void ShowFor(string keysPath,string gameMode,string keyId,string keyName,int left,int top,int height){
   path=keysPath;mode=gameMode;id=keyId;name=keyName;x=left;y=top;textHeight=Math.Max(12,height);Registered=MyKeys.Has(path,mode,id);if(!Visible)Show();Draw();
  }
  protected override void OnVisibleChanged(EventArgs e){if(!Visible){hot=false;tracking=false;}base.OnVisibleChanged(e);}
  protected override void WndProc(ref Message m){
   const int MOUSEACTIVATE=0x21,MOUSEMOVE=0x200,LBUTTONUP=0x202,MOUSELEAVE=0x2A3;
   if(m.Msg==MOUSEACTIVATE){m.Result=new IntPtr(3);return;} // MA_NOACTIVATE: clicking does not take focus from the game
   if(Visible){
   if(m.Msg==MOUSEMOVE){if(!tracking){var t=new TRACK{Size=Marshal.SizeOf(typeof(TRACK)),Flags=2,Window=Handle};tracking=TrackMouseEvent(ref t);}if(!hot){hot=true;Draw();}}
   else if(m.Msg==MOUSELEAVE){tracking=false;if(hot){hot=false;Draw();}}
   else if(m.Msg==LBUTTONUP&&id!=null){
    if(Registered){if(MyKeys.Remove(path,mode,id)||!MyKeys.Has(path,mode,id))Registered=false;}else{if(MyKeys.Add(path,mode,id,name)||MyKeys.Has(path,mode,id))Registered=true;}
    hot=false;Draw();var h=Toggled;if(h!=null)h(this,EventArgs.Empty);
   }
   }
   base.WndProc(ref m);
  }
  // A rounded pill: registered = green with a check, hovering it = red "빼기"; not registered = dark with a gold key mark.
  void Draw(){
   string text;Color fill,edge,ink;
   if(Registered&&hot){text=Language.T("누르면 지도에서 빼기");fill=Color.FromArgb(235,196,54,66);edge=Color.FromArgb(255,236,96,108);ink=Color.White;}
   else if(Registered){text="✓ "+Language.T("지도에 등록됨");fill=Color.FromArgb(235,34,150,84);edge=Color.FromArgb(255,92,210,140);ink=Color.White;}
   else{text="+ "+Language.T("지도에 열쇠 등록");fill=Color.FromArgb(hot?245:225,hot?36:18,hot?44:22,hot?62:30);edge=Color.FromArgb(255,255,206,84);ink=Color.FromArgb(255,255,226,140);}
   using(var font=new Font(Language.FontName,textHeight,FontStyle.Bold,GraphicsUnit.Pixel))using(var probe=new Bitmap(1,1))using(var pg=Graphics.FromImage(probe)){
    var size=pg.MeasureString(text,font,PointF.Empty,StringFormat.GenericTypographic);int padX=textHeight,padY=textHeight/2,w=(int)Math.Ceiling(size.Width)+padX*2,h=(int)Math.Ceiling(size.Height)+padY*2;
    using(var bmp=new Bitmap(w+2,h+2,PixelFormat.Format32bppArgb)){
     using(var g=Graphics.FromImage(bmp)){g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;g.TextRenderingHint=System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;g.Clear(Color.Transparent);
      float r=h/2f;using(var p=new System.Drawing.Drawing2D.GraphicsPath()){p.AddArc(1,1,h,h,90,180);p.AddArc(w-h+1,1,h,h,270,180);p.CloseFigure();
       using(var b=new SolidBrush(fill))g.FillPath(b,p);using(var pen=new Pen(edge,Math.Max(1.5f,textHeight/10f)))g.DrawPath(pen,p);}
      using(var b=new SolidBrush(ink))g.DrawString(text,font,b,1+padX,1+padY,StringFormat.GenericTypographic);}
     Blit(bmp);
    }
   }
  }
  void Blit(Bitmap bmp){
   // A delayed mouse-leave after Hide must never bring a closed inspect button back.
   if(!Visible)return;
   var screen=GetDC(IntPtr.Zero);var mem=CreateCompatibleDC(screen);var hbmp=bmp.GetHbitmap(Color.FromArgb(0));var old=SelectObject(mem,hbmp);
   try{var size=new SIZE{cx=bmp.Width,cy=bmp.Height};var pos=new Desktop.POINT{X=x,Y=y};var src=new Desktop.POINT();var blend=new BLEND{Op=0,Flags=0,Alpha=255,Format=1};UpdateLayeredWindow(Handle,screen,ref pos,ref size,mem,ref src,0,ref blend,2);}
   finally{SelectObject(mem,old);DeleteObject(hbmp);DeleteDC(mem);ReleaseDC(IntPtr.Zero,screen);}
   SetWindowPos(Handle,new IntPtr(-1),0,0,0,0,0x2|0x1|0x10|0x40);
  }
 }
 public static class Desktop {
  // The close button is a deep red (~#480D0D) with almost no green/blue; red bricks or blood in the scene are not.
  static bool CloseRed(Color c){return c.R>=40&&c.R<=120&&c.G<=24&&c.B<=24;}
  static bool TitleGrey(Color c){return c.R>=12&&c.R<=40&&Math.Abs(c.R-c.G)<6&&Math.Abs(c.G-c.B)<6;}
  // Is the inspect window's close button still at (cx,cy)? r = half the red box tested; the title bar's
  // grey must also sit just left of it (2.5 r), so a red scene behind a closed window does not count.
  public static bool CloseVisible(Bitmap b,int cx,int cy,int r){
   int hit=0,total=0;for(int y=Math.Max(0,cy-r);y<Math.Min(b.Height,cy+r);y+=2)for(int x=Math.Max(0,cx-r);x<Math.Min(b.Width,cx+r);x+=2){total++;if(CloseRed(b.GetPixel(x,y)))hit++;}
   int gx=cx-r*5/2,grey=0;if(gx<0)return false;for(int d=-r/2;d<=r/2;d+=2){if(TitleGrey(b.GetPixel(gx,Math.Min(b.Height-1,Math.Max(0,cy+d)))))grey++;}
   return total>0&&hit*4>=total&&grey*2>=(r/2+1);
  }
  // Tiny screen grab (button plus a strip of title bar to its left) so a shown price drops the moment the window closes.
  public static bool StillOpen(int cx,int cy,int r){
   int left=r*3;using(var b=new Bitmap(left+r,2*r,PixelFormat.Format32bppArgb)){using(var g=Graphics.FromImage(b))g.CopyFromScreen(cx-left,cy-r,0,0,b.Size,CopyPixelOperation.SourceCopy);return CloseVisible(b,left,r,r);}
  }
  public static void SaveGrab(int x,int y,int w,int h,string path){using(var b=new Bitmap(Math.Max(1,w),Math.Max(1,h),PixelFormat.Format32bppArgb)){using(var g=Graphics.FromImage(b))g.CopyFromScreen(x,y,0,0,b.Size,CopyPixelOperation.SourceCopy);b.Save(path,ImageFormat.Png);}}
  static bool Toolbar(Color c){return c.R>=15&&c.R<=48&&Math.Abs(c.R-c.G)<7&&Math.Abs(c.G-c.B)<7;}
  // Top of the inspect window's button row (장착하기/버리기): the first flat grey band (>=20 px) under the
  // near-black picture panel, at three columns left of the window's right edge. Frame pixels; -1 if not found.
  public static int PanelBottom(Bitmap b,int windowRight,int titleBottom,int titleHeight){
   int[] xs={windowRight-titleHeight,windowRight-titleHeight*2,windowRight-titleHeight*3};
   foreach(var x in xs)if(x<0||x>=b.Width)return -1;
   int band=Math.Max(12,titleHeight/3),limit=Math.Min(b.Height-band-1,titleBottom+titleHeight*30);
   for(int y=titleBottom+titleHeight*3;y<limit;y+=2){
    bool ok=true;
    foreach(var x in xs){var above=b.GetPixel(x,y-4);if(above.R+above.G+above.B>45){ok=false;break;}for(int d=0;d<band&&ok;d+=4)if(!Toolbar(b.GetPixel(x,y+d)))ok=false;if(!ok)break;}
    if(ok)return y;
   }
   return -1;
  }
  [StructLayout(LayoutKind.Sequential)] public struct RECT{public int Left,Top,Right,Bottom;}
  [StructLayout(LayoutKind.Sequential)] public struct POINT{public int X,Y;}
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h,out RECT r);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h,ref POINT p);
  [DllImport("user32.dll")] static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  // Process.GetProcessById snapshots every process; resolve the name only when the foreground pid changes.
  static uint lastPid;static bool lastIsGame;
  public static IntPtr Game(){
   var h=GetForegroundWindow();uint pid;GetWindowThreadProcessId(h,out pid);
   if(pid!=lastPid){lastPid=pid;try{lastIsGame=Process.GetProcessById((int)pid).ProcessName.Equals("EscapeFromTarkov",StringComparison.OrdinalIgnoreCase);}catch{lastIsGame=false;}}
   return lastIsGame&&!IsIconic(h)?h:IntPtr.Zero;
  }
  public static Rectangle Bounds(IntPtr h){RECT r;POINT p=new POINT();if(!GetClientRect(h,out r)||!ClientToScreen(h,ref p))return Rectangle.Empty;return new Rectangle(p.X,p.Y,r.Right-r.Left,r.Bottom-r.Top);}
  public static Bitmap Capture(IntPtr h){var r=Bounds(h);if(r.Width<100||r.Height<100||r.Width>10000||r.Height>10000)throw new Exception(Language.T("게임 화면 크기 오류"));using(var full=new Bitmap(r.Width,r.Height,PixelFormat.Format32bppArgb)){using(var g=Graphics.FromImage(full))g.CopyFromScreen(r.Location,Point.Empty,r.Size,CopyPixelOperation.SourceCopy);int w=Math.Min(2048,r.Width),height=(int)Math.Round((double)r.Height*w/r.Width);var b=new Bitmap(w,height,PixelFormat.Format32bppArgb);using(var g=Graphics.FromImage(b)){g.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;g.DrawImage(full,0,0,w,height);}return b;}}
  // Full-resolution grab without the bicubic resize (that resize cost ~125 ms per 4K frame).
  public static Bitmap Grab(IntPtr h){var r=Bounds(h);if(r.Width<100||r.Height<100||r.Width>10000||r.Height>10000)throw new Exception(Language.T("게임 화면 크기 오류"));var b=new Bitmap(r.Width,r.Height,PixelFormat.Format32bppArgb);try{using(var g=Graphics.FromImage(b))g.CopyFromScreen(r.Location,Point.Empty,r.Size,CopyPixelOperation.SourceCopy);return b;}catch{b.Dispose();throw;}}
  // Nearest-neighbour BGRA sample at <=maxWidth: same geometry TitleBars was tuned on, a few ms instead of a resize.
  public static byte[] Sample(Bitmap b,int maxWidth,out int w,out int h){
   w=Math.Min(maxWidth,b.Width);h=(int)Math.Round((double)b.Height*w/b.Width);var outp=new int[w*h];var row=new int[b.Width];var sx=new int[w];
   for(int x=0;x<w;x++)sx[x]=Math.Min(b.Width-1,(int)((long)x*b.Width/w));
   var d=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
   try{for(int y=0;y<h;y++){int sy=Math.Min(b.Height-1,(int)((long)y*b.Height/h));Marshal.Copy(IntPtr.Add(d.Scan0,sy*d.Stride),row,0,row.Length);int o=y*w;for(int x=0;x<w;x++)outp[o+x]=row[sx[x]];}}
   finally{b.UnlockBits(d);}
   var bytes=new byte[outp.Length*4];Buffer.BlockCopy(outp,0,bytes,0,bytes.Length);return bytes;
  }
  // Title-bar OCR input: trimmed to the text, inverted to dark-on-light, 2x, with a 20 px margin.
  // Windows OCR drops words when glyphs touch the image edge ("Military cable 군용 케이블" -> "cable").
  public static byte[] TitlePng(Bitmap b,int x,int y,int w,int h){return TextPng(b,x,y,w,h,450,2,false);}
  // bright: R+G+B above which a pixel counts as text when trimming (grey category text needs ~250);
  // binarize: hard black/white after inversion, a second chance when anti-aliased glyphs misread.
  public static byte[] TextPng(Bitmap b,int x,int y,int w,int h,int bright,int scale,bool binarize){
   x=Math.Max(0,x);y=Math.Max(0,y);w=Math.Min(w,b.Width-x);h=Math.Min(h,b.Height-y);if(w<8||h<8)throw new Exception(Language.T("OCR 영역 오류"));
   int end=x;for(int xx=x;xx<x+w;xx+=2)for(int yy=y;yy<y+h;yy+=2){var p=b.GetPixel(xx,yy);if(p.R+p.G+p.B>bright){end=xx;break;}}
   int cw=Math.Max(8,Math.Min(x+w,end+30)-x),pad=20;scale=Math.Max(1,scale);
   using(var outp=new Bitmap(cw*scale+2*pad,h*scale+2*pad,PixelFormat.Format32bppArgb)){
    using(var g=Graphics.FromImage(outp)){g.Clear(Color.White);g.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;
     var invert=new ColorMatrix(new float[][]{new float[]{-1,0,0,0,0},new float[]{0,-1,0,0,0},new float[]{0,0,-1,0,0},new float[]{0,0,0,1,0},new float[]{1,1,1,0,1}});
     using(var attr=new ImageAttributes()){attr.SetColorMatrix(invert);g.DrawImage(b,new Rectangle(pad,pad,cw*scale,h*scale),x,y,cw,h,GraphicsUnit.Pixel,attr);}}
    if(binarize){var d=outp.LockBits(new Rectangle(0,0,outp.Width,outp.Height),ImageLockMode.ReadWrite,PixelFormat.Format32bppArgb);
     try{var px=new int[outp.Width*outp.Height];Marshal.Copy(d.Scan0,px,0,px.Length);int cut=255*3-bright/2;
      for(int i=0;i<px.Length;i++){int c=px[i];int s=((c>>16)&255)+((c>>8)&255)+(c&255);px[i]=s<cut?unchecked((int)0xFF000000):unchecked((int)0xFFFFFFFF);}
      Marshal.Copy(px,0,d.Scan0,px.Length);}finally{outp.UnlockBits(d);}}
    using(var ms=new System.IO.MemoryStream()){outp.Save(ms,ImageFormat.Png);return ms.ToArray();}
   }
  }
  public static byte[] Pixels(Bitmap b){var d=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);try{byte[] pixels=new byte[b.Width*b.Height*4];for(int y=0;y<b.Height;y++)Marshal.Copy(IntPtr.Add(d.Scan0,y*d.Stride),pixels,y*b.Width*4,b.Width*4);return pixels;}finally{b.UnlockBits(d);}}
  // The exit list region (right 55 %, top 72 %, doubled) as PNG, captured on a pool thread: the helper's loop keeps
  // reading keys meanwhile (done inline it held the loop ~0.25-0.5 s per read, so the map key went unanswered).
  public static System.Threading.Tasks.Task<byte[]> ExitListPngAsync(IntPtr game){return System.Threading.Tasks.Task.Run(()=>{using(var f=Capture(game))return CropPng(f,(int)(f.Width*.45),0,(int)(f.Width*.55),(int)(f.Height*.72),2);});}
  public static byte[] CropPng(Bitmap b,int x,int y,int w,int h,int scale){x=Math.Max(0,x);y=Math.Max(0,y);w=Math.Min(w,b.Width-x);h=Math.Min(h,b.Height-y);if(w<1||h<1)throw new Exception(Language.T("OCR 영역 오류"));scale=Math.Min(scale,Math.Max(1,2500/w));using(var crop=new Bitmap(w*scale,h*scale)){using(var g=Graphics.FromImage(crop)){g.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;g.DrawImage(b,new Rectangle(0,0,crop.Width,crop.Height),new Rectangle(x,y,w,h),GraphicsUnit.Pixel);}using(var ms=new System.IO.MemoryStream()){crop.Save(ms,ImageFormat.Png);return ms.ToArray();}}}
 }
 // Polls mouse/Esc state; no global hook. Two ways open an inspect window: right click (context menu)
 // then a left release within 3 s ("Inspect"), or a left double-click on the item. While a raid runs
 // the OS cursor must be visible, so aiming (right) + firing (left, double taps) never triggers a capture.
 public class ClickTrigger {
  [StructLayout(LayoutKind.Sequential)] struct CURSORINFO{public int cbSize,flags;public IntPtr hCursor;public Desktop.POINT pt;}
  [DllImport("user32.dll")] static extern short GetAsyncKeyState(int key);
  [DllImport("user32.dll")] static extern bool GetCursorInfo(ref CURSORINFO info);
  [DllImport("user32.dll")] static extern uint GetDoubleClickTime();
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
  bool left,right,esc;long rightAt=-1,clickAt=-1;int clickX,clickY;public int X,Y;
  // The last Inspect came from a double-click (containers open their own window that way, not the inspect window).
  public bool Double;
  // Windows' double-click time plus one poll of slack; distance is 3x the system box (4 px is tiny at 4K).
  static readonly long doubleMs=GetDoubleClickTime()+60;static readonly int doubleW=Math.Max(12,GetSystemMetrics(36)*3),doubleH=Math.Max(12,GetSystemMetrics(37)*3);
  public const int None=0,Inspect=1,Click=2,Escape=3;
  public static bool CursorShown(){var c=new CURSORINFO();c.cbSize=Marshal.SizeOf(c);return GetCursorInfo(ref c)&&(c.flags&1)!=0;}
  static bool Down(int vk){return GetAsyncKeyState(vk)<0;}
  public void Reset(){left=Down(1);right=Down(2);esc=Down(27);rightAt=-1;clickAt=-1;}
  // The fast part stays in C#: sample buttons every 20 ms (a click lasts 50-120 ms, so none is missed) and
  // return on the first event or after maxMs. The PowerShell loop then wakes ~4x/s instead of 40x/s.
  public readonly Stopwatch Clock=Stopwatch.StartNew();
  public int Wait(bool requireCursor,int maxMs){
   long until=Clock.ElapsedMilliseconds+maxMs;
   while(true){int e=Poll(requireCursor,Clock.ElapsedMilliseconds);if(e!=None)return e;if(Clock.ElapsedMilliseconds>=until)return None;System.Threading.Thread.Sleep(20);}
  }
  public int Poll(bool requireCursor,long nowMs){
   bool l=Down(1),r=Down(2),e=Down(27);bool rPress=r&&!right,lRelease=!l&&left,ePress=e&&!esc;left=l;right=r;esc=e;
   if(ePress){rightAt=-1;clickAt=-1;return Escape;}
   if(!rPress&&!lRelease)return None;
   if(requireCursor&&!CursorShown()){rightAt=-1;clickAt=-1;return None;}
   if(rPress){rightAt=nowMs;clickAt=-1;return None;}
   var c=new CURSORINFO();c.cbSize=Marshal.SizeOf(c);if(GetCursorInfo(ref c)){X=c.pt.X;Y=c.pt.Y;}
   return Left(nowMs,X,Y);
  }
  // Left release at (x,y): menu pick after a right click, second half of a double-click, or a plain click.
  public int Left(long nowMs,int x,int y){
   if(rightAt>=0&&nowMs-rightAt<=3000){rightAt=-1;clickAt=-1;Double=false;return Inspect;}
   if(clickAt>=0&&nowMs-clickAt<=doubleMs&&Math.Abs(x-clickX)<=doubleW&&Math.Abs(y-clickY)<=doubleH){clickAt=-1;Double=true;return Inspect;}
   clickAt=nowMs;clickX=x;clickY=y;return Click;
  }
 }
}
