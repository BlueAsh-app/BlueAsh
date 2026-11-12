#pragma once
// 스마트 부스트: Windows changes BlueAsh may make for a game, each one opt-in, recorded before it is made and undone
// when the game exits, when the person presses 복원, or at the next BlueAsh start if BlueAsh closed in between.
//  - Power plan: switches to the High / Ultimate performance plan that already exists on this PC (PowerSetActiveScheme,
//    no admin rights). A missing plan is not created. The previous plan's GUID goes to boost.txt first.
// Read-only here (the page shows them and opens the Windows page; nothing is written): Game Mode, windowed
// optimizations. Background programs: listed with their CPU and memory use; one is closed only after the person confirms,
// and only by asking its windows to close (WM_CLOSE: the program may save or refuse) — no process is killed.
// Not done on purpose: Defender, updates, services, priorities, memory purges, overclocking, HAGS, registry tweaks.
#include <windows.h>
#include <powrprof.h>
#include <string>
#include <vector>
#include <memory>
#include "perf_trace.h"
namespace perfboost {
const GUID highPerf={0x8c5e7fda,0xe8bf,0x4a96,{0x9a,0x85,0xa6,0xe2,0x3a,0x8c,0x63,0x5c}};
const GUID ultimate={0xe9a42b02,0xd5df,0x448d,{0xaa,0x00,0x03,0xf1,0x47,0x49,0xeb,0x61}};
inline std::wstring guidText(const GUID& g){wchar_t b[64];StringFromGUID2(g,b,64);return b;}
inline bool guidOf(const std::wstring& t,GUID& g){return !t.empty()&&IIDFromString(t.c_str(),&g)==S_OK;}
inline bool active(GUID& g){GUID* a=nullptr;if(PowerGetActiveScheme(nullptr,&a)!=ERROR_SUCCESS||!a)return false;g=*a;LocalFree(a);return true;}
inline bool exists(const GUID& want){for(ULONG i=0;;i++){GUID g;DWORD n=sizeof(g);if(PowerEnumerate(nullptr,nullptr,nullptr,ACCESS_SCHEME,i,(UCHAR*)&g,&n)!=ERROR_SUCCESS)return false;if(IsEqualGUID(g,want))return true;}}
// The plan to switch to: Ultimate if present, else High performance; none = the PC offers neither (common on laptops).
inline bool gamePlan(GUID& g){if(exists(ultimate)){g=ultimate;return true;}if(exists(highPerf)){g=highPerf;return true;}return false;}
inline bool isGamePlan(const GUID& g){return IsEqualGUID(g,ultimate)||IsEqualGUID(g,highPerf);}
// Journal: power=<GUID to restore>, pid=<game process>, game=<key>. Written before the change; removed after the restore.
struct Journal {std::wstring path;GUID power={};bool hasPower=false;DWORD pid=0;std::wstring game;
    bool load(){power={};hasPower=false;pid=0;game.clear();FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return false;char b[512]={};size_t n=fread(b,1,511,f);fclose(f);std::string s(b,n);
        auto val=[&](const char* k){auto at=s.find(k);if(at==std::string::npos)return std::string();at+=strlen(k);auto e=s.find('\n',at);return s.substr(at,e==std::string::npos?std::string::npos:e-at);};
        std::string p=val("power=");std::wstring w(p.begin(),p.end());while(!w.empty()&&(w.back()==L'\r'||w.back()==L' '))w.pop_back();hasPower=guidOf(w,power);pid=(DWORD)atol(val("pid=").c_str());std::string g=val("game=");game.assign(g.begin(),g.end());return true;}
    bool save(){std::string g(game.begin(),game.end()),p;std::wstring t=guidText(power);p.assign(t.begin(),t.end());char b[512];snprintf(b,512,"power=%s\npid=%lu\ngame=%s\n",hasPower?p.c_str():"",(unsigned long)pid,g.c_str());
        FILE* f=_wfopen(path.c_str(),L"wb");if(!f)return false;size_t n=fwrite(b,1,strlen(b),f);return fclose(f)==0&&n==strlen(b);}
    bool exists()const{return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}};
enum class Apply {Done,Already,Unsupported,Failed};
// Switches to the game plan for pid. Running it twice keeps the first recorded plan (no stacking).
inline Apply applyPower(Journal& j,DWORD pid,const std::wstring& game){GUID now={},want={};if(!active(now))return Apply::Failed;if(!gamePlan(want))return Apply::Unsupported;
    bool had=j.load()&&j.hasPower;if(isGamePlan(now)&&!had)return Apply::Already;
    if(!had){j.power=now;j.hasPower=true;}j.pid=pid;j.game=game;if(!j.save())return Apply::Failed; // recorded before changing anything
    if(!IsEqualGUID(now,want)&&PowerSetActiveScheme(nullptr,&want)!=ERROR_SUCCESS){return Apply::Failed;}return Apply::Done;}
// Puts the recorded plan back. False = it could not (the journal stays, so a later attempt or the person can still do it).
inline bool restore(Journal& j){if(!j.load())return true;if(j.hasPower){GUID now={};if(!(active(now)&&IsEqualGUID(now,j.power))&&PowerSetActiveScheme(nullptr,&j.power)!=ERROR_SUCCESS)return false;}
    return DeleteFileW(j.path.c_str())!=FALSE||GetLastError()==ERROR_FILE_NOT_FOUND;}
inline bool alive(DWORD pid){if(!pid)return false;HANDLE h=OpenProcess(SYNCHRONIZE,FALSE,pid);if(!h)return false;bool up=WaitForSingleObject(h,0)==WAIT_TIMEOUT;CloseHandle(h);return up;}
// Waits (own thread) for the boosted game to exit, then restores. done(ok) runs on that thread.
inline void restoreWhenGone(const std::wstring& path,DWORD pid,void (*done)(bool)){struct W{std::wstring path;DWORD pid;void (*done)(bool);};auto* w=new W{path,pid,done};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<W> w((W*)p);HANDLE h=OpenProcess(SYNCHRONIZE,FALSE,w->pid);if(h){WaitForSingleObject(h,INFINITE);CloseHandle(h);}
        Journal j;j.path=w->path;if(!j.load()||j.pid!=w->pid)return 0;bool ok=restore(j);if(w->done)w->done(ok);return 0;},w,0,nullptr);if(t)CloseHandle(t);else delete w;}
// Programs a person may close: a visible window in this session, not BlueAsh, not the game, not part of Windows.
inline bool systemName(const std::wstring& n){static const wchar_t* sys[]={L"explorer.exe",L"dwm.exe",L"csrss.exe",L"winlogon.exe",L"svchost.exe",L"lsass.exe",L"services.exe",L"smss.exe",L"wininit.exe",L"System",L"Registry",
        L"MsMpEng.exe",L"SecurityHealthService.exe",L"SearchHost.exe",L"StartMenuExperienceHost.exe",L"ShellExperienceHost.exe",L"TextInputHost.exe",L"ctfmon.exe",L"sihost.exe",L"fontdrvhost.exe",L"audiodg.exe",L"taskhostw.exe",L"RuntimeBroker.exe",L"ApplicationFrameHost.exe",L"SystemSettings.exe",L"LockApp.exe",L"conhost.exe",L"powershell.exe",L"BlueAsh.exe"};
    for(auto* s:sys){if(!_wcsicmp(n.c_str(),s))return true;}return false;}
inline bool hasWindow(DWORD pid){struct F{DWORD pid;bool found;} f{pid,false};EnumWindows([](HWND h,LPARAM l)->BOOL{auto* f=(F*)l;DWORD p=0;GetWindowThreadProcessId(h,&p);if(p==f->pid&&IsWindowVisible(h)&&!GetWindow(h,GW_OWNER)){f->found=true;return FALSE;}return TRUE;},(LPARAM)&f);return f.found;}
inline std::vector<perftrace::Busy> closable(const std::vector<perftrace::Busy>& all,DWORD game,size_t max){std::vector<perftrace::Busy> out;DWORD me=GetCurrentProcessId(),sess=0,own=0;ProcessIdToSessionId(me,&own);
    for(auto& b:all){if(out.size()>=max)break;if(b.pid==game||b.pid==me||systemName(b.name)||!ProcessIdToSessionId(b.pid,&sess)||sess!=own||!hasWindow(b.pid))continue;out.push_back(b);}return out;}
// Asks the program's top-level windows to close (the program decides; unsaved work prompts stay its own).
inline int closeWindows(DWORD pid){struct F{DWORD pid;int n;} f{pid,0};EnumWindows([](HWND h,LPARAM l)->BOOL{auto* f=(F*)l;DWORD p=0;GetWindowThreadProcessId(h,&p);if(p==f->pid&&IsWindowVisible(h)&&!GetWindow(h,GW_OWNER)){PostMessageW(h,WM_CLOSE,0,0);f->n++;}return TRUE;},(LPARAM)&f);return f.n;}
}
// 최적화 모드: Windows settings switched together for games, all per-user and all the same switches Windows Settings
// offers (no admin rights, no services, nothing hidden):
//  - power plan: the High / Ultimate performance plan already on this PC (not created when missing)
//  - Game Mode on (HKCU\Software\Microsoft\GameBar AutoGameModeEnabled = 1)
//  - Xbox Game Bar background recording off (HKCU\System\GameConfigStore GameDVR_Enabled = 0,
//    HKCU\...\CurrentVersion\GameDVR AppCaptureEnabled = 0)
// Every original value goes to the journal (opt-mode.txt) before the first change; a value that did not exist is
// recorded as absent and removed again on restore. Turning the mode off, quitting BlueAsh, or the next start after a
// crash puts everything back. Applying twice keeps the first recorded originals.
namespace optmode {
const wchar_t* gameBar=L"Software\\Microsoft\\GameBar";const wchar_t* gameConfig=L"System\\GameConfigStore";
const wchar_t* personalize=L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
const wchar_t* gameDvr=L"Software\\Microsoft\\Windows\\CurrentVersion\\GameDVR";const wchar_t* directX=L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
inline long readDword(const wchar_t* path,const wchar_t* name){DWORD v=0,n=sizeof(v);return RegGetValueW(HKEY_CURRENT_USER,path,name,RRF_RT_REG_DWORD,nullptr,&v,&n)==ERROR_SUCCESS?(long)v:-1;}
inline bool readText(const wchar_t* path,const wchar_t* name,std::wstring& out){wchar_t b[1024]={};DWORD n=sizeof(b);if(RegGetValueW(HKEY_CURRENT_USER,path,name,RRF_RT_REG_SZ,nullptr,b,&n)!=ERROR_SUCCESS)return false;out=b;return true;}
inline bool writeDword(const wchar_t* path,const wchar_t* name,DWORD v){HKEY k=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return false;bool ok=RegSetValueExW(k,name,0,REG_DWORD,(const BYTE*)&v,sizeof(v))==ERROR_SUCCESS;RegCloseKey(k);return ok;}
inline bool writeText(const wchar_t* path,const wchar_t* name,const std::wstring& v){HKEY k=nullptr;if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return false;bool ok=RegSetValueExW(k,name,0,REG_SZ,(const BYTE*)v.c_str(),(DWORD)((v.size()+1)*sizeof(wchar_t)))==ERROR_SUCCESS;RegCloseKey(k);return ok;}
inline bool removeValue(const wchar_t* path,const wchar_t* name){LONG r=RegDeleteKeyValueW(HKEY_CURRENT_USER,path,name);return r==ERROR_SUCCESS||r==ERROR_FILE_NOT_FOUND;}
// Window animations and effects (SystemParametersInfo get/set pairs; font smoothing is left alone). Applied live and saved
// to the user profile; the originals go in the journal as one bit per entry ("anim"), the minimize/maximize animation as bit 15.
const UINT animGet[]={SPI_GETCLIENTAREAANIMATION,SPI_GETMENUANIMATION,SPI_GETCOMBOBOXANIMATION,SPI_GETLISTBOXSMOOTHSCROLLING,SPI_GETTOOLTIPANIMATION,SPI_GETCURSORSHADOW,SPI_GETDROPSHADOW,SPI_GETSELECTIONFADE,SPI_GETUIEFFECTS};
inline int animNow(){int bits=0;for(int i=0;i<9;i++){BOOL b=FALSE;if(SystemParametersInfoW(animGet[i],0,&b,0)&&b)bits|=1<<i;}ANIMATIONINFO a={sizeof(a),0};if(SystemParametersInfoW(SPI_GETANIMATION,sizeof(a),&a,0)&&a.iMinAnimate)bits|=1<<15;return bits;}
inline bool animSet(int bits){bool ok=true;for(int i=0;i<9;i++){ok=SystemParametersInfoW(animGet[i]+1,0,(PVOID)(INT_PTR)((bits>>i)&1),SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)&&ok;}
    ANIMATIONINFO a={sizeof(a),(bits>>15)&1};return SystemParametersInfoW(SPI_SETANIMATION,sizeof(a),&a,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)&&ok;}
// Transparency: DWM picks it up from the "ImmersiveColorSet" broadcast.
inline void themeChanged(){DWORD_PTR r=0;SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,(LPARAM)L"ImmersiveColorSet",SMTO_ABORTIFHUNG,200,&r);}
// Mouse acceleration ("enhance pointer precision") = the third SPI_GETMOUSE value; the two thresholds stay.
inline bool mouseGet(int m[3]){return SystemParametersInfoW(SPI_GETMOUSE,0,m,0)!=0;}
inline bool mouseSet(const int m[3]){int v[3]={m[0],m[1],m[2]};return SystemParametersInfoW(SPI_SETMOUSE,0,v,SPIF_UPDATEINIFILE|SPIF_SENDCHANGE)!=0;}
// What is in place now (for the page): true = the game-friendly value.
struct Now {bool power=false,gameMode=false,dvr=false,windowed=false,visual=false,mouse=false;std::wstring plan;};
inline Now now(){Now n;GUID g={};if(perfboost::active(g)){n.power=perfboost::isGamePlan(g);}long gm=readDword(gameBar,L"AutoGameModeEnabled");n.gameMode=gm!=0;
    n.dvr=readDword(gameConfig,L"GameDVR_Enabled")==0&&readDword(gameDvr,L"AppCaptureEnabled")!=1;std::wstring dx;n.windowed=readText(directX,L"DirectXUserGlobalSettings",dx)&&dx.find(L"SwapEffectUpgradeEnable=1")!=std::wstring::npos;
    n.visual=animNow()==0&&readDword(personalize,L"EnableTransparency")==0;int m[3]={};n.mouse=mouseGet(m)&&m[2]==0;return n;}
// Journal: one "key=value" per line (UTF-8). Values: a GUID, a number, "-" = did not exist, or the original text.
struct Journal {std::wstring path;std::map<std::string,std::wstring> v;
    bool load(){v.clear();FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return false;std::string s;char b[4096];size_t n;while((n=fread(b,1,sizeof(b),f))>0)s.append(b,n);fclose(f);size_t i=0;
        while(i<s.size()){size_t e=s.find('\n',i);if(e==std::string::npos)e=s.size();std::string l=s.substr(i,e-i);i=e+1;auto q=l.find('=');if(q==std::string::npos)continue;
            std::string val=l.substr(q+1);int w=MultiByteToWideChar(CP_UTF8,0,val.data(),(int)val.size(),nullptr,0);std::wstring wv(w,0);MultiByteToWideChar(CP_UTF8,0,val.data(),(int)val.size(),&wv[0],w);v[l.substr(0,q)]=wv;}
        return true;}
    bool save(){std::string s;for(auto& [k,w]:v){int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string u(n,0);WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&u[0],n,nullptr,nullptr);s+=k+"="+u+"\n";}
        // written beside and swapped in: a crash mid-write must not lose the original values
        std::wstring tmp=path+L".tmp";FILE* f=_wfopen(tmp.c_str(),L"wb");if(!f)return false;size_t n=fwrite(s.data(),1,s.size(),f);bool ok=fflush(f)==0&&n==s.size();ok=fclose(f)==0&&ok;
        if(!ok){DeleteFileW(tmp.c_str());return false;}return MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}
    bool exists()const{return GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;}};
inline std::wstring dwordText(long v){return v<0?L"-":std::to_wstring(v);}
// Applies everything. Returns how many settings could not be changed (0 = all done). The journal is saved before any change.
inline int apply(Journal& j){j.load();
    if(!j.v.count("power")){GUID g={};j.v["power"]=perfboost::active(g)?perfboost::guidText(g):L"-";}
    if(!j.v.count("gamemode"))j.v["gamemode"]=dwordText(readDword(gameBar,L"AutoGameModeEnabled"));
    if(!j.v.count("dvr"))j.v["dvr"]=dwordText(readDword(gameConfig,L"GameDVR_Enabled"));
    if(!j.v.count("capture"))j.v["capture"]=dwordText(readDword(gameDvr,L"AppCaptureEnabled"));
    // 창 게임 최적화 (flip model for windowed DX11) is no longer part of the mode: with BlueAsh's own overlays over PUBG it
    // made the game stutter. A value an older version changed is put back here.
    {auto it=j.v.find("windowed");if(it!=j.v.end()){if(it->second==L"-"?removeValue(directX,L"DirectXUserGlobalSettings"):writeText(directX,L"DirectXUserGlobalSettings",it->second.substr(1))){j.v.erase(it);}}}
    if(!j.v.count("anim"))j.v["anim"]=std::to_wstring(animNow());
    if(!j.v.count("transparency"))j.v["transparency"]=dwordText(readDword(personalize,L"EnableTransparency"));
    if(!j.v.count("mouse")){int m[3]={};if(mouseGet(m))j.v["mouse"]=std::to_wstring(m[0])+L","+std::to_wstring(m[1])+L","+std::to_wstring(m[2]);}
    if(!j.save()){return 5;}int failed=0;
    GUID want={},cur={};if(perfboost::gamePlan(want)&&perfboost::active(cur)&&!IsEqualGUID(cur,want)&&PowerSetActiveScheme(nullptr,&want)!=ERROR_SUCCESS)failed++;
    if(!writeDword(gameBar,L"AutoGameModeEnabled",1))failed++;
    if(!writeDword(gameConfig,L"GameDVR_Enabled",0))failed++;
    if(!writeDword(gameDvr,L"AppCaptureEnabled",0))failed++;
    if(!animSet(0))failed++;
    if(writeDword(personalize,L"EnableTransparency",0)){themeChanged();}else failed++;
    {int m[3]={};if(!mouseGet(m)){failed++;}else if(m[2]){m[2]=0;if(!mouseSet(m))failed++;}}
    return failed;}
// Puts every recorded value back. True when all are back (the journal is then removed); false keeps it for another try.
inline bool restore(Journal& j){if(!j.load())return true;bool ok=true;
    auto dword=[&](const char* key,const wchar_t* path,const wchar_t* name){auto it=j.v.find(key);if(it==j.v.end())return;if(it->second==L"-"){ok=removeValue(path,name)&&ok;}else{ok=writeDword(path,name,(DWORD)_wtol(it->second.c_str()))&&ok;}};
    {auto it=j.v.find("power");GUID g={},cur={};if(it!=j.v.end()&&perfboost::guidOf(it->second,g)&&!(perfboost::active(cur)&&IsEqualGUID(cur,g))){ok=PowerSetActiveScheme(nullptr,&g)==ERROR_SUCCESS&&ok;}}
    dword("gamemode",gameBar,L"AutoGameModeEnabled");dword("dvr",gameConfig,L"GameDVR_Enabled");dword("capture",gameDvr,L"AppCaptureEnabled");
    {auto it=j.v.find("windowed");if(it!=j.v.end()){if(it->second==L"-"){ok=removeValue(directX,L"DirectXUserGlobalSettings")&&ok;}else{ok=writeText(directX,L"DirectXUserGlobalSettings",it->second.substr(1))&&ok;}}}
    {auto it=j.v.find("anim");if(it!=j.v.end()){ok=animSet(_wtoi(it->second.c_str()))&&ok;}}
    {auto it=j.v.find("transparency");if(it!=j.v.end()){ok=(it->second==L"-"?removeValue(personalize,L"EnableTransparency"):writeDword(personalize,L"EnableTransparency",(DWORD)_wtol(it->second.c_str())))&&ok;themeChanged();}}
    {auto it=j.v.find("mouse");int m[3]={};if(it!=j.v.end()&&swscanf(it->second.c_str(),L"%d,%d,%d",&m[0],&m[1],&m[2])==3){ok=mouseSet(m)&&ok;}}
    if(ok){DeleteFileW(j.path.c_str());}return ok;}
}
