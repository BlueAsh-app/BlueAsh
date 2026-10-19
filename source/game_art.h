#pragma once
// Game pictures for the dashboard rows and the game cards, taken from what is already on this PC: nothing is
// bundled or downloaded (the art belongs to the publishers). PUBG and Tarkov on Steam: the store cover Steam keeps
// in its library cache. Tarkov from the BSG launcher: the icon of EscapeFromTarkov.exe. None found: the gamepad glyph.
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
// game 0 = Tarkov, 1 = PUBG. Looked up once, again every minute while nothing is found.
Gdiplus::Bitmap* get(int game){
    if(image[game]||!painting::token){return image[game];}ULONGLONG now=GetTickCount64();if(triedAt[game]&&now-triedAt[game]<60000){return nullptr;}triedAt[game]=now;
    std::wstring cover=steamCover(game?L"578080":L"3932890");if(!cover.empty()&&(image[game]=load(cover))){anchor[game]=.28f;return image[game];}
    if(game==0){std::vector<std::wstring> exes;if(!raidWatch.folder.empty()){auto d=raidWatch.folder;while(!d.empty()&&(d.back()==L'\\'||d.back()==L'/'))d.pop_back();auto cut=d.find_last_of(L"\\/");if(cut!=std::wstring::npos)exes.push_back(d.substr(0,cut)+L"\\EscapeFromTarkov.exe");}
        exes.push_back(L"C:\\Battlestate Games\\Escape from Tarkov\\EscapeFromTarkov.exe");
        for(auto& e:exes)if((image[0]=exeIcon(e))){anchor[0]=.5f;break;}}
    return image[game];
}
void release(){for(auto& i:image){delete i;i=nullptr;}}
}
