#pragma once
#include "i18n.h"
#include "pubg_log.h"
namespace pubggame {
inline bool foreground(DWORD pid){DWORD front=0;GetWindowThreadProcessId(GetForegroundWindow(),&front);return pid&&front==pid;}
inline DWORD gamePid(){
    HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE)return 0;
    // Two TslGame.exe can run at once (the game and a small windowless helper): take the one with a visible window.
    PROCESSENTRY32W e={};e.dwSize=sizeof(e);std::vector<DWORD> pids;
    if(Process32FirstW(snap,&e))do{if(!_wcsicmp(e.szExeFile,L"TslGame.exe"))pids.push_back(e.th32ProcessID);}while(Process32NextW(snap,&e));
    CloseHandle(snap);if(pids.size()<2)return pids.empty()?0:pids[0];
    struct Find {const std::vector<DWORD>* pids;DWORD found;};Find f={&pids,0};
    EnumWindows([](HWND h,LPARAM l)->BOOL{auto& f=*(Find*)l;DWORD p=0;GetWindowThreadProcessId(h,&p);
        if(IsWindowVisible(h)&&!GetWindow(h,GW_OWNER)&&std::find(f.pids->begin(),f.pids->end(),p)!=f.pids->end()){f.found=p;return FALSE;}return TRUE;},(LPARAM)&f);
    return f.found?f.found:pids.back();
}
}
// Last match join seen in TslGame.log (0 = none since start) and whether its plane boarding was already announced.
ULONGLONG pubgMatchAt=0;bool pubgBoarded=false;
inline void pubgMarkBoarded(){pubgBoarded=true;}
// Alert sound volume for PUBG (PubgAudioVolume, kept from the old plane-sound settings).
unsigned pubgLoudness=50;
inline bool pubgSound(){unsigned old=raidWatch.loudness;raidWatch.loudness=pubgLoudness;bool ok=raidWatch.sound();raidWatch.loudness=old;return ok;}
// One line per step of the boarding alert in %LOCALAPPDATA%\ModeSwitch\pubg-board.log (restarted when over 256 KB).
void phoneAlert(const char* kind); // phone_game.h: PUBG away alert to the paired phone when the PC is idle
inline void pubgNote(const std::wstring& s){
    wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local)))return;
    std::wstring p=std::wstring(local)+L"\\ModeSwitch\\pubg-board.log";WIN32_FILE_ATTRIBUTE_DATA a;
    bool big=GetFileAttributesExW(p.c_str(),GetFileExInfoStandard,&a)&&a.nFileSizeLow>262144;FILE* f=_wfopen(p.c_str(),big?L"wb":L"ab");if(!f)return;
    SYSTEMTIME t;GetLocalTime(&t);wchar_t c[32];swprintf(c,32,L"%02u:%02u:%02u ",t.wHour,t.wMinute,t.wSecond);std::wstring line=c+s+L"\r\n";
    int n=WideCharToMultiByte(CP_UTF8,0,line.c_str(),(int)line.size(),nullptr,0,nullptr,nullptr);std::string u(n,0);WideCharToMultiByte(CP_UTF8,0,line.c_str(),(int)line.size(),&u[0],n,nullptr,nullptr);fwrite(u.data(),1,u.size(),f);fclose(f);
}

// Match join from TslGame.log (pubg_log.h): arms the boarding alert (pubg_board.h), no sound of its own. Only the file size is checked each tick; new bytes are
// read when it grows. A new log on game start (smaller or newly created) is read from its beginning.
struct PubgJoin {
    std::wstring path;ULONGLONG offset=0,created=0,lastAlert=0;bool attached=false;pubglog::Parser parser;
    void load(){pubgJoinOn=regString(configPath,L"PubgJoinEnabled")!=L"0";unsigned v;if(settings::parseVolume(regString(configPath,L"PubgAudioVolume"),v))pubgLoudness=v;
        wchar_t local[MAX_PATH]={};if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local)))path=std::wstring(local)+L"\\TslGame\\Saved\\Logs\\TslGame.log";
        auto test=regString(configPath,L"PubgLogPathTest");if(!test.empty())path=test;} // end-to-end tests with a fake log
    void toggle(){pubgJoinOn=!pubgJoinOn;saveString(configPath,L"PubgJoinEnabled",pubgJoinOn?L"1":L"0");attached=false;}
    void poll(){
        if(!pubgJoinOn||path.empty())return;
        // Size from the open handle: the directory entry (GetFileAttributesEx) lags while the game keeps the log open.
        HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE){attached=false;return;}
        BY_HANDLE_FILE_INFORMATION info;if(!GetFileInformationByHandle(h,&info)){CloseHandle(h);return;}
        ULONGLONG made=RaidWatch::number(info.ftCreationTime),size=((ULONGLONG)info.nFileSizeHigh<<32)|info.nFileSizeLow;
        if(!attached){attached=true;created=made;offset=size;parser.reset();pubgNote(i18n::text(L"로그 감시 시작 (")+std::to_wstring(size)+L" bytes)");resume(h,size);CloseHandle(h);return;}
        if(made!=created||size<offset){created=made;offset=0;parser.reset();pubgNote(i18n::text(L"새 게임 로그 · 처음부터 읽기"));}
        if(size==offset){CloseHandle(h);return;}
        LARGE_INTEGER pos;pos.QuadPart=(LONGLONG)offset;char buf[16384];DWORD got=0;unsigned budget=0;
        if(SetFilePointerEx(h,pos,nullptr,FILE_BEGIN))while(budget<1048576&&ReadFile(h,buf,sizeof(buf),&got,nullptr)&&got){offset+=got;budget+=got;parser.feed(buf,got,[&](const std::string&){alert();});}
        CloseHandle(h);
    }
    // BlueAsh (re)started mid-match: a join in the last 2 minutes is taken over, so that match's boarding alert still
    // comes (the log is only read from here on). Older ones are left alone: the plane has probably gone already.
    void resume(HANDLE h,ULONGLONG size){
        ULONGLONG from=size>2097152?size-2097152:0;LARGE_INTEGER pos;pos.QuadPart=(LONGLONG)from;if(!SetFilePointerEx(h,pos,nullptr,FILE_BEGIN)){return;}
        std::string tail(size_t(size-from),0);DWORD got=0;if(!ReadFile(h,&tail[0],(DWORD)tail.size(),&got,nullptr)){return;}tail.resize(got);
        long long t=pubglog::lastJoinUtc(tail);if(t<0){return;}FILETIME ft;GetSystemTimeAsFileTime(&ft);long long nowUtc=(long long)(RaidWatch::number(ft)/10000000ULL)-11644473600LL;
        long long age=nowUtc-t;if(age<0||age>120){return;}
        ULONGLONG now=GetTickCount64();pubgMatchAt=now>ULONGLONG(age)*1000+1?now-ULONGLONG(age)*1000:1;pubgBoarded=false;lastAlert=pubgMatchAt;
        pubgNote(i18n::text(L"재시작 전 매치 입장 이어받음 (")+std::to_wstring(age)+i18n::text(L"초 전)"));
    }
    void alert(){
        ULONGLONG now=GetTickCount64();if(lastAlert&&now-lastAlert<60000)return;lastAlert=now;
        pubgMatchAt=now;pubgBoarded=false;pubgNote(i18n::text(L"매치 입장 감지"));phoneAlert("pubg.match.away");
    }
} pubgJoin;

