using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using BlueAshUi;
namespace ModePulse {
 public sealed partial class CompanionApp {
  // Network transport is asynchronous; no game memory, injection or keyboard hook.
  // Two routes: the BlueAsh relay (one-click Discord connect, alerts arrive as bot DMs) or an advanced channel webhook.
  sealed class SendItem{public string Game,Type,Id,Text,Target,Relay;public DateTime Expires;public int Tries;}
  HttpClientHandler handler;HttpClient http;readonly Queue<SendItem> queue=new Queue<SendItem>();
  Task<HttpResponseMessage> sendTask;SendItem sendItem;HttpContent sendContent;DateTime sendAfter=DateTime.MinValue;bool disabledTransport;string activeUrl="";
  string claimState;Task<HttpResponseMessage> claimTask;DateTime claimUntil=DateTime.MinValue,nextClaim=DateTime.MinValue;
  string phoneCode="";bool phonePaired,phoneChecked;DateTime phoneWatchUntil=DateTime.MinValue,phoneNext=DateTime.MinValue;Task<HttpResponseMessage> phoneStatusTask,phoneSendTask;HttpContent phoneSendContent;
  static string Language{get{return I18n.Language;}}

  void InitTransport(){
   handler=new HttpClientHandler{AllowAutoRedirect=false};http=new HttpClient(handler){Timeout=TimeSpan.FromSeconds(20),MaxResponseContentBufferSize=2000000};
   connectBtn.Click+=delegate{StartConnect();};
   // ---- 휴대폰 알림 (web push through the relay: relay/phone.js) ----
   // A random pairing code made here (DPAPI-saved like the Discord key). The PC opens /phone/pair (a QR); the phone allows
   // notifications there. Alerts are POST /phone/notify {c, kind}; the relay owns the wording.
   phoneCode=Unprotect(config.phoneCode);
   phoneBtn.Click+=delegate{StartPhone();};
   UpdatePhone();
   // Turning the alert on with nothing connected starts the phone pairing (the main route).
   discordCheck.CheckedChanged+=delegate{if(discordCheck.Checked&&!phonePaired&&relayKey.Length==0&&hook.Text.Trim().Length==0&&relay.Length>0)StartPhone();};
   testBtn.Click+=delegate{
    // With the relay: the phone only. A build without the relay tests the channel webhook.
    if(relay.Length>0){if(PhoneSend("test",null))DStatus(T("휴대폰으로 테스트 알림을 보내는 중…"));else DStatus(T("먼저 휴대폰 연결을 눌러 휴대폰을 연결해 주세요."));return;}
    if(!ExitMatch.ValidWebhook(hook.Text.Trim())){DStatus(T("먼저 웹후크 주소를 넣어 주세요. (웹후크 만드는 법 참고)"));return;}
    activeUrl=hook.Text.Trim();disabledTransport=false;Enqueue("system","test",Guid.NewGuid().ToString(),T("BlueAsh 테스트 알림입니다."));
   };
   UpdateLink();
  }
  void Enqueue(string game,string type,string id,string text){if(queue.Count<32)queue.Enqueue(new SendItem{Game=game,Type=type,Id=id,Text=text,Expires=DateTime.UtcNow.AddMinutes(3),Tries=0,Target=activeUrl,Relay=relayKey});}
  void UpdateLink(){
   connectBtn.Text=T("Discord 연결");
   if(relayKey.Length>0)linkLabel.Text=T("연결됨 · 알림은 BlueAsh 봇이 DM으로 보냅니다.");
   else if(claimState!=null)linkLabel.Text=T("브라우저에서 Discord 승인 대기 중…");
   else if(hook.Text.Trim().Length>0)linkLabel.Text=relay.Length>0?T("고급 웹후크로 전송합니다."):T("웹후크 채널로 알림을 보냅니다.");
   else if(relay.Length==0)linkLabel.Text=T("아래 웹후크의 채널로 알림이 옵니다.");
   else linkLabel.Text=T("아직 연결 안 됨 · 오른쪽 버튼을 눌러 디스코드에서 승인하세요.");
  }
  static string RandomHex(){var b=new byte[16];using(var rng=System.Security.Cryptography.RandomNumberGenerator.Create())rng.GetBytes(b);return string.Concat(b.Select(x=>x.ToString("x2")));}
  void StartConnect(){
   if(relay.Length==0){DStatus(T("간편 연결이 아직 설정되지 않은 빌드입니다. 고급 웹후크를 사용하세요."));return;}
   claimState=RandomHex();claimUntil=DateTime.UtcNow.AddMinutes(5);nextClaim=DateTime.UtcNow.AddSeconds(3);
   // Default browser: one Discord approval click (log in first if needed). Joining the server and DMs follow automatically.
   Process.Start(relay+"/connect?state="+claimState+"&lang="+Language);
   DStatus(T("Discord 승인 창을 열었습니다. 승인하면 자동으로 연결됩니다. (5분 대기)"));UpdateLink();
  }
  void UpdatePhone(){
   phoneLabel.Text=relay.Length==0?T("이 빌드에는 휴대폰 알림 서버가 없습니다."):phonePaired?T("연결됨 · 이 휴대폰으로 바로 알림이 옵니다."):DateTime.UtcNow<phoneWatchUntil?T("휴대폰에서 알림 허용을 기다리는 중…"):T("아직 연결 안 됨 · 오른쪽 버튼을 눌러 주세요.");
   phoneLabel.Dot=relay.Length>0&&phonePaired?Color.FromArgb(95,211,141):relay.Length>0&&DateTime.UtcNow<phoneWatchUntil?Color.FromArgb(255,196,84):Color.FromArgb(158,174,204);phoneLabel.Invalidate();
  }
  void StartPhone(){
   if(relay.Length==0)return;
   if(phoneCode.Length==0){phoneCode=RandomHex();config.phoneCode=Protect(phoneCode);config.Save(settingsPath);}
   phoneWatchUntil=DateTime.UtcNow.AddMinutes(10);phoneNext=DateTime.UtcNow.AddSeconds(3);
   UpdatePhone();ShowPhonePair();
  }
  static object ReadJson(HttpResponseMessage r){return Json.Parse(r.Content.ReadAsStringAsync().GetAwaiter().GetResult());}
  // The QR in BlueAsh's own window (the relay draws it: /phone/qr.gif). "Connected" means a phone subscribed after the
  // window opened (/phone/status "at" changed): a PC that is already paired used to show "connected" a moment later.
  void ShowPhonePair(){
   var pairUrl=relay+"/phone/pair?c="+phoneCode+"&lang="+Language;
   var f=new Form();f.Text=T("BlueAsh · 휴대폰 알림 연결");Brand.Apply(f,appExe);f.StartPosition=FormStartPosition.CenterScreen;f.FormBorderStyle=FormBorderStyle.FixedDialog;f.MaximizeBox=false;f.MinimizeBox=false;f.Font=uiFont;f.TopMost=true;
   f.ClientSize=new Size(Px(420),Px(600));
   var title=new Label{Text=T("휴대폰 알림 연결"),Font=UiFont.New(fontName,15f,FontStyle.Bold)};title.SetBounds(Px(24),Px(20),Px(372),Px(34));f.Controls.Add(title);
   // Two numbered steps (a small rounded number badge each) instead of one sentence.
   var stepCards=new List<Control>();var stepNumbers=new List<Control>();var stepTexts=new List<Label>();int i=0;
   foreach(var s in new[]{T("휴대폰 카메라로 QR을 찍습니다."),T("열린 페이지에서 알림 허용을 누릅니다.")}){int y=64+i*34;
    var n=new Panel();n.SetBounds(Px(24),Px(y),Px(26),Px(26));string num=(i+1).ToString(CultureInfo.InvariantCulture);var d=n;
    // A circle with its digit centred on the glyph's own outline (a label's text box sits off-centre: line height, padding).
    n.Paint+=(o,e)=>{var g=e.Graphics;g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;g.Clear(n.Parent!=null?n.Parent.BackColor:Theme.Bg);
     using(var b=new SolidBrush(Theme.Field))g.FillEllipse(b,0.5f,0.5f,n.Width-1.5f,n.Height-1.5f);using(var p=new Pen(Theme.Border))g.DrawEllipse(p,0.5f,0.5f,n.Width-1.5f,n.Height-1.5f);
     using(var path=new System.Drawing.Drawing2D.GraphicsPath()){path.AddString(num,n.Font.FontFamily,(int)FontStyle.Bold,n.Font.SizeInPoints*g.DpiY/72f,new PointF(0,0),StringFormat.GenericTypographic);var r=path.GetBounds();
      using(var m=new System.Drawing.Drawing2D.Matrix()){m.Translate(n.Width/2f-(r.X+r.Width/2f),n.Height/2f-(r.Y+r.Height/2f));path.Transform(m);}using(var b=new SolidBrush(Theme.AccentText))g.FillPath(b,path);}};
    f.Controls.Add(n);
    var t=new Label{Text=s,TextAlign=ContentAlignment.MiddleLeft};t.SetBounds(Px(58),Px(y-3),Px(338),Px(30));f.Controls.Add(t);stepCards.Add(n);stepNumbers.Add(d);stepTexts.Add(t);i++;}
   // White plate behind the code: phone cameras read a dark-on-light QR best.
   var plate=new ThemeCard();plate.SetBounds(Px(70),Px(140),Px(280),Px(280));f.Controls.Add(plate);
   var pic=new PictureBox{SizeMode=PictureBoxSizeMode.Zoom};pic.SetBounds(Px(14),Px(14),Px(252),Px(252));plate.Controls.Add(pic);
   // The state under the code in colour: amber while waiting, green when connected, red on an error.
   var state=new Label{Text=T("QR을 불러오는 중…"),TextAlign=ContentAlignment.MiddleCenter};state.SetBounds(Px(24),Px(432),Px(372),Px(52));f.Controls.Add(state);
   var open=new ThemeButton{Text=T("브라우저로 열기")};open.SetBounds(Px(24),Px(532),Px(180),Px(40));f.Controls.Add(open);open.Click+=delegate{Process.Start(pairUrl);};
   var close=new ThemeButton{Text=T("닫기")};close.SetBounds(Px(276),Px(532),Px(120),Px(40));f.Controls.Add(close);close.Click+=delegate{f.Close();};f.CancelButton=close;
   Theme.Apply(f);plate.BackColor=Color.White;pic.BackColor=Color.White;
   foreach(var c in stepCards)c.BackColor=f.BackColor;
   state.ForeColor=Color.FromArgb(255,196,84);
   // Buttons in the same size as the text around them (the form font is larger than the rest of the app reads).
   var small=UiFont.New(fontName,9.5f);foreach(var c in new Control[]{open,close,state}.Concat(stepTexts).Concat(stepNumbers))c.Font=small;
   Task<byte[]> pairQr=http.GetByteArrayAsync(relay+"/phone/qr.gif?c="+phoneCode+"&lang="+Language);Task<HttpResponseMessage> pairStatus=null;double? pairBase=null;bool pairDone=false;DateTime pairAt=DateTime.MinValue,pairNext=DateTime.MinValue;
   var timer=new Timer{Interval=250};
   timer.Tick+=delegate{
    if(pairQr!=null&&pairQr.IsCompleted){try{pic.Image=Image.FromStream(new MemoryStream(pairQr.GetAwaiter().GetResult()));}catch{state.ForeColor=Color.FromArgb(255,100,110);state.Text=T("QR을 불러오지 못했습니다. 브라우저로 열기를 눌러 주세요.");}pairQr=null;}
    if(pairStatus!=null&&pairStatus.IsCompleted){
     try{using(var r=pairStatus.GetAwaiter().GetResult()){if(r.IsSuccessStatusCode){var j=ReadJson(r);double at=Json.Num(Json.Get(j,"at"),0);bool paired=Json.Bool(Json.Get(j,"paired"),false);
      if(pairBase==null){pairBase=at;state.Text=paired?T("이미 연결된 휴대폰이 있습니다. 다른 휴대폰으로 바꾸려면 QR을 찍으세요."):T("휴대폰에서 알림 허용을 기다리는 중…");}
      else if(paired&&at!=pairBase.Value&&!pairDone){pairDone=true;phonePaired=true;phoneWatchUntil=DateTime.MinValue;UpdatePhone();
       state.ForeColor=Color.FromArgb(95,211,141);state.Text=T("연결 완료! 휴대폰에 확인 알림이 왔는지 보세요.");DStatus(T("휴대폰 알림 연결 완료 · 휴대폰에 확인 알림이 왔는지 보세요."));pairAt=DateTime.UtcNow;}}}}catch{}
     pairStatus=null;}
    if(pairStatus==null&&!pairDone&&DateTime.UtcNow>=pairNext){pairNext=DateTime.UtcNow.AddSeconds(2);pairStatus=http.GetAsync(relay+"/phone/status?c="+phoneCode);}
    if(pairDone&&(DateTime.UtcNow-pairAt).TotalSeconds>3)f.Close();
   };
   f.FormClosed+=delegate{timer.Stop();timer.Dispose();if(pic.Image!=null)pic.Image.Dispose();};
   timer.Start();f.ShowDialog();f.Dispose();
  }
  bool PhoneSend(string kind,string[] values){
   if(relay.Length==0||phoneCode.Length==0||!phonePaired||phoneSendTask!=null)return false;
   var body=new Dictionary<string,object>{{"c",phoneCode},{"kind",kind},{"language",Language}};if(values!=null&&values.Length>0)body["values"]=values;
   phoneSendContent=new StringContent(Json.Write(body),Encoding.UTF8,"application/json");
   phoneSendTask=http.PostAsync(relay+"/phone/notify",phoneSendContent);return true;
  }
  void PhonePump(){
   if(phoneStatusTask!=null&&phoneStatusTask.IsCompleted){
    try{using(var r=phoneStatusTask.GetAwaiter().GetResult()){if(r.IsSuccessStatusCode){bool was=phonePaired;phonePaired=Json.Bool(Json.Get(ReadJson(r),"paired"),false);
     if(phonePaired&&!was&&DateTime.UtcNow<phoneWatchUntil){phoneWatchUntil=DateTime.MinValue;DStatus(T("휴대폰 알림 연결 완료 · 휴대폰에 확인 알림이 왔는지 보세요."));}}}}catch{}
    phoneStatusTask=null;phoneChecked=true;UpdatePhone();
   }
   // Ask once at start (already paired?), then every 3 s while waiting for the phone.
   if(phoneStatusTask==null&&relay.Length>0&&phoneCode.Length>0&&DateTime.UtcNow>=phoneNext&&(!phoneChecked||DateTime.UtcNow<phoneWatchUntil)){
    phoneNext=DateTime.UtcNow.AddSeconds(3);phoneStatusTask=http.GetAsync(relay+"/phone/status?c="+phoneCode);}
   if(phoneSendTask!=null&&phoneSendTask.IsCompleted){
    try{using(var r=phoneSendTask.GetAwaiter().GetResult()){int code=(int)r.StatusCode;
     if(r.IsSuccessStatusCode)DStatus(T("휴대폰으로 알림을 보냈습니다."));
     else if(code==410){phonePaired=false;UpdatePhone();DStatus(T("휴대폰 연결이 끊겼습니다 (알림을 껐거나 브라우저 데이터를 지움). 휴대폰 연결을 다시 눌러 주세요."));}
     else if(code==429)DStatus(T("휴대폰 알림은 1분에 1번까지 보냅니다."));
     else DStatus(T("휴대폰 알림 전송 실패 · 서버 응답 ")+code);
    }}catch{DStatus(T("휴대폰 알림 전송 실패 · 인터넷 연결을 확인하세요."));}
    phoneSendTask=null;if(phoneSendContent!=null){phoneSendContent.Dispose();phoneSendContent=null;}
   }
  }
  void ClaimPump(){
   if(claimTask!=null&&claimTask.IsCompleted){
    try{using(var r=claimTask.GetAwaiter().GetResult()){
     if((int)r.StatusCode==200){var key=Json.Str(Json.Get(ReadJson(r),"key"));
      if(key.Length>0){relayKey=key;claimState=null;disabledTransport=false;
       if(!discordCheck.Checked)discordCheck.Checked=true;
       UpdateLink();if(SaveConfig())DStatus(T("Discord 연결 완료 · BlueAsh 서버에 들어갔고 DM으로 알림을 받습니다. 테스트 전송으로 확인해 보세요."));}}
    }}catch{}
    claimTask=null;
   }
   if(claimState!=null&&claimTask==null&&DateTime.UtcNow>=nextClaim){
    if(DateTime.UtcNow>claimUntil){claimState=null;DStatus(T("Discord 승인 대기 시간이 지났습니다. 다시 연결을 눌러 주세요."));UpdateLink();return;}
    nextClaim=DateTime.UtcNow.AddSeconds(2);claimTask=http.GetAsync(relay+"/claim?state="+claimState);
   }
  }
  void NetworkPump(){
   ClaimPump();PhonePump();
   if(sendTask!=null&&sendTask.IsCompleted){
    bool viaRelay=!string.IsNullOrEmpty(sendItem.Relay);
    try{using(var r=sendTask.GetAwaiter().GetResult()){int code=(int)r.StatusCode;
     if(r.IsSuccessStatusCode)DStatus(T("Discord 전송 완료"));
     else if(viaRelay&&code==429)DStatus(T("Discord 알림 간격 제한 (1분에 1회) · 이번 알림은 건너뜁니다."));
     else if(viaRelay&&code==401){relayKey="";config.relayKey="";config.Save(settingsPath);UpdateLink();DStatus(T("Discord 연결이 만료되었습니다. 다시 연결을 눌러 주세요."));}
     else if(viaRelay)DStatus(T("Discord DM 전송 실패 · BlueAsh 서버를 나갔거나 서버 멤버 DM이 꺼져 있습니다. 다시 연결하거나 Discord 개인정보 설정을 확인하세요."));
     else if(code==429){double wait=Math.Max(1,Json.Num(Json.Get(ReadJson(r),"retry_after"),1));sendAfter=DateTime.UtcNow.AddSeconds(wait);if(sendItem.Tries<3)queue.Enqueue(sendItem);DStatus(T("Discord 요청 제한 · 지정된 시간 이후 재시도"));}
     else if(code==401||code==403||code==404){disabledTransport=true;DStatus(T("Discord 권한/주소 오류 · 주소 수정 후 테스트 버튼을 눌러 주세요."));}
     else DStatus(T("Discord 실패 HTTP ")+code);
    }}catch{DStatus(T("Discord 전송 실패 · 자동 중복 전송하지 않습니다."));}
    sendTask=null;sendItem=null;if(sendContent!=null){sendContent.Dispose();sendContent=null;}
   }
   if(sendTask==null&&queue.Count>0&&DateTime.UtcNow>=sendAfter){
    var item=queue.Dequeue();if(item.Type=="raid.started.away"&&!config.discord)return;if(item.Type.StartsWith("maintenance.")&&!config.maintenance)return;if(item.Expires<DateTime.UtcNow)return;
    item.Tries++;string url,payload;
    if(!string.IsNullOrEmpty(item.Relay)&&relay.Length>0){
     // The relay owns the wording; only official notice text is passed along as detail.
     var detail=item.Type.StartsWith("maintenance.")?item.Text:"";
     payload=Json.Write(new Dictionary<string,object>{{"key",item.Relay},{"kind",item.Type},{"detail",detail},{"language",Language}});url=relay+"/notify";
    }else{
     if(disabledTransport)return;url=item.Target;if(!ExitMatch.ValidWebhook(url)){DStatus(T("Discord 연결 또는 웹후크가 설정되지 않았습니다."));return;}
     payload=Json.Write(new Dictionary<string,object>{{"content",item.Text},{"allowed_mentions",new Dictionary<string,object>{{"parse",new object[0]}}}});url=url+"?wait=true";
    }
    sendContent=new StringContent(payload,Encoding.UTF8,"application/json");sendItem=item;sendTask=http.PostAsync(url,sendContent);sendAfter=DateTime.UtcNow.AddSeconds(2);
   }
  }
 }
}
