#pragma once
// DLSS 5 그래픽 (sidebar, page 14). Lists installed games where DLSS 5 can be used (dlss_policy.h decides; Tarkov, PUBG
// and anti-cheat games never show) and, for one game: a before/after viewer of real frames, the presets that game's
// route really offers, apply / restore, a preset editor and live GPU readings.
//  - "ingame" titles (official DLSS 5): the game's own menu switches it; BlueAsh saves the choice, captures the screen
//    with Windows Graphics Capture for the comparison and changes no game file.
//  - "optiscaler" titles (community route): BlueAsh installs Dagherbou's OptiScaler DLSS-NR build (dlss_optiscaler.h)
//    plus the person's own signed nvngx_dlssnr.dll (dlss_runtime.h) as one transaction (dlss_apply.h), writes the real
//    [DlssNr] settings (dlss_nr.h), and takes the comparison from the build's own before/after capture of the same frame.
// Screenshots, settings and records stay on this PC (%LOCALAPPDATA%\ModeSwitch\dlss\).
#include <dxgi1_2.h>
#include "dlss_library.h"
#include "dlss_optiscaler.h"
#include "dlss_gpu.h"
#include "dlss_live.h"
#include "dlss_approx.h"
#include "dlss_capture_pair.h"
#include "dlss_status.h"
#include "dlss_face.h"
#include "dlss_name.h"
#include "perf_core.h"
#include "perf_hw.h"
#include "perf_trace.h"
#include "perf_boost.h"
#include "pc_check.h"
#include "win_tune.h"
#include <chrono>
namespace dlssui {
// Live view of the running game (dlss_live.h): started while one game's page or the editor is open and its window is up.
// mode 0 = live, 1 = the baseline capture beside the live picture, 2 = the same frame before/after (optiscaler build only).
dlsslive::Capture livecap;HWND liveWin=nullptr;ULONGLONG liveRetry=0;bool paused=false,gameUp=false;int mode=3;double liveSame=1;
std::vector<unsigned char> liveBuf;int liveW=0,liveH=0;unsigned liveSeq=0;Gdiplus::Bitmap *liveBmp=nullptr,*baseBmp=nullptr;
struct Snap {bool ok=false;dlssgpu::Reading gpu;dlsslive::Stats fps;} baseSnap; // readings when the baseline was taken
// First screen (view 0): tab 0 화질 향상 / 1 프레임 설정 / 2 성능 분석 over one game carousel (or, 전체 보기, a grid).
// carPos follows carTarget (both in card units, unbounded: the carousel turns round); carIdx is the chosen card.
HWND stage=nullptr; // the carousel's own window (see paintStage)
// 근사 미리보기 pictures (see apxRefresh): the source and the filtered copy; the bitmaps wrap the byte buffers.
std::vector<unsigned char> apxSrc,apxOut;int apxW=0,apxH=0;Gdiplus::Bitmap *apxSrcBmp=nullptr,*apxBmp=nullptr;bool apxDirty=false;std::wstring apxFrom;
std::atomic<bool> shotJob{false};
const int maxPics=6;int picIdx=0,picCount=0;void showPic(int i); // 미리보기(예상): which of the pictures (apxLoad)
bool bigView=false; // the picture enlarged over the page (the viewer's corner button), with big preset buttons under it
int tab=0,carIdx=0,dragX=0;bool gridMode=false,carDrag=false,dragMoved=false;double carPos=0,carTarget=0,dragPos=0;std::map<std::wstring,Gdiplus::Bitmap*> cards,thumbs;
std::vector<dlsslib::Game> games;bool scanning=false,showHidden=false,working=false;int view=0,pick=-1,hoverKnob=-1,dragKnob=-1;dlss::Preset preset=dlss::Preset::Original;
std::wstring status,gpu;bool statusBad=false;HWND page=nullptr,viewer=nullptr;std::map<std::wstring,Gdiplus::Bitmap*> covers;
int applyProgress=-1;
dlssgpu::Nvml nvml;dlssgpu::Reading live;dlssnr::Settings edit;dlssrt::Info runtimeInfo;
const char* knownLatest="310.9.16"; // the newest DLSS 5 runtime seen in public sources (2026-10-16); there is no official feed to ask
std::wstring folders(){return regString(configPath,L"DlssFolders");}
std::wstring root(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+L"\\ModeSwitch\\dlss";CreateDirectoryW((std::wstring(d)+L"\\ModeSwitch").c_str(),nullptr);CreateDirectoryW(r.c_str(),nullptr);return r;}
std::wstring runtimePath(){std::wstring d=root()+L"\\runtime";CreateDirectoryW(d.c_str(),nullptr);return d+L"\\nvngx_dlssnr.dll";}
std::wstring previousRuntime(){std::wstring d=root()+L"\\runtime\\previous";CreateDirectoryW(d.c_str(),nullptr);return d+L"\\nvngx_dlssnr.dll";}
std::wstring componentDir(){std::wstring d=root()+L"\\components";CreateDirectoryW(d.c_str(),nullptr);return d+L"\\OptiScaler-DLSSNR-"+dlssopti::version;}
std::wstring keyOf(const dlsslib::Game& g){std::wstring k(g.e.platform.begin(),g.e.platform.end());k+=L"-";for(char c:g.e.id)k+=isalnum((unsigned char)c)?(wchar_t)c:L'_';return k;}
std::wstring gameDir(const dlsslib::Game& g){std::wstring d=root()+L"\\"+keyOf(g);CreateDirectoryW(d.c_str(),nullptr);return d;}
// Every installed game except blocked ones (Tarkov, PUBG, anti-cheat): verified first, then by name. Unverified games
// can be browsed and watched live, but nothing is applied to them. Rebuilt when a scan finishes.
std::vector<int> listOrder;
void buildList(){listOrder.clear();for(int i=0;i<(int)games.size();i++)if(games[i].v.status!=dlss::Status::Blocked)listOrder.push_back(i);
    std::stable_sort(listOrder.begin(),listOrder.end(),[](int a,int b){bool va=games[a].v.status==dlss::Status::Verified,vb=games[b].v.status==dlss::Status::Verified;return va!=vb?va:_wcsicmp(games[a].e.name.c_str(),games[b].e.name.c_str())<0;});}
const std::vector<int>& listed(){return listOrder;}
// A game BlueAsh has not verified but that ships its own DLSS (nvngx_dlss.dll next to its exe, dlsslib::findDlss) and is
// not blocked: the same community route as the verified titles, with a stronger warning before applying.
bool community(const dlsslib::Game& g){return !g.v.title&&g.v.status==dlss::Status::Unknown&&!g.dlssDir.empty();}
// "verified" here = BlueAsh can apply DLSS 5 to it (a verified title, or the community route above).
bool verified(const dlsslib::Game& g){return g.v.status==dlss::Status::Verified||community(g);}
bool opti(const dlsslib::Game& g){return community(g)||(g.v.title&&std::string(g.v.title->method)=="optiscaler");}
bool presetOk(const dlsslib::Game& g,dlss::Preset p){return p==dlss::Preset::Original||community(g)||dlss::presetSupported(g.v.title,p);}
// The folder the route's files go into (beside the game's executable).
std::wstring exeDir(const dlsslib::Game& g){if(community(g))return g.dlssDir;std::wstring rel=g.v.title&&g.v.title->exeRel?g.v.title->exeRel:L"";auto cut=rel.find_last_of(L'\\');return cut==std::wstring::npos?g.e.dir:g.e.dir+L"\\"+rel.substr(0,cut);}
std::wstring exeName(const dlsslib::Game& g){if(community(g))return g.dlssExe;std::wstring rel=g.v.title&&g.v.title->exeRel?g.v.title->exeRel:L"";auto cut=rel.find_last_of(L'\\');return cut==std::wstring::npos?rel:rel.substr(cut+1);}
bool installed(const dlsslib::Game& g){return GetFileAttributesW((gameDir(g)+L"\\applied.txt").c_str())!=INVALID_FILE_ATTRIBUTES;}
// Saved choice per game: dlss\<game>\preset.txt, the custom values in custom.txt (survive restarts; re-checked on every use).
dlss::Preset loadPreset(const dlsslib::Game& g){FILE* f=_wfopen((gameDir(g)+L"\\preset.txt").c_str(),L"rb");if(!f)return dlss::Preset::Original;char b[32]={};fread(b,1,31,f);fclose(f);
    for(int p=0;p<5;p++){if(!strcmp(b,dlss::presetKey((dlss::Preset)p)))return (dlss::Preset)p;}return dlss::Preset::Original;}
bool writeFile(const std::wstring& path,const std::string& text){FILE* f=_wfopen(path.c_str(),L"wb");if(!f)return false;size_t n=fwrite(text.data(),1,text.size(),f);return fclose(f)==0&&n==text.size();}
std::string readFile(const std::wstring& path,size_t max=1<<20){FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return "";std::string s(max,0);s.resize(fread(&s[0],1,max,f));fclose(f);return s;}
bool savePreset(const dlsslib::Game& g,dlss::Preset p){return writeFile(gameDir(g)+L"\\preset.txt",dlss::presetKey(p));}
dlssnr::Settings custom(const dlsslib::Game& g){dlssnr::Settings s=dlssnr::defaults();dlssnr::parse(readFile(gameDir(g)+L"\\custom.txt",8192),s);return s;}
dlssnr::Settings settingsFor(const dlsslib::Game& g,dlss::Preset p){return p==dlss::Preset::Custom?custom(g):dlssnr::builtin((int)p);}
std::wstring wideOf(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n);return w;}
// The graphics card: name and memory from DXGI (real numbers only; nothing is estimated).
std::wstring gpuText(){IDXGIFactory1* f=nullptr;std::wstring out;if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&f)))return out;IDXGIAdapter1* a=nullptr;
    for(UINT i=0;f->EnumAdapters1(i,&a)!=DXGI_ERROR_NOT_FOUND;i++){DXGI_ADAPTER_DESC1 d={};a->GetDesc1(&d);
        if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&out.empty()){wchar_t b[256];swprintf(b,256,L"%ls · VRAM %.0f GB",d.Description,d.DedicatedVideoMemory/1073741824.0);out=b;
            std::wstring n=d.Description;bool rtx50=n.find(L"RTX 50")!=std::wstring::npos;out+=rtx50?i18n::text(L" · DLSS 5 지원 GPU"):i18n::text(L" · DLSS 5는 RTX 50 시리즈가 필요합니다");}a->Release();}
    f->Release();return out;}
// Live line (NVML, once a second while the page is open): measured values only.
std::wstring liveText(){if(!live.ok)return gpu.empty()?std::wstring():L"GPU · "+gpu;wchar_t b[320];
    swprintf(b,320,i18n::text(L"%ls · 드라이버 %ls · 사용률 %u%% · %u°C · %u W · VRAM %.1f / %.1f GB"),wideOf(live.name).c_str(),wideOf(live.driver).c_str(),live.util,live.temp,live.watts,live.usedGB,live.totalGB);return b;}
// Frame rate as far as BlueAsh can see it: the rate at which composed game frames reach the screen capture. It is an
// estimate (bounded by the refresh rate, and a still picture sends no frames); the game's own render rate and its 1 % low
// need a present-event trace (PresentMon / ETW, administrator), which BlueAsh does not run.
std::wstring fpsText(){auto s=livecap.stats();if(!gameUp||s.samples<30)return i18n::text(L"표시 FPS (추정): 데이터 모으는 중");wchar_t b[200];
    if(s.low1>0)swprintf(b,200,i18n::text(L"표시 FPS (추정, 캡처 기준) %.0f · 1%% Low %.0f · 표본 %d"),s.fps,s.low1,s.samples);else swprintf(b,200,i18n::text(L"표시 FPS (추정, 캡처 기준) %.0f · 1%% Low는 표본 100개부터"),s.fps);return b;}
// Change since the baseline capture (both sides measured; frame rates are the estimate above).
std::wstring deltaText(){if(!baseSnap.ok||!live.ok)return i18n::text(L"'지금 화면 저장'을 누르면 그때와 지금의 차이가 나옵니다.");auto s=livecap.stats();wchar_t b[220];
    swprintf(b,220,i18n::text(L"저장한 화면 대비 · 사용률 %+d%% · VRAM %+.1f GB · %+d W"),(int)live.util-(int)baseSnap.gpu.util,live.usedGB-baseSnap.gpu.usedGB,(int)live.watts-(int)baseSnap.gpu.watts);std::wstring t=b;
    if(baseSnap.fps.samples>=30&&s.samples>=30){swprintf(b,220,i18n::text(L" · 표시 FPS %+.0f (추정)"),s.fps-baseSnap.fps.fps);t+=b;}return t;}
// Worker thread; the page gets the list (or, if it was closed meanwhile, the result is dropped).
void scan(){if(scanning||!page)return;scanning=true;SetTimer(page,4,30,nullptr);struct Job{HWND page;std::wstring folders;};auto* j=new Job{page,folders()};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(Job*)p;auto* r=new std::vector<dlsslib::Game>(dlsslib::scan(j->folders));
        auto* g=new std::wstring(gpuText());if(!PostMessageW(j->page,WM_APP+190,(WPARAM)g,(LPARAM)r)){delete r;delete g;}delete j;return 0;},j,0,nullptr);if(t)CloseHandle(t);else{delete j;scanning=false;}}
// An apply that never finished (BlueAsh closed in the middle) is undone at start.
void recoverAll(){WIN32_FIND_DATAW f;std::wstring r=root();HANDLE h=FindFirstFileW((r+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return;
    do{if((f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&f.cFileName[0]!=L'.'&&!dlssapply::recover(r+L"\\"+f.cFileName))problem(i18n::text(L"DLSS 5 적용 기록을 되돌리지 못했습니다."));}while(FindNextFileW(h,&f));FindClose(h);}
// --dlss-restore-all (the uninstaller runs it before removing BlueAsh): every game BlueAsh applied DLSS 5 to gets its own
// files back from the backups. A file that cannot be put back (the game is running) keeps its record so a later restore
// still can. Returns how many games failed (0 = all back to original).
int restoreAll(){WIN32_FIND_DATAW f;std::wstring r=root();int failed=0;HANDLE h=FindFirstFileW((r+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return 0;
    do{if(!(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||f.cFileName[0]==L'.')continue;std::wstring d=r+L"\\"+f.cFileName;
        if(!dlssapply::recover(d)){failed++;continue;}
        if(GetFileAttributesW((d+L"\\applied.txt").c_str())!=INVALID_FILE_ATTRIBUTES&&!dlssapply::restore(d,nullptr).empty()){failed++;}}while(FindNextFileW(h,&f));
    FindClose(h);return failed;}
// The game's process: any running exe under its install folder.
DWORD runningPid(const dlsslib::Game& g){HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(s==INVALID_HANDLE_VALUE)return 0;PROCESSENTRY32W e={};e.dwSize=sizeof(e);DWORD found=0;
    if(Process32FirstW(s,&e))do{HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(!p)continue;wchar_t path[MAX_PATH*2];DWORD n=MAX_PATH*2;
        if(QueryFullProcessImageNameW(p,0,path,&n)&&n>g.e.dir.size()&&!_wcsnicmp(path,g.e.dir.c_str(),g.e.dir.size())&&path[g.e.dir.size()]==L'\\'&&wgc::window(e.th32ProcessID)){found=e.th32ProcessID;}CloseHandle(p);}while(!found&&Process32NextW(s,&e));
    CloseHandle(s);return found;}
CLSID pngClsid(){CLSID id={};UINT n=0,size=0;Gdiplus::GetImageEncodersSize(&n,&size);std::vector<BYTE> buf(size);auto* e=(Gdiplus::ImageCodecInfo*)buf.data();
    if(size&&Gdiplus::GetImageEncoders(n,size,e)==Gdiplus::Ok){for(UINT i=0;i<n;i++)if(!wcscmp(e[i].MimeType,L"image/png"))id=e[i].Clsid;}return id;}
bool savePng(const std::wstring& path,int w,int h,int stride,const unsigned char* bgra){Gdiplus::Bitmap shot(w,h,stride,PixelFormat32bppRGB,(BYTE*)bgra);CLSID png=pngClsid();return shot.Save(path.c_str(),&png,nullptr)==Gdiplus::Ok;}
// ---- The before/after viewer: one scene, the left part from the original frame and the right part from the DLSS 5
// one, split where the line is. Wheel zooms around the cursor, dragging moves the line (near it) or the picture, a double
// click switches between "fit" and the frame's own pixels.
struct Compare {Gdiplus::Bitmap *a=nullptr,*b=nullptr;double split=.5,zoom=0,cx=0,cy=0;int drag=0;POINT last={};std::wstring missing;bool mismatch=false;} cmp;
void drop(Gdiplus::Bitmap*& b){delete b;b=nullptr;}
// Edges of both pictures at 64x36, correlated: a low score means the two screenshots are not the same view.
double sameScene(Gdiplus::Bitmap* a,Gdiplus::Bitmap* b){const int W=64,H=36;std::vector<double> e[2];Gdiplus::Bitmap* in[2]={a,b};
    for(int k=0;k<2;k++){Gdiplus::Bitmap s(W,H,PixelFormat32bppRGB);{Gdiplus::Graphics g(&s);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);g.DrawImage(in[k],0,0,W,H);}
        std::vector<double> gray(W*H);for(int y=0;y<H;y++)for(int x=0;x<W;x++){Gdiplus::Color c;s.GetPixel(x,y,&c);gray[y*W+x]=.3*c.GetR()+.59*c.GetG()+.11*c.GetB();}
        for(int y=1;y<H;y++){for(int x=1;x<W;x++)e[k].push_back(std::fabs(gray[y*W+x]-gray[y*W+x-1])+std::fabs(gray[y*W+x]-gray[(y-1)*W+x]));}}
    double ma=0,mb=0;size_t n=e[0].size();for(size_t i=0;i<n;i++){ma+=e[0][i];mb+=e[1][i];}ma/=n;mb/=n;double sab=0,saa=0,sbb=0;
    for(size_t i=0;i<n;i++){double x=e[0][i]-ma,y=e[1][i]-mb;sab+=x*y;saa+=x*x;sbb+=y*y;}return saa>0&&sbb>0?sab/std::sqrt(saa*sbb):0;}
std::wstring shotPath(const dlsslib::Game& g,dlss::Preset p){return gameDir(g)+L"\\"+std::wstring(dlss::presetKey(p),dlss::presetKey(p)+strlen(dlss::presetKey(p)))+L".png";}
std::wstring basePath(const dlsslib::Game& g){return gameDir(g)+L"\\baseline.png";}
// Settings BlueAsh last wrote into the game's OptiScaler.ini; its time tells whether the running game started with them
// (the build reads the ini once, at game start: a change while the game runs needs a restart).
std::wstring writtenPath(const dlsslib::Game& g){return gameDir(g)+L"\\written.txt";}
bool renoAvailable(const dlsslib::Game& g){auto d=exeDir(g);return GetFileAttributesW((d+L"\\renodx-dlss5.addon64").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((d+L"\\renodx-dlss.addon64").c_str())!=INVALID_FILE_ATTRIBUTES;}
// Who put DLSS 5 into the game: BlueAsh (its applied record), another tool's OptiScaler (OptiScaler.ini + the runtime) or
// a RenoDX add-on (ReShade.ini with its section). BlueAsh never replaces another tool's files, but it writes the chosen
// look into that tool's own settings (writeSettings), so presets, the editor and the frame limit work there too.
enum class Route {None,Ours,Opti,Reno};
Route route(const dlsslib::Game& g){if(installed(g))return Route::Ours;std::wstring d=exeDir(g);if(GetFileAttributesW((d+L"\\nvngx_dlssnr.dll").c_str())==INVALID_FILE_ATTRIBUTES)return Route::None;
    if(GetFileAttributesW((d+L"\\OptiScaler.ini").c_str())!=INVALID_FILE_ATTRIBUTES)return Route::Opti;
    if(renoAvailable(g)&&readFile(d+L"\\ReShade.ini",1<<20).find("[RenoDX.DLSS5]")!=std::string::npos)return Route::Reno;
    return Route::None;}
bool managed(const dlsslib::Game& g){return route(g)!=Route::None;}
bool loadRenoPair(const dlsslib::Game& g){
    std::wstring dir=exeDir(g)+L"\\DLSS5 Screenshots\\",latest;FILETIME time={};WIN32_FIND_DATAW f;
    HANDLE h=FindFirstFileW((dir+L"NR_CAPTURE_*.meta.txt").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return false;
    do{std::wstring n=f.cFileName;if(!dlsscapture::validName(n))continue;std::wstring base=n.substr(0,n.size()-9);
        if(!dlsscapture::validMetadata(readFile(dir+n,32768)))continue;
        if(GetFileAttributesW((dir+base+L"_NR_OFF.png").c_str())==INVALID_FILE_ATTRIBUTES||GetFileAttributesW((dir+base+L".png").c_str())==INVALID_FILE_ATTRIBUTES)continue;
        if(latest.empty()||CompareFileTime(&f.ftLastWriteTime,&time)>0){latest=base;time=f.ftLastWriteTime;}
    }while(FindNextFileW(h,&f));FindClose(h);if(latest.empty())return false;
    Gdiplus::Bitmap* a=gameart::load(dir+latest+L"_NR_OFF.png");Gdiplus::Bitmap* b=gameart::load(dir+latest+L".png");
    if(!a||!b||a->GetWidth()!=b->GetWidth()||a->GetHeight()!=b->GetHeight()){delete a;delete b;return false;}
    cmp.a=a;cmp.b=b;return true;
}
DWORD livePid=0;ULONGLONG lastPidScan=0; // the running game (see gamePid)
std::string readLogTail(const std::wstring& path,FILETIME started,FILETIME& modified){HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return "";
    FILETIME t={};LARGE_INTEGER size={};std::string s;if(GetFileTime(h,nullptr,nullptr,&t)&&CompareFileTime(&t,&started)>=0&&GetFileSizeEx(h,&size)){LARGE_INTEGER pos={};pos.QuadPart=std::max<LONGLONG>(0,size.QuadPart-65536);SetFilePointerEx(h,pos,nullptr,FILE_BEGIN);s.resize((size_t)(size.QuadPart-pos.QuadPart));DWORD n=0;if(!s.empty()&&ReadFile(h,&s[0],(DWORD)s.size(),&n,nullptr))s.resize(n);else s.clear();modified=t;}CloseHandle(h);return s;}
dlssstatus::State applyState(const dlsslib::Game& g){std::wstring d=exeDir(g);bool runtime=GetFileAttributesW((d+L"\\nvngx_dlssnr.dll").c_str())!=INVALID_FILE_ATTRIBUTES;
    bool external=runtime&&(renoAvailable(g)||GetFileAttributesW((d+L"\\OptiScaler.ini").c_str())!=INVALID_FILE_ATTRIBUTES);bool files=external;
    FILETIME start={},exit={},kernel={},user={};bool running=false,restart=false;HANDLE p=livePid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,livePid):nullptr;
    if(p){wchar_t path[MAX_PATH*2];DWORD n=MAX_PATH*2;running=QueryFullProcessImageNameW(p,0,path,&n)&&n>g.e.dir.size()&&!_wcsnicmp(path,g.e.dir.c_str(),g.e.dir.size())&&path[g.e.dir.size()]==L'\\'&&GetProcessTimes(p,&start,&exit,&kernel,&user);CloseHandle(p);}
    if(running&&managed(g)){WIN32_FILE_ATTRIBUTE_DATA a;if(GetFileAttributesExW(writtenPath(g).c_str(),GetFileExInfoStandard,&a)||GetFileAttributesExW((gameDir(g)+L"\\applied.txt").c_str(),GetFileExInfoStandard,&a))restart=CompareFileTime(&start,&a.ftLastWriteTime)<=0;}
    dlssstatus::Log log=dlssstatus::Log::Unknown;if(running&&!restart){FILETIME best={};for(auto name:{L"OptiScaler.log",L"ReShade.log"}){FILETIME t={};auto s=readLogTail(d+L"\\"+name,start,t);auto l=dlssstatus::logState(s);if(l!=dlssstatus::Log::Unknown&&CompareFileTime(&t,&best)>=0){best=t;log=l;}}}
    return dlssstatus::classify(files,running,restart,log==dlssstatus::Log::Active,log==dlssstatus::Log::Waiting);}
int applyLevel(const dlsslib::Game& g){auto s=applyState(g);return s==dlssstatus::State::Active?2:s==dlssstatus::State::Absent?0:1;}
std::wstring applyText(const dlsslib::Game& g){int l=applyLevel(g);
    if(applyState(g)==dlssstatus::State::Restart)return i18n::text(L"게임 재시작 필요");
    if(l==1)return livePid?i18n::text(L"작동 미확인 · 게임 내 DLSS 5와 지원되는 XeSS 또는 FSR 설정을 확인하세요."):i18n::text(L"설치됨 · 게임 실행 후 작동 여부 확인");
    if(l==2)return i18n::text(L"DLSS 5 처리 확인됨");
    if(!opti(g))return i18n::text(L"화면만 받는 중 · DLSS 5는 게임 메뉴에서 켜고 끕니다 (BlueAsh는 켜졌는지 알 수 없음)");
    if(l==0)return i18n::text(L"화면만 받는 중 · DLSS 5 연결 안 됨 (적용 안 됨)");
    return i18n::text(L"DLSS 5 처리 확인됨");}
void loadShots(){drop(cmp.a);drop(cmp.b);drop(baseBmp);cmp.zoom=0;cmp.mismatch=false;cmp.missing.clear();if(pick<0||pick>=(int)games.size())return;auto& g=games[pick];
    baseBmp=gameart::load(basePath(g));
    // Same-frame pairs exist only on the optiscaler route: the build hands over the input and output of one frame.
    if(loadRenoPair(g)){}
    else if(!opti(g))cmp.missing=i18n::text(L"이 게임은 정확 비교를 할 수 없습니다. '저장한 화면과 비교'를 쓰세요.");
    else if(preset==dlss::Preset::Original)cmp.missing=i18n::text(L"원본 프리셋은 비교할 DLSS 5 화면이 없습니다. 다른 프리셋을 고르세요.");
    else{cmp.a=gameart::load(gameDir(g)+L"\\"+wideOf(dlss::presetKey(preset))+L"-before.png");cmp.b=gameart::load(shotPath(g,preset));
        if(!cmp.a||!cmp.b){drop(cmp.a);drop(cmp.b);cmp.missing=i18n::text(L"이 프리셋으로 찍은 실제 화면이 없습니다. 적용한 뒤 게임을 켜고 '실제 전후 캡처'를 누르세요.");}}
    if(viewer){InvalidateRect(viewer,nullptr,FALSE);}}
// What the viewer shows now: one picture (live) or two split by the line, with their labels; or, with no picture, why.
struct Shown {Gdiplus::Bitmap *a=nullptr,*b=nullptr;std::wstring la,lb,title,why,hint;bool loading=false;}; // loading: a shimmering skeleton instead of a picture
const dlsslib::Game* current();
Shown shown(){Shown v;
    if(mode==3){if(apxSrcBmp&&apxBmp){auto* g=current();v.a=apxSrcBmp;v.b=apxBmp;v.la=g?g->e.name:apxFrom;v.lb=i18n::text(L"미리보기(예상)");
            if(view!=2&&preset==dlss::Preset::Original){v.b=nullptr;v.hint=i18n::text(L"프리셋을 선택해 주세요");}} /* 원본: nothing to compare, the picture alone */
        else if(shotJob){v.loading=true;}
        else{v.title=i18n::text(L"미리보기에 쓸 그림이 없습니다");v.why=i18n::text(L"게임을 켜고 '지금 화면 저장'을 한 번 누르면, 그 뒤에는 게임 없이도 볼 수 있습니다.");}return v;}
    if(mode==2){if(cmp.a&&cmp.b){v.a=cmp.a;v.b=cmp.b;v.la=i18n::text(L"DLSS 5 전 (같은 순간)");v.lb=i18n::text(L"DLSS 5 후 (같은 순간)");}else{v.title=i18n::text(L"정확 비교 화면이 없습니다");v.why=cmp.missing;}return v;}
    if(!liveBmp){std::wstring why;auto st=livecap.state(&why);
        if(!gameUp){v.title=i18n::text(L"게임이 실행되지 않았습니다");v.why=i18n::text(L"'게임 실행'을 누르거나 게임을 직접 켜세요. 게임 창이 뜨면 여기에 실시간으로 나옵니다.");}
        else if(st==dlsslive::Failed||st==dlsslive::Minimized||st==dlsslive::Ended){v.title=i18n::text(L"게임 화면을 받지 못했습니다");v.why=why+L"\n"+i18n::text(L"게임을 최소화하지 말고, 진짜 전체 화면이면 테두리 없는 창 모드로 바꿔 보세요.");}
        else{v.title=i18n::text(L"게임 화면을 연결하는 중…");}return v;}
    if(mode==1){if(!baseBmp){v.title=i18n::text(L"저장한 화면이 없습니다");v.why=i18n::text(L"이 화면을 누르면 지금 화면을 저장합니다. 그 뒤 설정을 바꾸고 같은 장면에서 비교하세요.");return v;}
        v.a=baseBmp;v.b=liveBmp;v.la=i18n::text(L"저장한 화면");v.lb=paused?i18n::text(L"지금 화면 · 일시정지"):i18n::text(L"지금 화면");return v;}
    v.a=liveBmp;v.la=paused?i18n::text(L"지금 화면 · 일시정지"):i18n::text(L"지금 화면");return v;}
// Screen rectangle of the picture inside the viewer client rc.
Gdiplus::RectF place(RECT rc,Gdiplus::Bitmap* img){double iw=img->GetWidth(),ih=img->GetHeight(),fit=std::min((rc.right)/iw,(rc.bottom)/ih),s=cmp.zoom>0?cmp.zoom:fit;
    if(cmp.zoom<=0){cmp.cx=iw/2;cmp.cy=ih/2;}double w=iw*s,h=ih*s;return Gdiplus::RectF((Gdiplus::REAL)(rc.right/2.0-cmp.cx*s),(Gdiplus::REAL)(rc.bottom/2.0-cmp.cy*s),(Gdiplus::REAL)w,(Gdiplus::REAL)h);}
const dlsslib::Game* current();
// 미리보기(예상) with more than one picture: the picture sits inside side and bottom margins that hold the ‹ › buttons
// and one dot per picture (white; the shown one a white disc with a cobalt centre).
bool gallery(const Shown& v){return mode==3&&picCount>1&&v.a;}
RECT picArea(RECT rc,bool gal){return gal?RECT{8,0,rc.right-8,rc.bottom-24}:rc;}
POINT arrowAt(RECT rc,int side){RECT pa=picArea(rc,true);return {side<0?28:rc.right-28,std::max<int>((pa.top+pa.bottom)/2,74)};} // below the corner button in a short viewer
int dotX(RECT rc,int i){return rc.right/2-(picCount-1)*11+i*22;}
int dotY(RECT rc){return rc.bottom-20;}
// What a click at p hits in the gallery chrome: -1 / +1 for the arrows, 10+i for dot i, 0 for nothing.
// The enlarge / back button in the viewer's top-right corner.
RECT bigButton(RECT rc){return {rc.right-47,11,rc.right-9,49};}
int galleryHit(RECT rc,POINT p){for(int side:{-1,1}){POINT c=arrowAt(rc,side);if((p.x-c.x)*(p.x-c.x)+(p.y-c.y)*(p.y-c.y)<=22*22)return side;}
    if(std::abs(p.y-dotY(rc))<=11){for(int i=0;i<picCount;i++){if(std::abs(p.x-dotX(rc,i))<=10)return 10+i;}}return 0;}
// While the split line is dragged both pictures are scaled once into bitmaps and only copied on each move: rescaling
// them with bicubic on every mouse move lagged in a maximised window. Dropped when the drag ends (live frames are not cached).
struct Scaled{Gdiplus::Bitmap* src=nullptr;int w=0,h=0;Gdiplus::RectF r;HBITMAP bmp=nullptr;};Scaled scaled[2];
void dropScaled(){for(auto& c:scaled){if(c.bmp){DeleteObject(c.bmp);}c=Scaled{};}}
void blitScaled(HDC to,HDC ref,int i,Gdiplus::Bitmap* src,RECT pa,Gdiplus::RectF r,int x0,int x1){Scaled& c=scaled[i];int w=pa.right-pa.left,h=pa.bottom-pa.top;
    if(!(c.bmp&&c.src==src&&c.w==w&&c.h==h&&c.r.Equals(r))){if(c.bmp){DeleteObject(c.bmp);}c=Scaled{src,w,h,r,CreateCompatibleBitmap(ref,w,h)};
        HDC t=CreateCompatibleDC(ref);auto o=SelectObject(t,c.bmp);RECT all={0,0,w,h};HBRUSH dark=CreateSolidBrush(RGB(5,9,20));FillRect(t,&all,dark);DeleteObject(dark);
        {Gdiplus::Graphics g(t);g.SetInterpolationMode(cmp.zoom>=2?Gdiplus::InterpolationModeNearestNeighbor:Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);g.DrawImage(src,r);}
        SelectObject(t,o);DeleteDC(t);}
    HDC t=CreateCompatibleDC(ref);auto o=SelectObject(t,c.bmp);BitBlt(to,pa.left+x0,pa.top,x1-x0,h,t,x0,0,SRCCOPY);SelectObject(t,o);DeleteDC(t);}
// The viewer's rounded edge: straight sides with a GDI pen and only the four corners through GDI+. One GDI+ stroke around
// the whole viewer made GDI+ read back and rewrite the full window on every paint (most of a frame while dragging).
void viewerEdge(HDC d,RECT r,COLORREF c,int rad){HPEN p=CreatePen(PS_SOLID,1,c);auto o=SelectObject(d,p);
    MoveToEx(d,r.left+rad,r.top,nullptr);LineTo(d,r.right-rad,r.top);MoveToEx(d,r.left+rad,r.bottom-1,nullptr);LineTo(d,r.right-rad,r.bottom-1);
    MoveToEx(d,r.left,r.top+rad,nullptr);LineTo(d,r.left,r.bottom-rad);MoveToEx(d,r.right-1,r.top+rad,nullptr);LineTo(d,r.right-1,r.bottom-rad);SelectObject(d,o);DeleteObject(p);
    for(int i=0;i<4;i++){int x=i%2?r.right-rad:r.left,y=i/2?r.bottom-rad:r.top;HRGN cl=CreateRectRgn(x,y,x+rad,y+rad);SelectClipRgn(d,cl);painting::outline(d,r,c,rad);SelectClipRgn(d,nullptr);DeleteObject(cl);}}
LRESULT CALLBACK viewerProc(HWND h,UINT m,WPARAM w,LPARAM l){
    RECT rc;GetClientRect(h,&rc);Shown v=shown();bool both=v.a&&v.b,any=v.a!=nullptr,gal=gallery(v);
    RECT pa=picArea(rc,gal),ps={0,0,pa.right-pa.left,pa.bottom-pa.top}; // the picture's area in the viewer, and its size
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps2;HDC dc=BeginPaint(h,&ps2);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,rc.right,rc.bottom);auto o=SelectObject(mem,bmp);
        HBRUSH dark=CreateSolidBrush(RGB(5,9,20));FillRect(mem,&rc,dark);DeleteObject(dark);
        auto pill=[&](const std::wstring& t,bool right){if(mode==3&&!right){RECT n={16,14,rc.right-60,40};RECT sh=n;OffsetRect(&sh,1,1);
                textAt(mem,t,sh,smallFont,RGB(0,0,0),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);textAt(mem,t,n,smallFont,RGB(226,232,244),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);return;}SIZE s={};auto of=SelectObject(mem,smallBold);GetTextExtentPoint32W(mem,t.c_str(),(int)t.size(),&s);SelectObject(mem,of);int maxW=right||mode!=3?rc.right/2-40:rc.right-47-12-9; /* the game name runs up to the enlarge button */if(s.cx+22>maxW){s.cx=maxW-22;}RECT p={right?pa.right-s.cx-82:(mode==3?9:pa.left+12),12,0,48};p.right=p.left+s.cx+22;if(mode==3){p.bottom=49;p.top=11;}else{p.bottom=40;}roundBox(mem,p,RGB(10,16,32),line,p.bottom-p.top);textAt(mem,t,{p.left+11,p.top,p.right-11,p.bottom},smallBold,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);};
        if(any){bool fast=both&&cmp.drag==1&&v.a!=liveBmp&&v.b!=liveBmp;auto r0=place(ps,v.a);
            if(fast){int sx=(int)std::lround(ps.right*cmp.split);blitScaled(mem,dc,0,v.a,pa,r0,0,sx);blitScaled(mem,dc,1,v.b,pa,r0,sx,ps.right);}
            {Gdiplus::Graphics g(mem);g.SetInterpolationMode(cmp.zoom>=2?Gdiplus::InterpolationModeNearestNeighbor:Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
            g.TranslateTransform((float)pa.left,(float)pa.top);g.SetClip(Gdiplus::RectF(0,0,(float)ps.right,(float)ps.bottom));
            auto r=r0;
            if(both){float sx=(float)(ps.right*cmp.split);if(!fast){g.SetClip(Gdiplus::RectF(0,0,sx,(float)ps.bottom));g.DrawImage(v.a,r);
                g.SetClip(Gdiplus::RectF(sx,0,(float)ps.right-sx,(float)ps.bottom));g.DrawImage(v.b,r);}g.ResetClip();
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen pen(Gdiplus::Color(235,255,255,255),2);g.DrawLine(&pen,sx,0.f,sx,(float)ps.bottom);
                Gdiplus::SolidBrush knob(painting::color(blue));float cy=ps.bottom/2.f;g.FillEllipse(&knob,sx-15,cy-15,30.f,30.f);g.DrawEllipse(&pen,sx-15,cy-15,30.f,30.f);
                Gdiplus::SolidBrush white(Gdiplus::Color(255,255,255,255));Gdiplus::PointF la[3]={{sx-9,cy},{sx-3,cy-5},{sx-3,cy+5}},ra[3]={{sx+9,cy},{sx+3,cy-5},{sx+3,cy+5}};g.FillPolygon(&white,la,3);g.FillPolygon(&white,ra,3);}
            else g.DrawImage(v.a,r);
            g.ResetClip();g.ResetTransform();
            if(gal){g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                // ‹ › : dark discs with a thin edge and a white chevron
                for(int side:{-1,1}){POINT c=arrowAt(rc,side);Gdiplus::SolidBrush disc(Gdiplus::Color(235,14,22,44));g.FillEllipse(&disc,c.x-19.f,c.y-19.f,38.f,38.f);Gdiplus::Pen edge(painting::color(line),1.f);g.DrawEllipse(&edge,c.x-19.f,c.y-19.f,38.f,38.f);
                    Gdiplus::Pen chev(Gdiplus::Color(255,236,240,250),2.2f);painting::roundCaps(chev);float d=(float)side;Gdiplus::PointF pts[3]={{c.x-3*d,c.y-7.f},{c.x+4*d,(float)c.y},{c.x-3*d,c.y+7.f}};g.DrawLines(&chev,pts,3);}
                // dots: white; the shown one a larger white disc with a cobalt centre
                for(int i=0;i<picCount;i++){float x=(float)dotX(rc,i),y=(float)dotY(rc);
                    if(i==picIdx){Gdiplus::SolidBrush wd(Gdiplus::Color(255,255,255,255));g.FillEllipse(&wd,x-6.f,y-6.f,12.f,12.f);Gdiplus::SolidBrush bd(painting::color(blue));g.FillEllipse(&bd,x-3.f,y-3.f,6.f,6.f);}
                    else{Gdiplus::SolidBrush wd(Gdiplus::Color(150,255,255,255));g.FillEllipse(&wd,x-3.5f,y-3.5f,7.f,7.f);}}}}
            pill(v.la,false);if(both&&mode!=3){pill(v.lb,true);}
            if(!v.hint.empty()){int iy=pa.top+(int)(r0.Y+r0.Height)+10;iy=std::min(iy,(int)pa.bottom-34);RECT n={pa.left+16,iy,pa.right-16,iy+28},sh=n; /* just under the picture */OffsetRect(&sh,1,1);textAt(mem,v.hint,sh,boldFont,RGB(0,0,0),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);textAt(mem,v.hint,n,boldFont,RGB(236,242,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);} // 미리보기(예상): the mode button already says so
            {RECT b=bigButton(rc);Gdiplus::Graphics g(mem);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::SolidBrush disc(Gdiplus::Color(235,14,22,44));g.FillEllipse(&disc,(float)b.left,(float)b.top,38.f,38.f);Gdiplus::Pen edge(painting::color(line),1.f);g.DrawEllipse(&edge,(float)b.left,(float)b.top,38.f,38.f);
                Gdiplus::Pen pen(Gdiplus::Color(255,236,240,250),2.f);painting::roundCaps(pen);
                // four corner brackets: pointing out = enlarge, pointing in = back to the page
                float l=b.left+12.f,t=b.top+12.f,r2=b.right-12.f,bt=b.bottom-12.f,k=5.f;
                for(int c=0;c<4;c++){float x=c%2?r2:l,y=c/2?bt:t,dx=c%2?-1.f:1.f,dy=c/2?-1.f:1.f;if(bigView){Gdiplus::PointF q[3]={{x+dx*k,y},{x+dx*k,y+dy*k},{x,y+dy*k}};g.DrawLines(&pen,q,3);}
                    else{Gdiplus::PointF q[3]={{x,y+dy*k},{x,y},{x+dx*k,y}};g.DrawLines(&pen,q,3);}}}
            // Bottom strip: what the picture is. Baseline vs live are different moments, so that is always said.
            std::wstring why;auto st=livecap.state(&why);auto* game=current();std::wstring note;COLORREF fill=RGB(10,16,32),edge=line,text=ink;
            if(mode==1){note=i18n::text(L"왼쪽은 저장한 화면, 오른쪽은 지금 화면입니다. 같은 장면에서 비교하세요.");if(liveSame<.5)note=i18n::text(L"지금 장면이 저장한 화면과 많이 다릅니다. 같은 장면으로 돌아가 비교하세요.");fill=RGB(60,40,12);edge=RGB(140,100,30);text=RGB(255,214,140);}
            else if(mode==2)note=i18n::text(L"같은 순간의 DLSS 5 적용 전(왼쪽)과 후(오른쪽)입니다.");
            else if(mode==3){} // the label on the picture already says 미리보기(예상)
            else if(st==dlsslive::Minimized||st==dlsslive::Failed||st==dlsslive::Ended){note=why;fill=RGB(60,40,12);edge=RGB(140,100,30);text=RGB(255,214,140);}
            else if(game)note=applyText(*game);
            if(!note.empty()){RECT n={12,rc.bottom-40,rc.right-12,rc.bottom-12};roundBox(mem,n,fill,edge,12);textAt(mem,note,{n.left+12,n.top,n.right-12,n.bottom},smallFont,text,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}}
        else if(v.loading){RECT k=rc;InflateRect(&k,-24,-24);painting::skeleton(mem,k,16,k.left,k.right,RGB(17,29,58),RGB(96,128,200));}
        else{RECT t=rc;InflateRect(&t,-40,-30);textAt(mem,v.title,{t.left,t.top,t.right,rc.bottom/2-6},sectionFont,ink,DT_CENTER|DT_BOTTOM|DT_SINGLELINE|DT_END_ELLIPSIS);
            textAt(mem,v.why,{t.left,rc.bottom/2+6,t.right,t.bottom},smallFont,muted,DT_CENTER|DT_TOP|DT_WORDBREAK);}
        viewerEdge(mem,rc,line,16);
        BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);SelectObject(mem,o);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps2);return 0;}
    case WM_LBUTTONDOWN:{{RECT b=bigButton(rc);POINT c={(short)LOWORD(l),(short)HIWORD(l)};if(any&&PtInRect(&b,c)){if(page)PostMessageW(page,WM_COMMAND,346,0);return 0;}}
        if(!any){if(mode==1&&liveBmp&&page)PostMessageW(page,WM_COMMAND,343,0);return 0;} /* empty 비교 view: a click saves the screen */
        if(gal){int hit=galleryHit(rc,{(short)LOWORD(l),(short)HIWORD(l)});if(hit){showPic(hit>=10?hit-10:picIdx+hit);return 0;}}
        int x=((int)(short)LOWORD(l))-pa.left;cmp.drag=both&&std::abs(x-(int)(ps.right*cmp.split))<=16?1:(cmp.zoom>0?2:(both?1:0));if(cmp.drag==1)cmp.split=std::clamp(x/(double)ps.right,0.0,1.0);cmp.last={x,((int)(short)HIWORD(l))-pa.top};SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_MOUSEMOVE:{int x=((int)(short)LOWORD(l))-pa.left,y=((int)(short)HIWORD(l))-pa.top;
        RECT bb=bigButton(rc);POINT at={x+pa.left,y+pa.top};
        if(any)SetCursor(LoadCursor(nullptr,!cmp.drag&&(PtInRect(&bb,at)||(gal&&galleryHit(rc,at)))?IDC_HAND:cmp.drag==2?IDC_SIZEALL:(both&&std::abs(x-(int)(ps.right*cmp.split))<=16)||cmp.drag==1?IDC_SIZEWE:IDC_ARROW));
        if(cmp.drag==1){cmp.split=std::clamp(x/(double)ps.right,0.0,1.0);InvalidateRect(h,nullptr,FALSE);}else if(cmp.drag==2){cmp.cx-=(x-cmp.last.x)/cmp.zoom;cmp.cy-=(y-cmp.last.y)/cmp.zoom;InvalidateRect(h,nullptr,FALSE);}cmp.last={x,y};return 0;}
    case WM_LBUTTONUP:{bool was=cmp.drag==1;cmp.drag=0;ReleaseCapture();dropScaled();if(was){InvalidateRect(h,nullptr,FALSE);}return 0;}
    case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT&&any)return TRUE;break;
    case WM_MOUSEWHEEL:{if(!any)return 0;POINT p={((int)(short)LOWORD(l)),((int)(short)HIWORD(l))};ScreenToClient(h,&p);p.x-=pa.left;p.y-=pa.top;auto r=place(ps,v.a);double s=r.Width/v.a->GetWidth(),fit=std::min(ps.right/(double)v.a->GetWidth(),ps.bottom/(double)v.a->GetHeight());
        double ix=(p.x-r.X)/s,iy=(p.y-r.Y)/s,ns=std::clamp(s*(GET_WHEEL_DELTA_WPARAM(w)>0?1.25:0.8),fit,8.0);
        if(ns<=fit*1.001){cmp.zoom=0;}else{cmp.zoom=ns;cmp.cx=ix-(p.x-ps.right/2.0)/ns;cmp.cy=iy-(p.y-ps.bottom/2.0)/ns;}InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_LBUTTONDBLCLK:{if(!any)return 0;{RECT b=bigButton(rc);POINT c={(short)LOWORD(l),(short)HIWORD(l)};if(PtInRect(&b,c)){return 0;}}if(gal&&galleryHit(rc,{(short)LOWORD(l),(short)HIWORD(l)})){return 0;}if(cmp.zoom>0)cmp.zoom=0;else{auto r=place(ps,v.a);double s=r.Width/v.a->GetWidth();cmp.cx=(((int)(short)LOWORD(l))-pa.left-r.X)/s;cmp.cy=(((int)(short)HIWORD(l))-pa.top-r.Y)/s;cmp.zoom=1;}InvalidateRect(h,nullptr,FALSE);return 0;}
    }return DefWindowProcW(h,m,w,l);}
// ---- Live capture management (UI thread). Started for the open game's window, stopped when the page leaves the game.
void wrapLive(){delete liveBmp;liveBmp=nullptr;if(liveW>0&&liveH>0&&liveBuf.size()>=(size_t)liveW*liveH*4)liveBmp=new Gdiplus::Bitmap(liveW,liveH,liveW*4,PixelFormat32bppRGB,liveBuf.data());}
void stopLive(){livecap.stop();liveWin=nullptr;livePid=0;lastPidScan=0;gameUp=false;paused=false;delete liveBmp;liveBmp=nullptr;liveBuf.clear();liveW=liveH=0;}
// Finding the game's process walks every process, so it runs at most every 3 s; a game already found is only checked
// for still running.

DWORD gamePid(const dlsslib::Game& g){if(livePid){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,livePid);DWORD code=0;bool alive=p&&GetExitCodeProcess(p,&code)&&code==STILL_ACTIVE;if(p)CloseHandle(p);if(alive)return livePid;}
    ULONGLONG now=GetTickCount64();if(lastPidScan&&now-lastPidScan<3000)return 0;lastPidScan=now;return runningPid(g);}
void manageLive(){auto* g=current();DWORD pid=g?gamePid(*g):0;HWND w=pid?wgc::window(pid):nullptr;
    if(!w){if(gameUp||livecap.running()){stopLive();if(viewer)InvalidateRect(viewer,nullptr,FALSE);}}
    else{gameUp=true;livePid=pid;if(w!=liveWin||(!livecap.running()&&GetTickCount64()>=liveRetry)){livecap.start(w,page,WM_APP+193);liveWin=w;liveRetry=GetTickCount64()+10000;}}
    // 게임 실행 (on the game's screen) only while the game is not running.
    if(page&&view==1){HWND run=GetDlgItem(page,312);if(run&&(IsWindowVisible(run)!=0)==gameUp)ShowWindow(run,gameUp||mode!=0?SW_HIDE:SW_SHOW);}}
// A new frame (or a state change) from the capture thread.
void onLiveFrame(){if(!paused){delete liveBmp;liveBmp=nullptr;livecap.latest(liveBuf,liveW,liveH,liveSeq);wrapLive();}if(viewer&&IsWindowVisible(viewer))InvalidateRect(viewer,nullptr,FALSE);}
// The baseline: the live picture now, saved for this game, with the GPU readings and frame rate at that moment.
void setStatus(const std::wstring& s,bool bad);
void takeBaseline(){auto* g=current();if(!g)return;
    if(!liveBmp){setStatus(i18n::text(L"게임 화면이 나올 때 저장할 수 있습니다."),true);return;}
    if(!savePng(basePath(*g),liveW,liveH,liveW*4,liveBuf.data())){setStatus(i18n::text(L"캡처를 저장하지 못했습니다."),true);return;}
    drop(baseBmp);baseBmp=gameart::load(basePath(*g));baseSnap.ok=live.ok;baseSnap.gpu=live;baseSnap.fps=livecap.stats();mode=1;cmp.zoom=0;}
// ---- 근사 미리보기 (mode 3, dlss_approx.h): the filter run on a saved picture, never presented as DLSS 5. The picture:
// the baseline capture, else the same-frame "before" picture, else a snapshot of the live view (shrunk to <= 960 wide).
// Recomputed at most every 100 ms (timer 3) after a preset or a slider changes.
// Faces in the preview picture, found on a worker thread (Windows face detection takes tens of ms, more the first time);
// the picture is shown at once and redrawn with the face pass when they arrive (WM_APP+197, matched by generation).
dlssapprox::Faces apxFaces;unsigned apxFaceGen=0;
void apxFindFaces(){apxFaces=dlssapprox::Faces{};unsigned gen=++apxFaceGen;struct Job{HWND page;unsigned gen;int w,h;std::vector<unsigned char> px;};
    auto* j=new Job{page,gen,apxW,apxH,apxSrc};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID a)->DWORD{auto* j=(Job*)a;std::vector<unsigned char> gray((size_t)j->w*j->h);for(size_t i=0;i<gray.size();i++){const unsigned char* q=&j->px[i*4];gray[i]=(unsigned char)((q[0]*29+q[1]*150+q[2]*77)>>8);}
        bool ro=SUCCEEDED(RoInitialize(RO_INIT_MULTITHREADED));auto boxes=dlssface::detect(gray.data(),j->w,j->h);if(ro){RoUninitialize();}
        auto* r=new dlssapprox::Faces(dlssapprox::prepare(j->px,j->w,j->h,boxes)); /* skin is found by colour even when no face box */
        if(!PostMessageW(j->page,WM_APP+197,j->gen,(LPARAM)r)){delete r;}delete j;return 0;},j,0,nullptr);if(t){CloseHandle(t);}else{delete j;}}
void apxReset(){delete apxSrcBmp;apxSrcBmp=nullptr;delete apxBmp;apxBmp=nullptr;apxSrc.clear();apxOut.clear();apxW=apxH=0;}
std::wstring shotFile(const dlsslib::Game& g,int i=0);std::wstring shotMark(const dlsslib::Game& g);void fetchShot(const dlsslib::Game& g);
// 미리보기(예상) pictures, at most six (more dots and arrows than that read as clutter under the picture): the saved screen
// (or the same-frame "before"), then the store screenshots (Steam, else IGDB through the relay). None: the live picture.
// picIdx is the one shown (the viewer's arrows and dots move it).
bool apxLoad(){auto* g=current();Gdiplus::Bitmap* from=nullptr;std::unique_ptr<Gdiplus::Bitmap> shot;
    struct Pic {Gdiplus::Bitmap* bmp;std::wstring file,label;};std::vector<Pic> pics;
    if(baseBmp){pics.push_back({baseBmp,L"",i18n::text(L"저장한 화면")});}else if(cmp.a){pics.push_back({cmp.a,L"",i18n::text(L"DLSS 5 전 (같은 순간)")});}
    if(g){std::wstring src=wideOf(readFile(shotMark(*g),16));std::wstring label=src.rfind(L"igdb",0)==0?i18n::text(L"IGDB 스크린샷"):i18n::text(L"스팀 스크린샷");
        for(int i=0;i<maxPics&&(int)pics.size()<maxPics;i++){std::wstring f=shotFile(*g,i);if(GetFileAttributesW(f.c_str())!=INVALID_FILE_ATTRIBUTES)pics.push_back({nullptr,f,label});}
        fetchShot(*g);} // once per game (it keeps a mark): more pictures may arrive and refresh the preview
    picCount=(int)pics.size();
    if(picCount){picIdx=std::clamp(picIdx,0,picCount-1);auto& p=pics[picIdx];if(p.bmp){from=p.bmp;}else{shot.reset(gameart::load(p.file));from=shot.get();}apxFrom=p.label;}
    else if(liveBmp){from=liveBmp;apxFrom=i18n::text(L"지금 화면");}
    if(!from||from->GetWidth()<16)return false;
    double k=std::min(1.0,960.0/from->GetWidth());int w=std::max(1,(int)(from->GetWidth()*k)),h=std::max(1,(int)(from->GetHeight()*k));
    Gdiplus::Bitmap small(w,h,PixelFormat32bppRGB);{Gdiplus::Graphics g(&small);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);g.DrawImage(from,0,0,w,h);}
    Gdiplus::BitmapData d;Gdiplus::Rect all(0,0,w,h);if(small.LockBits(&all,Gdiplus::ImageLockModeRead,PixelFormat32bppRGB,&d)!=Gdiplus::Ok)return false;
    apxSrc.resize((size_t)w*h*4);for(int y=0;y<h;y++)memcpy(&apxSrc[(size_t)y*w*4],(BYTE*)d.Scan0+(size_t)y*d.Stride,(size_t)w*4);small.UnlockBits(&d);
    for(size_t i=3;i<apxSrc.size();i+=4){apxSrc[i]=255;}apxW=w;apxH=h;apxSrcBmp=new Gdiplus::Bitmap(w,h,w*4,PixelFormat32bppRGB,apxSrc.data());apxFindFaces();return true;}
// Store screenshots, fetched once per game in the background into dlss\covers\<game>-shot.jpg, -shot1.jpg … -shot5.jpg:
// the Steam store page's (store.steampowered.com/api/appdetails), else IGDB's through our relay (/shots, relay/cover.js).
// <game>-shots.txt marks it done and says where they came from ("steam", "igdb", "none"); a failed connection leaves no
// mark, so it is tried again. Only https Steam / IGDB image hosts are accepted, and each file must open as a picture.
std::wstring shotFile(const dlsslib::Game& g,int i){return root()+L"\\covers\\"+keyOf(g)+L"-shot"+(i?std::to_wstring(i):L"")+L".jpg";}
std::wstring shotMark(const dlsslib::Game& g){return root()+L"\\covers\\"+keyOf(g)+L"-shots.txt";}
std::wstring urlPart(const std::wstring& s);
void fetchShot(const dlsslib::Game& g){if(shotJob||GetFileAttributesW(shotMark(g).c_str())!=INVALID_FILE_ATTRIBUTES)return;
    std::wstring id(g.e.id.begin(),g.e.id.end());bool steam=g.e.platform=="steam"&&!id.empty()&&std::all_of(id.begin(),id.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';});
    CreateDirectoryW((root()+L"\\covers").c_str(),nullptr);shotJob=true;if(page){SetTimer(page,4,30,nullptr);}struct Job{HWND page;std::wstring steam,name,mark;std::vector<std::wstring> files;};auto* j=new Job{page,steam?id:L"",g.e.name,shotMark(g),{}};
    for(int i=0;i<maxPics;i++){j->files.push_back(shotFile(g,i));}
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(Job*)p;
        // Downloads the listed pictures into the files (in order); returns how many arrived.
        auto save=[&](const std::vector<std::wstring>& urls){int n=0;for(auto& w:urls){if(n>=(int)j->files.size())break;
                bool host=w.rfind(L"https://",0)==0&&(w.find(L".steamstatic.com/")!=std::wstring::npos||w.find(L".akamaihd.net/")!=std::wstring::npos||w.rfind(L"https://images.igdb.com/",0)==0);
                if(!host||!dlssopti::download(w,j->files[n],8u<<20).empty()){continue;}Gdiplus::Bitmap* b=gameart::load(j->files[n]);if(b){delete b;n++;PostMessageW(j->page,WM_APP+196,0,0);}else DeleteFileW(j->files[n].c_str());}
            return n;};
        int got=0;bool answered=j->steam.empty(); /* Steam answered (or there is no Steam page to ask) */
        if(!j->steam.empty()){std::wstring json=j->mark+L".json";
            if(dlssopti::download(L"https://store.steampowered.com/api/appdetails?appids="+j->steam+L"&filters=screenshots",json,4u<<20).empty()){answered=true;
                std::string s=readFile(json,4u<<20);DeleteFileW(json.c_str());std::vector<std::wstring> urls;
                for(size_t a=s.find("\"path_full\":\"");a!=std::string::npos&&urls.size()<j->files.size();a=s.find("\"path_full\":\"",a)){a+=13;size_t b=s.find('"',a);if(b==std::string::npos)break;
                    std::string u=s.substr(a,b-a),url;for(size_t i=0;i<u.size();i++){if(u[i]=='\\'&&i+1<u.size()&&u[i+1]=='/')continue;url+=u[i];}urls.push_back(wideOf(url));a=b;}
                got=save(urls);if(got){writeFile(j->mark,"steam");}}}
        if(!got){std::wstring list=j->mark+L".list",e=dlssopti::download(L"https://"+std::wstring(updateHost)+L"/shots?name="+urlPart(j->name)+(j->steam.empty()?L"":L"&steam="+j->steam),list,64u<<10);
            if(e.empty()){std::string s=readFile(list,64u<<10);std::vector<std::wstring> urls;size_t a=0;while(a<s.size()){size_t b=s.find('\n',a);if(b==std::string::npos)b=s.size();if(b>a)urls.push_back(wideOf(s.substr(a,b-a)));a=b+1;}
                got=save(urls);if(got){writeFile(j->mark,"igdb");}}
            else if(e.find(L"HTTP 410")!=std::wstring::npos&&answered){writeFile(j->mark,"none");} /* neither has any */
            DeleteFileW(list.c_str());}
        shotJob=false;PostMessageW(j->page,WM_APP+196,0,0);delete j;return 0;},j,0,nullptr);if(t)CloseHandle(t);else{shotJob=false;delete j;}}
// The knobs to imitate: the editor's values, else the chosen preset (원본 = no change).
dlssnr::Settings apxSettings(){if(view==2)return edit;auto* g=current();dlssnr::Settings s=dlssnr::defaults();if(!g||preset==dlss::Preset::Original){s.v[0]=0;return s;}return settingsFor(*g,preset);}
void apxRefresh(){apxDirty=false;if(!apxSrcBmp&&!apxLoad()){if(viewer)InvalidateRect(viewer,nullptr,FALSE);return;}
    delete apxBmp;apxBmp=nullptr;apxOut=dlssapprox::apply(apxSrc,apxW,apxH,apxSettings(),&apxFaces);apxBmp=new Gdiplus::Bitmap(apxW,apxH,apxW*4,PixelFormat32bppRGB,apxOut.data());if(viewer)InvalidateRect(viewer,nullptr,FALSE);}
void apxChanged(){if(mode==3)apxDirty=true;}
// A game opens on 미리보기(예상) (the first mode button): the effect on a saved picture, no game needed.
int modeOf(int id){return id==345?3:id-340;}
void previewFirst(){mode=3;picIdx=0;cmp.zoom=0;cmp.split=.5;apxReset();apxRefresh();if(page){SetTimer(page,3,100,nullptr);}}
void showPic(int i){if(picCount<2)return;picIdx=(i%picCount+picCount)%picCount;cmp.zoom=0;apxReset();apxRefresh();}
// ---- The page. view 0 = choose a game (carousel or 전체 보기), 3 = the game's own screen (its cover on the left, three
// choices on the right), 1 = 화질 향상 (viewer, presets, apply), 4 = 프레임 설정, 5 = 성능 분석 (both: cover left,
// settings right), 2 = preset editor (optiscaler route).
// One game: the viewer across the top, its toolbar below. Editor: the viewer on the left (about 65 %), settings on the right.
const RECT stageRect={24,76,828,452}; // the carousel window (view 0)
const RECT viewerRect={36,82,490,392},editorView={36,62,540,346},firstView={36,422,418,582};const int panelL=560,panelR=816;
// The page can be larger than its 852 x 622 design (a bigger or maximized window: EX, EY extra). Design coordinates map
// to the page piecewise: x from sx on moves right by EX; y from the split of its column (syL left of sx, syR right of it)
// moves down by EY. What crosses a split stretches (the picture views, panels, wide text boxes, the carousel); the rest
// keeps its size. Splits sit in gaps, so no button or tile is cut.
int EX=0,EY=0;struct Warp {int sx,syL,syR,a=0,b=0;}; // a < b: x stretches evenly between a and b instead of splitting at sx
Warp warp(){if(view>=3)return {100000,100000,100000}; /* laid out from the page size itself (hubCover) */if(view==1)return {489,384,540};if(view==2)return {539,340,550};if(gridMode)return {620,530,530};return {417,300,300};} // carousel: the stage grows both ways, the name and button below move down // 프레임 설정 / 성능 분석: the one wide panel and its rows stretch evenly
RECT W(RECT r){Warp w=warp();bool right=w.b<=w.a&&r.left>=w.sx;int sy=right?w.syR:w.syL;auto X=[&](LONG x)->LONG{if(w.b>w.a){return x+(LONG)std::lround(EX*std::clamp((x-w.a)/(double)(w.b-w.a),0.0,1.0));}return x<w.sx?x:x+EX;};auto Y=[&](LONG y){return y<sy?y:y+EY;};
    return {X(r.left),Y(r.top),X(r.right-1)+1,Y(r.bottom-1)+1};}
POINT unW(POINT p){Warp w=warp();LONG x=w.b>w.a?(p.x<w.a?p.x:p.x>w.b+EX?p.x-EX:w.a+(LONG)std::lround((p.x-w.a)*(w.b-w.a)/(double)(w.b-w.a+EX))):p.x<w.sx?p.x:p.x<w.sx+EX?w.sx-1:p.x-EX;int sy=w.b<=w.a&&x>=w.sx?w.syR:w.syL;LONG y=p.y<sy?p.y:p.y<sy+EY?sy-1:p.y-EY;return {x,y};}
void inval(RECT r){RECT m=W(r);InvalidateRect(page,&m,FALSE);}
// Every child keeps its design rectangle (home) and is placed at its mapping; a few are placed in page coordinates (raw).
std::map<HWND,RECT> home;std::set<HWND> rawPlaced;
void put(HWND c,int x,int y,int w,int h){if(!c)return;home[c]={x,y,x+w,y+h};rawPlaced.erase(c);RECT m=W(home[c]);SetWindowPos(c,nullptr,m.left,m.top,m.right-m.left,m.bottom-m.top,SWP_NOZORDER|SWP_NOACTIVATE);}
void putRaw(HWND c,int x,int y,int w,int h,HWND after=nullptr){if(!c)return;rawPlaced.insert(c);SetWindowPos(c,after,x,y,w,h,(after?0:SWP_NOZORDER)|SWP_NOACTIVATE);}
// The children no layout step places (made once in WM_CREATE): recorded at their design place the first time, then
// mapped.
void warpAll(){for(HWND c=GetWindow(page,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){if(rawPlaced.count(c))continue;auto it=home.find(c);RECT now;GetWindowRect(c,&now);MapWindowPoints(nullptr,page,(POINT*)&now,2);
    if(it==home.end()){it=home.emplace(c,now).first;}RECT m=W(it->second);bool sized=m.right-m.left==now.right-now.left&&m.bottom-m.top==now.bottom-now.top;
    wchar_t cls[16]={};GetClassNameW(c,cls,16);int h=_wcsicmp(cls,L"ComboBox")?m.bottom-m.top:200; // a drop-down's height is its open list (both are made 200 high)
    if(m.left!=now.left||m.top!=now.top||!sized){SetWindowPos(c,nullptr,m.left,m.top,m.right-m.left,h,SWP_NOZORDER|SWP_NOACTIVATE|(sized?SWP_NOSIZE:0));}}}
RECT presetRect(int p){if(view==0)return {452+p*74,402,452+p*74+70,452};if(p==4){return {522,196,812,226};}return {522+(p%2)*150,124+(p/2)*36,522+(p%2)*150+140,154+(p/2)*36};} // first screen: small cards in the right panel
const int trackL=panelL,trackR=panelR-8;
// Views 3-5, in page pixels: the cover on the left (2:3, as tall as the page allows, at most 40 % of its width); on the
// right the game's name and labels at the top, then the three choices (view 3) or the settings panel (views 4, 5).
int hubHover=-1;
// Opening a game: its card flies from where it was (carousel / 전체 보기) to the cover's place on the left, turning once
// round its upright axis on the way (eased in and out over 0.8 s), while skeletons of the name, labels and the three
// choices slide in on the right; then the real content takes their place. ‹ 게임 목록 plays the same flight backwards.
// Timer 5 repaints. Everything that does not move (backdrop and panel, the glow, both card faces) is made once when the
// flight starts, so a frame is one copy and a few scaled images: the page's own paint was too slow to run every frame.
const double flyMs=800;double flyStart=0;int flyDir=1;RECT flyFrom={};Gdiplus::Bitmap *flyFront=nullptr,*flyBack=nullptr,*flyGlow=nullptr;HBITMAP flyBase=nullptr,flyFrame=nullptr;
// Flight frames are drawn into DIB sections: GDI+ on a screen-compatible bitmap took 30-75 ms a frame (no text is drawn while flying, so gdiScaling does not matter).
HBITMAP dibOf(int w,int h){BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=std::max(1,w);bi.bmiHeader.biHeight=-std::max(1,h);bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;return CreateDIBSection(nullptr,&bi,DIB_RGB_COLORS,&bits,nullptr,0);}
double nowMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();} // GetTickCount64 moves in 16 ms steps: the card jumped
bool flying(){return flyStart>0&&nowMs()-flyStart<flyMs;}
RECT hubCover(){RECT pr={0,0,852,622};if(page)GetClientRect(page,&pr);int avail=std::max(200,(int)pr.bottom-72),cw=std::min(avail*2/3,(int)(pr.right*.4)),ch=cw*3/2,y=36+(avail-ch)/2;return {36,y,36+cw,y+ch};}
RECT hubRight(){RECT pr={0,0,852,622};if(page)GetClientRect(page,&pr);RECT c=hubCover();return {c.right+36,c.top+92,pr.right-36,c.bottom};}
RECT hubCard(int i){RECT R=hubRight();int h=(R.bottom-R.top-28)/3;return {R.left,R.top+i*(h+14),R.right,R.top+i*(h+14)+h};}
int hubAt(POINT p){if(view!=3)return -1;for(int i=0;i<3;i++){RECT r=hubCard(i);if(PtInRect(&r,p))return i;}return -1;}
// Editor setting rows on the right: name and value on one line, the slider under it.
RECT knobRow(int i){return {panelL,104+i*46,panelR,104+i*46+44};}
const wchar_t* presetName(dlss::Preset p){const wchar_t* n[]={L"원본",L"균형",L"사실적",L"영화적",L"사용자 지정"};return i18n::text(n[(int)p]);}
void setStatus(const std::wstring& s,bool bad=false){status=s;statusBad=bad;if(page)InvalidateRect(page,nullptr,TRUE);}
const dlsslib::Game* current(){return pick>=0&&pick<(int)games.size()?&games[pick]:nullptr;} // view 0: the carousel's chosen game (selectChosen)
const dlsslib::Game* chosen();int loadLimit(const dlsslib::Game& g);int chipValue(int i);
// Everything is moved and shown with drawing turned off, then the page and its controls are painted once: switching views
// (carousel <-> 전체 보기, tabs) used to repaint each control on its own, which looked like stutter.
void layoutNow();void perfLayoutNow();
std::map<std::wstring,std::vector<std::pair<std::wstring,COLORREF>>> pillMemo; // labels under the carousel per game: they read files
void layout(){pillMemo.clear();bool hold=page&&IsWindowVisible(page);if(hold)SendMessageW(page,WM_SETREDRAW,FALSE,0);layoutNow();
    if(hold){SendMessageW(page,WM_SETREDRAW,TRUE,0);if(!quietMoves)RedrawWindow(page,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);}}
// The enlarged picture: the viewer over the whole page, five big preset buttons (painted, bigPreset) under it.
RECT bigPreset(int p){RECT pr;GetClientRect(page,&pr);int w=(pr.right-56-4*14)/5,x=28+p*(w+14),y=pr.bottom-28-68;return {x,y,x+w,y+68};}
void layoutNow(){auto* g=current();bool o=g&&opti(*g);
    if(bigView&&!(view>=1||(view==0&&tab==0&&!gridMode))){bigView=false;}
    if(bigView){for(HWND c=GetWindow(page,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){if(c!=viewer){ShowWindow(c,SW_HIDE);}}
        RECT pr;GetClientRect(page,&pr);putRaw(viewer,28,28,pr.right-56,view==2?pr.bottom-56:pr.bottom-28-68-28-20); /* the editor has no preset row: its sliders are the preset */ShowWindow(viewer,SW_SHOW);InvalidateRect(viewer,nullptr,FALSE);return;}

    for(int id:{312,313,314,315})ShowWindow(GetDlgItem(page,id),view==1?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(page,300),view==3&&!flying()?SW_SHOW:SW_HIDE);ShowWindow(GetDlgItem(page,301),view==1||view==4||view==5?SW_SHOW:SW_HIDE);
    if(view>=3){RECT pr;GetClientRect(page,&pr);for(int id:{300,301}){putRaw(GetDlgItem(page,id),pr.right-36-150,view==5?20:hubCover().top,150,40);}}
    perfLayoutNow();
    for(int id:{316,317}){ShowWindow(GetDlgItem(page,id),view==1&&o?SW_SHOW:SW_HIDE);}
    // First screen: 캐러셀 / 전체 보기, the carousel arrows and 게임 열기 under the chosen game.
    {int n=(int)listed().size();bool first=view==0,loading=scanning&&!n,grid=first&&(gridMode||(!n&&!loading)),car=first&&!grid&&(n>0||loading);
        for(int id:{320,321,322}){ShowWindow(GetDlgItem(page,id),grid?SW_SHOW:SW_HIDE);}
        for(int i=0;i<3;i++){ShowWindow(GetDlgItem(page,350+i),SW_HIDE);}
        for(int id:{353,358}){putRaw(GetDlgItem(page,id),(id==353?560:692)+EX,28,124,40);ShowWindow(GetDlgItem(page,id),first&&(n>0||loading)?SW_SHOW:SW_HIDE);SetWindowLongPtrW(GetDlgItem(page,id),GWLP_USERDATA,(id==358)==gridMode?1:0);}
        for(int id:{371,372})ShowWindow(GetDlgItem(page,id),grid&&n>0&&!showHidden?SW_SHOW:SW_HIDE);
        {HWND e=GetDlgItem(page,370);ShowWindow(e,grid&&n>0&&!showHidden&&(GetWindowTextLengthW(e)>0||GetFocus()==e)?SW_SHOW:SW_HIDE);}
        if(stage){ShowWindow(stage,car?SW_SHOW:SW_HIDE);SetWindowPos(stage,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);if(car)InvalidateRect(stage,nullptr,FALSE);}
        for(int id:{356,357})ShowWindow(GetDlgItem(page,id),car&&n>1?SW_SHOW:SW_HIDE);
        ShowWindow(GetDlgItem(page,354),car&&n>0?SW_SHOW:SW_HIDE);ShowWindow(GetDlgItem(page,355),SW_HIDE);SetDlgItemTextW(page,354,i18n::text(L"게임 열기"));{RECT pr;GetClientRect(page,&pr);RECT m=W({0,536,0,576}),st=W(stageRect);putRaw(GetDlgItem(page,354),(pr.right-180)/2,m.top,180,40);
            putRaw(GetDlgItem(page,356),40,(st.top+st.bottom)/2-22,44,44);putRaw(GetDlgItem(page,357),pr.right-84,(st.top+st.bottom)/2-22,44,44);}
        // 프레임 설정 (view 4): the limit choices in the right panel, two to a row.
        auto* c=current();bool o=c&&opti(*c);int lim=c?loadLimit(*c):0;RECT R=hubRight();int cw=(R.right-R.left-48-12)/2;
        for(int i=0;i<6;i++){HWND b=GetDlgItem(page,360+i);int v=chipValue(i);bool show=view==4&&(i<5||(v>0&&v!=60&&v!=90&&v!=120&&v!=144));putRaw(b,R.left+24+(i%2)*(cw+12),R.top+76+(i/2)*56,cw,44);ShowWindow(b,show?SW_SHOW:SW_HIDE);EnableWindow(b,o&&!working);SetWindowLongPtrW(b,GWLP_USERDATA,o&&lim==v?1:0);
            if(i==5&&v>0){wchar_t t[64];swprintf(t,64,i18n::text(L"모니터 %d Hz"),v);SetWindowTextW(b,t);}}}
    for(int id:{330,331,332,333,334,335,336,337}){ShowWindow(GetDlgItem(page,id),view==2?SW_SHOW:SW_HIDE);}
    // Viewer toolbar (live / baseline vs live / same frame, baseline capture, pause): under the viewer in both views.
    RECT vr=view==2?editorView:viewerRect;bool firstPanels=false;put(viewer,vr.left,vr.top,vr.right-vr.left,vr.bottom-vr.top);
    // Modes on the left (라이브, 기준 vs 라이브, 같은 프레임, 근사 미리보기), 기준 캡처 and 일시정지 on the right (in a row in the editor).
    if(view==0||view>=3){for(int i=0;i<6;i++)ShowWindow(GetDlgItem(page,340+i),SW_HIDE);}
    else if(view==1){const int ids[6]={345,340,341,342,343,344};RECT V=W(viewerRect);int bw=((V.right-V.left)-20)/3; // the modes share the viewer's width, two rows under it
        for(int i=0;i<6;i++){HWND c=GetDlgItem(page,ids[i]);putRaw(c,V.left+(i%3)*(bw+10),V.bottom+10+(i/3)*48,bw,40);ShowWindow(c,SW_SHOW);if(i<4)SetWindowLongPtrW(c,GWLP_USERDATA,mode==modeOf(ids[i])?1:0);}
        SetDlgItemTextW(page,344,paused?i18n::text(L"재개"):i18n::text(L"일시정지"));SetPropW(GetDlgItem(page,344),L"BlueAsh.Icon",(HANDLE)(UINT_PTR)(paused?0xE768:0xE769));EnableWindow(GetDlgItem(page,342),o||(g&&renoAvailable(*g))||(cmp.a&&cmp.b));
        // Bottom bar: the route's actions left, 적용하기 right; 게임 실행 sits on the viewer while the game is not running.
        int x=36;for(int id:{316,317,313,315}){HWND c=GetDlgItem(page,id);if((id==316||id==317)&&!o)continue;int w=id==316?180:id==317?128:id==313?128:116;put(c,x,546,w,40);x+=w+8;} /* 프리셋 만들기 ends before the split (489) so it keeps its size */
        put(GetDlgItem(page,314),666,546,150,40);put(GetDlgItem(page,301),676,28,140,38);
        HWND run=GetDlgItem(page,312);putRaw(run,(V.left+V.right)/2-70,(V.top+V.bottom)/2+52,140,40,HWND_TOP);ShowWindow(run,gameUp||mode!=0?SW_HIDE:SW_SHOW);}
    else {int y=vr.bottom+6;const int ids[6]={345,340,341,342,343,344},w1[6]={130,100,150,110,130,100},w2[6]={79,79,79,79,79,79};const int* wd=view==2?w2:w1;int gap=6,x=vr.left,right=vr.right-wd[4]-wd[5]-gap;
        for(int i=0;i<6;i++){HWND c=GetDlgItem(page,ids[i]);int at=view==2||i<4?x:(i==4?right:right+wd[4]+gap);put(c,at,y,wd[i],40);x+=wd[i]+gap;
            ShowWindow(c,view>=1?SW_SHOW:SW_HIDE);if(i<4)SetWindowLongPtrW(c,GWLP_USERDATA,mode==modeOf(ids[i])?1:0);}
        SetDlgItemTextW(page,344,paused?i18n::text(L"재개"):i18n::text(L"일시정지"));SetPropW(GetDlgItem(page,344),L"BlueAsh.Icon",(HANDLE)(UINT_PTR)(paused?0xE768:0xE769));EnableWindow(GetDlgItem(page,342),o||(g&&renoAvailable(*g))||(cmp.a&&cmp.b));}
    ShowWindow(viewer,view==1||view==2||firstPanels?SW_SHOW:SW_HIDE);SetDlgItemTextW(page,322,showHidden?i18n::text(L"숨긴 게임 닫기"):i18n::text(L"숨긴 게임 보기"));
    // Model buttons: the chosen one drawn as primary. Whether the runtime has model B is checked when applying.
    if(view==2){SetWindowLongPtrW(GetDlgItem(page,330),GWLP_USERDATA,edit.preset==1?1:0);SetWindowLongPtrW(GetDlgItem(page,331),GWLP_USERDATA,edit.preset==2?1:0);checkBox(page,332,edit.autoMask);}
    // Games not verified for DLSS 5 can be watched live, but nothing is applied or edited for them.
    {auto* c=chosen();bool ok=g?verified(*g):(c&&verified(*c));for(int id:{313,314,315,316,317,355})EnableWindow(GetDlgItem(page,id),!working&&ok);}
    // Buttons on a lighter inner box (rowFill) paint their corners in that colour (ModeSwitch.cpp buttonSurface).
    {std::set<int> row;if(view==0&&gridMode){row={320,321,322};}else if(view==4){row={360,361,362,363,364,365};}else if(view==1){row={301,313,314,315,316,317};}else if(view==6){row={400,404,414};}else if(view==5){row={385,386,387,388,389,390,391,392,393,394,395,396,397,398,399,400,401,402,403,404,405};}
        for(HWND c=GetWindow(page,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){bool on=row.count(GetDlgCtrlID(c))>0;if(on!=(GetPropW(c,L"BlueAsh.OnRow")!=nullptr)){if(on){SetPropW(c,L"BlueAsh.OnRow",(HANDLE)1);}else{RemovePropW(c,L"BlueAsh.OnRow");}InvalidateRect(c,nullptr,FALSE);}}}
    warpAll();
    if(!IsWindowVisible(page)){for(HWND c=GetWindow(page,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){InvalidateRect(c,nullptr,FALSE);}InvalidateRect(page,nullptr,TRUE);}}
void refreshRuntime(){if(GetFileAttributesW(runtimePath().c_str())==INVALID_FILE_ATTRIBUTES){runtimeInfo=dlssrt::Info();runtimeInfo.error=L"-";}else runtimeInfo=dlssrt::check(runtimePath());}
Gdiplus::Bitmap* coverFor(const dlsslib::Game& g);RECT hubCover();
Gdiplus::Bitmap* flyFace(int w,int h,Gdiplus::Bitmap* cover,bool back){auto* b=new Gdiplus::Bitmap(w,h,PixelFormat32bppPARGB);Gdiplus::Graphics g(b);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.Clear(Gdiplus::Color(0,0,0,0));
    Gdiplus::GraphicsPath path;painting::path(path,{0,0,w,h},24);
    if(back||!cover||cover->GetWidth()<16){Gdiplus::LinearGradientBrush bg(Gdiplus::Point(0,0),Gdiplus::Point(0,h),Gdiplus::Color(255,30,52,108),Gdiplus::Color(255,10,18,40));g.FillPath(&bg,&path);}
    else{g.SetClip(&path);double sw=cover->GetWidth(),sh=cover->GetHeight(),k=std::max(w/sw,h/sh);int dw=(int)(sw*k+.5),dh=(int)(sh*k+.5);g.DrawImage(cover,(w-dw)/2,(int)((h-dh)*.25f),dw,dh);g.ResetClip();}
    Gdiplus::Pen edge(Gdiplus::Color(120,110,150,255),1.5f);g.DrawPath(&edge,&path);return b;}
void endFly(){if(flyStart>0)timeEndPeriod(1);delete flyFront;delete flyBack;delete flyGlow;flyFront=flyBack=flyGlow=nullptr;for(HBITMAP* b:{&flyBase,&flyFrame}){if(*b){DeleteObject(*b);*b=nullptr;}}flyStart=0;}
void startFly(RECT from,int dir){endFly();auto* g=current();RECT c=hubCover();int w=c.right-c.left,h=c.bottom-c.top;if(!g||w<20||h<20||!page)return;
    flyFront=flyFace(w,h,coverFor(*g),false);flyBack=flyFace(w,h,nullptr,true);
    {int gw=w+60,gh=h+50;flyGlow=new Gdiplus::Bitmap(gw,gh,PixelFormat32bppPARGB);Gdiplus::Graphics o(flyGlow);o.Clear(Gdiplus::Color(0,0,0,0));Gdiplus::GraphicsPath p;p.AddEllipse(0.f,0.f,(float)gw,(float)gh);
     Gdiplus::PathGradientBrush b(&p);b.SetCenterColor(Gdiplus::Color(34,GetRValue(blue),GetGValue(blue),GetBValue(blue)));Gdiplus::Color edge(0,GetRValue(blue),GetGValue(blue),GetBValue(blue));int n=1;b.SetSurroundColors(&edge,&n);
     Gdiplus::REAL pos[]={0.f,.55f,1.f},fall[]={0.f,.7f,1.f};b.SetBlend(fall,pos,3);o.FillPath(&b,&p);}
    {RECT r;GetClientRect(page,&r);HDC dc=dpi::getDC(page);HDC mem=CreateCompatibleDC(dc);flyBase=dibOf(r.right,r.bottom);flyFrame=dibOf(r.right,r.bottom);auto old=SelectObject(mem,flyBase);
     POINT o={};MapWindowPoints(page,win,&o,1);if(!copyBackdrop(mem,r,o))FillRect(mem,&r,backgroundBrush);::roundBox(mem,{12,12,r.right-12,r.bottom-12},panel,line,20);SelectObject(mem,old);DeleteDC(mem);ReleaseDC(page,dc);}
    flyFrom=from;flyDir=dir;flyStart=nowMs();timeBeginPeriod(1);SetTimer(page,5,8,nullptr);} /* 1 ms timer resolution while flying: the default 15.6 ms made frames uneven */
// Where the chosen card is now, in page pixels: the carousel's middle card or the grid card.
RECT chosenCardRect(int index);
void backToList(){view=0;pick=-1;bigView=false;status.clear();stopLive();apxReset();layout();InvalidateRect(page,nullptr,FALSE);}
void open(int index){bool same=pick==index;{RECT from=chosenCardRect(index);bool fly=view==0&&from.right>from.left;if(fly){pick=index;startFly(from,1);}}{auto ok=listed();for(int i=0;i<(int)ok.size();i++)if(ok[i]==index){carIdx=i;carPos=carTarget=i;}}pick=index;view=3;hubHover=-1;preset=loadPreset(games[index]);status.clear();
    if(!same){apxReset();paused=false;baseSnap=Snap();stopLive();}if(runtimeInfo.version.empty()&&runtimeInfo.error.empty())refreshRuntime();loadShots();if(!same){previewFirst();}layout();manageLive();}
// Re-checks the game before anything is done for it (the files may have changed since the scan).
bool recheck(){auto* g=current();if(!g)return false;dlsslib::Game fresh;fresh.e.platform=g->e.platform;fresh.e.id=g->e.id;fresh.e.name=g->e.name;fresh.e.dir=g->e.dir;dlsslib::gather(fresh);dlsslib::findDlss(fresh);
    auto v=dlss::classify(fresh.e);if(v.status!=dlss::Status::Verified&&!(v.status==dlss::Status::Unknown&&!v.title&&!fresh.dlssDir.empty())){setStatus(i18n::text(L"안전 확인을 다시 통과하지 못해 멈췄습니다: ")+i18n::text(v.reason),true);return false;}return true;}
std::wstring runtimeLine(){if(runtimeInfo.error==L"-")return i18n::text(L"DLSS 5 런타임: 아직 가져오지 않음");if(!runtimeInfo.error.empty())return i18n::text(L"DLSS 5 런타임: 사용할 수 없음 · ")+i18n::text(runtimeInfo.error);
    std::wstring v=wideOf(runtimeInfo.version);std::wstring s=i18n::text(L"DLSS 5 런타임 ")+v+L" · NVIDIA "+i18n::text(L"서명 확인")+L" · "+(runtimeInfo.models_ab?i18n::text(L"모델 A·B"):i18n::text(L"모델 A만"));
    int a=0,b=0,c=0,x=0,y=0,z=0;std::sscanf(runtimeInfo.version.c_str(),"%d.%d.%d",&a,&b,&c);std::sscanf(knownLatest,"%d.%d.%d",&x,&y,&z);if(std::tie(a,b,c)<std::tie(x,y,z))s+=i18n::text(L" · 더 새 버전이 있습니다: ")+wideOf(knownLatest);return s;}
// Page painting below is in design coordinates: these map each rectangle (W) before drawing.
void textW(HDC dc,const std::wstring& t,RECT r,HFONT f,COLORREF c,UINT fl=DT_LEFT|DT_VCENTER|DT_SINGLELINE){::textAt(dc,t,W(r),f,c,fl);}
void boxW(HDC dc,RECT r,COLORREF fill,COLORREF edge,int radius){::roundBox(dc,W(r),fill,edge,radius);}
void glyphW(HDC dc,wchar_t g,RECT r,HFONT f,COLORREF c){::glyph(dc,g,W(r),f,c);}
std::wstring knobValue(size_t i,double v){wchar_t b[32];swprintf(b,32,dlssnr::knobs()[i].step>=.1?L"%.1f":L"%.2f",v);return b;}
void paintEditor(HDC dc){
    textW(dc,i18n::text(L"프리셋 만들기 · 사용자 지정"),{36,22,540,52},sectionFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    // The build reads its settings once, at game start: say so where the sliders are, instead of pretending they are live.
    {RECT b={panelL,24,panelR,52};boxW(dc,b,RGB(60,40,12),RGB(140,100,30),14);textW(dc,i18n::text(L"실시간 변경 미지원 · 재시작 후 적용"),{b.left+10,b.top,b.right-10,b.bottom},smallBold,RGB(255,214,140),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
    textW(dc,i18n::text(L"모델"),{panelL,62,638,102},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    for(size_t i=0;i<dlssnr::knobs().size();i++){auto& k=dlssnr::knobs()[i];RECT r=knobRow((int)i);if(hoverKnob==(int)i||dragKnob==(int)i)boxW(dc,{r.left-8,r.top,r.right+4,r.bottom},rowFill,rowFill,10);
        textW(dc,i18n::text(k.name),{r.left,r.top+2,r.right-56,r.top+22},smallBold,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        textW(dc,knobValue(i,edit.v[i]),{r.right-56,r.top+2,trackR,r.top+22},smallBold,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        int cy=r.top+32,x=trackL+(int)std::lround((edit.v[i]-k.lo)/(k.hi-k.lo)*(trackR-trackL));boxW(dc,{trackL,cy-2,trackR,cy+3},well,well,5);boxW(dc,{trackL,cy-2,x,cy+3},blue,blue,5);
        int dx=trackL+(int)std::lround((k.def-k.lo)/(k.hi-k.lo)*(trackR-trackL));boxW(dc,{dx-1,cy-7,dx+1,cy+7},muted,muted,1);painting::circle(dc,W({x-7,cy-7,x+7,cy+7}),ink,CLR_NONE);}
    if(auto* g=current())textW(dc,applyText(*g),{panelL,464,panelR,504},smallFont,applyLevel(*g)==2?RGB(120,220,160):muted,DT_LEFT|DT_TOP|DT_WORDBREAK);
    // Under the live picture: measured GPU values, the estimated frame rate, the change since the baseline.
    textW(dc,liveText(),{36,398,540,416},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    textW(dc,fpsText(),{36,416,540,434},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    textW(dc,deltaText(),{36,434,540,452},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    // The hovered setting's own description from the build's documentation, and what it costs.
    std::wstring help=hoverKnob>=0?i18n::text(dlssnr::knobs()[hoverKnob].help):i18n::text(L"막대에 마우스를 올리면 설명이 나옵니다. 세로 눈금은 기본값입니다. 값은 이 게임에만 저장됩니다.");
    textW(dc,help,{36,458,540,504},smallFont,muted,DT_LEFT|DT_TOP|DT_WORDBREAK|DT_END_ELLIPSIS);
    textW(dc,status,{36,508,816,548},smallBold,statusBad?danger:RGB(120,170,255),DT_LEFT|DT_VCENTER|DT_WORDBREAK|DT_END_ELLIPSIS);}
// ---- First screen: the game carousel.
// Card picture, in order: a cover saved in dlss\covers\<game>.jpg|png (where a cover fetcher or the person can put one;
// nothing is downloaded here yet), Steam's own library cache, the game exe's icon.
Gdiplus::Bitmap* coverFor(const dlsslib::Game& g){std::wstring k=keyOf(g);auto it=covers.find(k);if(it!=covers.end())return it->second;
    Gdiplus::Bitmap* b=nullptr;std::wstring d=root()+L"\\covers\\"+k;for(const wchar_t* e:{L".jpg",L".png"}){if(!b)b=gameart::load(d+e);}
    if(!b&&g.e.platform=="steam"){std::wstring c=gameart::steamCover(std::wstring(g.e.id.begin(),g.e.id.end()).c_str());if(!c.empty())b=gameart::load(c);}
    if(!b){b=gameart::exeIcon(g.exe);}covers[k]=b;return b;}
// The card as a picture, made once per game (200 x 300): rounded navy card, the cover cropped to fill it (an icon sits in
// the middle instead), a light shade at the bottom and a faint edge. Drawn each frame scaled and faded.
const int cardW=200,cardH=300;
Gdiplus::Bitmap* cardFor(const dlsslib::Game& g){std::wstring k=keyOf(g);auto it=cards.find(k);if(it!=cards.end())return it->second;
    auto* out=new Gdiplus::Bitmap(cardW,cardH,PixelFormat32bppPARGB);Gdiplus::Bitmap* c=coverFor(g);
    {Gdiplus::Graphics o(out);o.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);o.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);o.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);o.Clear(Gdiplus::Color(0,0,0,0));
        Gdiplus::GraphicsPath p;painting::path(p,{0,0,cardW,cardH},24);Gdiplus::LinearGradientBrush bg(Gdiplus::Point(0,0),Gdiplus::Point(0,cardH),Gdiplus::Color(255,26,46,96),Gdiplus::Color(255,10,18,40));o.FillPath(&bg,&p);
        if(c&&c->GetWidth()>15){double iw=c->GetWidth(),ih=c->GetHeight();o.SetClip(&p);
            if(ih>=iw*1.2){double k2=std::max(cardW/iw,cardH/ih);int dw=(int)(iw*k2+.5),dh=(int)(ih*k2+.5);o.DrawImage(c,(cardW-dw)/2,(int)((cardH-dh)*.3),dw,dh);}
            else{int s=(int)(cardW*.52);o.DrawImage(c,(cardW-s)/2,(cardH-s)/2-16,s,s);}o.ResetClip();}
        // Only the lower half (a gradient brush repeats past its end points).
        Gdiplus::LinearGradientBrush shade(Gdiplus::Point(0,cardH/2-1),Gdiplus::Point(0,cardH+1),Gdiplus::Color(0,5,9,20),Gdiplus::Color(140,5,9,20));o.SetClip(&p);o.FillRectangle(&shade,0,cardH/2,cardW,cardH-cardH/2);o.ResetClip();
        Gdiplus::Pen edge(Gdiplus::Color(56,255,255,255),1.f);o.DrawPath(&edge,&p);}
    cards[k]=out;return out;}
// Where card i sits when the carousel is at carPos: d = its distance from the middle (the shortest way round). Further
// cards are smaller, fainter and a little higher, on a slight arc, so the row reads as a ring turning away.
// The carousel is the whole first screen: the middle card is 200 x 300 at the design size and grows with a taller window
// (stageMetrics, from the stage window's height). f scales the ring, spacing and light with the cards.
const int carX=426;double carY=262,cardScale=1.0,carF=1.43;
HWND stageWnd();
void stageMetrics(){RECT rc={0,0,804,376};if(HWND w=stageWnd())GetClientRect(w,&rc);double hgt=std::max<LONG>(rc.bottom,376);double wid=std::max<LONG>(rc.right,804);cardScale=std::clamp(std::min(hgt*.8/300,(wid/2-70)/375),1.0,2.2); /* the second card out on each side stays inside, clear of the arrows */carF=cardScale/.7;carY=stageRect.top+hgt*.5;}
// The same card made once at the stage size of the middle card (big windows show it up to 2.2x) as plain premultiplied
// pixels, so every frame only shrinks it with our own bilinear blend (GDI+ scaling cost 30 ms a frame at full screen).
struct BigCard {int w=0,h=0;std::vector<uint32_t> px;}; // px has a one-pixel transparent border: (w+2) x (h+2)
std::map<std::wstring,BigCard> bigCards;double bigScale=0;
const BigCard& bigCard(const dlsslib::Game& g){if(bigScale!=cardScale){bigCards.clear();bigScale=cardScale;}
    std::wstring k=keyOf(g);auto it=bigCards.find(k);if(it!=bigCards.end())return it->second;BigCard& c=bigCards[k];c.w=(int)std::lround(cardW*cardScale);c.h=(int)std::lround(cardH*cardScale);c.px.assign((size_t)(c.w+2)*(c.h+2),0);
    Gdiplus::Bitmap b(c.w,c.h,(c.w+2)*4,PixelFormat32bppPARGB,(BYTE*)(c.px.data()+c.w+3));Gdiplus::Graphics t(&b);t.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);t.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);t.DrawImage(cardFor(g),Gdiplus::Rect(0,0,c.w,c.h));t.Flush(Gdiplus::FlushIntentionSync);
    return c;}
// Two 8-bit channels at a time (0x00ff00ff lanes): p towards q by w/256.
inline uint32_t mix2(uint32_t p,uint32_t q,uint32_t w){uint32_t rb=(((p&0xff00ff)*(256-w)+(q&0xff00ff)*w)>>8)&0xff00ff,ag=((((p>>8)&0xff00ff)*(256-w)+((q>>8)&0xff00ff)*w)>>8)&0xff00ff;return rb|(ag<<8);}
inline uint32_t scale2(uint32_t p,uint32_t w){return (((p&0xff00ff)*w>>8)&0xff00ff)|((((p>>8)&0xff00ff)*w>>8)&0xff00ff)<<8;}
// Card c laid over the frame (top-down 32-bit, dw x dh) in the rectangle x,y,w,h, bilinear, pulled towards colour vc by v
// (0..1, inside the card's own rounded shape) - the fade of far cards, folded into the same pass.
void blendCard(uint32_t* dst,int dw,int dh,const BigCard& c,float x,float y,float w,float h,float v,COLORREF vc){
    int x0=std::max(0,(int)std::floor(x)),x1=std::min(dw,(int)std::ceil(x+w)),y0=std::max(0,(int)std::floor(y)),y1=std::min(dh,(int)std::ceil(y+h));if(x0>=x1||y0>=y1||c.w<2)return;
    const float kx=c.w/w,ky=c.h/h;const uint32_t iv=(uint32_t)std::clamp((int)(v*256),0,256),col=(uint32_t)GetBValue(vc)|((uint32_t)GetGValue(vc)<<8)|((uint32_t)GetRValue(vc)<<16)|0xff000000u;const int stride=c.w+2;
    std::vector<int> sx(x1-x0),wx(x1-x0);for(int X=x0;X<x1;X++){float f=(X+.5f-x)*kx-.5f;int i=(int)std::floor(f);int a=(int)((f-i)*256);if(i<-1){i=-1;a=0;}if(i>c.w-1){i=c.w-1;a=0;}sx[X-x0]=i+1;wx[X-x0]=a;}
    for(int Y=y0;Y<y1;Y++){float f=(Y+.5f-y)*ky-.5f;int sy=(int)std::floor(f),wy=(int)((f-sy)*256);if(sy<-1){sy=-1;wy=0;}if(sy>c.h-1){sy=c.h-1;wy=0;}
        const uint32_t* r0=c.px.data()+(size_t)(sy+1)*stride;const uint32_t* r1=r0+stride;uint32_t* row=dst+(size_t)Y*dw;
        for(int X=x0;X<x1;X++){int i=sx[X-x0];uint32_t a=wx[X-x0];uint32_t p=mix2(mix2(r0[i],r0[i+1],a),mix2(r1[i],r1[i+1],a),wy);uint32_t al=p>>24;if(!al)continue;
            if(iv){p=mix2(p,scale2(col,al+1),iv);}row[X]=p+scale2(row[X],256-al);}}}
double wrapD(double d,int n){if(n<2)return d;d=std::fmod(d,(double)n);if(d>n/2.0)d-=n;if(d<=-n/2.0)d+=n;return d;}
struct Slot {double d;Gdiplus::RectF r;float alpha;};
Slot slot(int i,int n){double d=wrapD(i-carPos,n),a=std::min(std::fabs(d),2.6),s=std::max(.52,1-.21*a)*cardScale,x=carX+(d<0?-1:1)*132*carF*a*(1-.08*a),y=carY+7*carF*a*a;
    float w=(float)(cardW*s),h=(float)(cardH*s);return {d,Gdiplus::RectF((float)(x-w/2),(float)(y-h/2),w,h),(float)std::clamp(1-.3*a,0.,1.)};}
std::vector<int> drawOrder(int n){std::vector<int> o(n);for(int i=0;i<n;i++)o[i]=i;std::sort(o.begin(),o.end(),[n](int a,int b){return std::fabs(slot(a,n).d)>std::fabs(slot(b,n).d);});return o;}
const dlsslib::Game* chosen(){auto& ok=listed();return ok.empty()?nullptr:&games[ok[std::clamp(carIdx,0,(int)ok.size()-1)]];}
// The carousel draws in its own child window (stage, page coordinates stageRect) so a turn repaints only that window:
// a background made once (panel colour, the blue light and the shadow), then the visible cards scaled and faded, then
// the dots, all in memory and copied in one go. Frames come from a 10 ms timer; the motion is eased by elapsed time,
// so a late tick does not make it jerk.
HBITMAP stageBg=nullptr,stageFrame=nullptr;int stageBgW=0,stageBgH=0;ULONGLONG lastStep=0;
HWND stageWnd(){return stage;}
int stageShift(){RECT rc;GetClientRect(stage,&rc);return (rc.right-(stageRect.right-stageRect.left))/2;} // the design is centred in a wider stage
// The ring the cards ride on: a thin blue arc under them, brightest in the middle and fading out at both ends.
void orbit(HDC dc,int ox,int oy){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for(int i=0;i<24;i++){float t=i/23.f,a0=20.f+t*140.f;int alpha=(int)(150*std::sin(3.14159f*t));Gdiplus::Pen pen(Gdiplus::Color((BYTE)alpha,70,120,255),1.6f);
        g.DrawArc(&pen,(float)(carX-430*carF-ox),(float)(carY-40*carF-oy),(float)(860*carF),(float)(170*carF),a0,140.f/23.f+.5f);}}
void paintStage(HDC dc){
    stageMetrics();RECT rc;GetClientRect(stage,&rc);HDC mem=CreateCompatibleDC(dc);
    const int ox=stageRect.left-stageShift(),oy=stageRect.top;
    if(stageBg&&(stageBgW!=rc.right||stageBgH!=rc.bottom)){DeleteObject(stageBg);DeleteObject(stageFrame);stageBg=stageFrame=nullptr;}
    if(!stageBg){stageBgW=rc.right;stageBgH=rc.bottom;HDC bg=CreateCompatibleDC(dc);stageBg=dibOf(rc.right,rc.bottom);stageFrame=dibOf(rc.right,rc.bottom);auto o=SelectObject(bg,stageBg);HBRUSH b=CreateSolidBrush(panel);FillRect(bg,&rc,b);DeleteObject(b);
        {int cy=(int)carY,gx=(int)(190*carF),gy=(int)(150*carF);painting::glow(bg,{carX-gx-ox,cy-gy-oy,carX+gx-ox,cy+gy-oy},blue,46);painting::glow(bg,{carX-(int)(100*carF)-ox,cy+(int)(100*carF)-oy,carX+(int)(100*carF)-ox,cy+(int)(128*carF)-oy},RGB(0,0,0),170);}orbit(bg,ox,oy);SelectObject(bg,o);DeleteDC(bg);}
    auto old=SelectObject(mem,stageFrame);{HDC bg=CreateCompatibleDC(dc);auto o=SelectObject(bg,stageBg);BitBlt(mem,0,0,rc.right,rc.bottom,bg,0,0,SRCCOPY);SelectObject(bg,o);DeleteDC(bg);}
    auto& ok=listed();int n=(int)ok.size();
    if(!n&&scanning){for(int d:{-2,2,-1,1,0}){double a=std::abs(d),sc=std::max(.52,1-.21*a)*cardScale,x=carX+(d<0?-1:1)*132*carF*a*(1-.08*a),y=carY+7*carF*a*a;int w=(int)(cardW*sc),hh=(int)(cardH*sc);
            RECT r={(int)(x-w/2.0)-ox,(int)(y-hh/2.0)-oy,(int)(x+w/2.0)-ox,(int)(y+hh/2.0)-oy};double fade=std::clamp(1-.3*a,0.,1.);
            painting::skeleton(mem,r,24,0,rc.right,painting::mixColor(panel,RGB(20,34,68),fade),painting::mixColor(panel,RGB(96,128,200),fade));}} // games being found: skeleton cards on the ring
    DIBSECTION ds={};GetObject(stageFrame,sizeof(ds),&ds);Gdiplus::Bitmap frame(rc.right,rc.bottom,ds.dsBm.bmWidthBytes,PixelFormat32bppPARGB,(BYTE*)ds.dsBm.bmBits); // GDI+ draws straight into the frame bits: through the HDC every card cost several ms
    if(n){Gdiplus::Graphics g(&frame);g.SetCompositingQuality(Gdiplus::CompositingQualityHighSpeed);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);g.TranslateTransform((float)-ox,(float)-oy);
        for(int i:drawOrder(n)){Slot s=slot(i,n);if(std::fabs(s.d)>2.55)continue;auto& game=games[ok[i]];
            // Faded and dimmed with distance (alpha, and the colour pulled down a little so far cards sit in shadow).
            // The veil towards the panel colour is part of the card blend (blendCard).
            g.Flush(Gdiplus::FlushIntentionSync);blendCard((uint32_t*)ds.dsBm.bmBits,rc.right,rc.bottom,bigCard(game),s.r.X-ox,s.r.Y-oy,s.r.Width,s.r.Height,1-s.alpha*(.55f+.45f*s.alpha),panel);
            if(!coverFor(game)){g.Flush(Gdiplus::FlushIntentionSync);float k=s.alpha;COLORREF c=RGB((int)(18+(GetRValue(mint)-18)*k),(int)(32+(GetGValue(mint)-32)*k),(int)(66+(GetBValue(mint)-66)*k));float cx=s.r.X+s.r.Width/2-ox,cy=s.r.Y+s.r.Height*.42f-oy,w=s.r.Width*.36f;
                painting::gamepad(mem,{(LONG)(cx-w),(LONG)(cy-w*.8f),(LONG)(cx+w),(LONG)(cy+w*.8f)},c,std::max(1.5f,3*k));}
            if(std::fabs(s.d)<.5){Gdiplus::GraphicsPath p;painting::path(p,{(LONG)s.r.X,(LONG)s.r.Y,(LONG)(s.r.X+s.r.Width),(LONG)(s.r.Y+s.r.Height)},24);Gdiplus::Pen pen(Gdiplus::Color((BYTE)(255*(1-std::fabs(s.d)*2)),90,140,255),2.f);g.DrawPath(&pen,&p);}}}
    if(n>1)for(int i=0;i<n&&i<16;i++){int x=carX-ox-(std::min(n,16)-1)*7+i*14;int dy=(int)(carY+cardH*cardScale/2+16)-oy;painting::circle(mem,{x-3,dy,x+4,dy+7},i==carIdx%16?blue:line,CLR_NONE);}
    BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteDC(mem);}
int cardAt(POINT p,int n){auto o=drawOrder(n);for(auto it=o.rbegin();it!=o.rend();++it){Slot s=slot(*it,n);if(std::fabs(s.d)<=2.55&&s.r.Contains((Gdiplus::REAL)p.x,(Gdiplus::REAL)p.y))return *it;}return -1;}
// A viewer mode or pause changed: only the mode buttons' highlight, the pause label and 게임 실행 (detail view) change.
void modeButtons(){const int ids[4]={340,341,342,345};for(int i=0;i<4;i++){HWND c=GetDlgItem(page,ids[i]);LONG_PTR on=mode==modeOf(ids[i])?1:0;if(GetWindowLongPtrW(c,GWLP_USERDATA)!=on){SetWindowLongPtrW(c,GWLP_USERDATA,on);InvalidateRect(c,nullptr,FALSE);}}
    std::wstring pause=paused?i18n::text(L"재개"):i18n::text(L"일시정지");wchar_t now[64]={};GetDlgItemTextW(page,344,now,64);if(pause!=now){SetDlgItemTextW(page,344,pause.c_str());SetPropW(GetDlgItem(page,344),L"BlueAsh.Icon",(HANDLE)(UINT_PTR)(paused?0xE768:0xE769));InvalidateRect(GetDlgItem(page,344),nullptr,FALSE);}
    if(view==1){HWND run=GetDlgItem(page,312);bool want=!gameUp&&mode==0;if((IsWindowVisible(run)!=0)!=want)ShowWindow(run,want?SW_SHOW:SW_HIDE);}}
void chosenChanged(){inval({12,stageRect.bottom,840,600});} // the name and labels under the carousel; the game itself loads when it is opened
// While the carousel moves only the name and labels follow the middle card; the rest (preset, live view, files) waits
// until it stops (chosenChanged from stepCarousel / a click), so turning never waits on disk or process lookups.
void chosenMoving(){inval({12,stageRect.bottom,840,600});}
// Turning: carTarget moves by whole cards, carPos eases towards it.
void turn(double delta){int n=(int)listed().size();if(n<2||!stage)return;carTarget=std::round(carTarget+delta);int was=carIdx;carIdx=((int)carTarget%n+n)%n;lastStep=GetTickCount64();SetTimer(stage,1,10,nullptr);if(carIdx!=was)chosenMoving();}
void turnTo(int i){int n=(int)listed().size();if(n<2||i==carIdx)return;turn(wrapD(i-carTarget,n));}
void stepCarousel(){ULONGLONG now=GetTickCount64();double dt=(double)std::min<ULONGLONG>(now-lastStep,50);lastStep=now;carPos+=(carTarget-carPos)*(1-std::exp(-dt/70.0));
    if(std::fabs(carTarget-carPos)<.002){carPos=carTarget;KillTimer(stage,1);int n=(int)listed().size();if(n>0){double k=std::floor(carTarget/n)*n;carPos-=k;carTarget-=k;}chosenChanged();}InvalidateRect(stage,nullptr,FALSE);}
// Stage input: drag sideways to turn, wheel to step, click a side card to bring it round, click the middle one to open it.
void firstCommand(int id);
LRESULT CALLBACK stageProc(HWND h,UINT m,WPARAM w,LPARAM l){stageMetrics();POINT p={(int)(short)LOWORD(l)+stageRect.left-stageShift(),(int)(short)HIWORD(l)+stageRect.top};int n=(int)listed().size();
    switch(m){
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paintStage(dc);EndPaint(h,&ps);return 0;}
    case WM_TIMER:stepCarousel();return 0;
    case WM_LBUTTONDOWN:if(n){carDrag=true;dragMoved=false;dragX=p.x;dragPos=carPos;KillTimer(h,1);SetCapture(h);}return 0;
    case WM_MOUSEMOVE:if(carDrag&&n>1){if(std::abs(p.x-dragX)>4)dragMoved=true;if(dragMoved){carPos=carTarget=dragPos-(p.x-dragX)/150.0;InvalidateRect(h,nullptr,FALSE);
            // The card now nearest the middle is the chosen one while dragging, not only after letting go.
            int nearest=((int)std::lround(carPos)%n+n)%n;if(nearest!=carIdx){carIdx=nearest;chosenMoving();}}}return 0;
    case WM_LBUTTONUP:if(!carDrag)return 0;carDrag=false;ReleaseCapture();
        if(dragMoved){carTarget=std::round(carPos);int was=carIdx;carIdx=((int)carTarget%n+n)%n;lastStep=GetTickCount64();SetTimer(h,1,10,nullptr);if(carIdx!=was)chosenMoving();}
        else{int i=cardAt(p,n);if(i>=0&&i==carIdx)firstCommand(354);else if(i>=0)turnTo(i);}return 0;
    case WM_MOUSEWHEEL:turn(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1);return 0;
    case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT){SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;}break;
    case WM_DESTROY:if(stageBg){DeleteObject(stageBg);DeleteObject(stageFrame);stageBg=stageFrame=nullptr;}stage=nullptr;return 0;
    }return DefWindowProcW(h,m,w,l);}
// Steam games with no cover on this PC: Steam's public store image (library_600x900) into dlss\covers, once. A game Steam
// has no such picture for gets a .none marker and is not asked again; a network failure is retried at the next scan.
std::atomic<bool> coverJob{false};
// Pictures for games without one: Steam's library cover (by app id), else IGDB through our relay (/cover, relay/cover.js:
// it holds the Twitch key and answers with the cover picture, or 410 when there is none). One try per game; '.none2' marks a game neither
// has (an older '.none' meant only Steam had none, so those are asked again). A relay that is not set up or not
// reachable (5xx, network) is asked again on the next scan.
std::wstring urlPart(const std::wstring& s){std::string u;int n=WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0,nullptr,nullptr);u.resize(n);WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),&u[0],n,nullptr,nullptr);
    std::wstring out;const char* hex="0123456789ABCDEF";for(unsigned char c:u){if(isalnum(c)||c=='-'||c=='_'||c=='.'||c=='~')out+=(wchar_t)c;else{out+=L'%';out+=(wchar_t)hex[c>>4];out+=(wchar_t)hex[c&15];}}return out;}
void fetchCovers(){if(coverJob||!page)return;std::wstring dir=root()+L"\\covers";CreateDirectoryW(dir.c_str(),nullptr);struct Want{std::wstring base,steam,name;};auto* want=new std::vector<Want>;
    for(int i:listed()){auto& g=games[i];std::wstring id(g.e.id.begin(),g.e.id.end()),base=dir+L"\\"+keyOf(g);bool steam=g.e.platform=="steam"&&!id.empty()&&std::all_of(id.begin(),id.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';});
        if(GetFileAttributesW((base+L".jpg").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((base+L".png").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((base+L".none2").c_str())!=INVALID_FILE_ATTRIBUTES)continue;
        want->push_back({base,steam?id:L"",g.e.name});}
    if(want->empty()){delete want;return;}coverJob=true;struct Job{HWND page;std::vector<Want>* want;};auto* j=new Job{page,want};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(Job*)p;
        auto picture=[](const std::wstring& jpg){Gdiplus::Bitmap* b=gameart::load(jpg);if(!b){DeleteFileW(jpg.c_str());return false;}delete b;return true;};
        for(auto& w:*j->want){std::wstring jpg=w.base+L".jpg";bool got=false,retry=false;
            if(!w.steam.empty()){std::wstring e=dlssopti::download(L"https://cdn.cloudflare.steamstatic.com/steam/apps/"+w.steam+L"/library_600x900.jpg",jpg,4u<<20);got=e.empty()&&picture(jpg);}
            if(!got){std::wstring q=L"https://"+std::wstring(updateHost)+L"/cover?name="+urlPart(w.name)+(w.steam.empty()?L"":L"&steam="+w.steam);
                std::wstring e=dlssopti::download(q,jpg,4u<<20);got=e.empty()&&picture(jpg);retry=!got&&e.find(L"HTTP 410")==std::wstring::npos;} /* 410 = IGDB has none; anything else (old relay, offline) is asked again */
            if(got){PostMessageW(j->page,WM_APP+194,0,0);}else if(!retry){writeFile(w.base+L".none2","");}}
        coverJob=false;delete j->want;delete j;return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{coverJob=false;delete want;delete j;}}
// Game title logos (the transparent wordmark beside the name in the game's view), saved once as
// dlss\covers\<game>-logo.png: Steam's own logo.png for Steam games, else SteamGridDB through our relay (/logo, which
// holds the key; IGDB has no game logos). '.nologo' when neither has one (relay 410); a relay that is not set up or not
// reachable is asked again at the next scan. Loaded cropped to the visible part (logos are padded with transparency).
std::map<std::wstring,Gdiplus::Bitmap*> logos;std::atomic<bool> logoJob{false};
Gdiplus::Bitmap* logoFor(const dlsslib::Game& g){std::wstring k=keyOf(g);auto it=logos.find(k);if(it!=logos.end())return it->second;
    Gdiplus::Bitmap* b=gameart::load(root()+L"\\covers\\"+k+L"-logo.png");
    if(b){int w=b->GetWidth(),h=b->GetHeight(),x0=w,y0=h,x1=-1,y1=-1;Gdiplus::BitmapData d;Gdiplus::Rect all(0,0,w,h);
        if(b->LockBits(&all,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&d)==Gdiplus::Ok){for(int y=0;y<h;y++){const BYTE* row=(const BYTE*)d.Scan0+(size_t)y*d.Stride;for(int x=0;x<w;x++)if(row[x*4+3]>16){x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}}b->UnlockBits(&d);}
        if(x1<x0){delete b;b=nullptr;}else if(x0>0||y0>0||x1<w-1||y1<h-1){auto* c=new Gdiplus::Bitmap(x1-x0+1,y1-y0+1,PixelFormat32bppARGB);{Gdiplus::Graphics gg(c);gg.DrawImage(b,Gdiplus::Rect(0,0,x1-x0+1,y1-y0+1),x0,y0,x1-x0+1,y1-y0+1,Gdiplus::UnitPixel);}delete b;b=c;}}
    logos[k]=b;return b;}
void fetchLogos(){if(logoJob||!page)return;std::wstring dir=root()+L"\\covers";CreateDirectoryW(dir.c_str(),nullptr);struct Want{std::wstring base,steam,name;};auto* want=new std::vector<Want>;
    for(int i:listed()){auto& g=games[i];std::wstring id(g.e.id.begin(),g.e.id.end()),base=dir+L"\\"+keyOf(g);bool steam=g.e.platform=="steam"&&!id.empty()&&std::all_of(id.begin(),id.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';});
        if(GetFileAttributesW((base+L"-logo.png").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((base+L".nologo").c_str())!=INVALID_FILE_ATTRIBUTES){continue;}want->push_back({base,steam?id:L"",g.e.name});}
    if(want->empty()){delete want;return;}logoJob=true;struct Job{HWND page;std::vector<Want>* want;};auto* j=new Job{page,want};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID a)->DWORD{auto* j=(Job*)a;bool any=false;
        for(auto& w:*j->want){std::wstring png=w.base+L"-logo.png";bool got=false,retry=false;
            auto picture=[&]{Gdiplus::Bitmap* b=gameart::load(png);if(b){delete b;return true;}DeleteFileW(png.c_str());return false;};
            if(!w.steam.empty()){got=dlssopti::download(L"https://cdn.cloudflare.steamstatic.com/steam/apps/"+w.steam+L"/logo.png",png,4u<<20).empty()&&picture();}
            if(!got){std::wstring e=dlssopti::download(L"https://"+std::wstring(updateHost)+L"/logo?name="+urlPart(w.name)+(w.steam.empty()?L"":L"&steam="+w.steam),png,4u<<20);
                got=e.empty()&&picture();retry=!got&&e.find(L"HTTP 410")==std::wstring::npos;}
            if(got){any=true;}else if(!retry){writeFile(w.base+L".nologo","");}}
        if(any){PostMessageW(j->page,WM_APP+194,0,0);}logoJob=false;delete j->want;delete j;return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{logoJob=false;delete want;delete j;}}
// A cover arrived: forget the pictures made so far (they are rebuilt from the new files on the next paint).
void coversChanged(){for(auto& c:covers)delete c.second;covers.clear();for(auto& c:logos)delete c.second;logos.clear();for(auto& c:cards)delete c.second;cards.clear();bigCards.clear();for(auto& c:thumbs)delete c.second;thumbs.clear();if(stage)InvalidateRect(stage,nullptr,FALSE);if(page)InvalidateRect(page,nullptr,FALSE);}
// 프레임 설정: the chosen limit per game (dlss\<game>\framelimit.txt, 0 = off); the monitor's refresh rate as one choice.
int loadLimit(const dlsslib::Game& g){return std::atoi(readFile(gameDir(g)+L"\\framelimit.txt",16).c_str());}
int refreshRate(){DEVMODEW m={};m.dmSize=sizeof(m);return EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&m)&&m.dmDisplayFrequency>1?(int)m.dmDisplayFrequency:0;}
int chipValue(int i){int v[]={0,60,90,120,144,refreshRate()};return v[i];}
// One short word for where a game stands.
std::wstring stateWord(const dlsslib::Game& g){if(applyState(g)!=dlssstatus::State::Absent)return i18n::text(L"설치됨");if(community(g))return installed(g)?i18n::text(L"적용됨"):i18n::text(L"커뮤니티 경로");
    if(g.v.status==dlss::Status::Unknown&&!g.v.title){return i18n::text(L"DLSS 없는 게임");}if(!verified(g))return i18n::text(g.v.status==dlss::Status::Unsupported?L"아직 미지원":L"확인 안 됨");
    if(!opti(g)){return i18n::text(L"공식 지원");}return installed(g)?i18n::text(L"적용됨"):i18n::text(L"커뮤니티 경로");}
// Under the carousel: the chosen game's name and one short line per tab; details live in the game's own screen.
// A rounded label under the game's name; returns its width.
int pillWidth(HDC dc,const std::wstring& t){SIZE s={};auto of=SelectObject(dc,smallFont);GetTextExtentPoint32W(dc,t.c_str(),(int)t.size(),&s);SelectObject(dc,of);return s.cx+28;}
// One system-information tile: name (small, dim), value (bold), an optional note under it.
void tile(HDC dc,RECT r,const std::wstring& label,const std::wstring& value,const std::wstring& note){boxW(dc,r,panel,line,12);UINT f=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;
    // A long name (other languages) steps down to the caption size before it would be cut.
    SIZE a={};auto of=SelectObject(dc,smallFont);GetTextExtentPoint32W(dc,label.c_str(),(int)label.size(),&a);SelectObject(dc,of);HFONT lf=a.cx>r.right-r.left-16?captionFont:smallFont;
    textW(dc,label,{r.left+10,r.top+5,r.right-6,r.top+22},lf,muted,f);bool low=r.bottom-r.top<56; /* short tile: the note sits at the right of the value line */
    textW(dc,value,{r.left+10,r.top+22,r.right-6,r.top+40},smallBold,ink,f);if(!note.empty()){if(low){textW(dc,note,{r.left+10,r.top+22,r.right-10,r.top+40},captionFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}else{textW(dc,note,{r.left+10,r.top+39,r.right-6,r.bottom-3},smallFont,muted,f);}}}
std::wstring gpuShort(){std::wstring n=wideOf(live.name);for(const wchar_t* cut:{L"NVIDIA ",L"GeForce "}){auto at=n.find(cut);if(at!=std::wstring::npos)n.erase(at,wcslen(cut));}return n.empty()?L"-":n;}
// The first screen under the carousel: the chosen game's name and state labels; left panel = its live view (the viewer
// window and three mode tabs, see layout), right panel per tab = presets and system information / frame limit / readings.
void paintFirst(HDC dc){auto& ok=listed();int n=(int)ok.size();if(gridMode)return;
    if(!n){RECT a=W({326,466,526,490}),l=W({60,0,792,0});painting::skeleton(dc,a,10,l.left,l.right,RGB(17,29,58),RGB(96,128,200)); /* games being found: the name and labels as skeletons */
        RECT b=W({296,498,556,522});for(int i=0;i<3;i++){int w=(b.right-b.left-16)/3,x=b.left+i*(w+8);painting::skeleton(dc,{x,b.top,x+w,b.bottom},12,l.left,l.right,RGB(17,29,58),RGB(96,128,200));}return;}
    auto& g=*chosen();bool o=opti(g),v=verified(g);
    textW(dc,g.e.name,{60,462,792,492},sectionFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    {auto& pills=pillMemo[keyOf(g)]; /* made once per game until the next layout: each card change of a turn repainted these from disk */
        if(pills.empty()){pills.push_back({stateWord(g),line});if(v)pills.push_back({L"DLSS 5",applyLevel(g)>0?blue:line});
        int lim=loadLimit(g);wchar_t fb[64];if(o&&lim)swprintf(fb,64,i18n::text(L"프레임 제한 %d FPS"),lim);else wcscpy(fb,i18n::text(L"프레임 생성 꺼짐"));pills.push_back({fb,line});
        if(v)pills.push_back({std::wstring(presetName(loadPreset(g)))+i18n::text(L" 프리셋"),line});}
        int total=-8;for(auto& p:pills)total+=pillWidth(dc,p.first)+8;RECT nb=W({60,498,792,522});int x=(nb.left+nb.right)/2-total/2;
        for(auto& p:pills){int w=pillWidth(dc,p.first);RECT r={x,nb.top,x+w,nb.bottom};::roundBox(dc,r,rowFill,p.second,13);::textAt(dc,p.first,r,smallFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);x+=w+8;}}}
// Views 3-5: the cover on the left, the name and labels on the right (hubCover / hubRight), then per view the choices,
// the frame limit panel or the readings.
void paint(HDC dc);
#include "perf_ui.h"
#include "tune_ui.h"
void paintFly(){} // (see paintHub)
void paintHub(HDC dc){auto* gp=current();if(!gp)return;auto& g=*gp;bool o=opti(g),v=verified(g);RECT c=hubCover(),R=hubRight();UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;wchar_t b[96];
    if(flyStart>0&&view==3&&flyFront&&flyBack){double p=std::clamp((nowMs()-flyStart)/flyMs,0.0,1.0),t=flyDir>0?p:1-p,e=t<.5?4*t*t*t:1-std::pow(-2*t+2,3)/2;
        auto lerp=[e](LONG a,LONG b2){return (LONG)std::lround(a+(b2-a)*e);};RECT r={lerp(flyFrom.left,c.left),lerp(flyFrom.top,c.top),lerp(flyFrom.right,c.right),lerp(flyFrom.bottom,c.bottom)};
        double turn=std::cos(e*2*3.14159265),k=std::fabs(turn);int cw=r.right-r.left,mid=(r.left+r.right)/2,w=(int)(cw*k);
        {Gdiplus::Graphics gg(dc);gg.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);gg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
         if(flyGlow){gg.DrawImage(flyGlow,Gdiplus::Rect(r.left-30,r.top-20,cw+60,r.bottom-r.top+50));}}
        // The right side: skeletons sliding in (from 0.15 s), the name, two labels and the three choices.
        // The right side: the name, the labels and the three choices slide in from the panel's right edge one after another
        // (each 0.4 of the flight, 0.08 apart) and all stop on the same left edge; backwards they leave in reverse order.
        const COLORREF base=RGB(17,29,58),light=RGB(96,128,200);RECT pr;GetClientRect(page,&pr);int edge=pr.right-12,run=edge-R.left;
        auto dxOf=[&](int k){double q=std::clamp((t-.12-.08*k)/.4,0.0,1.0);return q<=0?run:(int)std::lround((1-(1-std::pow(1-q,4)))*run);};
        {int sv=SaveDC(dc);IntersectClipRect(dc,12,12,edge,pr.bottom-12);
         {int dx=dxOf(0);if(dx<run)painting::skeleton(dc,{R.left+dx,c.top+6,R.left+dx+300,c.top+34},12,R.left,pr.right,base,light);}
         {int dx=dxOf(1);if(dx<run)for(int i=0;i<2;i++){painting::skeleton(dc,{R.left+dx+i*110,c.top+50,R.left+dx+i*110+100,c.top+74},12,R.left,pr.right,base,light);}}
         for(int i=0;i<3;i++){int dx=dxOf(2+i);if(dx>=run)continue;RECT q=hubCard(i);OffsetRect(&q,dx,0);painting::skeleton(dc,q,20,R.left,pr.right,base,light);}
         RestoreDC(dc,sv);}
        if(w>2){Gdiplus::Graphics gg(dc);gg.SetInterpolationMode(Gdiplus::InterpolationModeBilinear);gg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);gg.DrawImage(turn>=0?flyFront:flyBack,Gdiplus::Rect(mid-w/2,r.top,w,r.bottom-r.top));} /* the card over the skeletons it passes */
        return;}
    painting::glow(dc,{c.left-30,c.top-20,c.right+30,c.bottom+30},blue,30);
    if(!painting::art(dc,c,coverFor(g),24,.25f)){::roundBox(dc,c,tint,line,24);int cx=(c.left+c.right)/2,cy=(c.top+c.bottom)/2,w=(c.right-c.left)/4;painting::gamepad(dc,{cx-w,cy-w*4/5,cx+w,cy+w*4/5},mint,3.f);}
    painting::outline(dc,c,RGB(70,110,200),24);
    RECT pr;GetClientRect(page,&pr);{RECT nr={R.left,c.top,pr.right-36-166,c.top+40};SIZE ns={};auto of=SelectObject(dc,sectionFont);GetTextExtentPoint32W(dc,g.e.name.c_str(),(int)g.e.name.size(),&ns);SelectObject(dc,of);
        ::textAt(dc,g.e.name,nr,ns.cx>nr.right-nr.left?boldFont:sectionFont,ink,one);}
    {std::vector<std::wstring> pills={stateWord(g)};if(v)pills.push_back(std::wstring(presetName(loadPreset(g)))+i18n::text(L" 프리셋"));int lim=loadLimit(g);if(o&&lim){swprintf(b,96,i18n::text(L"프레임 제한 %d FPS"),lim);pills.push_back(b);}
        int x=R.left;for(auto& t:pills){int w=pillWidth(dc,t);RECT r={x,c.top+50,x+w,c.top+74};::roundBox(dc,r,rowFill,line,13);::textAt(dc,t,r,smallFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);x+=w+8;}}
    if(view==3){const wchar_t* title[3]={L"화질 향상",L"프레임 설정",L"성능 분석"},*note[3]={L"DLSS 5 프리셋 · 미리보기 · 적용",L"FPS 제한 · 모니터 주사율",L"GPU · VRAM · 온도 · FPS"};const wchar_t icon[3]={0xE91B,0xE916,0xE9D2};
        for(int i=0;i<3;i++){RECT r=hubCard(i);bool on=hubHover==i;::roundBox(dc,r,on?tint:rowFill,on?blue:line,20);int my=(r.top+r.bottom)/2;RECT disc={r.left+24,my-30,r.left+84,my+30};
            painting::circle(dc,disc,on?blue:RGB(24,44,96),CLR_NONE);glyph(dc,icon[i],disc,tileFont,RGB(236,242,255));
            ::textAt(dc,i18n::text(title[i]),{r.left+104,my-30,r.right-56,my},sectionFont,ink,DT_LEFT|DT_BOTTOM|DT_SINGLELINE|DT_END_ELLIPSIS);
            {std::wstring nt=i18n::text(note[i]);COLORREF nc=muted;if(i==2){if(shownFor!=keyOf(g)){shownFor=keyOf(g);shownCmp=loadCmp(g);}if(shownCmp.ok&&shownCmp.v==perf::Improved){nt=fmt(L"최근 최적화 효과 %+.0f FPS (%+.0f%%)",shownCmp.aFps-shownCmp.bFps,shownCmp.bFps>0?(shownCmp.aFps-shownCmp.bFps)/shownCmp.bFps*100:0);nc=RGB(90,200,140);}}
                ::textAt(dc,nt,{r.left+104,my+4,r.right-56,my+28},smallFont,nc,DT_LEFT|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);}
            glyph(dc,0xE76C,{r.right-52,r.top,r.right-20,r.bottom},iconFont,on?ink:muted);}
        return;}
    ::roundBox(dc,R,rowFill,line,20);auto s=livecap.stats();
    if(view==4){::textAt(dc,i18n::text(L"프레임 설정"),{R.left+24,R.top+18,R.right-24,R.top+44},boldFont,ink,one);::textAt(dc,i18n::text(L"FPS 제한"),{R.left+24,R.top+44,R.right-24,R.top+66},smallFont,muted,one);
        ::textAt(dc,o?i18n::text(L"게임을 다시 켜면 적용 · 프레임 생성은 늘 꺼짐"):i18n::text(L"게임 안 설정에서 바꿉니다"),{R.left+24,R.top+252,R.right-24,R.top+300},smallFont,muted,DT_LEFT|DT_TOP|DT_WORDBREAK);}
    else{::textAt(dc,i18n::text(L"성능 분석"),{R.left+24,R.top+18,R.right-24,R.top+44},boldFont,ink,one);::textAt(dc,i18n::text(L"측정값 · FPS는 화면 캡처 기준 추정"),{R.left+24,R.top+44,R.right-24,R.top+66},smallFont,muted,one);
        int cw=(R.right-R.left-48-12)/2;RECT t[4];for(int i=0;i<4;i++){int x=R.left+24+(i%2)*(cw+12),y=R.top+80+(i/2)*76;t[i]={x,y,x+cw,y+64};}
        if(live.ok){swprintf(b,96,L"%u%%",live.util);tile(dc,t[0],i18n::text(L"GPU 사용률"),b,gpuShort());swprintf(b,96,L"%u°C · %u W",live.temp,live.watts);tile(dc,t[1],i18n::text(L"온도 · 전력"),b,L"");
            swprintf(b,96,L"%.1f / %.1f GB",live.usedGB,std::max(0.0,live.totalGB-live.usedGB));tile(dc,t[2],i18n::text(L"VRAM 사용 / 남음"),b,L"");}
        else for(int i=0;i<3;i++)tile(dc,t[i],L"GPU",L"-",L"");
        if(gameUp&&s.samples>=30){swprintf(b,96,L"%.0f · %ls",s.fps,s.low1>0?std::to_wstring((int)std::lround(s.low1)).c_str():L"-");}else wcscpy(b,L"-");tile(dc,t[3],i18n::text(L"표시 FPS · 1% Low"),b,i18n::text(L"추정"));}
    ::textAt(dc,status,{R.left+24,R.bottom-48,R.right-24,R.bottom-18},smallBold,statusBad?danger:RGB(120,170,255),one);}
// ---- 전체 보기 (grid): search (370), which games (371: all / DLSS 5 ready / not checked), order (372: verified first /
// by name), a summary card that opens the hidden list, two rows of cards (wheel scrolls by rows) and a last card that
// adds a folder.
int gridTop=0;
bool hasText(const std::wstring& name,const wchar_t* q){if(!q||!*q)return true;std::wstring a=name,b=q;for(auto& c:a)c=towlower(c);for(auto& c:b)c=towlower(c);return a.find(b)!=std::wstring::npos;}
std::vector<int> gridList(){std::vector<int> v;wchar_t q[128]={};int f=0,so=0;if(page){GetDlgItemTextW(page,370,q,128);f=(int)SendDlgItemMessageW(page,371,CB_GETCURSEL,0,0);so=(int)SendDlgItemMessageW(page,372,CB_GETCURSEL,0,0);}
    for(int i:listed()){auto& g=games[i];if((f==1&&!verified(g))||(f==2&&verified(g))||!hasText(g.e.name,q))continue;v.push_back(i);}
    if(so==1){std::stable_sort(v.begin(),v.end(),[](int a,int b){return _wcsicmp(games[a].e.name.c_str(),games[b].e.name.c_str())<0;});}return v;}
// The grid's card picture, scaled once to its 112 x 168 size (scaling twelve 200 x 300 pictures on every paint was slow).
Gdiplus::Bitmap* gridThumb(const dlsslib::Game& g){std::wstring k=keyOf(g);auto it=thumbs.find(k);if(it!=thumbs.end())return it->second;
    auto* b=new Gdiplus::Bitmap(112,168,PixelFormat32bppPARGB);{Gdiplus::Graphics t(b);t.Clear(Gdiplus::Color(0,0,0,0));t.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);t.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);t.DrawImage(cardFor(g),Gdiplus::Rect(0,0,112,168));}
    thumbs[k]=b;return b;}
// 전체 보기 cards: six to a row, two rows on screen; the cover is the carousel's card picture (2:3).
int gridCols=6,gridRows=2,gridX=36; // set for the page size (WM_SIZE): more columns and rows in a bigger window
RECT gridCard(int k){k-=gridTop*gridCols;int x=gridX+(k%gridCols)*132,y=130+(k/gridCols)*206;return {x,y,x+124,y+200};} // on screen when 0 <= k-gridTop*gridCols < 12
const RECT summaryRect={624,76,816,120},searchRect={36,80,312,116};
void paintGrid(HDC dc){auto v=gridList();int total=(int)v.size(),hidden=(int)(games.size()-listed().size());UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;wchar_t b[128];
    boxW(dc,summaryRect,rowFill,line,12);swprintf(b,128,i18n::text(L"총 %d개 게임 발견"),(int)games.size());textW(dc,b,{summaryRect.left+14,summaryRect.top+4,summaryRect.right-30,summaryRect.top+24},smallBold,ink,one);
    swprintf(b,128,i18n::text(L"%d개 표시 중 · %d개 숨김"),total,hidden);textW(dc,b,{summaryRect.left+14,summaryRect.top+22,summaryRect.right-30,summaryRect.bottom-4},smallFont,muted,one);
    textW(dc,L"›",{summaryRect.right-28,summaryRect.top,summaryRect.right-8,summaryRect.bottom},sectionFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    boxW(dc,{24,544,828,594},rowFill,line,14);
    if(!showHidden){boxW(dc,searchRect,panel,line,10);glyphW(dc,0xE721,{searchRect.left+8,searchRect.top,searchRect.left+30,searchRect.bottom},iconFont,muted);
        HWND e=GetDlgItem(page,370);if(e&&!IsWindowVisible(e))textW(dc,i18n::text(L"게임 검색…"),{searchRect.left+30,searchRect.top,searchRect.right-8,searchRect.bottom},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);}
    if(showHidden){textW(dc,i18n::text(L"표시하지 않은 게임 (타르코프·배그·안티치트)"),{36,130,816,154},boldFont,ink,one);int y=162,shown=0;
        for(auto& g:games){if(g.v.status!=dlss::Status::Blocked)continue;if(shown==15){break;}textW(dc,g.e.name,{36,y,300,y+24},smallFont,ink,one);textW(dc,i18n::text(g.v.reason),{306,y,816,y+24},smallFont,muted,one);y+=26;shown++;}
        if(!shown){textW(dc,i18n::text(L"없음"),{36,148,816,172},smallFont,muted,one);}return;}
    if(scanning&&games.empty()){for(int k=0;k<gridCols;k++){RECT c=W(gridCard(gridTop*gridCols+k));RECT l=W(gridCard(0)),rr=W(gridCard(gridCols-1));painting::skeleton(dc,{c.left,c.top,c.right,c.top+168},16,l.left,rr.right,RGB(17,29,58),RGB(96,128,200));
            painting::skeleton(dc,{c.left+14,c.top+180,c.right-14,c.top+194},8,l.left,rr.right,RGB(17,29,58),RGB(96,128,200));}return;} // games being found: a row of shimmering cards
    const int first=gridTop*gridCols,last=first+gridCols*gridRows;
    for(int k=first;k<=total&&k<last;k++){RECT c=gridCard(k);RECT art={c.left+6,c.top+6,c.right-6,c.top+174};
        if(k==total){HPEN pen=CreatePen(PS_DOT,1,line);auto op=SelectObject(dc,pen);auto ob=SelectObject(dc,GetStockObject(NULL_BRUSH));RoundRect(dc,art.left,art.top,art.right,art.bottom,20,20);SelectObject(dc,op);SelectObject(dc,ob);DeleteObject(pen);
            int cx=(art.left+art.right)/2,cy=(art.top+art.bottom)/2;RECT o={cx-26,cy-26,cx+26,cy+26};painting::circle(dc,o,painting::mixColor(panel,blue,.18),blue,1.5f);glyphW(dc,0xE8F4,o,iconFont,ink);
            ::textAt(dc,i18n::text(L"폴더 추가"),{c.left,c.top+178,c.right,c.bottom},smallBold,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);continue;}
        auto& g=games[v[k]];bool sel=chosen()==&g,ver=verified(g);
        // The same card picture as the carousel, a soft glow behind the chosen one.
        if(sel)painting::glow(dc,{art.left-14,art.top-14,art.right+14,art.bottom+14},blue,60);
        {Gdiplus::Graphics gg(dc);gg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);gg.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);gg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            gg.DrawImage(gridThumb(g),Gdiplus::Rect(art.left,art.top,art.right-art.left,art.bottom-art.top));
            if(sel){Gdiplus::GraphicsPath gp;painting::path(gp,art,20);Gdiplus::Pen pen(Gdiplus::Color(255,90,140,255),2.f);gg.DrawPath(&pen,&gp);}}
        if(!coverFor(g))painting::gamepad(dc,{art.left+24,art.top+56,art.right-24,art.top+110},mint,2.5f);
        // The state on the cover's foot; a check on the chosen card.
        {std::wstring t=stateWord(g);int w=std::min(pillWidth(dc,t),(int)(art.right-art.left-12));RECT r={art.left+6,art.bottom-30,art.left+6+w,art.bottom-8};::roundBox(dc,r,RGB(10,16,32),ver?blue:line,10);::textAt(dc,t,{r.left+6,r.top,r.right-6,r.bottom},smallFont,ver?ink:muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        if(sel){RECT ck={art.right-28,art.top+8,art.right-8,art.top+28};painting::circle(dc,ck,blue,RGB(255,255,255),1.5f);
            Gdiplus::Graphics gg(dc);gg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen pen(Gdiplus::Color(255,255,255,255),2.f);painting::roundCaps(pen);Gdiplus::PointF m[3]={{ck.left+5.f,ck.top+10.5f},{ck.left+8.5f,ck.top+14.f},{ck.left+15.f,ck.top+6.5f}};gg.DrawLines(&pen,m,3);}
        ::textAt(dc,g.e.name,{c.left+2,c.top+178,c.right-2,c.bottom},smallBold,sel?ink:RGB(200,210,230),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
    int pages=(total+1+gridCols*gridRows-1)/(gridCols*gridRows);if(pages>1){swprintf(b,128,L"%d / %d",gridTop/gridRows+1,pages);textW(dc,b,{36,534,816,546},smallFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
// Which grid item is under p: a game index, -2 for the add-a-folder card, -1 for nothing.
int gridAt(POINT p){auto v=gridList();int total=(int)v.size();for(int k=gridTop*gridCols;k<=total&&k<gridTop*gridCols+gridCols*gridRows;k++){RECT c=gridCard(k);if(PtInRect(&c,p))return k==total?-2:v[k];}return -1;}
RECT chosenCardRect(int index){if(!page)return {}; /* also from view 3: the way back */
    if(!gridMode&&stage){auto& ok=listed();int n=(int)ok.size();for(int i=0;i<n;i++){if(ok[i]!=index)continue;stageMetrics();Slot s=slot(i,n);RECT sw;GetWindowRect(stage,&sw);MapWindowPoints(nullptr,page,(POINT*)&sw,2);
        int ox=stageRect.left-stageShift(),oy=stageRect.top;return {sw.left+(LONG)(s.r.X-ox),sw.top+(LONG)(s.r.Y-oy),sw.left+(LONG)(s.r.X+s.r.Width-ox),sw.top+(LONG)(s.r.Y+s.r.Height-oy)};}return {};}
    if(gridMode){auto v=gridList();for(size_t k=0;k<v.size();k++){if(v[k]==index){RECT c=W(gridCard((int)k));c.left+=6;c.right-=6;c.bottom=c.top+168;return c;}}}return {};}
// ---- One game (view 1): a banner with its cover and name; left the large viewer with its mode buttons; right three
// panels (presets, where it stands, system information); the actions in a bar at the bottom (positions in layout).
void paintDetail(HDC dc){auto* g=current();if(!g)return;bool o=opti(*g),ver=verified(*g);UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS,wrap=DT_LEFT|DT_TOP|DT_WORDBREAK|DT_END_ELLIPSIS;wchar_t b[128];
    // The title logo (fitted to 40 px high, at most 200 wide) before the name; the cover thumbnail when there is none.
    boxW(dc,{24,24,828,70},rowFill,line,14);int nameLeft=112;
    if(auto* lg=logoFor(*g)){double k=std::min(40.0/lg->GetHeight(),200.0/lg->GetWidth());int lw=(int)(lg->GetWidth()*k),lh=(int)(lg->GetHeight()*k);RECT m=W({38,47-lh/2,38+lw,47-lh/2+lh});
        painting::glow(dc,{m.left-18,m.top-10,m.right+18,m.bottom+10},RGB(150,175,230),60); /* a soft light behind it: some logos are dark on dark */
        Gdiplus::Graphics gg(dc);gg.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);gg.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);gg.DrawImage(lg,Gdiplus::Rect(m.left,m.top,m.right-m.left,m.bottom-m.top));nameLeft=38+lw+22;}
    else if(!painting::art(dc,{34,30,98,64},coverFor(*g),8,.25f)){boxW(dc,{34,30,98,64},tint,tint,8);}
    textW(dc,g->e.name,{nameLeft,24,660,70},sectionFont,ink,one);
    textW(dc,status,{36,500,490,518},smallBold,statusBad?danger:RGB(120,170,255),one);
    if(applyProgress>=0){boxW(dc,{36,520,440,527},well,well,7);if(applyProgress>0)boxW(dc,{36,520,36+404*applyProgress/100,527},blue,blue,7);textW(dc,std::to_wstring(applyProgress)+L"%",{446,514,490,530},smallBold,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
    // Presets
    // Presets: name on the left, its model and strengths on the right only where they fit beside the name.
    boxW(dc,{506,82,828,240},rowFill,line,14);textW(dc,i18n::text(L"그래픽 프리셋"),{522,94,812,116},boldFont,ink,one);
    for(int p=0;p<5;p++){bool sup=presetOk(*g,(dlss::Preset)p),sel=(int)preset==p;RECT r=presetRect(p);boxW(dc,r,sel?tint:panel,sel?blue:line,10);
        std::wstring name=presetName((dlss::Preset)p),l1;if(p==0)l1=i18n::text(L"DLSS 5 끔");else if(!sup)l1=i18n::text(L"미지원");else if(!o)l1=i18n::text(L"게임 기본");
        else{auto st=settingsFor(*g,(dlss::Preset)p);swprintf(b,128,L"%ls %.1f/%.1f",st.preset==1?L"A":L"B",st.v[0],st.v[1]);l1=b;}
        SIZE a={},c={};auto of=SelectObject(dc,smallBold);GetTextExtentPoint32W(dc,name.c_str(),(int)name.size(),&a);SelectObject(dc,smallFont);GetTextExtentPoint32W(dc,l1.c_str(),(int)l1.size(),&c);SelectObject(dc,of);
        bool both=a.cx+c.cx+34<=r.right-r.left;textW(dc,name,{both?r.left+12:r.left+6,r.top,both?r.right-10:r.right-6,r.bottom},!both&&a.cx>r.right-r.left-12?smallFont:smallBold,sup?ink:muted,both?one:(DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS));
        if(both){textW(dc,l1,{r.left+12,r.top,r.right-12,r.bottom},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}}
    // Where it stands
    boxW(dc,{506,252,828,368},rowFill,line,14);textW(dc,i18n::text(L"적용 상태"),{522,262,700,286},boldFont,ink,one);
    {auto st=applyState(*g);int lv=applyLevel(*g);std::wstring badge=st==dlssstatus::State::Active?i18n::text(L"적용됨"):st==dlssstatus::State::Restart?i18n::text(L"게임 재시작 필요"):lv==1?i18n::text(L"설치됨"):!ver?stateWord(*g):!o?i18n::text(L"공식 지원"):i18n::text(L"미적용");COLORREF bc=lv==2?RGB(90,210,140):st==dlssstatus::State::Restart?RGB(255,190,80):muted;
        int w=pillWidth(dc,badge)+10;RECT r={812-w,262,812,286};boxW(dc,r,panel,bc,12);painting::circle(dc,W({r.left+10,r.top+9,r.left+16,r.top+15}),bc,CLR_NONE);textW(dc,badge,{r.left+18,r.top,r.right-6,r.bottom},smallFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    if(o){if(applyLevel(*g)>0)textW(dc,applyText(*g),{522,292,812,333},smallFont,muted,wrap);
        else{textW(dc,runtimeLine(),{522,292,812,312},smallBold,runtimeInfo.error.empty()?ink:RGB(255,190,120),one);textW(dc,i18n::text(L"적용 안 됨"),{522,314,812,332},smallFont,muted,one);}}
    else textW(dc,applyLevel(*g)>0?applyText(*g):ver?i18n::text(L"세부 조절 항목은 이 게임이 제공하지 않습니다."):i18n::text(g->v.reason),{522,292,812,332},smallFont,muted,wrap);
    textW(dc,mode==1?deltaText():fpsText(),{522,336,812,356},smallFont,muted,one); // the GPU readings are in the tiles below
    // System information
    boxW(dc,{506,380,828,526},rowFill,line,14);textW(dc,i18n::text(L"현재 시스템 정보"),{522,390,812,412},boldFont,ink,one);
    {RECT t[4];for(int i=0;i<4;i++)t[i]={522+(i%2)*150,418+(i/2)*52,522+(i%2)*150+140,418+(i/2)*52+46};auto s=livecap.stats();
        tile(dc,t[0],L"GPU",live.ok?gpuShort():L"-",L"");
        if(live.ok){swprintf(b,128,L"%.1f / %.0f GB",live.usedGB,live.totalGB);tile(dc,t[1],L"VRAM",b,live.totalGB>0?std::to_wstring((int)std::lround(live.usedGB*100/live.totalGB))+L"%":L"");}else tile(dc,t[1],L"VRAM",L"-",L"");
        tile(dc,t[2],i18n::text(L"표시 FPS"),gameUp&&s.samples>=30?std::to_wstring((int)std::lround(s.fps))+(s.low1>0?L" · 1% "+std::to_wstring((int)std::lround(s.low1)):L""):L"-",i18n::text(L"추정"));
        if(live.ok){swprintf(b,128,L"%u°C · %u W",live.temp,live.watts);tile(dc,t[3],i18n::text(L"온도 · 전력"),b,L"");}else tile(dc,t[3],i18n::text(L"온도 · 전력"),L"-",L"");}
    boxW(dc,{24,538,828,598},rowFill,line,14);}
void paint(HDC dc){
    RECT pr;GetClientRect(page,&pr);::roundBox(dc,{12,12,pr.right-12,pr.bottom-12},panel,line,20);
    if(bigView){auto* g=current();if(!g||view==2){return;}bool o=opti(*g);wchar_t b[96];
        for(int p=0;p<5;p++){bool sup=presetOk(*g,(dlss::Preset)p),sel=(int)preset==p;RECT r=bigPreset(p);::roundBox(dc,r,sel?tint:rowFill,sel?blue:line,14);
            std::wstring l1;if(p==0)l1=i18n::text(L"DLSS 5 끔");else if(!sup)l1=i18n::text(L"미지원");else if(!o)l1=i18n::text(L"게임 기본");
            else{auto st=settingsFor(*g,(dlss::Preset)p);swprintf(b,96,L"%ls %ls · %.1f/%.1f",i18n::text(L"모델"),st.preset==1?L"A":L"B",st.v[0],st.v[1]);l1=b;}
            ::textAt(dc,presetName((dlss::Preset)p),{r.left+12,r.top+10,r.right-12,r.top+36},sectionFont,sup?ink:muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            ::textAt(dc,l1,{r.left+12,r.top+38,r.right-12,r.bottom-8},smallFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        return;}
    if(view==6){optPaint(dc);return;}if(view==5){perfPaint(dc);return;}if(view>=3){paintHub(dc);return;}
    if(view==0&&!gridMode&&(!listed().empty()||(scanning&&games.empty()))){paintFirst(dc);return;}
    if(view==2){paintEditor(dc);return;}
    if(view==0){paintGrid(dc);return;}
    paintDetail(dc);}
// ---- The optiscaler route.
struct Work {HWND page;std::wstring error,note;dlsslib::Game game;dlssnr::Settings settings;dlss::Preset preset;int kind;}; // kind 1 install, 2 capture
// The build's own capture: a trigger file beside it (its documented way to ask from outside), then its before/after
// frames of one frame, turned into this preset's pair of PNGs.
void captureReal(){auto* g=current();if(!g||!recheck())return;if(!installed(*g)){setStatus(i18n::text(L"먼저 적용하세요. 실제 캡처는 적용된 게임에서만 됩니다."),true);return;}
    if(preset==dlss::Preset::Original){setStatus(i18n::text(L"원본 프리셋은 비교할 DLSS 5 화면이 없습니다. 다른 프리셋을 고르세요."),true);return;}
    if(!runningPid(*g)){setStatus(i18n::text(L"게임을 실행해 DLSS 5가 켜진 화면까지 들어간 뒤 누르세요."),true);return;}
    auto* w=new Work{page,L"",L"",*g,{},preset,2};working=true;layout();setStatus(i18n::text(L"게임에 캡처를 요청했습니다. 몇 초 걸립니다…"));
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* w=(Work*)p;std::wstring dir=exeDir(w->game),cap=dir+L"\\dlssnr-capture\\manifest.txt";
        FILETIME before={};WIN32_FILE_ATTRIBUTE_DATA a;if(GetFileAttributesExW(cap.c_str(),GetFileExInfoStandard,&a))before=a.ftLastWriteTime;
        if(!writeFile(dir+L"\\dlssnr-capture.trigger","")){w->error=i18n::text(L"캡처 요청 파일을 만들지 못했습니다.");}
        else{bool done=false;for(int i=0;i<40&&!done;i++){Sleep(500);if(GetFileAttributesExW(cap.c_str(),GetFileExInfoStandard,&a)&&CompareFileTime(&a.ftLastWriteTime,&before)>0){Sleep(1500);done=true;}}
            if(!done)w->error=i18n::text(L"게임이 캡처하지 않았습니다. DLSS 5 패스가 실제로 돌고 있는지(OptiScaler 메뉴 Insert) 확인하세요.");
            else{std::vector<unsigned char> b,af;int cw=0,ch=0;w->error=dlssopti::readCapture(dir,b,af,cw,ch);std::wstring k=wideOf(dlss::presetKey(w->preset));
                if(w->error.empty()&&(!savePng(gameDir(w->game)+L"\\"+k+L"-before.png",cw,ch,cw*4,b.data())||!savePng(gameDir(w->game)+L"\\"+k+L".png",cw,ch,cw*4,af.data()))){w->error=i18n::text(L"캡처를 저장하지 못했습니다.");}}}
        if(!PostMessageW(w->page,WM_APP+192,0,(LPARAM)w)){delete w;}return 0;},w,0,nullptr);
    if(t)CloseHandle(t);else{delete w;working=false;layout();}}
// Writes the chosen preset into the installed OptiScaler.ini (no reinstall). Takes effect the next time the game starts.
std::wstring writeSettings(const dlsslib::Game& g,const dlssnr::Settings& s,bool enabled){Route r=route(g);
    if(r==Route::Reno){if(runningPid(g))return i18n::text(L"게임이 실행 중입니다. 게임을 끈 뒤 저장하세요 (ReShade가 끌 때 설정 파일을 다시 씁니다).");
        std::wstring ini=exeDir(g)+L"\\ReShade.ini";std::string text=readFile(ini,1<<20);if(text.empty())return i18n::text(L"설정을 저장하지 못했습니다.");return writeFile(ini,dlssnr::renoTune(text,s))?L"":i18n::text(L"설정을 저장하지 못했습니다.");}
    std::wstring ini=exeDir(g)+L"\\OptiScaler.ini";std::string text=readFile(ini,1<<20);if(text.empty())return i18n::text(L"OptiScaler.ini를 찾지 못했습니다.");
    auto name=dlssExecutableName(exeName(g));std::string out=r==Route::Opti?dlssnr::tune(text,s,enabled):dlssnr::configure(text,s,name,enabled); /* another tool's ini: only the look and the limit */
    return writeFile(ini,dlssnr::frameLimit(out,loadLimit(g)))?L"":i18n::text(L"설정을 저장하지 못했습니다.");}
void findRuntime(bool thenApply);
void install(){auto* g=current();if(!g||!recheck())return;
    // Checks first, all refusing rather than working around anything.
    live=nvml.read();if(!live.ok||!dlssgpu::blackwell(live.name)){setStatus(i18n::text(L"NVIDIA DLSS 5 런타임은 RTX 50 시리즈에서만 시작합니다. 이 GPU에는 적용하지 않습니다."),true);return;}
    refreshRuntime();if(!runtimeInfo.error.empty()){applyProgress=0;setStatus(i18n::text(L"이 PC에서 NVIDIA DLSS 5 파일을 찾는 중…"));findRuntime(true);return;}
    auto s=settingsFor(*g,preset);if(s.preset==2&&!runtimeInfo.models_ab){setStatus(i18n::text(L"이 런타임에는 모델 B가 없습니다 (310.9 이상 필요). 영화적(모델 A)을 쓰거나 새 런타임을 가져오세요."),true);return;}
    if(runningPid(*g)&&!managed(*g)){setStatus(i18n::text(L"게임이 실행 중입니다. 게임을 끈 뒤 적용하세요."),true);return;}
    std::wstring exe=exeDir(*g)+L"\\"+exeName(*g);if(GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES){setStatus(i18n::text(L"게임 실행 파일을 찾지 못했습니다: ")+exe,true);return;}
    if(GetFileAttributesW((exeDir(*g)+L"\\nvngx_dlss.dll").c_str())==INVALID_FILE_ATTRIBUTES){setStatus(i18n::text(L"이 경로는 게임의 DLSS가 필요합니다 (nvngx_dlss.dll 없음)."),true);return;}
    if(managed(*g)){auto e=writeSettings(*g,s,true);if(e.empty()){savePreset(*g,preset);writeFile(writtenPath(*g),dlssnr::serialize(s));}setStatus(e.empty()?i18n::text(L"설정을 바꿨습니다. 게임을 다시 켜면 적용됩니다."):e,!e.empty());layout();return;}
    auto c=dlssopti::conflict(exeDir(*g),{});if(!c.empty()){setStatus(i18n::text(c)+L" · "+i18n::text(L"그 모드를 넣은 도구로 먼저 되돌리세요. BlueAsh는 덮어쓰지 않습니다."),true);return;}
    std::wstring ask=std::wstring(i18n::text(L"이 게임에 DLSS 5 커뮤니티 경로를 적용할까요?"))+L"\n\n· OptiScaler DLSS-NR "+dlssopti::version+L" "+i18n::text(L"(GPL-3.0, 작성자 GitHub에서 받고 해시·Defender로 확인. OptiScaler.dll은 서명이 없습니다)")+
        L"\n· "+i18n::text(L"가져온 NVIDIA 서명 런타임")+L" "+wideOf(runtimeInfo.version)+L"\n· "+i18n::text(L"신경망 패스 1개, 프레임 생성 끔, 작업 해상도 1.0 이하")+L"\n· "+i18n::text(L"바꾸는 파일은 모두 백업하고 '원본 복구'로 되돌립니다")+
        L"\n\n"+i18n::text(L"비공식 경로라 NVIDIA나 게임사가 지원하지 않으며, 게임이 멈추면 원본 복구를 누르세요.");
    if(community(*g))ask+=L"\n\n"+std::wstring(i18n::text(L"이 게임은 BlueAsh가 직접 검증하지 않았습니다. 화면이 깨지거나 게임이 멈출 수 있고, 온라인 게임이면 계정에 불이익이 생길 수 있습니다. 싱글플레이에서만 쓰세요."));
    std::wstring agreed=gameDir(*g)+L"\\agreed.txt"; /* asked once per game; later applies (other presets) go straight on */
    if(GetFileAttributesW(agreed.c_str())==INVALID_FILE_ATTRIBUTES){if(MessageBoxW(win,ask.c_str(),L"BlueAsh",MB_YESNO|(community(*g)?MB_ICONWARNING:MB_ICONQUESTION))!=IDYES)return;writeFile(agreed,"yes");}
    auto* w=new Work{page,L"",L"",*g,s,preset,1};working=true;applyProgress=0;layout();setStatus(i18n::text(L"OptiScaler를 받아 확인하는 중… (처음에는 약 130 MB)"));
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* w=(Work*)p;try{auto progress=[w](int n){PostMessageW(w->page,WM_APP+199,(WPARAM)n,0);};std::wstring comp=componentDir();w->error=dlssopti::ensure(comp,&w->note,progress);
        if(w->error.empty()){auto rt=dlssrt::check(runtimePath());if(!rt.error.empty())w->error=rt.error;}  // checked again right before use
        if(w->error.empty()){progress(50);std::string base=readFile(comp+L"\\OptiScaler.ini",1<<20),name=dlssExecutableName(exeName(w->game));std::wstring ini=gameDir(w->game)+L"\\OptiScaler.ini";
            if(!writeFile(ini,dlssnr::frameLimit(dlssnr::configure(base,w->settings,name,true),loadLimit(w->game))))w->error=i18n::text(L"설정을 저장하지 못했습니다.");
            else{progress(55);w->error=dlssapply::apply(gameDir(w->game),dlssopti::plan(comp,exeDir(w->game),runtimePath(),ini),[w]{return runningPid(w->game)!=0;},progress);}}
        if(w->error.empty()&&(!savePreset(w->game,w->preset)||!writeFile(writtenPath(w->game),dlssnr::serialize(w->settings))))w->error=i18n::text(L"설정을 저장하지 못했습니다.");
        }catch(const std::exception&){w->error=i18n::text(L"DLSS 5 적용 중 오류가 발생했습니다. 이전 적용이 중단됐다면 BlueAsh를 다시 실행해 복구하세요.");}catch(...){w->error=i18n::text(L"DLSS 5 적용 중 오류가 발생했습니다. 이전 적용이 중단됐다면 BlueAsh를 다시 실행해 복구하세요.");}
        if(!PostMessageW(w->page,WM_APP+192,0,(LPARAM)w)){delete w;}return 0;},w,0,nullptr);
    if(t)CloseHandle(t);else{delete w;working=false;applyProgress=-1;layout();setStatus(i18n::text(L"DLSS 5 적용을 시작하지 못했습니다."),true);}}
void restore(const dlsslib::Game& g){DWORD pid=runningPid(g);auto e=dlssapply::restore(gameDir(g),[pid]{return pid!=0;});if(e.empty()){savePreset(g,dlss::Preset::Original);preset=dlss::Preset::Original;loadShots();}
    setStatus(e.empty()?i18n::text(L"원본 파일로 되돌렸습니다."):i18n::text(e),!e.empty());layout();}
// "DLSS 5 런타임 가져오기": the person's own file, NVIDIA-signed; the previous one is kept to go back to.
void importRuntime(){
    std::wstring prev=previousRuntime();if(GetFileAttributesW(prev.c_str())!=INVALID_FILE_ATTRIBUTES){auto pi=dlssrt::check(prev);
        std::wstring q=std::wstring(i18n::text(L"이전 런타임으로 되돌릴까요?"))+L"\n\n"+i18n::text(L"예: 이전 런타임 사용")+L" ("+wideOf(pi.version)+L")\n"+i18n::text(L"아니요: 새 파일 고르기");
        if(pi.error.empty()&&MessageBoxW(win,q.c_str(),L"BlueAsh",MB_YESNO|MB_ICONQUESTION)==IDYES){
            std::wstring cur=runtimePath(),swap=cur+L".swap";MoveFileExW(cur.c_str(),swap.c_str(),MOVEFILE_REPLACE_EXISTING);MoveFileExW(prev.c_str(),cur.c_str(),MOVEFILE_REPLACE_EXISTING);MoveFileExW(swap.c_str(),prev.c_str(),MOVEFILE_REPLACE_EXISTING);
            refreshRuntime();setStatus(i18n::text(L"이전 런타임으로 되돌렸습니다. 적용된 게임은 원본 복구 뒤 다시 적용하세요."));layout();return;}}
    std::wstring rights=std::wstring(i18n::text(L"DLSS 5 런타임(nvngx_dlssnr.dll)을 가져옵니다."))+L"\n\n"+i18n::text(L"NVIDIA는 이 파일을 따로 배포하지 않으며, BlueAsh도 포함하거나 내려받지 않습니다. 직접 적법하게 얻은 파일이어야 하고, 다른 게임에서 써도 되는지는 그 파일의 사용 조건을 따릅니다. BlueAsh는 NVIDIA 서명·버전만 확인할 뿐 사용 권한은 확인할 수 없습니다.")+L"\n\n"+i18n::text(L"계속할까요?");
    if(MessageBoxW(win,rights.c_str(),L"BlueAsh",MB_YESNO|MB_ICONWARNING)!=IDYES)return;
    wchar_t file[MAX_PATH]=L"nvngx_dlssnr.dll";OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=win;o.lpstrFilter=L"nvngx_dlssnr.dll\0nvngx_dlssnr.dll\0";o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetOpenFileNameW(&o)){return;}auto info=dlssrt::check(file);if(!info.error.empty()){setStatus(i18n::text(L"가져오지 않았습니다: ")+i18n::text(info.error),true);return;}
    std::wstring cur=runtimePath();if(GetFileAttributesW(cur.c_str())!=INVALID_FILE_ATTRIBUTES)MoveFileExW(cur.c_str(),prev.c_str(),MOVEFILE_REPLACE_EXISTING);
    if(!CopyFileW(file,cur.c_str(),FALSE)||!dlssrt::check(cur).error.empty()){setStatus(i18n::text(L"런타임을 저장하지 못했습니다."),true);return;}
    refreshRuntime();setStatus(i18n::text(L"런타임을 가져왔습니다: ")+wideOf(runtimeInfo.version)+L" · "+(runtimeInfo.models_ab?i18n::text(L"모델 A·B"):i18n::text(L"모델 A만")));layout();}
// The DLSS 5 runtime found on this PC by itself. NVIDIA ships nvngx_dlssnr.dll only inside games that use DLSS 5 and
// BlueAsh never downloads it, so the installed games' folders (and NVIDIA's NGX model folder) are searched; every copy
// must pass the same NVIDIA signature and version check as a hand-picked one, and the newest is copied in (the one in
// use is kept as the previous). Off the UI thread; WM_APP+198 brings the result. With thenApply, 적용하기 goes on after.
std::atomic<bool> runtimeJob{false};bool applyAfterRuntime=false;
struct RuntimeFind {HWND page;std::vector<std::wstring> dirs;std::wstring found;dlssrt::Info info;};
void walkFor(const std::wstring& dir,int depth,int& budget,std::vector<std::wstring>& out){if(depth<0||budget<=0)return;WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((dir+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return;
    do{if(--budget<=0)break;std::wstring n=f.cFileName;if(n==L"."||n==L"..")continue;
        if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))walkFor(dir+L"\\"+n,depth-1,budget,out);}
        else if(!_wcsicmp(n.c_str(),L"nvngx_dlssnr.dll"))out.push_back(dir+L"\\"+n);}while(FindNextFileW(h,&f));FindClose(h);}
void findRuntime(bool thenApply){applyAfterRuntime=applyAfterRuntime||thenApply;if(runtimeJob||!page)return;runtimeJob=true;auto* j=new RuntimeFind{page,{},L"",{}};
    for(auto& g:games){j->dirs.push_back(g.e.dir);}wchar_t pd[MAX_PATH]={};if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_COMMON_APPDATA,nullptr,SHGFP_TYPE_CURRENT,pd)))j->dirs.push_back(std::wstring(pd)+L"\\NVIDIA\\NGX\\models");
    HANDLE t=CreateThread(nullptr,0,[](LPVOID a)->DWORD{auto* j=(RuntimeFind*)a;std::vector<std::wstring> hits;for(auto& d:j->dirs){int budget=60000;walkFor(d,6,budget,hits);}
        int best[3]={-1,-1,-1};for(auto& f:hits){auto i=dlssrt::check(f);if(!i.error.empty())continue;int v[3]={0,0,0};std::sscanf(i.version.c_str(),"%d.%d.%d",&v[0],&v[1],&v[2]);
            if(std::lexicographical_compare(best,best+3,v,v+3)){std::copy(v,v+3,best);j->found=f;j->info=i;}}
        if(!PostMessageW(j->page,WM_APP+198,0,(LPARAM)j)){delete j;}return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{runtimeJob=false;delete j;if(applyAfterRuntime){applyAfterRuntime=false;applyProgress=-1;setStatus(i18n::text(L"DLSS 5 적용을 시작하지 못했습니다."),true);}}}
void command(int id);
void runtimeFound(RuntimeFind* j){runtimeJob=false;bool go=applyAfterRuntime;applyAfterRuntime=false;
    if(j->found.empty()){if(go){applyProgress=-1;setStatus(i18n::text(L"이 PC에서 NVIDIA DLSS 5 파일을 찾지 못했습니다. DLSS 5를 쓰는 게임(NBA 2K27, Metro Redux 등)이 설치돼 있어야 합니다. 따로 받은 파일이 있으면 'DLSS 5 런타임 가져오기'로 고르세요."),true);}return;}
    std::wstring cur=runtimePath();if(GetFileAttributesW(cur.c_str())!=INVALID_FILE_ATTRIBUTES){MoveFileExW(cur.c_str(),previousRuntime().c_str(),MOVEFILE_REPLACE_EXISTING);}
    if(!CopyFileW(j->found.c_str(),cur.c_str(),FALSE)){if(go){applyProgress=-1;setStatus(i18n::text(L"런타임을 저장하지 못했습니다."),true);}return;}
    runtimeInfo=j->info;if(go){setStatus(i18n::text(L"DLSS 5 런타임을 이 PC에서 찾아 가져왔습니다: ")+wideOf(runtimeInfo.version));command(314);}else if(page){InvalidateRect(page,nullptr,FALSE);}}
// ---- Editor (view 2).
void openEditor(){auto* g=current();if(!g)return;edit=custom(*g);view=2;apxChanged();hoverKnob=-1;status.clear();layout();}
int knobAt(POINT p){for(size_t i=0;i<dlssnr::knobs().size();i++){RECT r=knobRow((int)i);r.left-=8;r.right+=4;if(PtInRect(&r,p))return (int)i;}return -1;}
void setKnob(int i,int x){auto& k=dlssnr::knobs()[i];double v=k.lo+(std::clamp(x,trackL,trackR)-trackL)*(k.hi-k.lo)/(trackR-trackL);edit.v[i]=dlssnr::clampTo(k,v);apxChanged();InvalidateRect(page,nullptr,FALSE);}
void editorCommand(int id){auto* g=current();if(!g)return;apxChanged();
    if(id==330||id==331){edit.preset=id==330?1:2;layout();}
    else if(id==332){edit.autoMask=!edit.autoMask;layout();}
    else if(id==334){edit=dlssnr::defaults();setStatus(i18n::text(L"기본값으로 되돌렸습니다. 저장해야 남습니다."));layout();}
    else if(id==337){view=1;cmp.zoom=0;loadShots();layout();}
    else if(id==333){if(!writeFile(gameDir(*g)+L"\\custom.txt",dlssnr::serialize(edit))){setStatus(i18n::text(L"설정을 저장하지 못했습니다."),true);return;}
        preset=dlss::Preset::Custom;savePreset(*g,preset);
        // Applied = written into the installed build's OptiScaler.ini; it takes effect when the game next starts.
        if(managed(*g)){refreshRuntime();if(edit.preset==2&&runtimeInfo.error.empty()&&!runtimeInfo.models_ab){setStatus(i18n::text(L"이 런타임에는 모델 B가 없습니다 (310.9 이상 필요). 영화적(모델 A)을 쓰거나 새 런타임을 가져오세요."),true);return;}
            auto e=writeSettings(*g,edit,true);if(!e.empty()){setStatus(e,true);return;}writeFile(writtenPath(*g),dlssnr::serialize(edit));
            setStatus(livePid?i18n::text(L"게임 설정 파일에 저장했습니다. 게임을 다시 켜면 적용됩니다 (실행 중 변경은 게임 안 OptiScaler 메뉴 Insert)."):i18n::text(L"게임 설정 파일에 저장했습니다. 다음에 게임을 켜면 적용됩니다."));}
        else{setStatus(i18n::text(L"사용자 지정을 저장했습니다. 게임에는 '적용하기'를 눌러야 들어갑니다."));}layout();}
    else if(id==335||id==336){wchar_t file[MAX_PATH]=L"BlueAsh-DLSS5-preset.txt";OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=win;o.lpstrFilter=L"BlueAsh preset (*.txt)\0*.txt\0";o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.lpstrDefExt=L"txt";
        if(id==336){o.Flags=OFN_OVERWRITEPROMPT|OFN_NOCHANGEDIR;if(GetSaveFileNameW(&o))setStatus(writeFile(file,dlssnr::serialize(edit))?i18n::text(L"내보냈습니다."):i18n::text(L"설정을 저장하지 못했습니다."));}
        else{o.Flags=OFN_FILEMUSTEXIST|OFN_NOCHANGEDIR;if(GetOpenFileNameW(&o)){std::string err;dlssnr::Settings s;if(dlssnr::parse(readFile(file,8192),s,&err)){edit=s;setStatus(i18n::text(L"가져왔습니다. 저장해야 남습니다."));layout();}
            else setStatus(i18n::text(L"BlueAsh DLSS 5 프리셋이 아니거나 허용 범위를 벗어난 값이 있어 가져오지 않았습니다."),true);}}}}
// First screen actions (view 0): tabs, 전체 보기, turning, and the chosen game's buttons.
void command(int id);
// The game's screen: 0 = 화질 향상 (the viewer view, its preview already loaded by open), 1 = 프레임 설정, 2 = 성능 분석.
void enterHub(int i){hubHover=-1;status.clear();view=i==0?1:i==1?4:5;if(view==1){loadShots();previewFirst();}if(view==5){perfOpen();}layout();manageLive();}
void firstCommand(int id){auto* c=id>=360?current():chosen();auto ok=listed();
    if(id>=350&&id<=352){}
    else if(id==353||id==358){gridMode=id==358;gridTop=0;showHidden=false;layout();}
    else if(id==356||id==357)turn(id==356?-1:1);
    else if(!c||working)return;
    else if(id==354){open(ok[carIdx]);}
    else if(id>=360&&id<=365){if(!opti(*c))return;int v=chipValue(id-360);if(!writeFile(gameDir(*c)+L"\\framelimit.txt",std::to_string(v))){setStatus(i18n::text(L"설정을 저장하지 못했습니다."),true);return;}
        // Installed: the limit goes into the game's file now (with the current preset); otherwise it goes in with 적용.
        if(managed(*c)){auto e=writeSettings(*c,settingsFor(*c,loadPreset(*c)),true);if(!e.empty()){setStatus(e,true);layout();return;}writeFile(writtenPath(*c),dlssnr::serialize(settingsFor(*c,loadPreset(*c))));
            setStatus(i18n::text(L"게임 설정 파일에 저장했습니다. 다음에 게임을 켜면 적용됩니다."));}
        else{setStatus(i18n::text(L"저장했습니다. 이 게임에 DLSS 5를 적용할 때 함께 들어갑니다."));}layout();}}
void command(int id){if(id>=420&&id<=425){tuneSetTab(id-420);return;}if(id>=380&&id<=414){perfCommand(id);return;}auto* g=current();
    if(id==346){bigView=!bigView;cmp.zoom=0;layout();InvalidateRect(page,nullptr,TRUE);return;} // the viewer's enlarge / back button
    if(id>=330&&id<=337){editorCommand(id);return;}
    if(id>=350&&id<=365){firstCommand(id);return;}
    if(id==300){if(flying())return;stopLive();RECT to={};if(view==3){SendMessageW(page,WM_SETREDRAW,FALSE,0);view=0;layoutNow();to=chosenCardRect(pick);view=3;SendMessageW(page,WM_SETREDRAW,TRUE,0);} /* the carousel laid out as it will be, to land on its middle card */if(to.right>to.left){startFly(to,-1);if(flyStart>0){layout();return;}}backToList();}
    else if(id==301){view=3;bigView=false;cmp.zoom=0;status.clear();layout();}
    else if(id==320){std::wstring path=pickFolder(GetAncestor(page,GA_ROOT),i18n::text(L"게임 설치 폴더 선택"));if(!path.empty()){std::wstring f=folders();saveString(configPath,L"DlssFolders",f.empty()?path:f+L"|"+path);scan();}}
    else if(id==321)scan();
    else if(id==322){showHidden=!showHidden;layout();}
    else if(!g||working)return;
    else if(id>=340&&id<=342){if(id==342){loadShots();if(!opti(*g)&&!renoAvailable(*g)&&!cmp.a)return;}mode=id-340;KillTimer(page,3);cmp.zoom=0;cmp.split=.5;modeButtons();if(viewer)InvalidateRect(viewer,nullptr,FALSE);}
    else if(id==345){mode=3;cmp.zoom=0;cmp.split=.5;apxReset();apxRefresh();SetTimer(page,3,100,nullptr);modeButtons();}
    else if(id==343){takeBaseline();if(baseBmp&&baseSnap.ok)setStatus(i18n::text(L"화면을 저장했습니다. 설정을 바꾼 뒤 같은 장면에서 비교하세요."));layout();}
    else if(id==344){paused=!paused;if(!paused)onLiveFrame();modeButtons();}
    else if(id==316)importRuntime();
    else if(id==317)captureReal();
    else if(id==312){std::wstring target=g->e.platform=="steam"?L"steam://rungameid/"+std::wstring(g->e.id.begin(),g->e.id.end()):g->exe;if(target.empty()||(INT_PTR)ShellExecuteW(page,L"open",target.c_str(),nullptr,g->e.dir.c_str(),SW_SHOWNORMAL)<=32)setStatus(i18n::text(L"게임을 실행하지 못했습니다."),true);}
    else if(id==313){if(opti(*g))openEditor();else setStatus(i18n::text(L"이 게임의 DLSS 5는 게임 안에서 켜고 끄기만 할 수 있어 BlueAsh가 조절할 세부 항목이 없습니다. 지원 게임이 세부 항목을 열면 이곳에서 만들 수 있습니다."));}
    else if(id==314){if(!recheck())return;if(!presetOk(*g,preset)){setStatus(i18n::text(L"이 프리셋은 이 게임에서 지원하지 않습니다."),true);return;}
        if(opti(*g)){if(preset==dlss::Preset::Original){if(installed(*g))restore(*g);else if(route(*g)==Route::Opti){auto e=writeSettings(*g,settingsFor(*g,dlss::Preset::Balanced),false);if(e.empty())savePreset(*g,preset);setStatus(e.empty()?i18n::text(L"DLSS 5 패스를 껐습니다. 게임을 다시 켜면 적용됩니다."):e,!e.empty());layout();}else{savePreset(*g,preset);setStatus(i18n::text(L"원본 그대로입니다. BlueAsh가 바꾼 파일이 없습니다."));}}else install();return;}
        if(!savePreset(*g,preset)){setStatus(i18n::text(L"설정을 저장하지 못했습니다."),true);return;}
        setStatus(preset==dlss::Preset::Original?i18n::text(L"원본으로 저장했습니다. 게임 안 그래픽 설정에서 DLSS 5를 끄세요. BlueAsh는 게임 파일을 바꾸지 않습니다."):i18n::text(L"저장했습니다. 게임 안 그래픽 설정에서 DLSS 5를 켜세요. BlueAsh는 게임 파일을 바꾸지 않습니다."));}
    else if(id==315){if(installed(*g)){restore(*g);return;}
        preset=dlss::Preset::Original;savePreset(*g,preset);loadShots();layout();setStatus(i18n::text(L"BlueAsh가 바꾼 게임 파일은 없습니다. 원본으로 저장했습니다. 게임 안 설정에서 DLSS 5를 끄세요."));}}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    if(page&&(view==6||(msg>=WM_APP+210&&msg<=WM_APP+215)||(msg==WM_TIMER&&wp==14))&&tuneHandle(h,msg,wp,lp))return 0;
    if(((msg>=WM_APP+168&&msg<=WM_APP+176)||(msg==WM_TIMER&&(wp==6||wp==12||wp==13)))&&page&&perfMessage(msg,wp,lp))return 0;
    switch(msg){
    case WM_CREATE:{page=h;view=0;pick=-1;optMode=mainPage==15;if(optMode){view=6;}showHidden=false;working=false;status.clear();
        WNDCLASSW wc={};wc.lpfnWndProc=viewerProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshCompare";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.style=CS_DBLCLKS;RegisterClassW(&wc);
        viewer=CreateWindowExW(0,L"BlueAshCompare",L"",WS_CHILD|WS_CLIPSIBLINGS,viewerRect.left,viewerRect.top,viewerRect.right-viewerRect.left,viewerRect.bottom-viewerRect.top,h,nullptr,wc.hInstance,nullptr);
        WNDCLASSW sc={};sc.lpfnWndProc=stageProc;sc.hInstance=wc.hInstance;sc.lpszClassName=L"BlueAshCarousel";sc.hCursor=LoadCursor(nullptr,IDC_HAND);RegisterClassW(&sc);
        stage=CreateWindowExW(0,L"BlueAshCarousel",L"",WS_CHILD|WS_CLIPSIBLINGS,stageRect.left,stageRect.top,stageRect.right-stageRect.left,stageRect.bottom-stageRect.top,h,nullptr,wc.hInstance,nullptr);
        tab=0;pageButton(h,i18n::text(L"‹ 게임 목록"),676,18,140,300);pageButton(h,i18n::text(L"‹ 뒤로"),676,18,140,301);
        pageButton(h,i18n::text(L"실시간 화면"),36,354,110,340);pageButton(h,i18n::text(L"저장한 화면과 비교"),152,354,150,341);pageButton(h,i18n::text(L"정확 비교"),308,354,130,342);
        pageButton(h,i18n::text(L"미리보기(예상)"),414,354,130,345);pageButton(h,i18n::text(L"지금 화면 저장"),548,354,130,343);pageButton(h,i18n::text(L"일시정지"),686,354,130,344);
        pageButton(h,i18n::text(L"DLSS 5 런타임 가져오기"),36,556,180,316);pageButton(h,i18n::text(L"실제 전후 캡처"),224,556,120,317);pageButton(h,i18n::text(L"게임 실행"),352,556,100,312);
        pageButton(h,i18n::text(L"프리셋 만들기"),460,556,120,313);pageButton(h,i18n::text(L"원본 복구"),588,556,110,315);pageButton(h,i18n::text(L"적용하기"),706,556,110,314,true);
        pageButton(h,i18n::text(L"폴더 추가"),36,549,150,320);pageButton(h,i18n::text(L"다시 검색"),192,549,150,321);pageButton(h,L"",666,549,150,322);
        // 전체 보기 filters: search, which games, order.
        addSetting(h,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,66,90,236,20,370);ShowWindow(GetDlgItem(h,370),SW_HIDE); // shown while typing; the box and hint are drawn (paintGrid)
        addSetting(h,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,320,80,148,200,371);addSetting(h,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,476,80,140,200,372);
        for(const wchar_t* t:{L"모든 게임",L"DLSS 5 가능",L"확인 안 됨"}){SendDlgItemMessageW(h,371,CB_ADDSTRING,0,(LPARAM)i18n::text(t));}for(const wchar_t* t:{L"지원 먼저",L"이름순"})SendDlgItemMessageW(h,372,CB_ADDSTRING,0,(LPARAM)i18n::text(t));
        for(int id:{371,372}){SendDlgItemMessageW(h,id,CB_SETCURSEL,0,0);SendDlgItemMessageW(h,id,CB_SETITEMHEIGHT,(WPARAM)-1,30);SendDlgItemMessageW(h,id,CB_SETITEMHEIGHT,0,28);}
        pageButton(h,i18n::text(L"화질 향상"),36,28,120,350);pageButton(h,i18n::text(L"프레임 설정"),162,28,120,351);pageButton(h,i18n::text(L"성능 분석"),288,28,120,352);pageButton(h,i18n::text(L"캐러셀"),560,28,124,353);pageButton(h,i18n::text(L"전체 보기"),692,28,124,358);
        pageButton(h,L"‹",40,172,44,356);pageButton(h,L"›",768,172,44,357);pageButton(h,L"",286,470,130,354);pageButton(h,i18n::text(L"적용하기"),436,470,130,355,true);
        {const wchar_t* chip[]={i18n::text(L"끔"),L"60 FPS",L"90 FPS",L"120 FPS",L"144 FPS",L""};for(int i=0;i<6;i++)pageButton(h,chip[i],36+i*130,466,122,360+i);}perfCreate(h);tuneCreate(h);
        pageButton(h,L"A",646,62,56,330);pageButton(h,L"B",710,62,56,331);pageSwitch(h,i18n::text(L"자동 마스크"),panelL,428,panelR-panelL,32,332);
        pageButton(h,i18n::text(L"가져오기"),36,556,120,335);pageButton(h,i18n::text(L"내보내기"),164,556,120,336);pageButton(h,i18n::text(L"기본값으로"),292,556,130,334);
        pageButton(h,i18n::text(L"닫기"),560,556,120,337);pageButton(h,i18n::text(L"저장"),696,556,120,333,true);
        // Viewer modes and actions: an icon, the label beside it where it fits (every language), the label as a tooltip.
        for(auto [id,ic]:std::initializer_list<std::pair<int,wchar_t>>{{345,0xE890},{340,0xE714},{341,0xE8B9},{342,0xE8C8},{343,0xE74E},{344,0xE769},{316,0xE8B5},{317,0xE722},{312,0xE768},{313,0xE70F},{315,0xE7A7},{314,0xE73E},{335,0xE8B5},{336,0xEDE1},{334,0xE72C}}){iconButton(GetDlgItem(h,id),ic);}
        if(optMode){checkBox(h,414,optApplied());} /* the switch shows its real state from the first paint, not after the game scan */
        scan();layout();SetTimer(h,1,1000,nullptr);return 0;} // scanning first, so the carousel opens with its skeleton
    case WM_SIZE:{EX=std::max(0,(int)LOWORD(lp)-852);EY=std::max(0,(int)HIWORD(lp)-622);gridCols=std::max(1,(792+EX)/132);gridRows=std::max(1,(412+EY)/206);gridX=36+(784+EX-(gridCols*132-8))/2;
        gridTop=std::min(gridTop,std::max(0,((int)gridList().size()+gridCols)/gridCols-gridRows));if(page)layout();return 0;}
    case WM_APP+194:coversChanged();return 0;
    case WM_APP+198:{std::unique_ptr<RuntimeFind> j((RuntimeFind*)lp);runtimeFound(j.get());return 0;}
    case WM_APP+196:if(mode==3){apxReset();apxRefresh();}return 0;
    case WM_APP+197:{auto* r=(dlssapprox::Faces*)lp;if((unsigned)wp==apxFaceGen&&apxSrcBmp){apxFaces=std::move(*r);if(mode==3){apxRefresh();}}delete r;return 0;}
    case WM_TIMER:if(wp==5){InvalidateRect(h,nullptr,FALSE);if(!flying()){KillTimer(h,5);int dir=flyDir;endFly();if(dir<0){backToList();}else{layout();}}return 0;}
        if(wp==4){bool games0=scanning&&games.empty(),shots=shotJob&&mode==3&&!apxSrcBmp&&viewer&&IsWindowVisible(viewer);if(!games0&&!shots){KillTimer(h,4);}
            if(games0){InvalidateRect(h,nullptr,FALSE);if(stage&&IsWindowVisible(stage)){InvalidateRect(stage,nullptr,FALSE);}}if(shots){InvalidateRect(viewer,nullptr,FALSE);}return 0;} /* skeletons while games or screenshots load */
        if(wp==3){if(mode!=3)KillTimer(h,3);else if(apxDirty)apxRefresh();return 0;}if(wp==1){live=nvml.read();if(view>=1)manageLive();else if(gameUp||livecap.running())stopLive();if(mode==1&&baseBmp&&liveBmp)liveSame=sameScene(baseBmp,liveBmp);
        if(view==2)InvalidateRect(h,nullptr,FALSE);else if(view==1){inval({506,252,828,526});}} /* only the readings */return 0;
    case WM_APP+190:{auto* r=(std::vector<dlsslib::Game>*)lp;auto* g=(std::wstring*)wp;std::wstring was=pick>=0&&pick<(int)games.size()?games[pick].e.dir:L"";games=*r;gpu=*g;delete r;delete g;scanning=false;buildList();fetchCovers();fetchLogos();if(GetFileAttributesW(runtimePath().c_str())==INVALID_FILE_ATTRIBUTES){findRuntime(false);}
        pick=-1;for(int i=0;i<(int)games.size();i++)if(!was.empty()&&games[i].e.dir==was&&games[i].v.status!=dlss::Status::Blocked)pick=i;if(pick<0)view=0;
        {int n=(int)listed().size();carIdx=n?std::clamp(carIdx,0,n-1):0;carPos=carTarget=carIdx;}if(view==0){pick=-1;}if(optMode){optPick();}layout();
#ifdef BLUEASH_TT
        {wchar_t q[260]={};if(GetEnvironmentVariableW(L"BLUEASH_TT_OPEN",q,260)&&view==0){for(int i:listed())if(games[i].e.dir.find(q)!=std::wstring::npos){open(i);break;}}} /* test builds only: open a game straight away */
#endif
        return 0;}
    case WM_APP+193:onLiveFrame();return 0;
    case WM_APP+199:if(working){applyProgress=std::clamp((int)wp,0,95);InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_APP+192:{auto* w=(Work*)lp;working=false;if(w->kind==1)applyProgress=w->error.empty()?100:-1;
        if(w->kind==1)setStatus(w->error.empty()?i18n::text(L"적용했습니다. 게임을 켜고, 멈추거나 이상하면 '원본 복구'를 누르세요.")+(w->note.empty()?L"":L" ("+i18n::text(w->note)+L")"):i18n::text(w->error),!w->error.empty());
        else setStatus(w->error.empty()?i18n::text(L"실제 전후 화면을 찍었습니다. 이 PC에만 저장됩니다."):i18n::text(w->error),!w->error.empty());
        delete w;loadShots();layout();return 0;}
    case WM_LBUTTONDOWN:{POINT p={((int)(short)LOWORD(lp)),((int)(short)HIWORD(lp))},q=unW(p); // p on the page, q in design coordinates
        if(view==3&&!bigView){if(flying()){return 0;}int i=hubAt(p);if(i>=0)enterHub(i);return 0;}
        if(view==0&&!bigView&&gridMode){auto ok=listed();
            {if(PtInRect(&searchRect,q)&&!showHidden){HWND e=GetDlgItem(h,370);ShowWindow(e,SW_SHOW);SetFocus(e);inval(searchRect);return 0;}if(PtInRect(&summaryRect,q)){showHidden=!showHidden;layout();return 0;}if(showHidden)return 0;int i=gridAt(p);if(i==-2)command(320);else if(i>=0)open(i);return 0;}}
        else if(view==2&&!bigView){int k=knobAt(q);if(k>=0&&q.x>=trackL-10&&q.x<=trackR+10){dragKnob=k;SetCapture(h);setKnob(k,q.x);}}
        else if(auto* g=current();g&&(view==1||bigView))for(int i=0;i<5;i++){ // preset cards: the big row, the detail view's row, or the first screen's right panel
            if((view==0&&!verified(*g))||(bigView&&view==2)){break;}RECT r=bigView?bigPreset(i):presetRect(i);if(PtInRect(&r,bigView?p:q)){if(!presetOk(*g,(dlss::Preset)i)){setStatus(i18n::text(L"이 프리셋은 이 게임에서 지원하지 않습니다."),true);return 0;}
            preset=(dlss::Preset)i;status.clear();loadShots();apxChanged();if(mode==3){apxRefresh();}layout();if(bigView){InvalidateRect(h,nullptr,FALSE);}if(i==4&&opti(*g)&&!bigView&&GetFileAttributesW((gameDir(*g)+L"\\custom.txt").c_str())==INVALID_FILE_ATTRIBUTES)openEditor();return 0;}}
        return 0;}
    case WM_MOUSEWHEEL:if(view==0&&stage&&IsWindowVisible(stage)){turn(GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1);}
        else if(view==0&&gridMode&&!showHidden){int rows=((int)gridList().size()+gridCols)/gridCols,top=std::clamp(gridTop+(GET_WHEEL_DELTA_WPARAM(wp)>0?-1:1),0,std::max(0,rows-gridRows));if(top!=gridTop){gridTop=top;InvalidateRect(h,nullptr,FALSE);}}
        return 0;
    case WM_MOUSELEAVE:if(hubHover>=0){RECT r=hubCard(hubHover);hubHover=-1;InvalidateRect(h,&r,FALSE);}return 0;
    case WM_SETCURSOR:if(LOWORD(lp)==HTCLIENT&&view==3){POINT c;GetCursorPos(&c);ScreenToClient(h,&c);if(hubAt(c)>=0){SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;}}break;
    case WM_MOUSEMOVE:if(view==3){POINT c={((int)(short)LOWORD(lp)),((int)(short)HIWORD(lp))};int i=hubAt(c);if(i!=hubHover){for(int k:{hubHover,i}){if(k>=0){RECT r=hubCard(k);InvalidateRect(h,&r,FALSE);}}hubHover=i;TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}return 0;}
        if(view==2&&!bigView){POINT p=unW({((int)(short)LOWORD(lp)),((int)(short)HIWORD(lp))});if(dragKnob>=0)setKnob(dragKnob,p.x);int k=knobAt(p);if(k!=hoverKnob){hoverKnob=k;InvalidateRect(h,nullptr,FALSE);}}return 0;
    case WM_LBUTTONUP:if(dragKnob>=0){dragKnob=-1;ReleaseCapture();InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_COMMAND:if(LOWORD(wp)==370&&HIWORD(wp)==EN_KILLFOCUS){HWND e=(HWND)lp;if(!GetWindowTextLengthW(e)){ShowWindow(e,SW_HIDE);inval(searchRect);}return 0;}
        if((LOWORD(wp)==370&&HIWORD(wp)==EN_CHANGE)||((LOWORD(wp)==371||LOWORD(wp)==372)&&HIWORD(wp)==CBN_SELCHANGE)){gridTop=0;InvalidateRect(h,nullptr,FALSE);return 0;}
        if(HIWORD(wp)==BN_CLICKED){command(LOWORD(wp));}return 0;
    case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    // Painted once (WM_PAINT), not also on erase; only the invalid part is drawn and copied (the whole page used to be
    // drawn twice for every small change, e.g. the GPU numbers once a second).
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);RECT u=ps.rcPaint;
        if(perfChartFrame(dc,r,u)){EndPaint(h,&ps);return 0;}
        if(flyStart>0&&flyBase&&flyFrame&&view==3){
            HDC f=CreateCompatibleDC(dc),b=CreateCompatibleDC(dc);auto of=SelectObject(f,flyFrame),ob=SelectObject(b,flyBase);BitBlt(f,0,0,r.right,r.bottom,b,0,0,SRCCOPY);paintHub(f);
            BitBlt(dc,u.left,u.top,u.right-u.left,u.bottom-u.top,f,u.left,u.top,SRCCOPY);SelectObject(b,ob);SelectObject(f,of);DeleteDC(b);DeleteDC(f);
            EndPaint(h,&ps);return 0;}
#ifdef FLYLOG
        double t0=nowMs();
#endif
        // A bitmap only as big as the part being redrawn (the name strip under the carousel changes with every card;
        // a whole-page bitmap made that 13 ms at full screen), shifted so the page still draws in page coordinates.
        HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,std::max(1L,u.right-u.left),std::max(1L,u.bottom-u.top));auto old=SelectObject(mem,bmp);SetViewportOrgEx(mem,-u.left,-u.top,nullptr);
        IntersectClipRect(mem,u.left,u.top,u.right,u.bottom);POINT o={};MapWindowPoints(h,win,&o,1);
        if(!copyBackdrop(mem,r,o)){FillRect(mem,&r,backgroundBrush);}paint(mem);
        SetViewportOrgEx(mem,0,0,nullptr);BitBlt(dc,u.left,u.top,u.right-u.left,u.bottom-u.top,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);
#ifdef FLYLOG
        {static double last=0;FILE* lf=_wfopen(L"C:\\Users\\Public\\pagelog.txt",L"a");if(lf){fprintf(lf,"%.1f %.1f\n",t0-last,nowMs()-t0);fclose(lf);}last=t0;}
#endif
        return 0;}
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_DESTROY:KillTimer(h,1);KillTimer(h,3);KillTimer(h,5);endFly();perfDestroy();apxReset();home.clear();rawPlaced.clear();EX=EY=0;mode=3;carDrag=false;for(auto& c:cards)delete c.second;cards.clear();bigCards.clear();for(auto& c:thumbs)delete c.second;thumbs.clear();stopLive();drop(baseBmp);page=nullptr;viewer=nullptr;scanning=false;drop(cmp.a);drop(cmp.b);for(auto& c:covers)delete c.second;covers.clear();for(auto& c:logos)delete c.second;logos.clear();return 0;
    }return DefWindowProcW(h,msg,wp,lp);}
}
