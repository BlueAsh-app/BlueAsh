#pragma once
// Live GPU readings through NVIDIA NVML (nvml.dll, installed with the driver; loaded at run time, nothing bundled):
// utilisation, temperature, power, memory, driver version, name. These are measurements, read once a second while the
// DLSS 5 page is open. Frame rate, frame time and 1 % lows need a present-event trace (PresentMon / ETW), which BlueAsh
// does not do yet: the page says "not measured" rather than estimating. Per-process VRAM is not read either.
#include <windows.h>
#include <string>
namespace dlssgpu {
struct Reading {bool ok=false;std::string name,driver;unsigned util=0,temp=0,watts=0,mhz=0;double usedGB=0,totalGB=0;int throttle=-1;}; // throttle: -1 unknown, else nvmlClocksThrottleReasons bits
typedef int (*Init)();typedef int (*Count)(unsigned*);typedef int (*Handle)(unsigned,void**);typedef int (*Name)(void*,char*,unsigned);typedef int (*Util)(void*,unsigned*);
typedef int (*Temp)(void*,int,unsigned*);typedef int (*Power)(void*,unsigned*);typedef int (*Mem)(void*,unsigned long long*);typedef int (*Driver)(char*,unsigned);typedef int (*Reasons)(void*,unsigned long long*);typedef int (*Clock)(void*,int,unsigned*);
struct Nvml {HMODULE lib=nullptr;bool ready=false;void* dev=nullptr;Name name=nullptr;Util util=nullptr;Temp temp=nullptr;Power power=nullptr;Mem mem=nullptr;Driver driver=nullptr;Reasons reasons=nullptr;Clock clock=nullptr;
    template<class T> T get(const char* n){FARPROC p=GetProcAddress(lib,n);T f=nullptr;memcpy(&f,&p,sizeof(f));return f;}
    bool open(){if(ready)return true;if(lib)return false;wchar_t sys[MAX_PATH]={};GetSystemDirectoryW(sys,MAX_PATH);lib=LoadLibraryExW((std::wstring(sys)+L"\\nvml.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!lib)return false;
        auto init=get<Init>("nvmlInit_v2");auto handle=get<Handle>("nvmlDeviceGetHandleByIndex_v2");name=get<Name>("nvmlDeviceGetName");util=get<Util>("nvmlDeviceGetUtilizationRates");
        temp=get<Temp>("nvmlDeviceGetTemperature");power=get<Power>("nvmlDeviceGetPowerUsage");mem=get<Mem>("nvmlDeviceGetMemoryInfo");driver=get<Driver>("nvmlSystemGetDriverVersion");reasons=get<Reasons>("nvmlDeviceGetCurrentClocksThrottleReasons");clock=get<Clock>("nvmlDeviceGetClockInfo");
        ready=init&&handle&&init()==0&&handle(0,&dev)==0;return ready;}
    // nvmlUtilization_t {gpu, memory}; nvmlMemory_t {total, free, used} (bytes).
    Reading read(){Reading r;if(!open())return r;char b[96]={};if(name&&name(dev,b,96)==0)r.name=b;if(driver&&driver(b,96)==0)r.driver=b;
        unsigned u[2]={};if(util&&util(dev,u)==0)r.util=u[0];if(temp)temp(dev,0,&r.temp);unsigned mw=0;if(power&&power(dev,&mw)==0)r.watts=(mw+500)/1000;
        unsigned long long m[3]={};if(mem&&mem(dev,m)==0){r.totalGB=m[0]/1073741824.0;r.usedGB=m[2]/1073741824.0;}unsigned long long why=0;if(reasons&&reasons(dev,&why)==0)r.throttle=(int)(why&0xFFFF);if(clock)clock(dev,0,&r.mhz);r.ok=true;return r;}
};
// RTX 50 (Blackwell) is where NVIDIA's DLSS 5 runtime starts. Older RTX cards need a modified runtime, which BlueAsh
// does not ship, download or enable (it is not an NVIDIA build and is not shown as supported).
inline bool blackwell(const std::string& name){return name.find("RTX 50")!=std::string::npos||name.find("RTX PRO 6000 Blackwell")!=std::string::npos;}
inline int driverMajor(const std::string& v){return atoi(v.c_str());}
}
