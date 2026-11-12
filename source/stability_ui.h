#pragma once
// PC 안정성 (page 17, Stability Guard): sudden power-offs, blue screens, graphics driver timeouts, hardware (WHEA) and disk
// errors and game crashes found in the Windows logs of the last 30 days, each explained in plain words (what happened, what
// is suspected, how sure, what to try first) with a technical view of the very same records (event IDs with providers,
// bug check / exception codes, modules, WHEA data, the raw message and XML) for searching or asking someone. Local only.
#include "stability_collect.h"
void openPage(WNDPROC callback,const wchar_t* className,int page); // ModeSwitch.cpp, defined after the feature headers
namespace stabui {
using namespace stability;
HWND page=nullptr;std::vector<int> view,monthKeys;int monthSel=0;int drag=0,dragY0=0,dragTop0=0; // view: incidents of the chosen month (newest first); drag: 1 list bar, 2 technical bar
Time wantTime=0; // set by the alert's 원인 분석: select the incident nearest to it
std::wstring techBody;std::vector<std::wstring> techLines;int techTop=0,techWidth=0;std::vector<Incident> incidents;int sel=0,listTop=0,tab=0;bool loading=false,tech=false,denied=false;std::wstring collectError,gpuName,gpuDriver,status;
std::vector<stabcollect::Row> rows;HFONT mono=nullptr;
const COLORREF amber=RGB(255,184,77);
COLORREF severityColor(int s){return s>=2?danger:s==1?amber:blue;}
const wchar_t* levelWord(Level l){return l==Level::High?L"높음":l==Level::Medium?L"보통":l==Level::Low?L"낮음":L"판단 불가";}
std::wstring when(Time t){if(!t)return i18n::text(L"기록 없음");FILETIME f={(DWORD)t,(DWORD)(t>>32)},l;SYSTEMTIME s;FileTimeToLocalFileTime(&f,&l);FileTimeToSystemTime(&l,&s);
    wchar_t d[64]={},h[64]={};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,DATE_SHORTDATE,&s,nullptr,d,64,nullptr);GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT,TIME_NOSECONDS,&s,nullptr,h,64);return std::wstring(d)+L" "+h;}
const Incident* current(){return sel>=0&&sel<(int)incidents.size()?&incidents[sel]:nullptr;}
// ---- What the PC was doing: the last reading at or before the incident (within 15 minutes), and BlueAsh's own changes
// in the 72 hours before it.
const stabcollect::Row* before(const Incident& c){const stabcollect::Row* best=nullptr;for(auto& r:rows)if(r.time<=c.time+minute&&r.time+15*minute>=c.time)best=&r;return best;}
std::vector<stabcollect::Change> changesBefore(const Incident& c){std::vector<stabcollect::Change> out;Time from=c.time>72*60*minute?c.time-72*60*minute:0;
    for(auto& e:dlssui::tuneHistory().load()){Time t=stabcollect::localText(e.when);if(t&&t>=from&&t<=c.time+minute&&(e.state==L"applied"||e.state==L"done"))out.push_back({t,std::wstring(i18n::text(L"윈도우 최적화"))+L" · "+e.label,88});}
    WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((dlssui::root()+L"\\*").c_str(),&f);
    if(h!=INVALID_HANDLE_VALUE){do{if(!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||f.cFileName[0]==L'.')continue;std::wstring dir=dlssui::root()+L"\\"+f.cFileName,name=f.cFileName;
            for(auto& g:dlssui::games)if(dlssui::keyOf(g)==name)name=g.e.name;
            for(const wchar_t* file:{L"\\applied.txt",L"\\written.txt"}){Time t=stabcollect::fileTime(dir+file);if(t&&t>=from&&t<=c.time+minute){out.push_back({t,std::wstring(i18n::text(L"그래픽 설정"))+L" · DLSS 5 · "+name,86});break;}}}while(FindNextFileW(h,&f));FindClose(h);}
    std::sort(out.begin(),out.end(),[](auto& a,auto& b){return a.time>b.time;});return out;}

std::wstring programAt(const Incident& c,const std::vector<stabcollect::Row>& readings);
// ---- Technical text, drawn by the page itself in a monospace font and scrolled with the wheel (a system edit box
// painted outside its own area under the display scaling; copying goes through the copy and export buttons). Built from the same incident as the easy view; English field names as Windows uses them.
std::wstring crlf(std::wstring s){std::wstring o;for(size_t i=0;i<s.size();i++){if(s[i]==L'\n'&&(i==0||s[i-1]!=L'\r'))o+=L'\r';o+=s[i];}return o;}
std::wstring pretty(const std::wstring& xml){std::wstring o;for(size_t i=0;i<xml.size();i++){o+=xml[i];if(xml[i]==L'>'&&i+1<xml.size()&&xml[i+1]==L'<')o+=L"\r\n";}return o;}
std::wstring levelName(int l){return l==1?L"Critical":l==2?L"Error":l==3?L"Warning":l==4||l==0?L"Information":l==5?L"Verbose":std::to_wstring(l);}
std::wstring kindName(Kind k){switch(k){case Kind::PowerLoss:return L"Unexpected shutdown (Kernel-Power 41)";case Kind::BugCheck:return L"Bug check (BSOD)";case Kind::GpuTimeout:return L"GPU driver timeout (TDR)";
    case Kind::Hardware:return L"WHEA hardware error";case Kind::Disk:return L"Storage error";case Kind::GameCrash:return L"Application crash (game)";case Kind::GameHang:return L"Application hang (game)";case Kind::BlueAshCrash:return L"Application crash (BlueAsh)";default:return L"-";}}
std::wstring windowsBuild(){typedef LONG(WINAPI*Rtl)(OSVERSIONINFOW*);OSVERSIONINFOW v={};v.dwOSVersionInfoSize=sizeof(v);auto f=(Rtl)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion");if(f)f(&v);
    return L"Windows "+std::to_wstring(v.dwMajorVersion)+L"."+std::to_wstring(v.dwMinorVersion)+L" build "+std::to_wstring(v.dwBuildNumber);}
std::wstring field(const std::wstring& k,const std::wstring& v){return v.empty()?L"":k+L": "+v+L"\r\n";}
std::wstring eventFields(const Event& e,bool full){std::wstring s=field(L"Event Provider",e.provider)+field(L"Event ID",std::to_wstring(e.id))+field(L"Log Channel",e.channel)+field(L"Event Record ID",e.record?std::to_wstring(e.record):L"")
    +field(L"Level",levelName(e.level))+field(L"Event Timestamp",when(e.time));
    if(isLive(e)){s+=field(L"WER Event Name",L"LiveKernelEvent")+field(L"WER Problem Code (P1)",e.get(L"P1")+(stopName(liveCode(e)).empty()?L"":L" ("+stopName(liveCode(e))+L")"));for(int i=2;i<=4;i++)s+=field(L"P"+std::to_wstring(i),e.get((L"P"+std::to_wstring(i)).c_str()));
        s+=field(L"Attached Files",e.get(L"AttachedFiles"))+field(L"Report ID",e.get(L"ReportId"));}
    else if(full)for(auto& d:e.data){if(d.second.empty())continue;std::wstring v=d.second;if(v.size()>160)v=v.substr(0,160)+L"…";s+=field(L"  "+d.first,v);}
    return s;}
std::wstring techText(const Incident& c,int which){std::wstring s;
    if(which==0){s+=field(L"Error Type",kindName(c.kind))+field(L"Severity",c.severity>=2?L"High":c.severity==1?L"Medium":L"Low")+field(L"Occurred (estimated)",when(c.time));if(c.boot)s+=field(L"Restart record time",when(c.boot));
        s+=field(L"Windows Boot Time (this session)",when(stabcollect::bootTime()))+field(L"Windows Version",windowsBuild())+field(L"Faulting Process",c.process);
        s+=field(L"Program in use (BlueAsh estimate)",programAt(c,rows));
        s+=L"\r\n[Identifiers recorded by Windows]\r\n";for(auto& k:c.codes)s+=k.label+L": "+k.value+L"\r\n";
        s+=L"\r\n[Search text]\r\n";for(auto& q:c.searches)s+=q+L"\r\n";
        s+=L"\r\n"+std::wstring(i18n::text(L"기술 정보는 Windows 기록을 그대로 옮긴 것입니다. 기록에 없는 항목은 표시하지 않습니다."))+L"\r\n";}
    else if(which==1){for(auto& e:c.events){s+=L"[ "+e.provider+L" · "+std::to_wstring(e.id)+L" ]\r\n"+eventFields(e,true)+L"\r\n";}
        s+=std::wstring(i18n::text(L"이벤트 뷰어"))+L": "+std::wstring(i18n::text(L"Windows 로그 > 시스템(또는 응용 프로그램)에서 '현재 로그 필터링'에 원본 이름과 이벤트 ID를 넣으면 같은 기록을 찾을 수 있습니다."))+L"\r\n";}
    else if(which==2){s+=field(L"GPU Model",gpuName.empty()?std::wstring(i18n::text(L"측정 불가")):gpuName)+field(L"GPU Driver Version",gpuDriver.empty()?std::wstring(i18n::text(L"측정 불가")):gpuDriver)+L"\r\n";
        for(auto& e:c.events){Kind k=kindOf(e);
            if(k==Kind::BugCheck){unsigned long b=bugCode(e);s+=L"[BugCheck]\r\n"+field(L"BugCheck Code",hex(b))+field(L"Stop Code Name",stopName(b))+field(L"BugCheck Parameters",e.get(L"param1"))+field(L"Dump File Path",e.get(L"param2"))+field(L"Report ID",e.get(L"param3"))
                +L"Dump analysis: not performed\r\n\r\n";}
            if(k==Kind::GameCrash||k==Kind::BlueAshCrash){s+=L"[Application Error 1000]\r\n"+field(L"Faulting Application Name",e.get(L"AppName"))+field(L"Faulting Application Path",e.get(L"AppPath"))+field(L"Application Version",e.get(L"AppVersion"))
                +field(L"Faulting Module Name",e.get(L"ModuleName"))+field(L"Faulting Module Path",e.get(L"ModulePath"))+field(L"Module Version",e.get(L"ModuleVersion"))+field(L"Exception Code",e.get(L"ExceptionCode").empty()?L"":L"0x"+e.get(L"ExceptionCode"))
                +field(L"Fault Offset",e.get(L"FaultingOffset").empty()?L"":L"0x"+e.get(L"FaultingOffset"))+field(L"Process ID",e.get(L"ProcessId"))+field(L"Report ID",e.get(L"IntegratorReportId"))+L"\r\n";}
            if(k==Kind::GameHang){s+=L"[Application Hang 1002]\r\n"+field(L"Application",e.get(L"AppName"))+field(L"Path",e.get(L"ExeFileName"))+field(L"Version",e.get(L"AppVersion"))+field(L"Hang Type",e.get(L"HangType"))+field(L"Report ID",e.get(L"ReportId"))+L"\r\n";}
            if(k==Kind::GpuTimeout){s+=L"[Display / GPU driver]\r\n"+eventFields(e,true)+L"\r\n";}}
        if(s.find(L"[")==std::wstring::npos)s+=std::wstring(i18n::text(L"이 사고에는 드라이버·모듈 기록이 없습니다."))+L"\r\n";}
    else if(which==3){bool any=false;for(auto& e:c.events)if(kindOf(e)==Kind::Hardware||kindOf(e)==Kind::Disk){any=true;s+=L"[ "+e.provider+L" · "+std::to_wstring(e.id)+L" ]\r\n"+eventFields(e,true)+L"\r\n";}
        if(!any)s+=std::wstring(i18n::text(L"이 사고에는 WHEA·저장 장치 오류 기록이 없습니다."))+L"\r\n\r\n";
        s+=L"[Readings before the error (BlueAsh, every 10 s)]\r\n";bool got=false;for(auto& r:rows)if(r.time<=c.time+minute&&r.time+5*minute>=c.time){got=true;wchar_t b[300];
            auto v=[](int x,const wchar_t* u){return x<0?std::wstring(L"n/a"):std::to_wstring(x)+u;};
            swprintf(b,300,L"%ls  CPU %ls · RAM %ls · GPU %ls · %ls · %ls · %ls · VRAM %ls · %ls\r\n",when(r.time).c_str(),v(r.cpu,L"%").c_str(),v(r.ram,L"%").c_str(),v(r.gpu,L"%").c_str(),v(r.temp,L"°C").c_str(),v(r.watts,L" W (GPU only)").c_str(),v(r.mhz,L" MHz").c_str(),
                r.vram<0?L"n/a":(std::to_wstring((int)(r.vram*10)/10.0).substr(0,4)+L" GB").c_str(),r.exe.c_str());s+=b;}
        if(!got)s+=std::wstring(i18n::text(L"오류 직전 측정 기록 없음"))+L"\r\n";else if(auto r=before(c))s+=field(L"Last reading",when(r->time));}
    else if(which==4){s+=std::wstring(i18n::text(L"문제 종류 확신"))+L": "+i18n::text(levelWord(c.kindSure))+L" · "+i18n::text(L"원인 확신")+L": "+i18n::text(levelWord(c.causeSure))+L"\r\n\r\n";
        for(auto& k:c.causes){s+=std::wstring(i18n::text(L"의심되는 원인"))+L": "+i18n::text(k.title)+L"\r\n";for(auto& x:k.evidence)s+=L"  + "+std::wstring(i18n::text(x))+L"\r\n";for(auto& x:k.against)s+=L"  − "+std::wstring(i18n::text(x))+L"\r\n";for(auto& x:k.checks)s+=L"  → "+std::wstring(i18n::text(x))+L"\r\n";s+=L"\r\n";}
        auto ch=changesBefore(c);s+=std::wstring(i18n::text(L"사고 전 72시간 동안 BlueAsh에서 바꾼 설정"))+L":\r\n";if(ch.empty())s+=L"  "+std::wstring(i18n::text(L"없음"))+L"\r\n";for(auto& x:ch)s+=L"  "+when(x.time)+L"  "+x.label+L"\r\n";
        s+=L"\r\n"+std::wstring(i18n::text(L"+ 근거  − 반대 근거·확인되지 않은 정보  → 추가로 필요한 확인"))+L"\r\n"+std::wstring(i18n::text(L"신뢰도는 근거의 수준을 나타내는 분류이며 확률이 아닙니다."))+L"\r\n";}
    else{for(auto& e:c.events){s+=L"[ "+e.provider+L" · "+std::to_wstring(e.id)+L" · "+when(e.time)+L" ]\r\n"+std::wstring(i18n::text(L"원본 이벤트 메시지"))+L":\r\n"+(e.message.empty()?std::wstring(i18n::text(L"기록 없음")):crlf(e.message))+L"\r\n\r\nEvent XML:\r\n"+(e.xml.empty()?std::wstring(i18n::text(L"기록 없음")):pretty(e.xml))+L"\r\n\r\n";}}
    return s;}
// What leaves the PC (copy, export) never carries the profile path, the account or the PC name.
std::wstring safe(const std::wstring& s){return scrub(s,wintune::env(L"USERPROFILE"),wintune::env(L"USERNAME"),wintune::env(L"COMPUTERNAME"));}
bool copyText(const std::wstring& t){if(t.empty()||!OpenClipboard(page))return false;EmptyClipboard();HGLOBAL g=GlobalAlloc(GMEM_MOVEABLE,(t.size()+1)*sizeof(wchar_t));bool ok=false;
    if(g){memcpy(GlobalLock(g),t.c_str(),(t.size()+1)*sizeof(wchar_t));GlobalUnlock(g);ok=SetClipboardData(CF_UNICODETEXT,g)!=nullptr;if(!ok)GlobalFree(g);}CloseClipboard();return ok;}
std::wstring urlEncode(const std::wstring& w){int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string u(n,'\0');WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&u[0],n,nullptr,nullptr);
    std::wstring o;for(unsigned char ch:u){if(isalnum(ch)||ch=='-'||ch=='_'||ch=='.'||ch=='~')o+=(wchar_t)ch;else if(ch==' ')o+=L'+';else{wchar_t b[4];swprintf(b,4,L"%%%02X",ch);o+=b;}}return o;}
// Microsoft Learn pages checked to exist (2026-11-09); a kind without a matching page gets no link.
const wchar_t* docUrl(const Incident& c){bool gpu=first(c,Kind::GpuTimeout)!=nullptr;switch(c.kind){
    case Kind::BugCheck:return L"https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/bug-check-code-reference2";
    case Kind::GpuTimeout:return L"https://learn.microsoft.com/en-us/windows-hardware/drivers/display/timeout-detection-and-recovery";
    case Kind::PowerLoss:return gpu?L"https://learn.microsoft.com/en-us/windows-hardware/drivers/display/timeout-detection-and-recovery":L"https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/event-id-41-restart";
    case Kind::Hardware:return L"https://learn.microsoft.com/en-us/windows-hardware/drivers/whea/";
    case Kind::GameCrash:case Kind::GameHang:case Kind::BlueAshCrash:return L"https://learn.microsoft.com/en-us/windows/win32/wer/windows-error-reporting";
    default:return nullptr;}}

// ---- Layout: laid out on the page's real size (cw x ch, design pixels). The list keeps its
// width; the detail, the technical box and the button rows follow the page.
int cw=852,ch=622;const int listL=36,listR=284,rowH=78,detL=304;int listT=80; /* under the month chips (one or two rows) */int listB(){return ch-82;}int detR(){return cw-36;}
int visibleRows(){return (listB()-listT)/rowH;}
RECT rowRect(int i){int y=listT+(i-listTop)*rowH;return {listL,y,listR,y+rowH-8};}
// ---- Months: the list shows one month at a time (chips above it, newest month first), so a long history stays tidy.
int monthOf(Time t){FILETIME f={(DWORD)t,(DWORD)(t>>32)},l;SYSTEMTIME s;FileTimeToLocalFileTime(&f,&l);FileTimeToSystemTime(&l,&s);return s.wYear*12+(s.wMonth-1);}
int monthCount(int k){int n=0;for(auto& c:incidents)if(monthOf(c.time)==k)n++;return n;}
void buildView(){view.clear();if(monthKeys.empty())return;int k=monthKeys[std::clamp(monthSel,0,(int)monthKeys.size()-1)];for(int i=0;i<(int)incidents.size();i++)if(monthOf(incidents[i].time)==k)view.push_back(i);}
// One month at a time in a single bar: ‹ 2026년 11월 · 7건 › (‹ older, › newer), so it stays one line however long the history.
// chips[0] = ‹, chips[1] = › (only the arrows are clickable).
std::vector<RECT> chips;
void layoutChips(){chips={RECT{listL,36,listL+38,72},RECT{listR-38,36,listR,72}};listT=86;}
std::wstring monthTitle(int k){SYSTEMTIME s={};s.wYear=(WORD)(k/12);s.wMonth=(WORD)(k%12+1);s.wDay=1;wchar_t b[64]={};GetDateFormatEx(LOCALE_NAME_USER_DEFAULT,DATE_YEARMONTH,&s,nullptr,b,64,nullptr);
    wchar_t n[48];swprintf(n,48,i18n::text(L"%d건"),monthCount(k));return std::wstring(b)+L" · "+n;}
std::vector<RECT> chipRects(HDC){return chips;}
// The program in use when it happened: the crashed game itself, a game crash of this list within 10 minutes, else the
// foreground program BlueAsh read last before it. "" when none of these is known.
std::wstring programAt(const Incident& c,const std::vector<stabcollect::Row>& readings){if(!c.process.empty())return c.process;
    for(auto& o:incidents)if((o.kind==Kind::GameCrash||o.kind==Kind::GameHang)&&!o.process.empty()&&o.time+10*minute>=c.time&&o.time<=c.time+10*minute)return o.process;
    const stabcollect::Row* best=nullptr;for(auto& r:readings)if(r.time<=c.time+minute&&r.time+15*minute>=c.time)best=&r;return best?best->exe:L"";}
RECT tabRect(int i){int w=(detR()-listL-10*5)/6;int x=listL+i*(w+10);return {x,36,x+w,72};}
const wchar_t* tabName(int i){const wchar_t* n[]={L"오류 개요",L"Windows 이벤트",L"드라이버 및 모듈",L"하드웨어 기록",L"분석 근거",L"원본 로그"};return n[i];}
RECT statusRect(){return tech?RECT{200,ch-122,cw-36,ch-78}:RECT{36,ch-110,cw-36,ch-78};}
void setStatus(const std::wstring& s){status=s;RECT r=statusRect();InvalidateRect(page,&r,FALSE);}
void showButtons(){
    for(int id:{501,502,509}){ShowWindow(GetDlgItem(page,id),!tech&&!loading?SW_SHOW:SW_HIDE);}ShowWindow(GetDlgItem(page,503),!tech?SW_SHOW:SW_HIDE);
    for(int id:{504,505,506,507,508,510,511}){ShowWindow(GetDlgItem(page,id),tech?SW_SHOW:SW_HIDE);}
    EnableWindow(GetDlgItem(page,501),current()!=nullptr);if(auto c=current())EnableWindow(GetDlgItem(page,507),docUrl(*c)!=nullptr);
    if(tech&&current()){techBody=techText(*current(),tab);techTop=0;techWidth=0;}InvalidateRect(page,nullptr,FALSE);}
void place(){RECT r;GetClientRect(page,&r);cw=std::max(852L,r.right);ch=std::max(622L,r.bottom);int y=ch-66;
    auto mv=[&](int id,int x,int w,int yy){HWND c=GetDlgItem(page,id);if(!c)return;RECT b;GetClientRect(c,&b);SetWindowPos(c,nullptr,x,yy,w,b.bottom,SWP_NOZORDER|SWP_NOACTIVATE);};
    mv(501,36,180,y);mv(502,226,130,y);mv(509,366,170,y);mv(503,cw-156,120,y);mv(511,36,150,ch-122);
    int w=(cw-72-50)/6;const int ids[]={504,505,506,507,510,508};for(int i=0;i<6;i++){mv(ids[i],36+i*(w+10),w,y);}
    techWidth=0;InvalidateRect(page,nullptr,FALSE);}
void scan();
void loaded(stabcollect::Result* r){loading=false;denied=r->denied;collectError=r->error;rows=stabcollect::readings(&gpuName,&gpuDriver);
    if(gpuName.empty()){dlssgpu::Nvml n;auto g=n.read();if(g.ok){gpuName=std::wstring(g.name.begin(),g.name.end());gpuDriver=std::wstring(g.driver.begin(),g.driver.end());}}
    incidents=build(r->events,stabcollect::utcOffset());sel=incidents.empty()?-1:0;listTop=0;monthSel=0;
    monthKeys.clear();for(auto& c:incidents){int k=monthOf(c.time);if(std::find(monthKeys.begin(),monthKeys.end(),k)==monthKeys.end())monthKeys.push_back(k);}
    if(wantTime){long long best=-1;for(int i=0;i<(int)incidents.size();i++){long long d=(long long)incidents[i].time-(long long)wantTime;if(d<0){d=-d;}if(d<=60*(long long)minute&&(best<0||d<best)){best=d;sel=i;}}wantTime=0;}
    layoutChips();if(sel>=0){for(int m=0;m<(int)monthKeys.size();m++)if(monthKeys[m]==monthOf(incidents[sel].time))monthSel=m;}buildView();
    {int at=(int)(std::find(view.begin(),view.end(),sel)-view.begin());listTop=std::max(0,at-visibleRows()+1);}
    if(!incidents.empty())for(auto& c:incidents)if(c.boot){saveString(configPath,L"StabilitySeen",std::to_wstring(c.boot));break;}
    showButtons();}
// Each scan has a number; a result from an older one (the page was left and opened again meanwhile) is dropped.
int scanGen=0;
void scan(){if(loading)return;loading=true;tech=false;status.clear();showButtons();
    struct Job{HWND page;int gen;};auto* j=new Job{page,++scanGen};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(Job*)p;auto* r=new stabcollect::Result(stabcollect::collect(90));if(!PostMessageW(j->page,WM_APP+231,(WPARAM)j->gen,(LPARAM)r))delete r;delete j;return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{delete j;loading=false;showButtons();}}

RECT techBox(){return {36,84,cw-36,ch-132};}const int techLineH=20;
int techVisible(){RECT r=techBox();return (r.bottom-r.top-20)/techLineH;}
void wrapTech(HDC dc,int width){techLines.clear();auto o=SelectObject(dc,mono);size_t a=0;
    while(a<=techBody.size()){size_t e=techBody.find(L'\n',a);if(e==std::wstring::npos)e=techBody.size();std::wstring ln=techBody.substr(a,e-a);if(!ln.empty()&&ln.back()==L'\r')ln.pop_back();a=e+1;
        do{int fit=0;SIZE z{};GetTextExtentExPointW(dc,ln.c_str(),(int)ln.size(),width,&fit,nullptr,&z);if(fit<=0)fit=1;if(fit>=(int)ln.size()){techLines.push_back(ln);break;}techLines.push_back(ln.substr(0,fit));ln=ln.substr(fit);}while(!ln.empty());
        if(e>=techBody.size())break;}
    SelectObject(dc,o);techWidth=width;}
bool techBar(RECT& track,RECT& thumb){RECT r=techBox();int n=(int)techLines.size(),vis=techVisible();if(n<=vis)return false;track={r.right-16,r.top+10,r.right-2,r.bottom-10};int h=track.bottom-track.top,bar=std::max(30,h*vis/n),y=track.top+(h-bar)*techTop/std::max(1,n-vis);thumb={r.right-13,y,r.right-6,y+bar};return true;}
void paintTech(HDC dc){RECT r=techBox();roundBox(dc,r,well,line,14);int w=r.right-r.left-40;if(techWidth!=w)wrapTech(dc,w);
    int n=(int)techLines.size(),vis=techVisible();techTop=std::clamp(techTop,0,std::max(0,n-vis));int saved=SaveDC(dc);IntersectClipRect(dc,r.left+12,r.top+10,r.right-20,r.bottom-10);
    auto o=SelectObject(dc,mono);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,mint);for(int i=0;i<vis&&techTop+i<n;i++){const std::wstring& t=techLines[techTop+i];TextOutW(dc,r.left+16,r.top+10+i*techLineH,t.c_str(),(int)t.size());}
    SelectObject(dc,o);RestoreDC(dc,saved);
    RECT tr,th;if(techBar(tr,th))roundBox(dc,th,drag==2?blue:RGB(64,92,150),drag==2?blue:RGB(64,92,150),6);}
// ---- Painting.
void pill(HDC dc,int x,int y,const std::wstring& text,COLORREF c){HFONT f=captionFont;auto o=SelectObject(dc,f);SIZE z{};GetTextExtentPoint32W(dc,text.c_str(),(int)text.size(),&z);SelectObject(dc,o);
    RECT r={x,y,x+z.cx+24,y+26};roundBox(dc,r,painting::mixColor(panel,c,.16),painting::mixColor(panel,c,.55),26);textAt(dc,text,r,f,c,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
int pillWidth(HDC dc,const std::wstring& text){auto o=SelectObject(dc,captionFont);SIZE z{};GetTextExtentPoint32W(dc,text.c_str(),(int)text.size(),&z);SelectObject(dc,o);return z.cx+24;}
bool listBar(RECT& track,RECT& thumb){int n=(int)view.size(),vis=visibleRows();if(n<=vis)return false;track={listR+2,listT,listR+16,listT+vis*rowH-8};int h=track.bottom-track.top,bar=std::max(30,h*vis/n),y=track.top+(h-bar)*listTop/std::max(1,n-vis);thumb={listR+5,y,listR+12,y+bar};return true;}
void paintList(HDC dc){if(!monthKeys.empty()){RECT bar={listL,36,listR,72};roundBox(dc,bar,rowFill,line,18);
        textAt(dc,monthTitle(monthKeys[monthSel]),{listL+40,36,listR-40,72},smallBold,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        bool older=monthSel+1<(int)monthKeys.size(),newer=monthSel>0;glyph(dc,0xE76B,chips[0],iconFont,older?mint:line);glyph(dc,0xE76C,chips[1],iconFont,newer?mint:line);}
    for(int i=listTop;i<(int)view.size()&&i<listTop+visibleRows();i++){auto& c=incidents[view[i]];RECT r=rowRect(i);bool on=view[i]==sel;
        roundBox(dc,r,on?tint:rowFill,on?blue:line,14);roundBox(dc,{r.left+14,r.top+15,r.left+24,r.top+25},severityColor(c.severity),severityColor(c.severity),10);
        textAt(dc,c.title,{r.left+34,r.top+8,r.right-12,r.top+48},smallBold,ink,DT_LEFT|DT_WORDBREAK); /* two lines: the titles are whole sentences */
        textAt(dc,when(c.time),{r.left+34,r.top+48,r.right-12,r.bottom-6},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);}
    RECT tr,th;if(listBar(tr,th))roundBox(dc,th,drag==1?blue:RGB(64,92,150),drag==1?blue:RGB(64,92,150),6);}
// Height of wrapped text in a width (what textAt draws with DT_WORDBREAK).
int textHeight(HDC dc,const std::wstring& t,HFONT f,int width){std::wstring s=i18n::text(t);auto o=SelectObject(dc,f);std::wstring w=wrapWords(dc,s,width);RECT m={0,0,width,0};DrawTextW(dc,w.c_str(),-1,&m,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,o);return m.bottom;}
void paintDetail(HDC dc,const Incident& c){int x=detL,w=detR();
    textAt(dc,c.title,{x,36,w,92},sectionFont,ink,DT_LEFT|DT_WORDBREAK);
    std::wstring t=std::wstring(i18n::text(L"발생"))+L" "+when(c.time);if(c.boot&&c.boot!=c.time)t+=L" · "+std::wstring(i18n::text(L"다시 켜짐"))+L" "+when(c.boot);if(c.events.size()>1){wchar_t n[80];swprintf(n,80,i18n::text(L"기록 %d개"),(int)c.events.size());t+=std::wstring(L" · ")+n;}
    textAt(dc,t,{x,94,w,116},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    std::wstring a=std::wstring(i18n::text(L"문제 종류 확신"))+L" · "+i18n::text(levelWord(c.kindSure)),b=std::wstring(i18n::text(L"원인 확신"))+L" · "+i18n::text(levelWord(c.causeSure));
    pill(dc,x,124,a,severityColor(c.severity));pill(dc,x+pillWidth(dc,a)+8,124,b,c.causeSure==Level::High?RGB(110,220,150):c.causeSure==Level::Medium?amber:muted);
    textAt(dc,c.summary,{x,162,w,232},font,mint,DT_LEFT|DT_WORDBREAK);
    // Two cards side by side: what is suspected, and what to try first. Under them one card for the context.
    int gap=14,cwid=(w-x-gap)/2,top=240,pad=18;RECT a1={x,top,x+cwid,0},a2={x+cwid+gap,top,w,0};
    auto measure=[&](bool steps){int hh=pad+30;size_t n=steps?c.steps.size():c.causes.size();for(size_t i=0;i<n&&i<3;i++)hh+=textHeight(dc,steps?std::wstring(c.steps[i]):std::wstring(c.causes[i].title),smallFont,cwid-2*pad-22)+10;return hh+pad-4;};
    int ch2=std::max(measure(false),measure(true));a1.bottom=a2.bottom=top+ch2;
    for(int k=0;k<2;k++){RECT q=k?a2:a1;roundBox(dc,q,rowFill,line,16);bool steps=k==1;COLORREF accent=steps?blue:amber;
        glyph(dc,steps?0xE73E:0xE7BA,{q.left+pad-2,q.top+pad-4,q.left+pad+20,q.top+pad+20},iconFont,accent);
        textAt(dc,steps?L"먼저 해 볼 것":L"의심되는 원인",{q.left+pad+28,q.top+pad-4,q.right-pad,q.top+pad+20},smallBold,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        int yy=q.top+pad+30;size_t n=steps?c.steps.size():c.causes.size();
        for(size_t i=0;i<n&&i<3;i++){std::wstring t=steps?std::wstring(c.steps[i]):std::wstring(c.causes[i].title);int hh=textHeight(dc,t,smallFont,cwid-2*pad-22);
            if(steps)textAt(dc,std::to_wstring(i+1)+L".",{q.left+pad,yy,q.left+pad+20,yy+20},smallBold,blue,DT_LEFT|DT_TOP|DT_SINGLELINE);else roundBox(dc,{q.left+pad+4,yy+7,q.left+pad+10,yy+13},amber,amber,6);
            textAt(dc,t,{q.left+pad+22,yy,q.right-pad,yy+hh+2},smallFont,mint,DT_LEFT|DT_WORDBREAK);yy+=hh+10;}}
    // Context: the program in use, the readings just before, BlueAsh's own recent change (the graphics or optimization page has the undo).
    int y=top+ch2+14;RECT ctx={x,y,w,y+96};roundBox(dc,ctx,rowFill,line,16);int lx=x+pad,ly=y+14;
    std::wstring prog=programAt(c,rows);textAt(dc,std::wstring(i18n::text(L"사용 중이던 프로그램"))+L": "+(prog.empty()?std::wstring(i18n::text(L"기록에서 확인되지 않음")):prog),{lx,ly,w-pad,ly+22},smallBold,prog.empty()?muted:ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    std::wstring s;if(auto r=before(c)){wchar_t b[200];auto v=[](int n,const wchar_t* u){return n<0?std::wstring(i18n::text(L"측정 불가")):std::to_wstring(n)+u;};
        swprintf(b,200,L"%ls %ls · %ls · %ls",i18n::text(L"직전 그래픽카드"),v(r->gpu,L"%").c_str(),v(r->temp,L"°C").c_str(),v(r->watts,L"W").c_str());s=b;s+=L" ("+std::wstring(i18n::text(L"마지막 측정"))+L" "+when(r->time)+L")";}
    else s=i18n::text(L"오류 직전 측정 기록 없음");
    textAt(dc,s,{lx,ly+26,w-pad,ly+46},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    auto chg=changesBefore(c);textAt(dc,chg.empty()?std::wstring(i18n::text(L"최근 BlueAsh에서 바꾼 설정"))+L": "+i18n::text(L"없음"):std::wstring(i18n::text(L"최근 BlueAsh에서 바꾼 설정"))+L": "+chg.front().label+L" ("+when(chg.front().time)+L")",{lx,ly+48,w-pad,ly+68},captionFont,chg.empty()?muted:amber,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
void paint(HDC dc){
    if(loading){for(int i=0;i<6;i++)painting::skeleton(dc,{listL,listT+i*rowH,listR,listT+i*rowH+rowH-8},14,listL,detR(),rowFill,RGB(40,62,112));
        painting::skeleton(dc,{detL,36,detR()-120,64},10,listL,detR(),rowFill,RGB(40,62,112));painting::skeleton(dc,{detL,90,detR(),180},14,listL,detR(),rowFill,RGB(40,62,112));
        textAt(dc,L"Windows 기록을 확인하는 중…",statusRect(),smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);return;}
    if(tech){for(int i=0;i<6;i++){RECT r=tabRect(i);bool on=i==tab;roundBox(dc,r,on?blue:rowFill,on?blue:line,12);textAt(dc,tabName(i),r,smallBold,on?RGB(255,255,255):mint,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}paintTech(dc);}
    else if(incidents.empty()){RECT r={36,36,detR(),250};roundBox(dc,r,rowFill,line,18);glyph(dc,0xE73E,{60,70,108,118},iconBig,RGB(110,220,150));
        textAt(dc,denied?L"Windows 기록을 읽을 권한이 없습니다":L"최근 90일 동안 Windows 기록에서 찾은 문제가 없습니다",{124,66,detR()-20,98},sectionFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        textAt(dc,L"기록이 없다고 PC에 문제가 전혀 없다는 뜻은 아닙니다. Windows가 기록을 남기기 전에 꺼지면 기록이 남지 않을 수 있습니다.",{124,104,detR()-20,170},font,muted,DT_LEFT|DT_WORDBREAK);}
    else{paintList(dc);if(auto c=current())paintDetail(dc,*c);}
    if(!status.empty())textAt(dc,status,statusRect(),smallFont,amber,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}

LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{page=h;loading=false;tech=false;drag=0;status.clear(); /* fresh state: the last page may have been closed mid-scan */
        pageButton(h,i18n::text(L"기술 정보 보기"),36,556,180,501);pageButton(h,i18n::text(L"다시 검사"),226,556,130,502);pageButton(h,i18n::text(L"측정 기록 지우기"),366,556,170,509);pageButton(h,i18n::text(L"닫기"),696,556,120,503);
        int w=(816-36-5*10)/6;const wchar_t* names[]={L"오류 코드 복사",L"검색어 복사",L"웹에서 검색",L"Microsoft 문서",L"이벤트 뷰어",L"내보내기"};const int ids[]={504,505,506,507,510,508};for(int i=0;i<6;i++)pageButton(h,i18n::text(names[i]),36+i*(w+10),556,w,ids[i]);
        pageButton(h,i18n::text(L"‹ 쉬운 설명"),36,500,150,511);
        if(!mono)mono=CreateFontW(-13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        place();scan();return 0;}
    case WM_SIZE:place();return 0;
    case WM_APP+231:{std::unique_ptr<stabcollect::Result> r((stabcollect::Result*)lp);if((int)wp==scanGen)loaded(r.get());return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC m=CreateCompatibleDC(dc);HBITMAP b=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto o=SelectObject(m,b);
        eraseCarbon(h,m);paint(m);BitBlt(dc,0,0,r.right,r.bottom,m,0,0,SRCCOPY);SelectObject(m,o);DeleteObject(b);DeleteDC(m);EndPaint(h,&ps);return 0;}
    case WM_MOUSEWHEEL:if(tech){techTop=std::max(0,techTop+(GET_WHEEL_DELTA_WPARAM(wp)>0?-3:3));RECT r=techBox();InvalidateRect(h,&r,FALSE);return 0;}if(!tech&&(int)view.size()>visibleRows()){int d=GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1;listTop=std::clamp(listTop+d,0,(int)view.size()-visibleRows());InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_SETCURSOR:if((HWND)wp==h){POINT p;GetCursorPos(&p);ScreenToClient(h,&p);bool hand=false;
            if(tech){for(int i=0;i<6;i++){RECT r=tabRect(i);hand|=PtInRect(&r,p)!=FALSE;}}else{for(int i=listTop;i<(int)view.size()&&i<listTop+visibleRows();i++){RECT r=rowRect(i);hand|=PtInRect(&r,p)!=FALSE;}HDC d=GetDC(h);for(auto& q:chipRects(d))hand|=PtInRect(&q,p)!=FALSE;ReleaseDC(h,d);}
            {RECT tr,th;if((tech?techBar(tr,th):listBar(tr,th))&&PtInRect(&tr,p))hand=true;}
            if(hand){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}break;
    // Scroll bars: press the thumb and drag; pressing the track jumps there and keeps dragging.
    case WM_MOUSEMOVE:if(drag){int y=(short)HIWORD(lp);RECT tr,th;if(drag==1&&listBar(tr,th)){int n=(int)view.size(),vis=visibleRows(),span=(tr.bottom-tr.top)-(th.bottom-th.top);listTop=std::clamp(dragTop0+(y-dragY0)*(n-vis)/std::max(1,span),0,n-vis);}
            if(drag==2&&techBar(tr,th)){int n=(int)techLines.size(),vis=techVisible(),span=(tr.bottom-tr.top)-(th.bottom-th.top);techTop=std::clamp(dragTop0+(y-dragY0)*(n-vis)/std::max(1,span),0,n-vis);}InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:if(drag){drag=0;ReleaseCapture();InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_CAPTURECHANGED:if(drag&&(HWND)lp!=h){drag=0;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_LBUTTONDOWN:{POINT p={(short)LOWORD(lp),(short)HIWORD(lp)};
        {RECT tr,th;int which=tech?2:1;if((tech?techBar(tr,th):listBar(tr,th))&&PtInRect(&tr,p)){int& top=tech?techTop:listTop;
            if(!PtInRect(&th,p)){int n=tech?(int)techLines.size():(int)view.size(),vis=tech?techVisible():visibleRows(),span=(tr.bottom-tr.top)-(th.bottom-th.top);top=std::clamp((int)((p.y-tr.top-(th.bottom-th.top)/2)*(n-vis)/std::max(1L,(long)span)),0,n-vis);}
            drag=which;dragY0=p.y;dragTop0=top;SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}}
        if(tech){for(int i=0;i<6;i++){RECT r=tabRect(i);if(PtInRect(&r,p)&&i!=tab){tab=i;showButtons();}}}
        else{HDC d=GetDC(h);auto chips=chipRects(d);ReleaseDC(h,d);for(int m=0;m<(int)chips.size();m++)if(PtInRect(&chips[m],p)){int to=monthSel+(m==0?1:-1);if(to>=0&&to<(int)monthKeys.size()){monthSel=to;buildView();listTop=0;sel=view.empty()?-1:view.front();showButtons();}return 0;}
            for(int i=listTop;i<(int)view.size()&&i<listTop+visibleRows();i++){RECT r=rowRect(i);if(PtInRect(&r,p)&&view[i]!=sel){sel=view[i];showButtons();}}}return 0;}
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){int id=LOWORD(wp);const Incident* c=current();
        if(id==503)DestroyWindow(h);
        else if(id==502)scan();
        else if(id==509){bool gone=true;for(bool prev:{false,true}){std::wstring p=stabcollect::telemetryPath(prev);if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES&&!DeleteFileW(p.c_str()))gone=false;}
            {std::lock_guard<std::mutex> k(stabcollect::recorder.lock);stabcollect::recorder.ring.clear();}rows.clear();setStatus(gone?i18n::text(L"BlueAsh가 남긴 측정 기록을 지웠습니다. Windows 기록은 그대로입니다."):i18n::text(L"측정 기록을 지우지 못했습니다."));}
        else if(id==501&&c){tech=true;tab=0;showButtons();}
        else if(id==511){tech=false;showButtons();}
        else if(!c)return 0;
        else if(id==504){std::wstring t;for(auto& k:c->codes)t+=k.label+L": "+k.value+L"\r\n";setStatus(copyText(safe(t))?i18n::text(L"오류 코드를 복사했습니다."):i18n::text(L"복사하지 못했습니다."));}
        else if(id==505){setStatus(copyText(safe(searchText(*c)))?i18n::text(L"검색어를 복사했습니다."):i18n::text(L"복사하지 못했습니다."));}
        else if(id==506){std::wstring q=safe(searchText(*c));if(!q.empty())ShellExecuteW(h,L"open",(L"https://www.google.com/search?q="+urlEncode(q)).c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
        else if(id==507){if(auto u=docUrl(*c))ShellExecuteW(h,L"open",u,nullptr,nullptr,SW_SHOWNORMAL);}
        else if(id==510){ShellExecuteW(h,L"open",L"eventvwr.msc",nullptr,nullptr,SW_SHOWNORMAL);setStatus(i18n::text(L"이벤트 뷰어를 열었습니다. 원본 로그 탭의 원본 이름·이벤트 ID로 찾으세요."));}
        else if(id==508){wchar_t file[MAX_PATH]=L"BlueAsh-stability.txt";OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=win;o.lpstrFilter=L"Text (*.txt)\0*.txt\0";o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"txt";o.Flags=OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;
            if(GetSaveFileNameW(&o)){std::wstring t=L"BlueAsh Stability Guard · "+std::wstring(appVersion)+L"\r\n"+std::wstring(i18n::text(c->title))+L"\r\n"+std::wstring(i18n::text(c->summary))+L"\r\n\r\n";
                for(int i=0;i<6;i++){t+=L"===== "+std::wstring(i18n::text(tabName(i)))+L" =====\r\n"+techText(*c,i)+L"\r\n";}t=safe(t);
                int n=WideCharToMultiByte(CP_UTF8,0,t.c_str(),(int)t.size(),nullptr,0,nullptr,nullptr);std::string u(n,'\0');WideCharToMultiByte(CP_UTF8,0,t.c_str(),(int)t.size(),&u[0],n,nullptr,nullptr);
                FILE* f=_wfopen(file,L"wb");bool ok=false;if(f){fwrite("\xEF\xBB\xBF",1,3,f);ok=fwrite(u.data(),1,u.size(),f)==u.size();fclose(f);}setStatus(ok?i18n::text(L"내보냈습니다."):i18n::text(L"설정을 저장하지 못했습니다."));}}
        }return 0;
    case WM_CTLCOLORSTATIC:{wchar_t cls[16]={};GetClassNameW((HWND)lp,cls,16);if(!_wcsicmp(cls,L"Edit")){HDC dc=(HDC)wp;SetTextColor(dc,mint);SetBkColor(dc,panel);return (LRESULT)panelBrush;}return pageStatic(wp,lp);}
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:page=nullptr;loading=false;drag=0;++scanGen;return 0;
    }return DefWindowProcW(h,msg,wp,lp);}
// ---- The warning window: "BlueAsh Stability Guard", red, the mark in red, what was found and its plain explanation,
// with 나중에 and 원인 분석 (opens this page on that incident). Topmost, centred on the screen BlueAsh is on. A full-screen
// game in front holds it back until the game is left (SHQueryUserNotificationState), so it never covers play.
HWND alertWin=nullptr;Incident alertIncident;std::vector<Incident> alertQueue;int alertHover=0;ULONGLONG lastGpuAlert=0;
const COLORREF redInk=RGB(255,140,146),redFill=RGB(214,48,62);
RECT alertLater(){return {300,262,404,304};}RECT alertOpen(){return {414,262,532,304};}RECT alertX(){return {520,16,548,44};}
void paintAlert(HDC dc,RECT rc){
    if(painting::token){Gdiplus::Graphics g(dc);Gdiplus::LinearGradientBrush b(Gdiplus::Point(0,0),Gdiplus::Point(0,rc.bottom),Gdiplus::Color(255,74,14,22),Gdiplus::Color(255,22,8,14));g.FillRectangle(&b,0,0,rc.right,rc.bottom);}
    else{HBRUSH b=CreateSolidBrush(RGB(40,10,16));FillRect(dc,&rc,b);DeleteObject(b);}
    painting::glow(dc,{-20,-30,150,120},RGB(255,50,64),70);
    if(!painting::drawLogo(dc,{28,26,80,78},true))glyph(dc,0xE7BA,{28,26,80,78},iconBig,redInk);
    textAt(dc,L"BlueAsh Stability Guard",{96,26,500,48},smallBold,redInk,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,alertIncident.title,{96,50,532,104},sectionFont,ink,DT_LEFT|DT_WORDBREAK);
    textAt(dc,alertIncident.summary,{28,114,532,200},font,RGB(240,214,218),DT_LEFT|DT_WORDBREAK);
    std::wstring meta=when(alertIncident.time);if(!alertIncident.searches.empty())meta+=L" · "+alertIncident.searches.front();{std::wstring prog=programAt(alertIncident,stabcollect::readings());if(!prog.empty())meta+=L" · "+prog;}
    textAt(dc,meta,{28,208,532,230},captionFont,RGB(196,150,156),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    roundBox(dc,alertLater(),alertHover==1?RGB(78,24,34):RGB(52,16,24),RGB(120,40,52),12);textAt(dc,L"나중에",alertLater(),smallBold,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    roundBox(dc,alertOpen(),alertHover==2?RGB(236,64,78):redFill,alertHover==2?RGB(236,64,78):redFill,12);textAt(dc,L"원인 분석",alertOpen(),smallBold,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    glyph(dc,0xE711,alertX(),iconFont,alertHover==3?ink:RGB(196,150,156));}
int alertHit(POINT p){RECT a=alertLater(),b=alertOpen(),x=alertX();return PtInRect(&a,p)?1:PtInRect(&b,p)?2:PtInRect(&x,p)?3:0;}
LRESULT CALLBACK alertProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP b=CreateCompatibleBitmap(dc,r.right,r.bottom);auto o=SelectObject(mem,b);
        paintAlert(mem,r);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,o);DeleteObject(b);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_NCHITTEST:{LRESULT x=DefWindowProcW(h,m,w,l);if(x!=HTCLIENT)return x;POINT p={(short)LOWORD(l),(short)HIWORD(l)};ScreenToClient(h,&p);return alertHit(p)?HTCLIENT:HTCAPTION;} // drag anywhere else
    case WM_MOUSEMOVE:{POINT p={(short)LOWORD(l),(short)HIWORD(l)};int hv=alertHit(p);if(hv!=alertHover){alertHover=hv;InvalidateRect(h,nullptr,FALSE);TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}return 0;}
    case WM_MOUSELEAVE:if(alertHover){alertHover=0;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_SETCURSOR:if(alertHover){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
    case WM_LBUTTONUP:{POINT p={(short)LOWORD(l),(short)HIWORD(l)};int hv=alertHit(p);
        if(hv==2){Time t=alertIncident.time;DestroyWindow(h);wantTime=t;ShowWindow(win,SW_SHOW);ShowWindow(win,SW_RESTORE);SetForegroundWindow(win);
            if(page){scan();}else{openPage(proc,L"BlueAshStability",17);}}
        else if(hv==1||hv==3){DestroyWindow(h);}
        return 0;}
    case WM_KEYDOWN:if(w==VK_ESCAPE){DestroyWindow(h);}return 0;
    case WM_DESTROY:alertWin=nullptr;alertHover=0;return 0;
    }return DefWindowProcW(h,m,w,l);}
bool fullScreenBusy(){QUERY_USER_NOTIFICATION_STATE q;if(SHQueryUserNotificationState(&q)!=S_OK)return false;return q==QUNS_RUNNING_D3D_FULL_SCREEN||q==QUNS_BUSY||q==QUNS_PRESENTATION_MODE;}
void CALLBACK alertRetry(HWND,UINT,UINT_PTR,DWORD);
void showAlertNow(const Incident& c){
    alertIncident=c;if(alertWin){DestroyWindow(alertWin);}
    static bool reg=false;if(!reg){WNDCLASSW wc={};wc.lpfnWndProc=alertProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshGuard";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);reg=true;}
    HMONITOR mon=MonitorFromWindow(win,MONITOR_DEFAULTTOPRIMARY);MONITORINFO mi={};mi.cbSize=sizeof(mi);GetMonitorInfoW(mon,&mi);RECT wa=mi.rcWork;dpi::down(wa); // design coordinates like every window of ours
    const int W=560,H=328;int x=(wa.left+wa.right-W)/2,y=(wa.top+wa.bottom-H)/2;
#ifdef BLUEASH_TT
    if(GetEnvironmentVariableW(L"BLUEASH_TT_ALERT",nullptr,0)){x=-3000;} /* test builds: off screen, never over the person's work */
#endif
    alertWin=CreateWindowExW(WS_EX_TOPMOST,L"BlueAshGuard",L"BlueAsh Stability Guard",WS_POPUP,x,y,W,H,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);if(!alertWin){return;}
    int corner=2;COLORREF border=RGB(170,44,58);DwmSetWindowAttribute(alertWin,33,&corner,sizeof(corner));DwmSetWindowAttribute(alertWin,34,&border,sizeof(border));
    ShowWindow(alertWin,SW_SHOW);SetForegroundWindow(alertWin);}
// Shows now, or once no full-screen program is in front (checked every 3 s).
void showAlert(const Incident& c){if(fullScreenBusy()){alertQueue.push_back(c);SetTimer(win,0xB1E,3000,alertRetry);return;}showAlertNow(c);}
void CALLBACK alertRetry(HWND,UINT,UINT_PTR,DWORD){if(alertQueue.empty()){KillTimer(win,0xB1E);return;}if(fullScreenBusy()){return;}KillTimer(win,0xB1E);Incident c=alertQueue.back();alertQueue.clear();showAlertNow(c);}
// ---- Live watch: a game crash or hang (Application Error 1000 / Application Hang 1002) and a graphics driver timeout
// (Display 4101, nvlddmkm, amdkmdag) as Windows writes them (EvtSubscribe, future records only). The record goes to the
// main window (WM_APP+232) and becomes one incident; driver records within 3 minutes are one alert.
EVT_HANDLE subApp=nullptr,subSys=nullptr;
DWORD WINAPI onRecord(EVT_SUBSCRIBE_NOTIFY_ACTION a,PVOID,EVT_HANDLE e){if(a!=EvtSubscribeActionDeliver){return 0;}auto* ev=new Event(parse(stabcollect::renderXml(e)));
    Kind k=kindOf(*ev);if((k==Kind::GameCrash||k==Kind::GameHang||k==Kind::GpuTimeout)&&PostMessageW(win,WM_APP+232,0,(LPARAM)ev)){return 0;}delete ev;return 0;}
void watch(){if(subApp||subSys){return;}
    subApp=EvtSubscribe(nullptr,nullptr,L"Application",L"*[System[(Provider[@Name='Application Error'] and EventID=1000) or (Provider[@Name='Application Hang'] and EventID=1002)]]",nullptr,nullptr,onRecord,EvtSubscribeToFutureEvents);
    subSys=EvtSubscribe(nullptr,nullptr,L"System",L"*[System[(Provider[@Name='Display'] and EventID=4101) or Provider[@Name='nvlddmkm'] or Provider[@Name='amdkmdag']]]",nullptr,nullptr,onRecord,EvtSubscribeToFutureEvents);}
void unwatch(){if(subApp){EvtClose(subApp);}if(subSys){EvtClose(subSys);}subApp=subSys=nullptr;}
void recorded(Event* e){std::unique_ptr<Event> held(e);if(kindOf(*e)==Kind::GpuTimeout){if(GetTickCount64()-lastGpuAlert<180000){return;}lastGpuAlert=GetTickCount64();}
    auto v=build({*e},stabcollect::utcOffset());if(!v.empty()){showAlert(v.front());}}
// At start: watch live records, and (on a worker thread) a power loss or blue screen newer than the last one looked at,
// from the last 3 days, opens the warning window (WM_APP+230 carries the incident to the main window).
void checkAtStart(){watch();CreateThread(nullptr,0,[](LPVOID)->DWORD{
#ifdef BLUEASH_TT
        if(GetEnvironmentVariableW(L"BLUEASH_TT_ALERT",nullptr,0)){Sleep(1500);auto r=stabcollect::collect(30);auto v=build(r.events,stabcollect::utcOffset());if(!v.empty()){auto* c=new Incident(v.front());if(!PostMessageW(win,WM_APP+230,0,(LPARAM)c)){delete c;}}return 0;} /* test builds: the newest incident at once */
#endif
        Sleep(600);auto r=stabcollect::collect(3); /* right after start: the main window exists by now */auto v=build(r.events,stabcollect::utcOffset());const Incident* top=nullptr;for(auto& c:v){if(c.boot&&(!top||c.boot>top->boot))top=&c;}
        std::wstring seen=regString(configPath,L"StabilitySeen");if(top&&_wcstoui64(seen.c_str(),nullptr,10)<top->boot){saveString(configPath,L"StabilitySeen",std::to_wstring(top->boot));auto* c=new Incident(*top);if(!PostMessageW(win,WM_APP+230,0,(LPARAM)c)){delete c;}}return 0;},nullptr,0,nullptr);}
}
