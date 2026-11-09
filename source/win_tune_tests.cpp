// win_tune.h: history round trip and crash marks, startup approval bytes, refresh-rate choice, the Run command parser,
// the clean sweep (age, read-only, links never followed) on a scratch folder, and the approval value written and put
// back under a test key. Nothing on the real PC is changed.
#include "win_tune.h"
#include <cassert>
#include <cstdio>
using namespace wintune;
static std::wstring scratch(){wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);wchar_t n[64];swprintf(n,64,L"blueash-tune-%lu",GetCurrentProcessId());std::wstring d=std::wstring(t)+n;CreateDirectoryW(d.c_str(),nullptr);return d;}
static void make(const std::wstring& p,int bytes,int hoursAgo,DWORD attr=0){HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);assert(h!=INVALID_HANDLE_VALUE);
    std::string s(bytes,'x');DWORD w=0;WriteFile(h,s.data(),(DWORD)s.size(),&w,nullptr);FILETIME now;GetSystemTimeAsFileTime(&now);ULARGE_INTEGER u;u.LowPart=now.dwLowDateTime;u.HighPart=now.dwHighDateTime;u.QuadPart-=(ULONGLONG)hoursAgo*36000000000ULL;
    FILETIME t={u.LowPart,u.HighPart};SetFileTime(h,&t,&t,&t);CloseHandle(h);if(attr)SetFileAttributesW(p.c_str(),attr);}
int main(){
    // History: entries survive a reload, a pending change can be marked applied, restored and dropped.
    std::wstring dir=scratch();History hs;hs.path=dir+L"\\h.tsv";
    std::wstring a=hs.add({L"",L"",L"power",L"{x}",L"{old}",L"{new}",L"균형 → 고성능\twith tab",L"pending"});assert(!a.empty());
    std::wstring b=hs.add({L"",L"",L"clean",L"temp",L"",L"12",L"임시 파일",L"done"});assert(!b.empty()&&a!=b);
    auto all=hs.load();assert(all.size()==2&&all[0].kind==L"power"&&all[0].label==L"균형 → 고성능 with tab"&&all[0].state==L"pending");
    assert(hs.mark(a,L"applied")&&hs.load()[0].state==L"applied");assert(hs.mark(a,L"restored"));assert(hs.drop(b)&&hs.load().size()==1);assert(!hs.mark(L"nope",L"x"));
    for(int i=0;i<310;i++){hs.add({L"",L"",L"mode",L"",L"",L"",L"n",L"done"});}assert(hs.load().size()==300);
    // Startup approval: Task Manager's bytes.
    assert(approvedOn({})&&approvedOn({2,0,0,0,0,0,0,0,0,0,0,0})&&!approvedOn({3,0,0,0,1,2,3,4,5,6,7,8})&&approvedOn({6,0,0,0}));
    FILETIME ft={0x11223344,0x55667788};auto off=approvedBytes(false,ft);assert(off.size()==12&&off[0]==3&&off[4]==0x44&&off[11]==0x55);auto on=approvedBytes(true,ft);assert(on[0]==2&&on[4]==0);
    std::vector<BYTE> back;assert(unhex(hex(off),back)&&back==off);assert(!unhex(L"abc",back));
    // Refresh rate: the highest at the same resolution and depth, interlaced and other resolutions ignored.
    Mode cur{2560,1440,32,60};std::vector<Mode> modes={{2560,1440,32,60},{2560,1440,32,144},{2560,1440,32,165},{2560,1440,32,240,true},{1920,1080,32,240},{2560,1440,16,200}};
    assert(bestHz(cur,modes)==165);Mode top{2560,1440,32,165};assert(bestHz(top,modes)==165);
    // Run commands.
    assert(exeOf(L"\"C:\\Program Files\\Steam\\steam.exe\" -silent")==L"C:\\Program Files\\Steam\\steam.exe");
    assert(exeOf(L"C:\\Tools\\app.exe /min")==L"C:\\Tools\\app.exe");assert(exeOf(L"C:\\Tools\\noext /x")==L"C:\\Tools\\noext");
    assert(securityItem(L"SecurityHealth",L"C:\\Windows\\system32\\SecurityHealthSystray.exe",L"Microsoft Corporation"));assert(!securityItem(L"Steam",L"steam.exe",L"Valve"));assert(securityItem(L"Riot Vanguard",L"C:\\Program Files\\Riot Vanguard\\vgtray.exe",L"Riot Games, Inc."));
    {int g=gpuScheduling(),m=memoryIntegrity();assert(g>=-1&&g<=1&&m>=-1&&m<=1);printf("gpu scheduling %d, memory integrity %d\n",g,m);}
    assert(std::wstring(kindOf(L"chrome.exe"))==L"browser"&&std::wstring(kindOf(L"Discord.exe"))==L"overlay"&&std::wstring(kindOf(L"game.exe")).empty());
    assert(systemProc(L"svchost.exe")&&!systemProc(L"steam.exe"));
    // Clean: only files older than the limit, read-only and young files kept, nested old files found.
    std::wstring c=dir+L"\\clean";CreateDirectoryW(c.c_str(),nullptr);CreateDirectoryW((c+L"\\sub").c_str(),nullptr);
    make(c+L"\\old.tmp",1000,48);make(c+L"\\new.tmp",500,1);make(c+L"\\sub\\old2.tmp",300,30);make(c+L"\\ro.tmp",200,48,FILE_ATTRIBUTE_READONLY);
    Swept count;sweep(c,24,false,count);assert(count.files==3&&count.bytes==1500);
    Swept done;sweep(c,24,true,done);assert(done.files==2&&done.bytes==1300&&done.skipped==1);
    assert(GetFileAttributesW((c+L"\\new.tmp").c_str())!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW((c+L"\\old.tmp").c_str())==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW((c+L"\\ro.tmp").c_str())!=INVALID_FILE_ATTRIBUTES);
    // A junction inside the folder is not followed: the file behind it survives.
    std::wstring keep=dir+L"\\keep";CreateDirectoryW(keep.c_str(),nullptr);make(keep+L"\\precious.txt",100,72);
    std::wstring cmd=L"cmd /c mklink /J \""+c+L"\\link\" \""+keep+L"\" >nul";if(_wsystem(cmd.c_str())==0){Swept j;sweep(c,24,true,j);assert(GetFileAttributesW((keep+L"\\precious.txt").c_str())!=INVALID_FILE_ATTRIBUTES);}
    // Approval value written and put back exactly (test key only).
    std::wstring key=L"Software\\BlueAshTuneTest\\Approved";assert(writeBinary(HKEY_CURRENT_USER,key,L"App",off));std::vector<BYTE> got;assert(readBinary(HKEY_CURRENT_USER,key,L"App",got)&&got==off&&!approvedOn(got));
    assert(removeValue(HKEY_CURRENT_USER,key,L"App")&&!readBinary(HKEY_CURRENT_USER,key,L"App",got));assert(removeValue(HKEY_CURRENT_USER,key,L"App"));RegDeleteTreeW(HKEY_CURRENT_USER,L"Software\\BlueAshTuneTest");
    // Read-only queries answer without changing anything.
    (void)plans();(void)monitors();(void)gpuScheduling();(void)memoryIntegrity();double fg=0,fp=0;(void)systemDriveFree(fg,fp);(void)startups();auto s1=snapshot();Sleep(200);auto s2=snapshot();(void)compare(s1,s2,GetCurrentProcessId());
    // Leave nothing behind.
    SetFileAttributesW((c+L"\\ro.tmp").c_str(),FILE_ATTRIBUTE_NORMAL);_wsystem((L"cmd /c rmdir /s /q \""+dir+L"\" >nul 2>nul").c_str());
    printf("win_tune: ok\n");return 0;}
