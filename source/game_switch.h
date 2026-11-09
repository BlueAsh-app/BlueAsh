#pragma once
// 게임별 프로필 (profile page, below 게임 모드): PC games found from store records (dlsslib::scan, without the DLSS search)
// can each get their own colour profile, saved in HKCU GameProfiles keyed by the install folder (game_profile_state.h).
// A thread checks every 3 s which of those games runs (and PUBG / Tarkov by exe name); when one with its own settings
// starts, BlueAsh switches to 게임 모드 with that profile, and back to 일반 모드 when it closes (unless the person
// changed the mode since). A game whose settings were never changed (PUBG / Tarkov: still the defaults) switches nothing.
#include <mutex>
#include <memory>
#include "dlss_library.h"
#include "game_profile_state.h"
namespace gameswitch {
inline bool same(const settings::Profile& a,const settings::Profile& b){settings::Profiles x={a,a,a,a},y={b,b,b,b};return settings::encodeV2(x,false)==settings::encodeV2(y,false);}
inline bool touched(int i){return !same(profiles[i],settings::defaults()[i]);}
struct Row {std::wstring dir,name;};
std::vector<Row> rows;bool scanned=false,scanning=false;HWND notify=nullptr; // the list, scanned once per run on a thread
std::mutex lock;std::vector<std::wstring> dirs;  // saved games the watcher looks for
std::wstring autoFor;                            // the game that switched BlueAsh into 게임 모드
inline std::wstring leaf(const std::wstring& d){auto s=d.find_last_of(L'\\');return s==std::wstring::npos?d:d.substr(s+1);}
inline void setDirs(){std::lock_guard<std::mutex> g(lock);dirs.clear();for(auto& [d,p]:gameProfiles)dirs.push_back(d);}
// PC games for the profile list, PUBG and Tarkov left out (they have their fixed profiles 2 and 3). WM_APP+185 to notify.
inline void scan(HWND to){notify=to;if(scanned||scanning)return;scanning=true;std::wstring* folders=new std::wstring(regString(configPath,L"DlssFolders"));
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);std::unique_ptr<std::wstring> f((std::wstring*)p);
        auto* out=new std::vector<Row>;for(auto& g:dlsslib::scan(*f,false)){if(g.v.reason==dlss::blocked()[0].reason||g.v.reason==dlss::blocked()[1].reason)continue;out->push_back({g.e.dir,g.e.name.empty()?leaf(g.e.dir):g.e.name});}
        std::sort(out->begin(),out->end(),[](const Row& a,const Row& b){return _wcsicmp(a.name.c_str(),b.name.c_str())<0;});
        if(!PostMessageW(win,WM_APP+185,0,(LPARAM)out)){delete out;}return 0;},folders,0,nullptr);
    if(t)CloseHandle(t);else{delete folders;scanning=false;}}
// What runs now: a saved game's folder, "#2" PUBG, "#3" Tarkov, or "" — the foreground one first (choose).
inline DWORD WINAPI watch(LPVOID){std::wstring last=L"?";for(;;){std::vector<std::wstring> d;{std::lock_guard<std::mutex> g(lock);d=dirs;}
    DWORD fg=0;GetWindowThreadProcessId(GetForegroundWindow(),&fg);std::vector<std::wstring> running;std::wstring front;
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(s!=INVALID_HANDLE_VALUE){PROCESSENTRY32W e={};e.dwSize=sizeof(e);for(BOOL ok=Process32FirstW(s,&e);ok;ok=Process32NextW(s,&e)){int gid=gameId(e.szExeFile);std::wstring id;
            if(gid>=2)id=L"#"+std::to_wstring(gid);
            else if(!d.empty()){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);if(p){wchar_t path[MAX_PATH]={};DWORD n=MAX_PATH;
                if(QueryFullProcessImageNameW(p,0,path,&n)){for(auto& x:d){if(gameprofiles::within(path,x)){id=x;break;}}}CloseHandle(p);}}
            if(id.empty()){continue;}if(std::find(running.begin(),running.end(),id)==running.end())running.push_back(id);if(e.th32ProcessID==fg)front=id;}
        CloseHandle(s);}
    auto pick=gameprofiles::choose(running,front,last);if(pick!=last){auto* m=new std::wstring(pick);if(PostMessageW(win,WM_APP+186,0,(LPARAM)m))last=pick;else delete m;}
    Sleep(3000);}}
inline void start(){std::map<std::wstring,settings::Profile> saved;if(gameprofiles::decodeProfiles(regString(configPath,L"GameProfiles"),saved))gameProfiles=saved;setDirs();
    HANDLE t=CreateThread(nullptr,0,watch,nullptr,0,nullptr);if(t){SetThreadPriority(t,THREAD_PRIORITY_BELOW_NORMAL);CloseHandle(t);}}
inline void changed(const std::wstring& id){runningGame=id;int idx=id==L"#2"?2:id==L"#3"?3:4;bool own=!id.empty()&&(idx<4?touched(idx):gameProfiles.count(id)>0);
    // the game that switched BlueAsh in has closed: back to 일반 모드 when nothing with its own settings runs now
    if(!autoFor.empty()&&autoFor!=id){bool ours=active&&fps&&(autoFor[0]==L'#'?appliedProfileIndex==autoFor[1]-L'0':appliedProfileIndex==4&&gameProfileId==autoFor);autoFor.clear();if(ours&&!own)changeMode(false);}
    if(!own||(active&&fps&&appliedProfileIndex==idx&&(idx<4||gameProfileId==id)))return;
    if(idx==4){gameProfileId=id;}if(changeMode(true,idx))autoFor=id;}
}
