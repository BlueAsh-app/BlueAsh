#pragma once
// Measuring a running game, on a worker thread:
//  - Frames: an ETW real-time session on the Microsoft-Windows-DXGI provider, filtered to the game's process id. Every
//    IDXGISwapChain::Present call logs Present_Start (event 42) with the swap chain and the flags; the frame time is the
//    time between two presents of the busiest swap chain (the same basis as PresentMon's MsBetweenPresents). Present
//    calls with DXGI_PRESENT_TEST are not frames and are skipped. Nothing is injected into the game; Windows records the
//    events. Real-time ETW needs administrator rights (or the Performance Log Users group): without them start() says so.
//    Games that present through Vulkan/OpenGL without DXGI log no such events: then "no present events".
//    This is the presented (displayed) frame rate: with frame generation the generated frames may be included, so it is
//    never reported as the render rate (perf_core.h lowers the confidence).
//  - Around the frames, every 500 ms: CPU load per core and in total, CPU performance vs base clock, 3D engine load and
//    dedicated memory of the GPU (Windows performance counters, any GPU vendor), free RAM, and NVIDIA readings (NVML).
//  - Other processes' CPU use over the same time (process times, nothing else read).
// Intel PresentMon (the SDK needs its service installed) and AMD ADLX are not used: neither is bundled with BlueAsh.
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <atomic>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include "perf_core.h"
#include "dlss_gpu.h"
#ifndef EVENT_FILTER_TYPE_PID
#define EVENT_FILTER_TYPE_PID 0x80000004
#endif
namespace perftrace {
const wchar_t* sessionName=L"BlueAsh-PresentTrace";
const GUID dxgi={0xCA11C036,0x0102,0x4A2D,{0xA6,0xAD,0xF0,0x3C,0xFE,0xD5,0xD3,0xC9}};
struct Present {ULONGLONG chain;LONGLONG at;int sync;};
struct Sink {DWORD pid=0;std::vector<Present> got;};
inline void WINAPI onEvent(PEVENT_RECORD e){auto* s=(Sink*)e->UserContext;if(!s||!IsEqualGUID(e->EventHeader.ProviderId,dxgi)||e->EventHeader.EventDescriptor.Id!=42||e->EventHeader.ProcessId!=s->pid)return;
    size_t ptr=(e->EventHeader.Flags&EVENT_HEADER_FLAG_32_BIT_HEADER)?4:8;if(!e->UserData||e->UserDataLength<ptr+8)return;const BYTE* d=(const BYTE*)e->UserData;
    ULONGLONG chain=0;memcpy(&chain,d,ptr);UINT32 flags=0;INT32 sync=0;memcpy(&flags,d+ptr,4);memcpy(&sync,d+ptr+4,4);if(flags&1)return; // DXGI_PRESENT_TEST
    s->got.push_back({chain,e->EventHeader.TimeStamp.QuadPart,sync});}
struct Props {std::vector<BYTE> b;EVENT_TRACE_PROPERTIES* p(){return (EVENT_TRACE_PROPERTIES*)b.data();}
    Props(){size_t n=sizeof(EVENT_TRACE_PROPERTIES)+(wcslen(sessionName)+1)*sizeof(wchar_t);b.assign(n,0);auto* q=p();q->Wnode.BufferSize=(ULONG)n;q->Wnode.Flags=WNODE_FLAG_TRACED_GUID;q->Wnode.ClientContext=1; // QPC time stamps
        q->LogFileMode=EVENT_TRACE_REAL_TIME_MODE;q->LoggerNameOffset=sizeof(EVENT_TRACE_PROPERTIES);}};
// A session left running (BlueAsh closed mid-measurement) keeps logging in Windows: stopped before a new one and at exit.
inline void stopLeftover(){Props p;ControlTraceW(0,sessionName,p.p(),EVENT_TRACE_CONTROL_STOP);}
// Other processes' CPU time, to find what competes with the game.
struct ProcTimes {std::map<DWORD,std::pair<std::wstring,ULONGLONG>> t;ULONGLONG at=0;};
inline ProcTimes procTimes(){ProcTimes r;FILETIME now;GetSystemTimeAsFileTime(&now);r.at=((ULONGLONG)now.dwHighDateTime<<32)|now.dwLowDateTime;HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(s==INVALID_HANDLE_VALUE)return r;
    PROCESSENTRY32W e={};e.dwSize=sizeof(e);for(BOOL ok=Process32FirstW(s,&e);ok;ok=Process32NextW(s,&e)){if(!e.th32ProcessID)continue;HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(!h)continue;
        FILETIME c,x,k,u;if(GetProcessTimes(h,&c,&x,&k,&u))r.t[e.th32ProcessID]={e.szExeFile,(((ULONGLONG)k.dwHighDateTime<<32)|k.dwLowDateTime)+(((ULONGLONG)u.dwHighDateTime<<32)|u.dwLowDateTime)};CloseHandle(h);}
    CloseHandle(s);return r;}
struct Busy {DWORD pid;std::wstring name;double cpu;double mb;};
// CPU % of the whole machine (100 = every logical processor busy) between two snapshots, busiest first.
inline std::vector<Busy> busiest(const ProcTimes& a,const ProcTimes& b,DWORD skip1,DWORD skip2){std::vector<Busy> out;SYSTEM_INFO si;GetSystemInfo(&si);double span=(double)(b.at-a.at)*si.dwNumberOfProcessors;if(span<=0)return out;
    for(auto& [pid,v]:b.t){if(pid==skip1||pid==skip2)continue;auto it=a.t.find(pid);if(it==a.t.end()||it->second.first!=v.first||v.second<it->second.second)continue;
        Busy x{pid,v.first,(v.second-it->second.second)*100.0/span,0};HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);PROCESS_MEMORY_COUNTERS m={};
        if(h){if(K32GetProcessMemoryInfo(h,&m,sizeof(m)))x.mb=m.WorkingSetSize/1048576.0;CloseHandle(h);}out.push_back(x);}
    std::sort(out.begin(),out.end(),[](const Busy& x,const Busy& y){return x.cpu!=y.cpu?x.cpu>y.cpu:x.mb>y.mb;});return out;}
// Windows performance counters (English names, so they work in every Windows language).
struct Counters {PDH_HQUERY q=nullptr;PDH_HCOUNTER cores=nullptr,perf=nullptr,gpu=nullptr,vram=nullptr,avail=nullptr,total=nullptr;
    bool open(){if(PdhOpenQueryW(nullptr,0,&q)!=ERROR_SUCCESS)return false;PdhAddEnglishCounterW(q,L"\\Processor(*)\\% Processor Time",0,&cores);PdhAddEnglishCounterW(q,L"\\Processor(_Total)\\% Processor Time",0,&total);
        PdhAddEnglishCounterW(q,L"\\Processor Information(_Total)\\% Processor Performance",0,&perf);PdhAddEnglishCounterW(q,L"\\GPU Engine(*engtype_3D)\\Utilization Percentage",0,&gpu);
        PdhAddEnglishCounterW(q,L"\\GPU Adapter Memory(*)\\Dedicated Usage",0,&vram);PdhAddEnglishCounterW(q,L"\\Memory\\Available MBytes",0,&avail);PdhCollectQueryData(q);return true;}
    ~Counters(){if(q)PdhCloseQuery(q);}
    static bool one(PDH_HCOUNTER c,double& v){PDH_FMT_COUNTERVALUE x={};if(!c||PdhGetFormattedCounterValue(c,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,nullptr,&x)!=ERROR_SUCCESS||x.CStatus!=ERROR_SUCCESS)return false;v=x.doubleValue;return true;}
    template<class F> static bool each(PDH_HCOUNTER c,F f){if(!c)return false;DWORD size=0,n=0;if(PdhGetFormattedCounterArrayW(c,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,&size,&n,nullptr)!=(PDH_STATUS)PDH_MORE_DATA)return false;
        std::vector<BYTE> b(size);auto* items=(PDH_FMT_COUNTERVALUE_ITEM_W*)b.data();if(PdhGetFormattedCounterArrayW(c,PDH_FMT_DOUBLE|PDH_FMT_NOCAP100,&size,&n,items)!=ERROR_SUCCESS)return false;
        for(DWORD i=0;i<n;i++){if(items[i].FmtValue.CStatus==ERROR_SUCCESS)f(std::wstring(items[i].szName),items[i].FmtValue.doubleValue);}return true;}};
// Readings averaged over the measurement (perf::Sample fields filled; the rest stays "not measured").
struct Averages {double n=0,gpu=0,core=0,total=0,perf=0,vram=-1,avail=-1;int gpuN=0,coreN=0,totalN=0,perfN=0;double temp=-1;int thermal=-1,power=-1;};
inline void sample(Counters& c,dlssgpu::Nvml* nvml,Averages& a){PdhCollectQueryData(c.q);double v=0;
    double maxCore=-1;Counters::each(c.cores,[&](const std::wstring& name,double x){if(name!=L"_Total")maxCore=std::max(maxCore,x);});if(maxCore>=0){a.core+=std::min(100.0,maxCore);a.coreN++;}
    if(Counters::one(c.total,v)){a.total+=v;a.totalN++;}if(Counters::one(c.perf,v)){a.perf+=v;a.perfN++;}
    std::map<std::wstring,double> byGpu;bool g=Counters::each(c.gpu,[&](const std::wstring& name,double x){auto at=name.find(L"luid_");byGpu[at==std::wstring::npos?L"":name.substr(at,30)]+=x;});
    if(g){double best=0;for(auto& [k,x]:byGpu)best=std::max(best,x);a.gpu+=std::min(100.0,best);a.gpuN++;}
    double used=-1;Counters::each(c.vram,[&](const std::wstring&,double x){used=std::max(used,x);});if(used>=0)a.vram=std::max(a.vram,used/1073741824.0);
    if(Counters::one(c.avail,v))a.avail=a.avail<0?v:std::min(a.avail,v);
    if(nvml){auto r=nvml->read();if(r.ok){a.temp=std::max(a.temp,(double)r.temp);if(r.throttle>=0){a.thermal=std::max(a.thermal,(r.throttle&0x60)?1:0);a.power=std::max(a.power,(r.throttle&0x84)?1:0);}}}}
struct Result {std::wstring error;bool needAdmin=false;std::vector<double> ms;int vsync=-1;perf::Sample x;std::vector<Busy> others;bool gameExited=false;};
// Measures pid for `seconds`. alive() is polled (the game may exit); tick(percent) reports progress; cancel stops early.
inline Result measure(DWORD pid,int seconds,dlssgpu::Nvml* nvml,const std::function<bool()>& alive,const std::function<void(int)>& tick,const std::atomic<bool>& cancel){Result r;
    stopLeftover();Props props;TRACEHANDLE session=0;ULONG st=StartTraceW(&session,sessionName,props.p());
    if(st==ERROR_ACCESS_DENIED){r.needAdmin=true;r.error=L"FPS 측정(ETW)에는 관리자 권한이 필요합니다.";return r;}
    if(st!=ERROR_SUCCESS){r.error=L"FPS 측정 세션을 시작하지 못했습니다.";return r;}
    EVENT_FILTER_DESCRIPTOR filter={};DWORD pids[1]={pid};filter.Ptr=(ULONGLONG)pids;filter.Size=sizeof(DWORD);filter.Type=EVENT_FILTER_TYPE_PID;
    ENABLE_TRACE_PARAMETERS params={};params.Version=ENABLE_TRACE_PARAMETERS_VERSION_2;params.EnableFilterDesc=&filter;params.FilterDescCount=1;
    if(EnableTraceEx2(session,&dxgi,EVENT_CONTROL_CODE_ENABLE_PROVIDER,TRACE_LEVEL_INFORMATION,~0ULL,0,0,&params)!=ERROR_SUCCESS){ControlTraceW(session,nullptr,props.p(),EVENT_TRACE_CONTROL_STOP);r.error=L"FPS 측정 세션을 시작하지 못했습니다.";return r;}
    Sink sink;sink.pid=pid;EVENT_TRACE_LOGFILEW lf={};lf.LoggerName=(LPWSTR)sessionName;lf.ProcessTraceMode=PROCESS_TRACE_MODE_REAL_TIME|PROCESS_TRACE_MODE_EVENT_RECORD;lf.EventRecordCallback=onEvent;lf.Context=&sink;
    TRACEHANDLE consumer=OpenTraceW(&lf);bool opened=consumer!=(TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE;
    HANDLE th=opened?CreateThread(nullptr,0,[](LPVOID p)->DWORD{TRACEHANDLE h=*(TRACEHANDLE*)p;ProcessTrace(&h,1,nullptr,nullptr);return 0;},&consumer,0,nullptr):nullptr;
    Counters c;bool counters=c.open();Averages a;auto before=procTimes();ULONGLONG start=GetTickCount64(),end=start+seconds*1000ULL;int last=-1;
    while(GetTickCount64()<end&&!cancel){Sleep(500);if(counters)sample(c,nvml,a);if(alive&&!alive()){r.gameExited=true;break;}int pct=(int)((GetTickCount64()-start)*100/(seconds*1000ULL));if(pct!=last&&tick){last=pct;tick(std::min(99,pct));}}
    auto after=procTimes();
    ControlTraceW(session,nullptr,props.p(),EVENT_TRACE_CONTROL_STOP);if(th){WaitForSingleObject(th,5000);CloseHandle(th);}if(opened)CloseTrace(consumer);
    if(!opened){r.error=L"FPS 측정 세션을 시작하지 못했습니다.";return r;}
    // The busiest swap chain is the game's view; others (launchers, overlays) are ignored.
    std::map<ULONGLONG,int> count;for(auto& p:sink.got)count[p.chain]++;ULONGLONG main=0;int best=0;for(auto& [k,n]:count)if(n>best){best=n;main=k;}
    LARGE_INTEGER fq;QueryPerformanceFrequency(&fq);LONGLONG prev=0;std::map<int,int> syncs;
    for(auto& p:sink.got){if(p.chain!=main)continue;if(prev)r.ms.push_back((p.at-prev)*1000.0/fq.QuadPart);prev=p.at;syncs[p.sync]++;}
    int bestSync=0;for(auto& [k,n]:syncs)if(n>bestSync){bestSync=n;r.vsync=k;}
    if(sink.got.empty()&&!cancel&&!r.gameExited)r.error=L"이 게임의 Present 이벤트가 없습니다 (Vulkan·OpenGL 게임이거나 화면이 멈춰 있을 수 있음).";
    if(a.gpuN){r.x.gpuUtil=a.gpu/a.gpuN;}if(a.coreN)r.x.cpuMaxCore=a.core/a.coreN;if(a.totalN)r.x.cpuTotal=a.total/a.totalN;if(a.perfN)r.x.cpuPerf=a.perf/a.perfN;
    r.x.vramUsed=a.vram;r.x.gpuTemp=a.temp;r.x.thermal=a.thermal;r.x.power=a.power;
    MEMORYSTATUSEX ms={};ms.dwLength=sizeof(ms);if(GlobalMemoryStatusEx(&ms)&&a.avail>=0){r.x.ramTotalGB=ms.ullTotalPhys/1073741824.0;r.x.ramAvailPct=a.avail*1048576.0/ms.ullTotalPhys*100;}
    r.others=busiest(before,after,pid,GetCurrentProcessId());if(!r.others.empty()){r.x.topProc=r.others[0].name;r.x.topProcCpu=r.others[0].cpu;}
    if(tick){tick(100);}return r;}
}
