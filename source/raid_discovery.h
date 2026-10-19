#pragma once
#include <tlhelp32.h>
namespace raidpaths {
inline bool directory(const std::wstring& p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
inline std::wstring parent(std::wstring p){auto i=p.find_last_of(L"\\/");return i==std::wstring::npos?L"":p.substr(0,i);}
inline std::wstring logs(std::wstring root){
    if(root.size()>1&&root.front()==L'"'&&root.back()==L'"')root=root.substr(1,root.size()-2);
    while(!root.empty()&&(root.back()==L'\\'||root.back()==L'/'))root.pop_back();
    if(root.empty())return L"";
    for(const auto& p:{root+L"\\Logs",root+L"\\build\\Logs"})if(directory(p))return p;
    return L"";
}
inline std::wstring registry(HKEY hive,const std::wstring& path,REGSAM view){
    HKEY key=nullptr;if(RegOpenKeyExW(hive,path.c_str(),0,KEY_QUERY_VALUE|view,&key)!=ERROR_SUCCESS)return L"";
    wchar_t value[32768]={};DWORD type=0,size=sizeof(value);
    LONG r=RegQueryValueExW(key,L"InstallLocation",nullptr,&type,(BYTE*)value,&size);RegCloseKey(key);
    if(r!=ERROR_SUCCESS||(type!=REG_SZ&&type!=REG_EXPAND_SZ)||size<sizeof(wchar_t))return L"";
    value[32767]=0;
    if(type==REG_EXPAND_SZ){wchar_t expanded[32768];DWORD n=ExpandEnvironmentStringsW(value,expanded,32768);if(!n||n>32768)return L"";return expanded;}
    return value;
}
inline std::wstring discover(){
    // Running game is the best evidence, including custom and moved installs.
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot!=INVALID_HANDLE_VALUE){
        PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);std::wstring found;
        if(Process32FirstW(snapshot,&entry))do{
            if(_wcsicmp(entry.szExeFile,L"EscapeFromTarkov.exe"))continue;
            HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID);
            if(!process)continue;
            wchar_t path[32768];DWORD n=32768;
            BOOL ok=QueryFullProcessImageNameW(process,0,path,&n);CloseHandle(process);
            if(ok){auto root=parent(std::wstring(path,n));found=logs(root);if(found.empty())found=logs(parent(root));}
            if(!found.empty())break;
        }while(Process32NextW(snapshot,&entry));
        CloseHandle(snapshot);if(!found.empty())return found;
    }
    // BSG and Steam uninstall entries, both registry views, no drive-wide scan.
    for(HKEY hive:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER})for(REGSAM view:{KEY_WOW64_64KEY,KEY_WOW64_32KEY})
        for(const wchar_t* name:{L"EscapeFromTarkov",L"Steam App 3932890"}){
            auto root=registry(hive,std::wstring(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\")+name,view);
            auto result=logs(root);if(!result.empty())return result;
        }
    return L"";
}
}
