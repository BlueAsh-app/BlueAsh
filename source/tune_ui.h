// 윈도우 최적화 tabs (page 15, view 6; win_tune.h is the engine, docs/windows-optimization-research.md the research):
//   0 최적화 모드   the existing switch and 내 PC 성능 체크 (perf_ui.h optPaint)
//   1 점검          refresh rate, power plan, GPU scheduling, memory integrity, disk space, restart, busy programs
//   2 백그라운드    two-second resource sample per program, close requests only for programs with windows
//   3 시작 프로그램 switched like Task Manager does (StartupApproved), every switch recorded
//   4 정리          temp / error report / crash dump files older than a day; shader caches kept on purpose
//   5 변경 기록     every change with its old value and a 되돌리기 for those that can go back
// Everything slow (process sample, startup list with file versions, folder sizes, display mode change, the clean) runs
// on its own thread; results come back as WM_APP+210..215. Row buttons are painted and hit-tested (tuneHits).
int tuneTab=0;int tuneScroll[6]={};int tuneContent[6]={};int tuneView[6]={};
// A list area: clipped in logical coordinates (the page paints shifted bitmaps) and its visible height kept for the wheel.
int listClip(HDC dc,int tab,RECT R,int top,int bottom){int saved=SaveDC(dc);IntersectClipRect(dc,R.left,top,R.right,bottom);tuneView[tab]=bottom-top;return saved;}
std::vector<wintune::Monitor> tMon;std::vector<wintune::Plan> tPlans;int tGpu=-1,tHvci=-1;bool tBattery=false;double tFreeGB=0,tFreePct=100;bool tDiagReady=false,tDiagLoading=false;
std::vector<wintune::Proc> tProcs;bool tProcLoading=false;
std::vector<wintune::Startup> tStart;bool tStartLoading=false,tStartReady=false;
std::vector<wintune::Spot> tSpots;std::set<std::wstring> tPick={L"temp",L"reports",L"dumps"};bool tSpotLoading=false,tSpotReady=false,tCleaning=false;
std::wstring tuneMsg;bool tuneBad=false;
struct TuneHit {RECT r;int act,arg;bool on;};std::vector<TuneHit> tuneHits;int tuneHover=-1;
void tuneClearHits(){tuneHits.clear();}
std::wstring tunePath(){
#ifdef BLUEASH_TT
    return root()+L"\\tune-history-test.tsv"; /* the test build keeps its own history */
#endif
    return root()+L"\\tune-history.tsv";}
wintune::History tuneHistory(){wintune::History h;h.path=tunePath();return h;}
void tuneSay(const std::wstring& m,bool bad=false){tuneMsg=m;tuneBad=bad;if(page)InvalidateRect(page,nullptr,FALSE);}
// Changes left "pending" by a BlueAsh that stopped in the middle: kept when the new value is in place, dropped otherwise.
void tuneRecover(){auto h=tuneHistory();for(auto& e:h.load()){if(e.state!=L"pending")continue;bool done=false;
        if(e.kind==L"power"){GUID now={},want={};done=wintune::activePlan(now)&&wintune::guidOf(e.after,want)&&IsEqualGUID(now,want);}
        else if(e.kind==L"display"){for(auto& m:wintune::monitors())if(m.device==e.key)done=std::to_wstring(m.cur.hz)==e.after;}
        else if(e.kind==L"startup"){size_t a=e.key.find(L'|'),b=e.key.rfind(L'|');if(a!=std::wstring::npos&&b>a){std::vector<BYTE> v;wintune::readBinary(e.key.substr(0,a)==L"HKLM"?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER,e.key.substr(a+1,b-a-1),e.key.substr(b+1),v);done=wintune::hex(v)==e.after;}}
        if(done)h.mark(e.id,L"applied");else h.drop(e.id);}}
// ---- Loading (threads).
struct DiagData {std::vector<wintune::Monitor> mon;std::vector<wintune::Plan> plans;int gpu,hvci;bool battery;double freeGB=0,freePct=100;};
void tuneLoadDiag(){if(tDiagLoading||!page)return;tDiagLoading=true;HWND pg=page;
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* d=new DiagData;d->mon=wintune::monitors();d->plans=wintune::plans();d->gpu=wintune::gpuScheduling();d->hvci=wintune::memoryIntegrity();d->battery=wintune::onBattery();wintune::systemDriveFree(d->freeGB,d->freePct);
        if(!PostMessageW((HWND)p,WM_APP+210,0,(LPARAM)d)){delete d;}return 0;},pg,0,nullptr);if(t)CloseHandle(t);else tDiagLoading=false;}
void tuneLoadProcs(){if(tProcLoading||!page)return;tProcLoading=true;InvalidateRect(page,nullptr,FALSE);
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto a=wintune::snapshot();Sleep(2000);auto b=wintune::snapshot();auto* v=new std::vector<wintune::Proc>(wintune::compare(a,b,GetCurrentProcessId()));
        if(!PostMessageW((HWND)p,WM_APP+211,0,(LPARAM)v)){delete v;}return 0;},page,0,nullptr);if(t)CloseHandle(t);else tProcLoading=false;}
void tuneLoadStart(){if(tStartLoading||!page)return;tStartLoading=true;
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);auto* v=new std::vector<wintune::Startup>(wintune::startups());CoUninitialize();if(!PostMessageW((HWND)p,WM_APP+212,0,(LPARAM)v)){delete v;}return 0;},page,0,nullptr);
    if(t)CloseHandle(t);else tStartLoading=false;}
void tuneLoadSpots(){if(tSpotLoading||!page)return;tSpotLoading=true;
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* v=new std::vector<wintune::Spot>(wintune::spots());for(auto& s:*v){for(auto& d:s.dirs){wintune::Swept r;wintune::sweep(d,24,false,r);s.bytes+=r.bytes;s.files+=r.files;if(!r.readable)s.readable=false;}}
        if(!PostMessageW((HWND)p,WM_APP+213,0,(LPARAM)v)){delete v;}return 0;},page,0,nullptr);if(t)CloseHandle(t);else tSpotLoading=false;}
void tuneLoadTab(){if(tuneTab==1){tuneLoadDiag();if(tProcs.empty())tuneLoadProcs();loadHw();}else if(tuneTab==2&&tProcs.empty())tuneLoadProcs();else if(tuneTab==3&&!tStartReady)tuneLoadStart();else if(tuneTab==4&&!tSpotReady)tuneLoadSpots();}
// ---- Layout pieces.
RECT tuneTabsRect(){RECT pr;GetClientRect(page,&pr);return {36,24,pr.right-36,64};}
RECT tunePanel(){RECT pr;GetClientRect(page,&pr);return {36,80,pr.right-36,pr.bottom-28};}
const wchar_t* tuneTabName(int i){const wchar_t* n[]={L"최적화 모드",L"점검",L"백그라운드",L"시작 프로그램",L"정리",L"변경 기록"};return i18n::text(n[i]);}
void tuneLayoutTabs(){RECT T=tuneTabsRect();int n=6,gap=8,w=(T.right-T.left-gap*(n-1))/n;for(int i=0;i<n;i++){HWND b=GetDlgItem(page,420+i);if(!b)continue;putRaw(b,T.left+i*(w+gap),T.top,w,40);ShowWindow(b,SW_SHOW);
    LONG_PTR sel=tuneTab==i?1:0;if(GetWindowLongPtrW(b,GWLP_USERDATA)!=sel){SetWindowLongPtrW(b,GWLP_USERDATA,sel);InvalidateRect(b,nullptr,FALSE);}}}
void tuneCreate(HWND h){for(int i=0;i<6;i++){pageButton(h,tuneTabName(i),0,0,120,420+i);ShowWindow(GetDlgItem(h,420+i),SW_HIDE);}}
void tuneHideTabs(){for(int i=0;i<6;i++)ShowWindow(GetDlgItem(page,420+i),SW_HIDE);}
// ---- Painting helpers.
std::wstring sizeText(unsigned long long b){wchar_t t[32];if(b>=1000000000ULL)swprintf(t,32,L"%.1f GB",b/1e9);else if(b>=1000000ULL)swprintf(t,32,L"%.0f MB",b/1e6);else swprintf(t,32,L"%.0f KB",b/1e3);return t;}
// A painted row button: outline (or cobalt when primary), brighter while the mouse is on it; recorded for hit-testing.
void tuneButton(HDC dc,RECT r,const std::wstring& t,int act,int arg,bool primary=false,bool enabled=true){int idx=(int)tuneHits.size();tuneHits.push_back({r,act,arg,enabled});bool hov=enabled&&idx==tuneHover;
    COLORREF fill=primary?(hov?RGB(74,112,255):blue):(hov?painting::mixColor(rowFill,RGB(255,255,255),.08):panel),edge=primary?fill:(hov?RGB(84,112,180):line);
    ::roundBox(dc,r,enabled?fill:panel,enabled?edge:line,10);::textAt(dc,t,r,smallFont,enabled?(primary?RGB(255,255,255):ink):muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
int tuneButtonWidth(HDC dc,const std::wstring& t){SIZE s={};auto of=SelectObject(dc,smallFont);GetTextExtentPoint32W(dc,t.c_str(),(int)t.size(),&s);SelectObject(dc,of);return std::max(84,(int)s.cx+32);}
// The evidence level of a check (docs §2): a small outlined pill.
void evidence(HDC dc,int right,int y,int level){const wchar_t* n[]={L"효과 적음",L"상황 따라 효과",L"효과 큼",L"보안 기능"};COLORREF c[]={RGB(120,132,160),RGB(120,170,255),RGB(90,200,140),RGB(230,170,80)};std::wstring t=i18n::text(n[level]);
    SIZE s={};auto of=SelectObject(dc,captionFont);GetTextExtentPoint32W(dc,t.c_str(),(int)t.size(),&s);SelectObject(dc,of);int w=s.cx+20;::roundBox(dc,{right-w,y,right,y+22},panel,c[level],11);::textAt(dc,t,{right-w,y,right,y+22},captionFont,c[level],DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
void stateMark(HDC dc,RECT o,int state){COLORREF c=state>0?RGB(90,200,140):state==0?RGB(230,170,80):RGB(110,122,150);painting::circle(dc,o,painting::mixColor(rowFill,c,.22),c);
    if(state>0)::glyph(dc,0xE73E,o,iconFont,c);else if(state==0)painting::exclaim(dc,{o.left+8,o.top+7,o.right-8,o.bottom-7},c);else ::glyph(dc,0xE946,o,captionFont,c);}
// One check row: mark, title, one line of detail, then two fixed columns on the right: the effect pill (centred in its
// column) and the action. Every row reserves both, so pills and buttons line up down the list. Actions are only three
// words: 바꾸기 (BlueAsh changes it here), 설정 열기 (a Windows page), 자세히 (another tab of this page).
const wchar_t* const actionWords[]={L"바꾸기",L"설정 열기",L"자세히"};const wchar_t* const effectWords[]={L"효과 적음",L"상황 따라 효과",L"효과 큼",L"보안 기능"};
int widest(HDC dc,HFONT f,const wchar_t* const* w,int n){SIZE s={};int m=0;auto of=SelectObject(dc,f);for(int i=0;i<n;i++){std::wstring t=i18n::text(w[i]);GetTextExtentPoint32W(dc,t.c_str(),(int)t.size(),&s);m=std::max<int>(m,s.cx);}SelectObject(dc,of);return m;}
void diagRow(HDC dc,int x,int y,int w,int state,const std::wstring& title,const std::wstring& text,int ev,int action=-1,int act=0,int arg=0,bool enabled=true){
    stateMark(dc,{x,y+4,x+26,y+30},state);int bw=std::max(84,widest(dc,smallFont,actionWords,3)+32),ew=widest(dc,captionFont,effectWords,4)+20,right=x+w;
    if(action>=0){tuneButton(dc,{right-bw,y+4,right,y+38},i18n::text(actionWords[action]),act,arg,false,enabled);}right-=bw+12;
    {std::wstring t=i18n::text(effectWords[ev]);SIZE s={};auto of=SelectObject(dc,captionFont);GetTextExtentPoint32W(dc,t.c_str(),(int)t.size(),&s);SelectObject(dc,of);evidence(dc,right-(ew-(s.cx+20))/2,y+9,ev);}right-=ew+12;
    ::textAt(dc,title,{x+40,y,right,y+22},smallBold,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);::textAt(dc,text,{x+40,y+22,right,y+42},captionFont,state==0?RGB(230,170,80):muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
void tuneSkeletonRows(HDC dc,RECT R,int y,int n,int h){for(int i=0;i<n&&y+h<=R.bottom;i++,y+=h+8)painting::skeleton(dc,{R.left+20,y,R.right-20,y+h},10,R.left,R.right,RGB(17,29,58),RGB(96,128,200));}
void tuneFooter(HDC dc,RECT R,const std::wstring& t){::textAt(dc,t,{R.left+20,R.bottom-34,R.right-20,R.bottom-10},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
void tuneHead(HDC dc,RECT R,const wchar_t* title,const std::wstring& sub){::roundBox(dc,R,rowFill,line,16);::textAt(dc,i18n::text(title),{R.left+20,R.top+14,R.right-20,R.top+40},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    ::textAt(dc,sub,{R.left+20,R.top+40,R.right-20,R.top+62},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
// ---- Tab 1: 점검
void paintChecks(HDC dc,RECT R){tuneHead(dc,R,L"점검",i18n::text(L"오른쪽 표시는 이 항목이 게임 성능에 주는 효과의 크기입니다. 버튼으로 바로 바꾸거나 설정을 열 수 있습니다."));int x=R.left+20,w=R.right-R.left-40,y=R.top+76-tuneScroll[1],step=52;
    int saved=listClip(dc,1,R,R.top+72,R.bottom-12);
    if(!tDiagReady){tuneSkeletonRows(dc,R,R.top+76,6,42);RestoreDC(dc,saved);return;}wchar_t b[300];
    for(size_t i=0;i<tMon.size();i++){auto& m=tMon[i];bool low=m.maxHz>m.cur.hz;swprintf(b,300,low?i18n::text(L"지금 %d Hz · 이 해상도의 최대 %d Hz"):i18n::text(L"%d Hz · 최대 주사율로 동작 중"),m.cur.hz,m.maxHz);
        diagRow(dc,x,y,w,low?0:1,std::wstring(i18n::text(L"모니터 주사율"))+L" · "+m.name,b,2,low?0:1,low?1:8,(int)i);y+=step;}
    {std::wstring cur;bool saver=false;for(auto& p:tPlans){if(p.active){cur=p.name;saver=IsEqualGUID(p.id,GUID{0xa1841308,0x3541,0x4fab,{0xbc,0x81,0xf7,0x15,0x56,0xf2,0x0b,0x4a}})!=0;}}
        bool managed=optApplied();std::wstring t=cur.empty()?std::wstring(L"-"):cur;if(managed)t+=std::wstring(L" · ")+i18n::text(L"최적화 모드가 관리 중");else if(tBattery)t+=std::wstring(L" · ")+i18n::text(L"배터리 사용 중 (전원을 연결하면 성능이 오릅니다)");
        diagRow(dc,x,y,w,saver||tBattery?0:1,i18n::text(L"전원 계획"),t,1,0,2,0,!managed&&tPlans.size()>1);y+=step;}
    diagRow(dc,x,y,w,tGpu==1?1:-1,i18n::text(L"하드웨어 가속 GPU 예약"),tGpu<0?i18n::text(L"이 PC·드라이버에서 지원하지 않음"):tGpu?i18n::text(L"켜짐"):i18n::text(L"꺼짐 · DLSS 프레임 생성을 쓰려면 켜야 합니다"),0,1,3,0,tGpu>=0);y+=step;
    diagRow(dc,x,y,w,tHvci==0?1:-1,i18n::text(L"메모리 무결성"),tHvci==1?i18n::text(L"켜짐 · 보안 기능이라 BlueAsh는 바꾸지 않습니다"):tHvci==0?i18n::text(L"꺼짐"):i18n::text(L"확인할 수 없음"),3,1,4);y+=step;
    swprintf(b,300,i18n::text(L"남은 공간 %.0f%% (%.0f GB)"),tFreePct,tFreeGB);diagRow(dc,x,y,w,tFreePct<10?0:1,i18n::text(L"시스템 드라이브 공간"),b,0,2,5);y+=step;
    diagRow(dc,x,y,w,!hwReady?-1:hw.reboot?0:1,i18n::text(L"재부팅 대기"),!hwReady?std::wstring(L"…"):hw.reboot?i18n::text(L"재부팅을 기다리는 업데이트·설치가 있음"):i18n::text(L"없음"),1,1,9);y+=step;
    {bool busy=!tProcs.empty()&&tProcs[0].cpu>=5;std::wstring t=tProcLoading?std::wstring(L"…"):i18n::text(L"CPU를 많이 쓰는 프로그램이 없습니다");if(busy&&!tProcLoading){swprintf(b,300,L"%ls · CPU %.0f%%",tProcs[0].name.c_str(),tProcs[0].cpu);t=b;}
        diagRow(dc,x,y,w,tProcLoading?-1:busy?0:1,i18n::text(L"백그라운드 프로그램"),t,1,2,6);y+=step;}
    {auto n=optmode::now();int good=(n.gameMode?1:0)+(n.dvr?1:0);swprintf(b,300,i18n::text(L"게임 모드·백그라운드 녹화 중 %d개가 게임에 맞게 설정됨"),good);
        diagRow(dc,x,y,w,good==2?1:0,i18n::text(L"게임 관련 Windows 설정"),b,1,2,7);y+=step;}
    tuneContent[1]=y+tuneScroll[1]-(R.top+72);RestoreDC(dc,saved);}
// ---- Tab 2: 백그라운드
const wchar_t* kindName(const std::wstring& k){if(k==L"browser")return L"브라우저";if(k==L"launcher")return L"게임 런처";if(k==L"overlay")return L"오버레이·녹화";if(k==L"device")return L"장치·RGB";if(k==L"sync")return L"클라우드 동기화";if(k==L"update")return L"업데이트";return L"";}
void paintProcs(HDC dc,RECT R){tuneHead(dc,R,L"백그라운드",i18n::text(L"2초 동안 잰 사용량입니다. BlueAsh는 프로그램을 강제로 끄지 않고, 창이 있는 프로그램에만 닫기를 요청합니다."));
    {std::wstring t=i18n::text(L"다시 분석");int bw=tuneButtonWidth(dc,t);tuneButton(dc,{R.right-20-bw,R.top+12,R.right-20,R.top+44},t,20,0,false,!tProcLoading);}
    int x=R.left+20,w=R.right-R.left-40;int cCpu=x+w-370,cRam=x+w-280,cDisk=x+w-180;int top=R.top+72;
    for(auto [cx,t]:std::initializer_list<std::pair<int,const wchar_t*>>{{x,L"프로그램"},{cCpu,L"CPU"},{cRam,L"메모리"},{cDisk,L"디스크"}})::textAt(dc,i18n::text(t),{cx,top,cx+(cx==x?200:84),top+20},captionFont,muted,(cx==x?DT_LEFT:DT_RIGHT)|DT_VCENTER|DT_SINGLELINE);
    int saved=listClip(dc,2,R,top+26,R.bottom-48);int y=top+28-tuneScroll[2];
    if(tProcLoading&&tProcs.empty()){tuneSkeletonRows(dc,R,top+28,8,34);}
    else{wchar_t b[64];for(size_t i=0;i<tProcs.size()&&i<30;i++){auto& p=tProcs[i];if(y+40>R.bottom-48)break;if(i%2==0)::roundBox(dc,{x-8,y,x+w+8,y+40},painting::mixColor(rowFill,RGB(255,255,255),.025),painting::mixColor(rowFill,RGB(255,255,255),.025),8);
        std::wstring n=p.name;if(p.count>1){n+=L"  ×"+std::to_wstring(p.count);}::textAt(dc,n,{x+4,y,cCpu-120,y+40},smallFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(*kindName(p.kind)){std::wstring k=i18n::text(kindName(p.kind));::textAt(dc,k,{cCpu-116,y,cCpu-6,y+40},captionFont,RGB(120,170,255),DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        swprintf(b,64,L"%.1f%%",p.cpu);::textAt(dc,b,{cCpu,y,cCpu+84,y+40},smallFont,p.cpu>=10?RGB(230,170,80):ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        swprintf(b,64,L"%.0f MB",p.ramMB);::textAt(dc,b,{cRam,y,cRam+84,y+40},smallFont,p.ramMB>=2000?RGB(230,170,80):ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        swprintf(b,64,L"%.1f MB/s",p.diskMBs);::textAt(dc,b,{cDisk,y,cDisk+84,y+40},smallFont,p.diskMBs>=20?RGB(230,170,80):ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        if(p.window){tuneButton(dc,{x+w-80,y+5,x+w,y+35},i18n::text(L"닫기"),21,(int)i);}y+=44;}}
    tuneContent[2]=(int)std::min<size_t>(tProcs.size(),30)*44;RestoreDC(dc,saved);tuneFooter(dc,R,i18n::text(L"게임 중에 CPU·디스크를 많이 쓰는 프로그램은 끊김의 원인이 될 수 있습니다."));}
// ---- Tab 3: 시작 프로그램
const wchar_t* whereName(const std::wstring& w){return w==L"user"?L"이 계정":w==L"all"?L"모든 계정":w==L"user-folder"?L"시작 폴더":L"모든 계정 시작 폴더";}
void paintStartups(HDC dc,RECT R){int on=0;bool needAdmin=false;bool el=wintune::elevated();for(auto& s:tStart){if(s.on)on++;if(s.admin&&!el)needAdmin=true;}wchar_t b[200];swprintf(b,200,i18n::text(L"Windows를 켤 때 함께 시작하는 프로그램 · 켜짐 %d개 · 꺼짐 %d개"),on,(int)tStart.size()-on);
    tuneHead(dc,R,L"시작 프로그램",tStartReady?std::wstring(b):std::wstring(L"…"));
    if(needAdmin){std::wstring t=i18n::text(L"관리자 권한으로 다시 실행");int bw=tuneButtonWidth(dc,t);tuneButton(dc,{R.right-20-bw,R.top+12,R.right-20,R.top+44},t,30,0);}
    int x=R.left+20,w=R.right-R.left-40,top=R.top+72;int saved=listClip(dc,3,R,top,R.bottom-48);int y=top-tuneScroll[3];
    if(!tStartReady){tuneSkeletonRows(dc,R,top,7,40);}
    else if(tStart.empty()){::textAt(dc,i18n::text(L"시작 프로그램이 없습니다."),{x,top+20,x+w,top+60},smallFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    else for(size_t i=0;i<tStart.size();i++){auto& s=tStart[i];if(y+46>R.bottom-48)break;bool locked=s.protectedItem||(s.admin&&!el);
        ::textAt(dc,s.name,{x+4,y+2,x+w-180,y+24},smallBold,s.on?ink:muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        std::wstring d=std::wstring(i18n::text(whereName(s.where)));if(!s.publisher.empty())d=s.publisher+L" · "+d;::textAt(dc,d,{x+4,y+24,x+w-180,y+44},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(s.protectedItem){::textAt(dc,i18n::text(L"보안·안티치트 · 끄지 않음"),{x+w-240,y,x+w,y+46},captionFont,RGB(230,170,80),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
        else{RECT track={x+w-52,y+11,x+w-2,y+35};int idx=(int)tuneHits.size();tuneHits.push_back({{x+w-60,y+4,x+w,y+42},31,(int)i,!locked});drawSwitch(dc,track,s.on?1.0:0.0,idx==tuneHover&&!locked);
            if(s.admin&&!el)::textAt(dc,i18n::text(L"관리자 필요"),{x+w-200,y,x+w-64,y+46},captionFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
        y+=50;if(i+1<tStart.size()){HPEN pen=CreatePen(PS_SOLID,1,painting::mixColor(rowFill,line,.6));auto op=SelectObject(dc,pen);MoveToEx(dc,x,y-3,nullptr);LineTo(dc,x+w,y-3);SelectObject(dc,op);DeleteObject(pen);}}
    tuneContent[3]=(int)tStart.size()*50;RestoreDC(dc,saved);tuneFooter(dc,R,i18n::text(L"작업 관리자의 '시작 앱'과 같은 방식입니다. 끈 프로그램은 다음 Windows 시작부터 자동으로 켜지지 않으며, 언제든 다시 켤 수 있습니다."));}
// ---- Tab 4: 정리
const wchar_t* spotName(const std::wstring& id){return id==L"temp"?L"사용자 임시 파일":id==L"wintemp"?L"Windows 임시 파일":id==L"reports"?L"오류 보고서":id==L"reports-all"?L"오류 보고서 (모든 계정)":L"프로그램 충돌 덤프";}
const wchar_t* spotNote(const std::wstring& id){return id==L"temp"||id==L"wintemp"?L"프로그램이 쓰고 남긴 임시 파일 중 하루 넘게 지난 것":id==L"dumps"?L"프로그램이 비정상 종료될 때 남긴 메모리 덤프":L"Windows가 모아 둔 프로그램 오류 보고서";}
void paintClean(HDC dc,RECT R){tuneHead(dc,R,L"정리",i18n::text(L"하루 넘게 지난 파일만 지웁니다. 사용 중인 파일은 건너뜁니다. 지운 파일은 되돌릴 수 없습니다."));
    int x=R.left+20,w=R.right-R.left-40,y=R.top+76;bool el=wintune::elevated();unsigned long long total=0;
    if(!tSpotReady){tuneSkeletonRows(dc,R,y,5,40);}
    else for(size_t i=0;i<tSpots.size();i++){auto& s=tSpots[i];bool locked=s.admin&&!el,pick=tPick.count(s.id)&&!locked;if(pick)total+=s.bytes;
        RECT track={x,y+9,x+50,y+33};int idx=(int)tuneHits.size();tuneHits.push_back({{x-4,y,x+w-120,y+44},40,(int)i,!locked&&!tCleaning});drawSwitch(dc,track,pick?1.0:0.0,idx==tuneHover&&!locked);
        ::textAt(dc,i18n::text(spotName(s.id)),{x+66,y+2,x+w-140,y+24},smallBold,locked?muted:ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        ::textAt(dc,locked?std::wstring(i18n::text(L"관리자 권한으로 실행했을 때만 정리합니다")):std::wstring(i18n::text(spotNote(s.id))),{x+66,y+24,x+w-140,y+44},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        wchar_t b[64];swprintf(b,64,L"%ls",(locked&&!s.readable)?L"-":sizeText(s.bytes).c_str());::textAt(dc,b,{x+w-130,y,x+w,y+44},smallBold,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);y+=52;}
    if(tSpotReady){::textAt(dc,i18n::text(L"GPU 셰이더 캐시는 지우지 않습니다 — 지우면 게임이 처음 한동안 셰이더를 다시 만들며 오히려 끊깁니다."),{x,y+4,x+w,y+28},captionFont,RGB(120,170,255),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        wchar_t b[120];swprintf(b,120,i18n::text(L"선택한 항목 %ls"),sizeText(total).c_str());::textAt(dc,b,{x,R.bottom-60,x+w-200,R.bottom-20},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        std::wstring t=tCleaning?std::wstring(i18n::text(L"정리하는 중…")):std::wstring(i18n::text(L"정리하기"));tuneButton(dc,{x+w-180,R.bottom-62,x+w,R.bottom-18},t,41,0,true,!tCleaning&&total>0);}}
// ---- Tab 5: 변경 기록
const wchar_t* stateName(const std::wstring& s){return s==L"applied"?L"적용됨":s==L"restored"?L"되돌림":s==L"pending"?L"진행 중":L"완료";}
bool undoable(const wintune::Entry& e){return e.state==L"applied"&&(e.kind==L"power"||e.kind==L"display"||e.kind==L"startup");}
void paintHistory(HDC dc,RECT R){auto all=tuneHistory().load();tuneHead(dc,R,L"변경 기록",i18n::text(L"BlueAsh가 바꾼 Windows 설정과 그 이전 값입니다. 되돌릴 수 있는 것은 버튼이 있습니다."));
    int x=R.left+20,w=R.right-R.left-40,top=R.top+76;int saved=listClip(dc,5,R,top-4,R.bottom-12);int y=top-tuneScroll[5];
    if(all.empty()){::textAt(dc,i18n::text(L"아직 바뀐 설정이 없습니다."),{x,top+20,x+w,top+60},smallFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    for(int i=(int)all.size()-1;i>=0;i--){auto& e=all[i];if(y+46>R.bottom-12)break;bool can=undoable(e);int right=x+w;
        if(can){std::wstring t=i18n::text(L"되돌리기");int bw=tuneButtonWidth(dc,t);tuneButton(dc,{right-bw,y+6,right,y+40},t,50,i);right-=bw+12;}
        else{std::wstring st=e.kind==L"clean"?std::wstring(i18n::text(L"되돌릴 수 없음")):std::wstring(i18n::text(stateName(e.state)));::textAt(dc,st,{right-200,y,right,y+46},captionFont,e.state==L"restored"?muted:RGB(90,200,140),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);right-=210;}
        ::textAt(dc,e.label,{x,y+2,right,y+24},smallBold,e.state==L"restored"?muted:ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        std::wstring sub=e.when;if(can)sub+=std::wstring(L" · ")+i18n::text(stateName(e.state));::textAt(dc,sub,{x,y+24,right,y+44},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);y+=52;}
    tuneContent[5]=(int)all.size()*52;RestoreDC(dc,saved);}
// The status line under the panel (results of the last action).
void tunePaint(HDC dc){tuneHits.clear();RECT R=tunePanel();
    if(tuneTab==1)paintChecks(dc,R);else if(tuneTab==2)paintProcs(dc,R);else if(tuneTab==3)paintStartups(dc,R);else if(tuneTab==4)paintClean(dc,R);else if(tuneTab==5)paintHistory(dc,R);
    if(!tuneMsg.empty()){RECT pr;GetClientRect(page,&pr);::textAt(dc,tuneMsg,{R.left+4,R.bottom+2,R.right,pr.bottom-4},captionFont,tuneBad?RGB(235,110,110):RGB(90,200,140),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}}
// The page is going away: threads still running post to a dead window, so nothing must stay marked as loading.
void tuneDestroy(){tDiagLoading=tProcLoading=tStartLoading=tSpotLoading=tCleaning=false;tDiagReady=tStartReady=tSpotReady=false;tProcs.clear();tuneHits.clear();tuneHover=-1;}
bool tuneLoading(){return (tuneTab==1&&!tDiagReady)||(tuneTab<=2&&tProcLoading)||(tuneTab==3&&!tStartReady)||(tuneTab==4&&!tSpotReady);}
void tuneShimmer(){if(tuneLoading())SetTimer(page,14,40,nullptr);}
// ---- Actions.
bool ask(const std::wstring& q){return MessageBoxW(GetAncestor(page,GA_ROOT),q.c_str(),L"BlueAsh",MB_OKCANCEL|MB_ICONQUESTION)==IDOK;}
void tuneSetTab(int t){if(t==tuneTab)return;tuneTab=t;tuneHover=-1;tuneMsg.clear();layout();tuneLoadTab();tuneShimmer();InvalidateRect(page,nullptr,FALSE);}
void changePlan(){POINT c;GetCursorPos(&c);HMENU m=CreatePopupMenu();for(size_t i=0;i<tPlans.size();i++)AppendMenuW(m,MF_STRING|(tPlans[i].active?MF_CHECKED:0),1+i,tPlans[i].name.c_str());
    int pick=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,c.x,c.y,0,page,nullptr);DestroyMenu(m);if(pick<1||pick>(int)tPlans.size())return;auto& want=tPlans[pick-1];if(want.active)return;
    GUID now={};if(!wintune::activePlan(now))return;std::wstring was;for(auto& p:tPlans)if(p.active)was=p.name;
    wchar_t q[400];swprintf(q,400,i18n::text(L"전원 계획을 '%ls'(으)로 바꿀까요? 변경 기록에서 언제든 되돌릴 수 있습니다."),want.name.c_str());if(!ask(q))return;
    auto h=tuneHistory();std::wstring id=h.add({L"",L"",L"power",wintune::guidText(want.id),wintune::guidText(now),wintune::guidText(want.id),std::wstring(i18n::text(L"전원 계획"))+L": "+was+L" → "+want.name,L"pending"});
    if(wintune::setPlan(want.id)){h.mark(id,L"applied");tuneSay(i18n::text(L"전원 계획을 바꿨습니다."));}else{h.drop(id);tuneSay(i18n::text(L"전원 계획을 바꾸지 못했습니다."),true);}tuneLoadDiag();loadHw();}
struct HzJob {HWND page;std::wstring device,id;int hz;};
void changeHz(int i){if(i<0||i>=(int)tMon.size())return;auto m=tMon[i];wchar_t q[400];swprintf(q,400,i18n::text(L"%ls 모니터를 %d Hz에서 %d Hz로 바꿀까요? 화면이 잠깐 깜빡일 수 있습니다."),m.name.c_str(),m.cur.hz,m.maxHz);if(!ask(q))return;
    auto h=tuneHistory();wchar_t l[200];swprintf(l,200,L"%ls: %ls %d → %d Hz",i18n::text(L"주사율"),m.name.c_str(),m.cur.hz,m.maxHz);std::wstring id=h.add({L"",L"",L"display",m.device,std::to_wstring(m.cur.hz),std::to_wstring(m.maxHz),l,L"pending"});
    auto* j=new HzJob{page,m.device,id,m.maxHz};HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{HzJob* j=(HzJob*)p;bool ok=wintune::setHz(j->device,j->hz);if(!PostMessageW(j->page,WM_APP+215,ok,(LPARAM)j)){delete j;}return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{delete j;h.drop(id);}}
void toggleStartup(int i){if(i<0||i>=(int)tStart.size())return;auto& s=tStart[i];if(s.protectedItem)return;bool turnOn=!s.on;std::wstring name=wintune::approvedName(s);
    if(!turnOn){wchar_t q[400];swprintf(q,400,i18n::text(L"'%ls'이(가) Windows를 켤 때 자동으로 시작하지 않게 할까요? 언제든 다시 켤 수 있습니다."),s.name.c_str());if(!ask(q))return;}
    std::vector<BYTE> old;bool had=wintune::readBinary(s.root,s.approvedKey,name,old);FILETIME now;GetSystemTimeAsFileTime(&now);auto nb=wintune::approvedBytes(turnOn,now);
    auto h=tuneHistory();std::wstring key=std::wstring(s.root==HKEY_LOCAL_MACHINE?L"HKLM|":L"HKCU|")+s.approvedKey+L"|"+name;
    std::wstring id=h.add({L"",L"",L"startup",key,had?wintune::hex(old):L"-",wintune::hex(nb),std::wstring(turnOn?i18n::text(L"시작 프로그램 켬"):i18n::text(L"시작 프로그램 끔"))+L": "+s.name,L"pending"});
    if(wintune::writeBinary(s.root,s.approvedKey,name,nb)){h.mark(id,L"applied");s.on=turnOn;tuneSay(turnOn?i18n::text(L"다음 Windows 시작부터 자동으로 켜집니다."):i18n::text(L"다음 Windows 시작부터 자동으로 켜지지 않습니다."));}
    else{h.drop(id);tuneSay(s.admin?i18n::text(L"모든 계정 항목은 관리자 권한이 필요합니다."):i18n::text(L"바꾸지 못했습니다."),true);}InvalidateRect(page,nullptr,FALSE);}
struct CleanJob {HWND page;std::vector<std::wstring> dirs;};
void startClean(){std::vector<std::wstring> dirs;unsigned long long total=0;int files=0;bool el=wintune::elevated();for(auto& s:tSpots){if(!tPick.count(s.id)||(s.admin&&!el))continue;total+=s.bytes;files+=s.files;for(auto& d:s.dirs)dirs.push_back(d);}
    if(dirs.empty()||!total){return;}wchar_t q[400];swprintf(q,400,i18n::text(L"파일 %d개 (%ls)를 지울까요? 지운 파일은 되돌릴 수 없습니다."),files,sizeText(total).c_str());if(!ask(q)){return;}
    tCleaning=true;InvalidateRect(page,nullptr,FALSE);auto* j=new CleanJob{page,dirs};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<CleanJob> j((CleanJob*)p);auto* r=new wintune::Swept;for(auto& d:j->dirs){wintune::Swept x;wintune::sweep(d,24,true,x);r->bytes+=x.bytes;r->files+=x.files;r->skipped+=x.skipped;}
        if(!PostMessageW(j->page,WM_APP+214,0,(LPARAM)r)){delete r;}return 0;},j,0,nullptr);if(t)CloseHandle(t);else{delete j;tCleaning=false;}}
void undo(int i){auto h=tuneHistory();auto all=h.load();if(i<0||i>=(int)all.size())return;auto e=all[i];if(!undoable(e))return;
    wchar_t q[400];swprintf(q,400,i18n::text(L"'%ls'을(를) 이전 값으로 되돌릴까요?"),e.label.c_str());if(!ask(q))return;bool ok=false;
    if(e.kind==L"power"){GUID g={};ok=wintune::guidOf(e.before,g)&&wintune::setPlan(g);}
    else if(e.kind==L"display"){ok=wintune::setHz(e.key,_wtoi(e.before.c_str()));}
    else if(e.kind==L"startup"){size_t a=e.key.find(L'|'),b=e.key.rfind(L'|');if(a!=std::wstring::npos&&b>a){HKEY root=e.key.substr(0,a)==L"HKLM"?HKEY_LOCAL_MACHINE:HKEY_CURRENT_USER;std::wstring k=e.key.substr(a+1,b-a-1),n=e.key.substr(b+1);
        if(e.before==L"-")ok=wintune::removeValue(root,k,n);else{std::vector<BYTE> v;ok=wintune::unhex(e.before,v)&&wintune::writeBinary(root,k,n,v);}}}
    if(ok){h.mark(e.id,L"restored");tuneSay(i18n::text(L"이전 값으로 되돌렸습니다."));tStartReady=false;tDiagReady=false;}else tuneSay(i18n::text(L"되돌리지 못했습니다. 관리자 권한이 필요할 수 있습니다."),true);InvalidateRect(page,nullptr,FALSE);}
void tuneAct(int act,int arg){switch(act){
    case 1:changeHz(arg);break;case 2:changePlan();break;
    case 3:ShellExecuteW(nullptr,L"open",L"ms-settings:display-advancedgraphics",nullptr,nullptr,SW_SHOWNORMAL);break;
    case 4:ShellExecuteW(nullptr,L"open",L"windowsdefender://coreisolation",nullptr,nullptr,SW_SHOWNORMAL);break;
    case 8:ShellExecuteW(nullptr,L"open",L"ms-settings:display-advanced",nullptr,nullptr,SW_SHOWNORMAL);break;
    case 9:ShellExecuteW(nullptr,L"open",L"ms-settings:windowsupdate",nullptr,nullptr,SW_SHOWNORMAL);break;
    case 5:tuneSetTab(4);break;case 6:tuneSetTab(2);break;case 7:tuneSetTab(0);break;
    case 20:tProcs.clear();tuneLoadProcs();tuneShimmer();break;
    case 21:if(arg<(int)tProcs.size()){auto p=tProcs[arg];wchar_t q[400];swprintf(q,400,i18n::text(L"%ls 프로그램에 창 닫기를 요청할까요? 저장하지 않은 작업이 있으면 그 프로그램이 물어봅니다."),p.name.c_str());if(!ask(q))break;
        int n=0;for(DWORD pid:p.pids)n+=perfboost::closeWindows(pid);tuneSay(n?i18n::text(L"닫기를 요청했습니다."):i18n::text(L"닫을 창을 찾지 못했습니다."),!n);tProcs.clear();tuneLoadProcs();tuneShimmer();}break;
    case 30:restartAsAdmin();break;case 31:toggleStartup(arg);break;
    case 40:if(arg<(int)tSpots.size()){auto id=tSpots[arg].id;if(tPick.count(id))tPick.erase(id);else tPick.insert(id);InvalidateRect(page,nullptr,FALSE);}break;
    case 41:startClean();break;case 50:undo(arg);break;}}
// Messages, mouse and wheel for view 6 (dlss_ui.h proc calls this first; true = handled).
bool tuneHandle(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_APP+210){std::unique_ptr<DiagData> d((DiagData*)lp);tMon=d->mon;tPlans=d->plans;tGpu=d->gpu;tHvci=d->hvci;tBattery=d->battery;tFreeGB=d->freeGB;tFreePct=d->freePct;tDiagReady=true;tDiagLoading=false;InvalidateRect(h,nullptr,FALSE);return true;}
    if(msg==WM_APP+211){std::unique_ptr<std::vector<wintune::Proc>> v((std::vector<wintune::Proc>*)lp);tProcs=*v;tProcLoading=false;InvalidateRect(h,nullptr,FALSE);return true;}
    if(msg==WM_APP+212){std::unique_ptr<std::vector<wintune::Startup>> v((std::vector<wintune::Startup>*)lp);tStart=*v;tStartLoading=false;tStartReady=true;InvalidateRect(h,nullptr,FALSE);return true;}
    if(msg==WM_APP+213){std::unique_ptr<std::vector<wintune::Spot>> v((std::vector<wintune::Spot>*)lp);tSpots=*v;tSpotLoading=false;tSpotReady=true;InvalidateRect(h,nullptr,FALSE);return true;}
    if(msg==WM_APP+214){std::unique_ptr<wintune::Swept> r((wintune::Swept*)lp);tCleaning=false;wchar_t b[300];swprintf(b,300,i18n::text(L"파일 %d개 (%ls)를 정리했습니다. 사용 중인 %d개는 건너뛰었습니다."),r->files,sizeText(r->bytes).c_str(),r->skipped);
        wchar_t l[200];swprintf(l,200,L"%ls: %ls · %d",i18n::text(L"정리"),sizeText(r->bytes).c_str(),r->files);tuneHistory().add({L"",L"",L"clean",L"",L"",std::to_wstring(r->bytes),l,L"done"});tuneSay(b);tSpotReady=false;tuneLoadSpots();tuneShimmer();return true;}
    if(msg==WM_APP+215){std::unique_ptr<HzJob> j((HzJob*)lp);auto hs=tuneHistory();if(wp){hs.mark(j->id,L"applied");tuneSay(i18n::text(L"주사율을 바꿨습니다."));}else{hs.drop(j->id);tuneSay(i18n::text(L"이 모니터가 그 주사율을 받지 않아 바꾸지 않았습니다."),true);}tDiagReady=false;tuneLoadDiag();return true;}
    if(msg==WM_TIMER&&wp==14){if(view!=6||!tuneLoading())KillTimer(h,14);RECT R=tunePanel();InvalidateRect(h,&R,FALSE);return true;}
    if(view!=6||tuneTab==0)return false;
    auto at=[&](LPARAM l){POINT p={(int)(short)LOWORD(l),(int)(short)HIWORD(l)};for(int i=(int)tuneHits.size()-1;i>=0;i--)if(PtInRect(&tuneHits[i].r,p))return i;return -1;};
    if(msg==WM_MOUSEMOVE){int i=at(lp);if(i!=tuneHover){tuneHover=i;RECT R=tunePanel();InvalidateRect(h,&R,FALSE);TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}return true;}
    if(msg==WM_MOUSELEAVE){if(tuneHover>=0){tuneHover=-1;RECT R=tunePanel();InvalidateRect(h,&R,FALSE);}return true;}
    if(msg==WM_SETCURSOR&&LOWORD(lp)==HTCLIENT){POINT c;GetCursorPos(&c);ScreenToClient(h,&c);for(auto& t:tuneHits)if(t.on&&PtInRect(&t.r,c)){SetCursor(LoadCursor(nullptr,IDC_HAND));return true;}return false;}
    if(msg==WM_LBUTTONDOWN){int i=at(lp);if(i>=0&&tuneHits[i].on){auto t=tuneHits[i];tuneAct(t.act,t.arg);}return true;}
    if(msg==WM_MOUSEWHEEL){RECT R=tunePanel();int st=tuneTab==2?44:tuneTab==3?50:52,over=tuneContent[tuneTab]-tuneView[tuneTab],mx=over>0?(over+st-1)/st*st:0;int s=tuneScroll[tuneTab]-GET_WHEEL_DELTA_WPARAM(wp)/120*st;s=std::clamp(s,0,mx);
        if(s!=tuneScroll[tuneTab]){tuneScroll[tuneTab]=s;tuneHover=-1;InvalidateRect(h,&R,FALSE);}return true;}
    return false;}
