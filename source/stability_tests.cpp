// Stability Guard core: mock Windows records (shaped like the real ones on a Windows 11 PC) in, incidents out.
#include "stability_core.h"
#include <cassert>
#include <cstdio>
using namespace stability;
static std::wstring sys(const wchar_t* provider,int id,const wchar_t* time,int level,long long rec,const std::wstring& data,const wchar_t* channel=L"System"){
    wchar_t head[600];swprintf(head,600,L"<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='%ls'/><EventID>%d</EventID><Level>%d</Level><TimeCreated SystemTime='%ls'/><EventRecordID>%lld</EventRecordID><Channel>%ls</Channel></System>",provider,id,level,time,rec,channel);
    return std::wstring(head)+L"<EventData>"+data+L"</EventData></Event>";}
static Event kp41(const wchar_t* t,bool button=false){return parse(sys(L"Microsoft-Windows-Kernel-Power",41,t,1,100,std::wstring(L"<Data Name='BugcheckCode'>0</Data><Data Name='PowerButtonTimestamp'>")+(button?L"133000000000000000":L"0")+L"</Data><Data Name='LongPowerButtonPressDetected'>false</Data>"));}
static Event live(const wchar_t* t,const wchar_t* code,const wchar_t* dump){std::wstring d=L"<Data></Data><Data>0</Data><Data>LiveKernelEvent</Data><Data>n/a</Data><Data>0</Data><Data>";d+=code;
    d+=L"</Data><Data>a</Data><Data>a</Data><Data>a</Data><Data>a</Data><Data>10_0_26200</Data><Data>0_0</Data><Data>256_1</Data><Data></Data><Data></Data><Data>\\\\?\\C:\\WINDOWS\\LiveKernelReports\\WATCHDOG\\";d+=dump;
    d+=L"</Data><Data>q</Data><Data></Data><Data>0</Data><Data>fcbc586b-390e</Data><Data>2049</Data><Data></Data><Data>0</Data>";return parse(sys(L"Windows Error Reporting",1001,t,4,200,d,L"Application"));}
static Event crash(const wchar_t* t,const wchar_t* app,const wchar_t* path,const wchar_t* module,const wchar_t* modpath){std::wstring d=std::wstring(L"<Data Name='AppName'>")+app+L"</Data><Data Name='ModuleName'>"+module+L"</Data><Data Name='ExceptionCode'>c0000409</Data><Data Name='AppPath'>"+path+L"</Data><Data Name='ModulePath'>"+modpath+L"</Data>";
    return parse(sys(L"Application Error",1000,t,2,300,d,L"Application"));}
static bool has(const std::vector<Code>& c,const std::wstring& label,const std::wstring& value){for(auto& x:c)if(x.label==label&&x.value==value)return true;return false;}
int main(){
    // XML reading: named data, unnamed data, record/level/channel, UTC time.
    Event e=kp41(L"2026-11-09T06:53:22.5Z");assert(e.provider==L"Microsoft-Windows-Kernel-Power"&&e.id==41&&e.level==1&&e.record==100&&e.channel==L"System"&&e.get(L"BugcheckCode")==L"0");
    assert(isoTime(L"1970-01-01T00:00:00Z")==116444736000000000ULL);
    assert(unescape(L"a&lt;b&amp;c")==L"a<b&c");
    // 1. Kernel-Power 41 only: power loss, the kind is certain, the cause is not.
    {auto v=build({kp41(L"2026-11-09T06:53:22Z")},540);assert(v.size()==1&&v[0].kind==Kind::PowerLoss&&v[0].kindSure==Level::High&&v[0].causeSure==Level::Unknown&&v[0].stopName.empty());
     assert(has(v[0].codes,L"Microsoft-Windows-Kernel-Power Event ID",L"41")&&v[0].searches.front()==L"Kernel-Power Event ID 41");}
    // EventLog 6008 at the same boot is kept as context of the power loss; alone it is no incident.
    {auto v=build({kp41(L"2026-11-09T06:53:22Z"),parse(sys(L"EventLog",6008,L"2026-11-09T06:53:28Z",2,3,L"<Data>3:27:19</Data>"))},540);assert(v.size()==1&&v[0].events.size()==2&&v[0].kind==Kind::PowerLoss);
     assert(build({parse(sys(L"EventLog",6008,L"2026-11-09T06:53:28Z",2,3,L""))},540).empty());}
    // 2. A normal restart (User32 1074) and clean shutdown (6006) are no incident.
    {auto v=build({parse(sys(L"User32",1074,L"2026-11-09T05:00:00Z",4,1,L"<Data Name='param5'>restart</Data>")),parse(sys(L"EventLog",6006,L"2026-11-09T05:00:05Z",4,2,L""))},540);assert(v.empty());}
    // 3. BugCheck 0x116: the stop code comes from the record; dump analysis is not claimed.
    {auto v=build({parse(sys(L"Microsoft-Windows-WER-SystemErrorReporting",1001,L"2025-12-06T04:57:07Z",2,5,L"<Data Name='param1'>0x00000116 (0xffff, 0x0, 0x0, 0x0)</Data><Data Name='param2'>C:\\WINDOWS\\Minidump\\a.dmp</Data><Data Name='param3'>be9b</Data>"))},540);
     assert(v.size()==1&&v[0].kind==Kind::BugCheck&&v[0].stopName==L"VIDEO_TDR_FAILURE"&&has(v[0].codes,L"BugCheck Code",L"0x00000116")&&v[0].searches.front()==L"VIDEO_TDR_FAILURE 0x116");}
    // An unknown bug check code shows bare, without an invented name.
    {auto v=build({parse(sys(L"Microsoft-Windows-WER-SystemErrorReporting",1001,L"2025-12-06T04:57:07Z",2,5,L"<Data Name='param1'>0x000001b8 (0x1, 0x0, 0x0, 0x0)</Data>"))},540);assert(v[0].stopName.empty()&&v[0].causeSure==Level::Unknown&&v[0].searches.front()==L"0x1B8");}
    // 4 + 12. LiveKernelEvent 141 queued at 08:11 for a dump from 15:52 local (UTC+9 = 06:52Z) joins the restart at 06:53Z.
    {auto v=build({kp41(L"2026-11-09T06:53:22Z"),live(L"2026-11-09T08:11:01Z",L"141",L"WATCHDOG-20261109-1552.dmp")},540);
     assert(v.size()==1&&v[0].events.size()==2&&v[0].kind==Kind::PowerLoss&&v[0].causes.size()==3);
     assert(std::wstring(v[0].title)==L"그래픽카드가 응답하지 못한 뒤 컴퓨터가 꺼졌습니다");
     assert(has(v[0].codes,L"WER LiveKernelEvent",L"141")&&!has(v[0].codes,L"Windows Error Reporting Event ID",L"141")); // a WER code is not an event ID
     assert(v[0].causeSure==Level::Low);} // GPU timeout before a power cut does not prove the PSU
    // Current Windows names the WER fields (EventName, P1, AttachedFiles): the same incident.
    {std::wstring d=L"<Data Name='EventName'>LiveKernelEvent</Data><Data Name='P1'>141</Data><Data Name='AttachedFiles'>C:/WINDOWS/LiveKernelReports/WATCHDOG/WATCHDOG-20261109-1552.dmp</Data><Data Name='ReportId'>a8</Data>";
     auto v=build({kp41(L"2026-11-09T06:53:22Z"),parse(sys(L"Windows Error Reporting",1001,L"2026-11-09T08:11:01Z",4,201,d,L"Application"))},540);assert(v.size()==1&&v[0].events.size()==2&&v[0].causes.size()==3);}
    // The same WER report queued again (same ReportId) counts once.
    {auto a=live(L"2026-11-09T06:53:01Z",L"141",L"WATCHDOG-20261109-1552.dmp"),b=live(L"2026-11-09T08:11:01Z",L"141",L"WATCHDOG-20261109-1552.dmp");auto v=build({kp41(L"2026-11-09T06:53:22Z"),a,b},540);assert(v.size()==1&&v[0].events.size()==2);}
    // A LiveKernelEvent code outside the video family is no GPU incident.
    {auto v=build({live(L"2026-11-09T08:11:01Z",L"1b8",L"WATCHDOG4400-20261109-1552.dmp")},540);assert(v.empty());}
    // 5. Display 4101 alone: recovered GPU timeout.
    {auto v=build({parse(sys(L"Display",4101,L"2026-11-08T13:00:00Z",3,7,L"<Data>nvlddmkm</Data>"))},540);assert(v.size()==1&&v[0].kind==Kind::GpuTimeout&&v[0].recovered&&v[0].searches.front()==L"Display Event ID 4101");}
    // 6. WHEA corrected (warning) vs fatal (error).
    {auto w=build({parse(sys(L"Microsoft-Windows-WHEA-Logger",19,L"2026-09-21T11:20:15Z",3,8,L"<Data Name='ErrorSource'>3</Data>"))},540);assert(w[0].kind==Kind::Hardware&&w[0].severity==1);
     auto f=build({parse(sys(L"Microsoft-Windows-WHEA-Logger",18,L"2026-09-21T11:20:15Z",2,9,L"<Data Name='ErrorSource'>3</Data>"))},540);assert(f[0].severity==2);}
    // 7 + 8. A game crash with its module: the module is reported, not blamed.
    {auto v=build({crash(L"2026-11-06T05:19:45Z",L"DaysGone.exe",L"D:\\SteamLibrary\\steamapps\\common\\Days Gone\\DaysGone.exe",L"crashhandler64.dll",L"C:\\stean\\crashhandler64.dll")},540);
     assert(v.size()==1&&v[0].kind==Kind::GameCrash&&v[0].module==L"crashhandler64.dll"&&has(v[0].codes,L"Exception Code",L"0xc0000409")&&v[0].searches.front()==L"Faulting Module crashhandler64.dll 0xc0000409");
     assert(v[0].causeSure==Level::Low);}
    // 9. No module recorded: "원인 미확인", no invented DLL.
    {auto v=build({crash(L"2026-11-06T05:19:45Z",L"DaysGone.exe",L"D:\\SteamLibrary\\steamapps\\common\\Days Gone\\DaysGone.exe",L"",L"")},540);assert(v[0].module.empty()&&std::wstring(v[0].causes[0].title)==L"원인 미확인");
     for(auto& c:v[0].codes)assert(c.label!=L"Faulting Module");}
    // A non-game program's crash stays out; BlueAsh's own crash is its own kind.
    {assert(build({crash(L"2026-11-06T05:19:45Z",L"notepad.exe",L"C:\\Windows\\notepad.exe",L"x.dll",L"C:\\x.dll")},540).empty());
     auto b=build({crash(L"2026-11-06T05:19:45Z",L"BlueAsh.exe",L"C:\\Users\\a\\BlueAsh.exe",L"BlueAsh.exe",L"C:\\Users\\a\\BlueAsh.exe")},540);assert(b[0].kind==Kind::BlueAshCrash);}
    // Power button held: the user turned it off.
    {auto v=build({kp41(L"2026-11-09T06:53:22Z",true)},540);assert(v[0].userPower);}
    // Two separate restarts stay two incidents; repeats are counted.
    {auto v=build({kp41(L"2026-11-09T06:53:22Z"),kp41(L"2026-11-06T07:07:02Z"),kp41(L"2026-10-26T17:52:34Z")},540);assert(v.size()==3&&v[0].repeats==3&&v[0].time>v[1].time);
     bool said=false;for(auto& ev:v[0].causes[0].evidence)said|=ev==L"최근 90일 동안 같은 종료가 여러 번 반복됐습니다";assert(said);}
    // 14. Missing data: an empty record does not crash and yields no codes beyond its event ID.
    {Event bare=parse(L"<Event><System><Provider Name='Microsoft-Windows-Kernel-Power'/><EventID>41</EventID></System></Event>");assert(bare.id==41&&bare.time==0);auto v=build({bare},540);assert(v.size()==1&&v[0].codes.size()==1);}
    // 17. Privacy: profile path, account and PC name are masked case-insensitively.
    assert(scrub(L"C:\\Users\\Kim\\a.dmp by DESKTOP-1\\Kim",L"C:\\Users\\Kim",L"Kim",L"DESKTOP-1")==L"%USERPROFILE%\\a.dmp by <computer>\\<user>");
    // Event ID with provider: the same number from another provider is not mistaken (Display 4101 vs WER 4101 never mix).
    assert(kindOf(parse(sys(L"SomethingElse",41,L"2026-11-09T06:53:22Z",1,1,L"")))==Kind::None);
    std::puts("stability tests passed");
}
