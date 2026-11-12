#pragma once
// 윈도우 최적화 engine (docs/windows-optimization-research.md): what BlueAsh can change on top of 최적화 모드, each through a
// documented Windows API, each change recorded with its old value so it can be put back.
//   history  every change, written as "pending" before it happens and "applied" after, so a crash in between is detectable
//   power    the PC's own power plans (none created), the active one switched by the user
//   display  a monitor running below the highest refresh rate it offers at the same resolution
//   startup  programs started with Windows, switched the way Task Manager does (Explorer\StartupApproved)
//   procs    two-second resource sample per program (CPU, private memory, disk), nothing closed by itself
//   clean    temp / error report / crash dump files older than a day; links never followed, files in use skipped
// No security setting, service or update setting is ever changed here.
#include <windows.h>
#include <powrprof.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdint>
#include <cwctype>
#include <cstdio>

namespace wintune {
inline std::wstring lower(std::wstring s){for(auto& c:s)c=(wchar_t)towlower(c);return s;}
inline std::wstring hex(const std::vector<BYTE>& b){static const wchar_t* d=L"0123456789abcdef";std::wstring s;for(BYTE x:b){s+=d[x>>4];s+=d[x&15];}return s;}
inline bool unhex(const std::wstring& s,std::vector<BYTE>& b){b.clear();if(s.size()%2)return false;auto v=[](wchar_t c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;};
    for(size_t i=0;i<s.size();i+=2){int a=v(s[i]),c=v(s[i+1]);if(a<0||c<0)return false;b.push_back((BYTE)(a*16+c));}return true;}
inline std::wstring nowText(){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[32];swprintf(b,32,L"%04d-%02d-%02d %02d:%02d",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute);return b;}
inline bool elevated(){HANDLE t=nullptr;TOKEN_ELEVATION e={};DWORD n=0;bool r=false;if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&t)){r=GetTokenInformation(t,TokenElevation,&e,sizeof(e),&n)&&e.TokenIsElevated;CloseHandle(t);}return r;}
inline std::wstring env(const wchar_t* v){wchar_t b[MAX_PATH*2]={};DWORD n=GetEnvironmentVariableW(v,b,MAX_PATH*2);return n&&n<MAX_PATH*2?b:L"";}

// ---- History: one line per change, tab separated: id, time, kind, key, before, after, label, state.
// kind: power | display | startup | clean | mode. state: pending | applied | restored | done (clean: not undoable).
struct Entry {std::wstring id,when,kind,key,before,after,label,state;};
struct History {std::wstring path;
    static std::wstring clean(std::wstring s){for(auto& c:s){if(c==L'\t'||c==L'\n'||c==L'\r')c=L' ';}return s;}
    std::vector<Entry> load()const{std::vector<Entry> out;FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return out;std::string raw;char buf[4096];size_t n;while((n=fread(buf,1,sizeof buf,f))>0)raw.append(buf,n);fclose(f);
        int wn=MultiByteToWideChar(CP_UTF8,0,raw.data(),(int)raw.size(),nullptr,0);std::wstring all(wn,L'\0');MultiByteToWideChar(CP_UTF8,0,raw.data(),(int)raw.size(),all.data(),wn);
        size_t at=0;while(at<all.size()){size_t e=all.find(L'\n',at);if(e==std::wstring::npos)e=all.size();std::wstring line=all.substr(at,e-at);at=e+1;if(!line.empty()&&line.back()==L'\r')line.pop_back();
            std::vector<std::wstring> p;size_t s=0;for(;;){size_t t=line.find(L'\t',s);p.push_back(line.substr(s,t==std::wstring::npos?std::wstring::npos:t-s));if(t==std::wstring::npos)break;s=t+1;}
            if(p.size()==8)out.push_back({p[0],p[1],p[2],p[3],p[4],p[5],p[6],p[7]});}return out;}
    bool save(const std::vector<Entry>& all)const{std::wstring t;for(auto& e:all){for(auto* v:{&e.id,&e.when,&e.kind,&e.key,&e.before,&e.after,&e.label}){t+=clean(*v);t+=L'\t';}t+=clean(e.state);t+=L'\n';}
        int n=WideCharToMultiByte(CP_UTF8,0,t.data(),(int)t.size(),nullptr,0,nullptr,nullptr);std::string u(n,'\0');WideCharToMultiByte(CP_UTF8,0,t.data(),(int)t.size(),u.data(),n,nullptr,nullptr);
        std::wstring tmp=path+L".tmp";FILE* f=_wfopen(tmp.c_str(),L"wb");if(!f)return false;bool ok=fwrite(u.data(),1,u.size(),f)==u.size();ok=fflush(f)==0&&ok;fclose(f);if(!ok){DeleteFileW(tmp.c_str());return false;}
        return MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}
    // Kept to the newest 300 changes.
    std::wstring add(Entry e)const{auto all=load();LARGE_INTEGER c;QueryPerformanceCounter(&c);wchar_t id[40];swprintf(id,40,L"%llx",(unsigned long long)c.QuadPart);e.id=id;if(e.when.empty())e.when=nowText();all.push_back(e);
        if(all.size()>300){all.erase(all.begin(),all.end()-300);}return save(all)?e.id:L"";}
    bool mark(const std::wstring& id,const std::wstring& state)const{auto all=load();bool hit=false;for(auto& e:all){if(e.id==id){e.state=state;hit=true;}}return hit&&save(all);}
    bool drop(const std::wstring& id)const{auto all=load();size_t n=all.size();all.erase(std::remove_if(all.begin(),all.end(),[&](const Entry& e){return e.id==id;}),all.end());return all.size()!=n&&save(all);}};

// ---- Power plans: the ones Windows already has (nothing created).
struct Plan {GUID id={};std::wstring name;bool active=false;};
inline std::wstring guidText(const GUID& g){wchar_t b[64];StringFromGUID2(g,b,64);return b;}
inline bool guidOf(const std::wstring& t,GUID& g){return !t.empty()&&IIDFromString(t.c_str(),&g)==S_OK;}
inline std::vector<Plan> plans(){std::vector<Plan> out;GUID* a=nullptr;GUID act={};if(PowerGetActiveScheme(nullptr,&a)==ERROR_SUCCESS&&a){act=*a;LocalFree(a);}
    for(ULONG i=0;;i++){GUID g;DWORD n=sizeof(g);if(PowerEnumerate(nullptr,nullptr,nullptr,ACCESS_SCHEME,i,(UCHAR*)&g,&n)!=ERROR_SUCCESS)break;
        Plan p;p.id=g;p.active=IsEqualGUID(g,act)!=0;DWORD sz=0;if(PowerReadFriendlyName(nullptr,&g,nullptr,nullptr,nullptr,&sz)==ERROR_SUCCESS&&sz>1){std::vector<UCHAR> b(sz);if(PowerReadFriendlyName(nullptr,&g,nullptr,nullptr,b.data(),&sz)==ERROR_SUCCESS)p.name=(wchar_t*)b.data();}
        if(p.name.empty()){p.name=guidText(g);}out.push_back(p);}return out;}
inline bool activePlan(GUID& g){GUID* a=nullptr;if(PowerGetActiveScheme(nullptr,&a)!=ERROR_SUCCESS||!a)return false;g=*a;LocalFree(a);return true;}
inline bool setPlan(const GUID& g){return PowerSetActiveScheme(nullptr,&g)==ERROR_SUCCESS;}
inline bool onBattery(){SYSTEM_POWER_STATUS s={};return GetSystemPowerStatus(&s)&&s.ACLineStatus==0&&s.BatteryFlag!=128;}

// ---- Monitors: refresh rate now and the highest the same resolution and colour depth allow (interlaced modes left out).
struct Mode {int w=0,h=0,bpp=0,hz=0;bool interlaced=false;};
inline int bestHz(const Mode& cur,const std::vector<Mode>& all){int best=cur.hz;for(auto& m:all){if(m.w==cur.w&&m.h==cur.h&&m.bpp==cur.bpp&&!m.interlaced&&m.hz>best&&m.hz<1000)best=m.hz;}return best;}
struct Monitor {std::wstring device,name;Mode cur;int maxHz=0;};
inline std::vector<Monitor> monitors(){std::vector<Monitor> out;DISPLAY_DEVICEW d{};d.cb=sizeof(d);
    for(DWORD i=0;EnumDisplayDevicesW(nullptr,i,&d,0);i++,d=DISPLAY_DEVICEW{},d.cb=sizeof(d)){if(!(d.StateFlags&DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))continue;DEVMODEW c={};c.dmSize=sizeof(c);if(!EnumDisplaySettingsExW(d.DeviceName,ENUM_CURRENT_SETTINGS,&c,0))continue;
        Monitor m;m.device=d.DeviceName;DISPLAY_DEVICEW mon{};mon.cb=sizeof(mon);m.name=EnumDisplayDevicesW(d.DeviceName,0,&mon,0)&&*mon.DeviceString?mon.DeviceString:d.DeviceString;
        m.cur={(int)c.dmPelsWidth,(int)c.dmPelsHeight,(int)c.dmBitsPerPel,(int)c.dmDisplayFrequency,(c.dmDisplayFlags&DM_INTERLACED)!=0};std::vector<Mode> all;DEVMODEW x={};x.dmSize=sizeof(x);
        for(DWORD k=0;EnumDisplaySettingsExW(d.DeviceName,k,&x,0);k++){all.push_back({(int)x.dmPelsWidth,(int)x.dmPelsHeight,(int)x.dmBitsPerPel,(int)x.dmDisplayFrequency,(x.dmDisplayFlags&DM_INTERLACED)!=0});x={};x.dmSize=sizeof(x);}
        m.maxHz=bestHz(m.cur,all);out.push_back(m);}return out;}
// The driver is asked first (CDS_TEST); only a mode it accepts is applied and saved.
inline bool setHz(const std::wstring& device,int hz){DEVMODEW c={};c.dmSize=sizeof(c);if(!EnumDisplaySettingsExW(device.c_str(),ENUM_CURRENT_SETTINGS,&c,0))return false;
    c.dmDisplayFrequency=hz;c.dmFields=DM_DISPLAYFREQUENCY|DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
    if(ChangeDisplaySettingsExW(device.c_str(),&c,nullptr,CDS_TEST,nullptr)!=DISP_CHANGE_SUCCESSFUL){return false;}return ChangeDisplaySettingsExW(device.c_str(),&c,nullptr,CDS_UPDATEREGISTRY,nullptr)==DISP_CHANGE_SUCCESSFUL;}

// ---- Read-only checks.
inline long hklmDword(const wchar_t* path,const wchar_t* name){DWORD v=0,n=sizeof(v);return RegGetValueW(HKEY_LOCAL_MACHINE,path,name,RRF_RT_REG_DWORD,nullptr,&v,&n)==ERROR_SUCCESS?(long)v:-1;}
// Hardware-accelerated GPU scheduling as the graphics driver reports it (D3DKMT WDDM 2.7 caps, gdi32): 1 on, 0 off,
// -1 no adapter offers it. The registry value HwSchMode is often absent while the driver has it on by default.
struct KmtAdapter {UINT h;LUID luid;ULONG sources;BOOL precise;};struct KmtEnum2 {ULONG count;KmtAdapter* list;};struct KmtQuery {UINT h;UINT type;void* data;UINT size;};
inline int gpuScheduling(){HMODULE g=GetModuleHandleW(L"gdi32.dll");if(!g)g=LoadLibraryW(L"gdi32.dll");if(!g)return -1;
    using Enum=LONG(WINAPI*)(KmtEnum2*);using Query=LONG(WINAPI*)(KmtQuery*);using Close=LONG(WINAPI*)(UINT*);
    auto en=(Enum)(void*)GetProcAddress(g,"D3DKMTEnumAdapters2");auto qu=(Query)(void*)GetProcAddress(g,"D3DKMTQueryAdapterInfo");auto cl=(Close)(void*)GetProcAddress(g,"D3DKMTCloseAdapter");
    if(!en||!qu||!cl){long v=hklmDword(L"SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",L"HwSchMode");return v==2?1:v==1?0:-1;}
    KmtEnum2 e={0,nullptr};if(en(&e)!=0||!e.count)return -1;std::vector<KmtAdapter> a(e.count);e.list=a.data();if(en(&e)!=0)return -1;int best=-1;
    for(ULONG i=0;i<e.count;i++){UINT caps=0;KmtQuery q={a[i].h,70,&caps,sizeof(caps)}; /* KMTQAITYPE_WDDM_2_7_CAPS: bit 0 supported, bit 1 enabled */
        if(qu(&q)==0&&(caps&1)){best=std::max(best,(caps&2)?1:0);}UINT h=a[i].h;cl(&h);}return best;}
// Memory integrity (HVCI) as the kernel reports it now (SystemCodeIntegrityInformation, ntdll): 1 on, 0 off, -1 unknown.
inline int memoryIntegrity(){HMODULE n=GetModuleHandleW(L"ntdll.dll");using Q=LONG(WINAPI*)(ULONG,void*,ULONG,ULONG*);auto q=n?(Q)(void*)GetProcAddress(n,"NtQuerySystemInformation"):nullptr;
    if(q){struct {ULONG length,options;} ci={8,0};if(q(103,&ci,sizeof(ci),nullptr)==0)return (ci.options&0x400)?1:0;} /* CODEINTEGRITY_OPTION_HVCI_KMCI_ENABLED */
    long v=hklmDword(L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",L"Enabled");return v<0?-1:v?1:0;}
inline bool systemDriveFree(double& freeGB,double& pct){ULARGE_INTEGER a,t,f;std::wstring d=env(L"SystemDrive")+L"\\";if(d.size()<3)d=L"C:\\";if(!GetDiskFreeSpaceExW(d.c_str(),&a,&t,&f)||!t.QuadPart)return false;freeGB=a.QuadPart/1e9;pct=100.0*a.QuadPart/t.QuadPart;return true;}

// ---- Startup programs. A program's "approved" value (12 bytes) is what Task Manager writes: first byte even = on,
// odd = off, then the FILETIME it was switched off. No value = on.
inline bool approvedOn(const std::vector<BYTE>& b){return b.empty()||(b[0]&1)==0;}
inline std::vector<BYTE> approvedBytes(bool on,FILETIME when){std::vector<BYTE> b(12,0);b[0]=on?2:3;if(!on){memcpy(&b[4],&when,8);}return b;}
struct Startup {std::wstring name,command,file,publisher,where;HKEY root=HKEY_CURRENT_USER;std::wstring approvedKey;bool on=true,admin=false,protectedItem=false;};
const wchar_t* const approvedBase=L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\";
inline bool readBinary(HKEY root,const std::wstring& key,const std::wstring& name,std::vector<BYTE>& out){out.clear();DWORD n=0;if(RegGetValueW(root,key.c_str(),name.c_str(),RRF_RT_REG_BINARY,nullptr,nullptr,&n)!=ERROR_SUCCESS)return false;
    out.resize(n);return RegGetValueW(root,key.c_str(),name.c_str(),RRF_RT_REG_BINARY,nullptr,out.data(),&n)==ERROR_SUCCESS;}
inline bool writeBinary(HKEY root,const std::wstring& key,const std::wstring& name,const std::vector<BYTE>& b){HKEY k=nullptr;if(RegCreateKeyExW(root,key.c_str(),0,nullptr,0,KEY_SET_VALUE,nullptr,&k,nullptr)!=ERROR_SUCCESS)return false;
    bool ok=RegSetValueExW(k,name.c_str(),0,REG_BINARY,b.data(),(DWORD)b.size())==ERROR_SUCCESS;RegCloseKey(k);return ok;}
inline bool removeValue(HKEY root,const std::wstring& key,const std::wstring& name){LONG r=RegDeleteKeyValueW(root,key.c_str(),name.c_str());return r==ERROR_SUCCESS||r==ERROR_FILE_NOT_FOUND||r==ERROR_PATH_NOT_FOUND;}
// The executable a Run command starts: quoted path, or the text up to ".exe".
inline std::wstring exeOf(const std::wstring& cmd){std::wstring c=cmd;wchar_t b[MAX_PATH*2];if(ExpandEnvironmentStringsW(cmd.c_str(),b,MAX_PATH*2))c=b;while(!c.empty()&&c[0]==L' ')c.erase(0,1);
    if(!c.empty()&&c[0]==L'"'){size_t e=c.find(L'"',1);return e==std::wstring::npos?c.substr(1):c.substr(1,e-1);}std::wstring l=lower(c);size_t p=l.find(L".exe");return p==std::wstring::npos?c.substr(0,c.find(L' ')):c.substr(0,p+4);}
inline std::wstring companyOf(const std::wstring& file){DWORD h=0,n=GetFileVersionInfoSizeW(file.c_str(),&h);if(!n)return L"";std::vector<BYTE> b(n);if(!GetFileVersionInfoW(file.c_str(),0,n,b.data()))return L"";
    struct LC{WORD l,c;}* t=nullptr;UINT tn=0;if(!VerQueryValueW(b.data(),L"\\VarFileInfo\\Translation",(void**)&t,&tn)||tn<sizeof(LC))return L"";wchar_t q[64];swprintf(q,64,L"\\StringFileInfo\\%04x%04x\\CompanyName",t[0].l,t[0].c);
    wchar_t* v=nullptr;UINT vn=0;return VerQueryValueW(b.data(),q,(void**)&v,&vn)&&v?std::wstring(v):L"";}
// Security software keeps running from startup: listed, never offered for switching off.
inline bool securityItem(const std::wstring& name,const std::wstring& file,const std::wstring& company){std::wstring s=lower(name+L" "+file+L" "+company);
    for(const wchar_t* k:{L"securityhealth",L"windows defender",L"msmpeng",L"avast",L"avg ",L"kaspersky",L"bitdefender",L"eset",L"norton",L"mcafee",L"ahnlab",L"v3 lite",L"malwarebytes",L"sophos",L"trend micro",L"alyac",L"estsecurity",L"hauri",L"windows security",L"vanguard",L"vgc",L"vgtray",L"easyanticheat",L"battleye",L"faceit",L"eseaclient",L"nprotect",L"xigncode",L"ricochet",L"punkbuster"}){if(s.find(k)!=std::wstring::npos)return true;}return false;}
inline void runKey(std::vector<Startup>& out,HKEY root,const wchar_t* key,const wchar_t* approved,const wchar_t* where,REGSAM view){HKEY k=nullptr;if(RegOpenKeyExW(root,key,0,KEY_READ|view,&k)!=ERROR_SUCCESS)return;
    for(DWORD i=0;;i++){wchar_t n[512];DWORD nn=512,type=0,dn=0;if(RegEnumValueW(k,i,n,&nn,nullptr,&type,nullptr,&dn)!=ERROR_SUCCESS)break;if((type!=REG_SZ&&type!=REG_EXPAND_SZ)||!nn)continue;
        std::vector<wchar_t> d(dn/2+2,0);DWORD dn2=dn;nn=512;if(RegEnumValueW(k,i,n,&nn,nullptr,&type,(BYTE*)d.data(),&dn2)!=ERROR_SUCCESS)continue;
        Startup s;s.name=n;s.command=d.data();if(lower(s.name)==L"modeswitch"&&lower(s.command).find(L"blueash.exe")!=std::wstring::npos)s.name=L"BlueAsh";s.root=root;s.approvedKey=std::wstring(approvedBase)+approved;s.where=where;s.admin=root==HKEY_LOCAL_MACHINE;s.file=exeOf(s.command);s.publisher=companyOf(s.file);
        std::vector<BYTE> b;readBinary(root,s.approvedKey,s.name,b);s.on=approvedOn(b);s.protectedItem=securityItem(s.name,s.file,s.publisher);out.push_back(s);}RegCloseKey(k);}
inline void folder(std::vector<Startup>& out,REFKNOWNFOLDERID id,HKEY root,const wchar_t* where){PWSTR p=nullptr;if(SHGetKnownFolderPath(id,0,nullptr,&p)!=S_OK){if(p)CoTaskMemFree(p);return;}std::wstring dir=p;CoTaskMemFree(p);
    WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((dir+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return;
    do{if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;std::wstring n=f.cFileName;if(lower(n)==L"desktop.ini")continue;Startup s;s.name=n;size_t dot=n.rfind(L'.');if(dot!=std::wstring::npos&&dot>0)s.name=n.substr(0,dot);
        s.command=dir+L"\\"+n;s.file=s.command;s.root=root;s.approvedKey=std::wstring(approvedBase)+L"StartupFolder";s.where=where;s.admin=root==HKEY_LOCAL_MACHINE;s.publisher=companyOf(s.file);
        std::vector<BYTE> b;readBinary(root,s.approvedKey,n,b);s.on=approvedOn(b);s.protectedItem=securityItem(s.name,s.file,s.publisher);s.command=n; /* the approved value is named after the file */out.push_back(s);}while(FindNextFileW(h,&f));FindClose(h);}
// where: "user" = this account, "all" = every account (needs administrator rights to switch)
inline std::vector<Startup> startups(){std::vector<Startup> out;const wchar_t* run=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    runKey(out,HKEY_CURRENT_USER,run,L"Run",L"user",0);runKey(out,HKEY_LOCAL_MACHINE,run,L"Run",L"all",KEY_WOW64_64KEY);runKey(out,HKEY_LOCAL_MACHINE,run,L"Run32",L"all",KEY_WOW64_32KEY);
    folder(out,FOLDERID_Startup,HKEY_CURRENT_USER,L"user-folder");folder(out,FOLDERID_CommonStartup,HKEY_LOCAL_MACHINE,L"all-folder");
    std::sort(out.begin(),out.end(),[](const Startup& a,const Startup& b){if(a.on!=b.on)return a.on;return lower(a.name)<lower(b.name);});return out;}
// The approved value's name: the Run value's name, or the file name for the startup folders.
inline std::wstring approvedName(const Startup& s){return s.where.find(L"folder")!=std::wstring::npos?s.command:s.name;}

// ---- Programs using the PC now: CPU %, private memory and disk traffic over a short sample, grouped by program name.
struct Proc {std::wstring name;int count=0;double cpu=0,ramMB=0,diskMBs=0;std::vector<DWORD> pids;bool window=false;std::wstring kind;};
struct Snap {std::map<DWORD,std::pair<ULONGLONG,ULONGLONG>> t;std::map<DWORD,std::wstring> name;std::map<DWORD,double> ram;ULONGLONG at=0;};
inline Snap snapshot(){Snap s;FILETIME n;GetSystemTimeAsFileTime(&n);s.at=((ULONGLONG)n.dwHighDateTime<<32)|n.dwLowDateTime;HANDLE h=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(h==INVALID_HANDLE_VALUE)return s;
    PROCESSENTRY32W e{};e.dwSize=sizeof(e);for(BOOL ok=Process32FirstW(h,&e);ok;ok=Process32NextW(h,&e)){if(e.th32ProcessID==0)continue;HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(!p)continue;
        FILETIME c,x,k,u;IO_COUNTERS io={};ULONGLONG cpu=0,disk=0;if(GetProcessTimes(p,&c,&x,&k,&u)){cpu=(((ULONGLONG)k.dwHighDateTime<<32)|k.dwLowDateTime)+(((ULONGLONG)u.dwHighDateTime<<32)|u.dwLowDateTime);}
        if(GetProcessIoCounters(p,&io)){disk=io.ReadTransferCount+io.WriteTransferCount;}PROCESS_MEMORY_COUNTERS_EX m={};m.cb=sizeof(m);if(GetProcessMemoryInfo(p,(PROCESS_MEMORY_COUNTERS*)&m,sizeof(m))){s.ram[e.th32ProcessID]=m.PrivateUsage/1048576.0;}
        CloseHandle(p);s.t[e.th32ProcessID]={cpu,disk};s.name[e.th32ProcessID]=e.szExeFile;}CloseHandle(h);return s;}
// A rough kind for a program name, so the list reads at a glance (shown as a hint, never used to close anything).
inline const wchar_t* kindOf(const std::wstring& exe){std::wstring n=lower(exe);auto has=[&](std::initializer_list<const wchar_t*> l){for(auto* k:l)if(n.find(k)!=std::wstring::npos)return true;return false;};
    if(has({L"chrome",L"msedge.exe",L"firefox",L"brave",L"opera",L"whale",L"vivaldi"}))return L"browser";
    if(has({L"steam",L"epicgameslauncher",L"battle.net",L"eadesktop",L"origin",L"ubisoftconnect",L"upc.exe",L"riotclient",L"galaxyclient",L"xboxapp",L"gamebar"}))return L"launcher";
    if(has({L"discord",L"obs64",L"obs32",L"medal",L"overwolf",L"nvidia overlay",L"nvcontainer",L"radeonsoftware",L"amdrsserv",L"msiafterburner",L"rtss"}))return L"overlay";
    if(has({L"icue",L"lghub",L"razer",L"armourycrate",L"nzxt",L"signalrgb",L"openrgb",L"steelseries",L"wallpaper"}))return L"device";
    if(has({L"onedrive",L"dropbox",L"googledrive",L"icloud",L"megasync"}))return L"sync";
    if(has({L"update",L"installer",L"setup"})){return L"update";}return L"";}
inline bool systemProc(const std::wstring& exe){static const wchar_t* s[]={L"system",L"registry",L"idle",L"smss.exe",L"csrss.exe",L"wininit.exe",L"winlogon.exe",L"services.exe",L"lsass.exe",L"svchost.exe",L"dwm.exe",L"explorer.exe",L"fontdrvhost.exe",
    L"memory compression",L"msmpeng.exe",L"nissrv.exe",L"securityhealthservice.exe",L"spoolsv.exe",L"audiodg.exe",L"conhost.exe",L"sihost.exe",L"taskhostw.exe",L"ctfmon.exe",L"searchhost.exe",L"startmenuexperiencehost.exe",L"shellexperiencehost.exe",L"runtimebroker.exe",L"textinputhost.exe",L"lsaiso.exe",L"wudfhost.exe",L"dllhost.exe",L"sgrmbroker.exe",L"mpdefendercoreservice.exe"};
    std::wstring n=lower(exe);for(auto* x:s)if(n==x)return true;return false;}
inline bool hasWindow(DWORD pid){struct F{DWORD pid;bool found;} f{pid,false};EnumWindows([](HWND h,LPARAM l)->BOOL{auto* f=(F*)l;DWORD p=0;GetWindowThreadProcessId(h,&p);if(p==f->pid&&IsWindowVisible(h)&&!GetWindow(h,GW_OWNER)){f->found=true;return FALSE;}return TRUE;},(LPARAM)&f);return f.found;}
inline std::vector<Proc> compare(const Snap& a,const Snap& b,DWORD self){std::map<std::wstring,Proc> by;double sec=(b.at>a.at?(b.at-a.at)/1e7:1.0);SYSTEM_INFO si;GetSystemInfo(&si);double cores=std::max<DWORD>(1,si.dwNumberOfProcessors);
    for(auto& [pid,v]:b.t){auto it=a.t.find(pid);if(it==a.t.end()||pid==self)continue;std::wstring n=b.name.at(pid);if(systemProc(n))continue;auto& p=by[lower(n)];if(p.name.empty()){p.name=n;p.kind=kindOf(n);}
        p.count++;p.pids.push_back(pid);double dc=v.first>=it->second.first?(double)(v.first-it->second.first):0,dd=v.second>=it->second.second?(double)(v.second-it->second.second):0;
        p.cpu+=dc/1e7/sec/cores*100;p.diskMBs+=dd/1048576.0/sec;auto r=b.ram.find(pid);if(r!=b.ram.end())p.ramMB+=r->second;}
    std::vector<Proc> out;for(auto& [k,p]:by){for(DWORD pid:p.pids){if(hasWindow(pid)){p.window=true;break;}}out.push_back(p);}
    std::sort(out.begin(),out.end(),[](const Proc& x,const Proc& y){double sx=x.cpu*4+x.ramMB/256+x.diskMBs,sy=y.cpu*4+y.ramMB/256+y.diskMBs;return sx>sy;});return out;}

// ---- Clean: files older than minAge (hours) inside a folder, never through a link, files in use skipped.
struct Spot {std::wstring id;std::vector<std::wstring> dirs;bool admin=false;unsigned long long bytes=0;int files=0;bool readable=true;};
inline std::vector<Spot> spots(){std::vector<Spot> s;wchar_t t[MAX_PATH+1]={};std::wstring temp=GetTempPathW(MAX_PATH,t)?t:L"";while(!temp.empty()&&(temp.back()==L'\\'))temp.pop_back();
    std::wstring local=env(L"LOCALAPPDATA"),data=env(L"ProgramData"),win=env(L"WINDIR");
    s.push_back({L"temp",{temp}});s.push_back({L"wintemp",{win+L"\\Temp"},true});
    s.push_back({L"reports",{local+L"\\Microsoft\\Windows\\WER\\ReportArchive",local+L"\\Microsoft\\Windows\\WER\\ReportQueue"}});
    s.push_back({L"reports-all",{data+L"\\Microsoft\\Windows\\WER\\ReportArchive",data+L"\\Microsoft\\Windows\\WER\\ReportQueue"},true});
    s.push_back({L"dumps",{local+L"\\CrashDumps"}});
    // an unset variable would leave "\Temp" etc., the root of the current drive: such a spot is dropped
    s.erase(std::remove_if(s.begin(),s.end(),[](const Spot& p){for(auto& d:p.dirs)if(d.size()<3||d[1]!=L':')return true;return false;}),s.end());return s;}
inline bool oldEnough(const FILETIME& w,const FILETIME& now,double hours){ULONGLONG a=((ULONGLONG)w.dwHighDateTime<<32)|w.dwLowDateTime,b=((ULONGLONG)now.dwHighDateTime<<32)|now.dwLowDateTime;return b>a&&(b-a)/36e9>=hours;}
struct Swept {unsigned long long bytes=0;int files=0,skipped=0;bool readable=true;};
// del=false only counts. Depth-limited; reparse points (junctions, symlinks) are neither followed nor deleted.
inline void sweep(const std::wstring& dir,double hours,bool del,Swept& r,int depth=0){if(depth>16||dir.size()<4)return;WIN32_FIND_DATAW f;HANDLE h=FindFirstFileExW((dir+L"\\*").c_str(),FindExInfoBasic,&f,FindExSearchNameMatch,nullptr,FIND_FIRST_EX_LARGE_FETCH);
    if(h==INVALID_HANDLE_VALUE){if(depth==0&&GetLastError()==ERROR_ACCESS_DENIED)r.readable=false;return;}FILETIME now;GetSystemTimeAsFileTime(&now);
    do{std::wstring n=f.cFileName;if(n==L"."||n==L"..")continue;std::wstring p=dir+L"\\"+n;if(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)continue;
        if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){sweep(p,hours,del,r,depth+1);if(del&&oldEnough(f.ftLastWriteTime,now,hours))RemoveDirectoryW(p.c_str());continue;} /* only empty folders go */
        if(!oldEnough(f.ftLastWriteTime,now,hours)){continue;}unsigned long long size=((unsigned long long)f.nFileSizeHigh<<32)|f.nFileSizeLow;
        if(!del){r.bytes+=size;r.files++;continue;}if(f.dwFileAttributes&FILE_ATTRIBUTE_READONLY){r.skipped++;continue;}if(DeleteFileW(p.c_str())){r.bytes+=size;r.files++;}else r.skipped++;}while(FindNextFileW(h,&f));FindClose(h);}
}
