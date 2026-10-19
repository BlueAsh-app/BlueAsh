#pragma once
#include "i18n.h"
#include <shlobj.h>
HANDLE companionProcess=nullptr,companionJob=nullptr;
bool companionEnabled(){return regString(configPath,L"CompanionEnabled")==L"1";}
std::wstring companionStateFile(){wchar_t dir[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,dir);std::wstring root=std::wstring(dir)+L"\\ModeSwitch";CreateDirectoryW(root.c_str(),nullptr);return root+L"\\raid-state.txt";}
void publishRaidContext(const raid::Context& c){
 // Keep the start time across duplicate log streams and later role/map updates. Old seven-line readers still work.
 static unsigned serial=0;static std::string phase;static ULONGLONG entered=0;
 if(c.phase!="started")entered=0;else if(phase!="started"||serial!=c.serial)entered=GetTickCount64();
 serial=c.serial;phase=c.phase;
 std::string out=std::to_string(GetCurrentProcessId())+"\n"+c.map+"\n"+c.role+"\n"+c.id+"\n"+c.phase+"\n"+std::to_string(c.serial)+"\n"+std::to_string(GetTickCount64())+"\n"+std::to_string(entered);
 auto path=companionStateFile(),temp=path+L".tmp";HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return;DWORD wrote=0;WriteFile(f,out.data(),(DWORD)out.size(),&wrote,nullptr);CloseHandle(f);MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);
}
void stopCompanion();void openCompanion(bool silent);
// Like the price worker: the map/Discord helper starts hidden when Tarkov runs (CompanionAuto, default on)
// and an auto-started one stops with the game unless the user enabled it permanently (CompanionEnabled).
bool companionAutoStarted=false;ULONGLONG nextCompanionCheck=0,companionRetryAfter=0;
// One process snapshot per 3 s, shared by the price worker and map helper checks (tarkovPid: that game process).
DWORD tarkovPid=0;
bool tarkovProcess(){
 static ULONGLONG next=0;static bool last=false;ULONGLONG now=GetTickCount64();if(now<next)return last;next=now+3000;
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return last=false;
 PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);bool found=false;
 if(Process32FirstW(snapshot,&entry))do{if(gameId(entry.szExeFile)==3){found=true;tarkovPid=entry.th32ProcessID;}}while(!found&&Process32NextW(snapshot,&entry));
 if(!found){tarkovPid=0;}CloseHandle(snapshot);return last=found;
}
// The helpers run without a window: what PowerShell prints when it fails goes to %LOCALAPPDATA%\ModeSwitch\<name>-error.log
// (rewritten at every start) for the diagnostics file; standard output is dropped.
std::wstring helperLogPath(const wchar_t* name){auto p=companionStateFile();return p.substr(0,p.find_last_of(L"\\/"))+L"\\"+name+L"-error.log";}
struct HelperIo {
 HANDLE err=INVALID_HANDLE_VALUE,out=INVALID_HANDLE_VALUE;
 HelperIo(const wchar_t* name,STARTUPINFOW& si){SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};
  err=CreateFileW(helperLogPath(name).c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);out=CreateFileW(L"NUL",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
  if(ok()){si.dwFlags|=STARTF_USESTDHANDLES;si.hStdError=err;si.hStdOutput=out;si.hStdInput=nullptr;}}
 bool ok()const{return err!=INVALID_HANDLE_VALUE&&out!=INVALID_HANDLE_VALUE;}
 ~HelperIo(){if(err!=INVALID_HANDLE_VALUE)CloseHandle(err);if(out!=INVALID_HANDLE_VALUE)CloseHandle(out);}
};
void pollCompanion(){
 ULONGLONG now=GetTickCount64();if(now<nextCompanionCheck)return;nextCompanionCheck=now+3000;
 bool running=companionProcess&&WaitForSingleObject(companionProcess,0)==WAIT_TIMEOUT;
 if(!running&&companionProcess){DWORD code=0;if(GetExitCodeProcess(companionProcess,&code)&&code!=0)reportProblem(L"map-crash");}
 if(companionAutoStarted&&!running&&companionProcess){companionAutoStarted=false;companionRetryAfter=now+60000;}
 if(tarkovProcess()){if(!running&&now>=companionRetryAfter&&regString(configPath,L"CompanionAuto")!=L"0"){openCompanion(true);companionAutoStarted=companionProcess!=nullptr;}}
 else if(running&&companionAutoStarted&&!companionEnabled()){stopCompanion();companionAutoStarted=false;}
}
void stopCompanion(){if(companionJob){TerminateJobObject(companionJob,0);CloseHandle(companionJob);companionJob=nullptr;}if(companionProcess){CloseHandle(companionProcess);companionProcess=nullptr;}}
// The helper opens a window per menu entry: "map@<hwnd>" / "discord@<hwnd>" (pages inside the main window), "hideout" (its own window).
// The request goes through companion-page.txt, read at start-up and polled while running.
void openCompanionPage(const wchar_t* page){
 auto path=companionStateFile();path=path.substr(0,path.find_last_of(L"\\/"))+L"\\companion-page.txt";
 std::wstring w=page;std::string text(w.begin(),w.end());
 HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE){DWORD wrote=0;WriteFile(f,text.data(),(DWORD)text.size(),&wrote,nullptr);CloseHandle(f);}
 openCompanion(true);
}
void openCompanion(bool silent=false){
 if(companionProcess&&WaitForSingleObject(companionProcess,0)==WAIT_TIMEOUT)return;
 stopCompanion();
 wchar_t system[MAX_PATH]={};GetSystemDirectoryW(system,MAX_PATH);std::wstring root=bundleRoot();if(root.empty())return;
 auto script=root+L"\\companion\\Companion.ps1";if(GetFileAttributesW(script.c_str())==INVALID_FILE_ATTRIBUTES){problem(i18n::text(L"내장 도우미 파일을 찾지 못했습니다."));return;}
 auto shell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
 auto args=L"\""+shell+L"\" -NoProfile -STA -ExecutionPolicy Bypass -File \""+script+L"\" -ParentId "+std::to_wstring(GetCurrentProcessId())+(silent?L" -Hidden":L"");
 companionJob=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
 if(!companionJob||!SetInformationJobObject(companionJob,JobObjectExtendedLimitInformation,&limits,sizeof(limits))){stopCompanion();problem(i18n::text(L"도우미 프로세스 보호 설정 실패"));return;}
 STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};HelperIo io(L"companion",si);
 if(!CreateProcessW(shell.c_str(),&args[0],nullptr,nullptr,io.ok(),CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,root.c_str(),&si,&pi)){stopCompanion();problem(i18n::text(L"도우미 시작 실패"));return;}
 companionProcess=pi.hProcess;if(!AssignProcessToJobObject(companionJob,pi.hProcess)){TerminateProcess(pi.hProcess,1);CloseHandle(pi.hThread);stopCompanion();return;}ResumeThread(pi.hThread);CloseHandle(pi.hThread);
}
