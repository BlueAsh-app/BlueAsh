using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using Microsoft.Win32;
using ModeSwitchPrice;
using BlueAshUi;
namespace ModePulse {
 public sealed partial class CompanionApp {
  // ---- 은신처 레벨 (아이템 시세 표시에서 사용) ----
  // Hideout levels per game mode, used by the price overlay to show only upgrades still ahead.
  // Stations come from the price worker's cached tarkov.dev data (tasks-<mode>.tsv).
  sealed class HideoutCard{public ThemeCard Card;public Label Name,Max;public List<ThemeButton> Buttons=new List<ThemeButton>();public ThemeButton Lock;}
  void ShowHideoutLevels(){
   var levelsPath=Path.Combine(store,"hideout-levels.json");var logs=Reg("RaidLogs");
   var hideoutMode="pve";try{var m=PriceLog.ReadLogMode(logs);if(m.Length>0)hideoutMode=m;}catch{}
   var gameIni=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Battlestate Games","Escape from Tarkov","Settings","Game.ini");
   bool edition=TaskIndex.StashMaxedByEdition(gameIni);
   // One card per facility with a button per level: one click sets and saves it (no spinners, no save button).
   // The window can be resized: the number of card columns, the card width and the level buttons follow it (relayout).
   var f=new Form();f.Text=T("BlueAsh · 은신처 진행도");Brand.Apply(f,appExe);f.StartPosition=FormStartPosition.CenterScreen;f.FormBorderStyle=FormBorderStyle.Sizable;f.MaximizeBox=true;f.MinimizeBox=false;f.Font=uiFont;f.TopMost=true;
   f.MinimumSize=new Size(Px(560),Px(420));f.ClientSize=new Size(Px(720),Px(700));
   var sz=Reg("HideoutWindow");var szm=Regex.Match(sz,@"^(\d+),(\d+)$");
   if(szm.Success)f.ClientSize=new Size(Math.Max(Px(540),int.Parse(szm.Groups[1].Value)),Math.Max(Px(380),int.Parse(szm.Groups[2].Value)));
   var bold=UiFont.New(fontName,10.5f,FontStyle.Bold);var small=UiFont.New(fontName,9f);
   var top=new Panel{Dock=DockStyle.Top,Height=Px(112)};f.Controls.Add(top);
   var title=new Label{Text=T("은신처 진행도"),Font=UiFont.New(fontName,15f,FontStyle.Bold),AutoSize=true,Location=new Point(Px(18),Px(12))};top.Controls.Add(title);
   var modeButtons=new Dictionary<string,ThemeButton>();var modeOrder=new List<ThemeButton>();
   foreach(var pair in new[]{new[]{"pve","PvE"},new[]{"pvp-season",T("PvP 시즌")},new[]{"regular",T("PvP 일반")}}){var b=new ThemeButton{Text=pair[1],Tag=pair[0],Size=new Size(Px(112),Px(32))};top.Controls.Add(b);modeButtons[pair[0]]=b;modeOrder.Add(b);}
   var hint=new Label{Text=T("시설마다 지금 지어진 레벨을 누르세요 (0 = 아직 안 지음). 바로 저장되고, 모드마다 따로 기억합니다.")+"\r\n"+T("아무것도 누르지 않으면 시세 창에 모든 레벨의 재료를 보여 줍니다."),Location=new Point(Px(18),Px(56))};top.Controls.Add(hint);
   var flow=new FlowLayoutPanel{Dock=DockStyle.Fill,AutoScroll=true,Padding=new Padding(Px(12),Px(2),0,Px(8))};f.Controls.Add(flow);flow.BringToFront();DarkScroll(flow);
   var bottom=new Panel{Dock=DockStyle.Bottom,Height=Px(56)};f.Controls.Add(bottom);
   var note=new Label{Location=new Point(Px(18),Px(18)),AutoEllipsis=true};bottom.Controls.Add(note);
   var clear=new ThemeButton{Text=T("입력 지우기"),Size=new Size(Px(140),Px(34))};bottom.Controls.Add(clear);
   var close=new ThemeButton{Text=T("닫기"),Size=new Size(Px(112),Px(34))};bottom.Controls.Add(close);
   var ixs=new Dictionary<string,TaskIndex>();var wait=new Timer{Interval=400};
   var levels=new Dictionary<string,int>();var buttons=new Dictionary<string,List<ThemeButton>>();var cards=new List<HideoutCard>();var locks=new List<ThemeButton>();bool entered=false;
   Action relayout=null,paint=null,fill=null;Func<bool> save=null;
   // Everything that depends on the window width: header and footer positions, cards per row, card width, level buttons.
   relayout=delegate{
    // Header: mode buttons beside the title when they fit, else on the next line; the hint as tall as its wrapped text.
    int w=f.ClientSize.Width,bw=Px(3*112+2*6),x=w-Px(18)-bw,by=Px(14);if(x<title.Right+Px(16)){x=Px(18);by=title.Bottom+Px(8);}
    foreach(var b in modeOrder){b.Location=new Point(x,by);x+=Px(118);}
    int hy=Math.Max(title.Bottom,by+Px(32))+Px(8),hw=Math.Max(Px(200),w-Px(36)),hh=TextRenderer.MeasureText(hint.Text,hint.Font,new Size(hw,0),TextFormatFlags.WordBreak).Height;
    hint.SetBounds(Px(18),hy,hw,hh+Px(4));top.Height=hy+hh+Px(12);
    close.Location=new Point(w-Px(18+112),Px(11));clear.Location=new Point(w-Px(18+112+8+140),Px(11));note.Size=new Size(Math.Max(Px(120),w-Px(18+112+8+140+30)),Px(26));
    int avail=flow.ClientSize.Width-flow.Padding.Left-SystemInformation.VerticalScrollBarWidth-4;
    int cols=Math.Max(1,(avail+Px(12))/(Px(300)+Px(12)));int cw=(avail+Px(12))/cols-Px(12);
    flow.SuspendLayout();
    foreach(var c in cards){c.Card.Width=cw;c.Max.Location=new Point(cw-Px(12)-c.Max.PreferredWidth,Px(10));c.Name.Width=Math.Max(Px(60),cw-Px(24)-c.Max.PreferredWidth-Px(8));
     if(c.Lock!=null){c.Lock.SetBounds(Px(12),Px(36),cw-Px(24),Px(28));c.Lock.BringToFront();continue;}
     int n=c.Buttons.Count,step=Math.Min(Px(56),(cw-Px(24)+Px(6))/n),bx=Px(12);foreach(var b in c.Buttons){b.SetBounds(bx,Px(36),step-Px(6),Px(28));bx+=step;}}
    flow.ResumeLayout();
   };
   paint=delegate{
    foreach(var p in modeButtons){bool on=p.Key==hideoutMode;p.Value.BackColor=on?Theme.Accent:Theme.Field;p.Value.ForeColor=Theme.Ink;}
    foreach(var p in buttons)foreach(var b in p.Value){int v=(int)b.Tag;int have;levels.TryGetValue(p.Key,out have);bool on=entered&&v==have;b.BackColor=on?Theme.Accent:Theme.Field;b.ForeColor=Theme.Ink;}
    foreach(var l in locks){l.BackColor=Theme.Accent;l.ForeColor=Color.White;}
    int built=levels.Values.Count(v=>v>0);
    note.Text=entered?T("저장됨 · 지어진 시설 {0} / {1}",built,levels.Count):T("입력 안 함 · 필요한 레벨 모두 표시 중");
   };
   save=delegate{
    var d=new Dictionary<string,int>(levels);
    try{TaskIndex.WriteLevels(levelsPath,hideoutMode,d);return true;}catch(Exception ex){note.Text=T("저장 실패: ")+ex.Message;return false;}
   };
   fill=delegate{
    flow.SuspendLayout();foreach(Control c in flow.Controls.Cast<Control>().ToList())c.Dispose();flow.Controls.Clear();buttons.Clear();levels.Clear();cards.Clear();locks.Clear();
    // Station lists come from the price worker's cache, which only holds the modes played so far: a missing (or old format) one is fetched here.
    TaskIndex ix;if(!ixs.TryGetValue(hideoutMode,out ix)){ix=new TaskIndex(hideoutMode,store);ix.LoadCache();ixs[hideoutMode]=ix;}
    bool have=ix.Ready;if(!have){ix.RefreshIfStale(TimeSpan.FromHours(6));if(ix.Refreshing)wait.Start();}
    // A prestige or season wipe seen in the logs after the levels were saved starts the hideout over.
    bool wiped=false;try{wiped=TaskIndex.ClearIfOlder(levelsPath,hideoutMode,Quests.ResetAt(logs.Length>0?logs:@"C:\Battlestate Games\Escape from Tarkov\Logs",hideoutMode));}catch{}
    var saved=TaskIndex.ReadLevels(levelsPath,hideoutMode);entered=saved!=null;
    var stations=ix.Stations==null?new List<TaskIndex.Station>():ix.Stations.ToList();
    if(!have||stations.Count==0){flow.ResumeLayout();paint();note.Text=ix.Refreshing?T("시설 목록을 받아오는 중..."):!string.IsNullOrEmpty(ix.Error)?T("시설 목록을 받지 못했습니다 · 인터넷 연결을 확인하세요"):T("시설 목록이 없습니다");return;}
    bool stashAuto=false;
    // Unheard comes with the Cultist Circle built: not something to track here (and its materials are skipped in the price tags).
    bool cultistBuilt=TaskIndex.CultistByEdition(gameIni);
    foreach(var s in stations){
     bool isStash=s.Id==TaskIndex.StashId;
     // Edition-given stations: the stash (Unheard/EOD) and the cultist circle (Unheard only) are fixed at max.
     bool fixd=(isStash&&edition)||(cultistBuilt&&s.Id==TaskIndex.CultistId);
     var card=new ThemeCard{Size=new Size(Px(300),Px(74)),Margin=new Padding(0,0,Px(12),Px(10))};
     var name=new Label{Text=s.Name,Font=bold};name.SetBounds(Px(12),Px(8),Px(200),Px(26));card.Controls.Add(name);
     var max=new Label{Text=T("최대 Lv.")+s.MaxLevel,Font=small,AutoSize=true};card.Controls.Add(max);
     int v=0,sv;if(saved!=null&&saved.TryGetValue(s.Id,out sv))v=Math.Min(s.MaxLevel,sv);
     // Edition-given stations start at their last level: set (and save) it, so they count as built.
     if(fixd&&v<s.MaxLevel){v=s.MaxLevel;stashAuto=true;}
     levels[s.Id]=v;
     // Fixed by the edition: a badge instead of level buttons.
     if(fixd){var lk=new ThemeButton{TabStop=false,Cursor=Cursors.Default,Text="Lv."+s.MaxLevel+T(" 고정 · 에디션 기본")};card.Controls.Add(lk);locks.Add(lk);cards.Add(new HideoutCard{Card=card,Name=name,Max=max,Lock=lk});flow.Controls.Add(card);continue;}
     int count=Math.Max(1,s.MaxLevel)+1;var list=new List<ThemeButton>();var sid=s.Id;
     for(int lv=0;lv<count;lv++){var b=new ThemeButton{Text=lv.ToString(CultureInfo.InvariantCulture),Tag=lv};int pickLv=lv;b.Click+=delegate{levels[sid]=pickLv;entered=true;if(save())paint();};card.Controls.Add(b);list.Add(b);}
     buttons[s.Id]=list;cards.Add(new HideoutCard{Card=card,Name=name,Max=max,Buttons=list});flow.Controls.Add(card);}
    if(stashAuto){entered=true;save();}
    Theme.Apply(f);flow.BackColor=Theme.Bg;flow.ResumeLayout();relayout();paint();
    if(wiped)note.Text=T("로그에서 프레스티지/시즌 초기화를 확인해 레벨을 처음부터 시작합니다");
   };
   wait.Tick+=delegate{TaskIndex ix;if(!ixs.TryGetValue(hideoutMode,out ix)||!ix.Refreshing){wait.Stop();fill();}};
   foreach(var b in modeButtons.Values){var tag=(string)b.Tag;b.Click+=delegate{hideoutMode=tag;fill();};}
   clear.Click+=delegate{TaskIndex.WriteLevels(levelsPath,hideoutMode,null);fill();};
   close.Click+=delegate{f.Close();};
   f.Resize+=delegate{relayout();};flow.SizeChanged+=delegate{relayout();};
   // The flow panel has its real width only once the window is up: lay the cards out again and start at the top
   // (focusing the first level button would otherwise scroll the list to it).
   f.Shown+=delegate{relayout();f.ActiveControl=close;flow.AutoScrollPosition=new Point(0,0);};f.FormClosed+=delegate{wait.Stop();wait.Dispose();};
   f.FormClosing+=delegate{if(f.WindowState==FormWindowState.Normal)SetReg("HideoutWindow",f.ClientSize.Width+","+f.ClientSize.Height);};
   Theme.Apply(f);fill();relayout();f.ShowDialog();f.Dispose();
  }
 }
}
