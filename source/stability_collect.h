#pragma once
// Stability Guard, the Windows side: reads the event logs (wevtapi: EvtQuery/EvtNext/EvtRender/EvtFormatMessage), keeps
// a small ring of system readings so a crash can be matched with the state just before it, and lists the changes BlueAsh
// itself made (graphics, optimization) around that time. Everything stays on this PC.
#include <winevt.h>
#include "stability_core.h"
namespace stabcollect {
struct Result {std::vector<stability::Event> events;std::wstring error;bool denied=false;};
inline std::wstring renderXml(EVT_HANDLE e){DWORD used=0,props=0;EvtRender(nullptr,e,EvtRenderEventXml,0,nullptr,&used,&props);if(!used)return L"";std::wstring b(used/sizeof(wchar_t)+1,L'\0');
    if(!EvtRender(nullptr,e,EvtRenderEventXml,(DWORD)(b.size()*sizeof(wchar_t)),&b[0],&used,&props)){return L"";}b.resize(wcslen(b.c_str()));return b;}
inline std::wstring formatMessage(EVT_HANDLE pub,EVT_HANDLE e){if(!pub)return L"";DWORD used=0;EvtFormatMessage(pub,e,0,0,nullptr,EvtFormatMessageEvent,0,nullptr,&used);if(!used)return L"";std::wstring b(used+1,L'\0');
    if(!EvtFormatMessage(pub,e,0,0,nullptr,EvtFormatMessageEvent,(DWORD)b.size(),&b[0],&used)){return L"";}b.resize(wcslen(b.c_str()));return b;}
// The records Stability Guard reads, newest first, from the last `days` days. Only those it uses get their message
// formatted (that call is the slow part). Worker thread only.
inline Result collect(int days){Result r;std::map<std::wstring,EVT_HANDLE> pubs;
    wchar_t age[64];swprintf(age,64,L" and TimeCreated[timediff(@SystemTime) <= %lld]",(long long)days*86400000LL);
    const std::wstring system=L"*[System[((Provider[@Name='Microsoft-Windows-Kernel-Power'] and EventID=41) or (Provider[@Name='Microsoft-Windows-WER-SystemErrorReporting'] and EventID=1001)"
        L" or (Provider[@Name='Display'] and EventID=4101) or Provider[@Name='nvlddmkm'] or Provider[@Name='amdkmdag'] or Provider[@Name='amdwddmg'] or Provider[@Name='Microsoft-Windows-WHEA-Logger']"
        L" or (Provider[@Name='disk'] and (EventID=7 or EventID=11 or EventID=51 or EventID=153)) or (Provider[@Name='Ntfs'] and EventID=55) or (Provider[@Name='EventLog'] and EventID=6008)"
        L" or (Provider[@Name='User32'] and EventID=1074))"+std::wstring(age)+L"]]";
    const std::wstring app=L"*[System[((Provider[@Name='Application Error'] and EventID=1000) or (Provider[@Name='Application Hang'] and EventID=1002) or (Provider[@Name='Windows Error Reporting'] and EventID=1001))"+std::wstring(age)+L"]]";
    for(auto& q:{std::make_pair(L"System",system),std::make_pair(L"Application",app)}){
        EVT_HANDLE h=EvtQuery(nullptr,q.first,q.second.c_str(),EvtQueryChannelPath|EvtQueryReverseDirection);
        if(!h){DWORD err=GetLastError();if(err==ERROR_ACCESS_DENIED)r.denied=true;r.error+=std::wstring(q.first)+L" log: error "+std::to_wstring(err)+L"\n";continue;}
        EVT_HANDLE batch[64];DWORD got=0;int total=0;
        while(total<4000&&EvtNext(h,64,batch,5000,0,&got)){for(DWORD i=0;i<got;i++){std::wstring xml=renderXml(batch[i]);stability::Event e=stability::parse(xml);
                if(stability::kindOf(e)!=stability::Kind::None||stability::context(e)){auto it=pubs.find(e.provider);if(it==pubs.end())it=pubs.emplace(e.provider,EvtOpenPublisherMetadata(nullptr,e.provider.c_str(),nullptr,0,0)).first;
                    e.message=formatMessage(it->second,batch[i]);r.events.push_back(e);}
                EvtClose(batch[i]);}total+=got;}
        EvtClose(h);}
    for(auto& p:pubs){if(p.second)EvtClose(p.second);}return r;}
// Minutes east of UTC now (crash dumps are named in local time).
inline long long utcOffset(){TIME_ZONE_INFORMATION z;DWORD m=GetTimeZoneInformation(&z);long b=z.Bias+(m==TIME_ZONE_ID_DAYLIGHT?z.DaylightBias:0);return -b;}
inline stability::Time now(){FILETIME f;GetSystemTimeAsFileTime(&f);return ((stability::Time)f.dwHighDateTime<<32)|f.dwLowDateTime;}
inline stability::Time bootTime(){return now()-(stability::Time)GetTickCount64()*10000ULL;}

// ---- The state just before a crash: one line every 10 s (CPU, RAM, the GPU's own readings, the foreground program),
// the newest 15 minutes kept in memory and written to %LOCALAPPDATA%\BlueAsh\stability-telemetry.tsv once a minute. At
// start the previous session's file becomes stability-telemetry-prev.tsv, so the minutes before a power cut survive the
// restart. GPU power is the card's own draw, not the PC's; no reading becomes "측정 불가", never a guess.
struct Row {stability::Time time=0;std::wstring exe;int cpu=-1,ram=-1,gpu=-1,temp=-1,watts=-1,mhz=-1;double vram=-1,vramTotal=-1;};
inline std::wstring dataDir(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+dataSub();CreateDirectoryW(r.c_str(),nullptr);return r;}
inline std::wstring telemetryPath(bool previous=false){return dataDir()+(previous?L"\\stability-telemetry-prev.tsv":L"\\stability-telemetry.tsv");}
inline std::wstring foregroundExe(){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);if(!pid)return L"";HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return L"";
    wchar_t b[1024];DWORD n=1024;std::wstring s;if(QueryFullProcessImageNameW(p,0,b,&n))s=stability::fileName(b);CloseHandle(p);return s;}
inline std::wstring rowText(const Row& r){wchar_t b[256];swprintf(b,256,L"%llu\t%ls\t%d\t%d\t%d\t%d\t%d\t%d\t%.2f\t%.2f",r.time,r.exe.c_str(),r.cpu,r.ram,r.gpu,r.temp,r.watts,r.mhz,r.vram,r.vramTotal);return b;}
inline bool parseRow(const std::wstring& line,Row& r){std::vector<std::wstring> p;size_t s=0;for(;;){size_t t=line.find(L'\t',s);p.push_back(line.substr(s,t==std::wstring::npos?std::wstring::npos:t-s));if(t==std::wstring::npos)break;s=t+1;}
    if(p.size()!=10){return false;}r.time=_wcstoui64(p[0].c_str(),nullptr,10);r.exe=p[1];int* f[]={&r.cpu,&r.ram,&r.gpu,&r.temp,&r.watts,&r.mhz};for(int i=0;i<6;i++){*f[i]=_wtoi(p[2+i].c_str());}r.vram=_wtof(p[8].c_str());r.vramTotal=_wtof(p[9].c_str());return r.time!=0;}
struct Recorder {std::vector<Row> ring;std::mutex lock;std::atomic<bool> running{false};std::wstring gpuName,driver;HANDLE thread=nullptr;
    void start(){if(running.exchange(true))return;std::wstring cur=telemetryPath(),prev=telemetryPath(true);if(GetFileAttributesW(cur.c_str())!=INVALID_FILE_ATTRIBUTES)MoveFileExW(cur.c_str(),prev.c_str(),MOVEFILE_REPLACE_EXISTING);
        thread=CreateThread(nullptr,0,[](LPVOID p)->DWORD{((Recorder*)p)->loop();return 0;},this,0,nullptr);}
    void stop(){if(!running.exchange(false))return;if(thread){WaitForSingleObject(thread,3000);CloseHandle(thread);thread=nullptr;}flush();}
    void loop(){SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);dlssgpu::Nvml nvml;FILETIME li={},lk={},lu={};GetSystemTimes(&li,&lk,&lu);int tick=0;
        auto q=[](FILETIME f){return ((unsigned long long)f.dwHighDateTime<<32)|f.dwLowDateTime;};
        while(running){for(int i=0;i<20&&running;i++)Sleep(500);if(!running)break;Row r;r.time=now();r.exe=foregroundExe();
            FILETIME ci,ck,cu;if(GetSystemTimes(&ci,&ck,&cu)){unsigned long long idle=q(ci)-q(li),busy=(q(ck)-q(lk))+(q(cu)-q(lu));if(busy>0)r.cpu=(int)(100-(idle*100/busy));li=ci;lk=ck;lu=cu;}
            MEMORYSTATUSEX m={};m.dwLength=sizeof(m);if(GlobalMemoryStatusEx(&m))r.ram=(int)m.dwMemoryLoad;
            auto g=nvml.read();if(g.ok){r.gpu=(int)g.util;r.temp=(int)g.temp;r.watts=(int)g.watts;r.mhz=(int)g.mhz;r.vram=g.usedGB;r.vramTotal=g.totalGB;std::lock_guard<std::mutex> k(lock);gpuName=std::wstring(g.name.begin(),g.name.end());driver=std::wstring(g.driver.begin(),g.driver.end());}
            {std::lock_guard<std::mutex> k(lock);ring.push_back(r);if(ring.size()>90)ring.erase(ring.begin());}
            if(++tick%6==0)flush();}}
    void flush(){std::wstring t;{std::lock_guard<std::mutex> k(lock);t=L"#gpu\t"+gpuName+L"\t"+driver+L"\n";for(auto& r:ring)t+=rowText(r)+L"\n";}
        int n=WideCharToMultiByte(CP_UTF8,0,t.c_str(),(int)t.size(),nullptr,0,nullptr,nullptr);std::string u(n,'\0');WideCharToMultiByte(CP_UTF8,0,t.c_str(),(int)t.size(),&u[0],n,nullptr,nullptr);
        std::wstring path=telemetryPath(),tmp=path+L".tmp";FILE* f=_wfopen(tmp.c_str(),L"wb");if(!f)return;bool ok=fwrite(u.data(),1,u.size(),f)==u.size();fclose(f);if(ok)MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);else DeleteFileW(tmp.c_str());}};
inline Recorder recorder;
// Readings from both files (this session and the previous one), oldest first; gpu/driver from the newest header.
inline std::vector<Row> readings(std::wstring* gpu=nullptr,std::wstring* driver=nullptr){std::vector<Row> out;
    for(bool prev:{true,false}){FILE* f=_wfopen(telemetryPath(prev).c_str(),L"rb");if(!f)continue;std::string raw;char b[4096];size_t n;while((n=fread(b,1,sizeof b,f))>0)raw.append(b,n);fclose(f);
        int wn=MultiByteToWideChar(CP_UTF8,0,raw.data(),(int)raw.size(),nullptr,0);std::wstring all(wn,L'\0');MultiByteToWideChar(CP_UTF8,0,raw.data(),(int)raw.size(),&all[0],wn);
        size_t at=0;while(at<all.size()){size_t e=all.find(L'\n',at);if(e==std::wstring::npos)e=all.size();std::wstring line=all.substr(at,e-at);at=e+1;
            if(line.rfind(L"#gpu\t",0)==0){size_t t=line.find(L'\t',5);if(gpu&&t!=std::wstring::npos&&t>5)*gpu=line.substr(5,t-5);if(driver&&t!=std::wstring::npos)*driver=line.substr(t+1);continue;}
            Row r;if(parseRow(line,r))out.push_back(r);}}
    std::sort(out.begin(),out.end(),[](const Row& a,const Row& b){return a.time<b.time;});return out;}

// ---- Changes BlueAsh made shortly before: Windows optimization history and DLSS 5 applies (their record files' times).
struct Change {stability::Time time=0;std::wstring label;int page=0;}; // page: 88 = Windows optimization, 86 = graphics settings
inline stability::Time fileTime(const std::wstring& path){WIN32_FILE_ATTRIBUTE_DATA a;if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&a))return 0;return ((stability::Time)a.ftLastWriteTime.dwHighDateTime<<32)|a.ftLastWriteTime.dwLowDateTime;}
inline stability::Time localText(const std::wstring& when){int Y,M,D,h,m;if(swscanf(when.c_str(),L"%d-%d-%d %d:%d",&Y,&M,&D,&h,&m)!=5)return 0;wchar_t iso[40];swprintf(iso,40,L"%04d-%02d-%02dT%02d:%02d:00Z",Y,M,D,h,m);
    stability::Time t=stability::isoTime(iso);return t?(stability::Time)((long long)t-utcOffset()*(long long)stability::minute):0;}
}
