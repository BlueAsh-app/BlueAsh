#pragma once
// Stability Guard, the pure part (no Windows calls, unit-tested in stability_tests.cpp): Windows event records in, incidents
// out. An incident keeps every original record and splits what Windows recorded (facts: event IDs, codes, modules) from
// what BlueAsh infers (causes, each with evidence, counter-evidence and checks still needed). The easy screen and the
// technical screen are both built from the same Incident, so they cannot disagree.
// Rules only, no AI or cloud. Confidence is a category (high/medium/low/unknown), never a made-up percentage, and the
// confidence that a kind of failure happened is kept apart from the confidence in its root cause.
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
namespace stability {
typedef unsigned long long Time; // UTC FILETIME (100 ns since 1601)
const Time minute=600000000ULL;
// One record as Windows wrote it. Event ID and provider are always kept together with the channel; a WER problem code
// (LiveKernelEvent 141, ...) lives in data["P1"], never in id.
struct Event {std::wstring channel,provider;int id=0;long long record=0;int level=0;Time time=0;std::wstring message,xml;std::map<std::wstring,std::wstring> data;
    std::wstring get(const wchar_t* k)const{auto i=data.find(k);return i==data.end()?L"":i->second;}};
enum class Kind {None,PowerLoss,BugCheck,GpuTimeout,Hardware,Disk,GameCrash,GameHang,BlueAshCrash};
enum class Level {Unknown,Low,Medium,High};
// Copy-paste-able identifiers that Windows actually recorded (never built from guesses).
struct Code {std::wstring label,value;};
struct Cause {const wchar_t* title;std::vector<std::wstring> evidence,against,checks;};
struct Incident {Kind kind=Kind::None;Time time=0,boot=0;std::vector<Event> events;bool userPower=false,recovered=false;int repeats=0;
    const wchar_t* title=L"";const wchar_t* summary=L"";Level kindSure=Level::Unknown,causeSure=Level::Unknown;std::vector<Cause> causes;std::vector<const wchar_t*> steps;
    std::vector<Code> codes;std::vector<std::wstring> searches;std::wstring stopName,module,process;int severity=0;}; // severity: 0 info, 1 amber, 2 red

// ---- XML (EvtRender output): the few fields we read, without a parser library.
inline std::wstring unescape(std::wstring s){const std::pair<const wchar_t*,const wchar_t*> m[]={{L"&lt;",L"<"},{L"&gt;",L">"},{L"&quot;",L"\""},{L"&apos;",L"'"},{L"&amp;",L"&"}};
    for(auto& p:m){size_t a=0;std::wstring f=p.first;while((a=s.find(f,a))!=std::wstring::npos){s.replace(a,f.size(),p.second);a+=wcslen(p.second);}}return s;}
inline std::wstring between(const std::wstring& x,const std::wstring& open,const std::wstring& close,size_t from=0){size_t a=x.find(open,from);if(a==std::wstring::npos)return L"";a+=open.size();size_t b=x.find(close,a);return b==std::wstring::npos?L"":x.substr(a,b-a);}
inline std::wstring attr(const std::wstring& x,const std::wstring& tag,const std::wstring& name){size_t a=x.find(L"<"+tag);if(a==std::wstring::npos)return L"";size_t e=x.find(L">",a);
    for(wchar_t q:{L'\'',L'"'}){size_t p=x.find(name+L"="+q,a);if(p!=std::wstring::npos&&p<e){p+=name.size()+2;size_t z=x.find(q,p);if(z!=std::wstring::npos)return unescape(x.substr(p,z-p));}}return L"";}
// ISO 8601 UTC ("2026-11-09T06:52:36.0014Z") to FILETIME; 0 when unreadable.
inline Time isoTime(const std::wstring& s){int Y=0,M=0,D=0,h=0,m=0;double sec=0;if(swscanf(s.c_str(),L"%d-%d-%dT%d:%d:%lf",&Y,&M,&D,&h,&m,&sec)<6||Y<1601||M<1||M>12)return 0;
    auto days=[](int y,int mo,int d){y-=mo<=2;long era=(y>=0?y:y-399)/400;unsigned yoe=(unsigned)(y-era*400),doy=(153*(mo+(mo>2?-3:9))+2)/5+d-1,doe=yoe*365+yoe/4-yoe/100+doy;return era*146097L+(long)doe-719468L;}; // days since 1970-01-01
    long long unix=(long long)days(Y,M,D)*86400+h*3600+m*60;return (Time)((unix+11644473600LL)*10000000LL+(long long)(sec*10000000));}
// Fills channel/provider/id/record/level/time and data from one event's XML. Named <Data Name='x'> keep their names;
// unnamed ones are numbered "0","1",... (WER 1001 and EventLog 6008 write them that way); WER's become P1..P10 too.
inline Event parse(const std::wstring& xml,const std::wstring& message=L""){Event e;e.xml=xml;e.message=message;e.provider=attr(xml,L"Provider",L"Name");
    {std::wstring raw=between(xml,L"<EventID",L"</EventID>");size_t g=raw.find(L'>');e.id=_wtoi((g==std::wstring::npos?raw:raw.substr(g+1)).c_str());}
    e.level=_wtoi(between(xml,L"<Level>",L"</Level>").c_str());e.record=_wtoi64(between(xml,L"<EventRecordID>",L"</EventRecordID>").c_str());e.channel=between(xml,L"<Channel>",L"</Channel>");e.time=isoTime(attr(xml,L"TimeCreated",L"SystemTime"));
    size_t at=0;int n=0;while((at=xml.find(L"<Data",at))!=std::wstring::npos){size_t close=xml.find(L'>',at);if(close==std::wstring::npos)break;std::wstring head=xml.substr(at,close-at);
        std::wstring name;size_t p=head.find(L"Name=");if(p!=std::wstring::npos){wchar_t q=head[p+5];size_t z=head.find(q,p+6);if(z!=std::wstring::npos)name=head.substr(p+6,z-p-6);}
        std::wstring value;if(head.empty()||head.back()!=L'/'){size_t end=xml.find(L"</Data>",close);if(end==std::wstring::npos)break;value=unescape(xml.substr(close+1,end-close-1));at=end+7;}else at=close+1;
        if(name.empty()){name=std::to_wstring(n);}e.data[name]=value;n++;}
    // WER 1001 names its fields (EventName, P1..P10, AttachedFiles, ReportId); older records leave them unnamed, by position.
    if(e.provider==L"Windows Error Reporting"&&e.id==1001&&!e.data.count(L"EventName")){e.data[L"EventName"]=e.get(L"2");for(int i=1;i<=10;i++)e.data[L"P"+std::to_wstring(i)]=e.get(std::to_wstring(4+i).c_str());e.data[L"AttachedFiles"]=e.get(L"15");e.data[L"ReportId"]=e.get(L"19");}
    return e;}

// ---- Facts.
// Bug check names: only well-known codes (Microsoft's bug check reference); anything else shows as the bare code.
inline std::wstring stopName(unsigned long c){struct N{unsigned long c;const wchar_t* n;};static const N t[]={{0x0A,L"IRQL_NOT_LESS_OR_EQUAL"},{0x19,L"BAD_POOL_HEADER"},{0x1A,L"MEMORY_MANAGEMENT"},{0x1E,L"KMODE_EXCEPTION_NOT_HANDLED"},
    {0x3B,L"SYSTEM_SERVICE_EXCEPTION"},{0x50,L"PAGE_FAULT_IN_NONPAGED_AREA"},{0x7E,L"SYSTEM_THREAD_EXCEPTION_NOT_HANDLED"},{0x7F,L"UNEXPECTED_KERNEL_MODE_TRAP"},{0x9F,L"DRIVER_POWER_STATE_FAILURE"},{0xC2,L"BAD_POOL_CALLER"},
    {0xD1,L"DRIVER_IRQL_NOT_LESS_OR_EQUAL"},{0xEF,L"CRITICAL_PROCESS_DIED"},{0xFC,L"ATTEMPTED_EXECUTE_OF_NOEXECUTE_MEMORY"},{0x101,L"CLOCK_WATCHDOG_TIMEOUT"},{0x109,L"CRITICAL_STRUCTURE_CORRUPTION"},{0x113,L"VIDEO_DXGKRNL_FATAL_ERROR"},
    {0x116,L"VIDEO_TDR_FAILURE"},{0x117,L"VIDEO_TDR_TIMEOUT_DETECTED"},{0x119,L"VIDEO_SCHEDULER_INTERNAL_ERROR"},{0x124,L"WHEA_UNCORRECTABLE_ERROR"},{0x133,L"DPC_WATCHDOG_VIOLATION"},{0x139,L"KERNEL_SECURITY_CHECK_FAILURE"},
    {0x141,L"VIDEO_ENGINE_TIMEOUT_DETECTED"},{0x154,L"UNEXPECTED_STORE_EXCEPTION"}};for(auto& x:t)if(x.c==c)return x.n;return L"";}
inline std::wstring hex(unsigned long v,int width=8){wchar_t b[24];swprintf(b,24,L"0x%0*lX",width,v);return b;}
// BugCheck 1001 param1: "0x00000116 (0x..., 0x..., 0x..., 0x...)".
inline unsigned long bugCode(const Event& e){return wcstoul(e.get(L"param1").c_str(),nullptr,16);}
inline bool isLive(const Event& e){return e.provider==L"Windows Error Reporting"&&e.id==1001&&e.get(L"EventName")==L"LiveKernelEvent";}
inline unsigned long liveCode(const Event& e){return wcstoul(e.get(L"P1").c_str(),nullptr,16);}
// LiveKernelEvent codes that are the video timeout family (same numbers as bug checks 0x117/0x141/0x116).
inline bool gpuLive(unsigned long c){return c==0x117||c==0x141||c==0x116;}
// When a WER report is queued later than the problem, the dump's file name keeps the moment: "...-20261109-1552.dmp" (local time).
inline std::wstring dumpStamp(const Event& e){const std::wstring f=e.get(L"AttachedFiles");size_t d=f.find(L".dmp");if(d==std::wstring::npos||d<14)return L"";std::wstring s=f.substr(d-13,13);return s[8]==L'-'?s:L"";}
inline std::wstring fileName(const std::wstring& p){size_t s=p.find_last_of(L"\\/");return s==std::wstring::npos?p:p.substr(s+1);}
inline std::wstring lower(std::wstring s){for(auto& c:s)c=(wchar_t)towlower(c);return s;}
// A game: something from a store library or one of the games BlueAsh knows. Other programs' crashes stay out of the list.
inline bool gamePath(const std::wstring& path,const std::wstring& app){std::wstring p=lower(path),a=lower(app);
    return p.find(L"\\steamapps\\")!=std::wstring::npos||p.find(L"\\epic games\\")!=std::wstring::npos||p.find(L"\\xboxgames\\")!=std::wstring::npos||p.find(L"\\gog galaxy\\games\\")!=std::wstring::npos
        ||p.find(L"battlestate games")!=std::wstring::npos||a==L"tslgame.exe"||a==L"escapefromtarkov.exe";}
inline Kind kindOf(const Event& e){const std::wstring& p=e.provider;
    if(p==L"Microsoft-Windows-Kernel-Power"&&e.id==41)return Kind::PowerLoss;
    if(p==L"Microsoft-Windows-WER-SystemErrorReporting"&&e.id==1001)return Kind::BugCheck;
    if((p==L"Display"&&e.id==4101)||p==L"nvlddmkm"||p==L"amdkmdag"||p==L"amdwddmg"||p==L"igfx"||(isLive(e)&&gpuLive(liveCode(e))))return Kind::GpuTimeout;
    if(p==L"Microsoft-Windows-WHEA-Logger")return Kind::Hardware;
    if((p==L"disk"&&(e.id==7||e.id==11||e.id==51||e.id==153))||(p==L"Ntfs"&&e.id==55)||(p==L"stornvme"&&e.level<=2))return Kind::Disk;
    if(p==L"Application Error"&&e.id==1000){if(lower(e.get(L"AppName"))==L"blueash.exe")return Kind::BlueAshCrash;return gamePath(e.get(L"AppPath"),e.get(L"AppName"))?Kind::GameCrash:Kind::None;}
    if(p==L"Application Hang"&&e.id==1002)return gamePath(e.get(L"ExeFileName"),e.get(L"AppName"))?Kind::GameHang:Kind::None;
    return Kind::None;}
// Context records: kept with a restart they belong to (shown in the technical view), never an incident of their own.
inline bool context(const Event& e){return (e.provider==L"EventLog"&&e.id==6008)||(e.provider==L"User32"&&e.id==1074);}
// System kinds (the PC went down or the driver failed) rank above program kinds when one incident holds both.
inline int rank(Kind k){switch(k){case Kind::BugCheck:return 9;case Kind::PowerLoss:return 8;case Kind::GpuTimeout:return 7;case Kind::Hardware:return 6;case Kind::Disk:return 5;case Kind::GameCrash:return 4;case Kind::GameHang:return 3;case Kind::BlueAshCrash:return 2;default:return 0;}}

// ---- Grouping: records of one event chain become one incident (each original record kept). A restart record anchors its
// chain: everything from 20 minutes before it belongs to it. Otherwise records of the same kind within 10 minutes merge.
// occurred: the moment it happened when the record says so (a dump's name), else the record's own time.
inline Time occurred(const Event& e,long long utcOffsetMinutes){std::wstring s=dumpStamp(e);if(s.empty())return e.time;
    int Y=0,M=0,D=0,h=0,m=0;if(swscanf(s.c_str(),L"%4d%2d%2d-%2d%2d",&Y,&M,&D,&h,&m)!=5)return e.time;wchar_t iso[40];swprintf(iso,40,L"%04d-%02d-%02dT%02d:%02d:00Z",Y,M,D,h,m);
    Time local=isoTime(iso);return local?(Time)((long long)local-utcOffsetMinutes*(long long)minute):e.time;}
inline std::vector<Incident> group(std::vector<Event> all,long long utcOffsetMinutes){
    // WER queues the same report again (same ReportId) until it is sent: one record each.
    std::vector<std::pair<Time,Event>> ev;std::vector<std::wstring> reports;for(auto& e:all){if(kindOf(e)==Kind::None&&!context(e))continue;std::wstring r=e.get(L"ReportId");
        if(!r.empty()){if(std::find(reports.begin(),reports.end(),r)!=reports.end())continue;reports.push_back(r);}ev.push_back({occurred(e,utcOffsetMinutes),e});}
    std::sort(ev.begin(),ev.end(),[](auto& a,auto& b){return a.first<b.first;});
    std::vector<Incident> out;std::vector<bool> used(ev.size(),false);
    for(size_t i=0;i<ev.size();i++){Kind k=kindOf(ev[i].second);if(k!=Kind::PowerLoss&&k!=Kind::BugCheck)continue;if(used[i])continue;
        Incident c;c.boot=ev[i].second.time;c.time=ev[i].first;
        for(size_t j=0;j<ev.size();j++){if(used[j])continue;bool restart=kindOf(ev[j].second)==Kind::PowerLoss||kindOf(ev[j].second)==Kind::BugCheck;
            if(ev[j].first+20*minute>=c.boot&&ev[j].first<=c.boot+3*minute&&(!restart||j==i||ev[j].first+3*minute>=c.boot)){c.events.push_back(ev[j].second);used[j]=true;if(!restart)c.time=std::min(c.time,ev[j].first);}}
        out.push_back(c);}
    for(size_t i=0;i<ev.size();i++){if(used[i]||context(ev[i].second))continue;Kind k=kindOf(ev[i].second);Incident* into=nullptr;
        for(auto& c:out){if(c.boot)continue;Kind ck=kindOf(c.events.front());if(ck==k&&ev[i].first>=c.time&&ev[i].first<=c.time+10*minute+(Time)c.events.size()*minute)into=&c;}
        if(!into){out.push_back({});into=&out.back();into->time=ev[i].first;}into->events.push_back(ev[i].second);used[i]=true;}
    std::sort(out.begin(),out.end(),[](const Incident& a,const Incident& b){return a.time>b.time;});return out;}

// ---- Analysis. The texts are Korean source keys of the app's catalog (i18n_native.json), shown through i18n::text.
inline const Event* first(const Incident& c,Kind k){for(auto& e:c.events)if(kindOf(e)==k)return &e;return nullptr;}
inline void addCode(Incident& c,const std::wstring& label,const std::wstring& value){if(value.empty())return;for(auto& x:c.codes)if(x.label==label&&x.value==value)return;c.codes.push_back({label,value});}
inline void addSearch(Incident& c,const std::wstring& q){if(q.empty())return;for(auto& s:c.searches)if(s==q)return;c.searches.push_back(q);}
// powerLossCount: Kernel-Power 41 records in the whole window (repeats raise the weight of a hardware cause, never prove it).
inline void analyze(Incident& c,int powerLossCount){
    Kind top=Kind::None;for(auto& e:c.events)if(rank(kindOf(e))>rank(top))top=kindOf(e);c.kind=top;c.repeats=powerLossCount;
    const Event* gpu=first(c,Kind::GpuTimeout);const Event* hw=first(c,Kind::Hardware);
    // Identifiers that are really in the records: event IDs with their providers, codes, modules.
    for(auto& e:c.events){Kind k=kindOf(e);
        if(isLive(e)){addCode(c,L"WER LiveKernelEvent",e.get(L"P1"));addSearch(c,L"LiveKernelEvent "+e.get(L"P1"));}
        else addCode(c,e.provider+L" Event ID",std::to_wstring(e.id));
        if(k==Kind::PowerLoss)addSearch(c,L"Kernel-Power Event ID 41");
        if(k==Kind::BugCheck){unsigned long b=bugCode(e);std::wstring n=stopName(b);c.stopName=n;addCode(c,L"BugCheck Code",hex(b));if(!n.empty())addCode(c,L"Stop Code",n);addSearch(c,(n.empty()?L"":n+L" ")+L"0x"+hex(b,0).substr(2));}
        if(e.provider==L"Display"&&e.id==4101)addSearch(c,L"Display Event ID 4101");
        if(e.provider==L"nvlddmkm"){addSearch(c,L"nvlddmkm Event ID "+std::to_wstring(e.id));}
        if(k==Kind::Hardware)addSearch(c,L"WHEA-Logger Event ID "+std::to_wstring(e.id));
        if(k==Kind::Disk)addSearch(c,e.provider+L" Event ID "+std::to_wstring(e.id));
        if(k==Kind::GameCrash||k==Kind::BlueAshCrash){std::wstring m=e.get(L"ModuleName"),x=e.get(L"ExceptionCode");c.module=m;c.process=e.get(L"AppName");addCode(c,L"Exception Code",x.empty()?L"":L"0x"+x);addCode(c,L"Faulting Module",m);
            if(!m.empty())addSearch(c,L"Faulting Module "+m+(x.empty()?L"":L" 0x"+x));}
        if(k==Kind::GameHang){c.process=e.get(L"AppName");addSearch(c,c.process+L" Application Hang 1002");}}
    if(c.kind==Kind::PowerLoss){const Event* p=first(c,Kind::PowerLoss);c.userPower=p&&(p->get(L"LongPowerButtonPressDetected")==L"true"||(_wtoi64(p->get(L"PowerButtonTimestamp").c_str())!=0));}
    if(c.kind==Kind::GpuTimeout)c.recovered=true; // the PC kept running (no restart record in this incident)
    c.kindSure=Level::High; // every kind comes from a Windows record; what is unsure is the cause
    switch(c.kind){
    case Kind::PowerLoss:c.severity=2;
        if(c.userPower){c.title=L"전원 버튼으로 컴퓨터를 끈 기록이 있습니다";c.summary=L"화면이 멈췄거나 응답이 없어 전원 버튼을 눌러 끈 것으로 보입니다. 멈춘 이유는 남은 기록으로 확인해야 합니다.";c.causeSure=Level::Low;
            c.causes.push_back({L"컴퓨터가 멈춰서 직접 끔",{L"Windows가 전원 버튼 입력을 기록했습니다"},{L"멈춘 이유 자체는 기록되지 않았습니다"},{L"멈추기 전 화면 상태(검은 화면·소리)를 떠올려 보세요"}});}
        else if(gpu){c.title=L"그래픽카드가 응답하지 못한 뒤 컴퓨터가 꺼졌습니다";c.summary=L"꺼지기 직전에 그래픽카드 응답 실패 기록이 있고, 블루스크린 없이 바로 꺼졌습니다. 그래픽카드 쪽 전원 공급, 드라이버나 그래픽 설정, 과열 중 하나가 의심됩니다.";c.causeSure=Level::Low;
            c.causes.push_back({L"그래픽카드 전원 공급 부족 (파워 용량·케이블)",{L"꺼지기 직전 그래픽카드 응답 실패 기록",L"블루스크린 없이 전원이 끊긴 형태"},{L"파워 상태는 Windows 기록으로 확인할 수 없습니다"},{L"그래픽카드 전력 제한을 낮춰 같은 게임을 해 보기",L"그래픽카드 전원 케이블이 끝까지 꽂혔는지 확인"}});
            c.causes.push_back({L"그래픽 드라이버 또는 그래픽 설정",{L"그래픽카드 응답 실패 기록"},{L"드라이버 자체 결함은 기록만으로 확인할 수 없습니다"},{L"최근 바꾼 그래픽 설정(DLSS·모드·오버클럭)을 되돌려 보기"}});
            c.causes.push_back({L"그래픽카드 과열",{},{L"오류 직전 온도·전력 기록이 없으면 확인할 수 없습니다"},{L"게임 중 그래픽카드 온도 확인"}});}
        else{c.title=L"컴퓨터가 정상적으로 종료되지 않았습니다";c.summary=L"전원이 갑자기 끊겼거나 시스템이 멈춘 뒤 다시 시작된 것으로 보입니다. 현재 기록만으로는 원인을 알기 어렵습니다.";c.causeSure=Level::Unknown;
            c.causes.push_back({L"전원 공급 문제 또는 시스템 멈춤",{L"Windows가 비정상 종료를 기록했습니다"},{L"종료 직전 다른 오류 기록이 없습니다",L"정전·멀티탭 문제일 수도 있습니다"},{L"같은 문제가 어떤 게임·상황에서 반복되는지 적어 두세요"}});}
        if(powerLossCount>=3)for(auto& k:c.causes)k.evidence.push_back(L"최근 90일 동안 같은 종료가 여러 번 반복됐습니다");
        c.steps={L"최근 바꾼 설정을 되돌려 보세요",L"전원 케이블과 그래픽카드 연결 상태를 확인하세요",L"같은 문제가 어떤 게임·상황에서 반복되는지 적어 두세요"};break;
    case Kind::BugCheck:{c.severity=2;unsigned long b=bugCode(*first(c,Kind::BugCheck));c.title=L"블루스크린(시스템 오류)으로 다시 시작됐습니다";c.summary=L"Windows가 심각한 오류를 감지해 멈췄습니다. 오류 코드는 기술 정보에서 확인할 수 있습니다.";
        if(b==0x116||b==0x117||b==0x141||b==0x113||b==0x119){c.causeSure=Level::Medium;c.causes.push_back({L"그래픽 드라이버 또는 그래픽카드",{L"그래픽 관련 오류 코드"},{L"덤프 분석을 하지 않아 원인 드라이버는 확인되지 않았습니다"},{L"그래픽 드라이버를 깨끗하게 다시 설치",L"최근 바꾼 그래픽 설정을 되돌려 보세요"}});}
        else if(b==0x124){c.causeSure=Level::Medium;c.causes.push_back({L"하드웨어 오류 (CPU·메모리·메인보드·전원)",{L"하드웨어 오류 코드"},{L"어느 부품인지는 이 코드만으로 알 수 없습니다"},{L"오버클럭·XMP를 끄고 확인",L"Windows 메모리 진단 실행"}});}
        else if(b==0x1A||b==0x50){c.causeSure=Level::Low;c.causes.push_back({L"메모리 또는 드라이버",{L"메모리 관련 오류 코드"},{L"드라이버 문제로도 같은 코드가 납니다"},{L"Windows 메모리 진단 실행",L"오버클럭·XMP를 끄고 확인"}});}
        else if(b==0x0A||b==0xD1||b==0x3B||b==0x7E||b==0x1E||b==0x9F||b==0xD4){c.causeSure=Level::Low;c.causes.push_back({L"드라이버 오류",{L"드라이버 관련 오류 코드"},{L"덤프 분석을 하지 않아 원인 드라이버는 확인되지 않았습니다"},{L"최근 설치·업데이트한 드라이버 확인"}});}
        else{c.causeSure=Level::Unknown;c.causes.push_back({L"원인 미확인",{},{L"이 오류 코드는 BlueAsh 규칙에 없습니다"},{L"오류 코드를 검색해 보세요"}});}
        if(hw)c.causes.front().evidence.push_back(L"같은 시각에 하드웨어 오류 기록");
        c.steps={L"오류 코드를 검색해 보세요",L"최근 바꾼 드라이버·설정을 되돌려 보세요",L"같은 문제가 반복되는지 확인하세요"};break;}
    case Kind::GpuTimeout:c.severity=1;c.title=L"그래픽카드가 잠시 응답하지 못했습니다";c.summary=L"게임 중 그래픽 드라이버가 멈췄다가 복구된 기록입니다. 최근 그래픽 설정이나 드라이버 충돌, 과부하가 의심됩니다.";c.causeSure=Level::Low;
        c.causes.push_back({L"그래픽 드라이버 또는 그래픽 설정",{L"그래픽카드 응답 실패 기록"},{L"특정 설정과의 인과관계는 확인되지 않았습니다"},{L"최근 바꾼 그래픽 설정(DLSS·모드·오버클럭)을 되돌려 보기"}});
        c.causes.push_back({L"그래픽카드 과부하·전원·과열",{},{L"오류 직전 온도·전력 기록이 없으면 확인할 수 없습니다"},{L"프레임 제한을 걸고 같은 게임을 해 보기"}});
        c.steps={L"최근 바꾼 그래픽 설정을 되돌려 보세요",L"그래픽 드라이버 버전을 확인하세요",L"같은 문제가 반복되는지 확인하세요"};break;
    case Kind::Hardware:{bool fatal=hw&&hw->level<=2;c.severity=fatal?2:1;c.title=fatal?L"하드웨어 오류 기록이 있습니다":L"하드웨어가 스스로 바로잡은 오류 기록이 있습니다";
        c.summary=L"Windows가 CPU·메모리·PCIe 같은 장치에서 오류를 받았습니다. 기록 하나만으로 부품 고장을 확정할 수는 없습니다.";c.causeSure=Level::Low;
        c.causes.push_back({L"하드웨어 불안정 (오버클럭·XMP·전원·부품)",{L"WHEA 하드웨어 오류 기록"},{L"한 번의 기록은 고장의 증거가 아닙니다"},{L"오버클럭·XMP를 끄고 확인",L"BIOS·칩셋 드라이버 업데이트 확인"}});
        c.steps={L"오버클럭·XMP를 기본값으로 두고 지켜보세요",L"같은 문제가 반복되는지 확인하세요"};break;}
    case Kind::Disk:c.severity=1;c.title=L"저장 장치 오류 기록이 있습니다";c.summary=L"디스크나 SSD에서 읽기·쓰기 오류가 기록됐습니다. 케이블이나 장치 상태를 확인해 보세요.";c.causeSure=Level::Low;
        c.causes.push_back({L"저장 장치 또는 연결 문제",{L"디스크 오류 기록"},{L"일시적인 오류일 수도 있습니다"},{L"제조사 도구로 저장 장치 상태를 확인하세요",L"중요한 파일을 먼저 백업하세요"}});
        c.steps={L"중요한 파일을 먼저 백업하세요",L"제조사 도구로 저장 장치 상태를 확인하세요"};break;
    case Kind::GameCrash:case Kind::BlueAshCrash:{c.severity=1;bool ours=c.kind==Kind::BlueAshCrash;c.title=ours?L"BlueAsh가 오류로 종료됐습니다":L"게임 프로그램이 오류로 종료됐습니다";
        c.summary=ours?L"BlueAsh 프로그램에서 오류가 발견됐습니다. 문제·건의 보내기로 알려 주시면 고치겠습니다.":L"Windows 전체가 꺼진 기록은 없지만 게임 프로그램에서 오류가 발견됐습니다. 게임 파일이나 관련 드라이버·모드 충돌이 의심됩니다.";c.causeSure=Level::Low;
        std::wstring mp=lower(first(c,c.kind)->get(L"ModulePath")),ap=lower(first(c,c.kind)->get(L"AppPath"));size_t cut=ap.find_last_of(L'\\');bool inGame=cut!=std::wstring::npos&&mp.compare(0,cut,ap,0,cut)==0;
        if(!c.module.empty())c.causes.push_back({L"오류가 난 모듈과 관련된 프로그램·모드",{L"오류가 발생한 모듈 이름이 기록돼 있습니다"},{L"오류가 난 모듈이 원인이라고 단정할 수는 없습니다"},{inGame?L"게임 파일 무결성 검사를 해 보세요":L"그 모듈을 넣은 모드·프로그램을 빼고 실행해 보기"}});
        else c.causes.push_back({L"원인 미확인",{},{L"오류가 난 모듈이 기록되지 않았습니다"},{L"게임 파일 무결성 검사를 해 보세요"}});
        c.steps=ours?std::vector<const wchar_t*>{L"BlueAsh를 최신 버전으로 업데이트하세요",L"문제·건의 보내기로 알려 주세요"}:std::vector<const wchar_t*>{L"게임 파일 무결성 검사를 해 보세요",L"최근 넣은 모드를 빼고 실행해 보세요",L"그래픽 드라이버 버전을 확인하세요"};break;}
    case Kind::GameHang:c.severity=1;c.title=L"게임이 응답하지 않아 종료됐습니다";c.summary=L"게임이 멈춰서 Windows가 종료한 기록입니다.";c.causeSure=Level::Unknown;
        c.causes.push_back({L"원인 미확인",{},{L"응답 없음 기록에는 원인이 남지 않습니다"},{L"게임 파일 무결성 검사를 해 보세요"}});c.steps={L"게임 파일 무결성 검사를 해 보세요",L"같은 문제가 반복되는지 확인하세요"};break;
    default:break;}
    if(gpu&&c.kind!=Kind::GpuTimeout)for(auto& e:c.events)if(isLive(e)&&gpuLive(liveCode(e)))addSearch(c,L"LiveKernelEvent "+e.get(L"P1")+L" "+stopName(liveCode(e)));}
inline int countPowerLoss(const std::vector<Event>& all){int n=0;for(auto& e:all)if(kindOf(e)==Kind::PowerLoss)n++;return n;}
inline std::vector<Incident> build(const std::vector<Event>& all,long long utcOffsetMinutes){auto out=group(all,utcOffsetMinutes);int n=countPowerLoss(all);for(auto& c:out)analyze(c,n);return out;}
// ---- Privacy: what leaves the PC (copied text, exports) never carries the profile path or the account name.
inline std::wstring scrub(std::wstring s,const std::wstring& profile,const std::wstring& user,const std::wstring& computer){
    auto rep=[&](const std::wstring& from,const std::wstring& to){if(from.size()<2)return;std::wstring ls=lower(s),lf=lower(from);size_t a=0;while((a=ls.find(lf,a))!=std::wstring::npos){s.replace(a,from.size(),to);ls.replace(a,from.size(),lower(to));a+=to.size();}};
    rep(profile,L"%USERPROFILE%");rep(user,L"<user>");rep(computer,L"<computer>");return s;}
// Search text: only identifiers (codes, providers, module names); a path is cut to its file name.
inline std::wstring searchText(const Incident& c){return c.searches.empty()?L"":c.searches.front();}
}
