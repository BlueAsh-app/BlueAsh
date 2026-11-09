// 성능 분석 (graphics page, view 5) — included inside namespace dlssui by dlss_ui.h; uses its helpers and state.
// Two tabs shown: PC 진단 (perf_hw.h) and FPS 측정·진단 (perf_trace.h + perf_core.h); upgrade advice lives in 윈도우 최적화
// (pc_check.h). Work that takes time (WMI, the 20 s measurement, the 2 s process sample) runs on worker threads and
// posts its result (WM_APP+170..175); a page closed meanwhile drops it. Records stay on this PC: perf-history.tsv,
// boost.txt (the restore journal) and per game target.txt under %LOCALAPPDATA%\ModeSwitch\dlss.
// Button ids 380-384 tabs, 385 measure, 386 restart as administrator, 387-391 target chips, 392-394 direction,
// 395 apply power plan, 396 restore, 397/398 open Windows settings, 399 refresh background list, 400-403 close a
// program, 404 start comparison, 405 keep.
const int chipFps[4]={60,120,144,240};
extern int tuneTab;void tuneLoadTab();void tuneShimmer();void tuneHideTabs();void tuneLayoutTabs();void tunePaint(HDC dc);void tuneClearHits();wintune::History tuneHistory();void tuneRecover();void tuneDestroy();
void dropChart();extern bool animOn;void optLayout();RECT cmpBox();std::wstring optPath();void optToggle();void optStart();extern bool optMode;bool optApplied();bool optRestore();void setOptPref(bool on);void pcCheckDone();void optDone(int failed,bool on);void optAbandon();void optTick();void pcTick();void pcTimer();extern bool pcShown,pcChecking;
int perfTab=0;perfhw::Info hw;bool hwReady=false,hwLoading=false,measuring=false,needAdmin=false,bgLoading=false;int measurePct=0;std::atomic<bool> perfCancel{false};
struct Measured {bool ok=false;perf::Stats s;perf::Sample x;std::vector<perf::Finding> f;std::wstring error;int seconds=0;bool frameGen=false;};
Measured lastRun;std::vector<perftrace::Busy> bgList;std::wstring perfStatus;bool perfBad=false;
int cmpStep=0;Measured cmpA,cmpB;perf::Compare cmpResult;bool cmpDone=false;
// Live readings for 'PC 진단': Windows counters read once a second on their own thread (opening them, and the GPU engine
// counter on a busy PC, can take seconds; the page only reads the last values). liveGen ends a thread that is no longer wanted.
std::atomic<double> liveCpu{-1},livePerf{-1},liveGpu{-1};std::atomic<int> liveGen{0};bool liveOn=false;
std::wstring boostPath(){return root()+L"\\boost.txt";}
std::wstring historyPath(){return root()+L"\\perf-history.tsv";}
// Saved target: "<fps> <direction>" (direction 0 quality, 1 balanced, 2 competitive).
void loadTarget(const dlsslib::Game& g,int& fps,int& dir){fps=0;dir=1;std::string t=readFile(gameDir(g)+L"\\target.txt",64);if(!t.empty()){int a=0,b=1;if(sscanf(t.c_str(),"%d %d",&a,&b)>=1){fps=std::clamp(a,0,1000);dir=std::clamp(b,0,2);}}}
void saveTarget(const dlsslib::Game& g,int fps,int dir){char b[32];snprintf(b,32,"%d %d",fps,dir);writeFile(gameDir(g)+L"\\target.txt",b);}
std::vector<perf::Session> history(const std::wstring& game){std::vector<perf::Session> out;std::string all=readFile(historyPath(),4<<20);std::wstring w=wideOf(all);size_t i=0;
    while(i<w.size()){size_t e=w.find(L'\n',i);if(e==std::wstring::npos)e=w.size();perf::Session s;if(perf::parse(w.substr(i,e-i),s)&&s.game==game)out.push_back(s);i=e+1;}return out;}
void appendHistory(const perf::Session& s){std::wstring l=perf::line(s)+L"\n";int n=WideCharToMultiByte(CP_UTF8,0,l.c_str(),(int)l.size(),nullptr,0,nullptr,nullptr);std::string u(n,0);WideCharToMultiByte(CP_UTF8,0,l.c_str(),(int)l.size(),&u[0],n,nullptr,nullptr);
    FILE* f=_wfopen(historyPath().c_str(),L"ab");if(f){fwrite(u.data(),1,u.size(),f);fclose(f);}}
// Frame generation shipped with the game (DLSS-G, FSR frame generation): the presented rate may include generated frames.
bool frameGenFiles(const dlsslib::Game& g){for(const std::wstring& d:{exeDir(g),g.dlssDir}){if(d.empty())continue;for(const wchar_t* pat:{L"\\nvngx_dlssg.dll",L"\\sl.dlss_g.dll",L"\\*framegeneration*.dll",L"\\*frameinterpolation*.dll"}){
        WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((d+pat).c_str(),&f);if(h!=INVALID_HANDLE_VALUE){FindClose(h);return true;}}}return false;}
void fillSample(const dlsslib::Game& g,perf::Sample& x){x.powerSaver=hw.powerSaver;x.battery=hw.battery;x.refresh=hw.refresh;x.ratedMts=perfhw::ratedMts(hw);x.configuredMts=perfhw::configuredMts(hw);
    if(x.vramTotal<0&&hw.vramGB>0){x.vramTotal=hw.vramGB;}x.limit=opti(g)&&installed(g)?loadLimit(g):0;x.frameGen=frameGenFiles(g);}
// Hardware: once per page (and again on request); the game's folder decides which drive is reported.
void loadHw(){if(hwLoading||!page)return;hwLoading=true;struct J{HWND page;std::wstring dir;};auto* g=current();auto* j=new J{page,g?g->e.dir:L""};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<J> j((J*)p);auto* r=new perfhw::Info(perfhw::collect(j->dir));if(!PostMessageW(j->page,WM_APP+170,0,(LPARAM)r))delete r;return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{delete j;hwLoading=false;}}
std::wstring conf(perf::Conf c){const wchar_t* n[]={L"높음",L"보통",L"낮음",L"판단 불가"};return i18n::text(n[c]);}
COLORREF confColor(perf::Conf c){return c==perf::High?RGB(90,200,140):c==perf::Medium?RGB(110,160,255):c==perf::Low?RGB(220,170,80):muted;}
const wchar_t* kindName(perf::Kind k){const wchar_t* n[]={L"GPU 제한 가능성",L"CPU 제한 가능성",L"RAM 용량 부족 가능성",L"메모리 속도 영향 가능성",L"VRAM 부족 가능성",L"백그라운드 작업 영향",L"전력·온도 제한 가능성",L"FPS 제한·수직 동기화",L"원인 판단 불가"};return n[k];}
std::wstring lineText(const perf::Line& l){wchar_t b[300];swprintf(b,300,i18n::text(l.fmt),l.a,l.b);return b;}
std::wstring unknown(){return i18n::text(L"측정 불가");}
// Starts the 20 s measurement of the chosen game (the same worker the comparison uses).
struct MeasureJob {HWND page;DWORD pid;int seconds;int phase;std::wstring game;};
void startMeasure(int phase){auto* g=current();if(!g||measuring)return;DWORD pid=runningPid(*g);if(!pid){perfStatus=i18n::text(L"게임을 실행한 뒤 측정하세요.");perfBad=true;InvalidateRect(page,nullptr,FALSE);return;}
    measuring=true;measurePct=0;perfCancel=false;perfStatus=i18n::text(L"측정 중… 게임 안에서 평소처럼 플레이하세요.");perfBad=false;layout();
    auto* j=new MeasureJob{page,pid,20,phase,optPath()};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<MeasureJob> j((MeasureJob*)p);dlssgpu::Nvml nv;HWND pg=j->page;int ph=j->phase;DWORD pid=j->pid;
        auto* r=new perftrace::Result(perftrace::measure(pid,j->seconds,&nv,[pid]{return perfboost::alive(pid);},[pg,ph](int n){PostMessageW(pg,WM_APP+171,(WPARAM)n,ph);},perfCancel));
        if(ph==1&&r->error.empty()&&!r->gameExited&&!perfCancel){ // comparison: apply the power plan, let it settle, measure again
            optmode::Journal jr;jr.path=j->game;int failed=optmode::apply(jr);PostMessageW(pg,WM_APP+174,(WPARAM)failed,0);
            if(failed<5){Sleep(3000);auto* b=new perftrace::Result(perftrace::measure(pid,j->seconds,&nv,[pid]{return perfboost::alive(pid);},[pg](int n){PostMessageW(pg,WM_APP+171,(WPARAM)n,2);},perfCancel));
                if(!PostMessageW(pg,WM_APP+175,(WPARAM)b,(LPARAM)r)){delete b;delete r;}return 0;}}
        if(!PostMessageW(pg,WM_APP+172,(WPARAM)ph,(LPARAM)r)){delete r;}return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{delete j;measuring=false;}}
Measured toMeasured(const dlsslib::Game& g,const perftrace::Result& r,bool store){Measured m;m.error=r.error;if(r.gameExited)m.error=i18n::text(L"측정 중 게임이 종료되어 결과를 쓰지 않습니다.");
    m.s=perf::stats(r.ms);m.x=r.x;m.x.vsync=r.vsync;fillSample(g,m.x);m.frameGen=m.x.frameGen;m.seconds=(int)std::lround(m.s.seconds);if(!m.error.empty())return m;
    auto past=history(keyOf(g));auto first=perf::diagnose(m.s,m.x);int agree=0;if(!first.empty()){for(auto& p:past)if(p.main==(int)first[0].kind)agree++;}
    m.f=perf::diagnose(m.s,m.x,agree);m.ok=perf::enough(m.s);
    if(store&&m.ok){perf::Session s;s.game=keyOf(g);SYSTEMTIME t;GetLocalTime(&t);wchar_t b[64];swprintf(b,64,L"%04d-%02d-%02d %02d:%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute);s.when=b;
        s.cpu=hw.cpu;s.gpu=hw.gpu;s.ramGB=hw.ramGB;swprintf(b,64,L"%dx%d@%d",hw.width,hw.height,hw.refresh);s.res=b;s.method=L"ETW DXGI Present";s.seconds=m.s.seconds;s.fps=m.s.fps;s.low1=m.s.low1;s.avgMs=m.s.avgMs;s.sdMs=m.s.sdMs;
        s.gpuUtil=m.x.gpuUtil;s.cpuMaxCore=m.x.cpuMaxCore;s.frames=m.s.frames;s.frameGen=m.frameGen;if(!m.f.empty()){s.main=m.f[0].kind;s.mainConf=m.f[0].conf;}appendHistory(s);}
    return m;}
// Background programs: CPU over 2 s, on a worker.
bool inGame(DWORD pid,const std::vector<std::wstring>& dirs){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;wchar_t path[MAX_PATH]={};DWORD n=MAX_PATH;bool hit=false;
    if(QueryFullProcessImageNameW(p,0,path,&n)){std::wstring s=path;auto at=s.find_last_of(L'\\');std::wstring exe=at==std::wstring::npos?s:s.substr(at+1);hit=!_wcsicmp(exe.c_str(),L"TslGame.exe")||!_wcsicmp(exe.c_str(),L"EscapeFromTarkov.exe");
        for(auto& d:dirs){if(!d.empty()&&!_wcsnicmp(path,d.c_str(),d.size())){hit=true;break;}}}CloseHandle(p);return hit;}
void loadBackground(){if(bgLoading||!page)return;bgLoading=true;struct J{HWND page;DWORD game;std::vector<std::wstring> dirs;};auto* g=current();auto* j=new J{page,g?runningPid(*g):0,{}};for(auto& x:games){j->dirs.push_back(x.e.dir);}InvalidateRect(page,nullptr,FALSE);
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<J> j((J*)p);auto a=perftrace::procTimes();Sleep(2000);auto b=perftrace::procTimes();
        auto all=perftrace::busiest(a,b,j->game,GetCurrentProcessId());std::vector<perftrace::Busy> keep;for(auto& x:all){if(!inGame(x.pid,j->dirs)){keep.push_back(x);}} /* a game (PUBG, Tarkov, any found game) is never offered for closing */
        auto* v=new std::vector<perftrace::Busy>(perfboost::closable(keep,j->game,4));if(!PostMessageW(j->page,WM_APP+173,0,(LPARAM)v))delete v;return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{delete j;bgLoading=false;}}
void boostDone(bool ok){if(page)PostMessageW(page,WM_APP+176,ok?1:0,0);} // from the restore thread
// Start of BlueAsh: a plan BlueAsh switched and never put back (it closed while the game ran) is restored now, or when
// that game exits.
void perfRecover(){optStart();perfboost::Journal j;j.path=boostPath();if(!j.load())return;if(perfboost::alive(j.pid)){perfboost::restoreWhenGone(j.path,j.pid,boostDone);return;}
    if(!perfboost::restore(j)){problem(i18n::text(L"스마트 부스트가 바꾼 전원 계획을 되돌리지 못했습니다. Windows 설정 > 전원에서 원래 계획을 고르세요."));}}
void startLiveCounters(){int gen=++liveGen;struct J{HWND page;int gen;};auto* j=new J{page,gen};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<J> j((J*)p);perftrace::Counters c;if(!c.open())return 0;
        while(liveGen==j->gen){for(int i=0;i<10&&liveGen==j->gen;i++)Sleep(100);if(liveGen!=j->gen)break;PdhCollectQueryData(c.q);double v=0;
            liveCpu=perftrace::Counters::one(c.total,v)?v:-1;livePerf=perftrace::Counters::one(c.perf,v)?v:-1;
            std::map<std::wstring,double> by;bool any=perftrace::Counters::each(c.gpu,[&](const std::wstring& n,double x){auto at=n.find(L"luid_");by[at==std::wstring::npos?L"":n.substr(at,30)]+=x;});
            double g=-1;if(any){g=0;for(auto& [k,x]:by)g=std::max(g,std::min(100.0,x));}liveGpu=g;PostMessageW(j->page,WM_APP+169,0,0);}return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else delete j;}
void perfOpen(){if(perfTab!=1)perfTab=0;if(!hwReady)loadHw();if(!liveOn){liveOn=true;startLiveCounters();}}
// ---- Layout (page pixels; view 5 uses the whole page).
RECT perfContent(){RECT pr;GetClientRect(page,&pr);return {36,136,pr.right-36,pr.bottom-36};}
void perfLayoutNow(){bool on=view==5;dropChart();if(!on&&liveOn){liveOn=false;liveGen++;}RECT C=perfContent();int W=C.right-C.left;auto* g=current();
    const int tabIds[2]={0,1};for(int i=0;i<5;i++){ShowWindow(GetDlgItem(page,380+i),SW_HIDE);}
    for(int k=0;k<2;k++){int i=tabIds[k];HWND b=GetDlgItem(page,380+i);int w=(W-8)/2;putRaw(b,36+k*(w+8),80,w,40);ShowWindow(b,on?SW_SHOW:SW_HIDE);LONG_PTR sel=perfTab==i?1:0;if(GetWindowLongPtrW(b,GWLP_USERDATA)!=sel){SetWindowLongPtrW(b,GWLP_USERDATA,sel);InvalidateRect(b,nullptr,FALSE);}}
    auto show=[&](int id,bool v,int x,int y,int w,int h=40){HWND b=GetDlgItem(page,id);if(v)putRaw(b,x,y,w,h);ShowWindow(b,on&&v?SW_SHOW:SW_HIDE);};
    bool t1=perfTab==1,t2=perfTab==2,t3=perfTab==3;
    show(385,t1,C.left+20,C.top+20,220);EnableWindow(GetDlgItem(page,385),!measuring);show(386,t1&&needAdmin,C.left+248,C.top+20,240);
    int tf=0,td=1;if(g)loadTarget(*g,tf,td);int cy=C.top+186;
    for(int i=0;i<5;i++){int v=i<4?chipFps[i]:hw.refresh;HWND b=GetDlgItem(page,387+i);show(387+i,t1&&(i<4||(v>0&&v!=60&&v!=120&&v!=144&&v!=240)),C.left+120+i*92,cy,84,36);SetWindowLongPtrW(b,GWLP_USERDATA,tf==v&&v>0?1:0);
        if(i==4&&v>0){wchar_t t[32];swprintf(t,32,L"%d Hz",v);SetWindowTextW(b,t);}}
    for(int i=0;i<3;i++){HWND b=GetDlgItem(page,392+i);show(392+i,t1,C.left+120+i*110,cy+44,102,36);SetWindowLongPtrW(b,GWLP_USERDATA,td==i?1:0);}
    int half=(W-16)/2;perfboost::Journal jr;jr.path=boostPath();bool journal=jr.load()&&jr.hasPower;
    show(395,t2,C.left+20,C.top+158,180);show(396,(t2||t3)&&journal,t2?C.left+208:C.left+20+214,t2?C.top+158:C.bottom-60,140);
    show(397,t2,C.left+half+16+20,C.top+158,170);show(398,t2,C.left+half+16+198,C.top+158,170);show(399,t2,C.right-20-170,C.top+242,170);
    for(int i=0;i<4;i++)show(400+i,t2&&i<(int)bgList.size(),C.right-20-100,C.top+304+i*48,100,38);
    show(404,t3,C.left+20,C.top+20,220);EnableWindow(GetDlgItem(page,404),!measuring);show(405,t3&&cmpDone&&journal,C.left+20,C.bottom-60,200);
    optLayout();}
// ---- Painting.
void section(HDC dc,RECT r,const wchar_t* title){::roundBox(dc,r,rowFill,line,16);::textAt(dc,i18n::text(title),{r.left+20,r.top+14,r.right-20,r.top+40},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
void row(HDC dc,int x,int y,int w,const std::wstring& label,const std::wstring& value,COLORREF c=0){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;int lw=std::min(150,w*2/5);
    ::textAt(dc,label,{x,y,x+lw-8,y+28},smallFont,muted,one);::textAt(dc,value.empty()?unknown():value,{x+lw,y,x+w,y+28},smallFont,value.empty()?muted:(c?c:ink),one);}
std::wstring fmt(const wchar_t* f,double a=0,double b=0,double c=0){wchar_t t[300];swprintf(t,300,i18n::text(f),a,b,c);return t;}
void pill(HDC dc,int x,int y,const std::wstring& t,COLORREF c){int w=pillWidth(dc,t);::roundBox(dc,{x,y,x+w,y+24},panel,c,12);::textAt(dc,t,{x,y,x+w,y+24},smallFont,c,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
void paintHardware(HDC dc,RECT C){int half=(C.right-C.left-16)/2;RECT L={C.left,C.top,C.left+half,C.bottom},R={C.left+half+16,C.top,C.right,C.bottom};section(dc,L,L"CPU · GPU");section(dc,R,L"메모리 · Windows");
    if(!hwReady){for(RECT s:{L,R})for(int i=0;i<6;i++)painting::skeleton(dc,{s.left+20,s.top+56+i*40,s.right-20,s.top+80+i*40},10,s.left,s.right,RGB(17,29,58),RGB(96,128,200));return;}
    int x=L.left+20,w=L.right-L.left-40,y=L.top+52;wchar_t b[200];auto next=[&](const std::wstring& l,const std::wstring& v,COLORREF c=0){if(y+28<=L.bottom-8)row(dc,x,y,w,l,v,c);y+=32;};
    next(L"CPU",hw.cpu);swprintf(b,200,i18n::text(L"%d코어 · %d스레드"),hw.cores,hw.threads);next(i18n::text(L"코어 / 스레드"),hw.cores?b:L"");
    next(i18n::text(L"기본 클럭"),hw.baseMhz?fmt(L"%.0f MHz",hw.baseMhz):L"");next(i18n::text(L"CPU 사용률"),liveCpu>=0?fmt(L"%.0f%%",liveCpu):L"");
    next(i18n::text(L"CPU 동작 속도"),livePerf>=0?fmt(L"기본 클럭의 %.0f%%",livePerf):L"");
    next(L"GPU",hw.gpu.empty()?L"":hw.gpu+(perfhw::vendorName(hw.vendor)[0]?L" ("+std::wstring(perfhw::vendorName(hw.vendor))+L")":L""));
    next(i18n::text(L"드라이버"),live.ok?wideOf(live.driver):hw.driver);next(L"VRAM",live.ok&&live.totalGB>0?fmt(L"%.1f / %.1f GB 사용",live.usedGB,live.totalGB):hw.vramGB>0?fmt(L"%.0f GB",hw.vramGB):L"");
    next(i18n::text(L"GPU 사용률"),liveGpu>=0?fmt(L"%.0f%%",liveGpu):L"");
    next(i18n::text(L"GPU 온도 · 전력 · 클럭"),live.ok?fmt(L"%.0f°C · %.0f W · %.0f MHz",live.temp,live.watts,live.mhz):hw.vendor==0x1002?i18n::text(L"측정 불가 (AMD 센서는 지원 예정)"):L"");
    x=R.left+20;w=R.right-R.left-40;y=R.top+52;auto right=[&](const std::wstring& l,const std::wstring& v,COLORREF c=0){if(y+28<=R.bottom-8)row(dc,x,y,w,l,v,c);y+=32;};
    MEMORYSTATUSEX ms={};ms.dwLength=sizeof(ms);GlobalMemoryStatusEx(&ms);right(i18n::text(L"메모리"),hw.ramGB>0?fmt(L"%.1f GB · 사용 중 %.0f%%",hw.ramGB,(double)ms.dwMemoryLoad):L"");
    {std::wstring m;if(hw.modulesRead){std::map<int,int> sizes;for(auto& k:hw.modules)sizes[(int)std::lround(k.gb)]++;for(auto& [gb,n]:sizes){swprintf(b,200,L"%d × %d GB",n,gb);if(!m.empty())m+=L" + ";m+=b;}
        std::wstring ddr=perfhw::ddrName(hw.modules[0].type);if(!ddr.empty())m+=L" · "+ddr;}right(i18n::text(L"메모리 모듈"),m);}
    {int rate=perfhw::ratedMts(hw),cfg=perfhw::configuredMts(hw);right(i18n::text(L"메모리 속도"),rate||cfg?fmt(L"설정 %.0f MT/s · 정격 %.0f MT/s",cfg,rate):L"",cfg&&rate&&cfg<rate*.9?RGB(220,170,80):0);}
    right(i18n::text(L"XMP/EXPO · 타이밍"),i18n::text(L"Windows에서 확인할 수 없음 (BIOS에서 확인)"),muted);right(i18n::text(L"메인보드"),hw.board);right(L"Windows",hw.os);
    right(i18n::text(L"디스플레이"),hw.width?fmt(L"%.0f × %.0f · %.0f Hz",hw.width,hw.height,hw.refresh):L"");
    right(i18n::text(L"전원 계획"),hw.powerPlan+(hw.battery?std::wstring(i18n::text(L" · 배터리 사용 중")):L""),hw.powerSaver||hw.battery?RGB(220,170,80):0);
    right(i18n::text(L"게임 모드"),hw.gameMode==1?i18n::text(L"켜짐"):hw.gameMode==0?i18n::text(L"꺼짐"):L"");right(i18n::text(L"게임 드라이브"),hw.storage);
    right(i18n::text(L"재부팅 대기"),hw.reboot?i18n::text(L"재부팅을 기다리는 업데이트·설치가 있음"):i18n::text(L"없음"),hw.reboot?RGB(220,170,80):0);}
void progressBar(HDC dc,RECT r,int pct){::roundBox(dc,r,well,well,8);if(pct>0){RECT f=r;f.right=r.left+(r.right-r.left)*std::clamp(pct,0,100)/100;::roundBox(dc,f,blue,blue,8);}}
void paintMeasure(HDC dc,RECT C){auto* g=current();section(dc,{C.left,C.top,C.right,C.top+168},L"");UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
    int sx=C.left+20+220+12+(needAdmin?252:0);::textAt(dc,perfStatus,{sx,C.top+20,C.right-20,C.top+60},smallBold,perfBad?danger:RGB(120,170,255),one);if(measuring)progressBar(dc,{sx,C.top+60,C.right-20,C.top+66},measurePct);
    int tw=(C.right-C.left-40-36)/4;RECT t[4];for(int i=0;i<4;i++){int x=C.left+20+i*(tw+12);t[i]={x,C.top+74,x+tw,C.top+134};}
    const Measured& m=lastRun;bool ok=m.ok;tile(dc,t[0],i18n::text(L"평균 FPS"),ok?fmt(L"%.0f",m.s.fps):L"-",L"");tile(dc,t[1],L"1% Low",ok?fmt(L"%.0f",m.s.low1):L"-",L"");
    tile(dc,t[2],i18n::text(L"프레임타임"),ok?fmt(L"%.1f ± %.1f ms",m.s.avgMs,m.s.sdMs):L"-",L"");
    tile(dc,t[3],i18n::text(L"GPU · 가장 바쁜 코어"),ok&&m.x.gpuUtil>=0?fmt(L"%.0f%% · %.0f%%",m.x.gpuUtil,std::max(0.0,m.x.cpuMaxCore)):L"-",L"");
    std::wstring method=ok?fmt(L"DXGI Present 간격(ETW) · %.0f초 · 프레임 %.0f개 · 1%% Low 기준: 가장 느린 프레임 1%%",m.s.seconds,(double)m.s.frames):i18n::text(L"게임을 켜고 측정 시작을 누르면 20초 동안 실제 프레임을 기록합니다.");
    if(ok&&m.frameGen){method+=i18n::text(L" · 프레임 생성 지원 게임: 표시 FPS일 수 있음");}if(!m.error.empty())method=m.error;::textAt(dc,method,{C.left+20,C.top+138,C.right-20,C.top+162},smallFont,m.error.empty()?muted:danger,one);
    // Target row.
    RECT tg={C.left,C.top+180,C.right,C.top+300};::roundBox(dc,tg,rowFill,line,16);::textAt(dc,i18n::text(L"목표 FPS"),{tg.left+20,tg.top+4,tg.left+116,tg.top+44},boldFont,ink,one);
    int tf=0,td=1;if(g)loadTarget(*g,tf,td);std::wstring verdict=i18n::text(L"목표를 고르고 측정하면 달성 여부를 판단합니다.");COLORREF vc=muted;
    if(g&&tf>0&&ok){auto st=perf::target(m.s,tf,m.x.limit,m.x.refresh,m.x.vsync);const wchar_t* n[]={L"목표 달성: 평균과 1% Low가 목표 근처입니다.",L"목표 근접: 평균이 목표에 거의 닿았습니다.",L"목표 미달: 아래 진단과 업그레이드 추천을 확인하세요.",L"FPS 제한이나 수직 동기화가 목표보다 낮게 걸려 있습니다.",L"측정이 부족해 판단할 수 없습니다."};
        verdict=i18n::text(n[st]);vc=st==perf::Met?RGB(90,200,140):st==perf::Near?RGB(110,160,255):RGB(220,170,80);}
    ::textAt(dc,i18n::text(L"방향"),{tg.left+20,tg.top+48,tg.left+116,tg.top+88},smallFont,muted,one);::textAt(dc,verdict,{tg.left+20,tg.top+90,tg.right-20,tg.bottom-4},smallFont,vc,one);
    // Findings.
    int y=C.top+314;if(!ok){::textAt(dc,i18n::text(L"진단은 측정 뒤에 나옵니다. 한 번의 측정으로는 단정하지 않으며, 같은 결과가 다시 나오면 신뢰도가 올라갑니다."),{C.left+20,y,C.right-20,y+44},smallFont,muted,DT_LEFT|DT_WORDBREAK);return;}
    for(auto& f:m.f){if(y+66>C.bottom)break;RECT r={C.left,y,C.right,y+62};::roundBox(dc,r,rowFill,line,14);pill(dc,r.left+16,r.top+8,conf(f.conf),confColor(f.conf));
        ::textAt(dc,i18n::text(kindName(f.kind)),{r.left+110,r.top+6,r.right-16,r.top+32},smallBold,ink,one);std::wstring why;for(auto& l:f.why){if(!why.empty())why+=L" · ";why+=lineText(l);}
        std::wstring tip=std::wstring(f.fix[0]?i18n::text(f.fix):L"")+(f.next[0]?std::wstring(f.fix[0]?L"  ·  ":L"")+i18n::text(L"확인: ")+i18n::text(f.next):L"");
        ::textAt(dc,why,{r.left+110,r.top+30,r.right-16,r.top+46},captionFont,muted,one);::textAt(dc,tip,{r.left+110,r.top+44,r.right-16,r.top+60},captionFont,RGB(140,175,240),one);y+=70;}}
void paintBoost(HDC dc,RECT C){int half=(C.right-C.left-16)/2;RECT A={C.left,C.top,C.left+half,C.top+214},B={C.left+half+16,C.top,C.right,C.top+214},D={C.left,C.top+228,C.right,C.bottom};UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
    section(dc,A,L"전원 계획");GUID now={},want={};bool got=perfboost::active(now),has=perfboost::gamePlan(want);perfboost::Journal jr;jr.path=boostPath();bool journal=jr.load()&&jr.hasPower;
    row(dc,A.left+20,A.top+50,A.right-A.left-40,i18n::text(L"지금"),hw.powerPlan.empty()&&got?L"":hw.powerPlan);
    row(dc,A.left+20,A.top+80,A.right-A.left-40,i18n::text(L"게임용 계획"),has?(IsEqualGUID(want,perfboost::ultimate)?i18n::text(L"최고의 성능 (이 PC에 있음)"):i18n::text(L"고성능 (이 PC에 있음)")):i18n::text(L"이 PC에 없음 (새로 만들지 않음)"),has?0:RGB(220,170,80));
    ::textAt(dc,journal?i18n::text(L"적용됨 · 게임이 끝나면 원래 계획으로 돌아갑니다"):i18n::text(L"게임 실행 중에 적용하면 게임 종료 때 원래 계획으로 돌아갑니다"),{A.left+20,A.top+108,A.right-20,A.top+152},captionFont,journal?RGB(90,200,140):muted,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);
    section(dc,B,L"Windows 게임 설정");row(dc,B.left+20,B.top+50,B.right-B.left-40,i18n::text(L"게임 모드"),hw.gameMode==1?i18n::text(L"켜짐"):hw.gameMode==0?i18n::text(L"꺼짐"):L"");
    row(dc,B.left+20,B.top+80,B.right-B.left-40,i18n::text(L"창 게임 최적화"),hw.windowedOpt==1?i18n::text(L"켜짐"):hw.windowedOpt==0?i18n::text(L"꺼짐"):i18n::text(L"Windows 기본값"));
    ::textAt(dc,i18n::text(L"BlueAsh는 이 설정을 바꾸지 않습니다. Windows 설정에서 직접 바꿀 수 있습니다."),{B.left+20,B.top+108,B.right-20,B.top+152},captionFont,muted,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);
    section(dc,D,L"백그라운드 프로그램");::textAt(dc,i18n::text(L"창이 있는 프로그램만 보여 줍니다. 닫기는 확인 후 그 프로그램에 창 닫기를 요청합니다(강제 종료 안 함)."),{D.left+20,D.top+40,D.right-210,D.top+62},captionFont,muted,one);
    if(bgLoading){for(int i=0;i<3;i++)painting::skeleton(dc,{D.left+20,D.top+76+i*48,D.right-140,D.top+108+i*48},10,D.left,D.right,RGB(17,29,58),RGB(96,128,200));return;}
    if(bgList.empty()){::textAt(dc,i18n::text(L"'다시 확인'을 누르면 2초 동안 CPU 사용량을 잽니다."),{D.left+20,D.top+80,D.right-210,D.top+106},smallFont,muted,one);return;}
    for(size_t i=0;i<bgList.size();i++){int y=D.top+76+(int)i*48;auto& b=bgList[i];::textAt(dc,b.name,{D.left+20,y,D.left+300,y+38},smallBold,ink,one);
        ::textAt(dc,fmt(L"CPU %.1f%% · 메모리 %.0f MB",b.cpu,b.mb),{D.left+310,y,D.right-130,y+38},smallFont,b.cpu>=10?RGB(220,170,80):muted,one);}}
// 전후 비교 result as bars: for average FPS and 1% Low, the 'before' bar on the left and the 'after' bar on the right, the
// change above each pair, the target line, the noise band on the average. The last result of each game is kept in
// compare.txt (real measurements only) and shown again, with its date, until the next comparison; the bars grow in
// (before first, then after) whenever a result is shown. Timer 6 runs only while they grow.
struct CmpRecord {bool ok=false;double bFps=0,bLow=0,aFps=0,aLow=0,diff=0,pct=0,noise=0;int v=perf::Insufficient;bool shaky=false;std::wstring when;};
CmpRecord shownCmp;std::wstring shownFor;double barStart=0;const double barMs=1500;bool barHiRes=false;
// While a comparison measures, the chart shows placeholder bars with a passing light; then the bars rise: before over
// 0-0.8 s, after over 0.45-1.4 s (ease in and out, the numbers counting with them), the change and the verdict fading in
// at the end. 1 ms timer resolution while it moves (the default 15.6 ms made the rise uneven).
std::wstring cmpPath(const dlsslib::Game& g){return gameDir(g)+L"\\compare.txt";}
void saveCmp(const dlsslib::Game& g,const CmpRecord& r){std::string w(r.when.begin(),r.when.end());char b[256];snprintf(b,256,"v1 %.3f %.3f %.3f %.3f %.3f %.3f %.3f %d %d %s",r.bFps,r.bLow,r.aFps,r.aLow,r.diff,r.pct,r.noise,r.v,r.shaky?1:0,w.c_str());writeFile(cmpPath(g),b);}
// A damaged or missing file shows nothing (no made-up bars).
CmpRecord loadCmp(const dlsslib::Game& g){CmpRecord r;std::string t=readFile(cmpPath(g),256);char when[64]={};int v=0,sh=0;
    if(sscanf(t.c_str(),"v1 %lf %lf %lf %lf %lf %lf %lf %d %d %63s",&r.bFps,&r.bLow,&r.aFps,&r.aLow,&r.diff,&r.pct,&r.noise,&v,&sh,when)<9){return CmpRecord();}
    for(double x:{r.bFps,r.bLow,r.aFps,r.aLow,r.diff,r.pct,r.noise}){if(!std::isfinite(x)||std::fabs(x)>100000){return CmpRecord();}}
    if(r.bFps<=0||r.aFps<=0||r.bLow<0||r.aLow<0||v<0||v>2){return CmpRecord();}
    r.v=v;r.shaky=sh!=0;std::string w=when;for(auto& c:w){if(c=='_')c=' ';}r.when.assign(w.begin(),w.end());r.ok=true;return r;}
void animTimer(bool fast=true){if(!page)return;animOn=true;if(fast&&!barHiRes){timeBeginPeriod(1);barHiRes=true;}SetTimer(page,6,fast?10:50,nullptr);} // slow while measuring: BlueAsh must not load the game it measures
void stopAnim(){animOn=false;dropChart();if(page)KillTimer(page,6);if(barHiRes){timeEndPeriod(1);barHiRes=false;}}
void growBars(){barStart=nowMs();dropChart();animTimer();}
RECT chartRect(RECT C){return {C.left,C.top+134,C.right,C.bottom-76};}
double easeOut(double t){t=std::clamp(t,0.0,1.0);return 1-std::pow(1-t,3);}
double easeInOut(double t){t=std::clamp(t,0.0,1.0);return t<.5?4*t*t*t:1-std::pow(-2*t+2,3)/2;}
// While the chart moves, the page is drawn once without the moving parts (chartCache, chartLayer 1) and each frame only
// redraws those (chartLayer 2) over a copy of it: a whole-page paint took 33-40 ms, too slow for a smooth rise.
HBITMAP chartCache=nullptr;SIZE chartCacheSize={};int chartLayer=0;bool animOn=false;
void dropChart(){if(chartCache){DeleteObject(chartCache);chartCache=nullptr;}}
void paintCompare(HDC dc,RECT C,bool header=true){auto* g=current();UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;bool S=chartLayer!=2,D=chartLayer!=1;
    if(S&&header){section(dc,{C.left,C.top,C.right,C.top+120},L"");const wchar_t* steps[]={L"",L"1/3 최적화 전 측정 중…",L"2/3 전원 계획 적용 후 측정 중…",L"3/3 비교 완료"};
        ::textAt(dc,cmpStep>0&&cmpStep<3?i18n::text(steps[cmpStep]):perfStatus,{C.left+252,C.top+20,C.right-20,C.top+60},smallBold,perfBad?danger:RGB(120,170,255),one);if(measuring&&cmpStep>0){progressBar(dc,{C.left+252,C.top+60,C.right-20,C.top+66},measurePct);}
        ::textAt(dc,i18n::text(L"지금 측정 → 고성능 전원 계획 적용 → 같은 장면에서 다시 측정 → 비교. 측정하는 동안 같은 장면에 머물러 주세요."),{C.left+20,C.top+74,C.right-20,C.top+110},captionFont,muted,DT_LEFT|DT_WORDBREAK);}
    if(g&&shownFor!=keyOf(*g)){shownFor=keyOf(*g);shownCmp=loadCmp(*g);if(shownCmp.ok){growBars();}}
    RECT K=chartRect(C);if(S){section(dc,K,L"전후 비교");}const CmpRecord& r=shownCmp;
    RECT P={K.left+40,K.top+(header?112:60),K.right-40,K.bottom-(header?150:150)}; /* compact in 윈도우 최적화: the change sits on the label row, two verdict lines */int ph=P.bottom-P.top,gw=(P.right-P.left)/2,bw=std::clamp(gw/4,40,120);
    auto labels=[&](){for(int k=0;k<2;k++){int cx=P.left+gw*k+gw/2;for(int j=0;j<2;j++){int x=j?cx+10:cx-bw-10;::textAt(dc,i18n::text(j?L"최적화 후":L"최적화 전"),{x-20,P.bottom+8,x+bw+20,P.bottom+30},smallFont,muted,DT_CENTER|DT_TOP|DT_SINGLELINE);}
        ::textAt(dc,k?std::wstring(L"1% Low"):std::wstring(i18n::text(L"평균 FPS")),{cx-gw/2,P.bottom+34,cx+gw/2,P.bottom+60},boldFont,ink,DT_CENTER|DT_TOP|DT_SINGLELINE);}};
    if(measuring&&cmpStep>0){if(ph<60){return;}if(S){labels();}
        if(D){const double hs[4]={.55,.68,.47,.6};for(int k=0;k<2;k++){int cx=P.left+gw*k+gw/2;for(int j=0;j<2;j++){int x=j?cx+10:cx-bw-10,h=(int)(ph*hs[k*2+j]);painting::skeleton(dc,{x,P.bottom-h,x+bw,P.bottom},12,P.left,P.right,RGB(24,38,74),RGB(96,128,200));}}}
        return;}
    if(!r.ok){if(S){::textAt(dc,i18n::text(L"최종 판단은 두 번의 측정이 끝난 뒤에 나옵니다."),{K.left+20,K.top+50,K.right-20,K.top+78},smallFont,muted,one);}return;}
    if(S&&!r.when.empty()){wchar_t b[120];swprintf(b,120,i18n::text(L"마지막 비교: %ls"),r.when.c_str());::textAt(dc,b,{K.left+20,K.top+14,K.right-20,K.top+40},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
    COLORREF good=RGB(90,200,140),bad=RGB(220,120,120),flat=RGB(120,150,215),afterC=r.v==perf::Improved?blue:r.v==perf::Worse?bad:flat,beforeC=RGB(70,84,116);
    double t=nowMs()-barStart,eb=easeInOut(t/800),ea=easeInOut((t-450)/950),fade=easeOut((t-1150)/350);int tf=0,td=1;if(g){loadTarget(*g,tf,td);}
    if(ph<60){return;}
    double top=std::max({r.bFps,r.bLow,r.aFps,r.aLow,(double)tf,1.0})*1.18;auto yOf=[&](double v){return P.bottom-(int)std::lround(v/top*ph);};
    if(S){HPEN pen=CreatePen(PS_SOLID,1,line);auto op=SelectObject(dc,pen);MoveToEx(dc,P.left,P.bottom,nullptr);LineTo(dc,P.right,P.bottom);SelectObject(dc,op);DeleteObject(pen);labels();
        if(tf>0&&tf<top){int y=yOf(tf);HPEN dot=CreatePen(PS_DOT,1,RGB(230,190,90));auto od=SelectObject(dc,dot);int ob=SetBkMode(dc,TRANSPARENT);MoveToEx(dc,P.left,y,nullptr);LineTo(dc,P.left+gw-8,y);SetBkMode(dc,ob);SelectObject(dc,od);DeleteObject(dot);
            wchar_t b[48];swprintf(b,48,i18n::text(L"목표 %.0f FPS"),(double)tf);::textAt(dc,b,{P.left,y-22,P.left+180,y-2},captionFont,RGB(230,190,90),one);}}
    if(!D){return;}
    for(int k=0;k<2;k++){double bv=k?r.bLow:r.bFps,av=k?r.aLow:r.aFps;int cx=P.left+gw*k+gw/2,bx=cx-bw-10,ax=cx+10;
        auto bar=[&](int x,double v,double e,COLORREF c,bool strong,int lift){int y=yOf(v*e);if(P.bottom-y>=4){::roundBox(dc,{x,y,x+bw,P.bottom},c,c,12);}
            wchar_t b[32];swprintf(b,32,L"%.0f",v*e);::textAt(dc,b,{x-20,y-40-lift,x+bw+20,y-6-lift},strong?sectionFont:boldFont,strong?ink:RGB(170,180,200),DT_CENTER|DT_BOTTOM|DT_SINGLELINE);};
        bar(bx,bv,eb,beforeC,false,0);if(ea>0){bar(ax,av,ea,afterC,true,k==0&&r.noise>0?P.bottom-yOf(r.noise)+2:0);} /* the after label sits above the noise band */
        if(k==0&&fade>0&&r.noise>0){int y1=yOf(av+r.noise),y2=yOf(std::max(0.0,av-r.noise));HPEN pen=CreatePen(PS_SOLID,1,painting::mixColor(afterC,RGB(200,210,235),fade));auto op=SelectObject(dc,pen);for(int y:{y1,y2}){MoveToEx(dc,ax-6,y,nullptr);LineTo(dc,ax+bw+6,y);}SelectObject(dc,op);DeleteObject(pen);}
        if(fade>0){double d=av-bv,pc=bv>0?d/bv*100:0;bool noise=k==0&&r.v==perf::NoChange;COLORREF col=painting::mixColor(rowFill,noise||std::fabs(pc)<1?flat:d>0?good:bad,fade);wchar_t b[64];swprintf(b,64,L"%+.0f FPS (%+.0f%%)",d,pc);
            if(header){::textAt(dc,b,{cx-gw/2,K.top+58,cx+gw/2,K.top+94},sectionFont,col,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}else{::textAt(dc,b,{cx-gw/2,P.bottom+56,cx+gw/2,P.bottom+80},boldFont,col,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}}
    // Verdict under the chart, fading in.
    const wchar_t* n[]={L"개선 확인",L"유의미한 차이 확인 안 됨",L"성능 악화 가능성",L"측정 불충분"};COLORREF c[]={good,flat,bad,muted};int vy=header?P.bottom+70:P.bottom+86;
    if(fade<=0){return;}
    ::textAt(dc,i18n::text(n[r.v]),{K.left+20,vy,K.right-20,vy+30},sectionFont,painting::mixColor(rowFill,c[r.v],fade),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    ::textAt(dc,fmt(L"차이 %+.1f FPS (%+.1f%%) · 측정 오차 범위 ±%.1f FPS",r.diff,r.pct,r.noise),{K.left+20,vy+32,K.right-20,vy+54},smallFont,painting::mixColor(rowFill,ink,fade),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    if(!header){return;}
    ::textAt(dc,r.shaky?i18n::text(L"장면 변화가 커서 신뢰도 낮음 · 같은 장면에서 다시 비교하세요."):i18n::text(L"한 번의 비교입니다. 다시 비교해 같은 결과가 나오면 더 믿을 만합니다."),{K.left+20,vy+54,K.right-20,vy+76},captionFont,painting::mixColor(rowFill,muted,fade),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
// One animation frame: copy the cached page into a frame bitmap, draw the moving parts, put the chart on screen.
// False = not animating or the update is not only the chart (the normal paint runs).
bool perfChartFrame(HDC dc,RECT r,RECT u){if(!(view==5&&perfTab==3)||!animOn)return false;RECT K=chartRect(cmpBox());if(u.left<K.left||u.top<K.top||u.right>K.right||u.bottom>K.bottom)return false;
    if(!chartCache||chartCacheSize.cx!=r.right||chartCacheSize.cy!=r.bottom){dropChart();chartCache=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));chartCacheSize={r.right,r.bottom};
        HDC m=CreateCompatibleDC(dc);auto o=SelectObject(m,chartCache);POINT at={};MapWindowPoints(page,win,&at,1);if(!copyBackdrop(m,r,at)){FillRect(m,&r,backgroundBrush);}chartLayer=1;paint(m);chartLayer=0;SelectObject(m,o);DeleteDC(m);}
#ifdef FLYLOG
    double t0=nowMs();struct Log{double t0;~Log(){static double last=0;FILE* lf=_wfopen(L"C:\\Users\\Public\\pagelog.txt",L"a");if(lf){fprintf(lf,"F %.1f %.1f\n",t0-last,nowMs()-t0);fclose(lf);}last=t0;}} log{t0};
#endif
    HDC c=CreateCompatibleDC(dc),f=CreateCompatibleDC(dc);HBITMAP fb=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto oc=SelectObject(c,chartCache),of=SelectObject(f,fb);
    int w=K.right-K.left,h=K.bottom-K.top;BitBlt(f,K.left,K.top,w,h,c,K.left,K.top,SRCCOPY);chartLayer=2;paintCompare(f,cmpBox(),view!=6);chartLayer=0;BitBlt(dc,K.left,K.top,w,h,f,K.left,K.top,SRCCOPY);
    SelectObject(c,oc);SelectObject(f,of);DeleteObject(fb);DeleteDC(c);DeleteDC(f);return true;}
void perfPaint(HDC dc){auto* g=current();if(!g)return;RECT pr;GetClientRect(page,&pr);UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
    ::textAt(dc,g->e.name,{36,24,pr.right-36-170,58},sectionFont,ink,one);RECT C=perfContent();
    if(perfTab==0)paintHardware(dc,C);else if(perfTab==1)paintMeasure(dc,C);else if(perfTab==2)paintBoost(dc,C);else if(perfTab==3)paintCompare(dc,C);}
// ---- Commands and results.
void perfCommand(int id){if(id==414){optToggle();return;}if(id==404&&view==6){pcShown=false;pcChecking=true;hwReady=false;loadHw();optLayout();pcTimer();return;}auto* g=current();if(!g)return;
    if(id>=380&&id<=384){perfTab=id-380;if(perfTab==3&&shownCmp.ok)growBars();perfStatus.clear();perfBad=false;if(perfTab==2&&bgList.empty()&&!bgLoading)loadBackground();layout();return;}
    if(id==385){startMeasure(0);return;}
    if(id==386){restartAsAdmin();return;}
    if(id>=387&&id<=391){int tf=0,td=1;loadTarget(*g,tf,td);int v=id<391?chipFps[id-387]:hw.refresh;saveTarget(*g,v,td);layout();InvalidateRect(page,nullptr,FALSE);return;}
    if(id>=392&&id<=394){int tf=0,td=1;loadTarget(*g,tf,td);saveTarget(*g,tf,id-392);layout();InvalidateRect(page,nullptr,FALSE);return;}
    if(id==395){DWORD pid=runningPid(*g);if(!pid){perfStatus=i18n::text(L"게임을 실행한 뒤 적용하세요.");perfBad=true;InvalidateRect(page,nullptr,FALSE);return;}
        perfboost::Journal j;j.path=boostPath();auto a=perfboost::applyPower(j,pid,keyOf(*g));if(a==perfboost::Apply::Done)perfboost::restoreWhenGone(j.path,pid,boostDone);
        const wchar_t* n[]={L"고성능 전원 계획을 적용했습니다. 게임이 끝나면 되돌립니다.",L"이미 고성능 전원 계획입니다. 바꾼 것이 없습니다.",L"이 PC에는 고성능 전원 계획이 없어 적용하지 않았습니다.",L"전원 계획을 바꾸지 못했습니다."};
        perfStatus=i18n::text(n[(int)a]);perfBad=a==perfboost::Apply::Failed;hw.powerSaver=false;loadHw();layout();InvalidateRect(page,nullptr,FALSE);return;}
    if(id==396){perfboost::Journal j;j.path=boostPath();bool ok=perfboost::restore(j);perfStatus=ok?i18n::text(L"원래 전원 계획으로 되돌렸습니다."):i18n::text(L"되돌리지 못했습니다. Windows 설정 > 전원에서 원래 계획을 고르세요.");perfBad=!ok;cmpDone=cmpDone&&!ok;loadHw();layout();InvalidateRect(page,nullptr,FALSE);return;}
    if(id==397){ShellExecuteW(nullptr,L"open",L"ms-settings:gaming-gamemode",nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(id==398){ShellExecuteW(nullptr,L"open",L"ms-settings:display-advancedgraphics",nullptr,nullptr,SW_SHOWNORMAL);return;}
    if(id==399){loadBackground();return;}
    if(id>=400&&id<=403){size_t i=id-400;if(i>=bgList.size())return;auto b=bgList[i];wchar_t q[400];swprintf(q,400,i18n::text(L"%ls 프로그램에 창 닫기를 요청할까요? 저장하지 않은 작업이 있으면 그 프로그램이 물어봅니다."),b.name.c_str());
        if(MessageBoxW(GetAncestor(page,GA_ROOT),q,L"BlueAsh",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK){return;}int n=perfboost::closeWindows(b.pid);perfStatus=n?i18n::text(L"닫기를 요청했습니다."):i18n::text(L"닫을 창을 찾지 못했습니다.");perfBad=!n;bgList.clear();loadBackground();layout();return;}
    if(id==414){optToggle();return;}
    if(id==404){GUID want={},now={};perfboost::Journal j;j.path=boostPath();bool pending=j.load()&&j.hasPower;
        if(pending||!perfboost::gamePlan(want)||(perfboost::active(now)&&perfboost::isGamePlan(now))){perfStatus=pending?i18n::text(L"이미 적용된 최적화가 있습니다. 먼저 복원한 뒤 비교하세요."):i18n::text(L"적용할 최적화가 없습니다 (이미 고성능 전원 계획이거나 이 PC에 없음).");perfBad=true;InvalidateRect(page,nullptr,FALSE);return;}
        cmpStep=1;cmpDone=false;cmpA=cmpB=Measured();startMeasure(1);if(measuring){animTimer(false);}return;}
    if(id==405){cmpDone=false;perfStatus=i18n::text(L"설정을 유지합니다. 게임이 끝나면 원래 전원 계획으로 돌아갑니다.");perfBad=false;layout();InvalidateRect(page,nullptr,FALSE);return;}}
bool perfMessage(UINT msg,WPARAM wp,LPARAM lp){auto* g=current();
    if(msg==WM_TIMER&&wp==12){pcTick();return true;}
    if(msg==WM_TIMER&&wp==6){if(!(view==5&&perfTab==3)||(!(measuring&&cmpStep>0)&&nowMs()-barStart>barMs+100)){stopAnim();}RECT K=chartRect(cmpBox());InvalidateRect(page,&K,FALSE);return true;}
    if(msg==WM_APP+169){if(view==5&&perfTab==0){RECT C=perfContent();InvalidateRect(page,&C,FALSE);}return true;}
    if(msg==WM_APP+170){std::unique_ptr<perfhw::Info> r((perfhw::Info*)lp);hw=*r;hwReady=true;hwLoading=false;pcCheckDone();layout();InvalidateRect(page,nullptr,FALSE);return true;}
    if(msg==WM_APP+171){dropChart();measurePct=lp==1?(int)wp/2:lp==2?50+(int)wp/2:(int)wp;if(lp==2)cmpStep=2;RECT C=perfContent();InvalidateRect(page,&C,FALSE);return true;}
    if(msg==WM_APP+172){std::unique_ptr<perftrace::Result> r((perftrace::Result*)lp);measuring=false;needAdmin=r->needAdmin;
        if(g){auto m=toMeasured(*g,*r,wp==0);if(wp==0){lastRun=m;perfStatus=m.ok?i18n::text(L"측정 완료"):m.error.empty()?i18n::text(L"측정이 부족합니다 (10초·200프레임 이상 필요)."):m.error;perfBad=!m.ok;}
            else{cmpA=m;cmpStep=0;cmpDone=false;perfStatus=m.error.empty()?i18n::text(L"비교를 마치지 못했습니다."):m.error;perfBad=true;}}
        layout();InvalidateRect(page,nullptr,FALSE);return true;}
    if(msg==WM_APP+168){optDone((int)wp,lp!=0);return true;}
    if(msg==WM_TIMER&&wp==13){optTick();return true;}
    if(msg==WM_APP+174){setOptPref(true);checkBox(page,414,optApplied());if(wp){perfStatus=i18n::text(L"최적화 모드의 일부 설정을 바꾸지 못했습니다.");perfBad=true;}RECT pr;GetClientRect(page,&pr);InvalidateRect(page,&pr,FALSE);return true;}
    if(msg==WM_APP+175){std::unique_ptr<perftrace::Result> b((perftrace::Result*)wp),a((perftrace::Result*)lp);measuring=false;
        if(g){cmpA=toMeasured(*g,*a,false);cmpB=toMeasured(*g,*b,false);cmpResult=perf::compare(cmpA.s,cmpB.s);if(!cmpB.error.empty())cmpResult.v=perf::Insufficient;}
        cmpStep=3;cmpDone=true;if(g&&cmpA.ok&&cmpB.ok&&cmpResult.v!=perf::Insufficient){CmpRecord rc;rc.ok=true;rc.bFps=cmpA.s.fps;rc.bLow=cmpA.s.low1;rc.aFps=cmpB.s.fps;rc.aLow=cmpB.s.low1;rc.diff=cmpResult.diff;rc.pct=cmpResult.pct;rc.noise=cmpResult.noise;rc.v=cmpResult.v;rc.shaky=cmpResult.shaky;
            SYSTEMTIME st;GetLocalTime(&st);wchar_t w[32];swprintf(w,32,L"%04d-%02d-%02d_%02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute);rc.when=w;saveCmp(*g,rc);for(auto& ch:rc.when){if(ch==L'_')ch=L' ';}shownCmp=rc;shownFor=keyOf(*g);growBars();}perfStatus=i18n::text(L"비교를 마쳤습니다. 유지하거나 복원하세요.");perfBad=false;loadHw();layout();InvalidateRect(page,nullptr,FALSE);return true;}
    if(msg==WM_APP+173){std::unique_ptr<std::vector<perftrace::Busy>> v((std::vector<perftrace::Busy>*)lp);bgList=*v;bgLoading=false;layout();InvalidateRect(page,nullptr,FALSE);return true;}
    if(msg==WM_APP+176){if(!wp){problem(i18n::text(L"스마트 부스트가 바꾼 전원 계획을 되돌리지 못했습니다. Windows 설정 > 전원에서 원래 계획을 고르세요."));}cmpDone=false;loadHw();layout();InvalidateRect(page,nullptr,FALSE);return true;}
    return false;}
void perfCreate(HWND h){const wchar_t* tabs[]={L"PC 진단",L"FPS 측정·진단",L"스마트 부스트",L"전후 비교",L"업그레이드 추천"};for(int i=0;i<5;i++)pageButton(h,i18n::text(tabs[i]),0,0,140,380+i);
    pageButton(h,i18n::text(L"FPS 측정 시작 (20초)"),0,0,220,385,true);pageButton(h,i18n::text(L"관리자 권한으로 다시 실행"),0,0,240,386);
    for(int i=0;i<4;i++){wchar_t t[16];swprintf(t,16,L"%d",chipFps[i]);pageButton(h,t,0,0,84,387+i);}pageButton(h,L"",0,0,84,391);
    const wchar_t* dir[]={L"화질 우선",L"균형",L"FPS 우선"};for(int i=0;i<3;i++)pageButton(h,i18n::text(dir[i]),0,0,102,392+i);
    pageButton(h,i18n::text(L"게임 중 적용"),0,0,200,395,true);pageButton(h,i18n::text(L"복원"),0,0,140,396);pageButton(h,i18n::text(L"게임 모드 설정"),0,0,170,397);pageButton(h,i18n::text(L"그래픽 설정"),0,0,170,398);
    pageButton(h,i18n::text(L"다시 확인"),0,0,170,399);for(int i=0;i<4;i++)pageButton(h,i18n::text(L"닫기"),0,0,100,400+i);pageButton(h,i18n::text(L"성능 체크"),0,0,220,404,true);pageButton(h,i18n::text(L"유지"),0,0,200,405);pageSwitch(h,L"",0,0,64,32,414);pageButton(h,L"‹",0,0,40,410);pageButton(h,L"›",0,0,40,411);pageButton(h,i18n::text(L"자세히 보기 ›"),0,0,170,412);
    for(int id=380;id<=414;id++){HWND c=GetDlgItem(h,id);if(c)ShowWindow(c,SW_HIDE);}}
void perfDestroy(){tuneDestroy();if(page)KillTimer(page,12);optAbandon();stopAnim();perfCancel=true;liveGen++;liveOn=false;perftrace::stopLeftover();measuring=false;bgLoading=false;hwLoading=false;}

// ---- 윈도우 최적화 (sidebar, page 15): the same page window in a simple view (view 6), not tied to a game.
// Left: 최적화 모드 on/off (switch 414) and the four Windows settings it changes (perf_boost.h optmode), then two PC
// checks (background programs, pending restart). Right: 내 PC 성능 체크 — button 404 reads the hardware again (WMI on its own
// thread), then grades the parts one by one and shows the bottleneck and what to upgrade first (pc_check.h).
// The mode stays on until switched off; quitting BlueAsh puts every setting back, and the next start applies it again.
bool optMode=false;
std::wstring optPath(){return root()+L"\\opt-mode.txt";}
bool optPref(){return regString(configPath,L"OptMode")==L"1";}
void setOptPref(bool on){saveString(configPath,L"OptMode",on?L"1":L"0");}
bool optApplied(){optmode::Journal j;j.path=optPath();return j.exists();}
int optApply(){optmode::Journal j;j.path=optPath();return optmode::apply(j);}
bool optRestore(){optmode::Journal j;j.path=optPath();return optmode::restore(j);}
// BlueAsh start (perfRecover): settings left changed by a BlueAsh that did not close normally are restored, then the
// mode is applied again if it is on. Main window destroyed: everything goes back.
void optStart(){
#ifdef BLUEASH_TT
    return; /* the test build shares the journal file but not the registry: never touch the real settings */
#endif
    tuneRecover();if(!optRestore()){problem(i18n::text(L"최적화 모드가 바꾼 설정을 되돌리지 못했습니다."));}if(optPref()&&optApply()>0){problem(i18n::text(L"최적화 모드의 일부 설정을 바꾸지 못했습니다."));}}
void optQuit(){
#ifdef BLUEASH_TT
    return;
#endif
    optRestore();}
RECT optLeft(){RECT pr;GetClientRect(page,&pr);return {36,80,(pr.right-36-16+36)/2,pr.bottom-28};}
RECT optRight(){RECT pr;GetClientRect(page,&pr);RECT L=optLeft();return {L.right+16,80,pr.right-36,pr.bottom-28};}
RECT cmpBox(){return perfContent();}
// 내 PC 성능 체크 state: pcChecking from the button until the hardware arrives, then the four parts appear pcStep ms apart.
// 최적화 모드 switching: apply / restore runs on its own thread (SystemParametersInfo broadcasts a settings change to every
// window, which held the page for a moment), result in WM_APP+168. Meanwhile the six rows charge up one after another from
// the top (on) or wind down from the bottom (off), and a thin bar fills or drains: about 1.3 s, held near the end until
// the thread answers. Timer 13 repaints only the left panel.
struct OptAnim {double start=0;bool on=false,busy=false,done=false;} oa;std::atomic<bool> optThread{false}; // the thread also outlives the page; no second switch until it ends
void optAbandon(){if(page)KillTimer(page,13);oa=OptAnim{};}
const double oaRow=170,oaFill=320;double oaTotal(){return 4*oaRow+oaFill+120;}
double oaPhase(int row){double t=nowMs()-oa.start;int k=oa.on?row:4-row;return std::clamp((t-k*oaRow)/oaFill,0.0,1.0);}
bool pcShown=false,pcChecking=false;double pcRevealAt=0;const double pcStep=140;
void pcTimer(){SetTimer(page,12,15,nullptr);}
void pcTick(){RECT R=optRight();if(view!=6||(!pcChecking&&nowMs()-pcRevealAt>5*pcStep+50)){KillTimer(page,12);}InvalidateRect(page,&R,FALSE);}
void pcCheckDone(){if(!pcChecking)return;pcChecking=false;pcShown=true;pcRevealAt=nowMs();SetDlgItemTextW(page,404,i18n::text(L"다시 체크"));optLayout();pcTimer();}
pccheck::Report pcReport(){pccheck::Input in;in.gpu=hw.gpu;in.vramGB=hw.vramGB;in.cpu=hw.cpu;in.ramGB=hw.ramGB;in.modules=(int)hw.modules.size();for(auto& m:hw.modules){if(!in.ddr)in.ddr=m.type;}
    in.rated=perfhw::ratedMts(hw);in.configured=perfhw::configuredMts(hw);in.storage=hw.storage;in.width=hw.width;return pccheck::check(in);}
void optPick(){view=6;loadHw();tuneLoadTab();tuneShimmer();SetDlgItemTextW(page,404,i18n::text(pcShown?L"다시 체크":L"성능 체크"));
    if(!oa.busy){HWND s=GetDlgItem(page,414);checkBox(page,414,optApplied());switchAnims.erase(s);InvalidateRect(s,nullptr,FALSE);}} /* opening the page shows the switch as it is: sliding there read as switching it again */
void optLayout(){if(view!=6){tuneHideTabs();return;}tuneLayoutTabs();RECT L=optLeft(),R=optRight();bool t0=tuneTab==0;auto show=[&](int id,bool v,int x,int y,int w,int h=40){HWND b=GetDlgItem(page,id);v=v&&t0;if(v)putRaw(b,x,y,w,h);ShowWindow(b,v?SW_SHOW:SW_HIDE);};
    for(int id:{385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,401,402,403,405,410,411,412})ShowWindow(GetDlgItem(page,id),SW_HIDE);
    show(414,true,L.right-20-64,L.top+12,64,32);ShowWindow(GetDlgItem(page,400),SW_HIDE);
    if(pcShown||pcChecking){show(404,true,R.right-20-120,R.top+10,120,34);}else{show(404,true,R.left+20,R.top+130,R.right-R.left-40,46);}EnableWindow(GetDlgItem(page,404),!pcChecking);}
// charge 0..1 (switching 최적화 모드): a band of light passes over the row and a ring winds round the mark - clockwise in
// cobalt while charging up, back in grey while winding down - then the new mark appears with a short glow.
void checkRow(HDC dc,RECT r,int state,const wchar_t* title,const std::wstring& text,double charge=-1,bool up=true){ // state 1 = fine, 0 = attention, -1 = not in place / unknown
    COLORREF c=state>0?RGB(90,200,140):state==0?RGB(230,170,80):RGB(110,122,150);int cy=r.top+14;RECT o={r.left,cy-13,r.left+26,cy+13};
    bool moving=charge>0&&charge<1;COLORREF ray=up?RGB(70,130,255):RGB(120,132,160);
    if(moving){softBox(dc,{r.left-10,r.top-6,r.right+10,r.bottom-4},ray,ray,.16*std::sin(3.14159*charge),12);painting::glow(dc,{o.left-12,o.top-12,o.right+12,o.bottom+12},ray,(int)(120*std::sin(3.14159*charge)));}
    painting::circle(dc,o,painting::mixColor(rowFill,moving?ray:c,.22),moving?painting::mixColor(line,ray,.5):c);
    if(moving&&painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen p(painting::color(up?RGB(110,160,255):RGB(150,160,184)),2.4f);p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);
        double e=1-(1-charge)*(1-charge);float sweep=(float)(360*(up?e:1-e));if(sweep>1)g.DrawArc(&p,(float)o.left+1.5f,(float)o.top+1.5f,22.f,22.f,-90.f,sweep);}
    else if(state>0){glyphW(dc,0xE73E,o,iconFont,c);}else if(state==0){painting::exclaim(dc,{o.left+8,o.top+7,o.right-8,o.bottom-7},c);}
    ::textAt(dc,i18n::text(title),{r.left+40,r.top,r.right,r.top+28},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    ::textAt(dc,text,{r.left+40,r.top+27,r.right,r.top+47},smallFont,state==0?c:muted,DT_LEFT|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);}
// One part: name and model on the left, grade pill on the right; a skeleton until its turn in the reveal.
void partRow(HDC dc,RECT r,const wchar_t* title,const std::wstring& detail,int grade,bool shown){
    if(!shown){painting::skeleton(dc,{r.left,r.top+6,r.right,r.bottom-6},10,r.left,r.right,RGB(17,29,58),RGB(96,128,200));return;}
    static const wchar_t* names[]={L"보급",L"중급",L"상급",L"하이엔드"};static const COLORREF cs[]={RGB(235,110,110),RGB(230,170,80),RGB(90,200,140),RGB(120,170,255)};
    std::wstring t=grade>=0?i18n::text(names[grade]):i18n::text(L"확인 불가");COLORREF c=grade>=0?cs[grade]:RGB(110,122,150);int pw=pillWidth(dc,t);
    ::textAt(dc,i18n::text(title),{r.left,r.top,r.right-pw-12,r.top+22},smallBold,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    ::textAt(dc,detail.empty()?std::wstring(L"-"):detail,{r.left,r.top+22,r.right-pw-12,r.bottom},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);pill(dc,r.right-pw,r.top+(r.bottom-r.top-24)/2,t,c);}
// "AMD Ryzen 7 5800X 8-Core Processor" -> "AMD Ryzen 7 5800X"; trademarks dropped.
std::wstring cpuShort(std::wstring n){for(const wchar_t* x:{L"(R)",L"(TM)",L"(tm)"}){for(size_t p;(p=n.find(x))!=std::wstring::npos;)n.erase(p,wcslen(x));}size_t c=n.find(L"-Core");if(c!=std::wstring::npos){size_t s=n.rfind(L' ',c);if(s!=std::wstring::npos)n.resize(s);}
    size_t p=n.find(L" CPU @");if(p!=std::wstring::npos)n.resize(p);p=n.find(L" Processor");if(p!=std::wstring::npos)n.resize(p);return n;}
void pcPaint(HDC dc,RECT R){int x=R.left+20,w=R.right-R.left-40;
    if(!pcShown&&!pcChecking){::textAt(dc,i18n::text(L"그래픽카드·CPU·램·저장 장치를 확인해 병목이 있는지, 무엇을 업그레이드하면 좋은지 알려 줍니다."),{x,R.top+56,x+w,R.top+120},smallFont,muted,DT_LEFT|DT_WORDBREAK|DT_END_ELLIPSIS);return;}
    double t=pcChecking?-1:nowMs()-pcRevealAt;auto shown=[&](int i){return t>=i*pcStep;};auto rep=pcReport();
    wchar_t b[200];int y=R.top+56;const int step=46;
    swprintf(b,200,L"%ls · %.0f GB",hw.gpu.c_str(),hw.vramGB);partRow(dc,{x,y,x+w,y+42},L"그래픽카드",hw.gpu.empty()?L"":b,rep.gpu.grade,shown(0));y+=step;
    partRow(dc,{x,y,x+w,y+42},L"CPU",cpuShort(hw.cpu)+(hw.cores?L" · "+std::to_wstring(hw.cores)+L"C/"+std::to_wstring(hw.threads)+L"T":L""),rep.cpu.grade,shown(1));y+=step;
    {int mts=perfhw::configuredMts(hw);int ddr=hw.modules.empty()?0:hw.modules[0].type;swprintf(b,200,L"%.0f GB",hw.ramGB);std::wstring d=b;if(*perfhw::ddrName(ddr)){d+=L" · ";d+=perfhw::ddrName(ddr);}if(mts){d+=L" "+std::to_wstring(mts)+L" MT/s";}if(!hw.modules.empty()){d+=L" · "+std::to_wstring(hw.modules.size())+L"x";}
     partRow(dc,{x,y,x+w,y+42},L"램",d,rep.ram.grade,shown(2));y+=step;}
    partRow(dc,{x,y,x+w,y+42},L"저장 장치",hw.storage,rep.storage==0?0:rep.storage==1?2:rep.storage==2?3:-1,shown(3));y+=step;
    int foot=R.bottom-36;::textAt(dc,i18n::text(L"부품 이름으로 본 대략적인 등급이며, 게임·해상도에 따라 다릅니다."),{x,foot,x+w,R.bottom-12},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(!shown(4))return;
    {HPEN pen=CreatePen(PS_SOLID,1,line);auto op=SelectObject(dc,pen);MoveToEx(dc,x,y+2,nullptr);LineTo(dc,x+w,y+2);SelectObject(dc,op);DeleteObject(pen);}y+=14;
    auto block=[&](const wchar_t* head,const std::vector<pccheck::Line>& lines,bool numbered){if(y+24>foot)return;::textAt(dc,i18n::text(head),{x,y,x+w,y+22},smallBold,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE);y+=26;int n=1;
        for(auto& l:lines){wchar_t m[600];swprintf(m,600,i18n::text(l.key),l.arg.c_str());RECT tr={x+22,0,x+w,0};auto of=SelectObject(dc,smallFont);DrawTextW(dc,m,-1,&tr,DT_LEFT|DT_WORDBREAK|DT_CALCRECT);SelectObject(dc,of);int h=tr.bottom-tr.top;
            if(y+h>foot-6){break;}COLORREF c=l.level>=2?RGB(230,170,80):l.level==1?RGB(120,170,255):RGB(90,200,140);
            if(numbered){::textAt(dc,std::to_wstring(n++)+L".",{x,y,x+20,y+20},smallBold,c,DT_LEFT|DT_TOP|DT_SINGLELINE);}else{painting::circle(dc,{x+4,y+6,x+12,y+14},c,c);}
            ::textAt(dc,m,{x+22,y,x+w,y+h},smallFont,l.level==0?muted:ink,DT_LEFT|DT_WORDBREAK);y+=h+5;}y+=4;};
    block(L"진단",rep.findings,false);block(L"업그레이드 추천",rep.upgrades,true);}
void optPaint(HDC dc){if(tuneTab!=0){tunePaint(dc);return;}tuneClearHits();RECT L=optLeft(),R=optRight();UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
    section(dc,L,L"최적화 모드");bool on=optApplied();auto n=optmode::now();
    if(oa.busy){double t=nowMs()-oa.start,k=std::min(t/oaTotal(),oa.done?1.0:.96),e=1-(1-k)*(1-k)*(1-k),fill=oa.on?e:1-e;int bx=L.left+20,bw=L.right-L.left-40,by=L.top+64;
        wchar_t b[96];swprintf(b,96,L"%ls  %d%%",oa.on?i18n::text(L"부스트 적용 중…"):i18n::text(L"원래 설정으로 되돌리는 중…"),(int)std::lround(e*100));
        ::textAt(dc,b,{bx,L.top+44,bx+bw,L.top+62},smallFont,oa.on?RGB(130,175,255):muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        ::roundBox(dc,{bx,by,bx+bw,by+5},well,well,5);int fw=(int)(bw*fill);
        if(fw>4&&painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::GraphicsPath p;painting::path(p,{bx,by,bx+fw,by+5},5);
            Gdiplus::LinearGradientBrush gb(Gdiplus::Point(bx-1,0),Gdiplus::Point(bx+bw+1,0),painting::color(oa.on?RGB(52,92,255):RGB(70,82,110)),painting::color(oa.on?RGB(90,220,255):RGB(130,142,168)));g.FillPath(&gb,&p);
            float sx=(float)(bx+std::fmod(t*.6,(double)bw+120)-60);Gdiplus::LinearGradientBrush sh(Gdiplus::PointF(sx-40,0),Gdiplus::PointF(sx+40,0),Gdiplus::Color(0,255,255,255),Gdiplus::Color(0,255,255,255));
            Gdiplus::Color cs[]={Gdiplus::Color(0,255,255,255),Gdiplus::Color(150,255,255,255),Gdiplus::Color(0,255,255,255)};Gdiplus::REAL ps[]={0.f,.5f,1.f};sh.SetInterpolationColors(cs,ps,3);g.SetClip(&p);g.FillRectangle(&sh,sx-40,(float)by,80.f,5.f);
            painting::glow(dc,{bx+fw-14,by-10,bx+fw+14,by+15},oa.on?RGB(90,200,255):RGB(120,132,160),(int)(90*(oa.on?1:fill)));}}
    else ::textAt(dc,on?i18n::text(L"켜짐 · BlueAsh를 끄거나 이 스위치를 끄면 원래대로 돌아갑니다"):i18n::text(L"켜면 아래 Windows 설정을 게임에 맞게 바꿉니다"),{L.left+20,L.top+48,L.right-20,L.top+72},smallFont,on?RGB(90,200,140):muted,one);
    int x=L.left+20,w=L.right-L.left-40,y=L.top+84;GUID cur={};bool got=perfboost::active(cur);GUID want={};bool plan=perfboost::gamePlan(want);
    // While switching, each row shows the old state until its turn, the ring while it charges, then the new state.
    bool st[5]={n.power,n.gameMode,n.dvr,n.visual,n.mouse};double ch[5];for(int i=0;i<5;i++){ch[i]=-1;if(oa.busy){double p=oaPhase(i);ch[i]=p;st[i]=p>=1?oa.on:!oa.on;}}
    checkRow(dc,{x,y,x+w,y+56},st[0]?1:-1,L"고성능 전원 계획",!st[0]?(plan?std::wstring(L"-"):std::wstring(i18n::text(L"이 PC에 없음 (새로 만들지 않음)"))):got&&!hw.powerPlan.empty()&&!oa.busy?hw.powerPlan:std::wstring(i18n::text(L"켜짐")),ch[0],oa.on);y+=52;
    checkRow(dc,{x,y,x+w,y+56},st[1]?1:-1,L"게임 모드",st[1]?i18n::text(L"켜짐"):i18n::text(L"꺼짐"),ch[1],oa.on);y+=52;
    checkRow(dc,{x,y,x+w,y+56},st[2]?1:-1,L"백그라운드 녹화 끄기",st[2]?i18n::text(L"꺼짐"):i18n::text(L"켜짐"),ch[2],oa.on);y+=52;
    checkRow(dc,{x,y,x+w,y+56},st[3]?1:-1,L"애니메이션·투명 효과 끄기",st[3]?i18n::text(L"꺼짐"):i18n::text(L"켜짐"),ch[3],oa.on);y+=52;
    checkRow(dc,{x,y,x+w,y+56},st[4]?1:-1,L"마우스 가속 끄기",st[4]?i18n::text(L"꺼짐"):i18n::text(L"켜짐"),ch[4],oa.on);
    y+=52;::textAt(dc,i18n::text(L"시작 프로그램·백그라운드·정리 같은 나머지 점검은 위 탭에 있습니다."),{x,y+8,x+w,y+30},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    section(dc,R,L"내 PC 성능 체크");pcPaint(dc,R);}
void optToggle(){if(oa.busy||optThread)return;bool on=!optApplied();oa=OptAnim{};oa.start=nowMs();oa.on=on;oa.busy=true;perfStatus.clear();perfBad=false;checkBox(page,414,on);
    struct J {HWND page;bool on;};auto* j=new J{page,on};optThread=true;
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<J> j((J*)p);
#ifdef BLUEASH_TT
        Sleep(400);int r=0; /* the test build never changes the real Windows settings */
#else
        int r=j->on?optApply():(optRestore()?0:1);
#endif
setOptPref(j->on);optThread=false;PostMessageW(j->page,WM_APP+168,(WPARAM)r,j->on?1:0);return 0;},j,0,nullptr);
    if(t){CloseHandle(t);}else{delete j;optThread=false;setOptPref(on);PostMessageW(page,WM_APP+168,on?(WPARAM)optApply():(WPARAM)(optRestore()?0:1),on?1:0);}
    SetTimer(page,13,15,nullptr);RECT L=optLeft();InvalidateRect(page,&L,FALSE);}
void optDone(int failed,bool on){if(!oa.busy)return; /* page reopened meanwhile: the switch already shows the result */
    oa.done=true;
    if(!failed){tuneHistory().add({L"",L"",L"mode",L"",L"",on?L"1":L"0",on?i18n::text(L"최적화 모드 켬"):i18n::text(L"최적화 모드 끔"),L"done"});}
    if(on){perfStatus=failed?i18n::text(L"최적화 모드의 일부 설정을 바꾸지 못했습니다."):i18n::text(L"최적화 모드를 켰습니다.");}else{perfStatus=failed?i18n::text(L"최적화 모드가 바꾼 설정을 되돌리지 못했습니다."):i18n::text(L"원래 설정으로 되돌렸습니다.");}perfBad=failed>0;}
void optTick(){
    RECT L=optLeft();if(oa.done&&nowMs()-oa.start>=oaTotal()){oa.busy=false;KillTimer(page,13);checkBox(page,414,optApplied());loadHw();layout();InvalidateRect(page,nullptr,FALSE);return;}InvalidateRect(page,&L,FALSE);}
