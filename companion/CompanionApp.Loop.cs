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
namespace ModePulse {
 public sealed partial class CompanionApp {
  ExitBinding binding;string bindingStamp="";DateTime nextBindingCheck=DateTime.MinValue;Dictionary<int,TriggerState> triggerStates=new Dictionary<int,TriggerState>();
  bool mapsOn=true;DateTime mapsCheck=DateTime.MinValue,nextParentCheck=DateTime.MinValue,nextKeysCheck=DateTime.MinValue,nextStateCheck=DateTime.MinValue,fleaNext=DateTime.MinValue,fleaSentAt=DateTime.MinValue;DateTime? fleaSince,warmAt;
  string keysStamp,lastRaid="",lastSerial="";long stateStamp;sealed class Notice{public string Id;public DateTime Due;public uint Idle;}Notice pendingNotice;
  HashSet<string> announced=new HashSet<string>();List<string> announcedOrder=new List<string>();string sentFile;
  readonly List<FleaSales.Sale> fleaPending=new List<FleaSales.Sale>();
  Task<HttpResponseMessage> noticeTask;DateTime noticeNext=DateTime.MinValue;bool noticeInitialized;HashSet<string> noticeSeen=new HashSet<string>();

  public int Run(bool hidden,string page){
   BuildPages();BuildMapWindow();InitTransport();Theme.Apply(map);
   sentFile=Path.Combine(store,"discord-raids.json");
   try{if(File.Exists(sentFile))foreach(var id in Json.List(Json.Parse(File.ReadAllText(sentFile)))){var s=Json.Str(id);if(announced.Add(s))announcedOrder.Add(s);}}catch{}
   LoadMap();
   if(File.Exists(pagePath)){try{page=File.ReadAllText(pagePath).Trim();File.Delete(pagePath);}catch{}}
   if(!string.IsNullOrEmpty(page))OpenPage(page);else if(!hidden)mapForm.Show();
   Status(T("대기 중 · 레이드에서 탈출구 확인 키를 누르면 쓸 수 있는 탈출구가 지도에 초록으로 표시됩니다."));DStatus(config.discord?T("대기 중 · 알림을 지원하는 게임 중 자리를 비우면 알려 드립니다."):T("알림이 꺼져 있습니다. 받을 상황을 고르고 저장하세요."));
   // The quest card's browser (WebView2) starts hidden 15 s after the helper (it starts with the game), so the first card opens at once.
   warmAt=DateTime.UtcNow.AddSeconds(15);
   try{
    while(true){
     try{Iteration();}
     // One bad iteration (game focus change, OCR error...) must not kill the helper: report it and keep going.
     catch(ParentExit){throw;}
     catch(Exception ex){var m=T("도우미 오류 (계속 실행 중): ")+Message(ex);Status(m);DStatus(m);}
    }
   }catch(ParentExit){return 0;}
   finally{map.Dispose();mapForm.Dispose();discordForm.Dispose();http.Dispose();handler.Dispose();}
  }
  // The map settings and notifications pages, built and fitted at the system scale (also used by map_page_layout_tests).
  void BuildPages(){
   fontName=I18n.FontName;UiFont.Load(appExe,I18n.Language);
   k=Embed.Scale;uiFont=UiFont.New(fontName,10f);
   // The two pages match BlueAsh's own pages: 16 px text, 14 px hints, bold section headings, 704x590 inside the page box,
   // at the system scale (both processes are system-DPI aware).
   var px=GraphicsUnit.Pixel;pageFont=UiFont.New(fontName,16*k,FontStyle.Regular,px);hintFont=UiFont.New(fontName,14*k,FontStyle.Regular,px);sectionFont=UiFont.New(fontName,16*k,FontStyle.Bold,px);
   mapForm=NewPanel(T("BlueAsh · 인게임 중 지도 보기"));Embed.Identify(mapForm,"Map");
   discordForm=NewPanel(T("BlueAsh · 디스코드 알림"));Embed.Identify(discordForm,"Discord");
   BuildMapPage();BuildDiscordPage();
   Theme.Apply(mapForm);Theme.Apply(discordForm);
   // Same look as BlueAsh pages: graphite surfaces, bold headings, muted hints, cobalt primary actions.
   foreach(var f in new Form[]{mapForm,discordForm})f.BackColor=Theme.Panel;
   foreach(var c in sections)c.Font=sectionFont;foreach(var c in hints){c.Font=hintFont;c.ForeColor=Theme.Muted;}phoneLabel.Font=hintFont;
   foreach(var b in primary.Cast<Button>()){b.BackColor=Theme.Accent;b.FlatAppearance.BorderColor=Theme.Accent;}
   foreach(var f in new Form[]{mapForm,discordForm}){f.Scale(new SizeF(k,k));f.ClientSize=new Size(Px(704),Px(590));}
   FitPage(mapForm);FitNotifications();
   mapForm.ResumeLayout(true);discordForm.ResumeLayout(true);
  }
  public PageForm MapPage{get{return mapForm;}}
  public PageForm NotificationPage{get{return discordForm;}}
  // Pages only, for the layout test: no parent process, no watchers; settings from the given folder.
  public static CompanionApp PagesForTest(string root,string store){var app=new CompanionApp(0,root,store);app.BuildPages();return app;}
  sealed class ParentExit:Exception{}
  void Pump(){Application.DoEvents();NetworkPump();if(DateTime.UtcNow>=nextParentCheck){nextParentCheck=DateTime.UtcNow.AddSeconds(1);if(parent.HasExited)throw new ParentExit();}}
  // Item names for sale alerts: the price helper's catalogue for the current mode (price-<mode>.tsv: id TAB name ...).
  Dictionary<string,string> FleaNames(){var m=Quests.CurrentMode(questLogs);var f=Path.Combine(store,"price-"+m+".tsv");var h=new Dictionary<string,string>();if(File.Exists(f))foreach(var l in File.ReadLines(f)){var c=l.Split('\t');if(c.Length>=2&&c[0].Length==24)h[c[0]]=c[1];}return h;}
  static string Money(long n,string currency){var v=n.ToString("N0",CultureInfo.CurrentCulture);return currency=="₽"?v+"₽":currency+v;}

  void Iteration(){
   Pump();
   ReadScreenshots();
   // Menu entries in BlueAsh ask for a window by writing companion-page.txt (map / discord / hideout).
   if(File.Exists(pagePath)){try{var req=File.ReadAllText(pagePath).Trim();File.Delete(pagePath);OpenPage(req);}catch(Exception ex){Console.Error.WriteLine(ex);}}
   // Sleep until the map key / exit-check key changes, the next exit capture is due, or 250 ms pass.
   // The map switch lives in BlueAsh (게임별 기능 > 타르코프, registry MapEnabled), read every 2 s.
   if(DateTime.UtcNow>=mapsCheck){mapsCheck=DateTime.UtcNow.AddSeconds(2);mapsOn=Reg("MapEnabled")!="0";if(!mapsOn&&map.Visible)map.HideMap();}
   var codes=new List<int>();if(mapsOn){codes.Add(config.mapKey);if(binding!=null)foreach(var v in binding.Variants)codes.AddRange(v.Codes);}
   double waitMs=250;if(captures>0&&exitRead==null)waitMs=Math.Max(15,Math.Min(250,(pendingAt-DateTime.UtcNow).TotalMilliseconds));if(exitRead!=null||autoExits!=null||mapRead!=null||raidPhase=="loading")waitMs=Math.Min(waitMs,50); /* loading: catch GameStarted in raid-state.txt within 0.1 s */if(claimState!=null||sendTask!=null)waitMs=Math.Min(waitMs,100);
   Input.WaitKeys(codes.ToArray(),(int)waitMs);
   var now=DateTime.UtcNow;var game=Desktop.Game();
   // Pin edits in the map editor: save this map's full list as the user's own version.
   if(map.Canvas.Dirty){map.Canvas.Dirty=false;if(mapId.Length>0){pins[mapId]=map.Canvas.Pins.Where(p=>p.Kind=="exit"||p.Kind=="transit").Select(p=>new Dictionary<string,object>{{"Name",p.Name},{"X",p.X},{"Y",p.Y},{"Taxi",p.Taxi},{"Destination",p.Destination},{"Condition",p.Condition},{"Kind",p.Kind}}).ToList();SavePins();}}
   if(now>=nextBindingCheck){
    nextBindingCheck=now.AddSeconds(5);
    try{
     var path=EftControl.FindControlFile();var file=new FileInfo(path);
     if(file.Length>2097152)throw new Exception(T("Control.ini 크기 제한 초과"));
     var stamp=path+":"+file.LastWriteTimeUtc.Ticks+":"+file.Length;
     if(stamp!=bindingStamp||binding==null){
      binding=EftControl.ReadExitBinding(File.ReadAllText(path));bindingStamp=stamp;triggerStates.Clear();captures=0;
      bindingLabel.Text=binding.Label;bindingDetails.Text=T("게임 키 설정(Control.ini)에서 자동으로 읽습니다. 게임에서 키를 바꾸면 따라 바뀝니다.");
     }
    }catch(Exception ex){binding=null;triggerStates.Clear();captures=0;bindingLabel.Text=T("읽지 못함");bindingDetails.Text=T("게임 키 설정을 읽지 못했습니다: ")+ex.Message;}
   }
   if(mapsOn){
    // Presses come from a background key watcher: a tap while this loop reads the exit list is not lost.
    int key=config.mapKey;Input.Watch(new[]{key});bool edge=Input.TakePress(key);if(edge)mapKeyAt=now;
    bool conflict=binding!=null&&binding.Variants.Any(v=>v.Codes.Contains(key));
    if(edge&&!conflict){if(map.Visible){if(!map.Typing)map.HideMap();}else if(game!=IntPtr.Zero)RequestMapOpen(game);}
    if(game==IntPtr.Zero)triggerStates.Clear();else if(binding!=null){
     bool triggered=false;
     for(int i=0;i<binding.Variants.Length;i++){
      var v=binding.Variants[i];bool held=v.Codes.All(vk=>Input.GetAsyncKeyState(vk)<0);
      TriggerState st;if(!triggerStates.TryGetValue(i,out st)){st=new TriggerState();triggerStates[i]=st;}
      if(EftControl.UpdateTrigger(st,held,now.Ticks/10000.0,binding))triggered=true;
     }
     // The exit list fades in after the key: read 0.6 s later, then 4 more times 0.4 s apart (an exit shows after two matching reads).
     if(triggered){pendingAt=now.AddMilliseconds(600);captures=5;lastFrame.Clear();taxiLastFrame=new Dictionary<string,string>();}
    }
    long tick=(long)Input.GetTickCount64();
    if(autoExits!=null&&tick>=autoExits.Until)autoExits=null;
    bool autoDue=autoExits!=null&&tick>=autoExits.Next,manualDue=captures>0&&now>=pendingAt;
    if((autoDue||manualDue)&&exitRead==null&&game!=IntPtr.Zero&&!map.Visible&&!ocrBlocked){
     if(manualDue){captures--;pendingAt=now.AddMilliseconds(400);}
     if(autoDue)autoExits.Next=tick+150; /* the next read starts as soon as this one is done (one read takes about 0.1-0.25 s) */
     try{ReadExits(game);}catch(Exception ex){Status(Message(ex));captures=0;autoExits=null;}
    }
   }else Input.Watch(new int[0]);
   if(warmAt!=null&&DateTime.UtcNow>=warmAt.Value){warmAt=null;if(mapsOn){try{map.Warm();}catch{}}}
   // Flea market sales from the game's log (every 3 s): sent to the phone together, at most once a minute (the relay's limit).
   if(now>=fleaNext){fleaNext=now.AddSeconds(3);foreach(var sale in FleaSales.Poll(questLogs)){fleaPending.Add(sale);if(fleaSince==null)fleaSince=now;}
    if(!phonePaired){fleaPending.Clear();fleaSince=null;}
    if(fleaPending.Count>0&&(now-fleaSince.Value).TotalSeconds>=10&&now>=fleaSentAt.AddSeconds(61)){var names=FleaNames();
     var first=fleaPending[0];string name;if(!names.TryGetValue(first.Item,out name))name="?";bool sent;
     if(fleaPending.Count==1)sent=PhoneSend("tarkov.flea.sold",new[]{name,first.Count.ToString(CultureInfo.CurrentCulture),Money(first.Amount,first.Currency)});
     else{var tot=string.Join(" + ",fleaPending.GroupBy(s=>s.Currency).Select(g=>Money(g.Sum(s=>s.Amount),g.Key)));sent=PhoneSend("tarkov.flea.sold.many",new[]{name,first.Count.ToString(CultureInfo.CurrentCulture),Money(first.Amount,first.Currency),(fleaPending.Count-1).ToString(CultureInfo.CurrentCulture),tot});}
     if(sent){fleaPending.Clear();fleaSince=null;fleaSentAt=now;}}}
   PollExitRead();
   PollMapRead(game);
   // A key added or removed from an inspect window (price helper, my-keys.json): redraw the map and its key list.
   if(now>=nextKeysCheck){nextKeysCheck=now.AddSeconds(1);var kf=Path.Combine(store,"my-keys.json");long ks=File.Exists(kf)?File.GetLastWriteTimeUtc(kf).Ticks:0;
    var kd=Quests.CachePath(store,Quests.CurrentMode(questLogs));var stamp=ks+":"+(File.Exists(kd)?File.GetLastWriteTimeUtc(kd).Ticks:0);
    if(keysStamp==null)keysStamp=stamp;else if(stamp!=keysStamp){keysStamp=stamp;try{LoadMap();}catch{}}}
   var stateFile=Path.Combine(store,"raid-state.txt");
   if(now>=nextStateCheck&&File.Exists(stateFile)){nextStateCheck=now.AddMilliseconds(100);long stamp=File.GetLastWriteTimeUtc(stateFile).Ticks;if(stamp!=stateStamp){stateStamp=stamp;try{RaidState(File.ReadAllLines(stateFile,Encoding.UTF8),now);}catch{Status(T("레이드 상태 읽기 대기"));}}}
   if(pendingNotice!=null){if(Input.Idle()<pendingNotice.Idle)pendingNotice=null;else if(now>=pendingNotice.Due){if(config.discord){PhoneSend("raid.started.away",null);if(webhookRoute)Enqueue("tarkov","raid.started.away",pendingNotice.Id,T("타르코프 레이드가 시작되었습니다. PC 무입력 상태가 감지되었습니다."));}pendingNotice=null;}}
   if(webhookRoute&&config.maintenance&&noticeTask==null&&now>=noticeNext){noticeNext=now.AddMinutes(5);noticeTask=http.GetAsync("https://t.me/s/escapefromtarkovEN");}
   if(noticeTask!=null&&noticeTask.IsCompleted){try{using(var r=noticeTask.GetAwaiter().GetResult()){
     if(!r.IsSuccessStatusCode)throw new Exception(T("공식 공지 조회 실패 · 점검 상태를 추정하지 않습니다."));
     var html=r.Content.ReadAsStringAsync().GetAwaiter().GetResult();var found=Regex.Matches(html,"(?s)data-post=\"escapefromtarkovEN/(\\d+)\".*?<div class=\"tgme_widget_message_text[^\"]*\"[^>]*>(.*?)</div>");
     if(found.Count==0)throw new Exception(T("공식 공지 형식 미확인 · 알림 보류"));
     foreach(Match m in found){var id=m.Groups[1].Value;var text=System.Net.WebUtility.HtmlDecode(Regex.Replace(m.Groups[2].Value,"<[^>]+>"," "));bool isNew=noticeSeen.Add(id);var kind=ExitMatch.ClassifyNotice(text);
      if(isNew&&noticeInitialized&&kind.Length>0&&config.maintenance){var label=kind=="maintenance.started"?T("점검 시작"):kind=="maintenance.extended"?T("점검 연장"):T("점검 완료");Enqueue("tarkov",kind,id,T("타르코프 공식 공지 · ")+label+"\n"+text.Substring(0,Math.Min(1200,text.Length))+"\nhttps://t.me/escapefromtarkovEN/"+id);}}
     noticeInitialized=true;
    }}catch(Exception ex){DStatus(Message(ex));}noticeTask=null;}
  }
  // raid-state.txt from BlueAsh.exe: pid, map, role, raid id, phase, serial, tick, entered tick.
  void RaidState(string[] state,DateTime now){
   if(state.Length<7||state[0]!=parentId.ToString(CultureInfo.InvariantCulture))return;
   var serial=state[0]+":"+state[5];var raid=state[3];var phase=state[4];raidPhase=phase;
   if(serial!=lastSerial||(raid.Length>0&&raid!=lastRaid)||phase=="ended"){ResetRaid();pendingNotice=null;lastSerial=serial;lastRaid=raid;}
   var mapName=state[1];if(mapName=="factory4_night")mapName="factory4_day";if(mapName=="sandbox_high")mapName="sandbox";
   var found=CatalogRow(r=>S(r,"id")==mapName);if(found!=null)map.Maps.SelectedItem=S(found,"displayName");
   if(state[2]=="PMC"||state[2]=="SCAV")map.Role.SelectedItem=state[2];else map.State.Text=T("역할 미확정 · PMC/SCAV 직접 선택 필요");
   var schedule=RaidExits.Schedule(state,(long)Input.GetTickCount64(),autoExitKey);
   if(schedule!=null){autoExitKey=schedule.Key;if(mapsOn){autoExits=schedule;autoQuiet=0;lastFrame.Clear();}}
   if(config.discord&&phase=="started"&&raid.Length>0&&!announced.Contains(raid)){
    announced.Add(raid);announcedOrder.Add(raid);File.WriteAllText(sentFile,Json.Write(announcedOrder.Skip(Math.Max(0,announcedOrder.Count-200)).ToArray()),new UTF8Encoding(true));
    if(config.discord&&Input.Idle()>=(uint)(config.idle*1000))pendingNotice=new Notice{Id=raid,Due=now.AddSeconds(5),Idle=Input.Idle()};
   }
  }
 }
}
