using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Collections;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
using System.Windows.Forms;
namespace ModePulse {
 public static class I18n {
  // Codes first: static fields initialise in order and ReadLanguage needs it.
  public static readonly string[] Codes={"ko","en","zh-CN","ja","ru","es","pt-BR","de","fr","it","pl","tr","zh-TW"};
  public static readonly string Language=ReadLanguage();
  // tarkov.dev locale: Brazilian Portuguese uses "pt"; Traditional Chinese has no tarkov.dev locale, so names come in Simplified.
  public static string ApiLocale{get{return Language=="zh-CN"||Language=="zh-TW"?"zh":Language=="pt-BR"?"pt":Language;}}
  public static string FontName{get{return Language=="ko"?"Malgun Gothic":Language=="ja"?"Yu Gothic UI":Language=="zh-CN"?"Microsoft YaHei UI":Language=="zh-TW"?"Microsoft JhengHei UI":"Segoe UI";}}
  static readonly Dictionary<string,string> words=Load();
  static string ReadLanguage(){var value=Microsoft.Win32.Registry.GetValue("HKEY_CURRENT_USER\\Software\\ModeSwitch","Language","ko") as string;return value!=null&&Array.IndexOf(Codes,value)>0?value:"ko";}
  static string Decode(string s){return s.Replace("\\r\\n","\r\n").Replace("\\n","\n").Replace("\\u0020"," ");}
  static Dictionary<string,string> Load(){
   var result=new Dictionary<string,string>();int col=Math.Max(0,Array.IndexOf(Codes,Language));
   if(col==0)return result;var path=System.IO.Path.Combine(System.IO.Path.GetDirectoryName(typeof(I18n).Assembly.Location),"Localization.tsv");
   foreach(var line in System.IO.File.ReadAllLines(path,System.Text.Encoding.UTF8)){if(line.Length==0||line.StartsWith("#"))continue;var cells=line.Split('\t');if(cells.Length>col)result[Decode(cells[0])]=Decode(cells[col]);}return result;
  }
  public static string Text(string source){string value;return source!=null&&words.TryGetValue(source,out value)?value:source;}
  public static string Format(string source,params object[] values){return string.Format(System.Globalization.CultureInfo.GetCultureInfo(Language),Text(source),values);}
 }
 public static class Theme {
  [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr h,int attribute,ref int value,int size);
  // Dark title bar like BlueAsh's own windows (attribute 20; 19 on older Windows 10 builds).
  public static void DarkTitle(Form f){EventHandler set=delegate{int on=1;if(DwmSetWindowAttribute(f.Handle,20,ref on,4)!=0)DwmSetWindowAttribute(f.Handle,19,ref on,4);};if(f.IsHandleCreated)set(f,EventArgs.Empty);else f.HandleCreated+=set;}
  public static readonly Color Bg=Color.FromArgb(7,12,27),Panel=Color.FromArgb(13,22,44),Field=Color.FromArgb(21,34,66),Ink=Color.FromArgb(247,249,255),Muted=Color.FromArgb(158,174,204),Accent=Color.FromArgb(47,108,255),AccentText=Color.FromArgb(102,145,255),Border=Color.FromArgb(33,53,96),Hover=Color.FromArgb(22,46,104),AccentHover=Color.FromArgb(67,119,255),AccentDown=Color.FromArgb(30,78,210);
  public static Color Surface(Control c){var parent=c.Parent;while(parent!=null&&parent.BackColor.A==0)parent=parent.Parent;return parent==null?Panel:parent.BackColor;}
  public static void Round(Graphics g,RectangleF box,float radius,Color fill,Color edge,float width){
   if(box.Width<=0||box.Height<=0)return;float d=Math.Min(radius*2,Math.Min(box.Width,box.Height));
   using(var path=new GraphicsPath()){path.AddArc(box.Left,box.Top,d,d,180,90);path.AddArc(box.Right-d,box.Top,d,d,270,90);path.AddArc(box.Right-d,box.Bottom-d,d,d,0,90);path.AddArc(box.Left,box.Bottom-d,d,d,90,90);path.CloseFigure();
    using(var b=new SolidBrush(fill))g.FillPath(b,path);using(var p=new Pen(edge,width))g.DrawPath(p,path);}
  }
  public static void Apply(Control c){
   // The map overlay styles itself (no stretched carbon behind a full-screen-size canvas: it cost every repaint).
   if(c is MapCanvas||c is MapWindow)return;c.ForeColor=Ink;c.BackColor=c is Label&&c.Parent is ThemeCard?Panel:c is Label||c is CheckBox||c is FlowLayoutPanel?Color.Transparent:Panel; // a see-through label on a rounded card painted a dark box over its neighbours
   var form=c as Form;if(form!=null){DarkTitle(form);form.BackColor=Bg;}
   var button=c as Button;if(button!=null){button.UseVisualStyleBackColor=false;button.FlatStyle=FlatStyle.Flat;button.BackColor=Field;button.FlatAppearance.BorderColor=Border;button.FlatAppearance.MouseOverBackColor=Hover;button.FlatAppearance.MouseDownBackColor=Accent;}
   var check=c as CheckBox;if(check!=null)check.UseVisualStyleBackColor=false;var text=c as TextBoxBase;if(text!=null){text.BackColor=Field;text.BorderStyle=BorderStyle.FixedSingle;}var combo=c as ComboBox;if(combo!=null){combo.BackColor=Field;combo.FlatStyle=FlatStyle.Flat;}var number=c as NumericUpDown;if(number!=null){number.BackColor=Field;number.BorderStyle=BorderStyle.FixedSingle;}
   foreach(Control child in c.Controls)Apply(child);var map=c as MapWindow;if(map!=null)map.State.ForeColor=AccentText;
  }
 }
 public class ThemeCard:Panel {
  public ThemeCard(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);}
  protected override void OnPaintBackground(PaintEventArgs e){var g=e.Graphics;g.Clear(Theme.Surface(this));g.SmoothingMode=SmoothingMode.AntiAlias;Theme.Round(g,new RectangleF(0.75f,0.75f,Width-1.5f,Height-1.5f),10f*DeviceDpi/96f,BackColor,Theme.Border,1f);}
 }
 // Embedded pages repaint their own surface without painting over the native child inputs.
 public class PageForm:Form {
  public PageForm(){DoubleBuffered=true;}
  protected override CreateParams CreateParams{get{var p=base.CreateParams;p.Style|=0x02000000;return p;}}
 }
 // Short status messages stay quiet; longer wrapped errors retain the usual text selection and scrolling.
 public class CompactStatusBox:TextBox {
  bool sizing;
  public CompactStatusBox(){Multiline=true;ReadOnly=true;WordWrap=true;}
  void FitScroll(){
   if(sizing||Width<16||Height<8)return;sizing=true;
   try{var flags=TextFormatFlags.WordBreak|TextFormatFlags.TextBoxControl|TextFormatFlags.NoPrefix;
    int height=TextRenderer.MeasureText(Text,Font,new Size(Math.Max(1,Width-8),0),flags).Height;
    var next=height>ClientSize.Height-4?ScrollBars.Vertical:ScrollBars.None;if(ScrollBars!=next)ScrollBars=next;
   }finally{sizing=false;}
  }
  protected override void OnTextChanged(EventArgs e){base.OnTextChanged(e);FitScroll();}
  protected override void OnSizeChanged(EventArgs e){base.OnSizeChanged(e);FitScroll();}
  protected override void OnFontChanged(EventArgs e){base.OnFontChanged(e);FitScroll();}
 }
 // Rounded actions keep their surrounding corners on the parent's actual surface, including hideout cards.
 public class ThemeButton:Button {
  bool hot,pressed;
  public ThemeButton(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer,true);FlatStyle=FlatStyle.Flat;FlatAppearance.BorderSize=0;UseVisualStyleBackColor=false;Cursor=Cursors.Hand;}
  protected override void OnMouseEnter(EventArgs e){hot=true;Invalidate();base.OnMouseEnter(e);}
  protected override void OnMouseLeave(EventArgs e){hot=false;pressed=false;Invalidate();base.OnMouseLeave(e);}
  protected override void OnMouseDown(MouseEventArgs e){if(e.Button==MouseButtons.Left){pressed=true;Invalidate();}base.OnMouseDown(e);}
  protected override void OnMouseUp(MouseEventArgs e){pressed=false;Invalidate();base.OnMouseUp(e);}
  protected override void OnKeyDown(KeyEventArgs e){if(e.KeyCode==Keys.Space){pressed=true;Invalidate();}base.OnKeyDown(e);}
  protected override void OnKeyUp(KeyEventArgs e){pressed=false;Invalidate();base.OnKeyUp(e);}
  protected override void OnGotFocus(EventArgs e){Invalidate();base.OnGotFocus(e);}
  protected override void OnLostFocus(EventArgs e){pressed=false;Invalidate();base.OnLostFocus(e);}
  protected override void OnPaint(PaintEventArgs e){
   var g=e.Graphics;g.Clear(Theme.Surface(this));g.SmoothingMode=SmoothingMode.AntiAlias;
   bool primary=BackColor==Theme.Accent;var fill=!Enabled?Theme.Field:pressed?(primary?Theme.AccentDown:Theme.Hover):hot?(primary?Theme.AccentHover:Theme.Hover):BackColor;
   var edge=Focused&&ShowFocusCues?Theme.AccentText:primary?fill:Theme.Border;Theme.Round(g,new RectangleF(0.75f,0.75f,Width-1.5f,Height-1.5f),8f*DeviceDpi/96f,fill,edge,Focused&&ShowFocusCues?1.6f:1f);
   // Short buttons (level numbers, tabs) draw one centred line: word-break layout sat the text a few pixels low.
   var single=Height<40*DeviceDpi/96;var text=single?ClientRectangle:Rectangle.Inflate(ClientRectangle,-6,-3);
   TextRenderer.DrawText(g,Text,Font,text,Enabled?ForeColor:Theme.Muted,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter|TextFormatFlags.NoPrefix|TextFormatFlags.NoPadding|(single?TextFormatFlags.SingleLine|TextFormatFlags.EndEllipsis:TextFormatFlags.WordBreak));
  }
 }
 // Extra windows show BlueAsh's mark: the helper runs as powershell.exe, so the window would carry PowerShell's icon on
 // its title bar and taskbar button. It gets BlueAsh.exe's icon, and its own AppUserModelID with BlueAsh.exe as the
 // relaunch icon, so the taskbar groups it as "BlueAsh" with that mark instead of under PowerShell.
 public static class Brand {
  [ComImport,Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
  interface IPropertyStore{[PreserveSig]int GetCount(out uint c);[PreserveSig]int GetAt(uint i,out PropertyKey k);[PreserveSig]int GetValue(ref PropertyKey k,out PropVariant v);[PreserveSig]int SetValue(ref PropertyKey k,ref PropVariant v);[PreserveSig]int Commit();}
  [StructLayout(LayoutKind.Sequential,Pack=4)] struct PropertyKey{public Guid fmtid;public uint pid;}
  [StructLayout(LayoutKind.Explicit)] struct PropVariant{[FieldOffset(0)]public ushort vt;[FieldOffset(8)]public IntPtr p;}
  [DllImport("shell32.dll")] static extern int SHGetPropertyStoreForWindow(IntPtr hwnd,ref Guid iid,[MarshalAs(UnmanagedType.Interface)] out IPropertyStore store);
  static readonly Guid AppUserModel=new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3");
  static void Set(IPropertyStore s,uint pid,string value){var k=new PropertyKey{fmtid=AppUserModel,pid=pid};var v=new PropVariant{vt=31,p=Marshal.StringToCoTaskMemUni(value)};try{s.SetValue(ref k,ref v);}finally{Marshal.FreeCoTaskMem(v.p);}}
  public static void Apply(Form f,string exe){
   if(string.IsNullOrEmpty(exe)||!System.IO.File.Exists(exe))return;
   try{f.Icon=Icon.ExtractAssociatedIcon(exe);}catch{}
   EventHandler set=delegate{try{var iid=typeof(IPropertyStore).GUID;IPropertyStore s;if(SHGetPropertyStoreForWindow(f.Handle,ref iid,out s)!=0||s==null)return;
    // PKEY_AppUserModel_ID (5), RelaunchCommand (2), RelaunchIconResource (3), RelaunchDisplayNameResource (4).
    Set(s,5,"BlueAsh.Helper");Set(s,2,"\""+exe+"\"");Set(s,3,exe+",0");Set(s,4,"BlueAsh");s.Commit();Marshal.ReleaseComObject(s);}catch{}};
   if(f.IsHandleCreated)set(f,EventArgs.Empty);else f.HandleCreated+=set;
  }
 }
 // Shows a form inside a BlueAsh main-window page (another process): borderless child at x,y,w,h.
 // BlueAsh.exe is DPI-unaware while this helper is DPI-aware, so the form's windows are created unaware
 // (per-thread context, Windows 10 1607+); otherwise sizes and coordinates disagree on scaled displays.
 public static class Embed {
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern bool SetProp(IntPtr h,string name,IntPtr value);
  public static void Identify(Form f,string page){EventHandler tag=delegate{SetProp(f.Handle,"BlueAsh.Page."+page,new IntPtr(1));};if(f.IsHandleCreated)tag(f,EventArgs.Empty);else f.HandleCreated+=tag;}
  [DllImport("user32.dll")] static extern IntPtr SetParent(IntPtr child,IntPtr parent);
  [DllImport("user32.dll",EntryPoint="GetWindowLongW")] static extern int GetWindowLong(IntPtr h,int index);
  [DllImport("user32.dll",EntryPoint="SetWindowLongW")] static extern int SetWindowLong(IntPtr h,int index,int value);
  [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int height,uint flags);
  [DllImport("user32.dll")] static extern bool IsWindow(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] struct Rect{public int Left,Top,Right,Bottom;}
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h,out Rect rect);
  [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
  public static bool Attach(Form f,IntPtr host,int x,int y,int w,int height){
   if(host==IntPtr.Zero||!IsWindow(host))return false;
   // The whole attach runs unaware: window creation and the x,y,w,h (main-window units) both depend on it.
   IntPtr old=IntPtr.Zero;try{old=SetThreadDpiAwarenessContext(new IntPtr(-1));}catch(EntryPointNotFoundException){}
   try{
    f.Hide();f.SuspendLayout();f.FormBorderStyle=FormBorderStyle.None;f.ShowInTaskbar=false;f.StartPosition=FormStartPosition.Manual;
    Rect rect;if(GetClientRect(host,out rect)){w=Math.Max(420,rect.Right-308);height=Math.Max(200,Math.Min(height,rect.Bottom-158));}
    var hwnd=f.Handle;int style=GetWindowLong(hwnd,-16);
    style=unchecked((int)(((uint)style&~0x80000000u&~0x00C00000u&~0x00040000u)|0x40000000u));SetWindowLong(hwnd,-16,style);
    SetParent(hwnd,host);SetWindowPos(hwnd,IntPtr.Zero,x,y,w,height,0x0004|0x0020);f.ResumeLayout(true);f.Show();f.Update();return true;
   }finally{if(old!=IntPtr.Zero)SetThreadDpiAwarenessContext(old);}
  }
 }
 public static class Input {
  [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
  [DllImport("user32.dll")] public static extern short GetAsyncKeyState(int key);
  // Ctrl/Alt/Shift held: the key belongs to a game combo (Ctrl+M = bipod), not to the map.
  public static bool ModifierDown(){return GetAsyncKeyState(0x11)<0||GetAsyncKeyState(0x12)<0||GetAsyncKeyState(0x10)<0;}
  // Waits in C# (still pumping UI messages every 15 ms) until a watched key changes state or maxMs passes,
  // so the PowerShell loop wakes ~4x/s instead of 25x/s (it cost ~27% of a core while the game ran).
  public static void WaitKeys(int[] codes,int maxMs){
   var was=new bool[codes.Length];for(int i=0;i<codes.Length;i++)was[i]=GetAsyncKeyState(codes[i])<0;int start=Environment.TickCount;
   while(unchecked(Environment.TickCount-start)<maxMs){
    Application.DoEvents();System.Threading.Thread.Sleep(15);
    if(Waiting())return;
    for(int i=0;i<codes.Length;i++)if((GetAsyncKeyState(codes[i])<0)!=was[i])return;
   }
  }
  // Key presses seen by a background thread every 8 ms, so a tap is not lost while the helper's loop is busy (an exit
  // list read holds it for a few hundred ms: the map key pressed then did nothing). A press with Ctrl/Alt/Shift held is
  // a game combo and not counted. Only key states are read, as before; nothing is hooked.
  static readonly object watchLock=new object();static int[] watched=new int[0];static readonly Dictionary<int,int> presses=new Dictionary<int,int>();static System.Threading.Thread watcher;
  public static void Watch(int[] codes){lock(watchLock){watched=(int[])codes.Clone();foreach(var k in new List<int>(presses.Keys))if(Array.IndexOf(watched,k)<0)presses.Remove(k);}
   if(watcher!=null)return;watcher=new System.Threading.Thread(()=>{var down=new Dictionary<int,bool>();
    while(true){int[] now;lock(watchLock)now=watched;bool mod=ModifierDown();
     foreach(var k in now){bool d=GetAsyncKeyState(k)<0;bool was;down.TryGetValue(k,out was);if(d&&!was&&!mod){lock(watchLock){int n;presses.TryGetValue(k,out n);presses[k]=n+1;}}down[k]=d;}
     System.Threading.Thread.Sleep(8);}}){IsBackground=true,Name="BlueAsh keys"};watcher.Start();}
  // True once per press since the last call.
  public static bool TakePress(int code){lock(watchLock){int n;if(!presses.TryGetValue(code,out n)||n==0)return false;presses[code]=0;return true;}}
  static bool Waiting(){lock(watchLock){foreach(var v in presses.Values)if(v>0)return true;return false;}}
  [StructLayout(LayoutKind.Sequential)] struct LASTINPUTINFO{public uint cbSize,dwTime;}
  [DllImport("user32.dll")] static extern bool GetLastInputInfo(ref LASTINPUTINFO info);
  public static uint Idle(){var i=new LASTINPUTINFO();i.cbSize=(uint)Marshal.SizeOf(i);return GetLastInputInfo(ref i)?unchecked((uint)Environment.TickCount-i.dwTime):0;}
 }
 // Name = what the in-game exit list shows (OCR matches it); Condition shows in the hover tooltip.
 // Kind: exit, transit (both matched against the exit list), pmc (PMC spawn), boss, cultist.
 // quest: Count objectives merged into one pin, Outline = objective zone (normalised), Condition = one line each.
 public class Pin { public string Name="";public float X,Y;public bool Taxi;public string Destination="";public string Condition="";public string Kind="exit";public int Count=1;public PointF[] Outline;public string Note="";public string Ref="";public bool Faded;public string Guards=""; }
 // Draw the same vector sign on the map and in the preview. Scav-only exits are not co-op exits.
 public static class ExitSign {
  public static readonly Color Green=Color.FromArgb(0,135,56),Orange=Color.FromArgb(218,128,35);
  public static bool Cooperative(Pin p){return p.Kind=="exit"&&Regex.IsMatch((p.Name??"")+" "+(p.Condition??""),@"co[\s-]?op|PMC\s*\+\s*Scav|Scav\s*\+\s*PMC",RegexOptions.IgnoreCase);}
  static GraphicsPath Tile(float y){var p=new GraphicsPath();p.AddPolygon(new[]{new PointF(6,y+1),new PointF(29,y+1),new PointF(24,y+23),new PointF(1,y+23)});return p;}
  // Reconstructed from the user's emergency-exit reference: coloured figure in a white doorway.
  static readonly GraphicsPath person=PersonPath();
  static GraphicsPath PersonPath(){var p=new GraphicsPath{FillMode=FillMode.Winding};p.AddEllipse(36,0,48,48);
   p.StartFigure();p.AddLine(79,132,58,93);p.AddLine(58,93,37,118);p.AddLine(37,118,0,118);p.AddLine(0,118,0,95);p.AddLine(0,95,24,95);p.AddLine(24,95,58,58);p.AddBezier(58,58,65,51,69,50,78,50);p.AddLine(78,50,141,50);p.AddLine(141,50,171,95);p.AddBezier(171,95,179,108,164,120,153,109);p.AddLine(153,109,131,72);p.AddLine(131,72,108,72);p.AddLine(108,72,128,126);p.AddLine(128,126,137,185);p.AddLine(137,185,200,185);p.AddBezier(200,185,204,185,206,190,207,194);p.AddLine(207,194,211,208);p.AddLine(211,208,124,208);p.AddBezier(124,208,117,208,114,204,113,199);p.AddLine(113,199,103,142);p.AddLine(103,142,52,248);p.AddLine(52,248,26,248);p.AddLine(26,248,79,132);p.CloseFigure();return p;
  }
  static void Person(Graphics g,float x,float y,float scale,Color color,float outline){
   var state=g.Save();try{g.TranslateTransform(x,y);g.ScaleTransform(scale,scale);
    using(var rim=new Pen(Color.White,outline){LineJoin=LineJoin.Round})g.DrawPath(rim,person);
    using(var fill=new SolidBrush(color))g.FillPath(fill,person);
   }finally{g.Restore(state);}
  }
  static void Door(Graphics g,bool cooperative){
   var state=g.Save();try{g.TranslateTransform(3.8f,0.7f);g.ScaleTransform(0.063f,0.063f);
    float left=cooperative?75:85,right=cooperative?277:265;
    using(var opening=new GraphicsPath()){opening.AddPolygon(new[]{new PointF(left,37),new PointF(right,37),new PointF(right,308),new PointF(right+42,349),new PointF(left+43,349),new PointF(left,308)});g.FillPath(Brushes.White,opening);}
   }finally{g.Restore(state);}
  }
  public static void Draw(Graphics g,float x,float y,float size,Color color,bool cooperative){
   var state=g.Save();try{g.TranslateTransform(x-size*.625f,y-size/2);g.ScaleTransform(size/24,size/24);g.SmoothingMode=SmoothingMode.AntiAlias;
    using(var shadow=Tile(0.4f))using(var fill=new SolidBrush(Color.FromArgb(100,3,10,15)))g.FillPath(fill,shadow);
    using(var tile=Tile(0))using(var fill=new SolidBrush(color))using(var edge=new Pen(Color.FromArgb(170,3,35,20),0.55f){LineJoin=LineJoin.Round}){g.FillPath(fill,tile);g.DrawPath(edge,tile);}
    using(var lean=new Matrix(1,0,-0.14f,1,1.68f,0))g.MultiplyTransform(lean);
    Door(g,cooperative);
    // Floor threshold is the same colour as the sign, retaining the reference's negative-space layout.
    using(var floor=new GraphicsPath())using(var fill=new SolidBrush(color)){floor.AddPolygon(new[]{new PointF(10.667f,20.104f),new PointF(12.935f,20.104f),new PointF(15.581f,22.687f),new PointF(13.313f,22.687f)});g.FillPath(fill,floor);}
    if(cooperative){Person(g,12.6f,3.95f,0.0565f,color,7);Person(g,8.45f,5.2f,0.061f,color,14);}
    else Person(g,9.155f,3.976f,0.063f,color,7);
   }finally{g.Restore(state);}
  }
 }
 public enum TaxiState {Unknown,Normal,Using,Gone}
 public static class TaxiMarker {
  public static bool IsTaxi(Pin p){return p!=null&&p.Kind=="exit"&&(p.Taxi||Regex.IsMatch(p.Name??"",@"\bV[\s-]?Ex\b|\bTaxi\b|\bBridge Car Extract\b",RegexOptions.IgnoreCase));}
  public static Color Colour(TaxiState state){return state==TaxiState.Using?Color.FromArgb(191,63,64):state==TaxiState.Gone?Color.FromArgb(107,117,132):ExitSign.Green;}
  public static void Draw(Graphics g,float x,float y,float size,TaxiState state){
   Color color=Colour(state);var saved=g.Save();try{g.TranslateTransform(x-size*.625f,y-size/2);g.ScaleTransform(size/24,size/24);g.SmoothingMode=SmoothingMode.AntiAlias;
    using(var tile=new GraphicsPath()){tile.AddPolygon(new[]{new PointF(6,1),new PointF(29,1),new PointF(24,23),new PointF(1,23)});using(var b=new SolidBrush(color))g.FillPath(b,tile);using(var edge=new Pen(Color.FromArgb(170,3,20,30),0.55f))g.DrawPath(edge,tile);}
    using(var lean=new Matrix(1,0,-0.14f,1,1.68f,0))g.MultiplyTransform(lean);
    using(var body=new GraphicsPath()){body.AddBezier(7.1f,12.2f,7.1f,11.4f,8.0f,11.15f,8.8f,10.9f);body.AddLine(8.8f,10.9f,10.2f,7.0f);body.AddBezier(10.2f,7,10.4f,6.5f,10.9f,6.2f,11.5f,6.2f);body.AddLine(11.5f,6.2f,18.5f,6.2f);body.AddBezier(18.5f,6.2f,19.1f,6.2f,19.6f,6.5f,19.8f,7);body.AddLine(19.8f,7,21.2f,10.9f);body.AddBezier(21.2f,10.9f,22,11.15f,22.9f,11.4f,22.9f,12.2f);body.AddLine(22.9f,12.2f,22.9f,17.2f);body.AddBezier(22.9f,17.2f,22.9f,18,22.2f,18.3f,21.4f,18.3f);body.AddLine(21.4f,18.3f,8.6f,18.3f);body.AddBezier(8.6f,18.3f,7.8f,18.3f,7.1f,18,7.1f,17.2f);body.CloseFigure();g.FillPath(Brushes.White,body);}
    using(var cut=new SolidBrush(color)){g.FillPolygon(cut,new[]{new PointF(11.5f,7.6f),new PointF(18.5f,7.6f),new PointF(19.5f,10.7f),new PointF(10.5f,10.7f)});g.FillEllipse(cut,8.8f,12.9f,2.3f,2.3f);g.FillEllipse(cut,18.9f,12.9f,2.3f,2.3f);g.FillRectangle(cut,13,13.4f,4,0.9f);g.FillRectangle(cut,13.7f,16.1f,2.6f,0.8f);}
    g.FillRectangle(Brushes.White,8.4f,17.8f,2.3f,2.3f);g.FillRectangle(Brushes.White,19.3f,17.8f,2.3f,2.3f);
    g.FillRectangle(Brushes.White,12.4f,3.3f,5.2f,2.0f);using(var check=new SolidBrush(color)){g.FillRectangle(check,13,3.8f,0.8f,0.8f);g.FillRectangle(check,14.6f,3.8f,0.8f,0.8f);g.FillRectangle(check,16.2f,3.8f,0.8f,0.8f);}
   }finally{g.Restore(saved);}
  }
 }
 // A shot-required flare exit, distinct from the stationary green light at ordinary exits.
 public static class FlareMarker {
  public static bool IsFlare(Pin p){
   if(p==null||p.Kind!="exit")return false;
   if(Regex.IsMatch(p.Name??"",@"\bFlare\b|조명탄|신호탄",RegexOptions.IgnoreCase))return true;
   const string need="Requires a green flare to be shot into the sky while inside the signal flare area.";
   var condition=p.Condition??"";return condition.Length>0&&(condition.IndexOf(need,StringComparison.OrdinalIgnoreCase)>=0||condition.IndexOf(FandomText.Line(need),StringComparison.OrdinalIgnoreCase)>=0);
  }
  public static void Draw(Graphics g,float x,float y,float size,Color color){
   var state=g.Save();try{g.TranslateTransform(x-size*2/3,y-size*2/3);g.ScaleTransform(size/24,size/24);g.SmoothingMode=SmoothingMode.AntiAlias;
    using(var tile=new GraphicsPath()){tile.AddPolygon(new[]{new PointF(16,0.65f),new PointF(31.35f,16),new PointF(16,31.35f),new PointF(0.65f,16)});
     using(var fill=new SolidBrush(color))g.FillPath(fill,tile);using(var edge=new Pen(Color.FromArgb(170,3,35,20),0.55f){LineJoin=LineJoin.Round})g.DrawPath(edge,tile);}
    // Reference proportions: a wide flare barrel and grip above a separate, centered downward arrow.
    var gun=g.Save();try{g.TranslateTransform(-0.75f,0);
     using(var guard=new GraphicsPath()){guard.AddLine(12.5f,11.3f,13.0f,14.4f);guard.AddBezier(13.0f,14.4f,13.3f,15.5f,15.2f,15.6f,16.8f,14.9f);guard.AddLine(16.8f,14.9f,19.3f,13.3f);guard.AddLine(19.3f,13.3f,17.7f,10.4f);guard.CloseFigure();g.FillPath(Brushes.White,guard);}
     g.FillPolygon(Brushes.White,new[]{new PointF(7.5f,11.25f),new PointF(15.0f,4.6f),new PointF(17.3f,7.8f),new PointF(9.8f,14.45f)});
     using(var body=new GraphicsPath()){body.AddLine(12.7f,10.0f,16.5f,7.0f);body.AddBezier(16.5f,7.0f,18.1f,6.5f,20.3f,7.2f,22.4f,8.05f);body.AddLine(22.4f,8.05f,21.3f,9.5f);body.AddLine(21.3f,9.5f,21.2f,9.9f);body.AddLine(21.2f,9.9f,25.6f,13.4f);body.AddBezier(25.6f,13.4f,26.2f,13.9f,26.2f,14.4f,25.7f,14.9f);body.AddLine(25.7f,14.9f,24.2f,16.1f);body.AddBezier(24.2f,16.1f,23.7f,16.5f,23.2f,16.4f,22.8f,16.1f);body.AddLine(22.8f,16.1f,18.7f,12.7f);body.AddLine(18.7f,12.7f,16.7f,13.8f);body.AddLine(16.7f,13.8f,14.8f,13.4f);body.AddLine(14.8f,13.4f,12.7f,12.9f);body.CloseFigure();g.FillPath(Brushes.White,body);}
     using(var cut=new SolidBrush(color)){
      using(var opening=new GraphicsPath()){opening.AddLine(14.1f,11.7f,17.0f,11.6f);opening.AddLine(17.0f,11.6f,18.0f,12.7f);opening.AddBezier(18.0f,12.7f,17.1f,13.6f,15.9f,14.2f,14.9f,14.0f);opening.AddBezier(14.9f,14.0f,14.3f,13.9f,14.1f,12.8f,14.1f,11.7f);opening.CloseFigure();g.FillPath(cut,opening);}
      g.FillEllipse(cut,18.1f,8.2f,1.25f,1.25f);
     }
     using(var muzzle=new Pen(color,0.7f){StartCap=LineCap.Round,EndCap=LineCap.Round})g.DrawLine(muzzle,8.6f,11.5f,9.8f,13.0f);
    }finally{g.Restore(gun);}
    using(var arrow=new GraphicsPath()){arrow.AddBezier(14.55f,18.6f,14.55f,18.25f,14.8f,18.1f,15.1f,18.1f);arrow.AddLine(15.1f,18.1f,16.9f,18.1f);arrow.AddBezier(16.9f,18.1f,17.2f,18.1f,17.45f,18.25f,17.45f,18.6f);arrow.AddLine(17.45f,18.6f,17.45f,21.7f);arrow.AddLine(17.45f,21.7f,20.7f,21.7f);arrow.AddBezier(20.7f,21.7f,21.35f,21.7f,21.6f,22.1f,21.15f,22.6f);arrow.AddLine(21.15f,22.6f,16.5f,27.25f);arrow.AddBezier(16.5f,27.25f,16.2f,27.55f,15.8f,27.55f,15.5f,27.25f);arrow.AddLine(15.5f,27.25f,10.85f,22.6f);arrow.AddBezier(10.85f,22.6f,10.4f,22.1f,10.65f,21.7f,11.3f,21.7f);arrow.AddLine(11.3f,21.7f,14.55f,21.7f);arrow.CloseFigure();g.FillPath(Brushes.White,arrow);}
   }finally{g.Restore(state);}
  }
 }
 public static class TaxiVision {
  // Read the colour of the extract identifier, not the name (the game can leave the name white).
  // Restrict sampling to the first word and the small identifier strip directly before it.
  public static string Colour(byte[] png,RectangleF first,bool identifier=false){
   if(png==null||png.Length==0||first.Width<=0||first.Height<=0)return "unknown";
   using(var stream=new System.IO.MemoryStream(png))using(var image=new Bitmap(stream)){
    string ink=Ink(image,Rectangle.Round(first));if(ink=="green"||ink=="red"||(identifier&&ink=="white"))return ink;
    var before=Rectangle.Round(new RectangleF(first.Left-first.Height*5.5f,first.Top,first.Height*5.1f,first.Height));
    string prefix=Ink(image,before);if(prefix=="green"||prefix=="red")return prefix;return ink=="white"||prefix=="white"?"white":"unknown";
   }
  }
  static string Ink(Bitmap b,Rectangle box){
   box=Rectangle.Intersect(box,new Rectangle(0,0,b.Width,b.Height));if(box.Width<2||box.Height<2)return "unknown";
   int red=0,green=0,white=0,total=box.Width*box.Height;
   var data=b.LockBits(box,System.Drawing.Imaging.ImageLockMode.ReadOnly,System.Drawing.Imaging.PixelFormat.Format32bppArgb);
   var row=new byte[box.Width*4];try{for(int y=0;y<box.Height;y++){Marshal.Copy(IntPtr.Add(data.Scan0,y*data.Stride),row,0,row.Length);for(int x=0;x<row.Length;x+=4){int bl=row[x],g=row[x+1],r=row[x+2],max=Math.Max(r,Math.Max(g,bl)),min=Math.Min(r,Math.Min(g,bl));if(max<135)continue;if(r>g*1.35&&r>bl*1.35)red++;else if(g>r*1.12&&g>bl*1.35)green++;else if(max-min<48)white++;}}}finally{b.UnlockBits(data);}
   int needed=Math.Max(8,(int)(box.Height*1.1f));if(red>=needed&&red>green*2&&red>white*.4&&red<total*.5)return "red";if(green>=needed&&green>red*2&&green>white*.4&&green<total*.5)return "green";return white>=needed?"white":"unknown";
  }
  public static bool Countdown(string text){
   foreach(Match m in Regex.Matches(text??"",@"(?<![\d:：])\d{1,2}(?:[:：]\d{2}){1,2}(?![\d:：])")){var p=m.Value.Replace('：',':').Split(':');int sec=0;bool valid=true;foreach(var n in p){int v;if(!int.TryParse(n,out v)||v>=60){valid=false;break;}sec=sec*60+v;}if(valid&&sec<=60)return true;}return false;
  }
 }
 // Fandom map texts are English; FandomText.tsv (next to this DLL's companion folder) holds them per UI language, one
 // description line per row. Unknown lines stay English; exit names are never translated (the game and the exit OCR use English).
 public static class FandomText {
  static Dictionary<string,string> words;
  static Dictionary<string,string> Words(){
   if(words!=null)return words;var r=new Dictionary<string,string>();int col=I18n.Language=="ko"?1:I18n.Language=="en"?0:Math.Max(0,Array.IndexOf(I18n.Codes,I18n.Language));
   if(col>0)try{var path=System.IO.Path.Combine(System.IO.Path.GetDirectoryName(typeof(FandomText).Assembly.Location),"FandomText.tsv");
    foreach(var line in System.IO.File.ReadAllLines(path,System.Text.Encoding.UTF8)){if(line.Length==0||line[0]=='#')continue;var c=line.Split('\t');if(c.Length>col&&c[col].Length>0)r[c[0]]=c[col];}}catch{}
   return words=r;
  }
  static readonly Regex prefix=new Regex(@"^(\* |\d+\.\s*)");
  public static string Line(string s){string v;var w=Words();if(w.TryGetValue(s,out v))return v;var m=prefix.Match(s);if(m.Success&&w.TryGetValue(s.Substring(m.Length),out v))return m.Value+v;return s;}
  public static string Text(string s){if(string.IsNullOrEmpty(s))return s;var lines=s.Split('\n');for(int i=0;i<lines.Length;i++)lines[i]=Line(lines[i].Trim());return string.Join("\n",lines);}
  // "Tagilla Boss Spawn" -> "Tagilla" (the boss), "Random Boss Spawn" stays as a whole.
  public static string BossOf(string name){return name.EndsWith(" Boss Spawn")&&name!="Random Boss Spawn"?name.Substring(0,name.Length-11):name;}
 }
 // Map markers from the Escape from Tarkov Fandom interactive maps (Map:<title> pages): only exits, transits,
 // PMC spawns, bosses and cultists. Positions are normalised 0..1 from the top-left of the page's base image,
 // which build-maps.ps1 ships as maps/<id>.jpg together with a snapshot (maps/markers.json).
 // At run time a fresh copy is fetched at most daily; it is used only while its base image size matches the shipped one.
 public static class FandomMarkers {
  public static readonly Dictionary<string,string> Titles=new Dictionary<string,string>{{"sandbox","Ground Zero"},{"lighthouse","Lighthouse"},{"laboratory","The Lab"},{"rezervbase","Reserve"},{"labyrinth","The Labyrinth"},{"woods","Woods"},{"shoreline","Shoreline"},{"tarkovstreets","Streets of Tarkov"},{"icebreaker","Icebreaker"},{"interchange","Interchange"},{"bigmap","Customs"},{"terminal","Terminal"},{"factory4_day","Factory"}};
  static readonly string[] Wanted={"exfil_pmc","exfil_scav","exfil_transit","spawn_pmc","spawn_boss","spawn_cultist"};
  static readonly JavaScriptSerializer js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};
  static HttpClient http;
  static double D(object o){return Convert.ToDouble(o,System.Globalization.CultureInfo.InvariantCulture);}
  public static string PageUrl(string title){return "https://escapefromtarkov.fandom.com/api.php?action=query&prop=revisions&rvprop=content&rvslots=main&format=json&titles="+Uri.EscapeDataString("Map:"+title);}
  public static byte[] GetBytes(string url){Client();return http.GetByteArrayAsync(url).Result;}
  static void Client(){if(http==null){http=new HttpClient{Timeout=TimeSpan.FromSeconds(30)};http.DefaultRequestHeaders.UserAgent.ParseAdd("Mozilla/5.0 (Windows NT 10.0; Win64; x64) BlueAsh");}}
  public static string Get(string url){if(http==null){http=new HttpClient{Timeout=TimeSpan.FromSeconds(30)};http.DefaultRequestHeaders.UserAgent.ParseAdd("Mozilla/5.0 (Windows NT 10.0; Win64; x64) BlueAsh");}return http.GetStringAsync(url).Result;}
  // Wiki markup in popups: drop images, keep link text, strip formatting.
  public static string Clean(string s){
   if(string.IsNullOrEmpty(s))return "";
   s=Regex.Replace(s,@"\[\[(File|Image):[^\]]*\]\]","",RegexOptions.IgnoreCase);s=Regex.Replace(s,@"\[\[[^\]|]*\|([^\]]*)\]\]","$1");s=Regex.Replace(s,@"\[\[([^\]]*)\]\]","$1");
   s=Regex.Replace(s,@"\[https?://\S+\s([^\]]*)\]","$1");s=s.Replace("'''","").Replace("''","");s=Regex.Replace(s,@"<br\s*/?>","\n",RegexOptions.IgnoreCase);s=Regex.Replace(s,@"<[^>]+>","");
   s=Regex.Replace(s,@"[ \t]+"," ");s=Regex.Replace(s,@"\s*\n\s*","\n");return s.Trim();
  }
  // One Map: page (API JSON) -> {image, w, h, markers:[{k,n,d,x,y}]}.
  public static Dictionary<string,object> ParsePage(string apiJson){
   var root=(IDictionary)js.DeserializeObject(apiJson);IDictionary page=null;foreach(DictionaryEntry e in (IDictionary)((IDictionary)root["query"])["pages"]){page=(IDictionary)e.Value;break;}
   var main=(IDictionary)((IDictionary)((IDictionary)((object[])page["revisions"])[0])["slots"])["main"];var m=(IDictionary)js.DeserializeObject((string)main["*"]);
   bool yx=m.Contains("coordinateOrder")&&(string)m["coordinateOrder"]=="yx",bottom=m.Contains("origin")&&(string)m["origin"]=="bottom-left";
   var hi=(object[])((object[])m["mapBounds"])[1];double w=D(hi[yx?1:0]),h=D(hi[yx?0:1]);var list=new List<object>();
   foreach(IDictionary k in (object[])m["markers"]){var cat=(string)k["categoryId"];if(Array.IndexOf(Wanted,cat)<0)continue;var p=(object[])k["position"];double x=D(p[yx?1:0]),y=D(p[yx?0:1]);
    var pop=k.Contains("popup")?k["popup"] as IDictionary:null;string n=pop!=null&&pop.Contains("title")?Clean(pop["title"] as string):"",d=pop!=null&&pop.Contains("description")?Clean(pop["description"] as string):"";
    list.Add(new Dictionary<string,object>{{"k",cat},{"n",n},{"d",d},{"x",Math.Round(x/w,5)},{"y",Math.Round(bottom?1-y/h:y/h,5)}});}
   return new Dictionary<string,object>{{"image",m["mapImage"]},{"w",w},{"h",h},{"markers",list}};
  }
  public static string FetchAll(){var maps=new Dictionary<string,object>();foreach(var t in Titles)maps[t.Key]=ParsePage(Get(PageUrl(t.Value)));return js.Serialize(new Dictionary<string,object>{{"fetched",DateTime.UtcNow.ToString("o")},{"maps",maps}});}
  // Background refresh (BelowNormal thread) when the cache is missing or older than maxAge; never blocks the UI.
  public static string LastError="";
  public static void RefreshIfStale(string cachePath,TimeSpan maxAge){
   try{if(System.IO.File.Exists(cachePath)&&DateTime.UtcNow-System.IO.File.GetLastWriteTimeUtc(cachePath)<maxAge)return;}catch{}
   var t=new System.Threading.Thread(()=>{try{var json=FetchAll();var tmp=cachePath+".tmp";System.IO.File.WriteAllText(tmp,json);if(System.IO.File.Exists(cachePath))System.IO.File.Delete(cachePath);System.IO.File.Move(tmp,cachePath);LastError="";}catch(Exception e){LastError=e.Message;}});
   t.IsBackground=true;t.Priority=System.Threading.ThreadPriority.BelowNormal;t.Start();
  }
  // Parsed files are kept until they change on disk (quest pins look up the transform once per place).
  static readonly Dictionary<string,KeyValuePair<DateTime,IDictionary>> parsed=new Dictionary<string,KeyValuePair<DateTime,IDictionary>>();
  internal static IDictionary Parsed(string path){
   try{if(!System.IO.File.Exists(path))return null;var stamp=System.IO.File.GetLastWriteTimeUtc(path);KeyValuePair<DateTime,IDictionary> hit;
    lock(parsed){if(parsed.TryGetValue(path,out hit)&&hit.Key==stamp)return hit.Value;}
    var root=(IDictionary)js.DeserializeObject(System.IO.File.ReadAllText(path));lock(parsed)parsed[path]=new KeyValuePair<DateTime,IDictionary>(stamp,root);return root;}catch{return null;}
  }
  static IDictionary MapOf(string path,string id){var root=Parsed(path);var maps=root==null?null:root["maps"] as IDictionary;return maps!=null&&maps.Contains(id)?(IDictionary)maps[id]:null;}
  // Pins for one map and side: PMC sees PMC exits, transits, PMC spawns, bosses, cultists; Scav the same minus PMC spawns.
  public static List<Pin> Pins(string id,string role,string cachePath,string shippedPath){
   var shipped=MapOf(shippedPath,id);var fresh=MapOf(cachePath,id);var use=shipped;
   if(fresh!=null&&shipped!=null&&D(fresh["w"])==D(shipped["w"])&&D(fresh["h"])==D(shipped["h"]))use=fresh;
   var pins=new List<Pin>();if(use==null)return pins;bool scav=role=="SCAV";
   foreach(IDictionary k in (IEnumerable)use["markers"]){var cat=(string)k["k"];string kind;
    if(cat=="exfil_pmc"){if(scav)continue;kind="exit";}else if(cat=="exfil_scav"){if(!scav)continue;kind="exit";}else if(cat=="exfil_transit")kind="transit";
    else if(cat=="spawn_pmc"){if(scav)continue;kind="pmc";}else if(cat=="spawn_boss")kind="boss";else if(cat=="spawn_cultist")kind="cultist";else continue;
    var pin=new Pin{Name=(string)k["n"],Condition=(string)k["d"],X=(float)D(k["x"]),Y=(float)D(k["y"]),Kind=kind};pin.Taxi=TaxiMarker.IsTaxi(pin);pins.Add(pin);}
   return pins;
  }
  // ---- Game position -> map image (for "where am I" from screenshot names) ----
  // The Fandom image has no game coordinates, so build-maps.ps1 pairs its exit markers with tarkov.dev's exits
  // (game x,z) by name and fits u=a*x+b*z+c, v=d*x+e*z+f (normalised image units) by least squares, dropping
  // mismatched pairs. The six numbers ship in maps/markers.json as "transform".
  public static string Key(string name){var s=Regex.Replace((name??"").ToLowerInvariant(),@"\(co-?op\)|co-?op|v-?ex|[^a-z0-9]","");return s;}
  static double[] Solve3(double[,] m,double[] r){
   // Gaussian elimination with partial pivoting on a 3x3 system.
   var a=new double[3,4];for(int i=0;i<3;i++){for(int j=0;j<3;j++)a[i,j]=m[i,j];a[i,3]=r[i];}
   for(int c=0;c<3;c++){int p=c;for(int i=c+1;i<3;i++)if(Math.Abs(a[i,c])>Math.Abs(a[p,c]))p=i;if(Math.Abs(a[p,c])<1e-12)return null;
    for(int j=0;j<4;j++){var t=a[c,j];a[c,j]=a[p,j];a[p,j]=t;}for(int i=0;i<3;i++)if(i!=c){var f=a[i,c]/a[c,c];for(int j=c;j<4;j++)a[i,j]-=f*a[c,j];}}
   return new[]{a[0,3]/a[0,0],a[1,3]/a[1,1],a[2,3]/a[2,2]};
  }
  static double[] FitOnce(List<double[]> pairs){
   var m=new double[3,3];var ru=new double[3];var rv=new double[3];
   foreach(var p in pairs){var q=new[]{p[0],p[1],1.0};for(int i=0;i<3;i++){for(int j=0;j<3;j++)m[i,j]+=q[i]*q[j];ru[i]+=q[i]*p[2];rv[i]+=q[i]*p[3];}}
   var u=Solve3(m,ru);var v=Solve3(m,rv);return u==null||v==null?null:new[]{u[0],u[1],u[2],v[0],v[1],v[2]};
  }
  static double Err(double[] t,double[] p){double du=t[0]*p[0]+t[1]*p[1]+t[2]-p[2],dv=t[3]*p[0]+t[4]*p[1]+t[5]-p[3];return Math.Sqrt(du*du+dv*dv);}
  // pairs: {gameX, gameZ, u, v}. Returns {a,b,c,d,e,f, used, rms} or null when fewer than 4 pairs agree.
  public static double[] Fit(List<double[]> pairs){
   var use=new List<double[]>(pairs);
   while(use.Count>=4){var t=FitOnce(use);if(t==null)return null;
    int worst=-1;double worstErr=0;foreach(var p in use){var e=Err(t,p);if(e>worstErr){worstErr=e;worst=use.IndexOf(p);}}
    // A pair off by more than 3% of the image is a name mismatch (or a different floor): drop it and refit.
    if(worstErr>0.03){use.RemoveAt(worst);continue;}
    double sum=0;foreach(var p in use){var e=Err(t,p);sum+=e*e;}
    return new[]{t[0],t[1],t[2],t[3],t[4],t[5],use.Count,Math.Sqrt(sum/use.Count)};}
   return null;
  }
  // ---- Local correction on top of the affine fit ----
  // Hand-drawn maps bend locally, so one affine map leaves places off by a few percent. Control points are the
  // matched exits plus spawn markers paired with tarkov.dev spawns (nearest after the affine fit); each stores the
  // affine residual. A point is corrected by the residuals around it, Gaussian-weighted (sigma in game metres),
  // and fades to plain affine away from control points. Shipped as "ctrl" [x,z,du,dv,...] and "sigma".
  static double[] Affine(double[] t,double x,double z){return new[]{t[0]*x+t[1]*z+t[2],t[3]*x+t[4]*z+t[5]};}
  public static double[] Correct(double[] t,double[] ctrl,double sigma,double x,double z){
   var p=Affine(t,x,z);if(ctrl==null||ctrl.Length<4||sigma<=0)return p;double su=0,sv=0,sw=0;
   for(int i=0;i+3<ctrl.Length;i+=4){double dx=ctrl[i]-x,dz=ctrl[i+1]-z,w=Math.Exp(-(dx*dx+dz*dz)/(sigma*sigma));su+=w*ctrl[i+2];sv+=w*ctrl[i+3];sw+=w;}
   if(sw<1)sw=1;return new[]{p[0]+su/sw,p[1]+sv/sw};
  }
  // pairs {x,z,u,v}; extra = more {x,z,u,v} (spawns). Keeps pairs within 4% of the affine prediction.
  public static double[] Controls(double[] t,List<double[]> pairs){
   var c=new List<double>();foreach(var p in pairs){var a=Affine(t,p[0],p[1]);double du=p[2]-a[0],dv=p[3]-a[1];if(Math.Sqrt(du*du+dv*dv)>0.04)continue;c.Add(p[0]);c.Add(p[1]);c.Add(du);c.Add(dv);}return c.ToArray();
  }
  // Spawn markers (u,v) paired with the nearest tarkov.dev spawn (x,z) once both are in image space; 2.5% at most.
  public static List<double[]> PairNearest(double[] t,List<double[]> marks,List<double[]> game){
   var r=new List<double[]>();foreach(var m in marks){double best=0.025;double[] hit=null;foreach(var gp in game){var a=Affine(t,gp[0],gp[1]);double d=Math.Sqrt((a[0]-m[0])*(a[0]-m[0])+(a[1]-m[1])*(a[1]-m[1]));if(d<best){best=d;hit=gp;}}
    if(hit!=null)r.Add(new[]{hit[0],hit[1],m[0],m[1]});}return r;
  }
  // Leave-one-out check on the exit pairs: mean error of affine alone vs affine + local correction.
  public static double[] LeaveOneOut(double[] t,List<double[]> exits,List<double[]> spawns,double sigma){
   double ea=0,ec=0;int n=0;
   for(int i=0;i<exits.Count;i++){var rest=new List<double[]>(spawns);for(int j=0;j<exits.Count;j++)if(j!=i)rest.Add(exits[j]);var ctrl=Controls(t,rest);var p=exits[i];
    var a=Affine(t,p[0],p[1]);var c=Correct(t,ctrl,sigma,p[0],p[1]);if(Math.Sqrt((a[0]-p[2])*(a[0]-p[2])+(a[1]-p[3])*(a[1]-p[3]))>0.04)continue;
    ea+=Math.Sqrt((a[0]-p[2])*(a[0]-p[2])+(a[1]-p[3])*(a[1]-p[3]));ec+=Math.Sqrt((c[0]-p[2])*(c[0]-p[2])+(c[1]-p[3])*(c[1]-p[3]));n++;}
   return n==0?new[]{0.0,0.0,0}:new[]{ea/n,ec/n,n};
  }
  static readonly Dictionary<string,double[]> ctrlCache=new Dictionary<string,double[]>();static readonly Dictionary<string,double> sigmaCache=new Dictionary<string,double>();
  // Game x,z -> normalised image position for a map, with the shipped local correction (null without a transform).
  // maps/fit.json (source/fit-maps.mjs: refitted from hundreds of containers, doors and guns paired with tarkov.dev) wins
  // over markers.json's exit-only transform for the maps it lists.
  static readonly Dictionary<string,IDictionary> fitFiles=new Dictionary<string,IDictionary>();
  static IDictionary Fit(string id,string shippedPath){IDictionary all;if(!fitFiles.TryGetValue(shippedPath,out all)){all=null;try{var f=System.IO.Path.Combine(System.IO.Path.GetDirectoryName(shippedPath),"fit.json");if(System.IO.File.Exists(f))all=(IDictionary)new JavaScriptSerializer{MaxJsonLength=int.MaxValue}.DeserializeObject(System.IO.File.ReadAllText(f));}catch{}fitFiles[shippedPath]=all;}
   return all!=null&&all.Contains(id)?all[id] as IDictionary:null;}
  static double[] Nums(object o){var l=new List<double>();if(o is IEnumerable)foreach(var v in (IEnumerable)o)l.Add(D(v));return l.ToArray();}
  public static double[] ToImage(string id,string shippedPath,double x,double z){
   var fit=Fit(id,shippedPath);if(fit!=null){var ft=Nums(fit["t"]);if(ft.Length>=6)return Correct(ft,fit.Contains("ctrl")?Nums(fit["ctrl"]):null,fit.Contains("sigma")?D(fit["sigma"]):0,x,z);}
   var t=Transform(id,shippedPath);if(t==null)return null;double[] ctrl;double sigma;
   if(!ctrlCache.TryGetValue(id+"|"+shippedPath,out ctrl)){var m=MapOf(shippedPath,id);var l=new List<double>();sigma=0;
    if(m!=null&&m.Contains("ctrl"))foreach(var o in (IEnumerable)m["ctrl"])l.Add(D(o));if(m!=null&&m.Contains("sigma"))sigma=D(m["sigma"]);ctrl=l.ToArray();ctrlCache[id+"|"+shippedPath]=ctrl;sigmaCache[id+"|"+shippedPath]=sigma;}
   return Correct(t,ctrl,sigmaCache[id+"|"+shippedPath],x,z);
  }
  // Maps drawn floor by floor side by side (The Lab, Factory, Interchange's mall): maps/levels.json next to markers.json gives one transform per floor,
  // picked by the game height y; other maps (or no y) use the single transform above.
  static readonly Dictionary<string,IDictionary> levelFiles=new Dictionary<string,IDictionary>();
  public static double[] ToImage(string id,string shippedPath,double x,double y,double z){
   IDictionary all;if(!levelFiles.TryGetValue(shippedPath,out all)){all=null;try{var f=System.IO.Path.Combine(System.IO.Path.GetDirectoryName(shippedPath),"levels.json");if(System.IO.File.Exists(f))all=(IDictionary)new JavaScriptSerializer().DeserializeObject(System.IO.File.ReadAllText(f));}catch{}levelFiles[shippedPath]=all;}
   var floors=all!=null&&all.Contains(id)?all[id] as IEnumerable:null;
   if(floors!=null&&!double.IsNaN(y))foreach(IDictionary fl in floors){if(fl.Contains("below")&&y>=D(fl["below"]))continue;
    // "area" [x0,z0,x1,z1]: only inside it (Interchange mall floors); outside, the map's own transform
    if(fl.Contains("area")){var ar=Nums(fl["area"]);if(ar.Length>=4&&(x<ar[0]||x>ar[2]||z<ar[1]||z>ar[3]))continue;}var t=new List<double>();foreach(var o in (IEnumerable)fl["t"])t.Add(D(o));if(t.Count>=6)return Affine(t.ToArray(),x,z);}
   return ToImage(id,shippedPath,x,z);
  }
  public static double[] Transform(string id,string shippedPath){var fit=Fit(id,shippedPath);if(fit!=null){var ft=Nums(fit["t"]);if(ft.Length>=6)return ft;}var m=MapOf(shippedPath,id);if(m==null||!m.Contains("transform"))return null;var t=new List<double>();foreach(var o in (IEnumerable)m["transform"])t.Add(D(o));return t.Count>=6?t.ToArray():null;}
 }
 // ---- Active quests from the game's own logs, objective places from tarkov.dev ----
 // push-notifications logs carry trader messages: type 10 = quest started, 11 = failed, 12 = completed, with the
 // quest id in templateId ("<id> description"). The session's application log says which mode (PvE / PvP) it was.
 // Only log files are read. Objective progress is not logged, so a quest's places show until the quest is done.
 // The price worker consumes completed-quest/reset snapshots without scanning logs on an inspect click.
 // All its history reads share one background gate because Quests' log index is mutable.
 public sealed class QuestHistory {
  sealed class State {public HashSet<string> Completed;public long ResetAt;}
  static readonly object gate=new object();readonly string logs,mode;
  System.Threading.Tasks.Task<State> job;DateTime next=DateTime.MinValue;
  public bool Ready{get;private set;}
  public HashSet<string> Completed{get;private set;}
  public long ResetAt{get;private set;}
  public QuestHistory(string logs,string mode){this.logs=logs;this.mode=mode;}
  public void RefreshIfStale(){
   if(job!=null||DateTime.UtcNow<next)return;
   job=System.Threading.Tasks.Task.Run(()=>{var thread=System.Threading.Thread.CurrentThread;var priority=thread.Priority;
    try{thread.Priority=System.Threading.ThreadPriority.BelowNormal;lock(gate)return new State{Completed=Quests.Completed(logs,mode),ResetAt=Quests.ResetAt(logs,mode)};}
    finally{thread.Priority=priority;}});
  }
  // Called only by the worker UI thread. Keep the previous good snapshot if a refresh fails.
  public bool Poll(){
   if(job==null||!job.IsCompleted)return false;var done=job;job=null;next=DateTime.UtcNow.AddSeconds(30);
   if(done.IsFaulted){done.Exception.Handle(e=>true);return false;}var state=done.Result;
   Completed=state.Completed;ResetAt=state.ResetAt;Ready=true;return true;
  }
 }
 public static class Quests {
  static readonly Regex eventRx=new Regex("\"type\": (1[012]),\\s*\"dt\": (\\d+),\\s*\"text\": \"[^\"]*\",\\s*\"templateId\": \"([0-9a-f]{24}) ");
  // Each "Session mode" line with its local time: a session can switch PvE <-> PvP without restarting the game, so a
  // quest event belongs to the mode in force when it happened, not to the folder's last mode.
  static readonly Regex modeRx=new Regex(@"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\.\d+\|[^\r\n]*Session mode: (\w+)",RegexOptions.Multiline);
  class Folder{public long Size;public string Mode="";public List<long> SwitchAt=new List<long>();public List<string> SwitchMode=new List<string>();public List<long[]> Events=new List<long[]>();public List<string> Ids=new List<string>();public long BackendRead;public List<long> Prestige=new List<long>();}
  // Prestige raised: the backend log has the request ("client/prestige/obtain"); it resets quests in that mode.
  static readonly Regex prestigeRx=new Regex(@"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\.\d+\|[^|\r\n]*\|[^|\r\n]*\|backend\|<--- Response HTTPS[^\r\n]*/client/prestige/obtain",RegexOptions.Multiline);
  // Backend logs are large: only the part added since the last look is read.
  static void ReadBackend(string dir,Folder f){
   foreach(var file in System.IO.Directory.GetFiles(dir)){if(!System.IO.Path.GetFileName(file).Contains("backend"))continue;
    using(var st=new System.IO.FileStream(file,System.IO.FileMode.Open,System.IO.FileAccess.Read,System.IO.FileShare.ReadWrite|System.IO.FileShare.Delete)){
     if(st.Length<f.BackendRead)f.BackendRead=0;if(st.Length==f.BackendRead)continue;st.Seek(f.BackendRead,System.IO.SeekOrigin.Begin);
     string text;using(var r=new System.IO.StreamReader(st))text=r.ReadToEnd();
     // Keep a partial last line for next time.
     int end=text.LastIndexOf('\n');if(end<0)continue;f.BackendRead+=System.Text.Encoding.UTF8.GetByteCount(text.Substring(0,end+1));
     foreach(Match m in prestigeRx.Matches(text.Substring(0,end+1))){DateTime local;if(DateTime.TryParse(m.Groups[1].Value,System.Globalization.CultureInfo.InvariantCulture,System.Globalization.DateTimeStyles.AssumeLocal,out local))f.Prestige.Add((long)(local.ToUniversalTime()-new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc)).TotalSeconds);}}}
  }
  static readonly Dictionary<string,Folder> folders=new Dictionary<string,Folder>();
  static long Unix(string local){DateTime t;return DateTime.TryParse(local,System.Globalization.CultureInfo.InvariantCulture,System.Globalization.DateTimeStyles.AssumeLocal,out t)?(long)(t.ToUniversalTime()-new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc)).TotalSeconds:0;}
  // Mode at unix time t in this folder: the last switch at or before it (the first one for anything earlier).
  static string ModeAt(Folder f,long t){if(f.SwitchAt.Count==0)return f.Mode;string m=f.SwitchMode[0];for(int i=0;i<f.SwitchAt.Count;i++){if(f.SwitchAt[i]<=t+5)m=f.SwitchMode[i];else break;}return m;}
  static string ModeKey(string m){return m=="Pve"?"pve":m=="PvpSeason"?"pvp-season":m=="Regular"?"regular":"";}
  static long SizeOf(string dir){long n=0;foreach(var f in System.IO.Directory.GetFiles(dir)){var name=System.IO.Path.GetFileName(f);if(name.Contains("push-notifications")||name.Contains("application"))n+=new System.IO.FileInfo(f).Length;}return n;}
  static string ReadShared(string f){using(var s=new System.IO.FileStream(f,System.IO.FileMode.Open,System.IO.FileAccess.Read,System.IO.FileShare.ReadWrite|System.IO.FileShare.Delete))using(var r=new System.IO.StreamReader(s))return r.ReadToEnd();}
  // Rescans only log folders that grew since last time (the live session), the rest stay cached.
  // What was read from each log folder is kept on disk (CacheFile), so only new or growing folders are read again:
  // the first full read of all backend logs takes seconds, later starts take milliseconds.
  public static string CacheFile="";static bool cacheLoaded;
  static void LoadIndex(){
   cacheLoaded=true;if(CacheFile.Length==0||!System.IO.File.Exists(CacheFile))return;
   try{var root=(IDictionary)js.DeserializeObject(System.IO.File.ReadAllText(CacheFile));foreach(DictionaryEntry e in root){var d=(IDictionary)e.Value;if(!d.Contains("sw"))continue; // saved before per-event modes: read that folder again
     var f=new Folder{Size=Convert.ToInt64(d["size"]),Mode=(string)d["mode"],BackendRead=Convert.ToInt64(d["br"])};foreach(var o in (IEnumerable)d["sw"]){var a=(object[])o;f.SwitchAt.Add(Convert.ToInt64(a[0]));f.SwitchMode.Add((string)a[1]);}
     foreach(var o in (IEnumerable)d["ev"]){var a=(object[])o;f.Events.Add(new[]{Convert.ToInt64(a[0]),Convert.ToInt64(a[1])});}foreach(var o in (IEnumerable)d["ids"])f.Ids.Add((string)o);foreach(var o in (IEnumerable)d["pr"])f.Prestige.Add(Convert.ToInt64(o));
     folders[(string)e.Key]=f;}}catch{folders.Clear();}
  }
  static void SaveIndex(){
   if(CacheFile.Length==0)return;var root=new Dictionary<string,object>();
   foreach(var kv in folders){var f=kv.Value;var ev=new List<object>();foreach(var e in f.Events)ev.Add(new object[]{e[0],e[1]});var sw=new List<object>();for(int i=0;i<f.SwitchAt.Count;i++)sw.Add(new object[]{f.SwitchAt[i],f.SwitchMode[i]});root[kv.Key]=new Dictionary<string,object>{{"sw",sw},{"size",f.Size},{"mode",f.Mode},{"br",f.BackendRead},{"ev",ev},{"ids",f.Ids},{"pr",f.Prestige}};}
   try{var tmp=CacheFile+"."+System.Diagnostics.Process.GetCurrentProcess().Id+".tmp";System.IO.File.WriteAllText(tmp,js.Serialize(root));if(System.IO.File.Exists(CacheFile))System.IO.File.Replace(tmp,CacheFile,null);else System.IO.File.Move(tmp,CacheFile);}catch{}
  }
  static DateTime nextScan=DateTime.MinValue;static string scannedLogs="";
  static void Scan(string logs){
   if(string.IsNullOrEmpty(logs)||!System.IO.Directory.Exists(logs))return;if(!cacheLoaded)LoadIndex();
   if(logs==scannedLogs&&DateTime.UtcNow<nextScan)return;scannedLogs=logs;nextScan=DateTime.UtcNow.AddSeconds(10);bool changed=false;
   var dirs=System.IO.Directory.GetDirectories(logs);var made=new long[dirs.Length];for(int k=0;k<dirs.Length;k++)made[k]=System.IO.Directory.GetCreationTimeUtc(dirs[k]).Ticks;Array.Sort(made,dirs);
   for(int i=0;i<dirs.Length;i++){var dir=dirs[i];Folder f;if(i<dirs.Length-3&&folders.ContainsKey(dir))continue;
    Folder old;folders.TryGetValue(dir,out old);if(old!=null){long before=old.BackendRead;try{ReadBackend(dir,old);}catch{}if(old.BackendRead!=before)changed=true;}
    long size;try{size=SizeOf(dir);}catch{continue;}if(old!=null&&old.Size==size)continue;
    f=new Folder{Size=size};if(old!=null){f.BackendRead=old.BackendRead;f.Prestige=old.Prestige;}else{try{ReadBackend(dir,f);}catch{}}
    try{foreach(var file in System.IO.Directory.GetFiles(dir)){var name=System.IO.Path.GetFileName(file);
     if(name.Contains("application")){foreach(Match m in modeRx.Matches(ReadShared(file))){var k=ModeKey(m.Groups[2].Value);if(k.Length==0)continue;f.SwitchAt.Add(Unix(m.Groups[1].Value));f.SwitchMode.Add(k);f.Mode=k;}}
     else if(name.Contains("push-notifications")){foreach(Match m in eventRx.Matches(ReadShared(file))){f.Events.Add(new[]{long.Parse(m.Groups[2].Value),long.Parse(m.Groups[1].Value)});f.Ids.Add(m.Groups[3].Value);}}}}catch{continue;}
    folders[dir]=f;changed=true;}
   // The live mode: the newest folder by creation time (folder names do not sort: hours are not zero-padded).
   for(int i=dirs.Length-1;i>=0;i--){Folder lf;if(folders.TryGetValue(dirs[i],out lf)&&lf.Mode.Length>0){liveMode=lf.Mode;break;}}
   if(changed)SaveIndex();
  }
  // Mode of the newest session (the one being played), e.g. "pve".
  static string liveMode="";
  public static string CurrentMode(string logs){Scan(logs);return liveMode;}
  // Quests whose last event in this mode is "completed" (the price overlay leaves them out).
  public static HashSet<string> Completed(string logs,string mode){return WithLast(logs,mode,12);}
  // Quests whose last event in this mode is "started".
  public static HashSet<string> Active(string logs,string mode){return WithLast(logs,mode,10);}
  static HashSet<string> WithLast(string logs,string mode,long type){
   Scan(logs);var all=new List<KeyValuePair<long[],string>>();
   foreach(var f in folders.Values)for(int i=0;i<f.Events.Count;i++)if(ModeAt(f,f.Events[i][0])==mode)all.Add(new KeyValuePair<long[],string>(f.Events[i],f.Ids[i]));
   all.Sort((a,b)=>a.Key[0].CompareTo(b.Key[0]));
   // A wipe resets quests without any log line. It shows as already completed quests started again:
   // three or more such restarts within three days mark a reset. Only events after the latest reset count.
   long cut=Cut(all,mode);
   var last=new Dictionary<string,long>();foreach(var e in all)if(e.Key[0]>=cut)last[e.Value]=e.Key[1];
   var found=new HashSet<string>();foreach(var kv in last)if(kv.Value==type)found.Add(kv.Key);return found;
  }
  static long Cut(List<KeyValuePair<long[],string>> all,string mode){
   long cut=long.MinValue;const long days3=3*86400;
   // Exact: the latest prestige raised in this mode. Fallback (season wipes leave no such line): the restarts below.
   foreach(var f in folders.Values)foreach(var t in f.Prestige)if(ModeAt(f,t)==mode)cut=Math.Max(cut,t-60);
   // Restarts count only for quests completed since the latest prestige: picking up again, after a prestige, quests
   // finished before it is normal and must not read as one more wipe (it hid every PvE quest taken before it).
   var completed=new HashSet<string>();var restarts=new List<long>();
   foreach(var e in all){if(e.Key[0]<cut)continue;if(e.Key[1]==12)completed.Add(e.Value);else if(e.Key[1]==10&&completed.Contains(e.Value))restarts.Add(e.Key[0]);}
   for(int i=0;i<restarts.Count;i++){if(i>0&&restarts[i]-restarts[i-1]<=days3)continue;int n=0;for(int j=i;j<restarts.Count&&restarts[j]-restarts[i]<=days3;j++)n++;if(n>=3)cut=Math.Max(cut,restarts[i]-3600);}
   return cut;
  }
  // Latest prestige / wipe in this mode (unix seconds, a little before it), 0 when none: the hideout starts over then too.
  public static long ResetAt(string logs,string mode){
   Scan(logs);var all=new List<KeyValuePair<long[],string>>();
   foreach(var f in folders.Values)for(int i=0;i<f.Events.Count;i++)if(ModeAt(f,f.Events[i][0])==mode)all.Add(new KeyValuePair<long[],string>(f.Events[i],f.Ids[i]));
   all.Sort((a,b)=>a.Key[0].CompareTo(b.Key[0]));long cut=Cut(all,mode);return cut==long.MinValue?0:cut;
  }
  // tarkov.dev tasks and names for the UI locale, cached per mode and refreshed daily in the background.
  static readonly JavaScriptSerializer js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};
  public static string LastError="";
  static readonly HashSet<string> refreshing=new HashSet<string>();
  public static string CachePath(string store,string mode){return System.IO.Path.Combine(store,"quest-places-"+mode+(I18n.Language=="ko"?"":"."+I18n.ApiLocale)+".json");}
  public static void RefreshIfStale(string store,string mode,TimeSpan maxAge){
   if(mode.Length==0)return;var path=CachePath(store,mode);
   try{if(System.IO.File.Exists(path)&&DateTime.UtcNow-System.IO.File.GetLastWriteTimeUtc(path)<maxAge){var have=FandomMarkers.Parsed(path);if(have!=null&&have.Contains("keyDetails")&&Convert.ToInt32(have["keyDetails"])>=13)return;}}catch{}
   lock(refreshing){if(!refreshing.Add(mode))return;}
   var t=new System.Threading.Thread(()=>{try{var json=Build(mode);var tmp=path+".tmp";System.IO.File.WriteAllText(tmp,json);if(System.IO.File.Exists(path))System.IO.File.Replace(tmp,path,null);else System.IO.File.Move(tmp,path);LastError="";}catch(Exception e){LastError=e.Message;}finally{lock(refreshing)refreshing.Remove(mode);}});
   t.IsBackground=true;t.Priority=System.Threading.ThreadPriority.BelowNormal;t.Start();
  }
  static IEnumerable Seq(object o){var d=o as IDictionary;return d!=null?(IEnumerable)d.Values:(o as IEnumerable)??new object[0];}
  // tarkov.dev's text when it is in the UI language, else the game's own (SPT locale), else English.
  static string KeyDescription(IDictionary local,IDictionary english,string key,IDictionary game=null){var field=key+" Description";var text=local!=null&&local.Contains(field)?Convert.ToString(local[field]).Trim():"";
   if(text.Length>0&&Native(text))return text;var g=game!=null&&game.Contains(field)?Convert.ToString(game[field]).Trim():"";if(g.Length>0&&Native(g))return g;
   return text.Length>0?text:english!=null&&english.Contains(field)?Convert.ToString(english[field]).Trim():"";}
  // Text in the UI language's own script (for languages that have one; Latin-script languages always pass).
  static bool Native(string t){if(string.IsNullOrEmpty(t))return false;string script=I18n.Language=="ko"?"[\uac00-\ud7a3]":I18n.Language=="ja"?"[\u3040-\u30ff\u4e00-\u9fff]":I18n.Language.StartsWith("zh")?"[\u4e00-\u9fff]":I18n.Language=="ru"?"[\u0400-\u04ff]":null;
   return script==null||System.Text.RegularExpressions.Regex.IsMatch(t,script);}
  // Bodyguards that come with a boss: each escort kind has its own count distribution (Reshala: 2 25%, 3 25%, 4 50%);
  // added up into one distribution of the total, as [[count, chance], ...] by count.
  static List<object> Guards(IDictionary boss){var total=new Dictionary<int,double>{{0,1.0}};
   foreach(IDictionary e in Seq(boss.Contains("escorts")?boss["escorts"]:null)){var next=new Dictionary<int,double>();
    foreach(IDictionary a in Seq(e["amount"])){int n=Convert.ToInt32(a["count"]);double c=Convert.ToDouble(a["chance"]);foreach(var t in total){double v;next.TryGetValue(t.Key+n,out v);next[t.Key+n]=v+t.Value*c;}}
    if(next.Count>0)total=next;}
   var keys=new List<int>(total.Keys);keys.Sort();var list=new List<object>();foreach(var k in keys)if(total[k]>0.001)list.Add(new object[]{k,Math.Round(total[k],3)});return list;}
  // The game's own locale names (SPT's files): kr, jp, ch (both Chinese), ge, po (Portuguese), tu ...
  static string GameLocale(string ui){switch(ui){case "ko":return "kr";case "ja":return "jp";case "zh-CN":case "zh-TW":return "ch";case "de":return "ge";case "pt-BR":return "po";case "tr":return "tu";case "ru":case "es":case "fr":case "it":case "pl":return ui;default:return "en";}}
  static string Brief(IDictionary locale,string id){if(locale==null||!locale.Contains(id+" description"))return "";var t=Convert.ToString(locale[id+" description"]);
   // The game's text opens with the quest's name in brackets ("[Chumming]"), already the card's title.
   t=System.Text.RegularExpressions.Regex.Replace(t,"<[^>]+>","").Replace("\r","").Trim();t=System.Text.RegularExpressions.Regex.Replace(t,"^\\[[^\\]\\n]*\\]\\s*","");
   // Some quests are still English in a language's file: for a language with its own script, text without it is skipped
   // (the card then shows the wiki's English briefing, labelled as English).
   string script=I18n.Language=="ko"?"[\\uac00-\\ud7a3]":I18n.Language=="ja"?"[\\u3040-\\u30ff\\u4e00-\\u9fff]":I18n.Language.StartsWith("zh")?"[\\u4e00-\\u9fff]":I18n.Language=="ru"?"[\\u0400-\\u04ff]":null;
   if(script!=null&&!System.Text.RegularExpressions.Regex.IsMatch(t,script))return "";
   return t.Length>4000?t.Substring(0,4000).TrimEnd()+"…":t;}
  static bool KeyOptional(IDictionary needs,string quest,string key){var q=needs!=null&&needs.Contains(quest)?needs[quest] as IDictionary:null;return q!=null&&q.Contains(key)&&Convert.ToString(q[key])=="optional";}
  static string QuestName(IDictionary ko,IDictionary game,IDictionary t,bool sameQuest){var name=Tr(ko,t["name"]);if(Native(name)||!sameQuest||game==null)return name;var k=Convert.ToString(t["id"])+" name";var g=game.Contains(k)?Convert.ToString(game[k]).Trim():"";return g.Length>0&&Native(g)?g:name;}
  static string Tr(IDictionary ko,object key){var k=key as string;return k!=null&&ko!=null&&ko.Contains(k)?Convert.ToString(ko[k]):(k??"");}
  // Compact file: quest id -> {name, places:[{map (our id), text, x, z, outline:[x,z,...]}]}.
  static string Build(string mode){
   var b="https://json.tarkov.dev/"+mode+"/";var tasks=(IDictionary)((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"tasks")))["data"];
   var ko=((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"tasks_"+I18n.ApiLocale)))["data"] as IDictionary;
   var maps=new Dictionary<string,string>();var bosses=new Dictionary<string,object>();var guards=new Dictionary<string,object>();var locks=new Dictionary<string,object>();
   foreach(IDictionary m in Seq(((IDictionary)((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"maps")))["data"])["maps"])){
    var id=Convert.ToString(m["nameId"]).ToLowerInvariant();if(id=="sandbox_high")id="sandbox";if(id=="factory4_night")id="factory4_day";maps[Convert.ToString(m["id"])]=id;
    // Boss spawn chance per map (the highest entry of each boss; cultists = their priest), as tarkov.dev reads the game.
    // The Goons (Knight with Big Pipe and Birdeye) are three bosses, not a boss with guards: no guard count for them.
    var chance=bosses.ContainsKey(id)?(Dictionary<string,object>)bosses[id]:new Dictionary<string,object>();
    var escort=guards.ContainsKey(id)?(Dictionary<string,object>)guards[id]:new Dictionary<string,object>();
    foreach(IDictionary bs in Seq(m.Contains("bosses")?m["bosses"]:null)){var mob=Convert.ToString(bs["mob"]);if(!mob.StartsWith("boss")&&mob!="sectantPriest")continue;
     double c=Convert.ToDouble(bs["spawnChance"]);if(!chance.ContainsKey(mob)||Convert.ToDouble(chance[mob])<c){chance[mob]=c;if(mob!="bossKnight")escort[mob]=Guards(bs);}}
    bosses[id]=chance;guards[id]=escort;
    var list=locks.ContainsKey(id)?(List<object>)locks[id]:new List<object>();
    foreach(IDictionary l in Seq(m.Contains("locks")?m["locks"]:null)){var p=l["position"] as IDictionary;if(p==null||l["key"]==null)continue;
     list.Add(new Dictionary<string,object>{{"key",Convert.ToString(l["key"])},{"type",Convert.ToString(l["lockType"])},{"x",Convert.ToDouble(p["x"])},{"y",p.Contains("y")?Convert.ToDouble(p["y"]):double.NaN},{"z",Convert.ToDouble(p["z"])}});}
    locks[id]=list;}
   // The trader's briefing (the quest's own description) in the UI language: the game's text as kept in the SPT
   // project's locale files (github.com/sp-tarkov/server, about 3 MB per language, read once a day with this cache).
   // About 85% of current quests have it; the others keep the wiki's English briefing.
   // Those files are from an older game version and some quest and objective ids were reused for new quests since
   // (98 of 500), so an entry is used only when its English (SPT en.json) matches tarkov.dev's English.
   const string spt="https://raw.githubusercontent.com/sp-tarkov/server/master/project/assets/database/locales/global/";
   IDictionary briefs=null,sptEn=null,tasksEn=null;try{briefs=js.DeserializeObject(FandomMarkers.Get(spt+GameLocale(I18n.Language)+".json")) as IDictionary;sptEn=GameLocale(I18n.Language)=="en"?briefs:js.DeserializeObject(FandomMarkers.Get(spt+"en.json")) as IDictionary;
    tasksEn=I18n.ApiLocale=="en"?ko:((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"tasks_en")))["data"] as IDictionary;}catch{briefs=null;}
   Func<string,string,bool> same=(key,english)=>{if(sptEn==null||!sptEn.Contains(key)||string.IsNullOrEmpty(english))return false;Func<string,string> n=x=>System.Text.RegularExpressions.Regex.Replace(x.Trim().ToLowerInvariant()," - part 1$","");return n(Convert.ToString(sptEn[key]))==n(english);};
   Func<IDictionary,bool> sameQuest=t=>same(Convert.ToString(t["id"])+" name",Tr(tasksEn,t["name"]));
   // Objectives tarkov.dev has not translated (new or changed ones), translated once for BlueAsh and kept on its server
   // (relay/public/quest-guide/objectives/<lang>.json, made with source/guide-translate/objectives.mjs).
   IDictionary objectives=null;if(I18n.Language!="en"&&QuestWiki.Relay.Length>0)try{objectives=js.DeserializeObject(FandomMarkers.Get(QuestWiki.Relay+"/quest-guide/objectives/"+Uri.EscapeDataString(I18n.Language)+".json")) as IDictionary;}catch{}
   var keyMaps=new Dictionary<string,HashSet<string>>();foreach(var kv in locks)foreach(IDictionary l in (List<object>)kv.Value){var k=Convert.ToString(l["key"]);HashSet<string> set;if(!keyMaps.TryGetValue(k,out set))keyMaps[k]=set=new HashSet<string>();set.Add(kv.Key);}
   IDictionary keyNeeds=null;if(QuestWiki.Relay.Length>0)try{keyNeeds=js.DeserializeObject(FandomMarkers.Get(QuestWiki.Relay+"/quest-guide/keys.json")) as IDictionary;}catch{}
   Func<IDictionary,string> objective=o=>{var t=Tr(ko,o["description"]);if(Native(t))return t;var id=Convert.ToString(o["id"]);
    if(objectives!=null&&objectives.Contains(id))return Convert.ToString(objectives[id]);if(briefs!=null&&briefs.Contains(id)&&Native(Convert.ToString(briefs[id]))&&same(id,Tr(tasksEn,o["description"])))return Convert.ToString(briefs[id]);return t;};
   // Keys and keycards that open a lock somewhere: name in the UI language, keycard or not (English name says "keycard"),
   // and how many locks per map. Map names for the key list come from the same translation file.
   var keyInfo=new Dictionary<string,object>();var mapNames=new Dictionary<string,object>();IDictionary itemNames=null;
   try{var loc=((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"maps_"+I18n.ApiLocale)))["data"] as IDictionary;
    foreach(IDictionary m in Seq(((IDictionary)((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"maps")))["data"])["maps"])){var nid=Convert.ToString(m["nameId"]).ToLowerInvariant();if(nid=="factory4_night")continue;
     var ours=maps.ContainsKey(Convert.ToString(m["id"]))?maps[Convert.ToString(m["id"])]:"";if(ours.Length>0&&!mapNames.ContainsKey(ours))mapNames[ours]=Tr(loc,Convert.ToString(m["id"])+" Name");}
    var names=((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"items_"+I18n.ApiLocale)))["data"] as IDictionary;itemNames=names;
    var english=I18n.ApiLocale=="en"?names:((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"items_en")))["data"] as IDictionary;
    foreach(var kv in locks){foreach(IDictionary l in (List<object>)kv.Value){var key=Convert.ToString(l["key"]);Dictionary<string,object> info;
     if(keyInfo.ContainsKey(key))info=(Dictionary<string,object>)keyInfo[key];else{var en=Tr(english,key+" Name");info=new Dictionary<string,object>{{"name",Tr(names,key+" Name")},{"description",KeyDescription(names,english,key,briefs)},{"card",en.IndexOf("keycard",StringComparison.OrdinalIgnoreCase)>=0},{"wiki",en.Length>0?"https://escapefromtarkov.fandom.com/wiki/"+en.Replace(' ','_'):""},{"maps",new Dictionary<string,object>()}};keyInfo[key]=info;}
     var per=(Dictionary<string,object>)info["maps"];per[kv.Key]=(per.ContainsKey(kv.Key)?Convert.ToInt32(per[kv.Key]):0)+1;}}}catch{}
   var outQuests=new Dictionary<string,object>();
   foreach(IDictionary t in Seq(tasks["tasks"])){var places=new List<object>();
    foreach(IDictionary o in Seq(t.Contains("objectives")?t["objectives"]:null)){var text=objective(o);
     var spots=new List<IDictionary>();foreach(IDictionary z in Seq(o.Contains("zones")?o["zones"]:null))spots.Add(z);
     if(o.Contains("possibleLocations"))foreach(IDictionary pl in Seq(o["possibleLocations"])){var mapId=Convert.ToString(pl["map"]);foreach(IDictionary pos in Seq(pl["positions"]))spots.Add(new Dictionary<string,object>{{"map",mapId},{"position",pos}});}
     foreach(var z in spots){string map;if(!z.Contains("map")||!maps.TryGetValue(Convert.ToString(z["map"]),out map))continue;var p=z["position"] as IDictionary;if(p==null)continue;
      var outline=new List<double>();foreach(IDictionary q in Seq(z.Contains("outline")?z["outline"]:null)){outline.Add(Convert.ToDouble(q["x"]));outline.Add(Convert.ToDouble(q["z"]));}
      places.Add(new Dictionary<string,object>{{"map",map},{"text",text},{"x",Convert.ToDouble(p["x"])},{"y",p.Contains("y")?Convert.ToDouble(p["y"]):0.0},{"z",Convert.ToDouble(p["z"])},{"outline",outline}});}}
    var steps=new List<object>();foreach(IDictionary o in Seq(t.Contains("objectives")?t["objectives"]:null)){var text=objective(o);if(text.Length==0)continue;var on=new List<object>();foreach(var mp in Seq(o.Contains("maps")?o["maps"]:null)){string mid;if(maps.TryGetValue(Convert.ToString(mp),out mid))on.Add(mid);}
     steps.Add(new Dictionary<string,object>{{"text",text},{"optional",o.Contains("optional")&&Convert.ToBoolean(o["optional"])},{"maps",on}});}
    // Keys the quest needs (tarkov.dev neededKeys: per map, any one of the listed keys), names in the UI language;
    // "optional" when the wiki's guide does not say the key is required (BlueAsh's server: quest-guide/keys.json).
    // A key whose locks are all on other maps is a data slip (Vitamins listed a Shoreline key on Factory): left out.
    var needed=new List<object>();foreach(IDictionary nk in Seq(t.Contains("neededKeys")?t["neededKeys"]:null)){string nm;maps.TryGetValue(Convert.ToString(nk["map"]),out nm);var ids=new List<object>();
     foreach(var k in Seq(nk["keys"])){var kid=Convert.ToString(k);HashSet<string> on;if(nm!=null&&keyMaps.TryGetValue(kid,out on)&&!on.Contains(nm))continue;ids.Add(kid);}if(ids.Count==0)continue;
     needed.Add(new Dictionary<string,object>{{"map",nm??""},{"keys",ids.ConvertAll(k=>(object)new Dictionary<string,object>{{"id",k},{"name",itemNames!=null?Tr(itemNames,k+" Name"):Convert.ToString(k)},{"optional",KeyOptional(keyNeeds,Convert.ToString(t["id"]),Convert.ToString(k))}})}});}
    if(places.Count>0)outQuests[Convert.ToString(t["id"])]=new Dictionary<string,object>{{"name",QuestName(ko,briefs,t,sameQuest(t))},{"places",places},{"steps",steps},{"needed",needed},{"trader",Convert.ToString(t["trader"])},{"wiki",t.Contains("wikiLink")?Convert.ToString(t["wikiLink"]):""},{"brief",sameQuest(t)?Brief(briefs,Convert.ToString(t["id"])):""},{"level",t.Contains("minPlayerLevel")?Convert.ToInt32(t["minPlayerLevel"]):0}};}
   // Trader names in the UI language (for the quest card).
   var traders=new Dictionary<string,object>();try{var tn=((IDictionary)js.DeserializeObject(FandomMarkers.Get(b+"traders_"+I18n.ApiLocale)))["data"] as IDictionary;if(tn!=null)foreach(DictionaryEntry e in tn){var k=Convert.ToString(e.Key);if(k.EndsWith(" Nickname"))traders[k.Substring(0,k.Length-9)]=Convert.ToString(e.Value);}}catch{}
   return js.Serialize(new Dictionary<string,object>{{"keyDetails",13},{"mode",mode},{"quests",outQuests},{"bosses",bosses},{"guards",guards},{"traders",traders},{"locks",locks},{"keys",keyInfo},{"mapNames",mapNames}});
  }
  // Fandom boss, cultist and PMC-spawn pins in the UI language with this mode's spawn chance as the note; descriptions
  // translated line by line (FandomText). Exit names stay as they are.
  static readonly Dictionary<string,string> mobOf=new Dictionary<string,string>{{"Reshala","bossBully"},{"Killa","bossKilla"},{"Tagilla","bossTagilla"},{"Shadow of Tagilla","bossTagillaAgro"},
   {"Glukhar","bossGluhar"},{"Sanitar","bossSanitar"},{"Shturman","bossKojaniy"},{"Knight","bossKnight"},{"The Goons","bossKnight"},{"Goons","bossKnight"},{"Kaban","bossBoar"},
   {"Kollontay","bossKolontay"},{"Zryachiy","bossZryachiy"},{"Partisan","bossPartisan"},{"The Wedge","bossWedge"},{"Cultists","sectantPriest"}};
  static readonly Dictionary<string,string> nameOfMob=new Dictionary<string,string>{{"bossBully","Reshala"},{"bossKilla","Killa"},{"bossTagilla","Tagilla"},{"bossTagillaAgro","Shadow of Tagilla"},
   {"bossGluhar","Glukhar"},{"bossSanitar","Sanitar"},{"bossKojaniy","Shturman"},{"bossKnight","Goons"},{"bossBoar","Kaban"},{"bossKolontay","Kollontay"},{"bossZryachiy","Zryachiy"},
   {"bossPartisan","Partisan"},{"bossWedge","The Wedge"},{"sectantPriest","Cultists"}};
  static IDictionary ChanceOf(string id,string logs,string store){try{var mode=CurrentMode(logs);var root=mode.Length>0?FandomMarkers.Parsed(CachePath(store,mode)):null;var all=root!=null&&root.Contains("bosses")?root["bosses"] as IDictionary:null;return all!=null&&all.Contains(id)?all[id] as IDictionary:null;}catch{return null;}}
  static string ShortName(string english){var n=FandomText.Line(english);int cut=n.IndexOf(" (");return cut>0?n.Substring(0,cut):n;}
  // "이 맵 보스" and one line per boss, highest chance first, for every boss the game data lists on this map (also
  // those the Fandom map has no marker for); "" while the data has not been fetched.
  public static string BossSummary(string id,string logs,string store){return BossSummary(id,logs,store,null);}
  public static string BossSummary(string id,string logs,string store,List<Pin> pins){
   var chance=ChanceOf(id,logs,store);if(chance==null)return "";var rows=new List<KeyValuePair<string,double>>();
   if(pins!=null)foreach(var p in pins)if(p.Faded){bool seen=false;foreach(var e in rows)if(e.Key==p.Name)seen=true;if(!seen)rows.Add(new KeyValuePair<string,double>(p.Name,0));}
   foreach(DictionaryEntry e in chance){string en;if(!nameOfMob.TryGetValue(Convert.ToString(e.Key),out en))continue;rows.Add(new KeyValuePair<string,double>(ShortName(en),Convert.ToDouble(e.Value)));}
   if(rows.Count==0)return "";
   rows.Sort((a,b)=>b.Value.CompareTo(a.Value));var sb=new System.Text.StringBuilder(I18n.Text("이 맵 보스"));foreach(var r in rows)sb.Append("\n"+r.Key+"  "+Math.Round(r.Value*100)+"%");return sb.ToString();
  }
  // "호위 2~4명 (2명 25% · 3명 25% · 4명 50%)", "호위 2명", "호위 없음" from Guards' distribution.
  public static string GuardText(IEnumerable dist){var rows=new List<KeyValuePair<int,double>>();if(dist!=null)foreach(var o in dist){var a=o as IList;if(a!=null&&a.Count>=2)rows.Add(new KeyValuePair<int,double>(Convert.ToInt32(a[0]),Convert.ToDouble(a[1])));}
   if(rows.Count==0||(rows.Count==1&&rows[0].Key==0))return I18n.Text("호위 없음");if(rows.Count==1)return I18n.Format("호위 {0}명",rows[0].Key);
   var parts=new List<string>();foreach(var r in rows)parts.Add(I18n.Format("{0}명 {1}%",r.Key,Math.Round(r.Value*100)));
   return I18n.Format("호위 {0}~{1}명",rows[0].Key,rows[rows.Count-1].Key)+" ("+string.Join(" · ",parts)+")";}
  public static void Decorate(List<Pin> pins,string id,string logs,string store){
   IDictionary chance=null,guard=null;try{var mode=CurrentMode(logs);var root=mode.Length>0?FandomMarkers.Parsed(CachePath(store,mode)):null;var all=root!=null&&root.Contains("bosses")?root["bosses"] as IDictionary:null;chance=all!=null&&all.Contains(id)?all[id] as IDictionary:null;
    var g=root!=null&&root.Contains("guards")?root["guards"] as IDictionary:null;guard=g!=null&&g.Contains(id)?g[id] as IDictionary:null;}catch{}
   foreach(var p in pins){
    if(p.Kind=="boss"||p.Kind=="cultist"){var who=p.Kind=="cultist"?"Cultists":FandomText.BossOf(p.Name);string mob;string guards="";
     if(mobOf.TryGetValue(who,out mob)&&chance!=null&&chance.Contains(mob)){p.Note=Math.Round(Convert.ToDouble(chance[mob])*100)+"%";if(guard!=null&&guard.Contains(mob))guards=GuardText(guard[mob] as IEnumerable);}
     // The Fandom map still marks a boss the game data no longer lists here (e.g. Glukhar moved off Reserve by an event):
     // the spot stays, faded, at 0%.
     else if(mob!=null&&chance!=null){p.Note="0%";p.Faded=true;}
     // A random boss spot (Terminal): the bosses that can appear there and their chance.
     else if(who=="Random Boss Spawn"&&chance!=null){var parts=new List<string>();foreach(DictionaryEntry e in chance){string en;if(nameOfMob.TryGetValue(Convert.ToString(e.Key),out en))parts.Add(ShortName(en)+" "+Math.Round(Convert.ToDouble(e.Value)*100)+"%");}if(parts.Count>0)p.Condition=string.Join("\n",parts);} // replaces Fandom's plain name list
     p.Name=p.Kind=="cultist"?FandomText.Line("Cultists"):FandomText.Line(who=="Random Boss Spawn"?who:who);if(p.Condition.Trim()==who||FandomText.Line(p.Condition.Trim())==p.Name)p.Condition="";
     p.Guards=guards;}
    else if(p.Kind=="pmc")p.Name=FandomText.Line(p.Name);
    p.Condition=FandomText.Text(p.Condition);
    if(p.Note.Length>0)p.Condition=(p.Condition.Length>0?p.Condition+"\n":"")+I18n.Format("출현 확률 {0}",p.Note);
    if(p.Guards.Length>0)p.Condition+="\n"+p.Guards;}
  }
  // Keys the player inspected in the price window (my-keys.json: mode -> key id -> name): a pin on every lock they open.
  public static List<Pin> KeyPins(string id,string logs,string store,string shippedMarkers,string myKeys){
   var pins=new List<Pin>();try{var mode=CurrentMode(logs);if(mode.Length==0||!System.IO.File.Exists(myKeys))return pins;
    var mine=((IDictionary)js.DeserializeObject(System.IO.File.ReadAllText(myKeys)))[mode] as IDictionary;if(mine==null||mine.Count==0)return pins;
    var root=FandomMarkers.Parsed(CachePath(store,mode));var all=root!=null&&root.Contains("locks")?root["locks"] as IDictionary:null;var list=all!=null&&all.Contains(id)?all[id] as IEnumerable:null;if(list==null)return pins;
    foreach(IDictionary l in list){var key=Convert.ToString(l["key"]);if(!mine.Contains(key))continue;
     var img=FandomMarkers.ToImage(id,shippedMarkers,Convert.ToDouble(l["x"]),l.Contains("y")?Convert.ToDouble(l["y"]):double.NaN,Convert.ToDouble(l["z"]));float u=(float)img[0],v=(float)img[1];if(u<0||u>1||v<0||v>1)continue;
     var info=root.Contains("keys")&&((IDictionary)root["keys"]).Contains(key)?(IDictionary)((IDictionary)root["keys"])[key]:null;bool card=info!=null&&Convert.ToBoolean(info["card"]);
     pins.Add(new Pin{Kind=card?"keycard":"key",Ref=key,Name=info!=null?Convert.ToString(info["name"]):Convert.ToString(mine[key]),Condition=I18n.Text(Convert.ToString(l["type"])=="trunk"?"차량 트렁크":"문"),X=u,Y=v});}}catch{}
   return pins;
  }
  // Keys a quest needs for the card: one row per key (name, map name), and whether it is in my-keys.json for this mode.
  public static List<Dictionary<string,object>> NeededKeys(IDictionary quest,string logs,string store,string myKeys){
   var rows=new List<Dictionary<string,object>>();if(quest==null||!quest.Contains("needed"))return rows;var mode=CurrentMode(logs);var root=mode.Length>0?FandomMarkers.Parsed(CachePath(store,mode)):null;
   var mapNames=root!=null&&root.Contains("mapNames")?root["mapNames"] as IDictionary:null;IDictionary mine=null;
   try{if(System.IO.File.Exists(myKeys))mine=((IDictionary)js.DeserializeObject(System.IO.File.ReadAllText(myKeys)))[mode] as IDictionary;}catch{}
   foreach(IDictionary nk in (IEnumerable)quest["needed"]){var map=Convert.ToString(nk["map"]);var mapName=mapNames!=null&&mapNames.Contains(map)?Convert.ToString(mapNames[map]):"";
    foreach(IDictionary k in (IEnumerable)nk["keys"]){var id=Convert.ToString(k["id"]);rows.Add(new Dictionary<string,object>{{"id",id},{"name",Convert.ToString(k["name"])},{"map",mapName},{"mine",mine!=null&&mine.Contains(id)},{"optional",k.Contains("optional")&&Convert.ToBoolean(k["optional"])}});}}
   return rows;}
  // One quest for the card: name, trader (UI language), level, objectives (text, optional, maps), wiki link; null if unknown.
  public static IDictionary Quest(string logs,string store,string id,out string trader){
   trader="";var mode=CurrentMode(logs);var root=mode.Length>0?FandomMarkers.Parsed(CachePath(store,mode)):null;var data=root==null?null:root["quests"] as IDictionary;if(data==null||!data.Contains(id))return null;
   var q=(IDictionary)data[id];var tr=root.Contains("traders")?root["traders"] as IDictionary:null;var tid=q.Contains("trader")?Convert.ToString(q["trader"]):"";if(tr!=null&&tr.Contains(tid))trader=Convert.ToString(tr[tid]);return q;}
  // Pins for the active quests on this map: nearby objectives (within 2% of the image) share one pin.
  public static List<Pin> Pins(string id,string logs,string store,string shippedMarkers){
   var pins=new List<Pin>();var t=FandomMarkers.Transform(id,shippedMarkers);var mode=CurrentMode(logs);if(t==null||mode.Length==0)return pins;
   var root=FandomMarkers.Parsed(CachePath(store,mode));var data=root==null?null:root["quests"] as IDictionary;if(data==null)return pins;
   foreach(var q in Active(logs,mode)){if(!data.Contains(q))continue;var quest=(IDictionary)data[q];var name=Convert.ToString(quest["name"]);
    foreach(IDictionary p in (IEnumerable)quest["places"]){if(Convert.ToString(p["map"])!=id)continue;
     double x=Convert.ToDouble(p["x"]),z=Convert.ToDouble(p["z"]),y=p.Contains("y")?Convert.ToDouble(p["y"]):double.NaN;var img=FandomMarkers.ToImage(id,shippedMarkers,x,y,z);float u=(float)img[0],v=(float)img[1];if(u<0||u>1||v<0||v>1)continue;
     var line=name+" · "+Convert.ToString(p["text"]);Pin near=null;foreach(var e in pins)if(Math.Abs(e.X-u)<0.02f&&Math.Abs(e.Y-v)<0.02f){near=e;break;}
     if(near!=null){if(!near.Condition.Contains(line)){near.Condition+="\n"+line;near.Count++;}if((","+near.Ref+",").IndexOf(","+q+",",StringComparison.Ordinal)<0)near.Ref+=","+q;continue;}
     var pin=new Pin{Kind="quest",Name=name,Condition=line,X=u,Y=v,Count=1,Ref=q};var ol=(IEnumerable)p["outline"];var pts=new List<PointF>();var xs=new List<double>();foreach(var o in ol)xs.Add(Convert.ToDouble(o));
     for(int i=0;i+1<xs.Count;i+=2){var q2=FandomMarkers.ToImage(id,shippedMarkers,xs[i],y,xs[i+1]);pts.Add(new PointF((float)q2[0],(float)q2[1]));}if(pts.Count>=3)pin.Outline=pts.ToArray();
     pins.Add(pin);}}
   foreach(var p in pins)if(p.Count>1)p.Name=I18n.Format("퀘스트 목표 {0}개",p.Count);
   return pins;
  }
 }
 // Tarkov writes the player's position and rotation into in-raid screenshot names:
 //   2024-01-01[12-00]_123.45, 6.78, -90.12_0.00000, 0.70711, 0.00000, 0.70711_12.34 (0).png
 // Only names are read (files the player took); nothing is captured or sent to the game.
 public static class ScreenshotPosition {
  static readonly Regex name=new Regex(@"_(-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?)_(-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?), (-?\d+(?:\.\d+)?)_");
  // {x, y, z, forwardX, forwardZ} or null.
  public static double[] Parse(string file){
   var m=name.Match(System.IO.Path.GetFileName(file??""));if(!m.Success)return null;var c=System.Globalization.CultureInfo.InvariantCulture;
   double x=double.Parse(m.Groups[1].Value,c),y=double.Parse(m.Groups[2].Value,c),z=double.Parse(m.Groups[3].Value,c);
   double qx=double.Parse(m.Groups[4].Value,c),qy=double.Parse(m.Groups[5].Value,c),qz=double.Parse(m.Groups[6].Value,c),qw=double.Parse(m.Groups[7].Value,c);
   // Unity forward (0,0,1) rotated by the quaternion, projected on the ground plane.
   double fx=2*(qx*qz+qw*qy),fz=1-2*(qx*qx+qy*qy);
   return new[]{x,y,z,fx,fz};
  }
  // New screenshots, queued from the watcher thread; the companion loop takes them.
  static System.IO.FileSystemWatcher watcher;static readonly System.Collections.Concurrent.ConcurrentQueue<string> queue=new System.Collections.Concurrent.ConcurrentQueue<string>();
  public static string Folder(){return System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments),"Escape from Tarkov","Screenshots");}
  public static bool Watching{get{return watcher!=null;}}
  public static void Watch(bool on){
   if(!on){if(watcher!=null){watcher.EnableRaisingEvents=false;watcher.Dispose();watcher=null;}return;}
   if(watcher!=null||!System.IO.Directory.Exists(Folder()))return;
   watcher=new System.IO.FileSystemWatcher(Folder(),"*.png");watcher.NotifyFilter=System.IO.NotifyFilters.FileName;watcher.Created+=(s,e)=>queue.Enqueue(e.FullPath);watcher.EnableRaisingEvents=true;
  }
  public static string Next(){string f;return queue.TryDequeue(out f)?f:null;}
 }
 public class MapCanvas:Control {
  public Image MapImage;public List<Pin> Pins=new List<Pin>();public HashSet<string> Available=new HashSet<string>();public Dictionary<string,TaxiState> TaxiStates=new Dictionary<string,TaxiState>();
  // Player position from the last in-raid screenshot (normalised) and facing (image-pixel direction).
  public bool MeShown;public float MeX,MeY,MeDirX,MeDirY;
  public float Zoom=1,PanX,PanY;
  // Edit mode: click empty map = new pin, drag a pin = move it, right-click a pin = delete. Dirty tells the
  // helper script to save; SelectionChanged lets the window bind its editor fields to Selected.
  public bool EditMode,Dirty;public Pin Selected;public event EventHandler SelectionChanged;
  Pin hover,dragging;Point hoverAt;
  Point last;bool drag,moving;readonly Timer settle=new Timer{Interval=150};Font pinFont,tipFont,tipBold;
  // High-quality frame of the map as last settled (zoom/pan/size), so repaints are a 4 ms copy and panning
  // shifts it instead of rescaling. GDI+ "Bilinear" costs ~150 ms at 2100x1700; nearest-neighbour ~8 ms,
  // HighQualityBilinear ~50 ms once per settle.
  Bitmap cache;float cacheZoom,cachePanX,cachePanY;Size cacheSize;
  public MapCanvas(){SetStyle(ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.UserPaint|ControlStyles.Opaque,true);Dock=DockStyle.Fill;BackColor=Theme.Bg;
   settle.Tick+=delegate{settle.Stop();moving=false;Invalidate();};
   // "You are here" pulse: only while the map is visible and a position is shown; repaints just that spot.
   pulse.Tick+=delegate{bool quests=false;foreach(var p in Pins)if(p.Kind=="quest"){quests=true;break;}
    if((!MeShown&&Available.Count==0&&!quests&&HighlightRef.Length==0)||!Visible||MapImage==null){pulse.Stop();return;}
    var o=Origin;float s=Scale,e=Radius*3.6f;var area=new Region(Rectangle.Empty);if(MeShown)area.Union(MeArea());
    foreach(var p in Pins)if(p.Kind=="exit"&&Available.Contains(p.Name))area.Union(Rectangle.Round(new RectangleF(o.X+p.X*MapImage.Width*s-e,o.Y+p.Y*MapImage.Height*s-e,2*e,2*e)));
    if(quests&&!moving)foreach(var p in Pins)if(p.Kind=="quest")area.Union(QuestArea(p));
    if(HighlightRef.Length>0)foreach(var p in Pins)if(IsHighlighted(p)){var at=ToScreen(p);float ke=Math.Max(70,Radius*6);area.Union(Rectangle.Round(new RectangleF(at.X-ke,at.Y-ke,2*ke,2*ke)));}
    Invalidate(area);area.Dispose();};}
  // Expanding, fading ring around (x,y), one every periodMs.
  static void DrawWave(Graphics g,float x,float y,float r,int periodMs,Color c){
   float t=(Environment.TickCount&0x7fffffff)%periodMs/(float)periodMs,ring=r*(1.1f+2.4f*t);
   using(var wave=new Pen(Color.FromArgb((int)(235*(1-t)),c),3.2f))g.DrawEllipse(wave,x-ring,y-ring,2*ring,2*ring);
  }
  readonly Timer pulse=new Timer{Interval=50};
  Rectangle MeArea(){var o=Origin;float s=Scale,x=o.X+MeX*MapImage.Width*s,y=o.Y+MeY*MapImage.Height*s,e=Radius*3.6f;return Rectangle.Round(new RectangleF(x-e,y-e,2*e,2*e));}
  void DrawMap(Graphics g,InterpolationMode mode){
   using(var bg=new SolidBrush(BackColor))g.FillRectangle(bg,0,0,Width,Height);var o=Origin;float s=Scale;
   var view=RectangleF.Intersect(new RectangleF(o.X,o.Y,MapImage.Width*s,MapImage.Height*s),new RectangleF(0,0,Width,Height));if(view.Width<=0||view.Height<=0)return;
   var src=new RectangleF((view.X-o.X)/s,(view.Y-o.Y)/s,view.Width/s,view.Height/s);
   g.CompositingMode=CompositingMode.SourceCopy;g.PixelOffsetMode=PixelOffsetMode.Half;g.InterpolationMode=mode;g.DrawImage(MapImage,view,src,GraphicsUnit.Pixel);g.CompositingMode=CompositingMode.SourceOver;
  }
  new float Scale{get{return MapImage==null?1:Math.Min((float)Width/MapImage.Width,(float)Height/MapImage.Height)*Zoom;}}
  PointF Origin{get{return MapImage==null?PointF.Empty:new PointF((Width-MapImage.Width*Scale)/2+PanX,(Height-MapImage.Height*Scale)/2+PanY);}}
  // Premultiplied ARGB is GDI+'s fastest source format for scaled blits.
  public void LoadMap(string path){var previous=MapImage;using(var original=Image.FromFile(path)){var b=new Bitmap(original.Width,original.Height,System.Drawing.Imaging.PixelFormat.Format32bppPArgb);using(var g=Graphics.FromImage(b))g.DrawImage(original,0,0,original.Width,original.Height);MapImage=b;}if(previous!=null)previous.Dispose();if(cache!=null){cache.Dispose();cache=null;}Zoom=1;PanX=PanY=0;Invalidate();}
  public void ResetView(){Zoom=1;PanX=PanY=0;Invalidate();}
  // Fast bilinear while dragging/zooming, one high-quality redraw 150 ms after movement stops.
  void Moved(){moving=true;settle.Stop();settle.Start();Invalidate();}
  // Zoom about a point (cursor or centre) so the spot under it stays put.
  public void ZoomAt(float factor,Point at){
   if(MapImage==null)return;float z=Math.Max(1,Math.Min(8,Zoom*factor));if(z==Zoom)return;var o=Origin;float s=Scale;
   float mx=(at.X-o.X)/s,my=(at.Y-o.Y)/s;Zoom=z;float s2=Scale;
   float baseX=(Width-MapImage.Width*s2)/2,baseY=(Height-MapImage.Height*s2)/2;PanX=at.X-mx*s2-baseX;PanY=at.Y-my*s2-baseY;Moved();
  }
  // This map's bosses and their spawn chance (Quests.BossSummary): first line bold, in a dark box at the top-left.
  public string Info="";
  void DrawInfo(Graphics g){
   if(string.IsNullOrEmpty(Info))return;var lines=Info.Split('\n');float pad=8,y0=10,x0=10,w=0,h=0;var sizes=new SizeF[lines.Length];
   for(int i=0;i<lines.Length;i++){sizes[i]=g.MeasureString(lines[i],i==0?tipBold:tipFont);w=Math.Max(w,sizes[i].Width);h+=sizes[i].Height;}
   var box=new RectangleF(x0,y0,w+pad*2,h+pad*2);using(var path=new GraphicsPath()){float d=10;path.AddArc(box.Left,box.Top,d,d,180,90);path.AddArc(box.Right-d,box.Top,d,d,270,90);path.AddArc(box.Right-d,box.Bottom-d,d,d,0,90);path.AddArc(box.Left,box.Bottom-d,d,d,90,90);path.CloseFigure();
    using(var b=new SolidBrush(Color.FromArgb(215,14,18,26)))g.FillPath(b,path);using(var pen=new Pen(Color.FromArgb(120,90,110,150),1f))g.DrawPath(pen,path);}
   float y=y0+pad;using(var ink=new SolidBrush(Color.FromArgb(240,240,244,252)))using(var dim=new SolidBrush(Color.FromArgb(230,190,200,218)))for(int i=0;i<lines.Length;i++){g.DrawString(lines[i],i==0?tipBold:tipFont,i==0?ink:dim,x0+pad,y);y+=sizes[i].Height;}
  }
  protected override void OnPaint(PaintEventArgs e){
   var g=e.Graphics;if(MapImage==null||Width<1||Height<1){using(var bg=new SolidBrush(BackColor))g.FillRectangle(bg,ClientRectangle);return;}
   var o=Origin;float s=Scale;bool sameView=cache!=null&&cacheSize==Size&&cacheZoom==Zoom;
   if(!moving&&!(sameView&&cachePanX==PanX&&cachePanY==PanY)){
    // Settled on a new view: render it once in high quality.
    if(cache==null||cacheSize!=Size){if(cache!=null)cache.Dispose();cache=new Bitmap(Width,Height,System.Drawing.Imaging.PixelFormat.Format32bppPArgb);cacheSize=Size;}
    using(var cg=Graphics.FromImage(cache))DrawMap(cg,InterpolationMode.HighQualityBilinear);cacheZoom=Zoom;cachePanX=PanX;cachePanY=PanY;sameView=true;
   }
   if(sameView&&cachePanX==PanX&&cachePanY==PanY)g.DrawImageUnscaled(cache,0,0);
   else{
    // Moving: quick nearest-neighbour frame; while only panning, lay the sharp cached frame over it, shifted.
    DrawMap(g,InterpolationMode.NearestNeighbor);
    if(sameView)g.DrawImageUnscaled(cache,(int)Math.Round(PanX-cachePanX),(int)Math.Round(PanY-cachePanY));
   }
   g.SmoothingMode=SmoothingMode.AntiAlias;g.TextRenderingHint=System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
   if(pinFont==null){pinFont=new Font(I18n.FontName,9.5f,FontStyle.Bold);tipFont=new Font(I18n.FontName,9.5f);tipBold=new Font(I18n.FontName,10.5f,FontStyle.Bold);}
   DrawInfo(g);
   if(Pins.Count==0&&!MeShown){DrawKeyTip(g);return;}
   float big=Radius,small=big*0.42f;
   using(var plate=new SolidBrush(Color.FromArgb(200,14,14,15)))
   using(var dot=new SolidBrush(Color.FromArgb(225,226,214,170)))using(var dotEdge=new Pen(Color.FromArgb(220,10,10,10),1.5f))using(var pick=new Pen(Color.FromArgb(255,255,214,64),2.5f)){
    foreach(var p in Pins)if(p.Kind=="quest"&&p.Outline!=null)DrawZone(g,p.Outline);
    // After the exit list was read this raid: usable exits keep their green and send a green wave every 3 s,
    // the other exits turn grey. Transits are drawn as usual.
    bool listed=Available.Count>0;
    foreach(var p in Pins){if(p.Kind=="quest"||IsHighlighted(p))continue;var at=ToScreen(p);float x=at.X,y=at.Y;bool on=p.Kind=="exit"&&p.Name.Length>0&&Available.Contains(p.Name);
     if(on&&!TaxiMarker.IsTaxi(p))DrawWave(g,x,y,small*1.6f,3000,Color.FromArgb(0,230,90));
     if(p.Kind=="key"||p.Kind=="keycard"){
      // A key that would sit on another marker moves up-right a little; a gold dot keeps the exact spot.
      bool crowded=false;foreach(var other in Pins){if(other.Kind=="key"||other.Kind=="quest")continue;var q=ToScreen(other);if(Math.Abs(q.X-x)<small*2.8f&&Math.Abs(q.Y-y)<small*2.8f){crowded=true;break;}}
      if(crowded){using(var spot=new SolidBrush(Color.FromArgb(255,240,190,60)))using(var rim=new Pen(Color.FromArgb(220,0,0,0),1.2f)){float d=Math.Max(2f,small*0.28f);g.FillEllipse(spot,x-d,y-d,2*d,2*d);g.DrawEllipse(rim,x-d,y-d,2*d,2*d);}x+=small*2.1f;y-=small*2.1f;}
      bool lit=HighlightRef.Length>0&&p.Ref==HighlightRef;float ks=small*(lit?1.35f:0.95f);
      if(lit)using(var ring=new Pen(Color.White,2.2f))using(var halo=new Pen(Color.FromArgb(200,0,0,0),4.4f)){float rr=small*2.3f;g.DrawEllipse(halo,x-rr,y-rr,2*rr,2*rr);g.DrawEllipse(ring,x-rr,y-rr,2*rr,2*rr);}
      if(p.Kind=="keycard")DrawKeycard(g,x,y,ks);else DrawKey(g,x,y,ks);}
     else if(TaxiMarker.IsTaxi(p)){TaxiState ts;if(!TaxiStates.TryGetValue(p.Name,out ts))ts=TaxiState.Unknown;if(p.Faded)DrawFaded(g,p.Kind,x,y,small,dot,dotEdge,false,true,ts);else TaxiMarker.Draw(g,x,y,Math.Max(24,small*3.2f),ts);}
     else if(p.Faded)DrawFaded(g,p.Kind,x,y,small,dot,dotEdge,ExitSign.Cooperative(p),false,TaxiState.Unknown,FlareMarker.IsFlare(p));
     else DrawMarker(g,listed&&p.Kind=="exit"&&!on?"grey":p.Kind,x,y,small,dot,dotEdge,ExitSign.Cooperative(p),FlareMarker.IsFlare(p));
     if(p==Selected)g.DrawEllipse(pick,x-big-3,y-big-3,2*big+6,2*big+6);
     // Name tag right of the marker, or left of it when it would leave the map.
     if(on||EditMode){string label=Label(p);var size=g.MeasureString(label,pinFont);float lx=x+big*0.6f;if(lx+size.Width+6>Width-2)lx=x-big*0.6f-size.Width-6;lx=Math.Max(2,lx);g.FillRectangle(plate,lx,y+4,size.Width+6,size.Height+2);g.DrawString(label,pinFont,on?Brushes.White:Brushes.Gainsboro,lx+3,y+5);}
    }
   }
   foreach(var p in Pins)if(p.Note.Length>0){var at=ToScreen(p);var size=g.MeasureString(p.Note,tipFont);float nx=at.X+small*1.9f,ny=at.Y-size.Height/2;
    using(var back=new SolidBrush(Color.FromArgb(p.Faded?120:205,12,12,14)))g.FillRectangle(back,nx-2,ny,size.Width+4,size.Height);using(var ink=new SolidBrush(p.Faded?Color.FromArgb(150,255,255,255):Color.White))g.DrawString(p.Note,tipFont,ink,nx,ny);}
   if(Pins.Count>0)foreach(var p in Pins)if(p.Kind=="quest"){var at=ToScreen(p);DrawQuest(g,at.X,at.Y,Radius*0.55f,p.Count);}
   if(MeShown)DrawMe(g);else if((Available.Count>0||HighlightRef.Length>0)&&!pulse.Enabled)pulse.Start();
   foreach(var p in Pins)if(IsHighlighted(p))DrawFocusedKey(g,p,unchecked(Environment.TickCount-keyJumpAt));
   if(hover!=null&&dragging==null)DrawTip(g,hover);DrawKeyTip(g);
  }
  // Quest objective: a round sky-blue "!" (or count) balloon on a thin stem, its tip on the spot. Blue and round so it
  // never reads as a PMC spawn (yellow triangle); the balloon bobs gently (the tip stays put) so quests stand out from
  // the still markers. Its own zone is a faint blue outline.
  static readonly Color QuestBlue=Color.FromArgb(255,24,160,255);
  Font badgeFont;
  float QuestBob(){return (float)(0.5-0.5*Math.Cos((Environment.TickCount&0x7fffffff)%1800/1800.0*2*Math.PI));}
  // Screen box of a quest marker including the whole bob range, for the pulse repaint.
  Rectangle QuestArea(Pin p){var at=ToScreen(p);float r=Radius*0.55f,hr=r*1.75f,e=hr+4;return Rectangle.Round(new RectangleF(at.X-e,at.Y-r*2.2f-r*0.5f-2*hr-4,2*e,r*2.2f+r*0.5f+2*hr+r*0.6f+8));}
  void DrawQuest(Graphics g,float x,float y,float r,int count){
   float hr=r*1.75f,lift=r*2.2f+r*0.5f*QuestBob(),cx=x,cy=y-lift-hr;
   using(var halo=new Pen(Color.FromArgb(220,0,0,0),3.6f))g.DrawLine(halo,x,y,cx,cy+hr);
   using(var stem=new Pen(Color.White,1.6f))g.DrawLine(stem,x,y,cx,cy+hr);
   using(var tip=new SolidBrush(QuestBlue))using(var tipEdge=new Pen(Color.FromArgb(230,0,0,0),1.4f)){float d=r*0.42f;g.FillEllipse(tip,x-d,y-d,2*d,2*d);g.DrawEllipse(tipEdge,x-d,y-d,2*d,2*d);}
   using(var halo=new Pen(Color.FromArgb(215,0,0,0),4.2f))g.DrawEllipse(halo,cx-hr,cy-hr,2*hr,2*hr);
   using(var fill=new SolidBrush(QuestBlue))g.FillEllipse(fill,cx-hr,cy-hr,2*hr,2*hr);
   using(var rim=new Pen(Color.White,1.8f))g.DrawEllipse(rim,cx-hr,cy-hr,2*hr,2*hr);
   float size=Math.Max(9f,hr*1.4f);if(badgeFont==null||badgeFont.Size!=size){if(badgeFont!=null)badgeFont.Dispose();badgeFont=new Font(I18n.FontName,size,FontStyle.Bold,GraphicsUnit.Pixel);}
   using(var sf=new StringFormat{Alignment=StringAlignment.Center,LineAlignment=StringAlignment.Center})g.DrawString(count>1?count.ToString():"!",badgeFont,Brushes.White,new RectangleF(cx-hr,cy-hr+hr*0.06f,2*hr,2*hr),sf);
   if(!pulse.Enabled)pulse.Start();
  }
  void DrawZone(Graphics g,PointF[] outline){
   var o=Origin;float s=Scale;var pts=new PointF[outline.Length];for(int i=0;i<pts.Length;i++)pts[i]=new PointF(o.X+outline[i].X*MapImage.Width*s,o.Y+outline[i].Y*MapImage.Height*s);
   using(var fill=new SolidBrush(Color.FromArgb(40,24,160,255)))using(var edge=new Pen(Color.FromArgb(210,24,160,255),1.5f){DashStyle=DashStyle.Dash}){g.FillPolygon(fill,pts);g.DrawPolygon(edge,pts);}
  }
  // "You are here": red dot with a white rim, a red ring growing and fading every 1.2 s, and a cone toward
  // the facing direction, so the eye finds it at once on any map.
  void DrawMe(Graphics g){
   var o=Origin;float s=Scale,x=o.X+MeX*MapImage.Width*s,y=o.Y+MeY*MapImage.Height*s,r=Radius*0.75f;
   double len=Math.Sqrt(MeDirX*MeDirX+MeDirY*MeDirY);float dx=len>0?(float)(MeDirX/len):0,dy=len>0?(float)(MeDirY/len):-1;
   float t=(Environment.TickCount&0x7fffffff)%1200/1200f,ring=r*(1.2f+1.9f*t);
   using(var wave=new Pen(Color.FromArgb((int)(230*(1-t)),255,40,40),3.2f))g.DrawEllipse(wave,x-ring,y-ring,2*ring,2*ring);
   float tipX=x+dx*r*3.0f,tipY=y+dy*r*3.0f,side=r*1.05f;
   var cone=new[]{new PointF(tipX,tipY),new PointF(x-dy*side,y+dx*side),new PointF(x+dy*side,y-dx*side)};
   using(var c=new SolidBrush(Color.FromArgb(170,255,60,60)))g.FillPolygon(c,cone);
   using(var halo=new Pen(Color.FromArgb(200,0,0,0),5f))g.DrawEllipse(halo,x-r,y-r,2*r,2*r);
   using(var b=new SolidBrush(Color.FromArgb(255,240,24,24)))g.FillEllipse(b,x-r,y-r,2*r,2*r);
   using(var w=new Pen(Color.White,2.6f))g.DrawEllipse(w,x-r,y-r,2*r,2*r);
   if(!pulse.Enabled)pulse.Start();
  }  // exit: dark green dot, transit: purple dot, PMC spawn: yellow triangle, boss: red diamond (black outline),
  // cultist: black diamond (white outline). Exits/transits are drawn a little larger: they matter most.
  // Saturated fill, a dark halo and a thin white rim: readable on pale (Customs) and dark (Streets) maps alike.
  // A marker at 40% opacity: drawn into a small layer, then laid on the map through an alpha colour matrix.
  static void DrawFaded(Graphics g,string kind,float x,float y,float r,Brush exitFill,Pen edge,bool cooperative=false,bool taxi=false,TaxiState taxiState=TaxiState.Unknown,bool flare=false){
   int size=(int)Math.Ceiling(Math.Max(r*6,Math.Max(24,r*3.2f)*1.5f))+4;if(size<4)return;using(var layer=new Bitmap(size,size,System.Drawing.Imaging.PixelFormat.Format32bppPArgb)){
    using(var lg=Graphics.FromImage(layer)){lg.SmoothingMode=SmoothingMode.AntiAlias;lg.Clear(Color.Transparent);if(taxi)TaxiMarker.Draw(lg,size/2f,size/2f,Math.Max(24,r*3.2f),taxiState);else DrawMarker(lg,kind,size/2f,size/2f,r,exitFill,edge,cooperative,flare);}
    var cm=new System.Drawing.Imaging.ColorMatrix{Matrix33=0.4f};using(var ia=new System.Drawing.Imaging.ImageAttributes()){ia.SetColorMatrix(cm);
     g.DrawImage(layer,new Rectangle((int)Math.Round(x-size/2f),(int)Math.Round(y-size/2f),size,size),0,0,size,size,GraphicsUnit.Pixel,ia);}}
  }
  static void DrawMarker(Graphics g,string kind,float x,float y,float r,Brush exitFill,Pen edge,bool cooperative=false,bool flare=false){
   if(flare){FlareMarker.Draw(g,x,y,Math.Max(24,r*3.2f),kind=="grey"?Color.FromArgb(128,128,128):ExitSign.Green);return;}
   if(kind=="exit"||kind=="transit"||kind=="grey"){ExitSign.Draw(g,x,y,Math.Max(24,r*3.2f),kind=="grey"?Color.FromArgb(128,128,128):kind=="transit"?ExitSign.Orange:ExitSign.Green,cooperative);return;}
   Color c=kind=="grey"?Color.FromArgb(255,128,128,128):kind=="exit"?Color.FromArgb(255,0,214,84):kind=="transit"?Color.FromArgb(255,176,72,255):kind=="pmc"?Color.FromArgb(255,255,222,0):kind=="boss"?Color.FromArgb(255,255,36,36):Color.FromArgb(255,16,16,16);
   // Bosses: a white skull, cultists: a black hooded figure (red stays for "you are here" only).
   if(kind=="boss"){DrawSkull(g,x,y,r*1.6f);return;}if(kind=="cultist"){DrawHooded(g,x,y,r*1.7f);return;}
   PointF[] shape=null;float e=r*1.6f;if(kind=="grey")kind="exit";
   if(kind=="pmc"){float s=r*1.5f;shape=new[]{new PointF(x-s,y-s*0.8f),new PointF(x+s,y-s*0.8f),new PointF(x,y+s)};}
   else if(kind=="boss"||kind=="cultist"){float s=r*1.8f;shape=new[]{new PointF(x,y-s),new PointF(x+s,y),new PointF(x,y+s),new PointF(x-s,y)};}
   using(var b=new SolidBrush(c))using(var halo=new Pen(Color.FromArgb(215,0,0,0),4.2f){LineJoin=LineJoin.Round})using(var rim=new Pen(Color.FromArgb(240,255,255,255),1.3f)){
    if(shape==null){g.DrawEllipse(halo,x-e,y-e,2*e,2*e);g.FillEllipse(b,x-e,y-e,2*e,2*e);g.DrawEllipse(rim,x-e,y-e,2*e,2*e);}
    else{g.DrawPolygon(halo,shape);g.FillPolygon(b,shape);g.DrawPolygon(rim,shape);}
   }
  }
  string highlightRef="";int keyJumpAt;
  public string HighlightRef{get{return highlightRef;}set{if(highlightRef==value)return;highlightRef=value??"";keyJumpAt=Environment.TickCount;pulse.Interval=highlightRef.Length>0?16:50;if(highlightRef.Length>0&&!pulse.Enabled&&Visible)pulse.Start();Invalidate();}}
  bool IsHighlighted(Pin p){return HighlightRef.Length>0&&p.Ref==HighlightRef&&(p.Kind=="key"||p.Kind=="keycard");}
  public bool FocusKey(string id){
   if(MapImage==null||string.IsNullOrEmpty(id))return false;float minX=1,minY=1,maxX=0,maxY=0;int count=0;
   foreach(var p in Pins)if(p.Ref==id&&(p.Kind=="key"||p.Kind=="keycard")){minX=Math.Min(minX,p.X);maxX=Math.Max(maxX,p.X);minY=Math.Min(minY,p.Y);maxY=Math.Max(maxY,p.Y);count++;}
   if(count==0)return false;float fit=Math.Min((float)Width/MapImage.Width,(float)Height/MapImage.Height),roomX=Math.Max(1,Width-140),roomY=Math.Max(1,Height-140);
   float wanted=Math.Min(roomX/Math.Max(1,(maxX-minX)*MapImage.Width),roomY/Math.Max(1,(maxY-minY)*MapImage.Height))/fit;
   Zoom=Math.Max(1,Math.Min(count==1?2.2f:3f,wanted));PanX=(0.5f-(minX+maxX)/2)*MapImage.Width*Scale;PanY=(0.5f-(minY+maxY)/2)*MapImage.Height*Scale;
   HighlightRef=id;keyJumpAt=Environment.TickCount;Moved();return true;
  }
  // Eased hop, a single impact ripple and a small damped squash/rock about the ground point.
  void DrawFocusedKey(Graphics g,Pin p,int elapsed){var at=ToScreen(p);float r=Radius*.42f,size=r*2.05f;uint ms=(uint)elapsed%1400;
   float air=ms/900f,lift=ms<900?(float)(.5-.5*Math.Cos(air*2*Math.PI))*Math.Max(22,r*3.1f):0,squash=0,angle=0;
   if(ms>=900){float age=ms-900,t=age/500f,spread=t*(2-t),fade=1-t,rx=size*(.45f+3.15f*spread),ry=rx*.34f;
    var c=p.Kind=="keycard"?Color.FromArgb(70,205,250):Color.FromArgb(250,202,78);
    using(var soft=new Pen(Color.FromArgb((int)(55*fade),0,0,0),Math.Max(2.2f,r*.36f)))using(var wave=new Pen(Color.FromArgb((int)(220*fade),c),Math.Max(1.1f,r*.17f))){
     g.DrawEllipse(soft,at.X-rx,at.Y-ry,2*rx,2*ry);g.DrawEllipse(wave,at.X-rx,at.Y-ry,2*rx,2*ry);}
    squash=(float)(.12*Math.Sin(age*Math.PI/90)*Math.Exp(-age/130));angle=(float)(3.2*Math.Sin(age*Math.PI/80)*Math.Exp(-age/120));}
   var state=g.Save();g.TranslateTransform(at.X,at.Y-lift);g.RotateTransform(angle);g.ScaleTransform(1+squash,1-squash);
   if(p.Kind=="keycard")DrawKeycard(g,0,-size*.85f,size);else DrawKey(g,0,-size*.8f,size);g.Restore(state);
  }
  string keyTipName="",keyTipDescription="",keyTipPlaces="";int keyTipY;
  public Rectangle KeyTipBounds{get;private set;}
  public void SetKeyTip(string name,string description,string places,int y){keyTipName=name??"";keyTipDescription=description??"";keyTipPlaces=places??"";keyTipY=y;KeyTipBounds=Rectangle.Empty;Invalidate();}
  void DrawKeyTip(Graphics g){
   KeyTipBounds=Rectangle.Empty;if(keyTipName.Length==0||Width<40||Height<40)return;float k=DeviceDpi/96f,pad=12*k,gap=8*k,w=Math.Min(380*k,Width-2*gap),inner=Math.Max(1,w-2*pad),maxH=Height-2*gap;
   float titleH=Math.Min(tipBold.GetHeight(g)*2,g.MeasureString(keyTipName,tipBold,(int)inner).Height),placesH=keyTipPlaces.Length>0?Math.Min(tipFont.GetHeight(g)*2,g.MeasureString(keyTipPlaces,tipFont,(int)inner).Height):0;
   string hint=I18n.Text("클릭하면 사용처를 확인할 수 있습니다.");float hintH=Math.Min(tipFont.GetHeight(g)*2,g.MeasureString(hint,tipFont,(int)inner).Height);
   float bodyH=keyTipDescription.Length>0?Math.Min(tipFont.GetHeight(g)*6,g.MeasureString(keyTipDescription,tipFont,(int)inner).Height):0;
   bodyH=Math.Max(0,Math.Min(bodyH,maxH-2*pad-titleH-placesH-hintH-3*gap));float h=Math.Min(maxH,2*pad+titleH+placesH+hintH+(bodyH>0?bodyH+gap:0)+2*gap);
   var box=new RectangleF(Math.Max(gap,Width-w-gap),Math.Max(gap,Math.Min(keyTipY,Height-h-gap)),w,h);KeyTipBounds=Rectangle.Ceiling(box);
   using(var back=new SolidBrush(Color.FromArgb(252,Theme.Panel)))g.FillRectangle(back,box);using(var edge=new Pen(Theme.Border))g.DrawRectangle(edge,box.X,box.Y,box.Width-1,box.Height-1);
   using(var accent=new SolidBrush(Theme.Accent))g.FillRectangle(accent,box.X,box.Y,3*k,box.Height);
   var save=g.Save();g.SetClip(RectangleF.Inflate(box,-pad,-pad));float y=box.Y+pad;using(var sf=new StringFormat{Trimming=StringTrimming.EllipsisCharacter,FormatFlags=StringFormatFlags.LineLimit}){
    using(var ink=new SolidBrush(Theme.Ink))g.DrawString(keyTipName,tipBold,ink,new RectangleF(box.X+pad,y,inner,titleH),sf);y+=titleH+gap;
    using(var ink=new SolidBrush(Theme.AccentText))if(placesH>0){g.DrawString(keyTipPlaces,tipFont,ink,new RectangleF(box.X+pad,y,inner,placesH),sf);y+=placesH+gap;}
    using(var ink=new SolidBrush(Theme.Muted))if(bodyH>0){g.DrawString(keyTipDescription,tipFont,ink,new RectangleF(box.X+pad,y,inner,bodyH),sf);y+=bodyH+gap;}
    using(var ink=new SolidBrush(Theme.AccentText))g.DrawString(hint,tipFont,ink,new RectangleF(box.X+pad,y,inner,hintH),sf);
   }g.Restore(save);
  }
  // Small cyan keycard (magnetic stripe, gold chip), the same size class as the key.
  static void DrawKeycard(Graphics g,float x,float y,float s){
   float w=s*1.9f,h=s*1.25f;var r=new RectangleF(x-w/2,y-h/2,w,h);float c=Math.Max(2f,s*0.3f);
   using(var path=new GraphicsPath()){path.AddArc(r.X,r.Y,c,c,180,90);path.AddArc(r.Right-c,r.Y,c,c,270,90);path.AddArc(r.Right-c,r.Bottom-c,c,c,0,90);path.AddArc(r.X,r.Bottom-c,c,c,90,90);path.CloseFigure();
    using(var halo=new Pen(Color.FromArgb(220,0,0,0),Math.Max(2.6f,s*0.45f)))g.DrawPath(halo,path);using(var fill=new SolidBrush(Color.FromArgb(255,64,196,236)))g.FillPath(fill,path);}
   using(var stripe=new SolidBrush(Color.FromArgb(255,24,28,36)))g.FillRectangle(stripe,r.X,r.Y+h*0.22f,w,h*0.22f);
   using(var chip=new SolidBrush(Color.FromArgb(255,240,190,60)))g.FillRectangle(chip,r.X+w*0.14f,r.Y+h*0.56f,w*0.24f,h*0.26f);
  }
  // Small gold key lying across the spot (bow on the left, bit on the right), dark halo; kept smaller than the other markers.
  static void DrawKey(Graphics g,float x,float y,float s){
   using(var halo=new Pen(Color.FromArgb(220,0,0,0),Math.Max(3f,s*0.62f)){StartCap=LineCap.Round,EndCap=LineCap.Round})
   using(var gold=new Pen(Color.FromArgb(255,240,190,60),Math.Max(1.6f,s*0.32f)){StartCap=LineCap.Round,EndCap=LineCap.Round}){
    float bx=x-s*0.75f,br=s*0.48f;var shaft=new[]{new PointF(bx+br,y),new PointF(x+s*0.95f,y)};var bit1=new[]{new PointF(x+s*0.55f,y),new PointF(x+s*0.55f,y+s*0.45f)};var bit2=new[]{new PointF(x+s*0.85f,y),new PointF(x+s*0.85f,y+s*0.38f)};
    foreach(var pen in new[]{halo,gold}){g.DrawEllipse(pen,bx-br,y-br,2*br,2*br);g.DrawLine(pen,shaft[0],shaft[1]);g.DrawLine(pen,bit1[0],bit1[1]);g.DrawLine(pen,bit2[0],bit2[1]);}}
  }
  // White skull centred on x,y (s ~ half its width) with a dark halo so it reads on pale and dark maps.
  static void DrawSkull(Graphics g,float x,float y,float s){
   using(var p=new GraphicsPath{FillMode=FillMode.Winding}){p.AddEllipse(x-s,y-s*1.05f,2*s,1.7f*s);p.AddRectangle(new RectangleF(x-s*0.56f,y+s*0.3f,s*1.12f,s*0.62f));
    using(var halo=new Pen(Color.FromArgb(225,0,0,0),Math.Max(3f,s*0.34f)){LineJoin=LineJoin.Round})g.DrawPath(halo,p);g.FillPath(Brushes.White,p);}
   using(var hole=new SolidBrush(Color.FromArgb(255,18,18,20))){
    g.FillEllipse(hole,x-s*0.68f,y-s*0.42f,s*0.52f,s*0.56f);g.FillEllipse(hole,x+s*0.16f,y-s*0.42f,s*0.52f,s*0.56f);
    g.FillPolygon(hole,new[]{new PointF(x,y+s*0.12f),new PointF(x-s*0.14f,y+s*0.36f),new PointF(x+s*0.14f,y+s*0.36f)});
    using(var tooth=new Pen(hole,Math.Max(1f,s*0.11f)))foreach(var dx in new[]{-0.24f,0f,0.24f})g.DrawLine(tooth,x+s*dx,y+s*0.6f,x+s*dx,y+s*0.9f);}
  }
  // Black hooded figure with a white rim: hood, shoulders widening into a robe, a grey face in the hood.
  static void DrawHooded(Graphics g,float x,float y,float s){
   using(var p=new GraphicsPath()){ // pointed hood falling into shoulders and a wide robe
    p.AddBezier(x,y-s*1.25f,x+s*0.45f,y-s*1.05f,x+s*0.72f,y-s*0.55f,x+s*0.66f,y-s*0.05f);p.AddLine(x+s*0.66f,y-s*0.05f,x+s*1.02f,y+s*0.95f);
    p.AddLine(x+s*1.02f,y+s*0.95f,x-s*1.02f,y+s*0.95f);p.AddLine(x-s*1.02f,y+s*0.95f,x-s*0.66f,y-s*0.05f);
    p.AddBezier(x-s*0.66f,y-s*0.05f,x-s*0.72f,y-s*0.55f,x-s*0.45f,y-s*1.05f,x,y-s*1.25f);p.CloseFigure();
    using(var rim=new Pen(Color.FromArgb(235,255,255,255),Math.Max(2.4f,s*0.26f)){LineJoin=LineJoin.Round})g.DrawPath(rim,p);
    using(var robe=new SolidBrush(Color.FromArgb(255,10,10,12)))g.FillPath(robe,p);}
   using(var face=new SolidBrush(Color.FromArgb(255,92,92,98)))g.FillEllipse(face,x-s*0.38f,y-s*0.66f,s*0.76f,s*0.82f);
   using(var eye=new SolidBrush(Color.FromArgb(255,10,10,12))){g.FillEllipse(eye,x-s*0.24f,y-s*0.36f,s*0.17f,s*0.17f);g.FillEllipse(eye,x+s*0.07f,y-s*0.36f,s*0.17f,s*0.17f);}
  }
  float Radius{get{return Math.Max(12,Font.Height*0.95f);}}
  PointF ToScreen(Pin p){var o=Origin;float s=Scale;return new PointF(o.X+p.X*MapImage.Width*s,o.Y+p.Y*MapImage.Height*s);}
  static string Label(Pin p){var n=p.Name.Length>0?p.Name:I18n.Text("(이름 없음)");return p.Destination.Length>0?n+" → "+p.Destination:n;}
  Pin PinAt(Point at){if(MapImage==null)return null;float r=Radius*1.15f;Pin best=null;float bestD=r*r;
   foreach(var p in Pins){var q=ToScreen(p);if(p.Kind=="quest")q.Y-=Radius*0.55f*(2.2f+1.75f);float dx=q.X-at.X,dy=q.Y-at.Y,d=dx*dx+dy*dy;if(d<=bestD){bestD=d;best=p;}}return best;}
  // Tarkov-style tooltip: name, condition, transit target, and whether the exit list showed it this raid.
  void DrawTip(Graphics g,Pin p){
   var lines=new List<KeyValuePair<string,Font>>();string kind=I18n.Text(p.Kind=="transit"?"트랜짓":p.Kind=="pmc"?"PMC 스폰":p.Kind=="boss"?"보스":p.Kind=="cultist"?"컬티스트":p.Kind=="quest"?"퀘스트":p.Kind=="key"?"열쇠":p.Kind=="keycard"?"키카드":"");
   lines.Add(new KeyValuePair<string,Font>((kind.Length>0?"["+kind+"] ":"")+(p.Name.Length>0?p.Name:I18n.Text("(이름 없음)")),tipBold));
   if(p.Destination.Length>0)lines.Add(new KeyValuePair<string,Font>(I18n.Text("이동: ")+p.Destination,tipFont));
   // Fandom descriptions can be several lines (conditions, boss chance); exits without one say so.
   if(p.Condition.Length>0){foreach(var l in p.Condition.Split('\n'))if(l.Trim().Length>0)lines.Add(new KeyValuePair<string,Font>(l.Trim(),tipFont));}
   else if(p.Kind=="exit"||p.Kind=="transit")lines.Add(new KeyValuePair<string,Font>(I18n.Text("조건: 없음"),tipFont));
   if(TaxiMarker.IsTaxi(p)){TaxiState ts;if(!TaxiStates.TryGetValue(p.Name,out ts))ts=TaxiState.Unknown;lines.Add(new KeyValuePair<string,Font>(I18n.Text(ts==TaxiState.Using?"택시 · 출발 카운트다운 감지":ts==TaxiState.Gone?"택시 · 출발 완료 / 사용 불가":ts==TaxiState.Normal?"택시 · 입금 표시 없음 (차량 유무는 현장에서 확인)":"택시 · 상태 미확인"),tipFont));}
   if(!TaxiMarker.IsTaxi(p)&&Available.Contains(p.Name))lines.Add(new KeyValuePair<string,Font>(I18n.Text("● 이번 레이드에서 사용 가능"),tipFont));
   if(p.Kind=="quest"&&!EditMode)lines.Add(new KeyValuePair<string,Font>(I18n.Text("● 클릭하면 퀘스트 설명과 사진을 봅니다."),tipFont));
   float pad=8,maxW=Math.Max(220,Math.Min(Width*0.45f,560)),limit=Height-2*pad-8;
   float w=0,h=0;var sizes=new List<SizeF>();var shown=new List<KeyValuePair<string,Font>>();
   foreach(var l in lines){var sz=g.MeasureString(l.Key,l.Value,(int)maxW);if(h+sz.Height>limit){var more=new KeyValuePair<string,Font>(I18n.Format("… (+{0}줄)",lines.Count-shown.Count),tipFont);shown.Add(more);sizes.Add(g.MeasureString(more.Key,tipFont));h+=sizes[sizes.Count-1].Height;break;}
    shown.Add(l);sizes.Add(sz);w=Math.Max(w,sz.Width);h+=sz.Height;}
   float boxW=w+2*pad+2,boxH=h+2*pad,x=hoverAt.X+18,y=hoverAt.Y+18;
   if(x+boxW>Width-4)x=hoverAt.X-boxW-10;if(y+boxH>Height-4)y=hoverAt.Y-boxH-10;
   x=Math.Max(4,Math.Min(x,Width-boxW-4));y=Math.Max(4,Math.Min(y,Height-boxH-4));
   using(var bg=new SolidBrush(Color.FromArgb(246,Theme.Panel)))g.FillRectangle(bg,x,y,boxW,boxH);
   using(var edge=new Pen(Theme.Border))g.DrawRectangle(edge,x,y,boxW,boxH);
   using(var bar=new SolidBrush(Theme.Accent))g.FillRectangle(bar,x,y,3,boxH);
   float cy=y+pad;for(int i=0;i<shown.Count;i++){var color=i==0?Theme.Ink:shown[i].Key.StartsWith("●")?Theme.AccentText:Theme.Muted;
    using(var b=new SolidBrush(color))g.DrawString(shown[i].Key,shown[i].Value,b,new RectangleF(x+pad+2,cy,maxW,sizes[i].Height));cy+=sizes[i].Height;}  }
  void Select(Pin p){Selected=p;var h=SelectionChanged;if(h!=null)h(this,EventArgs.Empty);Invalidate();}
  protected override void OnMouseEnter(EventArgs e){base.OnMouseEnter(e);Focus();}
  protected override void OnMouseLeave(EventArgs e){base.OnMouseLeave(e);if(hover!=null){hover=null;Invalidate();}}
  protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);if(MapImage==null)return;
   if(EditMode){
    var hit=PinAt(e.Location);
    if(e.Button==MouseButtons.Right){if(hit!=null){Pins.Remove(hit);if(Selected==hit)Select(null);Dirty=true;hover=null;Invalidate();}return;}
    if(e.Button!=MouseButtons.Left)return;
    if(hit==null){var o=Origin;float s=Scale;float nx=(e.X-o.X)/(MapImage.Width*s),ny=(e.Y-o.Y)/(MapImage.Height*s);
     if(nx<0||nx>1||ny<0||ny>1)return;hit=new Pin{X=nx,Y=ny};Pins.Add(hit);Dirty=true;}
    Select(hit);dragging=hit;Capture=true;return;
   }
   if(e.Button!=MouseButtons.Left)return;drag=true;last=e.Location;Capture=true;downAt=e.Location;downPin=PinAt(e.Location);
  }
  // A click (no drag) on a quest marker opens its card (MapWindow.ShowQuest).
  public event EventHandler<PinEventArgs> PinClicked;Point downAt;Pin downPin;
  protected override void OnMouseMove(MouseEventArgs e){base.OnMouseMove(e);
   if(dragging!=null){var o=Origin;float s=Scale;dragging.X=Math.Max(0,Math.Min(1,(e.X-o.X)/(MapImage.Width*s)));dragging.Y=Math.Max(0,Math.Min(1,(e.Y-o.Y)/(MapImage.Height*s)));Invalidate();return;}
   if(drag){PanX+=e.X-last.X;PanY+=e.Y-last.Y;last=e.Location;Moved();return;}
   var h=PinAt(e.Location);if(h!=hover||(h!=null&&e.Location!=hoverAt)){hover=h;hoverAt=e.Location;Invalidate();}
   Cursor=h!=null&&h.Kind=="quest"&&!EditMode?Cursors.Hand:Cursors.Default;
  }
  protected override void OnMouseUp(MouseEventArgs e){if(dragging!=null){dragging=null;Dirty=true;}
   bool click=drag&&downPin!=null&&downPin.Kind=="quest"&&Math.Abs(e.X-downAt.X)<5&&Math.Abs(e.Y-downAt.Y)<5&&PinAt(e.Location)==downPin;var hit=downPin;downPin=null;
   drag=false;Capture=false;base.OnMouseUp(e);if(click){var c=PinClicked;if(c!=null)c(this,new PinEventArgs(hit));}}
  protected override void OnMouseWheel(MouseEventArgs e){ZoomAt(e.Delta>0?1.2f:1/1.2f,e.Location);base.OnMouseWheel(e);}
  protected override void Dispose(bool d){if(d){settle.Dispose();pulse.Dispose();if(MapImage!=null)MapImage.Dispose();foreach(var f in new[]{pinFont,tipFont,tipBold})if(f!=null)f.Dispose();if(cache!=null)cache.Dispose();}base.Dispose(d);}
 }
 // "My keys and keycards" beside the map: a search box to add, the list of what was added (icon, name, where it opens),
 // an x to remove. Hovering a row highlights that key's locks on the map. Stored per mode in my-keys.json; nothing is
 // read from the game. Icons are tarkov.dev's 64 px item icons, cached under %LOCALAPPDATA%\ModeSwitch\icons.
 public class KeyLocationEventArgs:EventArgs{public readonly string Id,MapId;public KeyLocationEventArgs(string id,string map){Id=id;MapId=map;}}
 public class KeyPanel:Panel {
  public event EventHandler Changed,HoverChanged;
  public event EventHandler<KeyLocationEventArgs> LocateRequested;
  public string Hover="",ActiveRef="";Dictionary<string,int> locateOrder;
  public string HoverName{get{var e=ById(Hover);return e==null?"":e.Name;}}
  public string HoverDescription{get{var e=ById(Hover);return e==null?"":e.Description;}}
  public string HoverPlaces{get{var e=ById(Hover);return e==null?"":Where(e);}}
  public string HoverWiki{get{var e=ById(Hover);return e==null?"":e.Wiki;}}
  public bool HoverCard{get{var e=ById(Hover);return e!=null&&e.Card;}}
  public int HoverY{get{for(int i=0;i<list.Items.Count;i++)if(((Entry)list.Items[i]).Id==Hover)return list.Top+list.GetItemRectangle(i).Top;return 0;}}
  void NotifyHover(){list.Invalidate();if(HoverChanged!=null)HoverChanged(this,EventArgs.Empty);}
  void HoverRow(Point at){int i=list.IndexFromPoint(at);var id=i>=0&&list.GetItemRectangle(i).Contains(at)&&at.X<list.Width-RemoveWidth?((Entry)list.Items[i]).Id:"";
   if(id==Hover)return;Hover=id;if(ActiveRef!=id)ActiveRef="";list.Cursor=id.Length>0?Cursors.Hand:Cursors.Default;NotifyHover();}
  public void ClearPreview(){if(Hover.Length==0&&ActiveRef.Length==0)return;Hover=ActiveRef="";NotifyHover();}
  public bool Locate(string id){
   var e=ById(id);if(e==null||!mine.Contains(id)||e.Maps.Count==0)return false;string target=mapId;
   if(!e.Maps.ContainsKey(target)){var choices=new List<string>(e.Maps.Keys);choices.Sort(StringComparer.Ordinal);target=choices[0];}
   locateOrder=new Dictionary<string,int>();for(int i=0;i<list.Items.Count;i++)locateOrder[((Entry)list.Items[i]).Id]=i;
   try{Hover=ActiveRef=id;if(LocateRequested!=null)LocateRequested(this,new KeyLocationEventArgs(id,target));NotifyHover();}finally{locateOrder=null;}return true;
  }
  // Shown name: tarkov.dev's Korean names end with the English one in brackets; the list shows the first part (search uses both).
  static string Short(string name){int open=name.LastIndexOf(" (");return open>0&&name.EndsWith(")")&&I18n.Language!="en"?name.Substring(0,open):name;}
  class Entry{public string Id,Name,Description="",Wiki="";public bool Card;public Dictionary<string,int> Maps=new Dictionary<string,int>();}
  readonly List<Entry> all=new List<Entry>();readonly Dictionary<string,string> mapNames=new Dictionary<string,string>();readonly List<string> mine=new List<string>();
  string myKeysPath="",mode="",mapId="";
  readonly Label title=new Label(),empty=new Label();readonly TextBox search=new TextBox();readonly ListBox found=new ListBox(),list=new ListBox();
  readonly Dictionary<string,Image> icons=new Dictionary<string,Image>();readonly HashSet<string> fetching=new HashSet<string>();
  static readonly Color Ink=Theme.Ink,Muted=Theme.Muted,Accent=Theme.AccentText,Back=Theme.Panel,Well=Theme.Field,Line=Theme.Border;
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern IntPtr SendMessage(IntPtr h,int msg,IntPtr w,string l);
  public KeyPanel(){
   BackColor=Back;
   title.ForeColor=Ink;title.AutoSize=false;title.TextAlign=ContentAlignment.MiddleLeft;
   search.BackColor=Well;search.ForeColor=Ink;search.BorderStyle=BorderStyle.FixedSingle;
   search.HandleCreated+=delegate{SendMessage(search.Handle,0x1501,new IntPtr(1),I18n.Text("열쇠·카드키 이름으로 추가"));}; // cue banner
   foreach(var b in new[]{found,list}){b.DrawMode=DrawMode.OwnerDrawFixed;b.BackColor=Well;b.ForeColor=Ink;b.BorderStyle=BorderStyle.None;b.IntegralHeight=false;b.DrawItem+=DrawRow;}
   found.Visible=false;found.BorderStyle=BorderStyle.FixedSingle;
   empty.ForeColor=Muted;empty.AutoSize=false;empty.Text=I18n.Text("아직 넣은 열쇠가 없습니다. 위 칸에 이름을 입력해 추가하세요.");
   Controls.AddRange(new Control[]{found,title,search,list,empty});found.BringToFront();
   search.TextChanged+=delegate{Find();};
   search.KeyDown+=(s,e)=>{if(e.KeyCode==Keys.Down&&found.Visible&&found.Items.Count>0){found.Focus();found.SelectedIndex=0;e.Handled=true;}
    else if(e.KeyCode==Keys.Enter&&found.Visible&&found.Items.Count>0){Toggle((Entry)found.Items[0]);e.Handled=true;e.SuppressKeyPress=true;}
    else if(e.KeyCode==Keys.Escape&&search.Text.Length>0){search.Text="";e.Handled=true;e.SuppressKeyPress=true;}};
   found.MouseClick+=(s,e)=>{int i=found.IndexFromPoint(e.Location);if(i>=0)Toggle((Entry)found.Items[i]);};
   found.KeyDown+=(s,e)=>{if(e.KeyCode==Keys.Enter&&found.SelectedIndex>=0){Toggle((Entry)found.Items[found.SelectedIndex]);e.Handled=true;}};
   list.MouseClick+=(s,e)=>{int i=list.IndexFromPoint(e.Location);if(i<0)return;var r=list.GetItemRectangle(i);if(e.X>=r.Right-RemoveWidth)Remove(((Entry)list.Items[i]).Id);else if(e.Button==MouseButtons.Left)Locate(((Entry)list.Items[i]).Id);};
   list.MouseMove+=(s,e)=>HoverRow(e.Location);list.MouseLeave+=delegate{ClearPreview();};
   list.MouseWheel+=delegate{if(IsHandleCreated)BeginInvoke((Action)(()=>HoverRow(list.PointToClient(MousePosition))));};
   list.KeyDown+=(s,e)=>{if(e.KeyCode==Keys.Enter&&list.SelectedIndex>=0){Locate(((Entry)list.SelectedItem).Id);e.Handled=true;e.SuppressKeyPress=true;}};
  }
  float K{get{return DeviceDpi/96f;}}
  int RemoveWidth{get{return (int)(34*K);}}
  protected override void OnResize(EventArgs e){base.OnResize(e);float k=K;int pad=(int)(10*k),w=Width-2*pad;
   title.Font=new Font("Malgun Gothic",10.5f*k/k,FontStyle.Bold);title.SetBounds(pad,pad,w,(int)(26*k));
   search.Font=new Font("Malgun Gothic",10f);search.SetBounds(pad,pad+(int)(30*k),w,(int)(28*k));
   list.ItemHeight=(int)(50*k);found.ItemHeight=(int)(40*k);int top=pad+(int)(66*k);
   list.SetBounds(pad,top,w,Height-top-pad);empty.SetBounds(pad,top,w,(int)(80*k));empty.Font=new Font("Malgun Gothic",9f);
   found.SetBounds(pad,search.Bottom+2,w,Math.Min(Height-search.Bottom-pad,found.ItemHeight*8+2));base.OnResize(e);}
  // From the quest-places file (keys, map names) and my-keys.json for this mode; mapId puts this map's keys first.
  public void Load(string dataPath,string myKeys,string gameMode,string map){
   if(locateOrder==null&&(mode!=gameMode||mapId!=map))ClearPreview();
   int scroll=list.TopIndex;myKeysPath=myKeys;mode=gameMode;mapId=map;all.Clear();mapNames.Clear();mine.Clear();
   try{var root=FandomMarkers.Parsed(dataPath);var keys=root!=null&&root.Contains("keys")?root["keys"] as IDictionary:null;
    if(root!=null&&root.Contains("mapNames"))foreach(DictionaryEntry m in (IDictionary)root["mapNames"])mapNames[(string)m.Key]=Convert.ToString(m.Value);
    if(keys!=null)foreach(DictionaryEntry kv in keys){var d=(IDictionary)kv.Value;var e=new Entry{Id=(string)kv.Key,Name=Convert.ToString(d["name"]),Card=Convert.ToBoolean(d["card"]),Description=d.Contains("description")?Convert.ToString(d["description"]):"",Wiki=d.Contains("wiki")?Convert.ToString(d["wiki"]):""};
     foreach(DictionaryEntry m in (IDictionary)d["maps"])e.Maps[(string)m.Key]=Convert.ToInt32(m.Value);if(e.Name.Length>0)all.Add(e);}
    all.Sort((a,b)=>string.Compare(a.Name,b.Name,StringComparison.CurrentCulture));}catch{}
   Prefetch();
   try{if(System.IO.File.Exists(myKeysPath)){var root=(IDictionary)new JavaScriptSerializer().DeserializeObject(System.IO.File.ReadAllText(myKeysPath));var m=root.Contains(mode)?root[mode] as IDictionary:null;if(m!=null)foreach(DictionaryEntry kv in m)mine.Add((string)kv.Key);}}catch{}
   Fill();if(locateOrder!=null&&list.Items.Count>0)list.TopIndex=Math.Min(scroll,list.Items.Count-1);if(Hover.Length>0)NotifyHover();
  }
  Entry ById(string id){foreach(var e in all)if(e.Id==id)return e;return null;}
  void Fill(){
   var rows=new List<Entry>();foreach(var id in mine){var e=ById(id);if(e!=null)rows.Add(e);}
   rows.Sort((a,b)=>{if(locateOrder!=null){int ax,bx;if(locateOrder.TryGetValue(a.Id,out ax)&&locateOrder.TryGetValue(b.Id,out bx))return ax.CompareTo(bx);}int x=a.Maps.ContainsKey(mapId)?0:1,y=b.Maps.ContainsKey(mapId)?0:1;return x!=y?x-y:mine.IndexOf(a.Id)-mine.IndexOf(b.Id);});
   list.BeginUpdate();list.Items.Clear();foreach(var e in rows)list.Items.Add(e);list.EndUpdate();
   int here=0;foreach(var e in rows)if(e.Maps.ContainsKey(mapId))here++;
   title.Text=I18n.Format("내 열쇠·카드키 {0}개",rows.Count)+(rows.Count>0?"  ·  "+I18n.Format("이 맵 {0}개",here):"");
   empty.Visible=rows.Count==0;if(found.Visible)Find();
  }
  void Find(){
   var q=search.Text.Trim();found.BeginUpdate();found.Items.Clear();
   if(q.Length>0)foreach(var e in all)if(e.Name.IndexOf(q,StringComparison.CurrentCultureIgnoreCase)>=0){found.Items.Add(e);if(found.Items.Count>=60)break;}
   found.EndUpdate();found.Visible=q.Length>0;if(found.Visible){found.BringToFront();found.Height=Math.Max(found.ItemHeight,Math.Min(Height-search.Bottom-10,found.ItemHeight*Math.Max(1,Math.Min(8,found.Items.Count))+2));}
  }
  void Toggle(Entry e){if(mine.Contains(e.Id))mine.Remove(e.Id);else mine.Add(e.Id);Save();Fill();found.Invalidate();search.Focus();}
  void Remove(string id){mine.Remove(id);if(Hover==id||ActiveRef==id)ClearPreview();Save();Fill();}
  void Save(){
   try{var js=new JavaScriptSerializer();var root=System.IO.File.Exists(myKeysPath)?js.Deserialize<Dictionary<string,object>>(System.IO.File.ReadAllText(myKeysPath)):null;if(root==null)root=new Dictionary<string,object>();
    var m=new Dictionary<string,object>();foreach(var id in mine){var e=ById(id);m[id]=e!=null?e.Name:id;}root[mode]=m;
    var tmp=myKeysPath+".tmp";System.IO.File.WriteAllText(tmp,js.Serialize(root));if(System.IO.File.Exists(myKeysPath))System.IO.File.Replace(tmp,myKeysPath,null);else System.IO.File.Move(tmp,myKeysPath);}catch{}
   if(Changed!=null)Changed(this,EventArgs.Empty);
  }
  static string IconDir{get{return System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"ModeSwitch","icons");}}
  // Every key icon once, in the background (211 icons, about 0.5 MB): the list then never shows an empty frame.
  static bool prefetched;
  void Prefetch(){
   if(prefetched||all.Count==0)return;prefetched=true;var ids=new List<string>();foreach(var e in all)ids.Add(e.Id);
   var t=new System.Threading.Thread(()=>{bool any=false;foreach(var id in ids){var file=System.IO.Path.Combine(IconDir,id+".jpg");if(System.IO.File.Exists(file))continue;
     try{var bytes=FandomMarkers.GetBytes("https://assets.tarkov.dev/"+id+"-icon.jpg");System.IO.Directory.CreateDirectory(IconDir);System.IO.File.WriteAllBytes(file,bytes);any=true;}catch{}}
    if(any&&IsHandleCreated)try{BeginInvoke((Action)(()=>{list.Invalidate();found.Invalidate();}));}catch{}});
   t.IsBackground=true;t.Priority=System.Threading.ThreadPriority.BelowNormal;t.Start();
  }
  Image Icon(string id){
   Image img;if(icons.TryGetValue(id,out img))return img;
   var dir=System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"ModeSwitch","icons");var file=System.IO.Path.Combine(dir,id+".jpg");
   if(System.IO.File.Exists(file)){try{using(var s=new System.IO.MemoryStream(System.IO.File.ReadAllBytes(file)))img=new Bitmap(Image.FromStream(s));icons[id]=img;return img;}catch{}}
   if(fetching.Add(id)){var t=new System.Threading.Thread(()=>{try{var bytes=FandomMarkers.GetBytes("https://assets.tarkov.dev/"+id+"-icon.jpg");System.IO.Directory.CreateDirectory(dir);System.IO.File.WriteAllBytes(file,bytes);
     if(IsHandleCreated)BeginInvoke((Action)(()=>{list.Invalidate();found.Invalidate();}));}catch{}});t.IsBackground=true;t.Start();}
   return null;
  }
  string Where(Entry e){
   int n;if(e.Maps.TryGetValue(mapId,out n))return I18n.Format("이 맵 {0}곳",n);
   var names=new List<string>();foreach(var m in e.Maps.Keys){string s;names.Add(mapNames.TryGetValue(m,out s)&&s.Length>0?s:m);}return string.Join(" · ",names);
  }
  void DrawRow(object sender,DrawItemEventArgs e){
   if(e.Index<0)return;var box=(ListBox)sender;var item=(Entry)box.Items[e.Index];var r=e.Bounds;bool inList=box==list,added=mine.Contains(item.Id),here=item.Maps.ContainsKey(mapId);
   bool hot=inList?item.Id==Hover:(e.State&DrawItemState.Selected)!=0;
   using(var bg=new SolidBrush(hot?Theme.Hover:Well))e.Graphics.FillRectangle(bg,r);
   using(var sep=new Pen(Line))e.Graphics.DrawLine(sep,r.Left,r.Bottom-1,r.Right,r.Bottom-1);
   int pad=(int)(6*K),size=r.Height-2*pad;var img=Icon(item.Id);var iconRect=new Rectangle(r.Left+pad,r.Top+pad,size,size);
   using(var frame=new SolidBrush(Theme.Bg))e.Graphics.FillRectangle(frame,iconRect);
   if(img!=null){e.Graphics.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;e.Graphics.DrawImage(img,iconRect);}
   int tx=iconRect.Right+pad,right=r.Right-(inList?RemoveWidth:pad);var nameRect=new Rectangle(tx,r.Top+pad-1,right-tx,inList?size/2+2:size);
   using(var bold=new Font("Malgun Gothic",9f,FontStyle.Bold))TextRenderer.DrawText(e.Graphics,Short(item.Name),bold,nameRect,Ink,TextFormatFlags.Left|(inList?TextFormatFlags.Top:TextFormatFlags.VerticalCenter)|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
   if(inList){var sub=(item.Card?I18n.Text("키카드")+" · ":"")+Where(item);using(var small=new Font("Malgun Gothic",8.5f))TextRenderer.DrawText(e.Graphics,sub,small,new Rectangle(tx,r.Top+r.Height/2,right-tx,r.Height/2-pad+2),here?Accent:Muted,TextFormatFlags.Left|TextFormatFlags.Top|TextFormatFlags.EndEllipsis|TextFormatFlags.NoPrefix);
    using(var x=new Font("Segoe UI",10f))TextRenderer.DrawText(e.Graphics,"✕",x,new Rectangle(r.Right-RemoveWidth,r.Top,RemoveWidth,r.Height),hot?Ink:Muted,TextFormatFlags.HorizontalCenter|TextFormatFlags.VerticalCenter);}
   else if(added)using(var small=new Font("Malgun Gothic",8.5f))TextRenderer.DrawText(e.Graphics,I18n.Text("✓ 추가됨"),small,new Rectangle(right-90,r.Top,90,r.Height),Accent,TextFormatFlags.Right|TextFormatFlags.VerticalCenter);
  }
 }
 // In-game map panel: borderless graphite header and cobalt actions, centred on
 // the game. Closing (M / Esc / ×) hands focus straight back to the game window.
 // Flea market sales in the game's push-notifications log: a trader message whose systemData has soldItem (item template)
 // and itemCount, its items.data the money paid (StackObjectsCount of roubles, dollars or euros). Only messages written
 // after the helper first looks are reported (a log already open is read from its end); a new log is read from its start.
 public static class FleaSales {
  public class Sale{public string Item="";public int Count=1;public long Amount;public string Currency="₽";}
  static string file="";static long offset;static string tail="";static readonly HashSet<string> seen=new HashSet<string>();
  static readonly Dictionary<string,string> money=new Dictionary<string,string>{{"5449016a4bdc2d6f028b456f","₽"},{"5696686a4bdc2da3298b456a","$"},{"569668774bdc2da2298b4568","€"}};
  static string Newest(string logs){try{System.IO.DirectoryInfo best=null;foreach(var d in new System.IO.DirectoryInfo(logs).GetDirectories("log_*"))if(best==null||d.CreationTimeUtc>best.CreationTimeUtc)best=d;if(best==null)return null;
    System.IO.FileInfo f=null;foreach(var x in best.GetFiles("*push-notifications*.log"))if(f==null||x.LastWriteTimeUtc>f.LastWriteTimeUtc)f=x;return f==null?null:f.FullName;}catch{return null;}}
  public static List<Sale> Poll(string logs){var found=new List<Sale>();try{
    var f=Newest(logs);if(f==null)return found;long len=new System.IO.FileInfo(f).Length;
    if(f!=file){bool first=file.Length==0;file=f;offset=first?len:0;tail="";}if(len<offset){offset=0;tail="";}if(len==offset)return found;
    string chunk;using(var fs=new System.IO.FileStream(f,System.IO.FileMode.Open,System.IO.FileAccess.Read,System.IO.FileShare.ReadWrite|System.IO.FileShare.Delete)){fs.Seek(offset,System.IO.SeekOrigin.Begin);var b=new byte[len-offset];int n=0;while(n<b.Length){int r=fs.Read(b,n,b.Length-n);if(r<=0)break;n+=r;}chunk=System.Text.Encoding.UTF8.GetString(b,0,n);offset+=n;}
    var text=tail+chunk;int last=text.LastIndexOf("Got notification |",StringComparison.Ordinal);var done=last>0?text.Substring(0,last):"";tail=last>=0?text.Substring(last):text;if(tail.Length>400000)tail="";
    foreach(System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(done,"\"soldItem\"\\s*:\\s*\"([0-9a-f]{24})\"")){
     var after=done.Substring(m.Index,Math.Min(2000,done.Length-m.Index));var before=done.Substring(Math.Max(0,m.Index-800),Math.Min(800,m.Index));
     var cnt=System.Text.RegularExpressions.Regex.Match(after,"\"itemCount\"\\s*:\\s*(\\d+)");var tpl=System.Text.RegularExpressions.Regex.Match(after,"\"_tpl\"\\s*:\\s*\"([0-9a-f]{24})\"");var amt=System.Text.RegularExpressions.Regex.Match(after,"\"StackObjectsCount\"\\s*:\\s*(\\d+)");
     var dt=System.Text.RegularExpressions.Regex.Matches(before,"\"dt\"\\s*:\\s*(\\d+)");string cur;if(!tpl.Success||!amt.Success||!money.TryGetValue(tpl.Groups[1].Value,out cur))continue;
     var key=(dt.Count>0?dt[dt.Count-1].Groups[1].Value:"")+"|"+m.Groups[1].Value+"|"+amt.Groups[1].Value;if(!seen.Add(key))continue;
     found.Add(new Sale{Item=m.Groups[1].Value,Count=cnt.Success?int.Parse(cnt.Groups[1].Value):1,Amount=long.Parse(amt.Groups[1].Value),Currency=cur});}
   }catch{}return found;}
 }
 public class PinEventArgs:EventArgs{public readonly Pin Pin;public PinEventArgs(Pin p){Pin=p;}}
 // Fandom wiki guide of a quest: the text of its Guide section and its pictures (URLs and captions). The page comes from the
 // wiki's API when a quest card opens (kept a week in %LOCALAPPDATA%\ModeSwitch\quest-wiki). The pictures are not downloaded
 // here: the wiki's image server answers non-browser clients with a bot check. The browser card (HtmlQuestCard, Edge
 // WebView2) shows them as any browser does; without it the card opens the page. Content: Escape from Tarkov Wiki (Fandom), CC BY-SA.
 public static class QuestWiki {
  public class Picture{public string Thumb="",Large="",Caption="";public int W,H;}
  public class Guide{public string Text="",Dialogue="";public List<Picture> Pictures=new List<Picture>();public string Error="";public bool Translated;}
  // BlueAsh's server (companion/relay.txt, set by the helper): the guide text translated once into the UI language
  // (relay/guide.js), kept a day per quest in quest-wiki\<id>.<lang>.txt. Only when it was made from this same English text.
  public static string Relay="";
  static void Translate(string store,string questId,Guide g){
   if(I18n.Language=="en"||Relay.Length==0)return;
   var t=g.Text.Length>0?Fetch(store,questId):"";if(t.Length>0){g.Text=t;g.Translated=true;}
   // The briefing (wiki Dialogue, English) of quests whose game text the locale files lack: translated as "<id>-b".
   var d=g.Dialogue.Length>0?Fetch(store,questId+"-b"):"";if(d.Length>0)g.Dialogue=d;}
  static string Fetch(string store,string key){
   try{var file=System.IO.Path.Combine(store,"quest-wiki",key+"."+I18n.Language+".json");string body=null;
    if(System.IO.File.Exists(file)&&DateTime.UtcNow-System.IO.File.GetLastWriteTimeUtc(file)<TimeSpan.FromDays(1))body=System.IO.File.ReadAllText(file);
    if(body==null){try{body=FandomMarkers.Get(Relay+"/quest/guide?id="+key+"&lang="+Uri.EscapeDataString(I18n.Language));System.IO.File.WriteAllText(file,body);}catch{body="{}";}}
    var d=js.DeserializeObject(body) as IDictionary;var t=d!=null&&d.Contains("text")?Convert.ToString(d["text"]).Trim():"";return t.Length>6000?t.Substring(0,6000).TrimEnd()+"…":t;}catch{return "";}}
  static string Sized(string url,int width){var u=System.Text.RegularExpressions.Regex.Replace(url,"/scale-to-width-down/\\d+","");int at=u.IndexOf("/revision/latest",StringComparison.Ordinal);return at<0?u:u.Insert(at+16,"/scale-to-width-down/"+width);}
  static readonly JavaScriptSerializer js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};
  static string Clean(string html){var t=System.Text.RegularExpressions.Regex.Replace(html,"<br\\s*/?>","\n");t=System.Text.RegularExpressions.Regex.Replace(t,"<[^>]+>","");t=System.Net.WebUtility.HtmlDecode(t);return System.Text.RegularExpressions.Regex.Replace(t,"[ \\t]+"," ").Trim();}
  // Blocking: called off the UI thread.
  public static Guide Load(string store,string questId,string wiki){
   var g=new Guide();try{
    var html=Page(System.IO.Path.Combine(store,"quest-wiki"),questId,wiki);if(html==null){g.Error="no page";return g;}
    // The trader's briefing: the first quote of the Dialogue section (the quest's own description, in English).
    int dia=html.IndexOf("id=\"Dialogue\"",StringComparison.Ordinal);if(dia>=0){var q=System.Text.RegularExpressions.Regex.Match(html.Substring(dia),"class=\"custom-quote(?:&#95;|_){2}text\"[^>]*>(.*?)</p>",System.Text.RegularExpressions.RegexOptions.Singleline);if(q.Success)g.Dialogue=Clean(q.Groups[1].Value);}
    int start=html.IndexOf("id=\"Guide\"",StringComparison.Ordinal);if(start<0){g.Error=g.Dialogue.Length>0?"":"no guide";Translate(store,questId,g);return g;}
    int end=html.IndexOf("<h2",start,StringComparison.Ordinal),nav=html.IndexOf("navbox",start,StringComparison.Ordinal);if(end<0||(nav>=0&&nav<end))end=nav;if(end<0)end=html.Length;var part=html.Substring(start,end-start);
    g.Pictures=Pictures(part,12);
    // The whole guide text: paragraphs and list items (tables of quest items left out), up to 4000 characters.
    var text=new List<string>();foreach(System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(System.Text.RegularExpressions.Regex.Replace(part,"<table.*?</table>|<ul class=\"gallery.*?</ul>","",System.Text.RegularExpressions.RegexOptions.Singleline),"<(p|li)(?:\\s[^>]*)?>(.*?)</\\1>",System.Text.RegularExpressions.RegexOptions.Singleline)){var t=Clean(m.Groups[2].Value);if(t.Length>0)text.Add((m.Groups[1].Value=="li"?"• ":"")+t);}
    g.Text=string.Join("\n",text);if(g.Text.Length>4000)g.Text=g.Text.Substring(0,4000).TrimEnd()+"…";
    Translate(store,questId,g);
   }catch(Exception e){g.Error=e.Message;}return g;
  }
  // Pictures: the wiki's file links in a page part (full image) with their thumbnail size; small item icons (under 120 px) are left out.
  internal static List<Picture> Pictures(string part,int max){var list=new List<Picture>();
    foreach(System.Text.RegularExpressions.Match m in System.Text.RegularExpressions.Regex.Matches(part,"<a href=\"(https://static\\.wikia\\.nocookie\\.net/[^\"]+)\"[^>]*class=\"mw-file-description[^\"]*\"([^>]*)>\\s*<img ([^>]*)>")){
     var img=m.Groups[3].Value;var w=System.Text.RegularExpressions.Regex.Match(img,"width=\"(\\d+)\"");var h=System.Text.RegularExpressions.Regex.Match(img,"height=\"(\\d+)\"");if(w.Success&&int.Parse(w.Groups[1].Value)<120)continue;
     var t2=System.Text.RegularExpressions.Regex.Match(m.Groups[2].Value,"title=\"([^\"]*)\"");var alt=System.Text.RegularExpressions.Regex.Match(img,"alt=\"([^\"]*)\"");
     var cap=System.Net.WebUtility.HtmlDecode(t2.Success?t2.Groups[1].Value:alt.Success?alt.Groups[1].Value:"").Trim();if(System.Text.RegularExpressions.Regex.IsMatch(cap,"\\.(png|jpe?g|gif|webp)$",System.Text.RegularExpressions.RegexOptions.IgnoreCase))cap="";
     var full=System.Net.WebUtility.HtmlDecode(m.Groups[1].Value);bool dup=false;foreach(var e in list)if(e.Large==Sized(full,1600))dup=true;if(dup)continue;
     list.Add(new Picture{Thumb=Sized(full,480),Large=Sized(full,1600),Caption=cap,W=w.Success?int.Parse(w.Groups[1].Value):0,H=h.Success?int.Parse(h.Groups[1].Value):0});if(list.Count>=max)break;}
    return list;}
  // A page's section by its heading id, up to the next h2 (empty when the page has none of the ids).
  internal static string Section(string html,params string[] ids){foreach(var id in ids){int start=html.IndexOf("id=\""+id+"\"",StringComparison.Ordinal);if(start<0)continue;int end=html.IndexOf("<h2",start,StringComparison.Ordinal);return html.Substring(start,(end<0?html.Length:end)-start);}return "";}
  // The page's parsed HTML from the wiki's API, cached a week under dir\name.html. Blocking.
  internal static string Page(string dir,string name,string wiki){
   int at=wiki.LastIndexOf("/wiki/",StringComparison.Ordinal);if(at<0)return null;var title=Uri.UnescapeDataString(wiki.Substring(at+6));
   System.IO.Directory.CreateDirectory(dir);var cache=System.IO.Path.Combine(dir,name+".html");
   if(System.IO.File.Exists(cache)&&DateTime.UtcNow-System.IO.File.GetLastWriteTimeUtc(cache)<TimeSpan.FromDays(7))return System.IO.File.ReadAllText(cache);
   var root=(IDictionary)js.DeserializeObject(FandomMarkers.Get("https://escapefromtarkov.fandom.com/api.php?action=parse&format=json&formatversion=2&prop=text&redirects=1&page="+Uri.EscapeDataString(title)));
   var html=Convert.ToString(((IDictionary)root["parse"])["text"]);System.IO.File.WriteAllText(cache,html);return html;}
 }
 // Pictures of where a key's lock is and what is behind it: the Lock Location and Behind the Lock sections of the key's
 // wiki page (kept a week in %LOCALAPPDATA%\ModeSwitch\key-wiki). Shown by HtmlKeyTip; the pictures load in the browser.
 public static class KeyWiki {
  public class Result{public List<QuestWiki.Picture> Lock=new List<QuestWiki.Picture>(),Inside=new List<QuestWiki.Picture>();public bool Failed;}
  static readonly Dictionary<string,Result> known=new Dictionary<string,Result>();
  public static Result Load(string store,string id,string wiki){
   lock(known){Result r;if(known.TryGetValue(id,out r))return r;}
   var res=new Result();try{var html=QuestWiki.Page(System.IO.Path.Combine(store,"key-wiki"),id,wiki);if(html==null)res.Failed=true;
    else{res.Lock=QuestWiki.Pictures(QuestWiki.Section(html,"Lock_Location","Lock_Locations","Lock_location","Lock_locations"),4);res.Inside=QuestWiki.Pictures(QuestWiki.Section(html,"Behind_the_Lock","Behind_the_lock"),4);}}
   catch{res.Failed=true;}
   if(!res.Failed)lock(known)known[id]=res;return res;}
 }
 // Card over the map's right side: quest name, trader and level, every objective (this map's in blue), the wiki guide text,
 // and a button to the wiki page's guide (its pictures). ✕ or Esc closes. Several quests on one marker are stacked.
 public class QuestCard:Panel {
  readonly FlowLayoutPanel body=new FlowLayoutPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.TopDown,WrapContents=false,AutoScroll=true};
  readonly Label close=new Label{Text="✕",TextAlign=ContentAlignment.MiddleCenter,Cursor=Cursors.Hand};
  int generation;
  // Set by MapWindow while WebView2 is missing: offers Microsoft's installer (pictures then show on the map).
  public Action<Button> InstallBrowser;public Func<IDictionary,List<Dictionary<string,object>>> NeededKeys;
  public QuestCard(){BackColor=Theme.Panel;ForeColor=Theme.Ink;Padding=new Padding(1);body.BackColor=Theme.Panel;
   close.ForeColor=Theme.Muted;close.BackColor=Theme.Panel;close.Click+=delegate{Hide();};close.MouseEnter+=delegate{close.ForeColor=Theme.Ink;close.BackColor=Theme.Hover;};close.MouseLeave+=delegate{close.ForeColor=Theme.Muted;close.BackColor=Theme.Panel;};
   Controls.Add(close);Controls.Add(body);}
  float K{get{return DeviceDpi/96f;}}
  int Inner{get{return Math.Max(80,body.ClientSize.Width-(int)(28*K));}}
  protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);using(var edge=new Pen(Theme.Border))e.Graphics.DrawRectangle(edge,0,0,Width-1,Height-1);using(var bar=new SolidBrush(Theme.Accent))e.Graphics.FillRectangle(bar,0,0,(int)(3*K),Height);}
  protected override void OnLayout(LayoutEventArgs e){base.OnLayout(e);int s=(int)(34*K);close.SetBounds(Width-s-4,4,s,s);if(close.Font.Size!=11f)close.Font=new Font(I18n.FontName,11f);close.BringToFront();body.Padding=new Padding((int)(14*K),(int)(10*K),(int)(8*K),(int)(10*K));}
  Label Line(string text,float size,FontStyle style,Color color,int top=0){var l=new Label{Text=text,AutoSize=true,ForeColor=color,BackColor=Theme.Panel,Font=new Font(I18n.FontName,size,style),Margin=new Padding(0,top,0,(int)(2*K)),UseMnemonic=false};l.MaximumSize=new Size(Inner,0);return l;}
  // quests: (quest id, quest data) for each quest on the marker; traders: their trader names; map: this map's id.
  public void Show(List<KeyValuePair<string,IDictionary>> quests,List<string> traders,string map,string store){
   generation++;int gen=generation;body.SuspendLayout();foreach(Control c in body.Controls)c.Dispose();body.Controls.Clear();
   for(int qi=0;qi<quests.Count;qi++){var q=quests[qi].Value;var name=Convert.ToString(q["name"]);
    body.Controls.Add(Line(name,12.5f,FontStyle.Bold,Theme.Ink,qi>0?(int)(18*K):(int)(2*K)));
    var meta=new List<string>();if(traders[qi].Length>0)meta.Add(traders[qi]);int level=q.Contains("level")?Convert.ToInt32(q["level"]):0;if(level>1)meta.Add(I18n.Format("레벨 {0} 이상",level));
    if(meta.Count>0)body.Controls.Add(Line(string.Join(" · ",meta),9.5f,FontStyle.Regular,Theme.Muted));
    body.Controls.Add(Line(I18n.Text("목표"),10f,FontStyle.Bold,Theme.Ink,(int)(10*K)));
    foreach(var o in (q.Contains("steps")?(IEnumerable)q["steps"]:new object[0])){var st=o as IDictionary;if(st==null)continue;bool here=false;foreach(var m in (IEnumerable)st["maps"])if(Convert.ToString(m)==map)here=true;
     var t="• "+Convert.ToString(st["text"])+(Convert.ToBoolean(st["optional"])?" "+I18n.Text("(선택)"):"");body.Controls.Add(Line(t,9.5f,here?FontStyle.Bold:FontStyle.Regular,here?Theme.AccentText:Theme.Muted));}
    if(NeededKeys!=null){var ks=NeededKeys(q);if(ks.Count>0){body.Controls.Add(Line(I18n.Text("필요한 열쇠"),10f,FontStyle.Bold,Theme.Ink,(int)(10*K)));
     foreach(var k in ks)body.Controls.Add(Line("• "+Convert.ToString(k["name"])+(Convert.ToBoolean(k["optional"])?" "+I18n.Text("(선택)"):"")+(Convert.ToString(k["map"]).Length>0?" · "+k["map"]:"")+(Convert.ToBoolean(k["mine"])?"  ✓ "+I18n.Text("내 열쇠에 있음"):""),9.5f,FontStyle.Regular,Convert.ToBoolean(k["mine"])?Theme.AccentText:Theme.Muted));}}
    var wiki=q.Contains("wiki")?Convert.ToString(q["wiki"]):"";if(wiki.Length==0)continue;
    var status=Line(I18n.Text("위키 안내를 불러오는 중…"),9f,FontStyle.Regular,Theme.Muted,(int)(10*K));body.Controls.Add(status);
    var open=new ThemeButton{Text=I18n.Text("위키에서 사진 보기"),AutoSize=false,FlatStyle=FlatStyle.Flat,BackColor=Theme.Field,ForeColor=Theme.Ink,Cursor=Cursors.Hand,TabStop=false,Font=new Font(I18n.FontName,9.5f),Margin=new Padding(0,(int)(8*K),0,0)};
    open.FlatAppearance.BorderColor=Theme.Border;open.Size=new Size(Math.Min(Inner,(int)(220*K)),(int)(32*K));var page=wiki+"#Guide";open.Click+=delegate{try{System.Diagnostics.Process.Start(page);}catch{}};body.Controls.Add(open);
    if(InstallBrowser!=null&&qi==0){var install=new ThemeButton{Text=I18n.Text("지도에서 사진 보기 설치 (Microsoft WebView2)"),AutoSize=false,FlatStyle=FlatStyle.Flat,BackColor=Theme.Accent,ForeColor=Theme.Ink,Cursor=Cursors.Hand,TabStop=false,Font=new Font(I18n.FontName,9.5f),Margin=new Padding(0,(int)(6*K),0,0)};
     install.FlatAppearance.BorderColor=Theme.Accent;install.Size=new Size(Inner,(int)(32*K));install.Click+=delegate{install.Enabled=false;install.Text=I18n.Text("설치 프로그램을 받는 중…");InstallBrowser(install);};body.Controls.Add(install);}
    var id=quests[qi].Key;
    System.Threading.ThreadPool.QueueUserWorkItem(_=>{var guide=QuestWiki.Load(store,id,wiki);
     try{if(!IsHandleCreated||IsDisposed)return;BeginInvoke((Action)(()=>{if(gen!=generation)return;
      if(guide.Text.Length==0&&guide.Dialogue.Length==0&&guide.Error.Length>0){status.Text=I18n.Text("위키를 불러오지 못했습니다.");return;}
      var parts=new List<string>();var own=q.Contains("brief")?Convert.ToString(q["brief"]):"";if(own.Length>0)parts.Add(I18n.Text("퀘스트 설명")+"\n"+own);else if(guide.Dialogue.Length>0)parts.Add(I18n.Text("퀘스트 설명")+"\n"+guide.Dialogue);
      if(guide.Text.Length>0)parts.Add(I18n.Text("위키 안내")+"\n"+guide.Text);status.Text=string.Join("\n\n",parts);
      open.Text=guide.Pictures.Count>0?I18n.Format("위키에서 사진 {0}장 보기",guide.Pictures.Count):I18n.Text("위키에서 보기");}));}catch{}});}
   body.Controls.Add(Line(I18n.Text("출처: Escape from Tarkov Wiki (Fandom) · CC BY-SA"),8f,FontStyle.Regular,Theme.Muted,(int)(14*K)));
   body.ResumeLayout();body.AutoScrollPosition=Point.Empty;Visible=true;BringToFront();}
 }
 // The quest card as a page in Edge WebView2 (part of Windows 11; the WebView2 SDK DLLs ship with the helper): the same
 // content as QuestCard plus the wiki pictures, which the browser loads like any browser. A picture click turns the card
 // into a full-map viewer (large picture, caption, ✕; Esc or a click beside it goes back). The page is built here from
 // the quest data and the wiki guide; it may load only the wiki's image server, navigation elsewhere is blocked.
 // Messages from the page: close, esc, key:<Tab|w|a|s|d>, open (viewer shown), shut (viewer closed), wiki:<i>, h:<px>.
 // WebView2 types stay inside this class's methods, so a missing DLL only disables it (MapWindow falls back to QuestCard).
 // Microsoft's WebView2 Runtime bootstrapper (go.microsoft.com, about 2 MB), run on the user's click when the runtime is
 // missing (rare: Windows 11 has it, Windows 10 gets it with Edge). Run only when signed by Microsoft.
 public static class WebViewSetup {
  public static void Run(Button status){
   System.Threading.ThreadPool.QueueUserWorkItem(_=>{string message;try{
    var path=System.IO.Path.Combine(System.IO.Path.GetTempPath(),"MicrosoftEdgeWebview2Setup.exe");System.IO.File.WriteAllBytes(path,FandomMarkers.GetBytes("https://go.microsoft.com/fwlink/p/?LinkId=2124703"));
    var cert=new System.Security.Cryptography.X509Certificates.X509Certificate2(System.Security.Cryptography.X509Certificates.X509Certificate.CreateFromSignedFile(path));
    if(cert.Subject.IndexOf("O=Microsoft Corporation",StringComparison.Ordinal)<0||!new System.Security.Cryptography.X509Certificates.X509Chain().Build(cert))throw new Exception("signature");
    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(path,"/install"){UseShellExecute=true});message=I18n.Text("설치 창을 따라 설치한 뒤 퀘스트 마크를 다시 누르세요.");}
    catch{message=I18n.Text("설치 프로그램을 받지 못했습니다.");}
    try{status.BeginInvoke((Action)(()=>{status.Text=message;}));}catch{}});
  }
 }
 public sealed class HtmlQuestCard {
  readonly Control view;readonly MapWindow owner;readonly string store;bool ready,loaded,expanded,starting;string html;readonly List<string> scripts=new List<string>();List<string> wikis=new List<string>();int generation;int contentHeight;Rectangle card;
  public bool Failed;public bool Visible{get{return view.Visible;}}
  static string Hex(Color c){return "#"+c.R.ToString("x2")+c.G.ToString("x2")+c.B.ToString("x2");}
  static readonly JavaScriptSerializer js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};
  static string Json(object o){return js.Serialize(o).Replace("</","<\\/");}
  public static bool Runtime(){try{return !string.IsNullOrEmpty(Microsoft.Web.WebView2.Core.CoreWebView2Environment.GetAvailableBrowserVersionString());}catch{return false;}}
  public HtmlQuestCard(MapWindow w,string store){owner=w;this.store=store;
   var v=new Microsoft.Web.WebView2.WinForms.WebView2{Visible=false,DefaultBackgroundColor=Theme.Panel,Anchor=AnchorStyles.Top|AnchorStyles.Right};view=v;
   v.CreationProperties=new Microsoft.Web.WebView2.WinForms.CoreWebView2CreationProperties{UserDataFolder=System.IO.Path.Combine(store,"webview2")};
   v.CoreWebView2InitializationCompleted+=(s,e)=>{if(!e.IsSuccess){Failed=true;view.Visible=false;owner.CardFailed();return;}
    var c=v.CoreWebView2;c.Settings.AreDevToolsEnabled=false;c.Settings.AreDefaultContextMenusEnabled=false;c.Settings.IsZoomControlEnabled=false;c.Settings.IsStatusBarEnabled=false;c.Settings.AreDefaultScriptDialogsEnabled=false;c.Settings.IsPasswordAutosaveEnabled=false;c.Settings.IsGeneralAutofillEnabled=false;
    c.NavigationStarting+=(s2,n)=>{if(!(n.Uri.StartsWith("data:",StringComparison.Ordinal)||n.Uri.StartsWith("about:",StringComparison.Ordinal)))n.Cancel=true;};
    c.NewWindowRequested+=(s2,n)=>{n.Handled=true;};
    c.NavigationCompleted+=(s2,n)=>{loaded=true;foreach(var sc in scripts)c.ExecuteScriptAsync(sc);scripts.Clear();};
    c.WebMessageReceived+=(s2,m)=>{string msg;try{msg=m.TryGetWebMessageAsString();}catch{return;}Message(msg);};
    ready=true;if(html!=null){loaded=false;c.NavigateToString(html);}};}
  public Control View{get{return view;}}
  // Start the browser ahead (helper start, a while after the game launched), hidden, so the first card opens at once.
  public void Warm(){if(ready||starting)return;starting=true;var h=view.Handle;try{((Microsoft.Web.WebView2.WinForms.WebView2)view).EnsureCoreWebView2Async(null);}catch{Failed=true;}}
  public void Hide(){Collapse();view.Visible=false;}
  void Run(string script){var v=(Microsoft.Web.WebView2.WinForms.WebView2)view;if(ready&&loaded)v.CoreWebView2.ExecuteScriptAsync(script);else scripts.Add(script);}
  void Collapse(){if(!expanded)return;expanded=false;view.Bounds=card;}
  // Always the full height beside the map (a short quest no longer makes a short card); long ones scroll inside.
  void Fit(){if(expanded)return;view.Bounds=card;}
  void Message(string m){
   if(m=="close"){Hide();owner.Canvas.Focus();}
   else if(m=="esc"){Hide();owner.Canvas.Focus();}
   else if(m.StartsWith("key:",StringComparison.Ordinal)){owner.HideMap();}
   else if(m=="open"){if(!expanded){expanded=true;var c=owner.Canvas.ClientRectangle;view.Bounds=c;}}
   else if(m=="shut"){expanded=false;Fit();}
   else if(m.StartsWith("h:",StringComparison.Ordinal)){int h;if(int.TryParse(m.Substring(2),out h)){contentHeight=h;Fit();}}
   else if(m.StartsWith("wiki:",StringComparison.Ordinal)){int i;if(int.TryParse(m.Substring(5),out i)&&i>=0&&i<wikis.Count){try{System.Diagnostics.Process.Start(wikis[i]+"#Guide");}catch{}}}
  }
  public void Show(List<KeyValuePair<string,IDictionary>> quests,List<string> traders,string map,Rectangle bounds){
   generation++;int gen=generation;scripts.Clear();expanded=false;card=bounds;contentHeight=0;view.Bounds=bounds;wikis=new List<string>();
   var data=new List<object>();for(int i=0;i<quests.Count;i++){var q=quests[i].Value;var steps=new List<object>();
    foreach(var o in (q.Contains("steps")?(IEnumerable)q["steps"]:new object[0])){var st=o as IDictionary;if(st==null)continue;bool here=false;foreach(var m in (IEnumerable)st["maps"])if(Convert.ToString(m)==map)here=true;steps.Add(new Dictionary<string,object>{{"text",Convert.ToString(st["text"])},{"optional",Convert.ToBoolean(st["optional"])},{"here",here}});}
    int level=q.Contains("level")?Convert.ToInt32(q["level"]):0;var wiki=q.Contains("wiki")?Convert.ToString(q["wiki"]):"";wikis.Add(wiki);
    List<Dictionary<string,object>> keys;try{keys=Quests.NeededKeys(q,owner.QuestLogs,owner.Store,owner.MyKeys);}catch{keys=new List<Dictionary<string,object>>();}
    data.Add(new Dictionary<string,object>{{"keys",keys},{"name",Convert.ToString(q["name"])},{"meta",string.Join(" · ",new List<string>(new[]{traders[i],level>1?I18n.Format("레벨 {0} 이상",level):""}).FindAll(x=>x.Length>0))},{"steps",steps},{"wiki",wiki.Length>0},{"brief",q.Contains("brief")?Convert.ToString(q["brief"]):""}});}
   var text=new Dictionary<string,object>{{"goals",I18n.Text("목표")},{"optional",I18n.Text("(선택)")},{"loading",I18n.Text("위키 안내를 불러오는 중…")},{"failed",I18n.Text("위키를 불러오지 못했습니다.")},
    {"guide",I18n.Text("위키 안내")},{"brief",I18n.Text("퀘스트 설명")},{"own",I18n.Text("퀘스트 설명")},{"auto",I18n.Text("위키 안내")},{"keys",I18n.Text("필요한 열쇠")},{"mine",I18n.Text("내 열쇠에 있음")},{"open",I18n.Text("위키에서 보기")},{"hint",I18n.Text("사진을 누르면 크게 봅니다.")},{"source",I18n.Text("출처: Escape from Tarkov Wiki (Fandom) · CC BY-SA")},{"close",I18n.Text("닫기 (Esc)")}};
   html=Page(Json(data),Json(text));var v=(Microsoft.Web.WebView2.WinForms.WebView2)view;view.Visible=true;view.BringToFront();
   if(ready){loaded=false;v.CoreWebView2.NavigateToString(html);}else if(!starting){starting=true;try{v.EnsureCoreWebView2Async(null);}catch{Failed=true;owner.CardFailed();return;}}
   for(int i=0;i<quests.Count;i++){if(wikis[i].Length==0)continue;var id=quests[i].Key;var url=wikis[i];int index=i;
    System.Threading.ThreadPool.QueueUserWorkItem(_=>{var g=QuestWiki.Load(store,id,url);var pics=new List<object>();foreach(var p in g.Pictures)pics.Add(new Dictionary<string,object>{{"thumb",p.Thumb},{"large",p.Large},{"caption",p.Caption},{"w",p.W},{"h",p.H}});
     var payload=Json(new Dictionary<string,object>{{"text",g.Text},{"translated",g.Translated},{"dialogue",g.Dialogue},{"error",g.Error.Length>0&&g.Text.Length==0&&g.Dialogue.Length==0&&g.Pictures.Count==0},{"pictures",pics}});
     try{view.BeginInvoke((Action)(()=>{if(gen==generation)Run("guide("+index+","+payload+")");}));}catch{}});}
  }
  string Page(string data,string text){
   var css=@"*{box-sizing:border-box}html,body{margin:0;background:transparent;color:INK;font:14px/1.45 'FONT','Segoe UI',sans-serif;overflow:hidden}
#card{position:absolute;top:0;right:0;width:100%;height:100vh;overflow-y:auto;background:PANEL;border:1px solid BORDER;border-left:3px solid ACCENT;padding:12px 14px 14px 16px;scrollbar-color:BORDER PANEL}
#x{position:sticky;top:0;float:right;margin:-6px -8px 0 6px;width:32px;height:32px;border:0;border-radius:6px;background:transparent;color:MUTED;font-size:16px;cursor:pointer}#x:hover{background:HOVER;color:INK}
h1{font-size:17px;margin:2px 0 2px}h2{font-size:13.5px;margin:12px 0 4px}.meta{color:MUTED;font-size:13px}.q+.q{margin-top:20px;border-top:1px solid BORDER;padding-top:14px}
.step{color:MUTED;margin:3px 0}.step.here{color:ATEXT;font-weight:600}.guide{color:MUTED;font-size:12.5px;white-space:pre-line;margin-top:10px}.hint{color:MUTED;font-size:12px;margin:10px 0 4px}
.pic{margin:8px 0 0;cursor:zoom-in}.pic img{display:block;width:100%;height:auto;border-radius:6px;background:FIELD;min-height:40px}.pic div{color:MUTED;font-size:12px;margin-top:3px}
.key{display:flex;align-items:center;gap:8px;margin:5px 0;font-size:13.5px}.key img{width:30px;height:30px;border-radius:4px;background:FIELD;flex:none}.km{color:MUTED}.mine{color:ATEXT;font-weight:600}
a.wiki{display:inline-block;margin-top:10px;color:ATEXT;font-size:13px;cursor:pointer;text-decoration:underline}.src{color:MUTED;font-size:11.5px;margin-top:16px}
#big{display:none;position:fixed;inset:0;background:#0a0c10;z-index:5;cursor:zoom-out}#big img{position:absolute;inset:24px 24px 56px 24px;margin:auto;max-width:calc(100% - 48px);max-height:calc(100% - 80px);object-fit:contain;cursor:default}
#big .cap{position:absolute;left:24px;right:24px;bottom:18px;text-align:center;color:INK;font-size:15px}
#bx{position:absolute;top:14px;right:14px;width:42px;height:42px;border-radius:50%;border:1.5px solid rgba(255,255,255,.8);background:rgba(30,34,44,.86);color:#fff;font-size:20px;cursor:pointer}#bx:hover{background:ACCENT}";
   css=css.Replace("INK",Hex(Theme.Ink)).Replace("PANEL",Hex(Theme.Panel)).Replace("BORDER",Hex(Theme.Border)).Replace("ACCENT",Hex(Theme.Accent)).Replace("MUTED",Hex(Theme.Muted)).Replace("HOVER",Hex(Theme.Hover)).Replace("ATEXT",Hex(Theme.AccentText)).Replace("FIELD",Hex(Theme.Field)).Replace("FONT",I18n.FontName);
   var script=@"const D=DATA,T=TEXT,post=m=>window.chrome.webview.postMessage(m),card=document.getElementById('card'),big=document.getElementById('big'),bi=big.querySelector('img'),bc=big.querySelector('.cap');
const el=(t,c,x)=>{const e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e};
document.getElementById('x').title=T.close;document.getElementById('x').onclick=()=>post('close');
D.forEach((q,i)=>{const s=el('div','q');s.append(el('h1','',q.name));if(q.meta)s.append(el('div','meta',q.meta));s.append(el('h2','',T.goals));
 q.steps.forEach(t=>s.append(el('div','step'+(t.here?' here':''),'• '+t.text+(t.optional?' '+T.optional:''))));
 if(q.keys.length){s.append(el('h2','',T.keys));q.keys.forEach(k=>{const r=el('div','key'),im=document.createElement('img');im.src='https://assets.tarkov.dev/'+k.id+'-icon.jpg';im.onerror=()=>im.remove();r.append(im);const n=el('div','');n.append(el('b','',k.name));if(k.optional)n.append(el('span','km',' '+T.optional));if(k.map)n.append(el('span','km',' · '+k.map));if(k.mine)n.append(el('span','mine',' ✓ '+T.mine));r.append(n);s.append(r)})}
 if(q.brief)s.append(el('div','guide',T.own+'\n'+q.brief));
 if(q.wiki){const g=el('div','guide',T.loading);g.id='g'+i;s.append(g);const p=el('div','');p.id='p'+i;s.append(p);const a=el('a','wiki',T.open);a.onclick=()=>post('wiki:'+i);s.append(a);}
 card.append(s)});
card.append(el('div','src',T.source));
const size=()=>post('h:'+Math.ceil(card.scrollHeight+2));new ResizeObserver(size).observe(card);size();
function guide(i,g){const box=document.getElementById('g'+i),p=document.getElementById('p'+i);if(!box)return;
 const own=D[i]&&D[i].brief;box.textContent=g.error?(own?'':T.failed):[g.dialogue&&!own?T.brief+'\n'+g.dialogue:'',g.text?(g.translated?T.auto:T.guide)+'\n'+g.text:''].filter(Boolean).join('\n\n');if(g.pictures.length)p.append(el('div','hint',T.hint));
 g.pictures.forEach(x=>{const f=el('div','pic'),im=document.createElement('img');im.src=x.thumb;im.loading='lazy';if(x.w&&x.h)im.style.aspectRatio=x.w+'/'+x.h;im.onerror=()=>f.remove();f.append(im);if(x.caption)f.append(el('div','',x.caption));f.onclick=()=>show(x);p.append(f)});size()}
function show(x){bi.src=x.thumb;const l=new Image();l.onload=()=>{if(big.dataset.on)bi.src=x.large};l.src=x.large;bc.textContent=x.caption||'';big.dataset.on='1';big.style.display='block';post('open')}
function hide(){big.style.display='none';delete big.dataset.on;post('shut')}
big.onclick=e=>{if(e.target!==bi)hide()};document.getElementById('bx').onclick=e=>{e.stopPropagation();hide()};
document.addEventListener('keydown',e=>{if(e.key==='Escape'){e.preventDefault();if(big.dataset.on)hide();else post('esc')}else if(e.key==='Tab'||(!e.ctrlKey&&!e.altKey&&'wasdWASD'.includes(e.key)&&e.key.length===1)){e.preventDefault();post('key:'+e.key)}});";
   script=script.Replace("DATA",data).Replace("TEXT",text);
   return "<!doctype html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src https://static.wikia.nocookie.net https://assets.tarkov.dev; style-src 'unsafe-inline'; script-src 'unsafe-inline'\"><style>"+css+"</style></head><body><div id=\"card\"><button id=\"x\">✕</button></div><div id=\"big\"><button id=\"bx\">✕</button><img alt=\"\"><div class=\"cap\"></div></div><script>"+script+"</script></body></html>";
  }
 }
 // The key tip (hover a key in the key list) as a small Edge WebView2 page beside the list: the key's icon, name, where it
 // opens, its description, and the wiki's pictures of the locked door and of what is behind it (KeyWiki), so a key is
 // easier to tell from the others. Only shows content; the mouse never reaches it (moving onto it leaves the list row).
 // The page loads once; each hover sends its content by script and the page answers its height (h:<generation>:<px>).
 // Without WebView2 (or when it fails) MapWindow keeps the canvas's own text tip (MapCanvas.SetKeyTip).
 public sealed class HtmlKeyTip {
  readonly Control view;readonly MapWindow owner;readonly string store;bool ready,loaded,starting,navigated;readonly List<string> scripts=new List<string>();int generation,rowY,height;
  public bool Failed;
  static readonly JavaScriptSerializer js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};
  static string Json(object o){return js.Serialize(o).Replace("</","<\\/");}
  static string Hex(Color c){return "#"+c.R.ToString("x2")+c.G.ToString("x2")+c.B.ToString("x2");}
  public HtmlKeyTip(MapWindow w,string store){owner=w;this.store=store;
   var v=new Microsoft.Web.WebView2.WinForms.WebView2{Visible=false,DefaultBackgroundColor=Theme.Panel,TabStop=false};view=v;
   v.CreationProperties=new Microsoft.Web.WebView2.WinForms.CoreWebView2CreationProperties{UserDataFolder=System.IO.Path.Combine(store,"webview2")};
   v.CoreWebView2InitializationCompleted+=(s,e)=>{if(!e.IsSuccess){Failed=true;view.Visible=false;owner.KeyTipFailed();return;}
    var c=v.CoreWebView2;c.Settings.AreDevToolsEnabled=false;c.Settings.AreDefaultContextMenusEnabled=false;c.Settings.IsZoomControlEnabled=false;c.Settings.IsStatusBarEnabled=false;c.Settings.AreDefaultScriptDialogsEnabled=false;c.Settings.IsPasswordAutosaveEnabled=false;c.Settings.IsGeneralAutofillEnabled=false;
    c.NavigationStarting+=(s2,n)=>{if(!(n.Uri.StartsWith("data:",StringComparison.Ordinal)||n.Uri.StartsWith("about:",StringComparison.Ordinal)))n.Cancel=true;};
    c.NewWindowRequested+=(s2,n)=>{n.Handled=true;};
    c.NavigationCompleted+=(s2,n)=>{loaded=true;foreach(var sc in scripts)c.ExecuteScriptAsync(sc);scripts.Clear();};
    c.WebMessageReceived+=(s2,m)=>{string msg;try{msg=m.TryGetWebMessageAsString();}catch{return;}Message(msg);};
    ready=true;Navigate();};}
  // A hidden, unsized WebView2 does not load a page (it stays about:blank), so the page loads on the first hover, shown.
  void Navigate(){if(!ready||navigated||!view.Visible)return;navigated=true;loaded=false;((Microsoft.Web.WebView2.WinForms.WebView2)view).CoreWebView2.NavigateToString(Page());}
  // The whole canvas height (like the quest card) beside the key list; when a quest card is open there, just left of
  // the card (both stay readable). The pictures are in one column, shrunk together when they would not all fit.
  void Place(int h){var c=owner.Canvas;float k=K;int gap=(int)(8*k),right=c.Width-gap;var card=owner.CardBounds;if(!card.IsEmpty&&card.Left-gap>(int)(240*k))right=card.Left-gap;
   int w=Math.Min((int)(420*k),right-gap);if(w<80)return;int ht=Math.Max(40,c.Height-2*gap);
   view.Bounds=new Rectangle(Math.Max(gap,right-w),gap,w,ht);view.BringToFront();}
  public Control View{get{return view;}}
  public void Warm(){if(ready||starting)return;starting=true;var h=view.Handle;try{((Microsoft.Web.WebView2.WinForms.WebView2)view).EnsureCoreWebView2Async(null);}catch{Failed=true;}}
  void Run(string script){var v=(Microsoft.Web.WebView2.WinForms.WebView2)view;if(ready&&loaded)v.CoreWebView2.ExecuteScriptAsync(script);else scripts.Add(script);}
  public void Hide(){generation++;scripts.RemoveAll(x=>x.StartsWith("show(",StringComparison.Ordinal)||x.StartsWith("pics(",StringComparison.Ordinal));view.Visible=false;}
  float K{get{return owner.Canvas.DeviceDpi/96f;}}
  // y: the hovered row's top in canvas coordinates.
  public void Show(string id,string name,string places,string description,string wiki,bool card,int y){
   generation++;int gen=generation;rowY=y;Warm();
   // Shown at once at the last height (the page answers its own height a moment later).
   if(height==0)height=(int)(150*K);Place(height);view.Visible=true;view.BringToFront();Navigate();
   var data=new Dictionary<string,object>{{"id",id},{"name",name},{"places",places},{"description",description},{"card",card},{"wiki",wiki.Length>0}};
   Run("show("+gen+","+Json(data)+")");if(wiki.Length==0)return;
   System.Threading.ThreadPool.QueueUserWorkItem(_=>{var r=KeyWiki.Load(store,id,wiki);Func<List<QuestWiki.Picture>,List<object>> list=ps=>ps.ConvertAll(x=>(object)new Dictionary<string,object>{{"src",x.Thumb},{"caption",x.Caption},{"w",x.W},{"h",x.H}});
    var payload=Json(new Dictionary<string,object>{{"failed",r.Failed},{"lock",list(r.Lock)},{"inside",list(r.Inside)}});
    try{view.BeginInvoke((Action)(()=>{if(gen==generation)Run("pics("+gen+","+payload+")");}));}catch{}});}
  void Message(string m){
   var parts=m.Split(':');int gen,h;if(parts.Length!=3||parts[0]!="h"||!int.TryParse(parts[1],out gen)||!int.TryParse(parts[2],out h)||gen!=generation)return;
   height=h;Place(h);}
  string Page(){
   var text=new Dictionary<string,object>{{"hint",I18n.Text("클릭하면 사용처를 확인할 수 있습니다.")},{"lock",I18n.Text("잠긴 문 위치")},{"inside",I18n.Text("문 안쪽")},{"loading",I18n.Text("위키 사진을 불러오는 중…")},{"none",I18n.Text("위키에 사진이 없습니다.")},{"failed",I18n.Text("위키를 불러오지 못했습니다.")},{"source",I18n.Text("출처: Escape from Tarkov Wiki (Fandom) · CC BY-SA")}};
   var css=@"*{box-sizing:border-box}html,body{margin:0;background:PANEL;color:INK;font:13.5px/1.42 'FONT','Segoe UI',sans-serif;overflow:hidden;user-select:none}
#t{border:1px solid BORDER;border-left:3px solid ACCENT;padding:11px 12px 11px 13px;background:PANEL;min-height:100vh;max-height:100vh;overflow:hidden}
.top{display:flex;gap:10px;align-items:center}.top img{width:44px;height:44px;border-radius:5px;background:FIELD;flex:none}.top b{font-size:14.5px}
.pl{color:ATEXT;margin-top:6px}.ds{color:MUTED;font-size:12.5px;margin-top:6px;display:-webkit-box;-webkit-line-clamp:5;-webkit-box-orient:vertical;overflow:hidden}
.hn{color:ATEXT;font-size:12.5px;margin-top:6px}h2{font-size:12.5px;margin:10px 0 4px;color:INK}.st{color:MUTED;font-size:12px;margin-top:8px}
.p{margin-top:6px}.p img{display:block;width:100%;max-width:100%;height:auto;margin:0 auto;border-radius:5px;background:FIELD;min-height:30px}.p div{color:MUTED;font-size:11.5px;margin-top:2px;text-align:center;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.src{color:MUTED;font-size:10.5px;margin-top:8px}";
   css=css.Replace("INK",Hex(Theme.Ink)).Replace("PANEL",Hex(Theme.Panel)).Replace("BORDER",Hex(Theme.Border)).Replace("ACCENT",Hex(Theme.Accent)).Replace("MUTED",Hex(Theme.Muted)).Replace("ATEXT",Hex(Theme.AccentText)).Replace("FIELD",Hex(Theme.Field)).Replace("FONT",I18n.FontName);
   var script=@"const T=TEXT,post=m=>window.chrome.webview.postMessage(m),t=document.getElementById('t');let G=0;
const el=(n,c,x)=>{const e=document.createElement(n);if(c)e.className=c;if(x!=null)e.textContent=x;return e};
const fit=()=>{const ims=[...t.querySelectorAll('.p img')];ims.forEach(i=>i.style.width='100%');if(!ims.length)return;const over=t.scrollHeight-innerHeight;if(over<=0)return;
 const ih=ims.reduce((a,i)=>a+i.getBoundingClientRect().height,0);if(ih<=0)return;const r=Math.max(.25,(ih-over-6)/ih);ims.forEach(i=>i.style.width=(r*100)+'%')};
const size=()=>{fit();if(G)post('h:'+G+':'+Math.ceil(t.getBoundingClientRect().height))};addEventListener('resize',size);
function show(g,d){G=g;t.replaceChildren();const top=el('div','top'),im=document.createElement('img');im.src='https://assets.tarkov.dev/'+d.id+'-icon.jpg';im.onerror=()=>im.remove();top.append(im,el('b','',d.name));t.append(top);
 if(d.places)t.append(el('div','pl',d.places));if(d.description)t.append(el('div','ds',d.description));t.append(el('div','hn',T.hint));
 const w=el('div','');w.id='w';if(d.wiki)w.append(el('div','st',T.loading));t.append(w);size()}
function pics(g,r){if(g!==G)return;const w=document.getElementById('w');if(!w)return;w.replaceChildren();
 if(r.failed){w.append(el('div','st',T.failed));size();return}if(!r.lock.length&&!r.inside.length){w.append(el('div','st',T.none));size();return}
 [[T.lock,r.lock],[T.inside,r.inside]].forEach(([h,ps])=>{if(!ps.length)return;w.append(el('h2','',h));const grid=el('div','');
  ps.forEach(x=>{const f=el('div','p'),im=document.createElement('img');im.src=x.src;if(x.w&&x.h)im.style.aspectRatio=x.w+'/'+x.h;im.onload=size;im.onerror=()=>{f.remove();size()};f.append(im);if(x.caption)f.append(el('div','',x.caption));grid.append(f)});w.append(grid)});
 w.append(el('div','src',T.source));size()}";
   script=script.Replace("TEXT",Json(text));
   return "<!doctype html><html><head><meta charset=\"utf-8\"><meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; img-src https://static.wikia.nocookie.net https://assets.tarkov.dev; style-src 'unsafe-inline'; script-src 'unsafe-inline'\"><style>"+css+"</style></head><body><div id=\"t\"></div><script>"+script+"</script></body></html>";
  }
 }
 public class MapWindow:Form {
  [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h,IntPtr pid);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a,uint b,bool attach);
  [DllImport("user32.dll")] static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] static extern bool ClipCursor(IntPtr r);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h,out RECTI r);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h,ref POINTI p);
  [DllImport("user32.dll")] static extern bool ReleaseCapture();
  [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h,int msg,IntPtr w,IntPtr l);
  [StructLayout(LayoutKind.Sequential)] struct RECTI{public int Left,Top,Right,Bottom;}
  [StructLayout(LayoutKind.Sequential)] struct POINTI{public int X,Y;}
  public int MapKey=77;public MapCanvas Canvas=new MapCanvas();public KeyPanel KeyList=new KeyPanel();public Label State=new Label();public ComboBox Maps=new ComboBox();public ComboBox Role=new ComboBox();
  // Pin editor (edit mode): name picks from this map's exit/transit list and names read from the in-game list.
  public ComboBox PinName=new ComboBox();public TextBox Condition=new TextBox(),Destination=new TextBox();public CheckBox Taxi=new CheckBox();public Button DeletePin=new ThemeButton(),ClearPins=new ThemeButton();
  // Quest card over the map (MapCanvas.PinClicked); MapId/QuestLogs/Store set by the helper's Load-Map.
  public QuestCard Card=new QuestCard{Visible=false};public string MapId="",QuestLogs="",Store="";
  public bool Typing{get{return ActiveControl is TextBox||ActiveControl==PinName;}}
  Pin shownPin;
  public void ShowQuest(Pin p){if(p==null||p.Ref.Length==0)return;
   // The same marker again closes its card.
   if(p==shownPin&&CardOpen){CloseCard();shownPin=null;Canvas.Focus();return;}shownPin=p;var quests=new List<KeyValuePair<string,IDictionary>>();var traders=new List<string>();
   foreach(var id in p.Ref.Split(',')){string trader;IDictionary q=null;try{q=Quests.Quest(QuestLogs,Store,id,out trader);}catch{trader="";}if(q!=null){quests.Add(new KeyValuePair<string,IDictionary>(id,q));traders.Add(trader);}}
   if(quests.Count==0)return;float k=DeviceDpi/96f;int w=Math.Min((int)(400*k),Math.Max(200,Canvas.Width-(int)(40*k)));var bounds=new Rectangle(Canvas.Width-w-(int)(12*k),(int)(12*k),w,Math.Max(100,Canvas.Height-(int)(24*k)));
   lastQuests=quests;lastTraders=traders;lastBounds=bounds;
   if(Browser()){Card.Hide();html.Show(quests,traders,MapId,bounds);return;}
   Card.InstallBrowser=BrowserMissing?(Action<Button>)WebViewSetup.Run:null;Card.Bounds=bounds;Card.Show(quests,traders,MapId,Store);}
  // The browser card (pictures) when WebView2 works; otherwise QuestCard. A failure is final for this run.
  HtmlQuestCard html;bool htmlFailed;List<KeyValuePair<string,IDictionary>> lastQuests;List<string> lastTraders;Rectangle lastBounds;
  bool Browser(){if(htmlFailed)return false;if(html!=null)return true;try{if(!HtmlQuestCard.Runtime())return false;html=Create();Canvas.Controls.Add(html.View);return true;}catch{htmlFailed=true;html=null;return false;}}
  public void Warm(){if(Store.Length==0||!Browser())return;var h=Handle;html.Warm();if(KeyTipBrowser())keyTip.Warm();}
  HtmlKeyTip keyTip;bool keyTipFailed;
  bool KeyTipBrowser(){if(keyTipFailed||htmlFailed||Store.Length==0)return false;if(keyTip!=null)return true;try{if(!HtmlQuestCard.Runtime())return false;keyTip=new HtmlKeyTip(this,Store);Canvas.Controls.Add(keyTip.View);return true;}catch{keyTipFailed=true;keyTip=null;return false;}}
  public void KeyTipFailed(){keyTipFailed=true;if(keyTip!=null)keyTip.View.Visible=false;if(KeyList.HoverName.Length>0){int y=Canvas.PointToClient(KeyList.PointToScreen(new Point(0,KeyList.HoverY))).Y;Canvas.SetKeyTip(KeyList.HoverName,KeyList.HoverDescription,KeyList.HoverPlaces,y);}}
  public bool BrowserMissing{get{try{return !htmlFailed&&!HtmlQuestCard.Runtime();}catch{return false;}}}
  HtmlQuestCard Create(){return new HtmlQuestCard(this,Store);}
  public void CardFailed(){htmlFailed=true;if(html!=null){html.View.Visible=false;}if(lastQuests!=null){Card.Bounds=lastBounds;Card.Show(lastQuests,lastTraders,MapId,Store);}}
  bool CardOpen{get{return Card.Visible||(html!=null&&html.Visible);}}
  // Where the open quest card is on the canvas (empty when none), so the key tip goes beside it.
  public Rectangle CardBounds{get{if(html!=null&&html.Visible)return html.View.Bounds;if(Card.Visible)return Card.Bounds;return Rectangle.Empty;}}
  void CloseCard(){Card.Hide();if(html!=null)html.Hide();}
  public string MyKeys="";
  public IntPtr ReturnTo=IntPtr.Zero;Label title=new Label();Panel header=new Panel(),editBar=new Panel();Button editButton;bool binding;
  readonly Dictionary<string,string> knownConditions=new Dictionary<string,string>();
  // names[i] goes into the picker; conditions[i] (may be empty) pre-fills the condition when that name is picked.
  public void SetSuggestions(string[] names,string[] conditions){
   PinName.Items.Clear();knownConditions.Clear();
   for(int i=0;i<names.Length;i++){if(string.IsNullOrWhiteSpace(names[i])||PinName.Items.Contains(names[i]))continue;PinName.Items.Add(names[i]);if(conditions!=null&&i<conditions.Length&&!string.IsNullOrEmpty(conditions[i]))knownConditions[names[i]]=conditions[i];}
  }
  void Bind(){
   binding=true;var p=Canvas.Selected;foreach(Control c in new Control[]{PinName,Condition,Destination,Taxi,DeletePin})c.Enabled=p!=null;
   PinName.Text=p==null?"":p.Name;Condition.Text=p==null?"":p.Condition;Destination.Text=p==null?"":p.Destination;Taxi.Checked=p!=null&&p.Taxi;binding=false;
   if(p!=null&&p.Name.Length==0)PinName.Focus();
  }
  void Edited(){
   var p=Canvas.Selected;if(binding||p==null)return;
   p.Name=PinName.Text.Trim();string known;if(Condition.Text.Length==0&&knownConditions.TryGetValue(p.Name,out known)){binding=true;Condition.Text=known;binding=false;}
   p.Condition=Condition.Text.Trim();p.Destination=Destination.Text.Trim();p.Taxi=Taxi.Checked;Canvas.Dirty=true;Canvas.Invalidate();
  }
  public void SetEditMode(bool on){editBar.Visible=on;Canvas.EditMode=on;editButton.BackColor=on?Theme.Accent:Theme.Field;if(!on)Canvas.Selected=null;Bind();Canvas.Invalidate();}
  static readonly Color HeaderBg=Theme.Panel,Line=Theme.Border,CloseBg=Theme.Accent;
  Button Flat(string text){var b=new ThemeButton{Text=text,FlatStyle=FlatStyle.Flat,BackColor=Theme.Field,ForeColor=Theme.Ink,Cursor=Cursors.Hand,TabStop=false};b.FlatAppearance.BorderColor=Line;b.FlatAppearance.MouseOverBackColor=Theme.Hover;b.FlatAppearance.MouseDownBackColor=Theme.Accent;return b;}
  public MapWindow(){
   Text=I18n.Text("BlueAsh · 지도");FormBorderStyle=FormBorderStyle.None;StartPosition=FormStartPosition.Manual;ShowInTaskbar=false;TopMost=true;KeyPreview=true;
   BackColor=Line;Padding=new Padding(1);Font=new Font(I18n.FontName,10f);ForeColor=Theme.Ink;Size=new Size(1150,820);
   // Header: map title, map/role pickers, fit, edit toggle, close.
   header.Dock=DockStyle.Top;header.BackColor=HeaderBg;header.Height=40;
   title.AutoSize=false;title.AutoEllipsis=true;title.Dock=DockStyle.Fill;title.TextAlign=ContentAlignment.MiddleLeft;title.Padding=new Padding(12,0,0,0);title.Font=new Font(I18n.FontName,12f,FontStyle.Bold);title.ForeColor=Theme.Ink;
   var tools=new FlowLayoutPanel{Dock=DockStyle.Right,AutoSize=true,WrapContents=false,BackColor=HeaderBg,Padding=new Padding(0,5,4,0)};
   Maps.DropDownStyle=ComboBoxStyle.DropDownList;Role.DropDownStyle=ComboBoxStyle.DropDownList;Role.Items.AddRange(new object[]{"PMC","SCAV"});Role.SelectedIndex=0;
   // Owner-drawn so the pickers stay dark (a flat DropDownList still paints a white box).
   foreach(var c in new[]{Maps,Role}){c.FlatStyle=FlatStyle.Flat;c.BackColor=Theme.Field;c.ForeColor=ForeColor;c.Margin=new Padding(3,0,3,0);c.DrawMode=DrawMode.OwnerDrawFixed;
    c.DrawItem+=(s,e)=>{var box=(ComboBox)s;bool sel=(e.State&DrawItemState.Selected)!=0&&(e.State&DrawItemState.ComboBoxEdit)==0;
     using(var b=new SolidBrush(sel?Theme.Hover:Theme.Field))e.Graphics.FillRectangle(b,e.Bounds);
     if(e.Index>=0)TextRenderer.DrawText(e.Graphics,box.Items[e.Index].ToString(),box.Font,e.Bounds,box.ForeColor,TextFormatFlags.VerticalCenter|TextFormatFlags.Left|TextFormatFlags.EndEllipsis);};}
   Maps.Width=200;Role.Width=80;
   var fit=Flat(I18n.Text("맞춤"));fit.Click+=delegate{Canvas.ResetView();};var edit=Flat(I18n.Text("편집"));editButton=edit;edit.Click+=delegate{SetEditMode(!editBar.Visible);};
   var close=new ThemeButton{Text="✕",FlatStyle=FlatStyle.Flat,BackColor=CloseBg,ForeColor=Theme.Ink,TabStop=false,Cursor=Cursors.Hand};close.FlatAppearance.BorderSize=0;close.FlatAppearance.MouseOverBackColor=Theme.AccentHover;close.Click+=delegate{HideMap();};
   tools.Controls.AddRange(new Control[]{Maps,Role,fit,edit,close});header.Controls.Add(title);header.Controls.Add(tools);
   // Drag the panel by its header like a game window.
   MouseEventHandler grab=(s,e)=>{if(e.Button==MouseButtons.Left){ReleaseCapture();SendMessage(Handle,0xA1,new IntPtr(2),IntPtr.Zero);}};header.MouseDown+=grab;title.MouseDown+=grab;
   // Pin editing tools stay hidden until "편집".
   editBar.Dock=DockStyle.Bottom;editBar.BackColor=HeaderBg;editBar.Visible=false;
   var row=new FlowLayoutPanel{Dock=DockStyle.Fill,WrapContents=true,AutoScroll=true,BackColor=HeaderBg,Padding=new Padding(8,6,8,0)};
   Func<string,Label> cap=t=>new Label{Text=I18n.Text(t),AutoSize=true,ForeColor=Theme.Muted,Padding=new Padding(4,6,0,0)};
   PinName.DropDownStyle=ComboBoxStyle.DropDown;PinName.AutoCompleteMode=AutoCompleteMode.SuggestAppend;PinName.AutoCompleteSource=AutoCompleteSource.ListItems;PinName.Width=260;PinName.FlatStyle=FlatStyle.Flat;
   Condition.Width=260;Destination.Width=150;Taxi.Text=I18n.Text("택시");Taxi.AutoSize=true;Taxi.ForeColor=ForeColor;
   foreach(Control t in new Control[]{PinName,Condition,Destination}){t.BackColor=Theme.Field;t.ForeColor=ForeColor;var tb=t as TextBox;if(tb!=null)tb.BorderStyle=BorderStyle.FixedSingle;}
   foreach(var b in new[]{DeletePin,ClearPins}){b.FlatStyle=FlatStyle.Flat;b.BackColor=Theme.Field;b.ForeColor=ForeColor;b.FlatAppearance.BorderColor=Line;b.AutoSize=true;}
   DeletePin.Text=I18n.Text("삭제");ClearPins.Text=I18n.Text("이 지도 초기화");
   DeletePin.Click+=delegate{var p=Canvas.Selected;if(p!=null){Canvas.Pins.Remove(p);Canvas.Selected=null;Canvas.Dirty=true;Bind();Canvas.Invalidate();}};
   PinName.TextChanged+=delegate{Edited();};PinName.SelectedIndexChanged+=delegate{Edited();};Condition.TextChanged+=delegate{Edited();};Destination.TextChanged+=delegate{Edited();};Taxi.CheckedChanged+=delegate{Edited();};
   Canvas.SelectionChanged+=delegate{Bind();};
   var hint=cap("지도 클릭: 추가 · 끌기: 이동 · 우클릭: 삭제");
   row.Controls.AddRange(new Control[]{cap("이름"),PinName,cap("조건"),Condition,cap("트랜짓"),Destination,Taxi,DeletePin,ClearPins,hint});editBar.Controls.Add(row);
   State.Dock=DockStyle.Bottom;State.Height=48;State.BackColor=HeaderBg;State.ForeColor=Theme.Muted;State.TextAlign=ContentAlignment.MiddleLeft;State.Padding=new Padding(12,0,0,0);
   KeyList.Dock=DockStyle.Right;KeyList.HoverChanged+=delegate{Canvas.HighlightRef=KeyList.ActiveRef;int y=Canvas.PointToClient(KeyList.PointToScreen(new Point(0,KeyList.HoverY))).Y;
    // The browser tip with the wiki's door and room pictures; the canvas's text tip when it cannot be used.
    if(KeyList.HoverName.Length>0&&KeyTipBrowser()){Canvas.SetKeyTip("","","",y);keyTip.Show(KeyList.Hover,KeyList.HoverName,KeyList.HoverPlaces,KeyList.HoverDescription,KeyList.HoverWiki,KeyList.HoverCard,y);}
    else{if(keyTip!=null)keyTip.Hide();Canvas.SetKeyTip(KeyList.HoverName,KeyList.HoverDescription,KeyList.HoverPlaces,y);}};
   VisibleChanged+=delegate{if(!Visible){KeyList.ClearPreview();CloseCard();}};
   Card.Anchor=AnchorStyles.Top|AnchorStyles.Right|AnchorStyles.Bottom;Canvas.Controls.Add(Card);
   Canvas.PinClicked+=(s,e)=>ShowQuest(e.Pin);Card.NeededKeys=q=>Quests.NeededKeys(q,QuestLogs,Store,MyKeys);
   Controls.Add(Canvas);Controls.Add(KeyList);Controls.Add(State);Controls.Add(editBar);Controls.Add(header);Bind();
   EventHandler retitle=delegate{title.Text=I18n.Text("지도")+"  ·  "+Maps.SelectedItem+"  ·  "+Role.SelectedItem;};Maps.SelectedIndexChanged+=retitle;Role.SelectedIndexChanged+=retitle;
   FormClosing+=delegate(object s,FormClosingEventArgs e){if(e.CloseReason==CloseReason.UserClosing){e.Cancel=true;HideMap();}};
   // Quick exits: Esc always; the map key, Tab and movement keys (W A S D) too unless typing in the editor,
   // so the map gets out of the way the moment you start moving.
   // Esc steps back one layer: the quest card, then the map. The map key closes everything; it is
   // handled by the helper's key watcher (also when the map has focus), so it is not handled here as well.
   KeyDown+=delegate(object s,KeyEventArgs e){bool typing=ActiveControl is TextBox||ActiveControl==PinName;
    if(e.KeyCode==Keys.Escape&&CardOpen){CloseCard();Canvas.Focus();e.Handled=true;e.SuppressKeyPress=true;}
    else if(e.KeyCode==Keys.Escape||(!typing&&(e.KeyCode==Keys.Tab||e.KeyCode==Keys.W||e.KeyCode==Keys.A||e.KeyCode==Keys.S||e.KeyCode==Keys.D))){HideMap();e.Handled=true;e.SuppressKeyPress=true;}
    else if(!typing&&(int)e.KeyCode==MapKey){e.Handled=true;e.SuppressKeyPress=true;}
    else if(e.KeyCode==Keys.Oemplus||e.KeyCode==Keys.Add){Canvas.ZoomAt(1.2f,new Point(Canvas.Width/2,Canvas.Height/2));e.Handled=true;}
    else if(e.KeyCode==Keys.OemMinus||e.KeyCode==Keys.Subtract){Canvas.ZoomAt(1/1.2f,new Point(Canvas.Width/2,Canvas.Height/2));e.Handled=true;}};
  }
  // Centre over the game's client area at ~half the screen (82% of its height, map-friendly width).
  public void ShowOver(IntPtr game){
   Rectangle area;RECTI r;var p=new POINTI();
   if(game!=IntPtr.Zero&&GetClientRect(game,out r)&&ClientToScreen(game,ref p)&&r.Right>200)area=new Rectangle(p.X,p.Y,r.Right,r.Bottom);else area=Screen.PrimaryScreen.Bounds;
   ReturnTo=game;float k=DeviceDpi/96f;header.Height=(int)(40*k);Maps.Width=(int)(200*k);Role.Width=(int)(90*k);State.Height=(int)(48*k);editBar.Height=(int)(132*k);PinName.Width=(int)(260*k);Condition.Width=(int)(260*k);Destination.Width=(int)(150*k);foreach(Control c in header.Controls[1].Controls)if(c is Button){c.Height=(int)(30*k);c.Width=c.Text=="✕"?(int)(40*k):Math.Max((int)(64*k),c.PreferredSize.Width+(int)(12*k));}
   KeyList.Width=(int)(290*k);
   int h=(int)(area.Height*0.82),w=Math.Min((int)(area.Width*0.94),Math.Max((int)(area.Width*0.5),(int)(h*1.15))+KeyList.Width);
   Bounds=new Rectangle(area.X+(area.Width-w)/2,area.Y+(area.Height-h)/2,w,h);
   if(!Visible)Show();TakeFocus();Canvas.Focus();
  }
  // The game holds the cursor (clipped and re-centred) while it is the foreground window, and Windows refuses a plain
  // Activate from this background helper the first time (no input went to it): join the game's input queue for the
  // switch, then lift the clip so the cursor moves freely over the map at once.
  void TakeFocus(){
   var front=GetForegroundWindow();
   if(front!=Handle){uint a=GetWindowThreadProcessId(front,IntPtr.Zero),b=GetCurrentThreadId();bool joined=a!=0&&a!=b&&AttachThreadInput(a,b,true);
    try{BringWindowToTop(Handle);SetForegroundWindow(Handle);Activate();}finally{if(joined)AttachThreadInput(a,b,false);}}
   ClipCursor(IntPtr.Zero);
  }
  public void HideMap(){if(!Visible)return;Hide();if(ReturnTo!=IntPtr.Zero)SetForegroundWindow(ReturnTo);}
  // Tab is a dialog-navigation key and never reaches KeyDown; catch it here so Tab closes the map too.
  protected override bool ProcessDialogKey(Keys k){if(k==Keys.Tab&&!(ActiveControl is TextBox||ActiveControl==PinName)){HideMap();return true;}return base.ProcessDialogKey(k);}
 }
}
