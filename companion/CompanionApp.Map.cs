using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32;
using ModeSwitchPrice;
using BlueAshUi;
namespace ModePulse {
 public sealed partial class CompanionApp {
  MapWindow map;string mapId="";string questLogs="",markerShipped,markerCache,seenPath,mode="";
  Dictionary<string,string[]> seenExits=new Dictionary<string,string[]>();object extracts;
  HashSet<string> lastFrame=new HashSet<string>();Dictionary<string,string> taxiLastFrame=new Dictionary<string,string>();
  DateTime pendingAt=DateTime.MaxValue;int captures=0;bool ocrBlocked;int exitGeneration=0;ExitSchedule autoExits;int autoQuiet;string autoExitKey="";

  void SavePins(){
   var b=new StringBuilder("{");bool first=true;
   foreach(var p in pins){if(!first)b.Append(',');first=false;b.Append(Json.Write(p.Key)).Append(':').Append(Json.Write(p.Value.ToArray()));}
   File.WriteAllText(pinsPath,b.Append('}').ToString(),new UTF8Encoding(true));
  }
  List<Dictionary<string,object>> PinsFor(string key){List<Dictionary<string,object>> l;return pins.TryGetValue(key,out l)?l:new List<Dictionary<string,object>>();}
  // Name suggestions: names seen in the in-game list first, then tarkov.dev's exits for this side and transits (with conditions).
  void SetSuggestions(string id,string role){
   var names=new List<string>();var conds=new List<string>();string[] seen;
   if(seenExits.TryGetValue(id+"_"+role,out seen))foreach(var n in seen)if(!string.IsNullOrEmpty(n)){names.Add(n);conds.Add("");}
   var m=Json.Get(extracts,id);
   if(m!=null){var want=role=="SCAV"?"scav":"pmc";
    foreach(var e in Json.List(Json.Get(m,"extracts"))){var f=Json.Str(Json.Get(e,"faction"));if(string.Equals(f,want,StringComparison.OrdinalIgnoreCase)||string.Equals(f,"shared",StringComparison.OrdinalIgnoreCase)){names.Add(Json.Str(Json.Get(e,"name")));conds.Add(Json.Bool(Json.Get(e,"switch"),false)?T("스위치 작동 필요"):"");}}
    foreach(var t in Json.List(Json.Get(m,"transits"))){var ko=Json.Str(Json.Get(t,"ko"));if(ko.Length>0){names.Add(ko);conds.Add(Json.Str(Json.Get(t,"conditions")));}}}
   map.SetSuggestions(names.ToArray(),conds.ToArray());
  }
  void RememberExits(IEnumerable<string> lines){
   if(mapId.Length==0)return;var list=new List<string>();string[] old;if(seenExits.TryGetValue(mapId,out old))foreach(var n in old)if(!string.IsNullOrEmpty(n))list.Add(n);
   bool changed=false;foreach(var l in lines){var t=(l??"").Trim();if(t.Length>=3&&Regex.IsMatch(t,"[A-Za-z가-힣]{2}")&&!list.Contains(t)){list.Add(t);changed=true;}}
   if(changed){while(list.Count>60)list.RemoveAt(0);seenExits[mapId]=list.ToArray();try{File.WriteAllText(seenPath,Json.Write(seenExits),new UTF8Encoding(true));}catch{}}
  }
  Dictionary<string,object> CatalogRow(Func<Dictionary<string,object>,bool> pick){foreach(var r in catalog)if(pick(r))return r;return null;}
  string MapRole(){return Convert.ToString(map.Role.SelectedItem)??"";}
  string MapName(){return Convert.ToString(map.Maps.SelectedItem)??"";}
  string CurrentMode(){try{mode=Quests.CurrentMode(questLogs);}catch{}return mode;}

  void LoadMap(){
   var rows=catalog.Where(r=>S(r,"displayName")==MapName()&&S(r,"role")==MapRole()).ToList();
   if(rows.Count!=1){map.Canvas.Visible=false;map.State.Text=T("이 역할의 지도는 제공되지 않았습니다.");return;}
   var row=rows[0];string id=S(row,"id"),role=S(row,"role"),key=id+"_"+role;
   // The picture comes with the data pack (downloaded after BlueAsh starts): until it is there, say so and try again next time.
   var picture=Path.Combine(root,"..","maps",S(row,"file"));if(!File.Exists(picture)){map.Canvas.Visible=false;map.State.Text=T("지도 그림을 받는 중입니다. 잠시 뒤 다시 열어 주세요.");mapId="";return;}
   if(key!=mapId){mapId=key;lastFrame.Clear();map.Canvas.Available.Clear();map.Canvas.TaxiStates.Clear();taxiLastFrame=new Dictionary<string,string>();map.Canvas.LoadMap(picture);}
   map.Canvas.Visible=true;map.Canvas.Pins.Clear();map.Canvas.Selected=null;map.MapId=id;map.QuestLogs=questLogs;map.Store=store;map.MyKeys=Path.Combine(store,"my-keys.json");
   // The player's own exit edits replace the Fandom exits/transits only; spawns, bosses and quests stay live.
   var own=PinsFor(key);
   foreach(var p in own){if(p==null)continue;var pin=new Pin{Name=Json.Str(Json.Get(p,"Name")),X=(float)Json.Num(Json.Get(p,"X"),0),Y=(float)Json.Num(Json.Get(p,"Y"),0),Taxi=Json.Bool(Json.Get(p,"Taxi"),false),Destination=Json.Str(Json.Get(p,"Destination")),Condition=Json.Str(Json.Get(p,"Condition")),Kind=Json.Str(Json.Get(p,"Kind"))=="transit"?"transit":"exit"};map.Canvas.Pins.Add(pin);}
   foreach(var pin in FandomMarkers.Pins(id,role,markerCache,markerShipped))if(own.Count==0||(pin.Kind!="exit"&&pin.Kind!="transit"))map.Canvas.Pins.Add(pin);
   // Bosses with this mode's spawn chance, map texts in the UI language; then my keys and the active quests.
   try{CurrentMode();Quests.RefreshIfStale(store,mode,TimeSpan.FromHours(24));}catch{}
   try{Quests.Decorate(map.Canvas.Pins,id,questLogs,store);}catch{}
   // Top-left box: every boss this mode's game data lists on this map, with its chance (also ones the map has no marker for).
   map.Canvas.Info="";try{map.Canvas.Info=Quests.BossSummary(id,questLogs,store,map.Canvas.Pins);}catch{}
   // Active quests missing from the quest data (new event quests): say so instead of showing nothing.
   if(config.showQuests&&role!="SCAV"){try{int n=Quests.Unlisted(questLogs,store);if(n>0){var line=T("진행 중 퀘스트 {0}개는 아직 위치 데이터가 없습니다 (이벤트 퀘스트 등)",n);map.Canvas.Info=string.IsNullOrEmpty(map.Canvas.Info)?line:map.Canvas.Info+"\n"+line;}}catch{}}
   if(config.showKeys){try{foreach(var pin in Quests.KeyPins(id,questLogs,store,markerShipped,Path.Combine(store,"my-keys.json")))map.Canvas.Pins.Add(pin);}catch{}}
   // The key list beside the map (add / remove there; its Changed event redraws the map).
   map.KeyList.Visible=config.showKeys;
   if(config.showKeys){try{map.KeyList.Load(Quests.CachePath(store,mode),Path.Combine(store,"my-keys.json"),mode,id);}catch{}}
   // Quest marks only on the PMC map: a Scav run cannot do PMC quests, the marks would only cover the map.
   Story.Use(Path.Combine(store,"story-progress.json"));
   if(config.showQuests&&role!="SCAV"&&Story.ShowGeneral){try{foreach(var pin in Quests.Pins(id,questLogs,store,markerShipped))map.Canvas.Pins.Add(pin);}catch{}}
   // Story chapters (ticked by the player, per mode): PMC map only, like the quest marks.
   map.Canvas.StoryUnknown="";if(role!="SCAV"){try{Story.Apply(map.Canvas,id,markerShipped,Quests.CurrentMode(questLogs),store);}catch{}}
   SetSuggestions(id,role);
   int exits=map.Canvas.Pins.Count(p=>p.Kind=="exit"||p.Kind=="transit");
   map.Canvas.Invalidate();map.State.Text=T("탈출구 {0}곳 · 레이드 시작 시 자동 확인 · 확인 키로 재확인\n초록 비상구: 탈출구 · 두 사람: 협동 탈출구 · 주황 비상구: 트랜짓 · 미확인: 회색 · 신호탄 권총: 플레어 탈출",exits);
  }
  void LocateKey(KeyLocationEventArgs e){
   var target=CatalogRow(r=>S(r,"id")==e.MapId&&S(r,"role")==MapRole())??CatalogRow(r=>S(r,"id")==e.MapId&&S(r,"role")=="PMC");if(target==null)return;
   map.Role.SelectedItem=S(target,"role");map.Maps.SelectedItem=S(target,"displayName");
   if(mapId!=S(target,"id")+"_"+S(target,"role"))LoadMap();
   if(!map.Canvas.FocusKey(e.Id))map.State.Text=T("이 지도의 열쇠 사용처 좌표를 찾지 못했습니다.");
  }
  void ResetRaid(){exitGeneration++;autoExits=null;map.Canvas.MeShown=false;lastFrame.Clear();captures=0;pendingAt=DateTime.MaxValue;map.Canvas.Available.Clear();map.Canvas.TaxiStates.Clear();taxiLastFrame=new Dictionary<string,string>();map.Canvas.Invalidate();}

  void BuildMapWindow(){
   map=new MapWindow();
   foreach(var name in catalog.Select(r=>S(r,"displayName")).Distinct())map.Maps.Items.Add(name);map.Maps.SelectedIndex=0;
   map.MapKey=config.mapKey;
   // Map markers (exits, transits, PMC spawns, bosses, cultists) from the Fandom interactive maps: the snapshot shipped
   // in maps/markers.json, replaced by a copy refreshed at most daily in the background. The user's own pin edits
   // (map-pins.json) still win per map.
   markerShipped=Path.Combine(root,"..","maps","markers.json");markerCache=Path.Combine(store,"fandom-markers.json");
   FandomMarkers.RefreshIfStale(markerCache,TimeSpan.FromHours(24));
   // Accepted quests (from the game's push-notification logs) and their objective places (tarkov.dev, refreshed daily).
   questLogs=Reg("RaidLogs");if(questLogs.Length==0)questLogs=@"C:\Battlestate Games\Escape from Tarkov\Logs";
   Quests.CacheFile=Path.Combine(store,"quest-log-index.json");
   // First log scan now (seconds once ever, then cached on disk), so opening the map only re-reads the newest logs.
   try{mode=Quests.CurrentMode(questLogs);Quests.RefreshIfStale(store,mode,TimeSpan.FromHours(24));}catch{}
   // Exit names read from the in-game exit list (F4), per map, offered as names in the pin editor.
   seenPath=Path.Combine(store,"seen-exits.json");
   try{if(File.Exists(seenPath)){var obj=Json.Parse(File.ReadAllText(seenPath,Encoding.UTF8)) as IDictionary<string,object>;if(obj!=null)foreach(var p in obj)seenExits[p.Key]=Json.List(p.Value).Select(x=>Json.Str(x)).ToArray();}}catch{}
   try{extracts=Json.Parse(File.ReadAllText(Path.Combine(root,"..","maps","extracts.json"),Encoding.UTF8));}catch{}
   map.KeyList.LocateRequested+=delegate(object s,KeyLocationEventArgs e){LocateKey(e);};
   // Story card: a tick or a filter change redraws the map; "show on map" for a step on another map switches to it (PMC).
   map.StoryChanged+=delegate{LoadMap();};
   map.StoryLocateRequested+=delegate(object s,KeyLocationEventArgs e){
    var target=CatalogRow(r=>S(r,"id")==e.MapId&&S(r,"role")=="PMC");if(target==null)return;
    map.Role.SelectedItem="PMC";map.Maps.SelectedItem=S(target,"displayName");LoadMap();
    if(!map.Canvas.FocusStory(e.Id))map.State.Text=T("이 지도에서 이 목표의 위치를 찾지 못했습니다.");};
   map.KeyList.Changed+=delegate{LoadMap();};map.Maps.SelectedIndexChanged+=delegate{LoadMap();};map.Role.SelectedIndexChanged+=delegate{LoadMap();};
   map.ClearPins.Click+=delegate{if(MessageBox.Show(T("이 지도에서 직접 고친 탈출구를 지우고 기본(공유) 데이터로 되돌릴까요?"),"BlueAsh",MessageBoxButtons.YesNo)==DialogResult.Yes){pins.Remove(mapId);SavePins();mapId="";LoadMap();}};
   resetBtn.Click+=delegate{ResetRaid();Status(T("이번 레이드 표시를 지웠습니다."));};
   mapBtn.Click+=delegate{LoadMap();map.ShowOver(Desktop.Game());};
   hideBtn.Click+=delegate{discordForm.Hide();};closeMapBtn.Click+=delegate{mapForm.Hide();};
   saveMapBtn.Click+=delegate{int before=config.mapKey;if(SaveConfig()){Status(T("저장했습니다."));if(config.mapKey!=before)config.mapKeyIgnore="";CheckMapKeyConflicts(false);}};
   saveBtn.Click+=delegate{if(SaveConfig())DStatus(T("저장했습니다."));};
  }
  bool SaveConfig(){
   var url=hook.Text.Trim();
   if(url.Length>0&&!ExitMatch.ValidWebhook(url)){var m=T("웹후크 주소가 올바르지 않습니다. 비워 두거나 discord.com 웹후크 주소를 넣어 주세요.");Status(m);DStatus(m);return false;}
   if(discordCheck.Checked&&!phonePaired&&url.Length==0){var m=relay.Length>0?T("자리 비움 알림을 켜려면 먼저 휴대폰 연결(또는 Discord 연결)을 해 주세요."):T("디스코드 알림을 켜려면 먼저 웹후크 주소를 넣어 주세요. (웹후크 만드는 법 참고)");Status(m);DStatus(m);return false;}
   if(mapKeyBox.SelectedIndex<0){Status(T("지도 키를 골라 주세요."));return false;}
   config.showQuests=questCheck.Checked;config.showKeys=keyCheck.Checked;config.shotPos=shotCheck.Checked;config.shotDelete=shotDelete.Checked;ScreenshotPosition.Watch(config.shotPos);
   config.discord=discordCheck.Checked;config.maintenance=maintenanceCheck.Checked;config.idle=(int)idle.Value;config.mapKey=Convert.ToString(mapKeyBox.SelectedItem)[0];
   config.webhook=Protect(url);config.relayKey=Protect(relayKey);
   activeUrl=url;map.MapKey=config.mapKey;
   config.Save(settingsPath);
   SetReg("MapScreenshotKey",shotOnMap.Checked&&config.shotPos?((char)config.mapKey).ToString():"");SetReg("CompanionEnabled",(config.maps||config.discord||config.maintenance)?"1":"0");
   ocrBlocked=false;UpdateLink();return true;
  }

  // ---- 지도 키 충돌 (타르코프 키 설정) ----
  // The map key alone (no Ctrl/Alt/Shift) must not also do something in the game. When Control.ini binds it, ask once
  // per key whether to move those game bindings; chosen keys go to TarkovKeyMoves, which BlueAsh writes into
  // Control.ini while the game is closed (tarkov_keys.h). "No" is remembered for that key (mapKeyIgnore).
  static readonly string[,] gameKeyNameList={{"WeaponMounting","무기 거치"},{"Recorder","녹음기"},{"ToggleBipods","양각대"},{"Walk","걷기"},{"Sprint","달리기"},{"Jump","점프"},{"Vaulting","넘어가기"},{"Duck","앉기"},{"Prone","엎드리기"},{"StepRight","오른쪽 기울이기"},{"StepLeft","왼쪽 기울이기"},{"LeanLockRight","오른쪽 기울이기 고정"},{"LeanLockLeft","왼쪽 기울이기 고정"},{"Shoot","사격"},{"Aim","조준"},{"ReloadWeapon","재장전"},{"QuickReloadWeapon","빠른 재장전"},{"ShootingMode","사격 모드"},{"CheckFireMode","사격 모드 확인"},{"NextMagazine","다음 탄창"},{"PreviousMagazine","이전 탄창"},{"UnloadMagazine","탄창 비우기"},{"CheckAmmo","탄약 확인"},{"CheckChamber","약실 확인"},{"ChamberUnload","약실 비우기"},{"ChangeAimScope","조준경 변경"},{"ChangeAimScopeMagnification","배율 변경"},{"ExamineWeapon","무기 살펴보기"},{"FoldStock","개머리판 접기"},{"PrimaryWeaponFirst","주무기 1"},{"PrimaryWeaponSecond","주무기 2"},{"Knife","칼"},{"SecondaryWeapon","권총"},{"QuickSecondaryWeapon","빠른 권총"},{"QuickKnife","빠른 칼"},{"ThrowGrenade","수류탄"},{"ToggleGoggles","고글"},{"ToggleVoip","음성 채팅"},{"PushToTalk","음성 채팅 말하기"},{"Tactical","전술 장비"},{"NextTacticalDevice","다음 전술 장비"},{"ToggleHeadLight","헤드라이트"},{"SwitchHeadLight","헤드라이트 모드"},{"Nidnod","고개 끄덕이기"},{"WatchTime","시간 확인"},{"Interact","상호작용"},{"ThrowItem","아이템 버리기"},{"DropBackpack","가방 내려놓기"},{"Breath","숨 참기"},{"Mumble","제스처"},{"WatchTimerAndExits","탈출구 확인"},{"Inventory","인벤토리"},{"Slot4","퀵슬롯 4"},{"Slot5","퀵슬롯 5"},{"Slot6","퀵슬롯 6"},{"Slot7","퀵슬롯 7"},{"Slot8","퀵슬롯 8"},{"Slot9","퀵슬롯 9"},{"Slot0","퀵슬롯 0"},{"MakeScreenshot","스크린샷"},{"ToggleInfo","정보 표시"},{"Console","콘솔"},{"ChangePointOfView","시점 변경"},{"TogglePinMode","고정 모드"},{"ToggleLockMode","잠금 모드"}};
  static string GameKeyName(string key){for(int i=0;i<gameKeyNameList.GetLength(0);i++)if(string.Equals(gameKeyNameList[i,0],key,StringComparison.OrdinalIgnoreCase))return T(gameKeyNameList[i,1]);return key;}
  // Unity key names offered as replacements, with what the player sees (in this order).
  static List<KeyValuePair<string,string>> GameKeyChoices(){
   var l=new List<KeyValuePair<string,string>>();foreach(var p in new[]{"Comma=,","Period=.","Slash=/","Semicolon=;","Quote='","LeftBracket=[","RightBracket=]","Minus=-","Equals==","BackQuote=`","Backslash=\\"}){int i=p.IndexOf('=');l.Add(new KeyValuePair<string,string>(p.Substring(0,i),p.Substring(i+1)));}
   for(int c=65;c<=90;c++)l.Add(new KeyValuePair<string,string>(((char)c).ToString(),((char)c).ToString()));for(int d=0;d<=9;d++)l.Add(new KeyValuePair<string,string>("Alpha"+d,d.ToString()));for(int f=1;f<=11;f++)l.Add(new KeyValuePair<string,string>("F"+f,"F"+f));
   for(int d=0;d<=9;d++)l.Add(new KeyValuePair<string,string>("Keypad"+d,T("숫자패드 ")+d));return l;
  }
  static string ControlIni(){return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Battlestate Games","Escape from Tarkov","Settings","Control.ini");}
  // Keys on movement axes (W/A/S/D...) live in axisBindings, not keyBindings.
  static HashSet<string> GameAxisKeys(){
   var keys=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   try{foreach(var a in Json.List(Json.Get(Json.Parse(File.ReadAllText(ControlIni(),Encoding.UTF8)),"axisBindings")))foreach(var pair in Json.List(Json.Get(a,"pairs")))foreach(var side in new[]{Json.Get(pair,"positive"),Json.Get(pair,"negative")})foreach(var k in Json.List(Json.Get(side,"keyCode")))keys.Add(Json.Str(k));}catch{}
   return keys;
  }
  static List<object> GameBindings(){var path=ControlIni();if(!File.Exists(path))return new List<object>();try{return Json.List(Json.Get(Json.Parse(File.ReadAllText(path,Encoding.UTF8)),"keyBindings"));}catch{return new List<object>();}}
  static List<string> SingleCodes(object variant){return Json.List(Json.Get(variant,"keyCode")).Select(x=>Json.Str(x)).ToList();}
  // Game bindings whose single key is the map key; ones with a move already queued are left out.
  List<string> MapKeyConflicts(List<object> bindings){
   var key=((char)config.mapKey).ToString();var pending=Reg("TarkovKeyMoves");var r=new List<string>();
   foreach(var b in bindings){var name=Json.Str(Json.Get(b,"keyName"));if(string.Equals(name,"MakeScreenshot",StringComparison.OrdinalIgnoreCase)||Regex.IsMatch(pending,"(^|;)"+Regex.Escape(name)+":",RegexOptions.IgnoreCase))continue;
    foreach(var v in Json.List(Json.Get(b,"variants"))){var codes=SingleCodes(v);if(codes.Count==1&&string.Equals(codes[0],key,StringComparison.OrdinalIgnoreCase)){r.Add(name);break;}}}
   return r;
  }
  void CheckMapKeyConflicts(bool asked){
   var bindings=GameBindings();var conflicts=MapKeyConflicts(bindings);var key=((char)config.mapKey).ToString();
   if(conflicts.Count==0||(!asked&&config.mapKeyIgnore==key))return;
   var names=string.Join(", ",conflicts.Select(GameKeyName));
   var answer=MessageBox.Show(mapForm,T("현재 지도 단축키 {0} 가 타르코프 안에서 '{1}'(으)로 지정되어 있습니다.\r\n그대로 두면 지도를 열 때 게임에서도 같이 동작합니다.\r\n\r\n타르코프 안에 지정되어 있는 키를 변경하시겠습니까?",key,names),T("BlueAsh · 지도 키 충돌"),MessageBoxButtons.YesNo,MessageBoxIcon.Warning);
   if(answer!=DialogResult.Yes){config.mapKeyIgnore=key;try{config.Save(settingsPath);}catch{}return;}
   // Keys free in the game (no single-key binding uses them) and not BlueAsh's own keys.
   var used=GameAxisKeys();foreach(var b in bindings)foreach(var v in Json.List(Json.Get(b,"variants"))){var codes=SingleCodes(v);if(codes.Count==1)used.Add(codes[0]);}
   var choices=GameKeyChoices();var free=choices.Where(c=>!used.Contains(c.Key)&&!string.Equals(c.Key,key,StringComparison.OrdinalIgnoreCase)).ToList();
   using(var f=new Form()){
    f.Text=T("BlueAsh · 타르코프 키 변경");Brand.Apply(f,appExe);f.StartPosition=FormStartPosition.CenterParent;f.FormBorderStyle=FormBorderStyle.FixedDialog;f.MaximizeBox=false;f.MinimizeBox=false;f.Font=uiFont;f.TopMost=true;
    f.ClientSize=new Size(Px(620),Px(138+44*conflicts.Count));
    var l=new Label{Text=T("게임에서 쓰지 않는 키만 보입니다. 게임이 꺼져 있을 때 자동으로 바뀝니다.")};l.SetBounds(Px(18),Px(14),Px(584),Px(50));f.Controls.Add(l);
    var boxes=new Dictionary<string,ComboBox>();int y=76,i=0;
    foreach(var c in conflicts){var n=new Label{Text=GameKeyName(c)+"  ("+key+" →)"};n.SetBounds(Px(18),Px(y+4),Px(340),Px(26));f.Controls.Add(n);
     var box=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList};foreach(var k2 in free)box.Items.Add(k2.Value);box.SelectedIndex=Math.Min(i,box.Items.Count-1);box.SetBounds(Px(380),Px(y),Px(220),Px(30));f.Controls.Add(box);boxes[c]=box;y+=44;i++;}
    var ok=new ThemeButton{Text=T("변경"),DialogResult=DialogResult.OK};ok.SetBounds(Px(380),Px(y+8),Px(105),Px(34));f.Controls.Add(ok);f.AcceptButton=ok;
    var no=new ThemeButton{Text=T("취소"),DialogResult=DialogResult.Cancel};no.SetBounds(Px(495),Px(y+8),Px(105),Px(34));f.Controls.Add(no);f.CancelButton=no;
    Theme.Apply(f);var result=f.ShowDialog(mapForm);
    if(result==DialogResult.OK){
     var picked=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var moves=new StringBuilder();
     foreach(var c in conflicts){var label=Convert.ToString(boxes[c].SelectedItem);var unity=choices.Where(x=>x.Value==label).Select(x=>x.Key).FirstOrDefault();
      if(unity==null||picked.Contains(unity)){MessageBox.Show(mapForm,T("기능마다 서로 다른 키를 골라 주세요."),"BlueAsh");return;}
      picked.Add(unity);moves.Append(c+":"+key+">"+unity+";");}
     SetReg("TarkovKeyMoves",Reg("TarkovKeyMoves")+moves);
     Status(T("타르코프 키 변경 예약: ")+string.Join(", ",conflicts.Select(c=>GameKeyName(c)+" → "+Convert.ToString(boxes[c].SelectedItem)))+T(" · 게임이 꺼져 있을 때 자동 적용"));
    }
   }
  }

  void OpenPage(string page){
   if(page=="discord"){discordForm.Show();discordForm.WindowState=FormWindowState.Normal;discordForm.Activate();return;}
   if(page=="hideout"){ShowHideoutLevels();return;}
   // The pairing window and the map window by request (same as the page buttons; used to check them).
   if(page=="phone"){StartPhone();return;}
   if(page=="mapview"){LoadMap();map.ShowOver(Desktop.Game());return;}
   // discord@<main window> / map@<main window>: a page inside BlueAsh's main window, in the page box under its header.
   if(page.StartsWith("discord@")){if(!Embed.Attach(discordForm,Handle(page.Substring(8)),260,118,852,622))discordForm.Show();return;}
   if(page.StartsWith("map@")){if(!Embed.Attach(mapForm,Handle(page.Substring(4)),260,118,852,600))mapForm.Show();CheckMapKeyConflicts(false);return;}
   mapForm.Show();mapForm.WindowState=FormWindowState.Normal;mapForm.Activate();CheckMapKeyConflicts(false);
  }
  static IntPtr Handle(string text){long v;return long.TryParse(text,out v)?new IntPtr(v):IntPtr.Zero;}
 }
}
