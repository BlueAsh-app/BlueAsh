using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.Win32;
using ModeSwitchPrice;
using BlueAshUi;
namespace ModePulse {
 // The map / notification helper (was companion/Companion.ps1 with CompanionCore, ControlBinding, ExitCapture, MapInput
 // and DiscordTransport): the in-game map window, the map settings page and the away-alert page inside BlueAsh's main
 // window (map@ / discord@<hwnd>), the hideout progress window and the phone pairing window. It talks to BlueAsh.exe
 // through files in %LOCALAPPDATA%\BlueAsh (companion-page.txt, raid-state.txt, my-keys.json ...) and the registry
 // key HKCU\Software\ModeSwitch, in the same formats as before.
 public sealed partial class CompanionApp {
  static string T(string s){return I18n.Text(s);}
  static string T(string s,params object[] v){return I18n.Format(s,v);}
  const string RegKey=@"HKEY_CURRENT_USER\Software\ModeSwitch";
  static string Reg(string name){var v=Registry.GetValue(RegKey,name,null);return v==null?"":Convert.ToString(v,CultureInfo.InvariantCulture);}
  static void SetReg(string name,string value){Registry.SetValue(RegKey,name,value??"",RegistryValueKind.String);}

  readonly int parentId;readonly Process parent;readonly string root,store,settingsPath,pinsPath,pagePath;
  readonly CompanionConfig config;string appExe="";string fontName;
  Font uiFont,pageFont,hintFont,sectionFont;float k=1;
  List<Dictionary<string,object>> catalog=new List<Dictionary<string,object>>();
  Dictionary<string,List<Dictionary<string,object>>> pins=new Dictionary<string,List<Dictionary<string,object>>>();
  PageForm mapForm,discordForm;Form form;
  readonly List<Control> sections=new List<Control>(),hints=new List<Control>(),primary=new List<Control>();

  public CompanionApp(int parentId,string root):this(parentId,root,null){}
  CompanionApp(int parentId,string root,string testStore){
   this.parentId=parentId;this.root=root;
   if(testStore==null){try{parent=Process.GetProcessById(parentId);appExe=parent.MainModule.FileName;}catch{if(parent==null)throw new Exception("parent-exit");}}
   store=testStore??DataDir.Folder;Directory.CreateDirectory(store);
   settingsPath=Path.Combine(store,"companion-settings.json");pinsPath=Path.Combine(store,"map-pins.json");pagePath=Path.Combine(store,"companion-page.txt");
   config=CompanionConfig.Load(settingsPath);
   // Enable every map checkbox on the first upgraded/new run, then respect saved choices.
   if(config.EnableMapDefaults()){SetReg("MapScreenshotKey",((char)config.mapKey).ToString());config.Save(settingsPath);}
   try{if(File.Exists(pinsPath)){var obj=Json.Parse(File.ReadAllText(pinsPath,Encoding.UTF8)) as IDictionary<string,object>;if(obj!=null)foreach(var p in obj){var list=new List<Dictionary<string,object>>();foreach(var x in Json.List(p.Value)){var d=x as Dictionary<string,object>;if(d!=null)list.Add(d);}pins[p.Key]=list;}}}catch{}
   foreach(var row in Json.List(Json.Parse(File.ReadAllText(Path.Combine(root,"..","maps","catalog.json"),Encoding.UTF8)))){var d=row as Dictionary<string,object>;if(d==null)continue;d["displayName"]=T(Json.Str(Json.Get(d,"name")));catalog.Add(d);}
  }
  static string S(Dictionary<string,object> row,string key){return Json.Str(Json.Get(row,key));}
  int Px(double v){return (int)Math.Round(v*k);}

  // ---- page building (design units at 96 DPI; the whole form is scaled to the system DPI before it is fitted) ----
  PageForm NewPanel(string title){
   var f=new PageForm();f.SuspendLayout();f.Text=title;f.ClientSize=new Size(704,590);f.StartPosition=FormStartPosition.CenterScreen;f.BackColor=Theme.Bg;f.ForeColor=Theme.Ink;f.Font=pageFont;f.FormBorderStyle=FormBorderStyle.FixedDialog;f.MaximizeBox=false;
   Brand.Apply(f,appExe);
   f.FormClosing+=delegate(object s,FormClosingEventArgs e){if(e.CloseReason==CloseReason.UserClosing){e.Cancel=true;((Form)s).Hide();}};return f;
  }
  Label Lab(string text,int x,int y,int width,int height=30){var c=new Label{Text=text};c.SetBounds(x,y,width,height);form.Controls.Add(c);return c;}
  Label Section(string text,int y){var c=Lab(text,8,y,688,28);sections.Add(c);return c;}
  Label Hint(string text,int x,int y,int width,int height=24){var c=Lab(text,x,y,width,height);hints.Add(c);return c;}
  ThemeSwitch Check(string text,int y,bool value){var c=new ThemeSwitch{Text=text};c.SetBounds(8,y,688,30);c.Checked=value;form.Controls.Add(c);return c;}
  ThemeButton Btn(string text,int x,int y,int width,bool isPrimary=false){var c=new ThemeButton{Text=text};c.SetBounds(x,y,width,40);form.Controls.Add(c);if(isPrimary)primary.Add(c);return c;}
  // Status lines are plain text (they looked like a box to type into); long text wraps inside the given height.
  Label StatusBox(int y,int height){var c=new Label{AutoSize=false,UseMnemonic=false};c.SetBounds(8,y,688,height);c.Text=T("준비 중");c.Font=hintFont;form.Controls.Add(c);return c;}

  // map page
  ComboBox mapKeyBox;Label bindingLabel,bindingDetails,status;ThemeSwitch questCheck,keyCheck,shotCheck,shotOnMap,shotDelete;ThemeButton mapBtn,resetBtn,keysClear,closeMapBtn,saveMapBtn;
  // notifications page
  string relay="";string relayKey="";bool webhookRoute;
  Label phoneTitle,phoneHelp,linkLabel,hookLabel,alertTitle,idleLabel,idleValue,idleHint,serverHint,discordStatus;StatusPill phoneLabel;ThemeButton phoneBtn,connectBtn,helpBtn,testBtn,hideBtn,saveBtn,idleMinus,idlePlus;
  TextBox hook;ThemeSwitch discordCheck,maintenanceCheck;NumericUpDown idle;Panel idleBox;

  void BuildMapPage(){
   form=mapForm;
   Section(T("키"),12);
   Lab(T("지도 키"),8,50,130);mapKeyBox=new ComboBox{DropDownStyle=ComboBoxStyle.DropDownList};for(int i=65;i<=90;i++)mapKeyBox.Items.Add(((char)i).ToString());form.Controls.Add(mapKeyBox);
   mapKeyBox.SetBounds(144,46,90,30);mapKeyBox.SelectedItem=((char)config.mapKey).ToString();
   Lab(T("탈출구 확인 키"),266,50,160);
   // Read from the game's Control.ini, not typed here: shown as text.
   bindingLabel=new Label{AutoSize=false,UseMnemonic=false,TextAlign=ContentAlignment.MiddleLeft};bindingLabel.SetBounds(430,46,266,30);form.Controls.Add(bindingLabel);
   bindingDetails=Hint(T("게임 키 설정(Control.ini)을 확인하는 중"),8,86,688);
   Section(T("지도에 표시"),126);
   questCheck=Check(T("진행 중인 퀘스트 목표"),158,config.showQuests);
   // The keys and keycards the player listed beside the map (my-keys.json): where they open, as small gold keys and cyan cards.
   keyCheck=Check(T("내 열쇠·카드키 목록 (지도 오른쪽에서 넣고 빼기)"),158,config.showKeys);keyCheck.SetBounds(8,192,688,30); // its own row: beside the quest switch the long text wrapped and was cut off
   // Position from the game's own screenshot names (the player presses the screenshot key; nothing is automated).
   shotCheck=Check(T("내 위치 (게임 스크린샷을 찍으면 그 자리로 갱신)"),226,config.shotPos);
   // The map key also as the game's second screenshot key (BlueAsh writes Control.ini while the game is closed:
   // tarkov_keys.h, registry MapScreenshotKey). One press: the game saves its own screenshot, BlueAsh opens the map.
   shotOnMap=Check(T("지도 키를 누를 때 내 위치도 갱신"),260,Reg("MapScreenshotKey")!="");
   shotDelete=Check(T("위치를 읽은 스크린샷은 자동 삭제"),294,config.shotDelete);
   Section(T("지도와 탈출구"),340);
   mapBtn=Btn(T("지도 열기 · 탈출구 편집"),8,372,250);
   resetBtn=Btn(T("이번 레이드 표시 지우기"),270,372,250);
   keysClear=Btn(T("내 열쇠 목록 비우기"),532,372,164);
   keysClear.Click+=delegate{var p=Path.Combine(store,"my-keys.json");try{if(File.Exists(p))File.WriteAllText(p,"{}");Status(T("내 열쇠 목록을 비웠습니다. 시세 창으로 열쇠를 다시 조사하면 다시 표시됩니다."));}catch{}};
   status=StatusBox(426,90);
   closeMapBtn=Btn(T("닫기"),424,540,120);saveMapBtn=Btn(T("저장"),556,540,140,true);
  }

  // Secrets (relay key, webhook) are DPAPI-encrypted for this Windows user, in ConvertFrom-SecureString's format:
  // the hex of a DPAPI blob of the UTF-16 text (BlueAsh.exe reads the phone code the same way, phone_game.h).
  static string Protect(string s){if(string.IsNullOrEmpty(s))return "";var blob=System.Security.Cryptography.ProtectedData.Protect(Encoding.Unicode.GetBytes(s),null,System.Security.Cryptography.DataProtectionScope.CurrentUser);var b=new StringBuilder(blob.Length*2);foreach(var x in blob)b.Append(x.ToString("x2"));return b.ToString();}
  static string Unprotect(string s){if(string.IsNullOrEmpty(s))return "";try{var blob=new byte[s.Length/2];for(int i=0;i<blob.Length;i++)blob[i]=Convert.ToByte(s.Substring(i*2,2),16);return Encoding.Unicode.GetString(System.Security.Cryptography.ProtectedData.Unprotect(blob,null,System.Security.Cryptography.DataProtectionScope.CurrentUser));}catch{return "";}}

  void BuildDiscordPage(){
   form=discordForm;
   // Relay URL ships with the build (companion/relay.txt). Empty = one-click connect not set up; webhook still works.
   var relayFile=Path.Combine(root,"relay.txt");if(File.Exists(relayFile)){relay=File.ReadAllText(relayFile).Trim().TrimEnd('/');if(!Regex.IsMatch(relay,"^https://[A-Za-z0-9.-]+(/[A-Za-z0-9._/-]*)?$"))relay="";}
   QuestWiki.Relay=relay;
   LocalAi.Init(store);
   relayKey=Unprotect(config.relayKey);
   // Phone alerts (web push: rings even while Discord is open on the PC). Server status alerts are sent by the relay
   // itself and switched on the phone. Discord DMs and the webhook were dropped from the page (2026-10-12): their controls
   // still exist, hidden, because the transport reads them; a build without the relay keeps the webhook route.
   phoneTitle=Section(T("휴대폰 알림"),12);
   phoneLabel=new StatusPill();phoneLabel.SetBounds(8,50,466,30);form.Controls.Add(phoneLabel); // connection state as a coloured badge (UpdatePhone)
   phoneBtn=Btn(T("휴대폰 연결"),486,44,210,true);phoneBtn.Visible=relay.Length>0;
   phoneHelp=Hint(T("휴대폰 연결을 누르면 PC에 QR이 뜹니다. 휴대폰 카메라로 찍고 알림 허용을 누르세요.\r\n게임이 시작될 때 PC에 움직임이 없으면 자리 비움으로 보고 휴대폰으로 바로 알려 드립니다."),8,84,688,48);
   linkLabel=Hint("",8,180,466,28);
   connectBtn=Btn(T("Discord 연결"),486,174,210);
   helpBtn=Btn(T("웹후크 만드는 법"),486,174,210);
   helpBtn.Click+=delegate{MessageBox.Show(T("1. Discord에서 알림 받을 서버의 채널(예: #모드펄스)을 정합니다. 내 서버가 없으면 왼쪽 + 로 서버를 만드세요.\r\n2. 채널 이름 옆 톱니바퀴(채널 편집) → 연동 → 웹후크 → 새 웹후크\r\n3. 웹후크 URL 복사 → 여기 웹후크 주소 칸에 붙여넣기 → 저장\r\n4. 테스트 알림 보내기로 도착 확인\r\n\r\n웹후크 주소는 이 PC에만 암호화해 저장합니다. 다른 사람에게 알려 주지 마세요."),T("BlueAsh · 디스코드 웹후크"));};
   hookLabel=Lab(T("웹후크 주소"),8,146,130);
   hook=new TextBox{UseSystemPasswordChar=true};hook.SetBounds(140,142,556,30);form.Controls.Add(hook);
   hook.Text=Unprotect(config.webhook);
   linkLabel.Visible=false;connectBtn.Visible=false;
   webhookRoute=relay.Length==0;helpBtn.Visible=webhookRoute;hookLabel.Visible=webhookRoute;hook.Visible=webhookRoute;
   int top=webhookRoute?196:142;
   alertTitle=Section(T("알림 받을 상황"),top);
   discordCheck=Check(T("알림을 지원하는 게임 중 자리를 비우면 알려 드립니다"),top+32,config.discord);
   maintenanceCheck=Check("",0,false);maintenanceCheck.Visible=false;
   idleLabel=Lab(T("자리 비움 기준"),8,top+78,130);
   // The seconds are kept in a NumericUpDown that is never shown (its white spin buttons did not fit the app);
   // the page shows  −  15  +  in the app's own buttons instead, 5 seconds a step.
   idle=new NumericUpDown{Minimum=10,Maximum=600};idle.Value=Math.Max(10,Math.Min(600,config.idle));
   idleBox=new Panel{Size=new Size(132,32)};form.Controls.Add(idleBox);
   idleMinus=new ThemeButton{Text="−"};idleMinus.SetBounds(0,0,32,32);idleBox.Controls.Add(idleMinus);
   idleValue=new Label{AutoSize=false,TextAlign=ContentAlignment.MiddleCenter};idleValue.SetBounds(34,0,64,32);idleValue.Text=idle.Value.ToString(CultureInfo.InvariantCulture);idleBox.Controls.Add(idleValue);
   idlePlus=new ThemeButton{Text="+"};idlePlus.SetBounds(100,0,32,32);idleBox.Controls.Add(idlePlus);
   idleMinus.Click+=delegate{idle.Value=Math.Max(idle.Minimum,idle.Value-5);idleValue.Text=idle.Value.ToString(CultureInfo.InvariantCulture);};
   idlePlus.Click+=delegate{idle.Value=Math.Min(idle.Maximum,idle.Value+5);idleValue.Text=idle.Value.ToString(CultureInfo.InvariantCulture);};
   idleHint=Hint(T("초 동안 입력이 없으면 자리 비움으로 봅니다."),240,top+80,456);
   serverHint=Hint(T("타르코프 점검·서버 알림은 휴대폰 BlueAsh 화면에서 켜고 끕니다. PC가 꺼져 있어도 옵니다."),8,top+118,688);
   discordStatus=StatusBox(top+154,64);
   testBtn=Btn(T("테스트 알림 보내기"),8,540,240);
   hideBtn=Btn(T("닫기"),424,540,120);saveBtn=Btn(T("저장"),556,540,140,true);
  }

  void Status(string text){status.Text=text;}
  void DStatus(string text){discordStatus.Text=text;}
 }
 // companion-settings.json (UTF-8 with BOM, the shape ConvertTo-Json wrote): unknown keys are dropped, missing ones keep defaults.
 public sealed class CompanionConfig {
  public string phoneCode="",mapKeyIgnore="",webhook="",relayKey="";
  public bool showQuests=true,showKeys=true,shotPos=true,shotDelete=true,maps=true,discord=false,maintenance=false;
  public int mapDefaults=0,idle=20,delay=1100,mapKey=77,exitKey=79;
  public static CompanionConfig Load(string path){
   var c=new CompanionConfig();if(!File.Exists(path))return c;
   try{var o=Json.Parse(File.ReadAllText(path,Encoding.UTF8));
    Func<string,object> g=key=>Json.Get(o,key);
    if(g("phoneCode")!=null)c.phoneCode=Json.Str(g("phoneCode"));if(g("mapKeyIgnore")!=null)c.mapKeyIgnore=Json.Str(g("mapKeyIgnore"));if(g("webhook")!=null)c.webhook=Json.Str(g("webhook"));if(g("relayKey")!=null)c.relayKey=Json.Str(g("relayKey"));
    c.showQuests=Json.Bool(g("showQuests"),c.showQuests);c.showKeys=Json.Bool(g("showKeys"),c.showKeys);c.shotPos=Json.Bool(g("shotPos"),c.shotPos);c.shotDelete=Json.Bool(g("shotDelete"),c.shotDelete);
    c.maps=Json.Bool(g("maps"),c.maps);c.discord=Json.Bool(g("discord"),c.discord);c.maintenance=Json.Bool(g("maintenance"),c.maintenance);
    c.mapDefaults=(int)Json.Num(g("mapDefaults"),c.mapDefaults);c.idle=(int)Json.Num(g("idle"),c.idle);c.delay=(int)Json.Num(g("delay"),c.delay);c.mapKey=(int)Json.Num(g("mapKey"),c.mapKey);c.exitKey=(int)Json.Num(g("exitKey"),c.exitKey);
   }catch{}
   return c;
  }
  // Apply the map defaults once; later saved opt-outs survive restarts and future updates.
  public bool EnableMapDefaults(){if(mapDefaults>=1)return false;showQuests=showKeys=shotPos=shotDelete=true;mapDefaults=1;return true;}
  static string Q(string s){return Json.Write(s??"");}
  static string B(bool b){return b?"true":"false";}
  public string ToJson(){
   var rows=new[]{"\"webhook\":  "+Q(webhook),"\"mapKeyIgnore\":  "+Q(mapKeyIgnore),"\"exitKey\":  "+exitKey,"\"relayKey\":  "+Q(relayKey),"\"delay\":  "+delay,"\"idle\":  "+idle,"\"shotPos\":  "+B(shotPos),"\"maps\":  "+B(maps),"\"maintenance\":  "+B(maintenance),"\"phoneCode\":  "+Q(phoneCode),"\"showKeys\":  "+B(showKeys),"\"shotDelete\":  "+B(shotDelete),"\"discord\":  "+B(discord),"\"showQuests\":  "+B(showQuests),"\"mapDefaults\":  "+mapDefaults,"\"mapKey\":  "+mapKey};
   return "{\r\n    "+string.Join(",\r\n    ",rows)+"\r\n}";
  }
  public void Save(string path){File.WriteAllText(path,ToJson(),new UTF8Encoding(true));}
 }
}
