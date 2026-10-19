#pragma once
// Support for users who report "the map / price does nothing":
// - Tarkov started as administrator: Windows then hides its keyboard and mouse state from a normal BlueAsh (UIPI), so
//   the map key and the inspect click never register. Detected per game process; the notice restarts BlueAsh elevated.
// - A diagnostics file (Desktop\BlueAsh-diagnostics.txt) with what the helpers saw, to send to whoever helps.
bool processElevated(HANDLE p){
    HANDLE t=nullptr;if(!OpenProcessToken(p,TOKEN_QUERY,&t))return GetLastError()==ERROR_ACCESS_DENIED; // a higher process refuses us
    TOKEN_ELEVATION e={};DWORD n=0;bool r=GetTokenInformation(t,TokenElevation,&e,sizeof(e),&n)&&e.TokenIsElevated;CloseHandle(t);return r;
}
bool selfElevated(){static int v=-1;if(v<0)v=processElevated(GetCurrentProcess())?1:0;return v==1;}
// -1 not running, 0 normal, 1 administrator; worked out once per game process.
int tarkovAdmin(){
    static DWORD pid=0;static int v=-1;tarkovProcess();if(!tarkovPid){pid=0;return v=-1;}if(pid==tarkovPid)return v;pid=tarkovPid;
    HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return v=GetLastError()==ERROR_ACCESS_DENIED?1:-1;
    v=processElevated(p)?1:0;CloseHandle(p);return v;
}
bool adminMismatch(){return !selfElevated()&&tarkovAdmin()==1;}
// Switching follows the game: when Tarkov runs as administrator, BlueAsh asks to restart as administrator, every time
// through the Windows prompt (the user approves each one). The new copy is started with --takeover: it asks the running
// one to close (WM_APP+177) and waits for it.
// Until 2.21.7 the elevated copy also registered a scheduled task (\BlueAsh\Elevated, highest privileges) so later
// switches needed no prompt. Anything able to replace BlueAsh.exe in its user-writable folder could then run as
// administrator through that task, so it is no longer created and an old one is removed (removeElevatedTask).
ULONGLONG balloonAdminAt=0;
const wchar_t* elevatedTask=L"BlueAsh\\Elevated";
DWORD runQuiet(std::wstring line){
    STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};
    if(!CreateProcessW(nullptr,&line[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return (DWORD)-1;
    WaitForSingleObject(pi.hProcess,15000);DWORD code=(DWORD)-1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code;
}
std::wstring schtasks(){wchar_t system[MAX_PATH]={};GetSystemDirectoryW(system,MAX_PATH);return L"\""+std::wstring(system)+L"\\schtasks.exe\"";}
// Removes the task an older BlueAsh registered. It was created elevated, so deleting it may need administrator rights:
// it is tried at every start (only while the registry still names it) and works at the latest when BlueAsh next runs
// as administrator, or when BlueAsh is uninstalled (installer/BlueAsh.iss).
void removeElevatedTask(){
    if(regString(configPath,L"ElevatedTaskPath").empty())return;
    bool gone=runQuiet(schtasks()+L" /Query /TN \""+elevatedTask+L"\"")!=0||runQuiet(schtasks()+L" /Delete /F /TN \""+elevatedTask+L"\"")==0;
    if(gone){HKEY k=nullptr;if(RegOpenKeyExW(HKEY_CURRENT_USER,configPath,0,KEY_SET_VALUE,&k)==ERROR_SUCCESS){RegDeleteValueW(k,L"ElevatedTaskPath");RegCloseKey(k);}}
}
// Start an elevated copy that takes over, through the Windows prompt.
bool elevateWithPrompt(bool hidden){
    std::wstring exe=executable(),args=hidden?L"--takeover --tray":L"--takeover";
    SHELLEXECUTEINFOW s={};s.cbSize=sizeof(s);s.lpVerb=L"runas";s.lpFile=exe.c_str();s.lpParameters=args.c_str();s.nShow=SW_SHOWNORMAL;
    return ShellExecuteExW(&s)!=FALSE; // FALSE: declined at the prompt, keep running as is
}
void restartAsAdmin(){elevateWithPrompt(false);}
void pollAdmin(){
    static DWORD tried=0;if(!adminMismatch()||tried==tarkovPid)return;tried=tarkovPid;reportProblem(L"tarkov-admin");
    if(elevateWithPrompt(true))return;
    balloonAdminAt=GetTickCount64();reportProblem(L"admin-cancel");
    balloon::show(i18n::text(L"관리자 권한 전환이 취소됐습니다"),i18n::text(L"타르코프가 관리자 권한으로 실행 중이라 지도와 시세가 동작하지 않습니다. 이 알림을 누르면 다시 시도합니다."),L"");
    if(mainPage==9)InvalidateRect(win,nullptr,FALSE);
}
std::wstring fileTail(const std::wstring& path,size_t limit){
    HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return L"(none)";
    LARGE_INTEGER size={};GetFileSizeEx(h,&size);LARGE_INTEGER at={};at.QuadPart=size.QuadPart>(LONGLONG)limit?size.QuadPart-(LONGLONG)limit:0;SetFilePointerEx(h,at,nullptr,FILE_BEGIN);
    std::string b((size_t)(size.QuadPart-at.QuadPart),'\0');DWORD n=0;if(!b.empty())ReadFile(h,&b[0],(DWORD)b.size(),&n,nullptr);CloseHandle(h);b.resize(n);
    if(b.size()>=3&&(unsigned char)b[0]==0xEF){b.erase(0,3);}if(b.empty())return L"(empty)";
    int w=MultiByteToWideChar(CP_UTF8,0,b.data(),(int)b.size(),nullptr,0);std::wstring s(w,L'\0');MultiByteToWideChar(CP_UTF8,0,b.data(),(int)b.size(),&s[0],w);return s;
}
std::wstring tarkovSetting(const wchar_t* file,const wchar_t* key){
    wchar_t roaming[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,roaming);
    std::wstring text=fileTail(std::wstring(roaming)+L"\\Battlestate Games\\Escape from Tarkov\\Settings\\"+file,262144);
    size_t at=text.find(key);if(at==std::wstring::npos)return text==L"(none)"?L"(no file)":L"(not found)";size_t end=text.find_first_of(L",\r\n}",at);return text.substr(at,end==std::wstring::npos?std::wstring::npos:end-at);
}
// Labels are English on purpose: the file is for whoever helps, whatever the UI language.
std::wstring diagnosticsText(){
    std::wstring s=L"BlueAsh diagnostics\r\n";auto add=[&](const std::wstring& k,const std::wstring& v){s+=k+L": "+v+L"\r\n";};
    SYSTEMTIME now;GetLocalTime(&now);wchar_t stamp[64];swprintf(stamp,64,L"%04u-%02u-%02u %02u:%02u:%02u",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);add(L"Time",stamp);
    add(L"Version",appVersion);add(L"Language",i18n::languageCode());
    typedef LONG(WINAPI*RtlGetVersionFn)(OSVERSIONINFOW*);OSVERSIONINFOW os={};os.dwOSVersionInfoSize=sizeof(os);FARPROC f=GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"RtlGetVersion");
    RtlGetVersionFn rtl=nullptr;memcpy(&rtl,&f,sizeof(rtl));if(rtl)rtl(&os);add(L"Windows",std::to_wstring(os.dwMajorVersion)+L"."+std::to_wstring(os.dwMinorVersion)+L"."+std::to_wstring(os.dwBuildNumber));
    add(L"BlueAsh path",executable());add(L"BlueAsh as administrator",selfElevated()?L"yes":L"no");
    int a=tarkovAdmin();add(L"Tarkov",a<0?L"not running":a?L"running AS ADMINISTRATOR":L"running");
    if(tarkovPid){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,tarkovPid);wchar_t path[1024]={};DWORD n=1024;if(p){if(!QueryFullProcessImageNameW(p,0,path,&n))path[0]=0;CloseHandle(p);}add(L"Tarkov path",path[0]?path:L"(hidden)");}
    add(L"Game logs",raidWatch.folder.empty()?L"(not found)":raidWatch.folder+(GetFileAttributesW(raidWatch.folder.c_str())==INVALID_FILE_ATTRIBUTES?L" (missing)":L""));
    add(L"Graphics.ini",tarkovSetting(L"Graphics.ini",L"\"FullScreenMode\""));add(L"Control.ini map key",tarkovSetting(L"Control.ini",L"\"ToggleMap\""));
    bool helper=companionProcess&&WaitForSingleObject(companionProcess,0)==WAIT_TIMEOUT;
    add(L"Map switch",regString(configPath,L"MapEnabled")==L"0"?L"off":L"on");add(L"Map helper",helper?L"running":L"stopped");
    add(L"Price switch",priceWatch.autoMode?L"on":L"off");add(L"Price worker",std::wstring(priceWatch.running()?L"running":L"stopped")+L" · "+priceWatch.message);
    std::wstring store=helperLogPath(L"price");store=store.substr(0,store.find_last_of(L"\\/"));
    s+=L"\r\n--- price-status.txt ---\r\n"+fileTail(store+L"\\price-status.txt",4000);
    s+=L"\r\n\r\n--- price-error.log ---\r\n"+fileTail(helperLogPath(L"price"),6000);
    s+=L"\r\n\r\n--- companion-error.log ---\r\n"+fileTail(helperLogPath(L"companion"),6000);
    s+=L"\r\n\r\n--- raid-state.txt ---\r\n"+fileTail(store+L"\\raid-state.txt",1000)+L"\r\n";
    return s;
}
void writeDiagnostics(){
    std::wstring s=diagnosticsText();
    wchar_t desk[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_DESKTOPDIRECTORY,nullptr,SHGFP_TYPE_CURRENT,desk);std::wstring out=std::wstring(desk)+L"\\BlueAsh-diagnostics.txt";
    int bytes=WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),nullptr,0,nullptr,nullptr);std::string u8="\xEF\xBB\xBF";u8.resize(3+bytes);WideCharToMultiByte(CP_UTF8,0,s.c_str(),(int)s.size(),&u8[3],bytes,nullptr,nullptr);
    HANDLE h=CreateFileW(out.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE){problem(i18n::text(L"진단 파일을 만들지 못했습니다."));return;}
    DWORD wrote=0;WriteFile(h,u8.data(),(DWORD)u8.size(),&wrote,nullptr);CloseHandle(h);
    std::wstring select=L"/select,\""+out+L"\"";ShellExecuteW(nullptr,L"open",L"explorer.exe",select.c_str(),nullptr,SW_SHOWNORMAL);
}
// Release check (source/release.ps1 runs "BlueAsh.exe --selftest <dir>" before publishing): unpack the bundle into <dir>,
// then PowerShell parses every script and loads both DLLs. Exit code 0 = fine; problems go to <dir>\selftest.txt.
std::wstring base64(const std::string& in){
    static const char* abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::wstring out;
    for(size_t i=0;i<in.size();i+=3){unsigned v=(unsigned char)in[i]<<16;if(i+1<in.size())v|=(unsigned char)in[i+1]<<8;if(i+2<in.size())v|=(unsigned char)in[i+2];
        out+=abc[(v>>18)&63];out+=abc[(v>>12)&63];out+=i+1<in.size()?abc[(v>>6)&63]:'=';out+=i+2<in.size()?abc[v&63]:'=';}
    return out;
}
int selfTest(std::wstring dir){
    while(!dir.empty()&&(dir.back()==L'"'||dir.back()==L' ')){dir.pop_back();}while(!dir.empty()&&dir[0]==L'"'){dir.erase(0,1);}
    CreateDirectoryW(dir.c_str(),nullptr);bundleBase=dir;std::wstring root=bundleRoot();if(root.empty())return 2;
    std::wstring ps=L"$r='"+root+L"';$o='"+dir+L"\\selftest.txt';$bad=@();"
        L"foreach($f in Get-ChildItem $r -Recurse -Filter *.ps1){$e=$null;[void][Management.Automation.Language.Parser]::ParseFile($f.FullName,[ref]$null,[ref]$e);if($e.Count){$bad+=$f.Name+': '+$e[0].Message}};"
        L"foreach($d in 'companion\\CompanionUI.dll','price\\PriceHelper.dll'){try{Add-Type -Path (Join-Path $r $d) -ErrorAction Stop}catch{$bad+=$d+': '+$_.Exception.Message}};"
        L"if(!(Test-Path (Join-Path $r 'companion\\Localization.tsv'))){$bad+='Localization.tsv missing'};"
        L"if($bad.Count){$bad|Set-Content -Encoding UTF8 $o;exit 1};'ok'|Set-Content $o;exit 0";
    std::string utf16((const char*)ps.data(),ps.size()*sizeof(wchar_t)); // -EncodedCommand takes UTF-16LE in base64
    wchar_t system[MAX_PATH]={};GetSystemDirectoryW(system,MAX_PATH);std::wstring shell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    std::wstring line=L"\""+shell+L"\" -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand "+base64(utf16);
    STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};
    if(!CreateProcessW(shell.c_str(),&line[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return 3;
    WaitForSingleObject(pi.hProcess,120000);DWORD code=4;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return (int)code;
}
