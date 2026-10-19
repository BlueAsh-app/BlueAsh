#pragma once
#include <wincrypt.h>
// Phone page per game: the game running on this PC (Tarkov / PUBG / none; the first started of several) goes to the
// relay (/phone/game) when it changes, so the phone shows that game's options. And PUBG away alerts to the phone
// (/phone/notify pubg.match.away / pubg.board.away) when the PC has had no input for the helper's "자리 비움 기준".
// The pairing code is the helper's (companion-settings.json phoneCode, DPAPI for this Windows user); no phone
// paired = nothing is sent. Only the game name and its start time leave the PC.
namespace phonegame {
inline std::wstring settingsPath(){wchar_t dir[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,dir);return std::wstring(dir)+L"\\ModeSwitch\\companion-settings.json";}
// "phoneCode": ConvertFrom-SecureString output = hex of a DPAPI blob of the UTF-16 text.
inline std::string jsonString(const std::string& s,const char* key){auto at=s.find(std::string("\"")+key+"\"");if(at==std::string::npos)return "";at=s.find(':',at);if(at==std::string::npos)return "";at=s.find('"',at);if(at==std::string::npos)return "";auto end=s.find('"',at+1);return end==std::string::npos?"":s.substr(at+1,end-at-1);}
inline int jsonInt(const std::string& s,const char* key,int fallback){auto at=s.find(std::string("\"")+key+"\"");if(at==std::string::npos)return fallback;at=s.find(':',at);if(at==std::string::npos)return fallback;return atoi(s.c_str()+at+1);}
inline std::string unprotect(const std::string& hex){
    if(hex.empty()||hex.size()%2){return "";}std::vector<BYTE> blob(hex.size()/2);for(size_t i=0;i<blob.size();i++){unsigned v=0;if(sscanf(hex.c_str()+i*2,"%2x",&v)!=1)return "";blob[i]=(BYTE)v;}
    DATA_BLOB in={(DWORD)blob.size(),blob.data()},out={};if(!CryptUnprotectData(&in,nullptr,nullptr,nullptr,nullptr,0,&out))return "";
    std::wstring w((const wchar_t*)out.pbData,out.cbData/sizeof(wchar_t));LocalFree(out.pbData);std::string a;
    for(wchar_t c:w){if((c>='0'&&c<='9')||(c>='a'&&c<='f'))a+=(char)c;else return "";}return a.size()==32?a:""; // the relay's code: 32 hex
}
inline unsigned long long fileTimeMs(const FILETIME& f){return ((((unsigned long long)f.dwHighDateTime<<32)|f.dwLowDateTime)/10000ULL)-11644473600000ULL;}
// Earliest-started running instance of each game (Unix ms), 0 = not running.
inline void running(unsigned long long& tarkov,unsigned long long& pubg){tarkov=pubg=0;HANDLE snap=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snap==INVALID_HANDLE_VALUE){return;}
    PROCESSENTRY32W e={};e.dwSize=sizeof(e);
    for(BOOL ok=Process32FirstW(snap,&e);ok;ok=Process32NextW(snap,&e)){unsigned long long* slot=!_wcsicmp(e.szExeFile,L"EscapeFromTarkov.exe")?&tarkov:!_wcsicmp(e.szExeFile,L"TslGame.exe")?&pubg:nullptr;if(!slot){continue;}
        HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,e.th32ProcessID);unsigned long long t=1;FILETIME c,x,k,u;if(p){if(GetProcessTimes(p,&c,&x,&k,&u))t=fileTimeMs(c);CloseHandle(p);}
        if(!*slot||t<*slot)*slot=t;}
    CloseHandle(snap);}
inline void post(const std::wstring& path,const std::string& body){auto* job=new std::pair<std::wstring,std::string>(path,body);
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(std::pair<std::wstring,std::string>*)p;relayRequest(L"POST",j->first,j->second);delete j;return 0;},job,0,nullptr);if(t)CloseHandle(t);else delete job;}
struct Watch {
    std::string code;int idle=20;ULONGLONG readAt=0,checkAt=0,sentAt=0;FILETIME stamp={};std::string sent="?";
    // The helper's settings: re-read when the file changes (pairing made, "자리 비움 기준" changed), looked at every 10 s.
    void settings(ULONGLONG now){if(now<readAt){return;}readAt=now+10000;WIN32_FILE_ATTRIBUTE_DATA a;if(!GetFileAttributesExW(settingsPath().c_str(),GetFileExInfoStandard,&a)){code.clear();return;}
        if(CompareFileTime(&a.ftLastWriteTime,&stamp)==0){return;}stamp=a.ftLastWriteTime;std::string s;FILE* f=_wfopen(settingsPath().c_str(),L"rb");if(f){char b[8192];size_t n;while((n=fread(b,1,sizeof b,f))>0)s.append(b,n);fclose(f);}
        code=unprotect(jsonString(s,"phoneCode"));idle=std::max(10,jsonInt(s,"idle",20));}
    // Every 3 s: the game in play; sent when it changes (and again every 12 h, the relay keeps it a day).
    void poll(){ULONGLONG now=GetTickCount64();settings(now);if(code.empty()||now<checkAt){return;}checkAt=now+3000;
        unsigned long long tk,pg;running(tk,pg);std::string game="none";unsigned long long since=0;
        if(tk&&(!pg||tk<=pg)){game="tarkov";since=tk;}else if(pg){game="pubg";since=pg;}
        if(game==sent&&now-sentAt<12ULL*3600*1000){return;}sent=game;sentAt=now;
        post(L"/phone/game","{\"c\":\""+code+"\",\"game\":\""+game+"\",\"since\":"+std::to_string(since)+"}");}
    static unsigned idleMs(){LASTINPUTINFO i={sizeof(i),0};return GetLastInputInfo(&i)?GetTickCount()-i.dwTime:0;}
    void alert(const char* kind){settings(GetTickCount64());if(code.empty()||idleMs()<unsigned(idle)*1000){return;}std::string lang;for(wchar_t c:i18n::languageCode())lang+=(char)c;
        pubgNote(L"phone alert "+std::wstring(kind,kind+strlen(kind))+L" (idle "+std::to_wstring(idleMs()/1000)+L" s)");
        post(L"/phone/notify","{\"c\":\""+code+"\",\"kind\":\""+kind+"\",\"language\":\""+lang+"\"}");}
};
}
phonegame::Watch phoneGame;
void phoneAlert(const char* kind){phoneGame.alert(kind);}
