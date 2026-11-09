#pragma once
// This PC's hardware and Windows state, read with documented Windows APIs only (no driver, no admin rights, nothing
// installed): WMI (Win32_Processor / Win32_PhysicalMemory / Win32_BaseBoard), DXGI, NVML (NVIDIA, dlss_gpu.h), the
// power API, the display settings and a few registry values. Run on a worker thread (WMI takes 100 ms or more). A value
// that could not be read stays at its "unknown" default (-1 / 0 / empty) and the page shows "측정 불가" for it.
// Not read on purpose: memory timings and XMP/EXPO state (no standard Windows API), memory channels and free slots
// (SMBIOS data is not reliable enough), AMD sensors (AMD's ADLX SDK is not bundled).
#include <windows.h>
#include <wbemidl.h>
#include <powrprof.h>
#include <dxgi1_4.h>
#include <winioctl.h>
#include <string>
#include <vector>
#include <functional>
namespace perfhw {
struct Module {double gb=0;int speed=0,configured=0,type=0;std::wstring slot;};
struct Info {
    std::wstring cpu;int cores=0,threads=0,baseMhz=0;
    std::wstring gpu,driver;unsigned vendor=0;double vramGB=0;
    double ramGB=0;std::vector<Module> modules;bool modulesRead=false;
    std::wstring board,os;int build=0;
    std::wstring powerPlan;GUID power={};bool powerSaver=false,battery=false;
    int width=0,height=0,refresh=0,gameMode=-1,windowedOpt=-1;
    std::wstring storage;bool reboot=false;
};
inline const wchar_t* vendorName(unsigned v){return v==0x10DE?L"NVIDIA":v==0x1002?L"AMD":v==0x8086?L"Intel":L"";}
inline const wchar_t* ddrName(int t){switch(t){case 20:return L"DDR";case 21:return L"DDR2";case 24:return L"DDR3";case 26:return L"DDR4";case 34:return L"DDR5";case 30:return L"LPDDR4";case 35:return L"LPDDR5";default:return L"";}}
// Rated speed (Speed) and the speed the memory runs at (ConfiguredClockSpeed): both MT/s for DDR4/DDR5 in WMI.
inline int ratedMts(const Info& i){int r=0;for(auto& m:i.modules)r=r?std::min(r,m.speed):m.speed;return r;}
inline int configuredMts(const Info& i){int r=0;for(auto& m:i.modules)r=r?std::min(r,m.configured):m.configured;return r;}
inline std::wstring regText(HKEY root,const wchar_t* path,const wchar_t* name){wchar_t b[512]={};DWORD n=sizeof(b);return RegGetValueW(root,path,name,RRF_RT_REG_SZ,nullptr,b,&n)==ERROR_SUCCESS?b:L"";}
inline bool regExists(HKEY root,const wchar_t* path,const wchar_t* name=nullptr){HKEY k=nullptr;if(RegOpenKeyExW(root,path,0,KEY_READ,&k)!=ERROR_SUCCESS)return false;bool ok=!name||RegQueryValueExW(k,name,nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;RegCloseKey(k);return ok;}
// One WMI query; f gets each row. Fails quietly (no rows) when WMI is unavailable.
inline void wmi(const wchar_t* query,const std::function<void(IWbemClassObject*)>& f){
    IWbemLocator* loc=nullptr;IWbemServices* svc=nullptr;IEnumWbemClassObject* e=nullptr;
    if(FAILED(CoCreateInstance(CLSID_WbemLocator,nullptr,CLSCTX_INPROC_SERVER,IID_IWbemLocator,(void**)&loc))||!loc)return;
    BSTR ns=SysAllocString(L"ROOT\\CIMV2"),lang=SysAllocString(L"WQL"),q=SysAllocString(query);
    if(SUCCEEDED(loc->ConnectServer(ns,nullptr,nullptr,nullptr,0,nullptr,nullptr,&svc))&&svc){
        CoSetProxyBlanket(svc,RPC_C_AUTHN_WINNT,RPC_C_AUTHZ_NONE,nullptr,RPC_C_AUTHN_LEVEL_CALL,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE);
        if(SUCCEEDED(svc->ExecQuery(lang,q,WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY,nullptr,&e))&&e){
            for(;;){IWbemClassObject* o=nullptr;ULONG n=0;if(e->Next(5000,1,&o,&n)!=S_OK||!n)break;f(o);o->Release();}e->Release();}
        svc->Release();}
    SysFreeString(ns);SysFreeString(lang);SysFreeString(q);loc->Release();}
inline std::wstring wstr(IWbemClassObject* o,const wchar_t* n){VARIANT v;VariantInit(&v);std::wstring r;if(SUCCEEDED(o->Get(n,0,&v,nullptr,nullptr))&&v.vt==VT_BSTR&&v.bstrVal)r=v.bstrVal;VariantClear(&v);
    while(!r.empty()&&r.back()==L' '){r.pop_back();}return r;}
inline long long wnum(IWbemClassObject* o,const wchar_t* n){VARIANT v;VariantInit(&v);long long r=0;if(SUCCEEDED(o->Get(n,0,&v,nullptr,nullptr))){
    if(v.vt==VT_BSTR&&v.bstrVal)r=_wtoi64(v.bstrVal);else if(v.vt==VT_I4||v.vt==VT_UI4)r=v.lVal;else if(v.vt==VT_I2||v.vt==VT_UI2)r=v.iVal;else if(v.vt==VT_UI1)r=v.bVal;}VariantClear(&v);return r;}
// The drive a path is on: NVMe SSD / SSD / HDD (seek penalty and bus type; no access rights needed).
inline std::wstring storageOf(const std::wstring& path){if(path.size()<2||path[1]!=L':')return L"";std::wstring dev=L"\\\\.\\"+path.substr(0,2);
    HANDLE h=CreateFileW(dev.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return L"";
    STORAGE_PROPERTY_QUERY q={};q.QueryType=PropertyStandardQuery;DWORD n=0;std::wstring r;
    q.PropertyId=StorageDeviceSeekPenaltyProperty;DEVICE_SEEK_PENALTY_DESCRIPTOR sp={};bool seekOk=DeviceIoControl(h,IOCTL_STORAGE_QUERY_PROPERTY,&q,sizeof(q),&sp,sizeof(sp),&n,nullptr)!=FALSE;
    q.PropertyId=StorageDeviceProperty;BYTE buf[1024]={};bool devOk=DeviceIoControl(h,IOCTL_STORAGE_QUERY_PROPERTY,&q,sizeof(q),buf,sizeof(buf),&n,nullptr)!=FALSE;CloseHandle(h);
    bool nvme=devOk&&((STORAGE_DEVICE_DESCRIPTOR*)buf)->BusType==17; // BusTypeNvme
    if(nvme)r=L"NVMe SSD";else if(seekOk)r=sp.IncursSeekPenalty?L"HDD":L"SSD";return r;}
// gamePath: the game's folder (for its drive), may be empty. Must run on a thread that may call CoInitializeEx.
inline Info collect(const std::wstring& gamePath){Info i;
    HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr); // fails harmlessly if already set
    wmi(L"SELECT Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed FROM Win32_Processor",[&](IWbemClassObject* o){if(i.cpu.empty())i.cpu=wstr(o,L"Name");i.cores+=(int)wnum(o,L"NumberOfCores");i.threads+=(int)wnum(o,L"NumberOfLogicalProcessors");if(!i.baseMhz)i.baseMhz=(int)wnum(o,L"MaxClockSpeed");});
    if(i.cpu.empty())i.cpu=regText(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",L"ProcessorNameString");
    if(!i.threads){SYSTEM_INFO si;GetSystemInfo(&si);i.threads=(int)si.dwNumberOfProcessors;}
    wmi(L"SELECT Capacity,Speed,ConfiguredClockSpeed,SMBIOSMemoryType,DeviceLocator FROM Win32_PhysicalMemory",[&](IWbemClassObject* o){Module m;m.gb=wnum(o,L"Capacity")/1073741824.0;m.speed=(int)wnum(o,L"Speed");m.configured=(int)wnum(o,L"ConfiguredClockSpeed");m.type=(int)wnum(o,L"SMBIOSMemoryType");m.slot=wstr(o,L"DeviceLocator");if(m.gb>0)i.modules.push_back(m);});
    i.modulesRead=!i.modules.empty();
    wmi(L"SELECT Manufacturer,Product FROM Win32_BaseBoard",[&](IWbemClassObject* o){if(i.board.empty()){i.board=wstr(o,L"Manufacturer");std::wstring p=wstr(o,L"Product");if(!p.empty())i.board+=(i.board.empty()?L"":L" ")+p;}});
    if(SUCCEEDED(co))CoUninitialize();
    MEMORYSTATUSEX ms={};ms.dwLength=sizeof(ms);if(GlobalMemoryStatusEx(&ms))i.ramGB=ms.ullTotalPhys/1073741824.0;
    // GPU: the first hardware adapter (DXGI); its user-mode driver version as Windows reports it.
    IDXGIFactory1* f=nullptr;if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),(void**)&f))){IDXGIAdapter1* a=nullptr;
        for(UINT k=0;i.gpu.empty()&&f->EnumAdapters1(k,&a)!=DXGI_ERROR_NOT_FOUND;k++){DXGI_ADAPTER_DESC1 d={};a->GetDesc1(&d);
            if(!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){i.gpu=d.Description;i.vendor=d.VendorId;i.vramGB=d.DedicatedVideoMemory/1073741824.0;LARGE_INTEGER v={};
                if(SUCCEEDED(a->CheckInterfaceSupport(__uuidof(IDXGIDevice),&v))){wchar_t b[64];swprintf(b,64,L"%u.%u.%u.%u",HIWORD(v.HighPart),LOWORD(v.HighPart),HIWORD(v.LowPart),LOWORD(v.LowPart));i.driver=b;}}
            a->Release();}f->Release();}
    // Windows: the build decides 10 / 11 (ProductName still says 10 on 11).
    {typedef LONG (WINAPI *Rtl)(OSVERSIONINFOW*);HMODULE nt=GetModuleHandleW(L"ntdll.dll");FARPROC p=nt?GetProcAddress(nt,"RtlGetVersion"):nullptr;Rtl rtl=nullptr;memcpy(&rtl,&p,sizeof(rtl));
     OSVERSIONINFOW v={};v.dwOSVersionInfoSize=sizeof(v);if(rtl&&rtl(&v)==0){i.build=(int)v.dwBuildNumber;std::wstring dv=regText(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",L"DisplayVersion");
        wchar_t b[96];swprintf(b,96,L"Windows %d %ls (%d)",i.build>=22000?11:10,dv.c_str(),i.build);i.os=b;}}
    // Power plan (the active scheme's name) and AC / battery.
    {GUID* g=nullptr;if(PowerGetActiveScheme(nullptr,&g)==ERROR_SUCCESS&&g){i.power=*g;wchar_t b[256]={};DWORD n=sizeof(b);if(PowerReadFriendlyName(nullptr,g,nullptr,nullptr,(UCHAR*)b,&n)==ERROR_SUCCESS)i.powerPlan=b;
        static const GUID saver={0xa1841308,0x3541,0x4fab,{0xbc,0x81,0xf7,0x15,0x56,0xf2,0x0b,0x4a}};i.powerSaver=IsEqualGUID(*g,saver)!=0;LocalFree(g);}
     SYSTEM_POWER_STATUS ps={};if(GetSystemPowerStatus(&ps))i.battery=ps.ACLineStatus==0;}
    {DEVMODEW dm={};dm.dmSize=sizeof(dm);if(EnumDisplaySettingsW(nullptr,ENUM_CURRENT_SETTINGS,&dm)){i.width=(int)dm.dmPelsWidth;i.height=(int)dm.dmPelsHeight;i.refresh=(int)dm.dmDisplayFrequency;}}
    // Game Mode: on unless AutoGameModeEnabled = 0. Windowed optimizations: only what the person set (absent = Windows default).
    {DWORD v=0,n=sizeof(v);i.gameMode=RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\GameBar",L"AutoGameModeEnabled",RRF_RT_REG_DWORD,nullptr,&v,&n)==ERROR_SUCCESS?(v?1:0):1;
     std::wstring s=regText(HKEY_CURRENT_USER,L"Software\\Microsoft\\DirectX\\UserGpuPreferences",L"DirectXUserGlobalSettings");i.windowedOpt=s.find(L"SwapEffectUpgradeEnable=1")!=std::wstring::npos?1:s.find(L"SwapEffectUpgradeEnable=0")!=std::wstring::npos?0:-1;}
    {wchar_t sys[MAX_PATH]={};GetSystemDirectoryW(sys,MAX_PATH);i.storage=storageOf(gamePath.size()>=2?gamePath:std::wstring(sys));}
    i.reboot=regExists(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WindowsUpdate\\Auto Update\\RebootRequired")||regExists(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Component Based Servicing\\RebootPending")
        ||regExists(HKEY_LOCAL_MACHINE,L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",L"PendingFileRenameOperations");
    return i;}
}
