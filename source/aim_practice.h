#pragma once
// PUBG 에임랩 – BlueAsh's own practice screen (not PUBG): a full-screen window that turns a view by the mouse's raw
// counts (WM_INPUT) at PUBG's speed for the saved sensitivity, and runs the tests with candidate multipliers in shuffled
// order: 표적 전환 (flick to a target and click), 수직 반동 (hold the button while the view climbs, pull down) and, when
// chosen in 고급 설정, 조준(ADS) 표적 전환 (the same flick at the 조준 sensitivity and a narrower view). Nothing is
// captured: the screen is drawn here, so target and crosshair positions are known exactly. Raw input is registered only
// while measuring and removed when it stops; the keyboard is not read except the start/stop key and Esc.
// Physical pixels (dpi::Raw). Included by aim_ui.h after the shared helpers.
#include <mmsystem.h>
#include <functional>
#include "aim_core.h"
namespace aimprac {
struct Options {double conv=0,vertical=1,adsConv=0,radius=1.4,yaw=aim::defaultYaw;unsigned key=VK_INSERT;bool ads=false,debug=false,diag=false;int test=0;double speed=1;}; // speed: 표적 반응 crosshair speed (x a 1080p desktop at 1 pixel a count); test: 1 반응 속도, 2 표적 반응 (0: the old sensitivity runs) // yaw: degrees per count per converted unit (2.5 estimate, or the player's calibration); diag: 입력 진단 instead of the tests
struct Result {std::vector<aim::Trial> trials;aim::Verdict flick,recoil,ads;int flickDone=0,recoilDone=0,adsDone=0;double conv=0,vertical=1,adsConv=0,yaw=aim::defaultYaw;bool stopped=false,withAds=false,calibrated=false;};
const std::vector<double> cands={.95,1.0,1.05};const int flickBlocks=6,recoilBlocks=4,adsBlocks=4,per=2;
// shortcut: the ADS view is drawn with a 70-degree width; PUBG's real ADS zoom per sight is not published. Fine for
// comparing candidates against each other, not for matching PUBG's ADS picture exactly.
const double climbDegPerS=7.0,holdS=1.8,minHoldS=1.2,hipHalf=45.0,adsHalf=35.0;
void(*onDone)(const Result&)=nullptr; // the page stores the result even if it was closed meanwhile
HWND win=nullptr,owner=nullptr;bool measuring=false,offscreen=false; // offscreen: test builds, the cursor is never clipped or hidden
Options opt;int phase=0; // 0 ready, 1 flick, 2 recoil, 3 ADS flick
double yaw=0,pitch=0,tYaw=0,tPitch=0,trialAt=-1,gapUntil=0,holdAt=-1,lastTick=0;bool targetUp=false,holding=false;
std::vector<double> flickOrder,recoilOrder,adsOrder;size_t idx=0;std::vector<aim::Sample> samples;Result res;std::mt19937 rng;
LARGE_INTEGER freq={};double now(){LARGE_INTEGER c;QueryPerformanceCounter(&c);return (double)c.QuadPart/freq.QuadPart;}
double endingAt=0,shownProgress=0,paintAt=0; // endingAt: the last trial is done, the 분석 완료 check plays for a moment before the result is kept
struct Burst {double yaw,pitch,at;bool hit;};std::vector<Burst> bursts; // a ring spreading from where a target was hit (or missed)
double frames=0,fps=0,fpsAt=0,lastPaint=0;HDC backDC=nullptr;HBITMAP backBmp=nullptr;int backW=0,backH=0;
bool flickPhase(){return phase==1||phase==3;}
const std::vector<double>& order(){return phase==1?flickOrder:phase==2?recoilOrder:adsOrder;}
double mult(){const auto& o=order();return idx<o.size()?o[idx]:1.0;}
double conv(){return phase==3?opt.adsConv:opt.conv;}
double fovHalf(){return phase==3?adsHalf:hipHalf;}
int total(){return (int)(flickOrder.size()+recoilOrder.size()+adsOrder.size());}
int done(){return (int)(phase==1?idx:phase==2?flickOrder.size()+idx:flickOrder.size()+recoilOrder.size()+idx);}
void sample(){if(trialAt<0)return;samples.push_back({now()-trialAt,tYaw-yaw,tPitch-pitch});}
void spawnFlick(){double reach=phase==3?20:28;std::uniform_real_distribution<double> a(0,6.283185307179586),d(phase==3?6:8,reach);double th=a(rng),dist=d(rng),dx=std::cos(th)*dist,dy=std::sin(th)*dist*.55;
    tYaw=yaw+dx;tPitch=pitch+dy;targetUp=true;samples.clear();trialAt=now();sample();}
void spawnRecoil(){tYaw=yaw;tPitch=pitch;targetUp=true;samples.clear();trialAt=-1;holding=false;holdAt=-1;}
void next(double gap){targetUp=false;trialAt=-1;gapUntil=now()+gap;}
void finish(bool stopped);
void reactTick(double t);void reactInput(long dx,long dy,USHORT buttons);
void stop(){if(endingAt>0){endingAt=0;finish(false);}else finish(true);}
void advance(){idx++;if(phase==1&&idx>=flickOrder.size()){phase=2;idx=0;}if(phase==2&&idx>=recoilOrder.size()){phase=3;idx=0;}if(phase==3&&idx>=adsOrder.size()){endingAt=now();targetUp=false;return;}next(phase==2?.5:.35);}
void begin(){if(measuring)return;measuring=true;endingAt=0;shownProgress=0;bursts.clear();phase=1;idx=0;res=Result();res.conv=opt.conv;res.vertical=opt.vertical;res.adsConv=opt.adsConv;res.withAds=opt.ads;res.yaw=opt.yaw;res.calibrated=std::fabs(opt.yaw-aim::defaultYaw)>1e-9;rng.seed((unsigned)GetTickCount64());
    flickOrder=aim::schedule(cands,flickBlocks,per,rng());recoilOrder=aim::schedule(cands,recoilBlocks,per,rng());adsOrder=opt.ads?aim::schedule(cands,adsBlocks,per,rng()):std::vector<double>();
    RAWINPUTDEVICE rid={1,2,0,win};RegisterRawInputDevices(&rid,1,sizeof(rid));timeBeginPeriod(1);
    if(!offscreen){RECT r;GetWindowRect(win,&r);ClipCursor(&r);while(ShowCursor(FALSE)>=0){}}next(.6);}
void releaseInput(){RAWINPUTDEVICE rid={1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&rid,1,sizeof(rid));if(!offscreen){ClipCursor(nullptr);while(ShowCursor(TRUE)<0){}}}
void finish(bool stopped){if(!win)return;if(measuring){releaseInput();timeEndPeriod(1);}bool was=measuring;measuring=false;
    if(opt.test){HWND w=win;win=nullptr;DestroyWindow(w);return;}
    res.stopped=stopped;for(auto& t:res.trials){if(t.kind==aim::Flick)res.flickDone++;else if(t.kind==aim::Recoil)res.recoilDone++;else res.adsDone++;}
    aim::prune(res.trials,3);res.flick=aim::decide(res.trials,aim::Flick,6);res.recoil=aim::decide(res.trials,aim::Recoil,4);if(res.withAds)res.ads=aim::decide(res.trials,aim::Ads,4);
    HWND w=win;win=nullptr;DestroyWindow(w);if(was&&onDone)onDone(res);if(was&&owner&&IsWindow(owner))PostMessageW(owner,WM_APP+240,0,0);}
void addFlick(aim::Trial t){t.kind=phase==3?aim::Ads:aim::Flick;res.trials.push_back(t);}
// Called often (raw input, timer): the recoil climb, the gap between trials, timeouts.
void tick(){double t=now(),dt=lastTick?std::min(.05,t-lastTick):0;lastTick=t;if(opt.test){reactTick(t);return;}if(!measuring)return;
    if(endingAt>0){if(t-endingAt>=1.15){endingAt=0;finish(false);}return;}
    if(!targetUp){if(t>=gapUntil){if(phase==2)spawnRecoil();else spawnFlick();}return;}
    if(flickPhase()&&trialAt>=0&&t-trialAt>3.0){aim::Trial slow;slow.mult=mult();addFlick(slow);advance();return;} // too slow: recorded as not valid
    if(phase==2&&holding){pitch+=climbDegPerS*dt;sample();if(t-holdAt>=holdS){holding=false;bursts.push_back({tYaw,tPitch,t,true});res.trials.push_back(aim::recoil(samples,mult()));advance();}}}
// One mouse report: relative counts and the left button. PUBG's vertical multiplier scales y; the candidate multiplies
// both axes in the flick tests and only y in 수직 반동.
void input(long dx,long dy,USHORT buttons){tick();if(opt.test){reactInput(dx,dy,buttons);return;}
    if(dx||dy){double k=flickPhase()?mult():1.0,kv=phase==2?mult():1.0,c=conv()*opt.yaw;yaw+=dx*c*k;pitch-=dy*c*opt.vertical*k*kv;pitch=std::clamp(pitch,-80.0,80.0);sample();}
    if(buttons&RI_MOUSE_LEFT_BUTTON_DOWN){double t=now();
        if(flickPhase()&&targetUp&&trialAt>=0){bursts.push_back({tYaw,tPitch,t,std::hypot(tYaw-yaw,tPitch-pitch)<=opt.radius});addFlick(aim::flick(samples,opt.radius,t-trialAt,mult()));advance();}
        else if(phase==2&&targetUp&&!holding){holding=true;holdAt=t;trialAt=t;samples.clear();sample();}}
    if((buttons&RI_MOUSE_LEFT_BUTTON_UP)&&phase==2&&holding){holding=false;if(now()-holdAt<minHoldS){spawnRecoil();}}} // let go too early: the same trial again
// ---- 반응 속도 테스트 -------------------------------------------------------------------------------------------
// 1 반응 속도: after a random 1.4-3.6 s wait the whole screen turns cobalt; the time from the frame that shows it to the
// click (raw input) is the reaction. 5 rounds; a click before the change is "too early" and that round starts again.
// 2 표적 반응: a target appears 6-28 degrees away after 0.6-1.4 s; the time from its frame to the click that hits it.
// 10 hits; missed clicks are counted. Steps: 1 wait, 2 shown, 3 a round's result, 4 too early, 5 the summary.
struct React {int step=0;double goAt=0,shownAt=0,stepAt=0,summaryAt=0;std::vector<double> ms;int misses=0;double last=0,aimX=0,aimY=0,px=0,py=0;} rx; // px/py: the crosshair in screen pixels from the middle // aimX/Y: 표적 반응's crosshair on a still screen (degrees from the middle)
void(*onReact)(int test,double avg,double best,int n,int misses)=nullptr;void(*onSpeed)(double)=nullptr; // the page keeps the record
int rounds(){return opt.test==1?5:10;}
void reactNext(){std::uniform_real_distribution<double> d(opt.test==1?1.4:.35,opt.test==1?3.6:.9);rx.goAt=now()+d(rng);rx.shownAt=0;rx.step=1;rx.stepAt=now();targetUp=false;}
void reactBegin(){measuring=true;rx=React();bursts.clear();yaw=pitch=0;rng.seed((unsigned)GetTickCount64());RAWINPUTDEVICE rid={1,2,0,win};RegisterRawInputDevices(&rid,1,sizeof(rid));timeBeginPeriod(1);
    if(!offscreen){RECT r;GetWindowRect(win,&r);ClipCursor(&r);while(ShowCursor(FALSE)>=0){}}reactNext();}
void reactEnd(){releaseInput();timeEndPeriod(1);measuring=false;rx.step=5;rx.summaryAt=now();
    double sum=0,best=1e9;for(double m:rx.ms){sum+=m;best=std::min(best,m);}if(!rx.ms.empty()&&onReact)onReact(opt.test,sum/rx.ms.size(),best,(int)rx.ms.size(),rx.misses);}
void reactTick(double t){if(!measuring)return;
    if(rx.step==1&&t>=rx.goAt){rx.step=2;rx.stepAt=t;if(opt.test==2){std::uniform_real_distribution<double> ux(-hipHalf*.62,hipHalf*.62),uy(-hipHalf*.32,hipHalf*.32);for(int i=0;i<20;i++){tYaw=ux(rng);tPitch=uy(rng);if(std::hypot(tYaw-rx.aimX,tPitch-rx.aimY)>=8)break;}targetUp=true;trialAt=t;}}
    else if(rx.step==3&&t-rx.stepAt>(opt.test==1?1.0:.25)){if((int)rx.ms.size()>=rounds())reactEnd();else reactNext();}
    else if(rx.step==4&&t-rx.stepAt>1.3)reactNext();}
void reactInput(long dx,long dy,USHORT buttons){double t=now();
    if(opt.test==2&&(dx||dy)&&backW>0){double g=backH/1080.0*opt.speed,f=(backW/2.0)/std::tan(hipHalf*3.14159265358979/180);rx.px=std::clamp(rx.px+dx*g,-backW*.47,backW*.47);rx.py=std::clamp(rx.py+dy*g,-backH*.46,backH*.46);
        rx.aimX=std::atan(rx.px/f)*180/3.14159265358979;rx.aimY=-std::atan(rx.py/f)*180/3.14159265358979;}
    
    if(!(buttons&RI_MOUSE_LEFT_BUTTON_DOWN))return;
    if(rx.step==1&&opt.test==1){rx.step=4;rx.stepAt=t;return;} // too early
    if(rx.step!=2||!rx.shownAt)return;
    if(opt.test==1){rx.last=(t-rx.shownAt)*1000;rx.ms.push_back(rx.last);rx.step=3;rx.stepAt=t;return;}
    bool hit=std::hypot(tYaw-rx.aimX,tPitch-rx.aimY)<=opt.radius;bursts.push_back({tYaw,tPitch,t,hit});
    if(hit){rx.last=(t-rx.shownAt)*1000;rx.ms.push_back(rx.last);targetUp=false;rx.step=3;rx.stepAt=t;}else rx.misses++;}
#ifdef BLUEASH_TT
// Test builds only (BLUEASH_AIM_SIM): a simulated player whose hand is tuned to 1.05x sensitivity, vertical and ADS, fed
// through input() like the mouse, so the whole pipeline (trials, pruning, verdict, result page) can be checked offline.
bool sim=false;double simPref=1.05,simAt=0,simPlanX=0,simPlanY=0,simDone=0,simCorrect=0;std::normal_distribution<double> simNoise(0,1);std::mt19937 simRng(11);
void simTick(){if(sim&&opt.test){if(measuring&&rx.step==2&&rx.shownAt&&now()-rx.shownAt>.19+.05*std::sin(rx.shownAt*7)){if(opt.test==2){rx.aimX=tYaw;rx.aimY=tPitch;double f=(backW/2.0)/std::tan(hipHalf*3.14159265358979/180);rx.px=std::tan(tYaw*3.14159265358979/180)*f;rx.py=-std::tan(tPitch*3.14159265358979/180)*f;}reactInput(0,0,RI_MOUSE_LEFT_BUTTON_DOWN);}return;} /* test builds: a player who reacts in about 0.2 s */
    if(!sim||!measuring||!targetUp){return;}double t=now(),cpd=1/(conv()*opt.yaw);
    if(flickPhase()){if(trialAt<0)return;double since=t-trialAt;
        if(simAt!=trialAt){simAt=trialAt;simDone=0;simCorrect=0;double dx=tYaw-yaw,dy=tPitch-pitch;simPlanX=dx*cpd/simPref*(1+.04*simNoise(simRng));simPlanY=-dy*cpd/simPref/opt.vertical*(1+.04*simNoise(simRng));}
        if(since<.18)return; // reaction
        double u=std::min(1.0,(since-.18)/.22),want=u*u*(3-2*u),step=want-simDone;simDone=want;input(std::lround(simPlanX*step),std::lround(simPlanY*step),0);
        if(u>=1){double ex=tYaw-yaw,ey=tPitch-pitch,d=std::hypot(ex,ey);if(d<=opt.radius*.8){if(since-simCorrect>.06)input(0,0,RI_MOUSE_LEFT_BUTTON_DOWN);}
            else{simCorrect=since;double c=.35;input(std::lround(ex*cpd*c),std::lround(-ey*cpd/opt.vertical*c),0);}}}
    else{if(!holding){input(0,0,RI_MOUSE_LEFT_BUTTON_DOWN);return;}double since=t-holdAt;if(since<.12)return;
        static double carry=0,last=0;double dt=last&&t-last<.05?t-last:.008;last=t;carry+=climbDegPerS*dt*cpd/opt.vertical/simPref*(1+.02*simNoise(simRng));long c=(long)carry;carry-=c;input(0,c,0);}}
#endif
std::wstring keyLabel(){return keyName(opt.key);}
// 입력 진단: shows that raw input arrives (reports a second, counts a second, the biggest single report), a dot driven
// by the counts with the delay from a report to the frame that shows it, and measures the DPI from a 10 cm move along a
// ruler (click, move, click), up to five times; the median is kept when the runs agree within 5 %.
struct Diag {int events=0,eps=0;long sx=0,sy=0,spsX=0,spsY=0,maxStep=0,lastMax=0;double secAt=0,dotX=0,dotY=0;int dpiState=0;long dpiCounts=0;std::vector<double> dpis;double pending=-1,lagSum=0,lag=0;int lagN=0;} dg;
void(*onDiag)(int dpi)=nullptr;
void diagSecond(double t){if(t-dg.secAt<1)return;dg.eps=dg.events;dg.spsX=dg.sx;dg.spsY=dg.sy;dg.lastMax=dg.maxStep;dg.events=0;dg.sx=dg.sy=0;dg.maxStep=0;dg.lag=dg.lagN?dg.lagSum/dg.lagN*1000:0;dg.lagSum=0;dg.lagN=0;dg.secAt=t;}
void diagInput(long dx,long dy,USHORT b){double t=now();if(dx||dy){dg.events++;dg.sx+=std::labs(dx);dg.sy+=std::labs(dy);dg.maxStep=std::max(dg.maxStep,std::max(std::labs(dx),std::labs(dy)));dg.dotX+=dx;dg.dotY+=dy;if(dg.pending<0){dg.pending=t;}if(dg.dpiState==1){dg.dpiCounts+=dx;}}
    if(b&RI_MOUSE_LEFT_BUTTON_DOWN){if(dg.dpiState==1){double d=aim::dpiFrom(dg.dpiCounts,10);if(d>=100){dg.dpis.push_back(d);}if(dg.dpis.size()>5){dg.dpis.erase(dg.dpis.begin());}dg.dpiState=2;}else{dg.dpiState=1;dg.dpiCounts=0;}}
    diagSecond(t);}
// The kept DPI: the median when there are two runs or more and they agree within 5 %, else 0.
double diagDpi(double* spread=nullptr){if(dg.dpis.size()<2)return 0;std::vector<double> v=dg.dpis;std::sort(v.begin(),v.end());double m=aim::median(v),sp=(v.back()-v.front())/m*100;if(spread){*spread=sp;}return sp<=5?m:0;}
void paintDiag(HDC dc,int W,int H,HFONT big,HFONT small,const std::function<void(const std::wstring&,int,HFONT,COLORREF)>& say){
    // the dot: one count = one pixel, back to the middle when it leaves the screen
    if(std::fabs(dg.dotX)>W/2-20||std::fabs(dg.dotY)>H/2-20){dg.dotX=dg.dotY=0;}int x=W/2+(int)dg.dotX,y=H/2+(int)dg.dotY,r=std::max(5,H/140);
    HBRUSH b=CreateSolidBrush(RGB(47,108,255));HGDIOBJ ob=SelectObject(dc,b),np=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,x-r,y-r,x+r,y+r);SelectObject(dc,np);SelectObject(dc,ob);DeleteObject(b);
    wchar_t s[256];say(i18n::text(L"입력 진단"),H/12,big,RGB(247,249,255));
    swprintf(s,256,i18n::text(L"보고 %d회/초 · 이동 %ld / %ld 카운트/초 · 한 번에 최대 %ld"),dg.eps,dg.spsX,dg.spsY,dg.lastMax);say(dg.eps?s:i18n::text(L"마우스를 움직여 보세요. 움직임이 들어오는지 확인합니다."),H/12+H/14,small,dg.eps?RGB(196,208,232):RGB(240,180,90));
    if(dg.lag>0){swprintf(s,256,i18n::text(L"입력이 화면에 보일 때까지 약 %.1f ms · 화면 %.0f fps"),dg.lag,fps);say(s,H/12+H/14+H/28,small,RGB(158,174,204));}
    say(i18n::text(L"DPI 재기: 클릭 → 자를 대고 마우스를 오른쪽으로 정확히 10cm 옮기기 → 다시 클릭 (두 번 이상)"),H*3/5,small,RGB(196,208,232));
    if(dg.dpiState==1){swprintf(s,256,i18n::text(L"재는 중 · %ld 카운트"),dg.dpiCounts);say(s,H*3/5+H/24,small,RGB(140,176,255));}
    if(!dg.dpis.empty()){std::wstring list;for(double d:dg.dpis){list+=(list.empty()?L"":L"  ·  ")+std::to_wstring((int)std::lround(d));}double sp=0,m=diagDpi(&sp);say(list,H*3/5+H/12,small,RGB(247,249,255));
        if(m>0){swprintf(s,256,i18n::text(L"DPI 약 %d (차이 %.1f%%) · 닫으면 저장합니다"),(int)std::lround(m),sp);}else if(dg.dpis.size()<2){wcscpy(s,i18n::text(L"한 번 더 재 주세요"));}else{swprintf(s,256,i18n::text(L"재 때마다 차이가 큽니다 (%.1f%%) · 다시 재 주세요"),sp);}
        say(s,H*3/5+H/12+H/28,small,m>0?RGB(96,210,150):RGB(240,180,90));}
    say(i18n::text(L"Esc: 닫기"),H-H/18,small,RGB(110,124,154));}
// The reaction tests' cards, in the same look as the analysis (glass cards, cobalt, white).
const wchar_t* gradeOf(double ms,int test){double k=test==1?1:2.1;return i18n::text(ms<170*k?L"번개 같은 반응":ms<200*k?L"프로급":ms<240*k?L"매우 빠름":ms<280*k?L"빠름":ms<330*k?L"보통":L"조금 느림");}
void glass(Gdiplus::Graphics& g,RECT r,int round);double easeOut(double u);Gdiplus::Color argb(int a,COLORREF c);
void reactCards(Gdiplus::Graphics& g,HDC dc,int W,int H,double t,HFONT big,HFONT mid,HFONT small){
    static HFONT huge=nullptr;static int hugeH=0;if(hugeH!=H){if(huge)DeleteObject(huge);hugeH=H;huge=CreateFontW(-std::max(48,H/9),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,i18n::fontFamily());}
    auto put=[&](const std::wstring& s,RECT r,HFONT ft,COLORREF c,UINT fl){HGDIOBJ o=SelectObject(dc,ft);SetTextColor(dc,c);DrawTextW(dc,s.c_str(),-1,&r,fl|DT_NOPREFIX);SelectObject(dc,o);};
    UINT mid1=DT_CENTER|DT_VCENTER|DT_SINGLELINE;wchar_t s[256];std::wstring key=keyName(opt.key);int n=rounds(),done=(int)rx.ms.size();
    // the progress dots under a card
    auto dots=[&](int y){int r=std::max(4,H/220),gap=r*4,x=W/2-(n-1)*gap/2;for(int i=0;i<n;i++){bool on=i<done;Gdiplus::SolidBrush b(on?Gdiplus::Color(255,96,142,255):Gdiplus::Color(255,36,54,96));g.FillEllipse(&b,(float)(x+i*gap-r),(float)(y-r),(float)r*2,(float)r*2);}};
    auto pill=[&](RECT pr,const std::wstring& label){double br=(std::sin(t*2.4)+1)/2;int ph=pr.bottom-pr.top;{Gdiplus::GraphicsPath gp;RECT gl=pr;InflateRect(&gl,(int)(H/90*br)+2,(int)(H/90*br)+2);painting::path(gp,gl,ph+H/45);Gdiplus::SolidBrush gb(argb((int)(40+50*br),RGB(52,92,255)));g.FillPath(&gb,&gp);}
        Gdiplus::GraphicsPath pp;painting::path(pp,pr,ph);Gdiplus::LinearGradientBrush pb(Gdiplus::Point(pr.left,pr.top-1),Gdiplus::Point(pr.left,pr.bottom+1),Gdiplus::Color(255,84,128,255),Gdiplus::Color(255,46,84,236));g.FillPath(&pb,&pp);put(label,pr,mid,RGB(255,255,255),mid1);};
    int cw=std::min(W*46/100,H*95/100);
    if(!measuring&&rx.step!=5){ // the start card
        int ch=H*46/100;RECT card={W/2-cw/2,H/2-ch/2-H/40,W/2+cw/2,H/2+ch/2-H/40};glass(g,card,H/28);int top=card.top+ch/10,lg=H/13;RECT logo={W/2-lg/2,top,W/2+lg/2,top+lg};painting::drawLogo(dc,logo);
        put(i18n::text(opt.test==1?L"반응 속도 테스트":L"표적 반응 테스트"),{card.left,top+lg+H/60,card.right,top+lg+H/60+H/18},big,RGB(247,249,255),DT_CENTER|DT_TOP|DT_SINGLELINE);
        int pad=cw/12;put(i18n::text(opt.test==1?L"화면이 파랗게 바뀌는 순간 클릭하세요. 5번 재서 평균을 냅니다.":L"표적이 나타나면 조준점을 옮겨 맞히세요. 10번 재서 평균을 냅니다."),{card.left+pad,top+lg+H/60+H/18+H/50,card.right-pad,top+lg+H/60+H/18+H/50+H/12},small,RGB(206,216,236),DT_CENTER|DT_TOP|DT_WORDBREAK);
        int pw=cw-pad*2,ph=H/14;swprintf(s,256,i18n::text(L"클릭 또는 %ls로 시작 · Esc로 닫기"),key.c_str());pill({W/2-pw/2,card.bottom-ch/10-ph,W/2+pw/2,card.bottom-ch/10},s);return;}
    if(rx.step==5){ // the summary: the average counting up, the grade, a bar per round
        double sum=0,best=1e9,worst=0;for(double m:rx.ms){sum+=m;best=std::min(best,m);worst=std::max(worst,m);}double avg=rx.ms.empty()?0:sum/rx.ms.size(),u=easeOut((t-rx.summaryAt)/.8);
        int ch=H*62/100;RECT card={W/2-cw/2,H/2-ch/2,W/2+cw/2,H/2+ch/2};glass(g,card,H/28);int pad=cw/12,y=card.top+ch/14;
        put(i18n::text(L"결과"),{card.left,y,card.right,y+H/24},mid,RGB(150,164,196),mid1);y+=H/24;
        swprintf(s,256,L"%d ms",(int)std::lround(avg*u));put(s,{card.left,y,card.right,y+H/8},huge,RGB(247,249,255),mid1);y+=H/8;
        {std::wstring gr=gradeOf(avg,opt.test);SIZE ts={};HGDIOBJ o=SelectObject(dc,mid);GetTextExtentPoint32W(dc,gr.c_str(),(int)gr.size(),&ts);SelectObject(dc,o);int bw=ts.cx+H/20,bh=H/22;RECT b={W/2-bw/2,y,W/2+bw/2,y+bh};
            Gdiplus::GraphicsPath bp;painting::path(bp,b,bh);Gdiplus::SolidBrush bb(Gdiplus::Color((BYTE)(255*u),36,64,150));g.FillPath(&bb,&bp);put(gr,b,mid,RGB(206,222,255),mid1);y+=bh+H/40;}
        if(opt.test==2)swprintf(s,256,i18n::text(L"최고 %d ms · 빗나감 %d번"),(int)std::lround(best),rx.misses);else swprintf(s,256,i18n::text(L"최고 %d ms · 가장 느림 %d ms"),(int)std::lround(best),(int)std::lround(worst));put(s,{card.left,y,card.right,y+H/30},small,RGB(160,176,210),mid1);y+=H/30+H/50;
        // bars: one per round, growing in turn; the best one green
        int bars=(int)rx.ms.size(),area=cw-pad*2,bwid=std::max(6,area/std::max(1,bars)*6/10),gapw=bars?area/bars:0,base=card.bottom-ch/14-H/16-H/28,maxh=base-y-H/60;
        for(int i=0;i<bars;i++){double grow=easeOut((t-rx.summaryAt-.08*i)/.6),hh=std::max(4.0,maxh*rx.ms[i]/std::max(1.0,worst)*grow);float x=(float)(card.left+pad+gapw*i+(gapw-bwid)/2);
            Gdiplus::GraphicsPath bp;painting::path(bp,{(int)x,(int)(base-hh),(int)x+bwid,base},std::min(bwid,12));Gdiplus::SolidBrush fb(rx.ms[i]==best?Gdiplus::Color(255,96,210,150):Gdiplus::Color(255,72,118,255));g.FillPath(&fb,&bp);}
        int pw=cw-pad*2,ph=H/16;swprintf(s,256,i18n::text(L"클릭해서 다시 하기 · Esc로 닫기"));pill({W/2-pw/2,card.bottom-ch/14-ph,W/2+pw/2,card.bottom-ch/14},s);return;}
    if(opt.test==1){
        if(rx.step==2){ // the signal: the whole screen in cobalt
            RECT all={0,0,W,H};HBRUSH b=CreateSolidBrush(RGB(40,86,255));FillRect(dc,&all,b);DeleteObject(b);put(i18n::text(L"클릭!"),{0,0,W,H},huge,RGB(255,255,255),mid1);dots(H*3/4);return;}
        int ch=H*30/100;RECT card={W/2-cw/2,H/2-ch/2,W/2+cw/2,H/2+ch/2};glass(g,card,H/28);
        if(rx.step==3){swprintf(s,256,L"%d ms",(int)std::lround(rx.last));put(s,{card.left,card.top,card.right,card.top+ch*62/100},huge,RGB(247,249,255),mid1);put(gradeOf(rx.last,1),{card.left,card.top+ch*58/100,card.right,card.top+ch*84/100},mid,RGB(140,176,255),mid1);}
        else if(rx.step==4){put(i18n::text(L"너무 빨랐습니다"),{card.left,card.top,card.right,card.top+ch*60/100},big,RGB(240,180,90),mid1);put(i18n::text(L"파랗게 바뀐 뒤에 클릭하세요"),{card.left,card.top+ch*55/100,card.right,card.top+ch*80/100},small,RGB(160,176,210),mid1);}
        else{double br=(std::sin(t*3)+1)/2;float R=(float)(H/26)*(1+.1f*(float)br),cx=W/2.f,cy=card.top+ch*.28f;Gdiplus::Pen ring(argb((int)(90+90*br),RGB(72,118,255)),(float)std::max(2,H/400));g.DrawEllipse(&ring,cx-R,cy-R,R*2,R*2);
            put(i18n::text(L"기다리세요…"),{card.left,card.top+ch*58/100,card.right,card.top+ch*78/100},big,RGB(247,249,255),mid1);put(i18n::text(L"파랗게 바뀌면 바로 클릭"),{card.left,card.top+ch*76/100,card.right,card.top+ch*94/100},small,RGB(150,164,196),mid1);}
        dots(card.bottom+H/30);return;}
    // 표적 반응: the top card with the count and the last time
    int cw2=std::max(W/4,H/2),ch2=H/11;RECT top={W/2-cw2/2,H/40,W/2+cw2/2,H/40+ch2};glass(g,top,ch2/2);int pad=ch2/2;
    swprintf(s,256,L"%d / %d",done,n);put(i18n::text(L"표적 반응"),{top.left+pad,top.top,top.right-pad,top.bottom-ch2/3},mid,RGB(247,249,255),DT_LEFT|DT_VCENTER|DT_SINGLELINE);put(s,{top.left+pad,top.top,top.right-pad,top.bottom-ch2/3},mid,RGB(140,176,255),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    if(done){swprintf(s,256,i18n::text(L"방금 %d ms · 빗나감 %d번"),(int)std::lround(rx.last),rx.misses);put(s,{top.left+pad,top.bottom-ch2/2,top.right-pad,top.bottom-ch2/10},small,RGB(160,176,210),DT_LEFT|DT_VCENTER|DT_SINGLELINE);}
    else put(i18n::text(L"표적이 나타나면 맞히세요"),{top.left+pad,top.bottom-ch2/2,top.right-pad,top.bottom-ch2/10},small,RGB(160,176,210),DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    swprintf(s,256,i18n::text(L"속도 %.1fx · 휠로 조절"),opt.speed);put(s,{top.left+pad,top.bottom-ch2/2,top.right-pad,top.bottom-ch2/10},small,RGB(140,176,255),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
// The backdrop: a deep navy gradient in 48 plain bands (as cheap as one fill; copying a baked 4K picture every frame cost twice that).
void sky(HDC dc,int W,int H){for(int i=0;i<48;i++){double u=i/47.0;RECT r={0,H*i/48,W,H*(i+1)/48};HBRUSH b=CreateSolidBrush(RGB((int)(11-7*u),(int)(18-11*u),(int)(38-22*u)));FillRect(dc,&r,b);DeleteObject(b);}}
double easeOut(double u){u=std::clamp(u,0.0,1.0);return 1-(1-u)*(1-u)*(1-u);}
double easeBack(double u){u=std::clamp(u,0.0,1.0);double c=1.7;return 1+(c+1)*std::pow(u-1,3)+c*std::pow(u-1,2);}
Gdiplus::Color argb(int a,COLORREF c){return Gdiplus::Color((BYTE)std::clamp(a,0,255),GetRValue(c),GetGValue(c),GetBValue(c));}
// A frosted card: dark navy, a thin light edge.
void glass(Gdiplus::Graphics& g,RECT r,int round){Gdiplus::GraphicsPath p;painting::path(p,r,round);Gdiplus::SolidBrush f(Gdiplus::Color(218,12,20,42));g.FillPath(&f,&p);Gdiplus::Pen e(Gdiplus::Color(120,74,112,224),1.2f);g.DrawPath(&e,&p);}
void paint(HDC dc,int W,int H){double t=now(),dt=paintAt?std::min(.1,t-paintAt):0;paintAt=t;sky(dc,W,H);
    double fh=fovHalf(),f=(W/2.0)/std::tan(fh*3.14159265358979/180);auto sx=[&](double a){return (int)std::lround(W/2.0+std::tan(a*3.14159265358979/180)*f);};auto sy=[&](double b){return (int)std::lround(H/2.0-std::tan(b*3.14159265358979/180)*f);};
    // the world: a grid every 10 degrees (brighter every 30, the horizon brightest), so turning is felt
    HPEN fine=CreatePen(PS_SOLID,1,RGB(18,30,58)),major=CreatePen(PS_SOLID,1,RGB(28,46,90)),horizon=CreatePen(PS_SOLID,2,RGB(44,74,150));HGDIOBJ old=SelectObject(dc,fine);
    for(double g=std::floor((yaw-60)/10)*10;g<=yaw+60;g+=10){double a=g-yaw;if(std::fabs(a)>=fh+10)continue;int x=sx(a);SelectObject(dc,std::fmod(std::fabs(g),30)<.5?major:fine);MoveToEx(dc,x,0,nullptr);LineTo(dc,x,H);}
    for(double g=std::floor((pitch-40)/10)*10;g<=pitch+40;g+=10){double b=g-pitch;if(std::fabs(b)>=40)continue;int y=sy(b);SelectObject(dc,std::fabs(g)<.5?horizon:std::fmod(std::fabs(g),30)<.5?major:fine);MoveToEx(dc,0,y,nullptr);LineTo(dc,W,y);}
    SelectObject(dc,old);DeleteObject(fine);DeleteObject(major);DeleteObject(horizon);
    static HFONT big=nullptr,small=nullptr,mid=nullptr;static int fontH=0;if(fontH!=H){if(big){DeleteObject(big);DeleteObject(small);DeleteObject(mid);}fontH=H;big=CreateFontW(-std::max(22,H/26),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,i18n::fontFamily());small=CreateFontW(-std::max(14,H/60),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,i18n::fontFamily());mid=CreateFontW(-std::max(16,H/50),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,i18n::fontFamily());} // made once per screen height
    SetBkMode(dc,TRANSPARENT);auto say=[&](const std::wstring& s,int y,HFONT ft,COLORREF c){HGDIOBJ o=SelectObject(dc,ft);SetTextColor(dc,c);RECT r={0,y,W,y+H/10};DrawTextW(dc,s.c_str(),-1,&r,DT_CENTER|DT_TOP|DT_SINGLELINE);SelectObject(dc,o);};
    auto put=[&](const std::wstring& s,RECT r,HFONT ft,COLORREF c,UINT fl){HGDIOBJ o=SelectObject(dc,ft);SetTextColor(dc,c);DrawTextW(dc,s.c_str(),-1,&r,fl|DT_NOPREFIX);SelectObject(dc,o);};
    if(opt.diag){paintDiag(dc,W,H,big,small,say);return;}
    if(!painting::token){return;}
    {Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    int r=std::max(4,(int)std::lround(std::tan(opt.radius*3.14159265358979/180)*f));
    // rings spreading from hits (cobalt) and misses (grey); they stay where the target was in the world
    bursts.erase(std::remove_if(bursts.begin(),bursts.end(),[&](const Burst& b){return t-b.at>.5;}),bursts.end());
    for(auto& b:bursts){double u=(t-b.at)/.5,e=easeOut(u);float x=(float)sx(b.yaw-yaw),y=(float)sy(b.pitch-pitch),rr=(float)(r*(1+1.8*e));
        Gdiplus::Pen pen(argb((int)(230*(1-u)*(1-u)),b.hit?RGB(140,176,255):RGB(150,160,184)),(float)std::max(2.0,r/5.0*(1-u)+1));g.DrawEllipse(&pen,x-rr,y-rr,rr*2,rr*2);}
    if(targetUp){float x=(float)sx(tYaw-yaw),y=(float)sy(tPitch-pitch);
        if(phase==2){ // 수직 반동: a hollow ring; while the button is held, a cobalt arc fills it for the hold time
            float rr=(float)r*1.15f;Gdiplus::Pen ring(Gdiplus::Color(200,140,176,255),(float)std::max(2,r/6));g.DrawEllipse(&ring,x-rr,y-rr,rr*2,rr*2);
            if(holding){float sweep=(float)std::min(1.0,(t-holdAt)/holdS)*360.f,ar=rr+(float)std::max(5,r/3);Gdiplus::Pen arc(Gdiplus::Color(255,72,118,255),(float)std::max(3,r/4));painting::roundCaps(arc);g.DrawArc(&arc,x-ar,y-ar,ar*2,ar*2,-90.f,sweep);}
            else{double p=(std::sin(t*5)+1)/2;float gr=rr+(float)(r*.5*p);Gdiplus::Pen hint(argb((int)(110*(1-p)),RGB(140,176,255)),1.5f);g.DrawEllipse(&hint,x-gr,y-gr,gr*2,gr*2);}}
        else{ // 표적: pops in, a soft glow, a cobalt disc with a light ring and a white centre
            double born=trialAt>=0?t-trialAt:1;float k=(float)easeBack(born/.2),rr=(float)r*std::max(.05f,k);
            Gdiplus::GraphicsPath halo;float hr=rr*2.1f;halo.AddEllipse(x-hr,y-hr,hr*2,hr*2);Gdiplus::PathGradientBrush hb(&halo);hb.SetCenterColor(Gdiplus::Color(90,52,92,255));Gdiplus::Color he(0,52,92,255);INT n=1;hb.SetSurroundColors(&he,&n);g.FillPath(&hb,&halo);
            Gdiplus::LinearGradientBrush disc(Gdiplus::PointF(x,y-rr),Gdiplus::PointF(x,y+rr+1),Gdiplus::Color(255,96,142,255),Gdiplus::Color(255,40,78,226));g.FillEllipse(&disc,x-rr,y-rr,rr*2,rr*2);
            Gdiplus::Pen edge(Gdiplus::Color(235,190,212,255),(float)std::max(1.5,rr/7.0));g.DrawEllipse(&edge,x-rr,y-rr,rr*2,rr*2);
            float cr=std::max(1.5f,rr*.2f);Gdiplus::SolidBrush core(Gdiplus::Color(255,255,255,255));g.FillEllipse(&core,x-cr,y-cr,cr*2,cr*2);}}
    // crosshair: white with a dark edge so it reads on the light target; a red dot while aiming down sights
    float cx=opt.test==2?(float)sx(rx.aimX):W/2.f,cy=opt.test==2?(float)sy(rx.aimY):H/2.f,k=(float)std::max(7,H/110);
    if(!measuring||opt.test==1){}else if(phase==3){Gdiplus::GraphicsPath dh;dh.AddEllipse(cx-k,cy-k,k*2,k*2);Gdiplus::PathGradientBrush db(&dh);db.SetCenterColor(Gdiplus::Color(120,255,70,80));Gdiplus::Color de(0,255,70,80);INT n=1;db.SetSurroundColors(&de,&n);g.FillPath(&db,&dh);Gdiplus::SolidBrush dot(Gdiplus::Color(255,255,80,90));g.FillEllipse(&dot,cx-k/3,cy-k/3,k*2/3,k*2/3);}
    else{Gdiplus::PointF arms[4][2]={{{cx-k,cy},{cx-k/3,cy}},{{cx+k/3,cy},{cx+k,cy}},{{cx,cy-k},{cx,cy-k/3}},{{cx,cy+k/3},{cx,cy+k}}};Gdiplus::Pen shade(Gdiplus::Color(150,0,0,0),4.f),line(Gdiplus::Color(255,247,249,255),2.f);painting::roundCaps(shade);painting::roundCaps(line);
        for(auto& a:arms){g.DrawLine(&shade,a[0],a[1]);}for(auto& a:arms){g.DrawLine(&line,a[0],a[1]);}}
    wchar_t s[256];std::wstring key=keyLabel();
    if(opt.test){reactCards(g,dc,W,H,t,big,mid,small);}
    else if(endingAt>0){ // 분석 완료: a ring draws itself, then the check, then the words
        double u=t-endingAt;float R=(float)std::max(34,H/16);float x=cx,y=cy-H*.04f;Gdiplus::Pen ring(Gdiplus::Color(255,72,118,255),(float)std::max(3,H/260));painting::roundCaps(ring);g.DrawArc(&ring,x-R,y-R,R*2,R*2,-90.f,(float)(360*easeOut(u/.45)));
        double c=easeOut((u-.3)/.35);if(c>0){Gdiplus::PointF a(x-R*.42f,y+R*.02f),b(x-R*.1f,y+R*.32f),e(x+R*.45f,y-R*.3f);Gdiplus::Pen mark(Gdiplus::Color(255,247,249,255),(float)std::max(4,H/200));painting::roundCaps(mark);
            if(c<.4){float q=(float)(c/.4);g.DrawLine(&mark,a,Gdiplus::PointF(a.X+(b.X-a.X)*q,a.Y+(b.Y-a.Y)*q));}else{float q=(float)((c-.4)/.6);g.DrawLine(&mark,a,b);g.DrawLine(&mark,b,Gdiplus::PointF(b.X+(e.X-b.X)*q,b.Y+(e.Y-b.Y)*q));}}
        int al=(int)(255*easeOut((u-.5)/.4));if(al>0)put(i18n::text(L"분석 완료"),{0,(int)(y+R+H/40),W,(int)(y+R+H/40+H/16)},big,RGB(247*al/255,249*al/255,255*al/255),DT_CENTER|DT_TOP|DT_SINGLELINE);}
    else if(!measuring){ // the start card
        int cw=std::min(W*46/100,H*95/100),ch=H*52/100;RECT card={W/2-cw/2,H/2-ch/2-H/40,W/2+cw/2,H/2+ch/2-H/40};glass(g,card,H/28);
        int pad=cw/12,top=card.top+ch/12,lg=H/13;RECT logo={W/2-lg/2,top,W/2+lg/2,top+lg};painting::drawLogo(dc,logo);
        put(i18n::text(L"감도 분석"),{card.left,top+lg+H/60,card.right,top+lg+H/60+H/18},big,RGB(247,249,255),DT_CENTER|DT_TOP|DT_SINGLELINE);
        int ry=top+lg+H/60+H/18+H/40,row=H/17,nr=H/52;const wchar_t* steps[]={L"1. 파란 표적이 나타나면 조준점을 옮겨 클릭하세요.",L"2. 빈 원이 나타나면 왼쪽 버튼을 누른 채 위로 올라가는 화면을 아래로 당겨 원에 맞추세요."};
        for(int i=0;i<2;i++){int yy=ry+i*row;float ncx=(float)(card.left+pad+nr),ncy=(float)(yy+row/2);Gdiplus::SolidBrush nb(Gdiplus::Color(255,36,64,150));g.FillEllipse(&nb,ncx-nr,ncy-nr,(float)nr*2,(float)nr*2);
            put(std::to_wstring(i+1),{(int)(ncx-nr),(int)(ncy-nr),(int)(ncx+nr),(int)(ncy+nr)},mid,RGB(247,249,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            std::wstring st=i18n::text(steps[i]);if(st.size()>3&&st[1]==L'.'){st=st.substr(3);} /* the number is drawn in the circle */
            {RECT tr={card.left+pad+nr*2+pad/2,0,card.right-pad,0};HGDIOBJ o=SelectObject(dc,small);DrawTextW(dc,st.c_str(),-1,&tr,DT_LEFT|DT_WORDBREAK|DT_CALCRECT|DT_NOPREFIX);SelectObject(dc,o);int th=tr.bottom-tr.top,ty=(int)ncy-th/2;put(st,{card.left+pad+nr*2+pad/2,ty,card.right-pad,ty+th+2},small,RGB(206,216,236),DT_LEFT|DT_WORDBREAK);}}
        put(i18n::text(opt.ads?L"약 3분 걸립니다. 같은 조건에서 감도 후보를 섞어 여러 번 비교합니다.":L"약 2분 걸립니다. 같은 조건에서 감도 후보를 섞어 여러 번 비교합니다."),{card.left+pad,ry+row*2+H/80,card.right-pad,ry+row*2+H/80+H/20},small,RGB(140,154,184),DT_CENTER|DT_TOP|DT_WORDBREAK);
        // the start pill, breathing
        double br=(std::sin(t*2.4)+1)/2;int pw=cw-pad*2,ph=H/14;RECT pill={W/2-pw/2,card.bottom-ch/12-ph,W/2+pw/2,card.bottom-ch/12};
        {Gdiplus::GraphicsPath gp;RECT gl=pill;InflateRect(&gl,(int)(H/90*br)+2,(int)(H/90*br)+2);painting::path(gp,gl,ph+H/45);Gdiplus::SolidBrush gb(argb((int)(40+50*br),RGB(52,92,255)));g.FillPath(&gb,&gp);}
        {Gdiplus::GraphicsPath pp;painting::path(pp,pill,ph);Gdiplus::LinearGradientBrush pb(Gdiplus::Point(pill.left,pill.top-1),Gdiplus::Point(pill.left,pill.bottom+1),Gdiplus::Color(255,84,128,255),Gdiplus::Color(255,46,84,236));g.FillPath(&pb,&pp);}
        swprintf(s,256,i18n::text(L"클릭 또는 %ls로 시작 · Esc로 닫기"),key.c_str());put(s,pill,mid,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    else{ // the top card: the test and how far it is, the bar easing toward the count
        shownProgress+=(done()/(double)std::max(1,total())-shownProgress)*std::min(1.0,dt*8);int cw=std::max(W/4,H/2),ch=H/11;RECT card={W/2-cw/2,H/40,W/2+cw/2,H/40+ch};glass(g,card,ch/2);
        int pad=ch/2;swprintf(s,256,L"%d / %d",done(),total());put(i18n::text(phase==1?L"표적 전환":phase==2?L"수직 반동":L"조준(ADS) 표적 전환"),{card.left+pad,card.top+ch/8,card.right-pad,card.top+ch/2+ch/8},mid,RGB(247,249,255),DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        put(s,{card.left+pad,card.top+ch/8,card.right-pad,card.top+ch/2+ch/8},mid,RGB(140,176,255),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        float bx=(float)(card.left+pad),bw=(float)(cw-pad*2),by=(float)(card.top+ch*68/100),bh=(float)std::max(4,ch/12);Gdiplus::GraphicsPath track;painting::path(track,{(int)bx,(int)by,(int)(bx+bw),(int)(by+bh)},(int)bh);Gdiplus::SolidBrush tb(Gdiplus::Color(255,24,38,72));g.FillPath(&tb,&track);
        float fw=std::max(bh,(float)(bw*shownProgress));Gdiplus::GraphicsPath fill;painting::path(fill,{(int)bx,(int)by,(int)(bx+fw),(int)(by+bh)},(int)bh);Gdiplus::LinearGradientBrush fb(Gdiplus::PointF(bx,by),Gdiplus::PointF(bx+bw+1,by),Gdiplus::Color(255,52,92,255),Gdiplus::Color(255,120,170,255));g.FillPath(&fb,&fill);
        if(phase==2&&targetUp&&!holding)put(i18n::text(L"왼쪽 버튼을 누르고 있으세요"),{0,H/2+H/9,W,H/2+H/9+H/20},mid,RGB(140,176,255),DT_CENTER|DT_TOP|DT_SINGLELINE);
        swprintf(s,256,i18n::text(L"%ls 또는 Esc: 분석 종료"),key.c_str());int hw=std::max(W/5,H*4/10),hh=H/22;RECT hint={W/2-hw/2,H-H/18-hh/2,W/2+hw/2,H-H/18+hh/2};glass(g,hint,hh);put(s,hint,small,RGB(150,164,196),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    }
    if(opt.debug){wchar_t s_dbg[256];swprintf(s_dbg,256,L"fps %.0f  off %.2f, %.2f  deg/count %.5f  mult %.2f  phase %d",fps,tYaw-yaw,tPitch-pitch,conv()*opt.yaw,mult(),phase);HGDIOBJ o=SelectObject(dc,small);SetTextColor(dc,RGB(120,200,140));TextOutW(dc,12,12,s_dbg,(int)wcslen(s_dbg));SelectObject(dc,o);}
    }
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_INPUT:{RAWINPUT ri;UINT size=sizeof(ri);if(GetRawInputData((HRAWINPUT)l,RID_INPUT,&ri,&size,sizeof(RAWINPUTHEADER))==(UINT)-1||ri.header.dwType!=RIM_TYPEMOUSE)break;
        const RAWMOUSE& mo=ri.data.mouse;if(opt.diag){diagInput(mo.usFlags&MOUSE_MOVE_ABSOLUTE?0:mo.lLastX,mo.usFlags&MOUSE_MOVE_ABSOLUTE?0:mo.lLastY,mo.usButtonFlags);InvalidateRect(h,nullptr,FALSE);break;}input(mo.usFlags&MOUSE_MOVE_ABSOLUTE?0:mo.lLastX,mo.usFlags&MOUSE_MOVE_ABSOLUTE?0:mo.lLastY,mo.usButtonFlags);
        if(now()-lastPaint>=1.0/240){InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);} break;} // smooth turning, at most ~240 frames a second; drawn now: a mouse sending 1000 reports a second would otherwise keep WM_PAINT waiting (uneven frames)
    case WM_MOUSEWHEEL:if(opt.test==2){opt.speed=std::clamp(opt.speed*(GET_WHEEL_DELTA_WPARAM(w)>0?1.1:1/1.1),.3,4.0);if(onSpeed)onSpeed(opt.speed);InvalidateRect(h,nullptr,FALSE);}return 0; // 표적 반응: the wheel sets the crosshair speed (kept)
    case WM_LBUTTONDOWN:if(!measuring&&!opt.diag){if(opt.test)reactBegin();else begin();}return 0;
    case WM_KEYDOWN:if(opt.diag){if(w==VK_ESCAPE){DestroyWindow(h);}return 0;}if(w==opt.key){if(measuring)stop();else if(opt.test)reactBegin();else begin();return 0;}if(w==VK_ESCAPE){if(measuring)stop();else{HWND x=win;win=nullptr;DestroyWindow(x);}return 0;}break;
    case WM_TIMER:if(opt.diag){
#ifdef BLUEASH_TT
        if(sim&&dg.dpis.size()<3){static int step=0;if(step==0){diagInput(0,0,RI_MOUSE_LEFT_BUTTON_DOWN);}if(step<50){diagInput((step%2?63:64)+(int)dg.dpis.size(),(step%7)-3,0);}if(step==50){diagInput(0,0,RI_MOUSE_LEFT_BUTTON_DOWN);}step=(step+1)%60;} /* test builds: an 800 DPI mouse moved 10 cm, three times */
#endif
        diagSecond(now());InvalidateRect(h,nullptr,FALSE);return 0;}tick();
#ifdef BLUEASH_TT
        simTick();
#endif
        if(now()-lastPaint>=1.0/240){InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);}return 0;
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE&&measuring&&!offscreen){stop();return 0;}break; // another window took over: stop, keep what was measured
    case WM_ERASEBKGND:return 1;
    // one back buffer kept while the window lives (a new full-screen bitmap every frame cost a whole core at 4K)
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);
        if(!backDC||backW!=c.right||backH!=c.bottom){if(backDC){DeleteDC(backDC);DeleteObject(backBmp);}backDC=CreateCompatibleDC(dc);backBmp=CreateCompatibleBitmap(dc,c.right,c.bottom);SelectObject(backDC,backBmp);backW=c.right;backH=c.bottom;}
        paint(backDC,c.right,c.bottom);BitBlt(dc,0,0,c.right,c.bottom,backDC,0,0,SRCCOPY);EndPaint(h,&ps);if(opt.test&&rx.step==2&&!rx.shownAt){GdiFlush();rx.shownAt=now();}
        frames++;double t=now();lastPaint=t;if(dg.pending>=0){dg.lagSum+=t-dg.pending;dg.lagN++;dg.pending=-1;}if(t-fpsAt>=1){fps=frames/(t-fpsAt);frames=0;fpsAt=t;}return 0;}
    case WM_PRINTCLIENT:{RECT c;GetClientRect(h,&c);paint((HDC)w,c.right,c.bottom);return 0;}
    case WM_DESTROY:if(opt.diag){RAWINPUTDEVICE rid={1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&rid,1,sizeof(rid));timeEndPeriod(1);int d=(int)std::lround(diagDpi());if(d&&onDiag){onDiag(d);}opt.diag=false;}
        if(measuring){releaseInput();timeEndPeriod(1);measuring=false;}if(win==h)win=nullptr;KillTimer(h,1);if(backDC){DeleteDC(backDC);DeleteObject(backBmp);backDC=nullptr;}if(owner&&IsWindow(owner))PostMessageW(owner,WM_APP+241,0,0);return 0;
    }return DefWindowProcW(h,m,w,l);}
// Opens the practice screen on the monitor BlueAsh is on.
inline bool open(HWND notify,const Options& o){if(win){SetForegroundWindow(win);return true;}QueryPerformanceFrequency(&freq);owner=notify;opt=o;
    if(opt.conv<=0){opt.conv=aim::converted(50);}if(opt.vertical<=0){opt.vertical=1;}if(opt.adsConv<=0){opt.adsConv=opt.conv;}if(!opt.key){opt.key=VK_INSERT;}
    measuring=false;phase=0;idx=0;yaw=pitch=0;targetUp=false;lastTick=0;endingAt=0;bursts.clear();rx=React();flickOrder.clear();recoilOrder.clear();adsOrder.clear();
    dpi::Raw raw_; // physical pixels
    WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshAimPractice";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);
    MONITORINFO mi={};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromWindow(notify,MONITOR_DEFAULTTOPRIMARY),&mi);RECT r=mi.rcMonitor;
#ifdef BLUEASH_TT
    offscreen=getenv("BLUEASH_AIM_OFFSCREEN")!=nullptr;if(offscreen){OffsetRect(&r,-6000-r.left,-r.top);}sim=getenv("BLUEASH_AIM_SIM")!=nullptr;
#endif
    win=CreateWindowExW(WS_EX_TOPMOST,wc.lpszClassName,L"BlueAsh",WS_POPUP|WS_VISIBLE,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,wc.hInstance,nullptr);
    if(!win){return false;}SetForegroundWindow(win);SetTimer(win,1,8,nullptr);
    if(opt.diag){dg=Diag();dg.secAt=now();RAWINPUTDEVICE rid={1,2,0,win};RegisterRawInputDevices(&rid,1,sizeof(rid));timeBeginPeriod(1);}
#ifdef BLUEASH_TT
    if(sim&&!opt.diag){if(opt.test)reactBegin();else begin();}
#endif
    return true;}
inline bool active(){return win!=nullptr;}
inline unsigned key(){return opt.key;}
inline void toggle(){if(!win)return;if(measuring)stop();else if(opt.test)reactBegin();else begin();}
}
