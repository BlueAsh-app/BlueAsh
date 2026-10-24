#pragma once
#include "i18n.h"
#include <shlobj.h>
#include <mmsystem.h>
#include <map>
#include "raid_events.h"
#include "raid_files.h"
#include "raid_discovery.h"
#include "raid_deploy_capture.h"
// Included after ModeSwitch's UI and settings helpers.
// Tray alert for a minimized game: it leaves as soon as that game is brought back to the front instead of
// sitting over it for Windows' own 5+ seconds. Checked every 0.2 s, only while the alert is up (max 30 s).
// (A foreground event hook was tried: it fires before the switch completes, while the old window is still in front.)
namespace balloon {
inline std::wstring game;inline UINT_PTR timer=0;inline ULONGLONG until=0;
inline bool front(const std::wstring& exe){
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);if(!pid)return false;
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;
    wchar_t path[1024];DWORD n=1024;BOOL ok=QueryFullProcessImageNameW(p,0,path,&n);CloseHandle(p);if(!ok)return false;
    const wchar_t* name=wcsrchr(path,L'\\');return _wcsicmp(name?name+1:path,exe.c_str())==0;
}
inline void hide(){
    if(timer){KillTimer(nullptr,timer);timer=0;}
    NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;Shell_NotifyIconW(NIM_MODIFY,&n);
}
inline void CALLBACK check(HWND,UINT,UINT_PTR,DWORD){if(GetTickCount64()>=until||front(game))hide();}
inline void show(const wchar_t* title,const wchar_t* text,const wchar_t* exe){
    hide();game=exe;balloonUpdateAt=0;
    NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;
    wcsncpy(n.szInfoTitle,i18n::text(title),63);wcsncpy(n.szInfo,i18n::text(text),255);Shell_NotifyIconW(NIM_MODIFY,&n);
    // Game already in front: a short glance is enough (4 s). Otherwise it stays until the game comes back.
    bool now=front(game);until=GetTickCount64()+(now?4000:30000);timer=SetTimer(nullptr,0,now?4000:200,check);
}
}
struct RaidWatch {
    std::wstring folder,file,status=i18n::text(L"자동 탐색 대기 · 타르코프를 실행해 주세요.");
    bool enabled=false,baseline=true;
    ULONGLONG offset=0,startedAt=0;
    raid::Context context;
    raiddeploy::Gate deployment;RaidDeployCapture capture;unsigned captureToken=0;ULONGLONG captureRetry=0;
    struct Stream {std::string contextPartial;ULONGLONG offset=0,size=0;raid::Parser parser;};
    std::map<std::wstring,Stream> streams;
    std::wstring lastRead=i18n::text(L"아직 없음"),lastEvent=i18n::text(L"아직 없음");
    ULONGLONG bytesRead=0,lastAlert=0;
    unsigned readFailures=0;
    HWND dialog=nullptr;
    unsigned loudness=70;
    std::vector<unsigned char> wav;
    ULONGLONG soundUntil=0,nextDiscovery=0,nextSessionScan=0;std::wstring session;
    void discoverMissing(){
        if(raidpaths::directory(folder))return;
        auto found=raidpaths::discover();
        if(found.empty()){status=i18n::text(L"설치를 찾지 못했습니다. 게임 실행 후 자동 재탐색합니다.");showStatus();return;}
        folder=found;reset();
        status=i18n::text(L"로그 폴더를 자동으로 찾았습니다.");
        if(!saveString(configPath,L"RaidLogs",folder))status+=i18n::text(L" (경로 저장 실패)");
        showStatus();
    }
    static ULONGLONG number(FILETIME t){ULARGE_INTEGER n;n.LowPart=t.dwLowDateTime;n.HighPart=t.dwHighDateTime;return n.QuadPart;}
    // PMC profile ids survive resets and restarts: the scav/PMC decision needs them before the first raid.
    void loadProfiles(){context.pmc.clear();auto s=regString(configPath,L"RaidPmcProfiles");size_t at=0;
        while(at<s.size()){size_t end=s.find(L',',at);if(end==std::wstring::npos)end=s.size();auto p=s.substr(at,end-at);if(p.size()==24)context.pmc.push_back(std::string(p.begin(),p.end()));at=end+1;}}
    void saveProfiles(){if(!context.learned)return;context.learned=false;std::wstring s;for(auto& p:context.pmc){if(!s.empty())s+=L',';s+=std::wstring(p.begin(),p.end());}saveString(configPath,L"RaidPmcProfiles",s);}
    void reset(){capture.stop();unsigned token=deployment.token+1;deployment={};deployment.token=token;captureRetry=0;auto pmc=context.pmc;context=raid::Context{};context.pmc=pmc;publishRaidContext(context);file.clear();offset=0;baseline=true;streams.clear();bytesRead=0;readFailures=0;lastAlert=0;lastRead=i18n::text(L"아직 없음");lastEvent=i18n::text(L"아직 없음");FILETIME t;GetSystemTimeAsFileTime(&t);startedAt=number(t);}
    static std::wstring clock(){SYSTEMTIME t;GetLocalTime(&t);wchar_t b[32];swprintf(b,32,L"%02u:%02u:%02u",t.wHour,t.wMinute,t.wSecond);return b;}
    std::wstring diagnostics(){
        std::wstring d=i18n::text(L"BlueAsh 2.24.1\r\n알림: ")+std::wstring(enabled?L"ON":L"OFF")+i18n::text(L" / 볼륨: ")+std::to_wstring(loudness)+L"\r\nLogs: "+folder+
            i18n::text(L"\r\n상태: ")+status+i18n::text(L"\r\n최근 읽기: ")+lastRead+i18n::text(L" / 신규 읽은 바이트: ")+std::to_wstring(bytesRead)+i18n::text(L" / 읽기 오류: ")+std::to_wstring(readFailures)+
            i18n::text(L"\r\n최근 알림: ")+lastEvent+i18n::text(L"\r\n감시 파일 수: ")+std::to_wstring(streams.size());
        for(const auto& entry:streams)d+=L"\r\n"+entry.first+L" ["+std::to_wstring(entry.second.offset)+L" / "+std::to_wstring(entry.second.size)+L"]";
        auto r=capture.reading();d+=L"\r\nDeploy: "+std::to_wstring(deployment.token)+(deployment.armed?L" armed":L" idle")+L" / GET READY: "+(r.ready?L"1":L"0")+L" / ms: "+std::to_wstring(r.ms)+L" / OCR: "+r.text;
        return d;
    }
    void showStatus(){if(dialog){setText(GetDlgItem(dialog,76),status);setText(GetDlgItem(dialog,78),diagnostics());}}
    bool copyDiagnostics(){
        auto text=diagnostics();SIZE_T bytes=(text.size()+1)*sizeof(wchar_t);HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!data)return false;
        void* ptr=GlobalLock(data);if(!ptr){GlobalFree(data);return false;}memcpy(ptr,text.c_str(),bytes);GlobalUnlock(data);
        if(!OpenClipboard(dialog)){GlobalFree(data);return false;}
        bool ok=EmptyClipboard()!=FALSE;if(ok)ok=SetClipboardData(CF_UNICODETEXT,data)!=nullptr;CloseClipboard();if(!ok)GlobalFree(data);return ok;
    }
    bool sound(){
        if(GetTickCount64()<soundUntil)return true;
        PlaySoundW(nullptr,nullptr,0);
        const int rate=22050,samples=rate*12/10;
        wav.assign(44+samples*2,0);
        auto word=[&](size_t p,unsigned v,int n){for(int i=0;i<n;i++)wav[p+i]=(unsigned char)(v>>(i*8));};
        memcpy(wav.data(),"RIFF",4);word(4,(unsigned)wav.size()-8,4);memcpy(wav.data()+8,"WAVEfmt ",8);
        word(16,16,4);word(20,1,2);word(22,1,2);word(24,rate,4);word(28,rate*2,4);word(32,2,2);word(34,16,2);
        memcpy(wav.data()+36,"data",4);word(40,samples*2,4);
        for(int i=0;i<samples;i++){
            double t=(double)i/rate;int note=(int)(t/0.4);double local=t-note*0.4;
            double envelope=std::min(1.0,local/0.015)*std::max(0.0,std::min(1.0,(0.30-local)/0.025));
            double hz=note==0?660:note==1?880:1100;
            short value=(short)(12000*(loudness/100.0)*envelope*std::sin(6.28318530718*hz*t));
            word(44+i*2,(unsigned short)value,2);
        }
        BOOL ok=PlaySoundW((LPCWSTR)wav.data(),nullptr,SND_MEMORY|SND_ASYNC|SND_NODEFAULT);
        if(ok)soundUntil=GetTickCount64()+1300;
        return ok!=FALSE;
    }
    void deployed(unsigned token){
        if(!enabled||!deployment.armed||token!=deployment.token)return;
        deployment.finish();capture.stop();lastAlert=GetTickCount64();
        SYSTEMTIME t;GetLocalTime(&t);wchar_t clock[32];swprintf(clock,32,L"%02u:%02u:%02u",t.wHour,t.wMinute,t.wSecond);
        status=std::wstring(clock)+L" · "+i18n::text(L"출격 1초 전 · 카운트다운 확인");
        if(!sound())status+=i18n::text(L" / 소리 재생 실패");
        lastEvent=status;
        showStatus();
        balloon::show(i18n::text(L"타르코프 시작 알림"),i18n::text(L"출격 1초 전 · 카운트다운 확인"),L"EscapeFromTarkov.exe");
    }
    void pollDeployment(){
        ULONGLONG now=GetTickCount64();
        if(!enabled||!deployment.armed){if(capture.target())capture.stop();return;}
        if(now/1000.0-deployment.since>600||!tarkovProcess()){deployment.cancel();capture.stop();return;}
        HWND h=wgc::window(tarkovPid);if(!h){if(capture.target())capture.stop();return;}
        if(captureToken!=deployment.token){capture.stop();captureRetry=0;captureToken=deployment.token;}
        if((!capture.running()||capture.target()!=h)&&now>=captureRetry){captureRetry=now+3000;capture.start(h,win,captureToken);}
        status=capture.read();
    }
    struct Candidate{std::wstring path;ULONGLONG created=0;};
    void scan(const std::wstring& path,int depth,std::vector<Candidate>& files){
        WIN32_FIND_DATAW d;HANDLE h=FindFirstFileW((path+L"\\*").c_str(),&d);if(h==INVALID_HANDLE_VALUE)return;
        do{
            std::wstring name=d.cFileName;
            if(name==L"."||name==L".."||(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))continue;
            auto full=path+L"\\"+name;
            if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(depth>0)scan(full,depth-1,files);continue;}
            if(!raid::logName(name))continue;
            files.push_back({full,number(d.ftCreationTime)});
        }while(FindNextFileW(h,&d));FindClose(h);
    }
    void poll(){
        pollDeployment();
        if(!dialog&&GetTickCount64()>=nextDiscovery){nextDiscovery=GetTickCount64()+10000;discoverMissing();}
        // The auto-started map helper needs raid context even when the sound alert is off.
        if(!enabled&&!companionEnabled()&&!(companionProcess&&WaitForSingleObject(companionProcess,0)==WAIT_TIMEOUT)){showStatus();return;}
        if(!raidpaths::directory(folder)){status=i18n::text(L"로그 폴더를 기다리는 중 · 자동 찾기나 직접 선택을 눌러 주세요.");showStatus();return;}
        // The Logs folder holds hundreds of past sessions: walk all of it only every 10 s to find the newest
        // session, and list just that session's folder on the other ticks (this walk was most of the exe's CPU).
        std::vector<Candidate> files;ULONGLONG tick=GetTickCount64();
        if(session.empty()||tick>=nextSessionScan||!raidpaths::directory(session)){nextSessionScan=tick+10000;scan(folder,2,files);}else scan(session,0,files);
        if(files.empty()){session.clear();status=i18n::text(L"게임 로그를 기다리는 중 · 타르코프를 실행하면 시작합니다.");showStatus();baseline=false;return;}
        std::sort(files.begin(),files.end(),[](const Candidate& a,const Candidate& b){return a.created>b.created||(a.created==b.created&&a.path>b.path);});
        session=raidpaths::parent(files.front().path);
        files.erase(std::remove_if(files.begin(),files.end(),[&](const Candidate& c){return raidpaths::parent(c.path)!=session;}),files.end());
        if(files.size()>32)files.resize(32);
        for(auto it=streams.begin();it!=streams.end();){
            bool present=std::any_of(files.begin(),files.end(),[&](const Candidate& c){return c.path==it->first;});
            if(!present)it=streams.erase(it);else ++it;
        }
        for(const auto& candidate:files){
            HANDLE h=CreateFileW(candidate.path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(h==INVALID_HANDLE_VALUE){readFailures++;status=i18n::text(L"로그 읽기 실패 · Windows 오류 ")+std::to_wstring(GetLastError());continue;}
            LARGE_INTEGER size;if(!GetFileSizeEx(h,&size)){readFailures++;CloseHandle(h);continue;}
            auto inserted=streams.emplace(candidate.path,Stream{});auto& stream=inserted.first->second;
            if(inserted.second){
                stream.offset=raid::initialOffset(baseline,candidate.created,startedAt,(ULONGLONG)size.QuadPart);
                // Content skipped on attach still holds the login's selected-profile line: learn ids only.
                if(stream.offset&&stream.offset<=16777216){std::string text;char chunk[16384];DWORD got=0;ULONGLONG left=stream.offset;
                    while(left&&ReadFile(h,chunk,(DWORD)std::min<ULONGLONG>(sizeof(chunk),left),&got,nullptr)&&got){text.append(chunk,got);left-=got;}
                    size_t from=0,end;while((end=text.find('\n',from))!=std::string::npos){context.learn(text.substr(from,end-from));from=end+1;}saveProfiles();}
                status=i18n::text(L"감시 중 · 새 출격 신호를 기다립니다.");
            }
            stream.size=(ULONGLONG)size.QuadPart;
            if(stream.size<stream.offset){stream.offset=0;stream.parser.reset();}
            LARGE_INTEGER pos;pos.QuadPart=(LONGLONG)stream.offset;
            if(!SetFilePointerEx(h,pos,nullptr,FILE_BEGIN)){readFailures++;CloseHandle(h);continue;}
            char buf[16384];DWORD read=0;unsigned budget=0;
            while(budget<262144){
                if(!ReadFile(h,buf,sizeof(buf),&read,nullptr)){readFailures++;status=i18n::text(L"로그 읽기 중 오류가 발생했습니다.");break;}
                if(!read)break;
                stream.offset+=read;budget+=read;bytesRead+=read;lastRead=clock();file=candidate.path;
                stream.contextPartial.append(buf,read);size_t from=0,end;
                while((end=stream.contextPartial.find('\n',from))!=std::string::npos){auto line=stream.contextPartial.substr(from,end-from);if(context.line(line))publishRaidContext(context);if(enabled)deployment.line(line,GetTickCount64()/1000.0);from=end+1;}saveProfiles();stream.contextPartial.erase(0,from);if(stream.contextPartial.size()>65536)stream.contextPartial.clear();
            }
            CloseHandle(h);
        }
        baseline=false;pollDeployment();showStatus();
    }
    void load(){
        folder=regString(configPath,L"RaidLogs");enabled=regString(configPath,L"RaidEnabled")==L"1";
        unsigned v=70;if(settings::parseVolume(regString(configPath,L"RaidVolume"),v))loudness=v;
        loadProfiles();reset();discoverMissing();nextDiscovery=GetTickCount64()+10000;
        if(!enabled&&raidpaths::directory(folder))status=i18n::text(L"알림이 꺼져 있습니다 · 게임별 기능에서 켜세요.");
    }
    bool save(){return saveString(configPath,L"RaidLogs",folder)&&saveString(configPath,L"RaidEnabled",enabled?L"1":L"0")&&saveString(configPath,L"RaidVolume",std::to_wstring(loudness));}
} raidWatch;
LRESULT CALLBACK raidProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        raidWatch.dialog=h;
        // On/off is the switch on 게임별 기능; this page holds where the logs are and how loud the alert is.
        pageLabel(h,i18n::text(L"게임 로그 폴더"),24,28,688,28,2);
        pageLabel(h,i18n::text(L"로그로 감시를 시작하고 배치 화면의 카운트다운이 1초 남으면 알립니다."),24,60,688,24,1);
        addSetting(h,L"EDIT",raidWatch.folder.c_str(),WS_BORDER|ES_READONLY|ES_AUTOHSCROLL,24,94,420,34,71);
        pageButton(h,i18n::text(L"자동 찾기"),456,91,120,77);
        pageButton(h,i18n::text(L"직접 선택"),588,91,124,72);
        pageLabel(h,i18n::text(L"알림 볼륨"),24,160,688,28,2);
        // Volume: speaker icon and a slider (the number shows while it moves), then a sound test.
        addSetting(h,L"BlueAshLevel",L"",WS_TABSTOP,24,184,500,46,73);levelSet(h,73,(int)raidWatch.loudness);
        pageButton(h,i18n::text(L"소리 테스트"),562,194,150,74);
        pageLabel(h,i18n::text(L"브라우저 볼륨과 따로 정합니다. Windows나 BlueAsh가 음소거돼 있으면 들리지 않습니다."),24,238,688,40,1);
        pageLabel(h,i18n::text(L"상태"),24,288,688,28,2);
        pageLabel(h,raidWatch.status.c_str(),24,320,688,44,0,76);
        HWND detail=addSetting(h,L"EDIT",raidWatch.diagnostics().c_str(),WS_BORDER|ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,24,372,688,116,78);
        SendMessageW(detail,WM_SETFONT,(WPARAM)smallFont,TRUE);
        pageButton(h,i18n::text(L"진단 내용 복사"),24,556,170,79);
        pageButton(h,i18n::text(L"닫기"),440,556,120,80);
        pageButton(h,i18n::text(L"저장"),572,556,140,75,true);
        return 0;
    }
    case WM_COMMAND:{
        int id=LOWORD(wp);
        if(id==79){
            bool ok=raidWatch.copyDiagnostics();SetDlgItemTextW(h,76,ok?i18n::text(L"진단 내용을 복사했습니다. 문의할 때 Ctrl+V로 붙여 넣으세요."):i18n::text(L"복사 실패. 아래 진단 내용을 직접 선택해 복사하세요."));
        }else if(id==77){
            auto found=raidpaths::discover();
            if(found.empty())SetDlgItemTextW(h,76,i18n::text(L"찾지 못했습니다. 타르코프를 한 번 실행한 뒤 다시 눌러 주세요."));
            else {SetDlgItemTextW(h,71,found.c_str());SetDlgItemTextW(h,76,i18n::text(L"로그 폴더를 찾았습니다. 저장을 누르세요."));}
        }else if(id==72){
            BROWSEINFOW bi={};bi.hwndOwner=h;bi.lpszTitle=i18n::text(L"타르코프 설치 폴더의 Logs 선택");bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
            PIDLIST_ABSOLUTE p=SHBrowseForFolderW(&bi);
            if(p){wchar_t path[MAX_PATH];if(SHGetPathFromIDListW(p,path))SetDlgItemTextW(h,71,path);CoTaskMemFree(p);}
        }else if(id==80){DestroyWindow(h);
        }else if(id==74||id==75){
            unsigned v=(unsigned)levelValue(h,73);
            if(id==74){auto old=raidWatch.loudness;raidWatch.loudness=v;bool ok=raidWatch.sound();raidWatch.loudness=old;SetDlgItemTextW(h,76,ok?i18n::text(L"알림음을 울렸습니다. 안 들리면 Windows 볼륨을 확인하세요."):i18n::text(L"알림음을 재생하지 못했습니다."));return 0;}
            wchar_t path[32768]={};GetDlgItemTextW(h,71,path,32768);
            bool on=raidWatch.enabled;
            DWORD attrs=GetFileAttributesW(path);
            if(on&&*path&&(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_DIRECTORY))){SetDlgItemTextW(h,76,i18n::text(L"로그 폴더가 아닙니다. 자동 찾기나 직접 선택으로 다시 골라 주세요."));return 0;}
            bool reset=raidWatch.folder!=path;
            raidWatch.folder=path;raidWatch.loudness=v;
            if(reset)raidWatch.reset();
            if(!raidWatch.save()){SetDlgItemTextW(h,76,i18n::text(L"설정 저장 실패. 현재 실행에는 적용되지만 재실행 시 유지되지 않을 수 있습니다."));return 0;}
            raidWatch.status=on?(*path?i18n::text(L"저장했습니다 · 출격 신호를 기다립니다."):i18n::text(L"저장했습니다 · 타르코프를 실행하면 로그 폴더를 찾습니다.")):i18n::text(L"저장했습니다 · 알림은 꺼져 있습니다 (게임별 기능에서 켜기).");raidWatch.nextDiscovery=0;raidWatch.showStatus();
        }return 0;
    }
    case WM_ERASEBKGND:return eraseCarbon(h,(HDC)wp);
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:raidWatch.dialog=nullptr;editor=nullptr;EnableWindow(win,TRUE);SetForegroundWindow(win);return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
