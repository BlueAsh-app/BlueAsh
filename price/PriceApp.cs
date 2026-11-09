using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32;
using ModePulse;
using Windows.Media.Ocr;
namespace ModeSwitchPrice {
 // Game-log helpers shared by the price worker and the hideout window (was price/PriceCore.ps1).
 public static class PriceLog {
  // Last "Session mode:" line in the game's application log. Each mode keeps its own endpoint.
  public static string LogMode(string text){
   var m=Regex.Matches(text??"",@"\|application\|Session mode: (\w+)");if(m.Count==0)return "";
   switch(m[m.Count-1].Groups[1].Value){case "Pve":return "pve";case "PvpSeason":return "pvp-season";case "Regular":return "regular";}return "";
  }
  public static string ReadLogMode(string root){
   if(string.IsNullOrEmpty(root)||!Directory.Exists(root))return "";
   var dir=new DirectoryInfo(root).GetDirectories("log_*").OrderByDescending(d=>d.LastWriteTimeUtc).FirstOrDefault();if(dir==null)return "";
   foreach(var f in dir.GetFiles("*application*.log").OrderByDescending(f=>f.Name,StringComparer.OrdinalIgnoreCase)){
    if(f.Length>16777216)continue;
    // The game keeps the log open for writing; share access instead of locking it.
    string text;using(var s=new FileStream(f.FullName,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))using(var r=new StreamReader(s,Encoding.UTF8))text=r.ReadToEnd();
    var mode=LogMode(text);if(mode.Length>0)return mode;
   }
   return "";
  }
  // raid-state.txt from BlueAsh.exe: pid, map, role, raid id, phase, serial, tick. 'started' = inside a raid.
  public static string RaidPhase(string path,int parentId){
   try{var s=File.ReadAllLines(path,Encoding.UTF8);if(s.Length>=5&&s[0]==parentId.ToString(CultureInfo.InvariantCulture))return s[4];}catch{}return "unknown";
  }
  public static string PriceState(Item item,string mode){
   if(item==null||item.Mode!=mode)return "invalid";
   if(item.Banned)return "banned";
   if(item.Offers.HasValue&&item.Offers.Value==0)return "no-price";
   if(!item.Price.HasValue||item.Price.Value<=0)return "no-price";
   return "price";
  }
 }
 // The price worker (was price/PriceWorker.ps1): runs while Tarkov runs; after an inspect click it reads the inspect
 // window's title, finds the item and shows the flea price, quest and hideout tags beside it (up to three windows).
 public sealed class PriceApp {
  static string P(string s){return Language.T(s);}
  static string P(string s,params object[] v){return Language.T(s,v);}
  sealed class Hit{public Item Item;public string Mode;public int Th,Right,Left,Bottom,Top,Start,Cx,Cy,R,W;public bool FullOnly,Old;}
  sealed class Slot{public PriceTag Price,Quest,Hide;public KeyButton Key;public Hit Shown;public long At,Next;public int Z;public bool Covered;}
  sealed class Job{public long At;public string Kind;public int X,Y;public bool Dbl,Deep;public Slot Slot;}
  readonly int parentId;readonly Process parent;readonly string store,statusPath,missPath,levelsPath,keysPath,raidState,gameIni;string logRoot,gameLang,paddleModel;
  OcrEngine main,eng,alt;PaddleRec paddle;bool paddleOff;System.Threading.Tasks.Task<PaddleRec> paddleLoad;
  readonly Dictionary<string,Catalog> catalogs=new Dictionary<string,Catalog>();readonly Dictionary<string,TaskIndex> tasks=new Dictionary<string,TaskIndex>();readonly Dictionary<string,QuestHistory> histories=new Dictionary<string,QuestHistory>();
  readonly TimeSpan refreshAge=TimeSpan.FromMinutes(10),taskAge=TimeSpan.FromHours(6);
  readonly List<Slot> slots=new List<Slot>();int zc;int lastBars;DateTime missFrameAt=DateTime.MinValue,editionAt=DateTime.MinValue;HashSet<string> editionSkip;
  readonly Dictionary<string,DateTime> wipeAt=new Dictionary<string,DateTime>();long levelsStamp=-1;readonly Dictionary<string,Dictionary<string,int>> levels=new Dictionary<string,Dictionary<string,int>>();
  readonly Dictionary<string,string> lastCatalogErrors=new Dictionary<string,string>();

  public PriceApp(int parentId){
   this.parentId=parentId;parent=Process.GetProcessById(parentId);
   store=DataDir.Folder;Directory.CreateDirectory(store);
   statusPath=Path.Combine(store,"price-status.txt");missPath=Path.Combine(store,"price-misses.log");levelsPath=Path.Combine(store,"hideout-levels.json");keysPath=Path.Combine(store,"my-keys.json");raidState=Path.Combine(store,"raid-state.txt");
   gameIni=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Battlestate Games","Escape from Tarkov","Settings","Game.ini");
  }
  void Status(string s){File.WriteAllText(statusPath,s,new UTF8Encoding(true));}
  void Pump(){Application.DoEvents();if(parent.HasExited)throw new Exception(P("BlueAsh 종료"));}
  string Ocr(byte[] bytes,OcrEngine engine){if(engine==null)return "";return WinOcr.ReadPumping(engine,bytes,Pump,25).Text;}
  // Titles are read first by BlueAsh's own PaddleOCR model for this language (downloaded once in the background from
  // BlueAsh's server, nothing for the user to install); Windows OCR is the second reader and the fallback until then.
  PaddleRec Paddle(){
   if(paddle!=null||paddleOff||string.IsNullOrEmpty(paddleModel))return paddle;
   if(paddleLoad==null){var ocrRoot=OcrModels.Ready(store,paddleModel);if(string.IsNullOrEmpty(ocrRoot))return null;paddleLoad=PaddleRec.LoadAsync(ocrRoot,paddleModel);return null;}
   if(!paddleLoad.IsCompleted)return null;
   if(paddleLoad.IsFaulted){paddleOff=true;LogMiss("paddle-error",new[]{paddleLoad.Exception.GetBaseException().Message});}else paddle=paddleLoad.Result;return paddle;
  }
  // Titles read but not found, newest last (kept to 200 lines) so real misses can be diagnosed later.
  void LogMiss(string mode,IEnumerable<string> texts){
   var t=texts.Where(x=>x!=null&&Regex.IsMatch(x,"[A-Za-z가-힣]")).ToList();if(t.Count==0)return;
   try{var line=DateTime.Now.ToString("MM-dd HH:mm:ss")+"\t"+mode+"\t"+string.Join(" | ",t.Select(x=>Regex.Replace(x,@"\s+"," ")));
    var old=File.Exists(missPath)?File.ReadAllLines(missPath,Encoding.UTF8).ToList():new List<string>();if(old.Count>199)old=old.Skip(old.Count-199).ToList();old.Add(line);
    File.WriteAllLines(missPath,old.ToArray(),new UTF8Encoding(false));}catch{}
  }
  // Stash already maxed by the game edition (Unheard / Edge of Darkness): its upgrade items are not listed.
  HashSet<string> EditionSkip(){var now=DateTime.UtcNow;if(now>=editionAt){editionAt=now.AddMinutes(1);var set=new HashSet<string>();if(TaskIndex.StashMaxedByEdition(gameIni))set.Add(TaskIndex.StashId);if(TaskIndex.CultistByEdition(gameIni))set.Add(TaskIndex.CultistId);editionSkip=set;}return editionSkip;}
  // Every minute per mode: a prestige / season wipe in the logs after the levels were saved clears them (the file then changes and is read again).
  Dictionary<string,int> HideoutLevels(string mode){
   var now=DateTime.UtcNow;DateTime at;if(!wipeAt.TryGetValue(mode,out at)||now>=at){wipeAt[mode]=now.AddMinutes(1);try{TaskIndex.ClearIfOlder(levelsPath,mode,histories[mode].ResetAt);}catch{}}
   long t=File.Exists(levelsPath)?File.GetLastWriteTimeUtc(levelsPath).Ticks:0;if(t!=levelsStamp){levelsStamp=t;levels.Clear();}
   Dictionary<string,int> l;if(!levels.TryGetValue(mode,out l)){l=TaskIndex.ReadLevels(levelsPath,mode);levels[mode]=l;}return l;
  }
  static string N0(double v){return v.ToString("N0",Language.Culture);}
  // Yellow "현재 시세 : 8,500루블" in the bottom-right corner of the inspect window's picture panel.
  void ShowPrice(Hit hit,Slot slot){
   if(slot.Z!=0&&Covered(slot)){slot.Price.Hide();slot.Key.Hide();slot.Covered=true;return;}
   var item=hit.Item;var mode=hit.Mode;var state=PriceLog.PriceState(item,mode);
   if(state=="invalid")throw new Exception(P("가격 데이터 모드 불일치"));
   var modeLabel=mode=="pve"?"PvE":mode=="pvp-season"?P("PvP S · 시즌"):mode=="regular"?"PvP":null;string line;Color color;
   // A range appears only for several items sharing this exact name with clearly different prices.
   if(state=="price"){var value=N0(item.Price.Value);if(item.PriceMax.HasValue&&item.PriceMax.Value!=0)value+="~"+N0(item.PriceMax.Value);line=P("현재 시세 : {0}루블",value);color=Color.FromArgb(255,214,64);}
   else if(state=="banned"){line=P("플리마켓 판매 금지");color=Color.FromArgb(235,96,80);}
   else{line=P("플리마켓 물량 없음");color=Color.FromArgb(205,205,205);}
   // No flea number (banned or no offers): the best trader buy-back price instead, so a number is always shown.
   if(state!="price"&&item.TraderPrice.HasValue&&item.TraderPrice.Value>0){var who=catalogs[mode].TraderName(item.TraderId);var tp=N0(item.TraderPrice.Value);
    line+=" · "+(!string.IsNullOrEmpty(who)?P("상인 {0}루블 ({1})",tp,who):P("상인 {0}루블",tp));}
   int th=hit.Th;slot.Price.ShowAt(line,color,(int)(hit.Right-th*0.45),(int)(hit.Bottom-th*0.3),(int)(th*0.55));
   // Keys and keycards: a button at the picture's top-left puts the key on the map (or takes it off): the green
   // "✓ 지도에 등록됨" shows at a glance that this key is already on the map.
   if(MyKeys.IsKey(item))slot.Key.ShowFor(keysPath,mode,item.Id,item.Name,(int)(hit.Left+th*0.4),(int)(hit.Top+th*1.35),(int)(th*0.42));else slot.Key.Hide();
   // Optional quest/hideout tags follow only after the background history snapshot is ready.
   var history=histories[mode];history.RefreshIfStale();
   var tq=tasks[mode];tq.RefreshIfStale(taskAge);
   if(history.Ready){
    var ids=!string.IsNullOrEmpty(item.Ids)?item.Ids:item.Id;
    // Quest use (left, orange): which quests need this item or key; nothing when none.
    var q=tq.For(ids,history.Completed);
    if(q.Length>0)slot.Quest.ShowLines(q,Color.FromArgb(236,168,84),(int)(hit.Left+th*1.35),(int)(hit.Bottom-th*0.3),(int)(th*0.45));else slot.Quest.Hide();
    // Hideout upgrades (centre, light blue): "발전기 V2 (3개 필요)".
    var hz=tq.Hideout(ids,HideoutLevels(mode),EditionSkip());
    if(hz!=null&&hz.Length>0)slot.Hide.ShowLines(hz.Select(h=>h.Text).ToArray(),hz.Select(h=>h.Now?Color.FromArgb(130,192,236):Color.FromArgb(120,128,140)).ToArray(),(hit.Left+hit.Right)/2,(int)(hit.Bottom-th*0.3),(int)(th*0.45),true);else slot.Hide.Hide();
   }else{slot.Quest.Hide();slot.Hide.Hide();}
   Status(modeLabel+" · "+catalogs[mode].DisplayName(item)+" · "+line+" · tarkov.dev "+item.Updated);
  }
  // Name lookup; the magnifier icon left of the title often reads as a stray letter ("P Military cable ...").
  static Item FindTitle(Catalog catalog,string text){var item=catalog.Find(text);if(item==null){var m=Regex.Match(text??"",@"^\S{1,2}\s+(.+)$");if(m.Success)item=catalog.Find(m.Groups[1].Value);}return item;}
  // One capture: find inspect title bars, read the one nearest the click first, stop at the first catalog hit.
  // deep: the costly second chances (category line, binarized re-read, English OCR) only on the later attempts;
  // the early ones are quick so a slowly drawn window is caught as soon as it is readable.
  Hit Scan(IntPtr game,int clickX,int clickY,List<Hit> skip,bool deep){
   Bitmap frame=null;Hit old=null;
   try{
    var mode="";try{mode=PriceLog.ReadLogMode(logRoot);}catch{}
    frame=Desktop.Grab(game);
    if(mode.Length==0){
     // Mode footer: lower-left 40% width / 4% height, enlarged for small grey text.
     var modeBytes=Desktop.CropPng(frame,0,(int)(frame.Height*.96),(int)(frame.Width*.40),(int)(frame.Height*.04),2);
     mode=Core.Mode(Ocr(modeBytes,eng));
    }
    if(string.IsNullOrEmpty(mode)){Status(P("게임 모드(PvE·PvP)를 알 수 없습니다 · 레이드 시작 알림 설정에서 로그 폴더를 확인하세요."));return null;}
    var catalog=catalogs[mode];catalog.RefreshIfStale(refreshAge);
    int w,h;var pixels=Desktop.Sample(frame,2048,out w,out h);
    var bars=Core.TitleBars(pixels,w,h).ToList();pixels=null;lastBars=bars.Count;
    if(bars.Count==0)return null;
    if(!catalog.Ready){Status(P("{0} · 시세 데이터를 받는 중입니다. 잠시 뒤 다시 조사해 주세요.",mode));return null;}
    double f=(double)frame.Width/w;var bounds=Desktop.Bounds(game);double cx=(clickX-bounds.X)/f,cy=(clickY-bounds.Y)/f;
    bars=bars.OrderBy(b=>Math.Pow(b.X+b.W/2.0-cx,2)+Math.Pow(b.Y+b.H/2.0-cy,2)).ToList();
    var texts=new List<string>();
    foreach(var bar in bars){
     // Only the grey title strip (about 2/3 of the detected band); the line below it confuses OCR.
     var png=Desktop.TitlePng(frame,(int)(bar.X*f),(int)(bar.Y*f),(int)(bar.W*f),(int)(bar.H*f*0.67));
     Item item=null;string text="";var pr=Paddle();
     if(pr!=null){try{double conf;text=pr.Recognize(frame,new Rectangle((int)(bar.X*f),(int)(bar.Y*f),(int)(bar.W*f),(int)(bar.H*f*0.67)),out conf);if(!string.IsNullOrEmpty(text)){texts.Add("["+paddleModel.Split('_')[0]+"] "+text);item=FindTitle(catalog,text);}}catch(Exception ex){LogMiss("paddle-error",new[]{ex.Message});}}
     if(item==null&&(deep||pr==null)){var w2=Ocr(png,main);if(!string.IsNullOrEmpty(w2)){texts.Add(w2);item=FindTitle(catalog,w2);text=w2;}}
     if(string.IsNullOrEmpty(text))continue;
     if(item==null&&deep&&alt!=null){var t2=Ocr(png,alt);if(!string.IsNullOrEmpty(t2)){texts.Add(t2);item=FindTitle(catalog,t2);}}
     // Any reading with a letter or digit gets the second chance: garbled titles like '入 타 p F-I 丁五' have no two in a row.
     bool readable=Regex.IsMatch(text,"[A-Za-z가-힣0-9]",RegexOptions.IgnoreCase);
     if(item==null&&!readable)texts.Add(P("(2차 건너뜀)"));
     if(item==null&&deep&&readable){
      // Second chance, only on a miss (costs one or two more OCR passes, ~20-100 ms each): read the category
      // line under the title ("Weapons > Throwables") and match within that category; then a binarized re-read.
      int bx=(int)(bar.X*f),by=(int)(bar.Y*f),bw=(int)(bar.W*f),bh=(int)(bar.H*f);string category="",again="";
      try{
       category=Ocr(Desktop.TextPng(frame,bx,by+(int)(bh*0.8),(int)(bw*0.6),(int)(bh*0.75),330,2,true),eng);
       item=catalog.FindInCategory(text,category);
       if(item==null){again=Ocr(Desktop.TextPng(frame,bx,by,bw,(int)(bh*0.67),450,3,true),main);
        if(!string.IsNullOrEmpty(again)){texts.Add(again);item=FindTitle(catalog,again);if(item==null)item=catalog.FindInCategory(again,category);}}
      }catch(Exception ex){LogMiss("ocr-error",new[]{ex.Message});texts.Add(P("2차 오류: {0}",Language.Error(ex)));}
      if(item==null){texts.Add("["+(category??"").Trim()+"] bar "+bx+","+by+" "+bw+"x"+bh);
       // Keep what was actually read (title + category line, game pixels) for diagnosing misses: price-miss-debug.png.
       try{var dbg=Desktop.CropPng(frame,bx-(int)(bh*0.8),by-(int)(bh*0.3),bw+(int)(bh*2.5),(int)(bh*2.4),1);File.WriteAllBytes(Path.Combine(store,"price-miss-debug.png"),dbg);}catch{}}
     }
     if(item!=null){
      // Window geometry in frame pixels: right edge = title text end + close button; panel bottom = top of the button row.
      int th=(int)(bar.H*f),top=(int)(bar.Y*f),right=(int)((bar.X+bar.W)*f+th*1.07);
      int bottom=Desktop.PanelBottom(frame,right,top+th,th);if(bottom<0)bottom=top+(int)(th*10.8);
      // Close button: TitleBars ends the title 5 sampled px before the red button starts; centre is ~1/3 bar height in.
      int start=bounds.X+(int)((bar.X+bar.W+5)*f);
      var hit=new Hit{Item=item,Mode=mode,Th=th,Right=bounds.X+right,Left=bounds.X+(int)(bar.X*f),Bottom=bounds.Y+bottom,Top=bounds.Y+top,Start=start,
       Cx=start+(int)(th*0.33),Cy=bounds.Y+top+(int)(th*0.33),R=Math.Max(6,(int)(th*0.2)),W=(int)(bar.W*f)};
      // A window whose price is already shown (same item, same place): look for the newly opened one first.
      bool isOld=false;if(skip!=null)foreach(var o in skip)if(o!=null&&item.Id==o.Item.Id&&Math.Abs(hit.Top-o.Top)<=o.Th&&Math.Abs(start-o.Start)<=o.Th)isOld=true;
      if(isOld){hit.Old=true;old=hit;continue;}
      return hit;}
    }
    if(old!=null)return old;
    // Early quick attempts often see a window still being drawn: only the later (deep) ones report a miss.
    if(deep){Status(P("{0} · 아이템 이름을 읽지 못했습니다: {1}",mode,string.Join(" / ",texts)));if(texts.Count>0)LogMiss(mode,texts);}return null;
   }finally{if(frame!=null)frame.Dispose();}
  }
  // Full search: is an inspect title bar still at the shown window's place (within one bar height)?
  // Returns the close-button centre there, or a drag offset (Moved) for the same title elsewhere, or null.
  sealed class Found{public int Cx,Cy,Dx,Dy;public bool Moved;}
  Found FindWindow(IntPtr game,Hit shown,out int count){
   Bitmap frame=null;count=0;
   try{
    frame=Desktop.Grab(game);int w,h;var px=Desktop.Sample(frame,2048,out w,out h);double f=(double)frame.Width/w;var bounds=Desktop.Bounds(game);
    var bars=Core.TitleBars(px,w,h).ToList();count=bars.Count;Found moved=null;
    foreach(var bar in bars){
     int th=(int)(bar.H*f),top=bounds.Y+(int)(bar.Y*f),start=bounds.X+(int)((bar.X+bar.W+5)*f);
     if(Math.Abs(top-shown.Top)<=shown.Th&&Math.Abs(start-shown.Start)<=shown.Th)return new Found{Cx=start+(int)(th*0.33),Cy=top+(int)(th*0.33)};
     // Dragged window: the same title (same bar width and height) elsewhere. No OCR needed, the tags just move.
     if(moved==null&&shown.W!=0&&Math.Abs((int)(bar.W*f)-shown.W)<=(int)(f*1.5)+1&&Math.Abs(th-shown.Th)<=(int)f+1)moved=new Found{Moved=true,Dx=start-shown.Start,Dy=top-shown.Top};
    }
    return moved;
   }finally{if(frame!=null)frame.Dispose();}
  }
  // Nothing recognised after an inspect click: the screen (reduced to 1600 px) goes to price-miss-frame.jpg, with a
  // log line, at most every 20 s. Local only; it shows what the title-bar search or OCR missed.
  void SaveMissFrame(IntPtr game,string why){
   var now=DateTime.UtcNow;if(now<missFrameAt)return;missFrameAt=now.AddSeconds(20);
   Bitmap frame=null,small=null;
   try{frame=Desktop.Grab(game);double kk=Math.Min(1.0,1600.0/frame.Width);small=new Bitmap(frame,(int)(frame.Width*kk),(int)(frame.Height*kk));
    small.Save(Path.Combine(store,"price-miss-frame.jpg"),System.Drawing.Imaging.ImageFormat.Jpeg);LogMiss("inspect",new[]{why});}catch{}finally{if(small!=null)small.Dispose();if(frame!=null)frame.Dispose();}
  }
  // The quick close check misjudged an open window: keep the evidence (grab around the button) for diagnosis.
  void LogCloseMiss(Hit shown,Found again){
   try{Desktop.SaveGrab(shown.Cx-4*shown.R,shown.Cy-2*shown.R,6*shown.R,4*shown.R,Path.Combine(store,"price-close-debug.png"));
    LogMiss("close-check",new[]{P("{0} 기대 위치 {1},{2} → 실제 {3},{4}",shown.Item.Name,shown.Cx,shown.Cy,again.Cx,again.Cy)});}catch{}
  }
  // Overlapping inspect windows: the one opened or clicked last is on top (the game raises a clicked window).
  // A window under another one that overlaps it shows no tags (they would sit on top of the upper window) until it is uncovered.
  static Rectangle WindowRect(Hit h){return Rectangle.FromLTRB(h.Left-(int)(h.Th*0.3),h.Top,h.Right,h.Bottom+(int)(h.Th*1.2));}
  bool Covered(Slot s){if(s.Shown==null)return false;var a=WindowRect(s.Shown);foreach(var o in slots)if(o!=s&&o.Shown!=null&&o.Z>s.Z&&a.IntersectsWith(WindowRect(o.Shown)))return true;return false;}
  void UpdateCovers(){foreach(var s in slots){if(s.Shown==null)continue;bool c=Covered(s);if(c!=s.Covered){if(c){s.Price.Hide();s.Key.Hide();s.Covered=true;}else{s.Covered=false;ShowPrice(s.Shown,s);}}}}
  void RaiseAt(int x,int y){Slot best=null;foreach(var s in slots)if(s.Shown!=null&&WindowRect(s.Shown).Contains(x,y)&&(best==null||s.Z>best.Z))best=s;
   if(best!=null){int top=0;foreach(var s in slots)if(s.Shown!=null&&s.Z>top)top=s.Z;if(best.Z<top){zc++;best.Z=zc;UpdateCovers();}}}
  static bool SamePlace(Hit a,Hit b){return Math.Abs(a.Top-b.Top)<=a.Th&&Math.Abs(a.Start-b.Start)<=a.Th;}
  void HideSlot(Slot s){s.Price.Hide();s.Key.Hide();s.Shown=null;s.Covered=false;if(zc!=0)UpdateCovers();}
  List<Hit> ShownHits(){return slots.Where(s=>s.Shown!=null).Select(s=>s.Shown).ToList();}
  // The window's own slot (same place: a new window opened over an old one replaces it), else a free one, else the oldest.
  void Place(Hit hit,long now){
   Slot target=slots.FirstOrDefault(s=>s.Shown!=null&&SamePlace(s.Shown,hit))??slots.FirstOrDefault(s=>s.Shown==null)??slots.OrderBy(s=>s.At).First();
   zc++;target.Z=zc;target.Covered=false;target.Shown=hit;target.At=now;target.Next=now+400;ShowPrice(hit,target);UpdateCovers();
  }

  public int Run(){
   try{
    Language.Reload();
    Status(P("켜는 중 · 글자 인식(OCR)을 준비하고 있습니다."));
    // Titles are read in the game's own text language (Game.ini "Language"), matched against names in that language.
    gameLang=GameLanguage.Api(gameIni);
    main=WinOcr.Engine(GameLanguage.OcrTag(gameLang));eng=WinOcr.Engine("en");
    paddleModel=OcrModels.ModelFor(gameLang);
    OcrModels.Ready(store,paddleModel);
    // Only a language with no BlueAsh model and no Windows OCR is offered for install (ocr-missing.txt -> BlueAsh's one-click
    // install); a missing English pack is no longer asked for.
    var missing=new List<string>();if(main==null&&string.IsNullOrEmpty(paddleModel))missing.Add(GameLanguage.Capability(gameLang)+"\t"+gameLang);
    try{File.WriteAllLines(Path.Combine(store,"ocr-missing.txt"),missing.ToArray(),new UTF8Encoding(false));}catch{}
    if(main==null)main=eng;if(main==null)main=OcrEngine.TryCreateFromUserProfileLanguages();
    if(main==null&&string.IsNullOrEmpty(paddleModel))throw new Exception(P("Windows 글자 인식(OCR) 언어가 없습니다. BlueAsh 알림을 눌러 설치하세요."));
    if(eng==null)eng=main;
    // Second reader for titles the game-language OCR misses (English model names inside Korean titles, etc.).
    alt=ReferenceEquals(eng,main)?null:eng;
    logRoot=Convert.ToString(Registry.GetValue(@"HKEY_CURRENT_USER\Software\ModeSwitch","RaidLogs",""))??"";
    // Price tables: disk cache is usable at once; downloads run on a background thread every 10 minutes.
    foreach(var m in Catalog.Modes){var c=new Catalog(m,store,gameLang);c.LoadCache();catalogs[m]=c;}
    var mode="";try{mode=PriceLog.ReadLogMode(logRoot);}catch{}if(mode.Length>0)catalogs[mode].RefreshIfStale(refreshAge);
    // Quest needs per item: cached, refreshed every 6 h in the background.
    foreach(var m in Catalog.Modes){var t=new TaskIndex(m,store);t.LoadCache();tasks[m]=t;}if(mode.Length>0)tasks[mode].RefreshIfStale(taskAge);
    // Completed quests/reset history is read in the background, at most every 30 s per mode.
    Quests.CacheFile=Path.Combine(store,"quest-log-index.json");
    foreach(var m in Catalog.Modes)histories[m]=new QuestHistory(logRoot,m);
    if(mode.Length>0)histories[mode].RefreshIfStale();
    // Up to three inspect windows at once (compare two items side by side): each slot has its own price / quest /
    // hideout tags, close check and drag follow. A new inspect click keeps the others and looks for the new window.
    for(int i=0;i<3;i++){var p=new PriceTag();var qt=new PriceTag();var ht=new PriceTag();p.Linked.Add(qt);p.Linked.Add(ht);slots.Add(new Slot{Price=p,Quest=qt,Hide=ht,Key=new KeyButton()});}
    Loop();
    return 0;
   }catch(Exception ex){if(!parent.HasExited){LogMiss("error",new[]{ex.Message});Status(P("시세 인식 오류: {0}",Language.Error(ex)));}return parent.HasExited?0:1;}
   finally{foreach(var s in slots)foreach(var t in new Form[]{s.Price,s.Quest,s.Hide,s.Key})if(t!=null)t.Dispose();}
  }
  void Loop(){
   var trigger=new ClickTrigger();trigger.Reset();
   var clock=trigger.Clock;var game=IntPtr.Zero;long nextGame=0,nextPhase=0;var phase="unknown";
   // Scheduled captures: inspect (find the newly opened window), relocate (a slot's window moved or was covered), restore (after Alt+Tab).
   var jobs=new List<Job>();var lost=new List<Hit>();long lostUntil=0;
   Status(P("켜짐 · 아이템을 우클릭 → 조사하면 시세를 띄웁니다."));
   while(true){
    // This loop wakes about 4x/s; the 20 ms mouse sampling happens inside ClickTrigger.Wait.
    Pump();long now=clock.ElapsedMilliseconds;if(paddle==null)Paddle();
    var historyChanged=new HashSet<string>();foreach(var p in histories)if(p.Value.Poll())historyChanged.Add(p.Key);
    if(now>=nextGame){nextGame=now+250;var was=game;game=Desktop.Game();
     if(game==IntPtr.Zero){var hits=ShownHits();if(hits.Count>0){lost=hits;lostUntil=now+600000;}foreach(var s in slots)if(s.Shown!=null)HideSlot(s);jobs.Clear();trigger.Reset();}
     else if(was==IntPtr.Zero&&lost.Count>0){if(now<lostUntil)foreach(var l in lost)foreach(var d in new[]{400,1000})jobs.Add(new Job{At=now+d,Kind="restore",X=(l.Left+l.Right)/2,Y=l.Top});lost=new List<Hit>();}}
    if(game==IntPtr.Zero){System.Threading.Thread.Sleep(250);continue;}
    if(now>=nextPhase){nextPhase=now+1000;phase=PriceLog.RaidPhase(raidState,parentId);}
    // Closing the inventory (Tab) or a window gives no click to react to: watch each window's close button.
    // A failed quick check is confirmed with a full title-bar search: a moved window is followed, a covered or
    // replaced one is read again where it was, and only when no title bar is left is the slot hidden.
    foreach(var s in slots){var shown=s.Shown;if(shown==null||now<s.Next)continue;
     s.Next=now+(shown.FullOnly?600:250);
     if(!shown.FullOnly&&Desktop.StillOpen(shown.Cx,shown.Cy,shown.R))continue;
     Found again=null;int bars=0;try{again=FindWindow(game,shown,out bars);}catch{}
     if(again!=null&&again.Moved){
      shown.Left+=again.Dx;shown.Right+=again.Dx;shown.Start+=again.Dx;shown.Cx+=again.Dx;shown.Top+=again.Dy;shown.Bottom+=again.Dy;shown.Cy+=again.Dy;
      shown.FullOnly=false;ShowPrice(shown,s);UpdateCovers();
     }else if(again!=null){
      if(!shown.FullOnly)LogCloseMiss(shown,again);
      shown.Cx=again.Cx;shown.Cy=again.Cy;shown.FullOnly=!Desktop.StillOpen(shown.Cx,shown.Cy,shown.R);
     }else if(bars>0){jobs.Add(new Job{At=now,Kind="relocate",Slot=s,X=(shown.Left+shown.Right)/2,Y=shown.Top});s.Next=now+600;}
     else HideSlot(s);}
    foreach(var s in slots)if(s.Shown!=null&&historyChanged.Contains(s.Shown.Mode))ShowPrice(s.Shown,s);
    // Wait for a click no longer than the next job or close check (at most 250 ms).
    long wait=250;foreach(var j in jobs)wait=Math.Min(wait,j.At-now);foreach(var s in slots)if(s.Shown!=null)wait=Math.Min(wait,s.Next-now);
    int ev=trigger.Wait(phase=="started",(int)Math.Max(0,wait));now=clock.ElapsedMilliseconds;
    if(ev==ClickTrigger.Inspect){
     // Inspect window appears a few frames after the menu click; slow first-time model loads can delay it:
     // keep trying up to 2.8 s, stopping at the first window that is not already shown.
     jobs.RemoveAll(j=>j.Kind=="inspect");
     bool dbl=trigger.Double;foreach(var d in new[]{90,150,300,450,650,900,1200,1600,2200,2800})jobs.Add(new Job{At=now+d,Kind="inspect",X=trigger.X,Y=trigger.Y,Dbl=dbl,Deep=d>=900});
    }else if(ev==ClickTrigger.Click){
     // A click may have closed a window: check every shown one soon. It also brings the clicked window to the front.
     RaiseAt(trigger.X,trigger.Y);
     foreach(var s in slots)if(s.Shown!=null)s.Next=Math.Min(s.Next,now+250);
    }else if(ev==ClickTrigger.Escape){foreach(var s in slots)if(s.Shown!=null)HideSlot(s);jobs.Clear();}
    Job due=null;foreach(var j in jobs)if(j.At<=now&&(due==null||j.At<due.At))due=j;if(due==null)continue;
    jobs.Remove(due);
    try{
     if(due.Kind=="relocate"){
      var hit=Scan(game,due.X,due.Y,null,true);
      if(Desktop.Game()!=game)continue;
      var s=due.Slot;if(s.Shown==null)continue;
      // The window there now belongs to another slot (this one was covered): drop this slot; otherwise follow.
      bool other=hit!=null&&slots.Any(o=>o!=s&&o.Shown!=null&&SamePlace(o.Shown,hit));
      if(hit==null||other)HideSlot(s);else{s.Shown=hit;s.Next=now+400;ShowPrice(hit,s);}
      continue;
     }
     var found=Scan(game,due.X,due.Y,ShownHits(),due.Kind!="inspect"||due.Deep);
     if(Desktop.Game()!=game){jobs.Clear();continue;}
     // A double-click on a rig, backpack or case (items with their own grid) opens that container's window, which has
     // the same title bar: no price for it.
     if(found!=null&&!found.Old&&due.Dbl&&found.Item.Grid){jobs.RemoveAll(j=>j.Kind==due.Kind);continue;}
     if(found!=null&&!found.Old){jobs.RemoveAll(j=>j.Kind==due.Kind);Place(found,now);}
     else if(due.Kind=="inspect"&&!jobs.Any(j=>j.Kind=="inspect"))SaveMissFrame(game,lastBars==0?P("조사 후 상세보기 제목줄을 찾지 못함"):P("조사 후 아이템 이름 인식 실패"));
    }catch(Exception ex){if(parent.HasExited)throw;LogMiss("error",new[]{ex.Message});Status(P("시세 인식 오류: {0}",Language.Error(ex)));jobs.Clear();}
    foreach(var c in catalogs.Values)if(!string.IsNullOrEmpty(c.Error)){string last;lastCatalogErrors.TryGetValue(c.Mode,out last);if(last!=c.ErrorDetail){LogMiss("refresh-error",new[]{c.ErrorDetail});lastCatalogErrors[c.Mode]=c.ErrorDetail;}Status(c.Error);}
   }
  }
 }
}
