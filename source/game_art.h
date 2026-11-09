#pragma once
// Game pictures for the dashboard rows and the game cards. Everyone sees the same cover: the game's official cover through
// BlueAsh's server (/cover, the IGDB cover the graphics page uses too), fetched once in the background and kept in
// %LOCALAPPDATA%\BlueAsh\covers\game-<n>.jpg. Until it arrives (or offline): the Steam library cover on this PC, then
// the icon of EscapeFromTarkov.exe, then the gamepad glyph.
namespace gameart {
Gdiplus::Bitmap* image[2]={};float anchor[2]={.5f,.5f};ULONGLONG triedAt[2]={};
// Loads a picture without keeping the file open (Steam rewrites its cache).
Gdiplus::Bitmap* load(const std::wstring& path){
    if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES){return nullptr;}Gdiplus::Bitmap file(path.c_str());
    if(file.GetLastStatus()!=Gdiplus::Ok||file.GetWidth()<16){return nullptr;}
    // Drawn into a bitmap of our own (Clone() can stay tied to the file object that goes away here).
    auto* copy=new Gdiplus::Bitmap((INT)file.GetWidth(),(INT)file.GetHeight(),PixelFormat32bppARGB);{Gdiplus::Graphics c(copy);c.DrawImage(&file,0,0,(INT)file.GetWidth(),(INT)file.GetHeight());}return copy;
}
std::wstring steamCover(const wchar_t* app){
    std::wstring steam=regString(L"Software\\Valve\\Steam",L"SteamPath");if(steam.empty()){return {};}for(auto& ch:steam)if(ch==L'/')ch=L'\\';
    std::wstring cache=steam+L"\\appcache\\librarycache\\";
    std::wstring flat=cache+app+L"_library_600x900.jpg";if(GetFileAttributesW(flat.c_str())!=INVALID_FILE_ATTRIBUTES){return flat;}
    // Newer Steam: librarycache\<app>\[<hash>\]library_600x900.jpg
    std::wstring dir=cache+app;std::wstring direct=dir+L"\\library_600x900.jpg";if(GetFileAttributesW(direct.c_str())!=INVALID_FILE_ATTRIBUTES){return direct;}
    WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((dir+L"\\*").c_str(),&f);std::wstring found;
    if(h!=INVALID_HANDLE_VALUE){do{if((f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&f.cFileName[0]!=L'.'){std::wstring p=dir+L"\\"+f.cFileName+L"\\library_600x900.jpg";if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES){found=p;break;}}}while(FindNextFileW(h,&f));FindClose(h);}
    return found;
}
Gdiplus::Bitmap* exeIcon(const std::wstring& exe){
    if(GetFileAttributesW(exe.c_str())==INVALID_FILE_ATTRIBUTES){return nullptr;}HICON icon=nullptr;UINT id=0;
    if(PrivateExtractIconsW(exe.c_str(),0,256,256,&icon,&id,1,0)!=1||!icon){return nullptr;}auto* b=painting::iconBitmap(icon);DestroyIcon(icon);return b;
}
std::wstring coverPath(int game){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring dir=std::wstring(d)+dataSub()+L"\\covers";CreateDirectoryW((std::wstring(d)+dataSub()).c_str(),nullptr);CreateDirectoryW(dir.c_str(),nullptr);return dir+L"\\game-"+std::to_wstring(game)+L".jpg";}
bool fetching[2]={};bool official[2]={};
// One background download per game per run (WinHTTP via urlmon, redirects to the image host followed); on success the
// cached picture replaces whatever was shown and the main window repaints.
void fetch(int game){if(fetching[game])return;fetching[game]=true;CreateThread(nullptr,0,[](LPVOID p)->DWORD{int game=(int)(INT_PTR)p;
        std::wstring url=std::wstring(L"https://")+updateHost+(game?L"/cover?name=PUBG%3A%20BATTLEGROUNDS&steam=578080":L"/cover?name=Escape%20from%20Tarkov&steam=3932890");
        std::wstring path=coverPath(game),tmp=path+L".part";
        if(SUCCEEDED(URLDownloadToFileW(nullptr,url.c_str(),tmp.c_str(),0,nullptr))){bool ok=false;{Gdiplus::Bitmap check(tmp.c_str());ok=check.GetLastStatus()==Gdiplus::Ok&&check.GetWidth()>=64;} /* closed before the move (Bitmap keeps the file open) */
            if(ok&&MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)){PostMessageW(win,WM_APP+233,(WPARAM)game,0);return 0;}}
        DeleteFileW(tmp.c_str());return 0;},(LPVOID)(INT_PTR)game,0,nullptr);}
// The download finished (main thread): drop what was shown, the next paint loads the cover.
void arrived(int game){if(game<0||game>1)return;delete image[game];image[game]=nullptr;official[game]=false;triedAt[game]=0;if(win)InvalidateRect(win,nullptr,FALSE);}
// game 0 = Tarkov, 1 = PUBG. Looked up once, again every minute while nothing is found.
Gdiplus::Bitmap* get(int game){
    if((image[game]&&official[game])||!painting::token){return image[game];}
    {std::wstring c=coverPath(game);if(auto* b=load(c)){delete image[game];image[game]=b;official[game]=true;anchor[game]=.3f;return b;}fetch(game);}
    if(image[game]){return image[game];}ULONGLONG now=GetTickCount64();if(triedAt[game]&&now-triedAt[game]<60000){return nullptr;}triedAt[game]=now;
    std::wstring cover=steamCover(game?L"578080":L"3932890");if(!cover.empty()&&(image[game]=load(cover))){anchor[game]=.28f;return image[game];}
    if(game==0){std::vector<std::wstring> exes;if(!raidWatch.folder.empty()){auto d=raidWatch.folder;while(!d.empty()&&(d.back()==L'\\'||d.back()==L'/'))d.pop_back();auto cut=d.find_last_of(L"\\/");if(cut!=std::wstring::npos)exes.push_back(d.substr(0,cut)+L"\\EscapeFromTarkov.exe");}
        exes.push_back(L"C:\\Battlestate Games\\Escape from Tarkov\\EscapeFromTarkov.exe");
        for(auto& e:exes)if((image[0]=exeIcon(e))){anchor[0]=.5f;break;}}
    return image[game];
}
void release(){for(auto& i:image){delete i;i=nullptr;}}
}
