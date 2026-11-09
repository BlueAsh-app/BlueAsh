#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
// BlueAsh's own files live in %LOCALAPPDATA%\BlueAsh. Up to 2.27 the folder was called ModeSwitch (the app's old name);
// the main app renames it once at start (datadir::relocate, after it is the only BlueAsh running). Every part of BlueAsh
// (this app, the C# helper, the updater, the setup program) follows one rule: BlueAsh if it exists, else ModeSwitch if it
// exists, else BlueAsh. So a rename that fails (a file held open) leaves everything on the old folder until next start.
// The registry key HKCU\Software\ModeSwitch keeps its name (nobody sees it; renaming it would lose settings).
namespace datadir {
inline std::wstring local(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);return d;}
inline bool isDir(const std::wstring& p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
inline std::wstring& name(){static std::wstring n=[]{std::wstring l=local();return std::wstring(isDir(l+L"\\BlueAsh")||!isDir(l+L"\\ModeSwitch")?L"BlueAsh":L"ModeSwitch");}();return n;}
// Moves what the new folder lacks (and files the old one has newer, e.g. an older updater's last progress line), then
// removes the old folders that end up empty. Links are not followed.
inline void merge(const std::wstring& from,const std::wstring& to){
    WIN32_FIND_DATAW f;HANDLE h=FindFirstFileW((from+L"\\*").c_str(),&f);if(h==INVALID_HANDLE_VALUE)return;
    do{std::wstring n=f.cFileName;if(n==L"."||n==L"..")continue;std::wstring a=from+L"\\"+n,b=to+L"\\"+n;
        bool dir=(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!(f.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT);
        DWORD there=GetFileAttributesW(b.c_str());
        if(there==INVALID_FILE_ATTRIBUTES){MoveFileExW(a.c_str(),b.c_str(),0);continue;}
        if(dir&&(there&FILE_ATTRIBUTE_DIRECTORY)){merge(a,b);RemoveDirectoryW(a.c_str());continue;}
        if(!dir&&!(there&FILE_ATTRIBUTE_DIRECTORY)){WIN32_FILE_ATTRIBUTE_DATA d{};if(GetFileAttributesExW(b.c_str(),GetFileExInfoStandard,&d)&&CompareFileTime(&f.ftLastWriteTime,&d.ftLastWriteTime)>0)MoveFileExW(a.c_str(),b.c_str(),MOVEFILE_REPLACE_EXISTING);}
    }while(FindNextFileW(h,&f));FindClose(h);}
inline void relocate(const std::wstring& l){std::wstring old=l+L"\\ModeSwitch",neu=l+L"\\BlueAsh";
    if(!isDir(old)){name()=L"BlueAsh";return;}
    if(!isDir(neu)){name()=MoveFileExW(old.c_str(),neu.c_str(),0)?L"BlueAsh":L"ModeSwitch";return;}
    merge(old,neu);RemoveDirectoryW(old.c_str());name()=L"BlueAsh";}
inline void relocate(){relocate(local());}
}
// "\BlueAsh" (or "\ModeSwitch" until the rename succeeds): appended to %LOCALAPPDATA%.
inline std::wstring dataSub(){return L"\\"+datadir::name();}
