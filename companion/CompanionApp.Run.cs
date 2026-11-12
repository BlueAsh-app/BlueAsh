using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using ModeSwitchPrice;
using BlueAshUi;
using Windows.Media.Ocr;
namespace ModePulse {
 public sealed partial class CompanionApp {
  // A scroll bar, when a long translation needs one, in the dark system style instead of the white default.
  [System.Runtime.InteropServices.DllImport("uxtheme.dll",CharSet=System.Runtime.InteropServices.CharSet.Unicode)] static extern int SetWindowTheme(IntPtr h,string app,string id);
  static void DarkScroll(Control c){EventHandler set=delegate{try{SetWindowTheme(c.Handle,"DarkMode_Explorer",null);}catch{}};if(c.IsHandleCreated)set(c,EventArgs.Empty);else c.HandleCreated+=set;}
  // ---- layout: the pages are built in 96-DPI design units and scaled to the system DPI (k) before fitting ----
  // Rows retain their original gaps. Longer translations wrap and scroll at the native page size.
  void FitPage(Form f){
   var actions=f.Controls.Cast<Control>().Where(c=>c.Top>=Px(540)).ToList();var footer=new Panel{Dock=DockStyle.Bottom,Height=Px(56),Width=f.ClientSize.Width,BackColor=Theme.Panel};
   foreach(var c in actions){footer.Controls.Add(c);c.Top=Px(6);if(c.Left>=Px(400))c.Anchor=AnchorStyles.Top|AnchorStyles.Right;}
   var body=new Panel{Dock=DockStyle.Fill,Width=f.ClientSize.Width,BackColor=Theme.Panel,AutoScroll=true};
   foreach(var c in f.Controls.Cast<Control>().ToList()){body.Controls.Add(c);if(c.Right>=Px(660)){c.Width=Math.Max(Px(40),f.ClientSize.Width-Px(16)-c.Left);c.Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right;}}
   f.Controls.Add(body);f.Controls.Add(footer);body.BringToFront();DarkScroll(body);
   var rows=new List<List<Control>>();var rowTops=new List<int>();
   foreach(var c in body.Controls.Cast<Control>().OrderBy(c=>c.Top)){if(rows.Count==0||c.Top-rowTops[rowTops.Count-1]>Px(8)){rows.Add(new List<Control>{c});rowTops.Add(c.Top);}else rows[rows.Count-1].Add(c);}
   int shift=0,end=0,originalEnd=0;
   for(int i=0;i<rows.Count;i++){var row=rows[i];int oldEnd=row.Max(c=>c.Bottom);
    if(end!=0)shift=Math.Max(shift,end+Math.Max(Px(4),rowTops[i]-originalEnd)-rowTops[i]);
    foreach(var c in row){
     if(c is Label&&!string.IsNullOrEmpty(c.Text)){int h=TextRenderer.MeasureText(c.Text,c.Font,new Size(c.Width,0),TextFormatFlags.WordBreak).Height+Px(4);c.Height=Math.Max(c.Height,h);}
     // A button whose translation does not fit on one line shows it on two, split at the space nearest the middle (the
     // button's own word wrap split words), and grows to fit instead of cutting the text off.
     else if(c is Button&&!string.IsNullOrEmpty(c.Text)&&c.Text.IndexOf('\n')<0&&TextRenderer.MeasureText(c.Text,c.Font).Width>c.Width-Px(16)){
      int mid=c.Text.Length/2,cut=-1;for(int d=0;d<mid&&cut<0;d++){if(mid+d<c.Text.Length&&c.Text[mid+d]==' ')cut=mid+d;else if(c.Text[mid-d]==' ')cut=mid-d;}
      if(cut>0)c.Text=c.Text.Substring(0,cut)+"\n"+c.Text.Substring(cut+1);
      int h=TextRenderer.MeasureText(c.Text,c.Font,new Size(c.Width-Px(16),0),TextFormatFlags.WordBreak).Height+Px(18);c.Height=Math.Max(c.Height,h);}
     c.Top+=shift;}
    end=row.Max(c=>c.Bottom);originalEnd=oldEnd;
   }
   body.AutoScrollMinSize=new Size(0,end+Px(10));
  }
  // The notifications page uses two measured blocks instead of a status box that fills the unused height.
  // Control variables and event handlers stay the same, including the relay-free webhook route.
  Panel discordBody,discordActions;ThemeCard connectionCard,alertCard;bool notificationLayout;
  void FitNotifications(){
   discordForm.BackColor=Theme.Panel;
   discordBody=new Panel{Dock=DockStyle.Fill,BackColor=Theme.Panel,AutoScroll=true};
   connectionCard=new ThemeCard{BackColor=Theme.Panel};alertCard=new ThemeCard{BackColor=Theme.Panel};
   discordActions=new Panel{BackColor=Theme.Panel};
   foreach(var c in new Control[]{phoneTitle,phoneLabel,phoneBtn,phoneHelp,linkLabel,connectBtn,helpBtn,hookLabel,hook,discordStatus})connectionCard.Controls.Add(c);
   foreach(var c in new Control[]{alertTitle,discordCheck,maintenanceCheck,idleLabel,idleBox,idleHint,serverHint})alertCard.Controls.Add(c);
   foreach(var c in new Control[]{testBtn,hideBtn,saveBtn}){discordActions.Controls.Add(c);c.Top=0;}
   foreach(var c in new Control[]{connectionCard,alertCard,discordActions})discordBody.Controls.Add(c);
   discordForm.Controls.Add(discordBody);DarkScroll(discordBody);
   discordForm.Resize+=delegate{LayoutNotifications();};discordBody.SizeChanged+=delegate{LayoutNotifications();};phoneLabel.TextChanged+=delegate{LayoutNotifications();};
   LayoutNotifications();
  }
  int TextHeight(Control c,int width,int minimum){
   return Math.Max(minimum,TextRenderer.MeasureText(c.Text,c.Font,new Size(width,0),TextFormatFlags.WordBreak|TextFormatFlags.NoPrefix).Height+Px(4));
  }
  void LayoutNotifications(){
   if(notificationLayout||discordBody==null)return;notificationLayout=true;var body=discordBody;
   try{
    body.SuspendLayout();int w=Math.Max(Px(420),body.ClientSize.Width-Px(16)),inner=w-Px(32),y=Px(16),m=Px(16);
    phoneTitle.SetBounds(m,y,inner,Px(28));y+=Px(36);
    int buttonWidth=Math.Min(Px(224),Math.Max(Px(210),TextRenderer.MeasureText(phoneBtn.Text,phoneBtn.Font).Width+Px(24)));
    int labelWidth=relay.Length>0?inner-buttonWidth-m:inner,rowHeight=Px(40);
    phoneLabel.SetBounds(m,y+Px(5),Math.Min(labelWidth,phoneLabel.PreferredWidthFor(phoneLabel.Text)),Px(30));phoneBtn.SetBounds(w-m-buttonWidth,y,buttonWidth,Px(40));y+=rowHeight+Px(12);
    int hintHeight=TextHeight(phoneHelp,inner,Px(42));phoneHelp.SetBounds(m,y,inner,hintHeight);y+=hintHeight+Px(12);
    if(webhookRoute){
     labelWidth=Math.Max(Px(130),TextRenderer.MeasureText(hookLabel.Text,hookLabel.Font).Width+Px(8));
     hookLabel.SetBounds(m,y+Px(4),labelWidth,Px(30));hook.SetBounds(m+labelWidth+Px(12),y,inner-labelWidth-Px(12),Px(30));y+=Px(42);
     helpBtn.SetBounds(w-m-Px(224),y,Px(224),Px(40));y+=Px(52);
    }
    discordStatus.SetBounds(m,y,inner,Px(64));y+=Px(80);connectionCard.SetBounds(Px(8),Px(12),w,y);
    y=m;alertTitle.SetBounds(m,y,inner,Px(28));y+=Px(36);
    int checkHeight=TextHeight(discordCheck,inner-Px(24),Px(30));discordCheck.SetBounds(m,y,inner,checkHeight);y+=checkHeight+Px(14);
    labelWidth=Math.Max(Px(130),TextRenderer.MeasureText(idleLabel.Text,idleLabel.Font).Width+Px(8));
    int hintWidth=inner-labelWidth-Px(90)-Px(32);rowHeight=TextHeight(idleHint,hintWidth,Px(30));
    idleLabel.SetBounds(m,y+Px(4),labelWidth,rowHeight);idleBox.Location=new Point(m+labelWidth+Px(12),y-Px(1));idleHint.SetBounds(m+labelWidth+Px(160),y+Px(3),hintWidth-Px(42),rowHeight);y+=rowHeight+Px(14);
    hintHeight=TextHeight(serverHint,inner,Px(28));serverHint.SetBounds(m,y,inner,hintHeight);y+=hintHeight+m;
    alertCard.SetBounds(Px(8),connectionCard.Bottom+Px(12),w,y);
    discordActions.SetBounds(Px(8),alertCard.Bottom+m,w,Px(40));
    int testWidth=Math.Min(Px(280),Math.Max(Px(240),TextRenderer.MeasureText(testBtn.Text,testBtn.Font).Width+Px(24)));
    testBtn.SetBounds(0,0,testWidth,Px(40));saveBtn.SetBounds(w-Px(140),0,Px(140),Px(40));hideBtn.SetBounds(w-Px(272),0,Px(120),Px(40));
    body.AutoScrollMinSize=new Size(0,discordActions.Bottom+Px(12));
   }finally{body.ResumeLayout(true);notificationLayout=false;}
  }

  // ---- screenshots: new in-raid screenshots carry position and facing in their names ----
  sealed class PendingDelete{public string Path;public DateTime At,Until;}
  readonly List<PendingDelete> shotDeletes=new List<PendingDelete>();DateTime nextShotWatch=DateTime.MinValue,mapKeyAt=DateTime.MinValue;string raidPhase="";
  void ReadScreenshots(){
   var now=DateTime.UtcNow;bool watch=config.shotPos||shotOnMap.Checked;
   if(watch&&!ScreenshotPosition.Watching&&now>=nextShotWatch){nextShotWatch=now.AddSeconds(10);ScreenshotPosition.Watch(true);}
   string file;
   while((file=ScreenshotPosition.Next())!=null){
    // The map key is also the game's screenshot key ("지도 오픈 시 위치 갱신"), so the game saves one on every press,
    // menus and hideout included. Outside a raid such a screenshot is not wanted: remove it (PrintScreen ones stay).
    if((now-mapKeyAt).TotalSeconds<3&&raidPhase!="started"){shotDeletes.Add(new PendingDelete{Path=file,At=now.AddSeconds(2),Until=now.AddSeconds(20)});continue;}
    var pos=ScreenshotPosition.Parse(file);if(pos==null||!config.shotPos||mapId.Length==0)continue;
    var id=mapId.Substring(0,mapId.LastIndexOf('_'));var t=FandomMarkers.Transform(id,markerShipped);
    if(t==null||map.Canvas.MapImage==null){Status(T("이 지도는 스크린샷 위치 표시를 지원하지 않습니다 (탈출구 좌표가 부족한 지도)."));continue;}
    var c=map.Canvas;var img=FandomMarkers.ToImage(id,markerShipped,pos[0],pos[1],pos[2]);c.MeX=(float)img[0];c.MeY=(float)img[1];
    c.MeDirX=(float)((t[0]*pos[3]+t[1]*pos[4])*c.MapImage.Width);c.MeDirY=(float)((t[3]*pos[3]+t[4]*pos[4])*c.MapImage.Height);c.MeShown=true;c.Invalidate();
    Status(T("내 위치 표시 · ")+DateTime.Now.ToString("HH:mm:ss")+T(" 스크린샷 기준 · 지도 키로 확인하세요."));
    if(config.shotDelete)shotDeletes.Add(new PendingDelete{Path=file,At=now.AddSeconds(2),Until=now.AddSeconds(20)});
   }
   // The game may still be writing the PNG: delete a little later, retrying briefly.
   foreach(var d in shotDeletes.ToList()){if(now<d.At)continue;try{File.Delete(d.Path);if(!File.Exists(d.Path))shotDeletes.Remove(d);else throw new IOException();}catch{if(!File.Exists(d.Path)||now>d.Until)shotDeletes.Remove(d);else d.At=now.AddSeconds(1);}}
  }

  // ---- OCR: one WinRT operation chain at a time, polled alongside the map keys; no UI-thread waits ----
  OcrEngine engine,mapEngine;
  void InitOcr(){
   if(engine!=null)return;
   // Exit names are English in the game's list: English OCR reads them best, then Korean, then any installed language.
   foreach(var tag in new[]{"en","ko"}){if(engine!=null)break;engine=WinOcr.Engine(tag);}
   if(engine==null)engine=OcrEngine.TryCreateFromUserProfileLanguages();
   if(engine==null)throw new Exception(T("Windows 한국어 OCR 언어가 필요합니다."));
  }
  sealed class ExitRead{public Task<byte[]> Capture;public Task<OcrText> Recognize;public byte[] Png;public int Generation;public string Map;public DateTime Until;public bool Discard;}
  ExitRead exitRead;
  // The capture itself runs on a pool thread, so the map key is answered while the list is read.
  void ReadExits(IntPtr game){
   InitOcr();exitRead=new ExitRead{Capture=Desktop.ExitListPngAsync(game),Generation=exitGeneration,Map=mapId,Until=DateTime.UtcNow.AddSeconds(8)};
  }
  void PollExitRead(){
   var job=exitRead;if(job==null)return;var task=job.Recognize!=null?(Task)job.Recognize:job.Capture;
   if(!task.IsCompleted){
    if(!job.Discard&&DateTime.UtcNow>job.Until){job.Discard=true;Status(T("OCR 처리 시간 초과"));}
    return;
   }
   // A frame taken while the map window opened may show the map instead of the list: dropped.
   if(job.Discard||job.Generation!=exitGeneration||job.Map!=mapId||!mapsOn||(job.Recognize==null&&map.Visible)){exitRead=null;return;}
   try{
    if(job.Recognize==null){job.Png=job.Capture.GetAwaiter().GetResult();var png=job.Png;var e=engine;job.Recognize=Task.Run(()=>WinOcr.Read(e,png));return;}
    var result=job.Recognize.GetAwaiter().GetResult();var evidence=TaxiObservations(result,job.Png);exitRead=null;
    ApplyExitLines(result.Lines.Select(l=>l.Text).ToList(),evidence);
   }catch(Exception ex){exitRead=null;captures=0;autoExits=null;Status(Message(ex));}
  }
  static string Message(Exception ex){var a=ex as AggregateException;if(a!=null&&a.InnerException!=null)ex=a.InnerException;return ex.Message;}
  Dictionary<string,string> TaxiObservations(OcrText result,byte[] png){return map==null?new Dictionary<string,string>():TaxiRead.Observe(result,png,map.Canvas.Pins);}
  void UpdateTaxiStates(Dictionary<string,string> observations){taxiLastFrame=TaxiRead.Update(map.Canvas.TaxiStates,map.Canvas.Available,map.Canvas.Pins,observations,taxiLastFrame);}
  void ApplyExitLines(List<string> lines,Dictionary<string,string> taxiEvidence){
   RememberExits(lines);var hits=ExitMatch.Match(lines,map.Canvas.Pins.Where(p=>p.Kind=="exit"||p.Kind=="transit"));
   // The exit list also tells the side: when PMC/Scav was not detected and the other side's exits match clearly
   // better (transits count for both), switch to it; LoadMap then shows that side's exits.
   if(mapId.Length>0){var id=mapId.Substring(0,mapId.LastIndexOf('_'));var cur=MapRole();var other=cur=="SCAV"?"PMC":"SCAV";
    if(catalog.Any(r=>S(r,"id")==id&&S(r,"role")==other)){
     var alt=ExitMatch.Match(lines,FandomMarkers.Pins(id,other,markerCache,markerShipped).Where(p=>p.Kind=="exit"));
     var now1=ExitMatch.Match(lines,map.Canvas.Pins.Where(p=>p.Kind=="exit"));
     if(alt.Count>=2&&alt.Count>now1.Count+1){map.Role.SelectedItem=other;hits=ExitMatch.Match(lines,map.Canvas.Pins.Where(p=>p.Kind=="exit"||p.Kind=="transit"));lastFrame=new HashSet<string>(hits);}}}
   // Shown once a name is read in two frames of the same check (any two of them: a misread frame in between no longer
   // resets it); a new check (exit key, raid start) starts over.
   int known=map.Canvas.Available.Count;ExitMatch.Confirm(lastFrame,hits,map.Canvas.Available);UpdateTaxiStates(taxiEvidence);map.Canvas.Invalidate();
   // the raid-start check stops once two reads in a row add nothing to what is shown (each read is a capture + OCR while the game renders)
   if(autoExits!=null){if(known>0&&map.Canvas.Available.Count==known){if(++autoQuiet>=2)autoExits=null;}else autoQuiet=0;}
   map.State.Text=T("확인된 탈출구: ")+string.Join(", ",map.Canvas.Available);
   Status(T("탈출구 OCR 읽음: ")+string.Join(" / ",lines)+T("\r\n등록 좌표와 일치한 이름만 2회 확인 후 표시합니다."));
  }

  // ---- map key: check only on a map-key request with a visible game cursor (the flea market / item search screens
  // take the key); gameplay and closing the map stay immediate ----
  void InitMapOcr(){
   InitOcr();if(mapEngine!=null)return;
   var ini=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Battlestate Games","Escape from Tarkov","Settings","Game.ini");
   mapEngine=WinOcr.Engine(GameLanguage.OcrTag(GameLanguage.Api(ini)))??engine;
  }
  sealed class MapRead{public IntPtr Game;public Bitmap Image;public int Nav;public Task<OcrText> Task;public bool Discard;public DateTime Until;}
  MapRead mapRead;
  void DisposeMapRead(MapRead job){if(job.Image!=null)job.Image.Dispose();}
  void RequestMapOpen(IntPtr game){
   if(map.Visible){map.HideMap();return;}
   if(mapRead!=null){mapRead.Discard=true;return;}
   if(raidPhase=="started"&&!ClickTrigger.CursorShown()){LoadMap();map.ShowOver(game);return;}
   Bitmap frame=null;
   try{
    frame=Desktop.Grab(game);InitMapOcr();var job=new MapRead{Game=game,Until=DateTime.UtcNow.AddSeconds(2)};
    try{
     int w,h;var px=Desktop.Sample(frame,1600,out w,out h);var bars=Core.TitleBars(px,w,h).Select(b=>new Rectangle(b.X,b.Y,b.W,b.H)).ToArray();
     int nav;job.Image=MapSearch.Sheet(frame,bars,out nav);job.Nav=nav;byte[] bytes;
     using(var memory=new MemoryStream()){job.Image.Save(memory,System.Drawing.Imaging.ImageFormat.Png);bytes=memory.ToArray();}
     var e=mapEngine;job.Task=Task.Run(()=>WinOcr.Read(e,bytes));mapRead=job;
    }catch{DisposeMapRead(job);throw;}
   }catch(Exception ex){Status(Message(ex));}finally{if(frame!=null)frame.Dispose();}
  }
  void PollMapRead(IntPtr foreground){
   var job=mapRead;if(job==null)return;
   if(!mapsOn||foreground!=job.Game||map.Visible||DateTime.UtcNow>job.Until)job.Discard=true;
   if(!job.Task.IsCompleted)return;
   mapRead=null;
   try{
    if(job.Discard)return;
    var result=job.Task.GetAwaiter().GetResult();var text=new List<string>();var boxes=new List<RectangleF>();
    foreach(var line in result.Lines)foreach(var word in line.Words){text.Add(word.Text);boxes.Add(word.Box);}
    bool blocked=MapSearch.Blocked(job.Image,job.Nav,text.ToArray(),boxes.ToArray());
    if(!blocked&&mapsOn&&!map.Visible&&foreground==job.Game&&foreground!=IntPtr.Zero){LoadMap();map.ShowOver(foreground);}
   }catch(Exception ex){Status(Message(ex));}finally{DisposeMapRead(job);}
  }
 }
 // Vehicle (taxi) extracts: the car row in the exit list read for its identifier colour and countdown.
 public static class TaxiRead {
  // Match the car name first, then inspect that row's identifier colour and countdown only.
  public static Dictionary<string,string> Observe(OcrText result,byte[] png,IEnumerable<Pin> pins){
   var observations=new Dictionary<string,string>();if(png==null)return observations;
   var taxi=pins.Where(p=>TaxiMarker.IsTaxi(p)).ToList();if(taxi.Count==0)return observations;
   foreach(var line in result.Lines){
    if(line.Words.Count==0)continue;var names=ExitMatch.Match(new[]{line.Text},taxi);if(names.Count==0)continue;
    var r=line.Words[0].Box;bool identifier=Regex.IsMatch(line.Words[0].Text,@"d{1,2}$");var colour=TaxiVision.Colour(png,r,identifier);var band=new List<string>();
    foreach(var other in result.Lines)foreach(var word in other.Words){var b=word.Box;if(Math.Abs((b.Y+b.Height/2)-(r.Y+r.Height/2))<=Math.Max(r.Height,b.Height)*0.6)band.Add(word.Text);}
    bool countdown=TaxiVision.Countdown(string.Join(" ",band));
    var state=colour=="red"?"Gone":colour=="green"&&countdown?"Using":colour=="white"?"Normal":"Unknown";
    foreach(var name in names){string was;if(observations.TryGetValue(name,out was)&&was!=state)observations[name]="Unknown";else observations[name]=state;}
   }
   return observations;
  }
  // A state is taken after two equal reads in a row; returns this frame as the next "last".
  public static Dictionary<string,string> Update(IDictionary<string,TaxiState> states,ICollection<string> available,IEnumerable<Pin> pins,Dictionary<string,string> observations,Dictionary<string,string> last){
   foreach(var p in observations){
    if(!pins.Any(x=>x.Name==p.Key&&TaxiMarker.IsTaxi(x)))continue;
    var state=(TaxiState)Enum.Parse(typeof(TaxiState),p.Value);string was;
    if(state!=TaxiState.Unknown&&last!=null&&last.TryGetValue(p.Key,out was)&&was==p.Value){
     TaxiState now;if(!states.TryGetValue(p.Key,out now)||now!=TaxiState.Gone)states[p.Key]=state;}
   }
   // Departure is terminal for this raid; a failed read or white/green false match cannot resurrect a car.
   foreach(var p in states)if(p.Value==TaxiState.Gone)available.Remove(p.Key);
   return observations;
  }
 }
 // Windows OCR without PowerShell's reflection: WinRT async operations bridged to tasks (no System.Runtime.WindowsRuntime
 // facade, so only the system's own WinMetadata is needed to build). Results are copied out as plain text and boxes.
 public sealed class OcrWord{public string Text;public RectangleF Box;}
 public sealed class OcrLine{public string Text;public List<OcrWord> Words=new List<OcrWord>();}
 public sealed class OcrText{public string Text="";public List<OcrLine> Lines=new List<OcrLine>();}
 public static class WinOcr {
  public static Task<T> Run<T>(Windows.Foundation.IAsyncOperation<T> op){
   var t=new TaskCompletionSource<T>();
   op.Completed=delegate(Windows.Foundation.IAsyncOperation<T> o,Windows.Foundation.AsyncStatus s){
    try{if(s==Windows.Foundation.AsyncStatus.Completed)t.TrySetResult(o.GetResults());else if(s==Windows.Foundation.AsyncStatus.Canceled)t.TrySetCanceled();else t.TrySetException(o.ErrorCode??new Exception("OCR"));}catch(Exception ex){t.TrySetException(ex);}};
   return t.Task;
  }
  public static OcrEngine Engine(string tag){
   if(string.IsNullOrEmpty(tag))return null;
   foreach(var l in OcrEngine.AvailableRecognizerLanguages)if(l.LanguageTag==tag||l.LanguageTag.StartsWith(tag+"-"))return OcrEngine.TryCreateFromLanguage(l);
   return null;
  }
  public static async Task<OcrText> Read(OcrEngine engine,byte[] png){
   using(var stream=new Windows.Storage.Streams.InMemoryRandomAccessStream()){
    using(var writer=new Windows.Storage.Streams.DataWriter(stream)){writer.WriteBytes(png);await Run(writer.StoreAsync());writer.DetachStream();}
    stream.Seek(0);var decoder=await Run(Windows.Graphics.Imaging.BitmapDecoder.CreateAsync(stream));
    using(var bitmap=await Run(decoder.GetSoftwareBitmapAsync())){
     var r=await Run(engine.RecognizeAsync(bitmap));var o=new OcrText{Text=r.Text??""};
     foreach(var line in r.Lines){var l=new OcrLine{Text=line.Text??""};foreach(var w in line.Words){var b=w.BoundingRect;l.Words.Add(new OcrWord{Text=w.Text??"",Box=new RectangleF((float)b.X,(float)b.Y,(float)b.Width,(float)b.Height)});}o.Lines.Add(l);}
     return o;
    }
   }
  }
  // Synchronous read for a worker loop that keeps pumping its windows meanwhile (the price helper).
  public static OcrText ReadPumping(OcrEngine engine,byte[] png,Action pump,int timeoutSeconds){
   var task=Task.Run(()=>Read(engine,png));var watch=System.Diagnostics.Stopwatch.StartNew();
   while(!task.IsCompleted){pump();System.Threading.Thread.Sleep(5);if(watch.Elapsed.TotalSeconds>timeoutSeconds)throw new Exception(Language.T("처리 시간 초과"));}
   return task.GetAwaiter().GetResult();
  }
 }
}
