// The old data folder (ModeSwitch) becomes BlueAsh: renamed whole, merged when both exist, kept while a file is held open.
#include "data_dir.h"
#include <cassert>
#include <cstdio>
static void put(const std::wstring& p,const char* text){FILE* f=_wfopen(p.c_str(),L"wb");fputs(text,f);fclose(f);}
static std::string get(const std::wstring& p){FILE* f=_wfopen(p.c_str(),L"rb");if(!f)return "";char b[64]={};size_t n=fread(b,1,63,f);fclose(f);return std::string(b,n);}
static void makeOld(const std::wstring& p){HANDLE h=CreateFileW(p.c_str(),FILE_WRITE_ATTRIBUTES,0,nullptr,OPEN_EXISTING,0,nullptr);FILETIME ft={0,0x01000000};SetFileTime(h,nullptr,nullptr,&ft);CloseHandle(h);}
int main(){
    wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);std::wstring base=std::wstring(t)+L"blueash-datadir-test-"+std::to_wstring(GetCurrentProcessId());
    CreateDirectoryW(base.c_str(),nullptr);std::wstring old=base+L"\\ModeSwitch",neu=base+L"\\BlueAsh";
    // 1. only the old folder: renamed whole
    CreateDirectoryW(old.c_str(),nullptr);CreateDirectoryW((old+L"\\dlss").c_str(),nullptr);put(old+L"\\a.txt","settings");put(old+L"\\dlss\\x.txt","x");
    datadir::relocate(base);assert(datadir::name()==L"BlueAsh");assert(!datadir::isDir(old));
    assert(get(neu+L"\\a.txt")=="settings"&&get(neu+L"\\dlss\\x.txt")=="x");
    // 2. both (an older updater wrote to the old folder after the rename): missing and newer files move in
    CreateDirectoryW(old.c_str(),nullptr);CreateDirectoryW((old+L"\\dlss").c_str(),nullptr);put(old+L"\\progress.txt","done");put(old+L"\\dlss\\y.txt","y");
    put(old+L"\\a.txt","older");makeOld(old+L"\\a.txt");
    datadir::relocate(base);assert(datadir::name()==L"BlueAsh");
    assert(get(neu+L"\\progress.txt")=="done"&&get(neu+L"\\dlss\\y.txt")=="y"&&get(neu+L"\\a.txt")=="settings");
    // the older copy stays behind with its folder (nothing is deleted); everything else moved
    assert(get(old+L"\\a.txt")=="older"&&!datadir::isDir(old+L"\\dlss"));DeleteFileW((old+L"\\a.txt").c_str());RemoveDirectoryW(old.c_str());
    // 3. only the old folder, a file held open: the rename fails and this run stays on the old folder
    assert(MoveFileExW(neu.c_str(),old.c_str(),0));HANDLE held=CreateFileW((old+L"\\a.txt").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    datadir::relocate(base);assert(datadir::name()==L"ModeSwitch");assert(datadir::isDir(old)&&!datadir::isDir(neu));CloseHandle(held);
    // next start: renamed
    datadir::relocate(base);assert(datadir::name()==L"BlueAsh"&&!datadir::isDir(old)&&get(neu+L"\\a.txt")=="settings");
    // 4. a new install (neither folder): BlueAsh
    std::wstring empty=base+L"\\empty";CreateDirectoryW(empty.c_str(),nullptr);datadir::relocate(empty);assert(datadir::name()==L"BlueAsh");
    std::printf("data dir tests passed\n");return 0;}
