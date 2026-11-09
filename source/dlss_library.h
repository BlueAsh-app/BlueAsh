#pragma once
// DLSS 5 page: finds installed games from store records only (Steam library manifests, Epic manifests, GOG registry,
// Xbox .GamingRoot folders) plus folders the user added (registry DlssFolders, "|" separated). No drive-wide search.
// For each game it gathers the evidence dlss::classify needs: names of files and folders two levels deep and the
// product / company names of the exes there (bounded: 600 names, 24 exes).
#include "dlss_policy.h"
namespace dlsslib {
// dlssDir / dlssExe: the folder that holds the game's own DLSS (nvngx_dlss.dll) next to an exe, and that exe: where the
// community route (OptiScaler DLSS-NR) goes for a game BlueAsh has not verified. Empty when the game has no DLSS.
struct Game {dlss::Evidence e;dlss::Verdict v;std::wstring exe,dlssDir,dlssExe;};
inline std::string narrow(const std::wstring& w){std::string s;for(wchar_t c:w)s+=c<128?(char)tolower((int)c):'?';return s;}
inline std::wstring readUtf8(const std::wstring& path){HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return L"";
    std::string s;char b[8192];DWORD n=0;while(ReadFile(f,b,sizeof(b),&n,nullptr)&&n&&s.size()<4000000)s.append(b,n);CloseHandle(f);
    int w=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring r(w,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&r[0],w);return r;}
// "key"  "value" pairs of Valve's text format, and "key": "value" of JSON: both are a quoted key, then the next quoted value.
inline std::wstring field(const std::wstring& text,const std::wstring& key,size_t from=0){size_t k=text.find(L"\""+key+L"\"",from);if(k==std::wstring::npos)return L"";
    size_t a=text.find(L'"',k+key.size()+2);if(a==std::wstring::npos)return L"";size_t b=text.find(L'"',a+1);if(b==std::wstring::npos)return L"";
    std::wstring v=text.substr(a+1,b-a-1),out;for(size_t i=0;i<v.size();i++){if(v[i]==L'\\'&&i+1<v.size()){out+=v[++i];}else out+=v[i];}return out;}
inline std::wstring versionString(const std::wstring& exe,const wchar_t* what){
    DWORD h=0,n=GetFileVersionInfoSizeW(exe.c_str(),&h);if(!n||n>1<<20)return L"";std::vector<BYTE> buf(n);if(!GetFileVersionInfoW(exe.c_str(),0,n,buf.data()))return L"";
    struct Lang{WORD l,c;}*langs=nullptr;UINT len=0;std::wstring out;if(VerQueryValueW(buf.data(),L"\\VarFileInfo\\Translation",(void**)&langs,&len)){
        for(UINT i=0;i<len/sizeof(Lang)&&i<4;i++){wchar_t q[96];swprintf(q,96,L"\\StringFileInfo\\%04x%04x\\%ls",langs[i].l,langs[i].c,what);wchar_t* v=nullptr;UINT vl=0;if(VerQueryValueW(buf.data(),q,(void**)&v,&vl)&&v&&vl)out+=std::wstring(v)+L" ";}}
    return out;}
// Files and folders two levels down (names only), exe resources for up to 24 exes; the biggest exe at the top is "the" exe.
inline void gather(Game& g){
    std::vector<std::wstring> dirs={g.e.dir};int exes=0;ULONGLONG biggest=0;
    for(int depth=0;depth<3&&!dirs.empty();depth++){std::vector<std::wstring> next;
        for(auto& d:dirs){WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((d+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)continue;
            do{std::wstring n=f.cFileName;if(n==L"."||n==L"..")continue;if(g.e.files.size()>=600)break;g.e.files.push_back(narrow(n));bool dir=(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
                if(dir){if(!(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)&&depth<2)next.push_back(d+L"\\"+n);continue;}
                if(n.size()>4&&!_wcsicmp(n.c_str()+n.size()-4,L".exe")&&exes<24){exes++;std::wstring p=d+L"\\"+n;
                    for(auto* k:{L"ProductName",L"CompanyName",L"OriginalFilename"}){auto s=narrow(versionString(p,k));if(!s.empty())g.e.products.push_back(s);}
                    ULONGLONG size=(ULONGLONG(f.nFileSizeHigh)<<32)|f.nFileSizeLow;if(depth==0&&size>biggest){biggest=size;g.exe=p;}}
            }while(FindNextFileW(h,&f));FindClose(h);}
        dirs=next;}}
// Breadth first, at most 5 levels and 3000 folders: the shallowest folder with nvngx_dlss.dll and an exe (the biggest exe
// there that is not a launcher, crash reporter or installer).
inline void findDlss(Game& g){std::vector<std::wstring> dirs={g.e.dir};int seen=0;
    for(int depth=0;depth<6&&!dirs.empty();depth++){std::vector<std::wstring> next;
        for(auto& d:dirs){if(++seen>3000)return;WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((d+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)continue;bool dll=false;std::wstring exe;ULONGLONG big=0;
            do{std::wstring n=f.cFileName;if(n==L"."||n==L"..")continue;
                if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))next.push_back(d+L"\\"+n);continue;}
                if(!_wcsicmp(n.c_str(),L"nvngx_dlss.dll"))dll=true;
                if(n.size()>4&&!_wcsicmp(n.c_str()+n.size()-4,L".exe")){std::string l=narrow(n);bool tool=false;for(auto* t:{"crash","launcher","unins","setup","redist","report","helper","update"})if(l.find(t)!=std::string::npos)tool=true;
                    ULONGLONG size=(ULONGLONG(f.nFileSizeHigh)<<32)|f.nFileSizeLow;if(!tool&&size>big){big=size;exe=n;}}}while(FindNextFileW(h,&f));FindClose(h);
            if(dll&&!exe.empty()){g.dlssDir=d;g.dlssExe=exe;return;}}
        dirs=next;}}
inline void add(std::vector<Game>& out,const char* platform,const std::wstring& id,const std::wstring& name,const std::wstring& dir,bool deep=true){
    if(dir.empty()||GetFileAttributesW(dir.c_str())==INVALID_FILE_ATTRIBUTES)return;
    for(auto& g:out){if(!_wcsicmp(g.e.dir.c_str(),dir.c_str()))return;}
    Game g;g.e.platform=platform;g.e.id=narrow(id);g.e.name=name;g.e.dir=dir;gather(g);g.v=dlss::classify(g.e);if(deep&&g.v.status!=dlss::Status::Blocked)findDlss(g);out.push_back(g);}
inline std::wstring regValue(HKEY root,const std::wstring& key,const wchar_t* name){wchar_t v[1024]={};DWORD n=sizeof(v);if(RegGetValueW(root,key.c_str(),name,RRF_RT_REG_SZ,nullptr,v,&n)==ERROR_SUCCESS)return v;return L"";}
inline std::vector<Game> scan(const std::wstring& userFolders,bool deep=true){ // deep = also find the game's own DLSS (findDlss)
    std::vector<Game> out;
    // Steam: libraryfolders.vdf lists the libraries, each appmanifest_<id>.acf names one installed game.
    std::wstring steam=regValue(HKEY_CURRENT_USER,L"Software\\Valve\\Steam",L"SteamPath");for(auto& c:steam)if(c==L'/')c=L'\\';
    if(!steam.empty()){std::wstring vdf=readUtf8(steam+L"\\steamapps\\libraryfolders.vdf");std::vector<std::wstring> libs={steam};size_t at=0;
        while((at=vdf.find(L"\"path\"",at))!=std::wstring::npos){auto p=field(vdf,L"path",at);at+=6;if(!p.empty()&&std::find_if(libs.begin(),libs.end(),[&](const std::wstring& l){return !_wcsicmp(l.c_str(),p.c_str());})==libs.end())libs.push_back(p);}
        for(auto& lib:libs){WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((lib+L"\\steamapps\\appmanifest_*.acf").c_str(),&f);if(h==INVALID_HANDLE_VALUE)continue;
            do{auto acf=readUtf8(lib+L"\\steamapps\\"+f.cFileName);auto id=field(acf,L"appid"),name=field(acf,L"name"),dir=field(acf,L"installdir");
                if(id==L"228980")continue; // Steamworks redistributables
                if(!dir.empty())add(out,"steam",id,name,lib+L"\\steamapps\\common\\"+dir,deep);}while(FindNextFileW(h,&f));FindClose(h);}}
    // Epic: one JSON .item per installed app.
    {WIN32_FIND_DATAW f;std::wstring m=L"C:\\ProgramData\\Epic\\EpicGamesLauncher\\Data\\Manifests\\";HANDLE h=FindFirstFileW((m+L"*.item").c_str(),&f);
        if(h!=INVALID_HANDLE_VALUE){do{auto j=readUtf8(m+f.cFileName);add(out,"epic",field(j,L"AppName"),field(j,L"DisplayName"),field(j,L"InstallLocation"),deep);}while(FindNextFileW(h,&f));FindClose(h);}}
    // GOG Galaxy / installers: HKLM\SOFTWARE\WOW6432Node\GOG.com\Games\<id> gameName, path.
    {HKEY k=nullptr;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\WOW6432Node\\GOG.com\\Games",0,KEY_READ,&k)==ERROR_SUCCESS){wchar_t sub[256];
        for(DWORD i=0;;i++){DWORD n=256;if(RegEnumKeyExW(k,i,sub,&n,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS)break;std::wstring key=std::wstring(L"SOFTWARE\\WOW6432Node\\GOG.com\\Games\\")+sub;
            add(out,"gog",sub,regValue(HKEY_LOCAL_MACHINE,key,L"gameName"),regValue(HKEY_LOCAL_MACHINE,key,L"path"),deep);}RegCloseKey(k);}}
    // Xbox app: a .GamingRoot file at a drive's root names its games folder (UTF-16 after an 8-byte header); one folder per game (with a Content folder).
    for(wchar_t d=L'C';d<=L'Z';d++){std::wstring rootFile=std::wstring(1,d)+L":\\.GamingRoot";HANDLE f=CreateFileW(rootFile.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)continue;
        std::vector<char> b(4096);DWORD n=0;ReadFile(f,b.data(),(DWORD)b.size(),&n,nullptr);CloseHandle(f);if(n<10)continue;
        std::wstring rel((const wchar_t*)(b.data()+8),(n-8)/2);rel=rel.c_str();if(rel.empty())continue;std::wstring base=std::wstring(1,d)+L":\\"+rel;
        WIN32_FIND_DATAW g;HANDLE h=FindFirstFileW((base+L"\\*").c_str(),&g);if(h==INVALID_HANDLE_VALUE)continue;
        do{if((g.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&g.cFileName[0]!=L'.'){std::wstring content=base+L"\\"+g.cFileName+L"\\Content";if(GetFileAttributesW(content.c_str())!=INVALID_FILE_ATTRIBUTES)add(out,"xbox",g.cFileName,g.cFileName,content,deep);}}while(FindNextFileW(h,&g));FindClose(h);}
    // Folders the user added by hand: always "folder" (never verified by store id).
    size_t at=0;while(at<userFolders.size()){size_t bar=userFolders.find(L'|',at);std::wstring dir=userFolders.substr(at,bar==std::wstring::npos?std::wstring::npos:bar-at);
        if(!dir.empty()){auto cut=dir.find_last_of(L"\\/");add(out,"folder",L"",cut==std::wstring::npos?dir:dir.substr(cut+1),dir,deep);}if(bar==std::wstring::npos)break;at=bar+1;}
    return out;}
}
