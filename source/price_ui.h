#pragma once
#include "i18n.h"
// Optional OCR worker exists only while the user has enabled price recognition.
// Keep all children in a kill-on-close job: OFF and parent exit release OCR memory.
struct PriceWatch {
 HANDLE job=nullptr,process=nullptr;HWND dialog=nullptr;
 unsigned key=VK_ADD,mods=0;bool shortcut=false,capturing=false,autoMode=true,manualOff=false;ULONGLONG ignoreUntil=0,nextGameCheck=0,retryAfter=0,statusSince=0;
 std::wstring message=i18n::text(L"꺼짐 · 타르코프를 켜면 자동으로 시작합니다.");
 bool running(){return process&&WaitForSingleObject(process,0)==WAIT_TIMEOUT;}
 // OCR languages the worker found missing (ocr-missing.txt: capability name TAB tarkov.dev locale per line):
 // the game's text language, and English for the Latin model names inside titles. One click installs them through
 // Add-WindowsCapability (Windows asks for permission); the worker restarts afterwards and picks them up.
 std::wstring ocrMissing,ocrNotified;HANDLE ocrInstall=nullptr;ULONGLONG nextOcrCheck=0,balloonOcrAt=0;
 static std::wstring storeFile(const wchar_t* name){wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local)))return L"";return std::wstring(local)+L"\\ModeSwitch\\"+name;}
 void readOcrMissing(){std::wstring s;HANDLE h=CreateFileW(storeFile(L"ocr-missing.txt").c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
  if(h!=INVALID_HANDLE_VALUE){char data[2048];DWORD n=0;if(ReadFile(h,data,sizeof(data),&n,nullptr)&&n){int size=MultiByteToWideChar(CP_UTF8,0,data,n,nullptr,0);if(size>0){s.assign(size,0);MultiByteToWideChar(CP_UTF8,0,data,n,&s[0],size);}}CloseHandle(h);}
  while(!s.empty()&&(s.back()==L'\r'||s.back()==L'\n'||s.back()==L' '))s.pop_back();
  if(s==ocrMissing){return;}ocrMissing=s;if(dialog)ShowWindow(GetDlgItem(dialog,196),ocrMissing.empty()?SW_HIDE:SW_SHOW);
  if(!ocrMissing.empty()&&ocrMissing!=ocrNotified&&!ocrInstall){ocrNotified=ocrMissing;balloonOcrAt=GetTickCount64();
   balloon::show(i18n::text(L"글자 인식 언어 설치"),i18n::text(L"아이템 이름을 더 잘 읽으려면 Windows 글자 인식(OCR) 언어가 필요합니다. 이 알림을 누르면 설치합니다."),L"");}}
 void installOcr(){if(ocrInstall||ocrMissing.empty())return;std::wstring cmd;size_t at=0;
  while(at<ocrMissing.size()){size_t end=ocrMissing.find(L'\n',at);if(end==std::wstring::npos)end=ocrMissing.size();std::wstring line=ocrMissing.substr(at,end-at);at=end+1;
   size_t tab=line.find(L'\t');std::wstring name=line.substr(0,tab);while(!name.empty()&&(name.back()==L'\r'||name.back()==L' '))name.pop_back();
   bool safe=!name.empty();for(wchar_t c:name)if(!(iswalnum(c)||c==L'.'||c==L'~'||c==L'-'))safe=false;if(safe)cmd+=L"Add-WindowsCapability -Online -Name '"+name+L"' | Out-Null;";}
  if(cmd.empty()){return;}wchar_t system[MAX_PATH]={};GetSystemDirectoryW(system,MAX_PATH);std::wstring shell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
  std::wstring args=L"-NoProfile -NonInteractive -WindowStyle Hidden -Command \"try{"+cmd+L"exit 0}catch{exit 1}\"";
  SHELLEXECUTEINFOW e={};e.cbSize=sizeof(e);e.fMask=SEE_MASK_NOCLOSEPROCESS;e.hwnd=win;e.lpVerb=L"runas";e.lpFile=shell.c_str();e.lpParameters=args.c_str();e.nShow=SW_HIDE;
  if(!ShellExecuteExW(&e)||!e.hProcess){message=i18n::text(L"글자 인식 언어 설치를 취소했습니다.");render();return;}
  ocrInstall=e.hProcess;message=i18n::text(L"글자 인식 언어를 설치하는 중입니다. 몇 분 걸릴 수 있습니다.");render();}
 void pollOcrInstall(){if(!ocrInstall||WaitForSingleObject(ocrInstall,0)!=WAIT_OBJECT_0)return;DWORD code=1;GetExitCodeProcess(ocrInstall,&code);CloseHandle(ocrInstall);ocrInstall=nullptr;
  if(code==0){balloon::show(i18n::text(L"글자 인식 언어 설치 완료"),i18n::text(L"이제 아이템 이름을 더 잘 읽습니다."),L"");bool was=running();stop();retryAfter=0;if(was||tarkovRunning())start();}
  else{message=i18n::text(L"글자 인식 언어를 설치하지 못했습니다. Windows 설정 > 시간 및 언어에서 언어를 추가해 주세요.");reportProblem(L"ocr-install");render();}}
 std::wstring keyName(){auto k=hotkeyVk,m=hotkeyMods;hotkeyVk=key;hotkeyMods=mods;auto name=hotkeyName();hotkeyVk=k;hotkeyMods=m;return name;}
 void registerKey(){UnregisterHotKey(win,2);shortcut=RegisterHotKey(win,2,mods|MOD_NOREPEAT,key)!=FALSE;if(!shortcut)message=i18n::text(L"시세 단축키 등록 실패 · 다른 키를 선택하세요.");}
 void load(){unsigned k,m;auto binding=regString(configPath,L"PriceHotkeyV2");auto comma=binding.find(L',');
  if(comma!=std::wstring::npos&&settings::integer(binding.substr(0,comma),m)&&settings::integer(binding.substr(comma+1),k)&&m<=15&&k>=8&&k<=254&&k!=VK_F12){key=k;mods=m;}
  autoMode=regString(configPath,L"PriceAuto")!=L"0";registerKey();
 }
 bool bind(unsigned newKey,unsigned newMods){
  if(newKey<8||newKey>254||newKey==VK_F12){message=i18n::text(L"이 키는 지정할 수 없습니다. 다른 키를 누르세요.");render();return false;}
  UnregisterHotKey(win,2);
  if(!RegisterHotKey(win,2,newMods|MOD_NOREPEAT,newKey)){registerKey();message=i18n::text(L"다른 프로그램이 쓰는 키입니다. 지금 키를 그대로 두었습니다.");render();return false;}
  if(!saveString(configPath,L"PriceHotkeyV2",std::to_wstring(newMods)+L","+std::to_wstring(newKey))){registerKey();message=i18n::text(L"저장하지 못했습니다. 지금 키를 그대로 두었습니다.");render();return false;}
  key=newKey;mods=newMods;shortcut=true;message=i18n::text(L"단축키를 저장했습니다: ")+keyName();render();return true;
 }
 void beginCapture(){capturing=true;UnregisterHotKey(win,2);UnregisterHotKey(win,1);setText(GetDlgItem(dialog,192),i18n::text(L"누를 키를 입력하세요 · Esc 취소"));SetFocus(GetDlgItem(dialog,192));}
 void endCapture(){ignoreUntil=GetTickCount64()+600;capturing=false;registerKey();registerShortcut();if(dialog)setText(GetDlgItem(dialog,192),i18n::text(L"직접 눌러 지정"));}
 void stop(){if(job){TerminateJobObject(job,0);CloseHandle(job);job=nullptr;}if(process){CloseHandle(process);process=nullptr;}message=i18n::text(L"꺼짐");render();}
 void start(){
  if(running())return;
  stop();
  std::wstring root=bundleRoot();if(root.empty()){message=i18n::text(L"내장 시세 파일을 풀지 못했습니다.");render();return;}
  std::wstring script=root+L"\\price\\PriceWorker.ps1";
  if(GetFileAttributesW(script.c_str())==INVALID_FILE_ATTRIBUTES){message=i18n::text(L"내장 시세 파일을 찾지 못했습니다.");render();return;}
  wchar_t system[MAX_PATH]={};GetSystemDirectoryW(system,MAX_PATH);
  std::wstring shell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
  std::wstring command=L"\""+shell+L"\" -NoLogo -NoProfile -NonInteractive -STA -ExecutionPolicy Bypass -File \""+script+L"\" -ParentId "+std::to_wstring(GetCurrentProcessId());
  job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION info={};info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if(!job||!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&info,sizeof(info))){stop();message=i18n::text(L"OCR 작업을 준비하지 못했습니다.");render();return;}
  FILETIME started={};GetSystemTimeAsFileTime(&started);statusSince=((ULONGLONG)started.dwHighDateTime<<32)|started.dwLowDateTime;
  STARTUPINFOW startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION child={};HelperIo io(L"price",startup);
  if(!CreateProcessW(shell.c_str(),&command[0],nullptr,nullptr,io.ok(),CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,root.c_str(),&startup,&child)){stop();message=i18n::text(L"Windows OCR 실행 실패 · ")+std::to_wstring(GetLastError());render();return;}
  if(!AssignProcessToJobObject(job,child.hProcess)||ResumeThread(child.hThread)==(DWORD)-1){TerminateProcess(child.hProcess,1);CloseHandle(child.hThread);CloseHandle(child.hProcess);stop();message=i18n::text(L"OCR 작업 연결 실패");render();return;}
  CloseHandle(child.hThread);process=child.hProcess;message=i18n::text(L"켜는 중 · 글자 인식(OCR)을 준비하고 있습니다.");render();
 }
 // A manual OFF holds until the game exits; the next game launch starts the worker again.
 void toggle(){if(running()){stop();manualOff=true;}else{manualOff=false;retryAfter=0;start();}}
 void setAuto(bool on){autoMode=on;saveString(configPath,L"PriceAuto",on?L"1":L"0");nextGameCheck=0;render();}
 static bool tarkovRunning(){return tarkovProcess();}
 void poll(){
  ULONGLONG now=GetTickCount64();
  if(autoMode&&now>=nextGameCheck){nextGameCheck=now+3000;
   if(!tarkovRunning()){manualOff=false;if(running()){stop();message=i18n::text(L"꺼짐 · 타르코프가 종료되어 같이 껐습니다.");render();}}
   else if(!manualOff&&!running()&&now>=retryAfter)start();
  }
  pollOcrInstall();if(now>=nextOcrCheck){nextOcrCheck=now+5000;readOcrMissing();}
  if(!process)return;
  wchar_t local[MAX_PATH]={};if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){
   std::wstring file=std::wstring(local)+L"\\ModeSwitch\\price-status.txt";
   HANDLE h=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
   if(h!=INVALID_HANDLE_VALUE){FILETIME written={};bool fresh=GetFileTime(h,nullptr,nullptr,&written)&&(((ULONGLONG)written.dwHighDateTime<<32)|written.dwLowDateTime)>=statusSince;char data[8192];DWORD n=0;if(fresh&&ReadFile(h,data,sizeof(data),&n,nullptr)&&n){int size=MultiByteToWideChar(CP_UTF8,0,data,n,nullptr,0);if(size>0){std::wstring s(size,0);MultiByteToWideChar(CP_UTF8,0,data,n,&s[0],size);message=s;}}CloseHandle(h);}
  }
  if(!running()){DWORD code=0;if(GetExitCodeProcess(process,&code)&&code!=0)reportProblem(L"price-crash");if(job){CloseHandle(job);job=nullptr;}CloseHandle(process);process=nullptr;message=i18n::text(L"멈춤 (1분 뒤 다시 시도) · ")+message;retryAfter=now+60000;}
  render();
 }
 void render(){if(!dialog)return;setText(GetDlgItem(dialog,190),message);setText(GetDlgItem(dialog,191),running()?i18n::text(L"지금 끄기"):i18n::text(L"지금 켜기"));setText(GetDlgItem(dialog,194),i18n::text(L"단축키: ")+keyName()+(shortcut?L"":i18n::text(L" (등록 실패)")));InvalidateRect(GetDlgItem(dialog,198),nullptr,TRUE);}
} priceWatch;
LRESULT CALLBACK priceProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:{
  priceWatch.dialog=h;
  // The game page switch is "켜면 타르코프와 함께 자동 실행" (PriceAuto); this page shows the worker and its shortcut.
  pageLabel(h,i18n::text(L"지금 상태"),24,28,688,28,2);
  addSetting(h,L"EDIT",L"",WS_BORDER|ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL,24,60,688,70,190);
  pageButton(h,i18n::text(L"지금 켜기"),24,142,160,191);
  pageLabel(h,i18n::text(L"게임별 기능에서 켜 두면 타르코프와 함께 자동으로 켜지고 꺼집니다."),198,150,514,24,1);
  pageLabel(h,i18n::text(L"켜고 끄는 단축키"),24,212,688,28,2);
  pageLabel(h,L"",24,250,240,26,0,194);
  pageButton(h,i18n::text(L"직접 눌러 지정"),276,244,200,192);
  pageButton(h,i18n::text(L"기본값 (숫자패드 +)"),488,244,224,195);
  pageLabel(h,i18n::text(L"게임 화면 모드"),24,316,688,28,2);
  addSetting(h,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,24,348,320,44,198);
  pageLabel(h,i18n::text(L"전체화면에서는 시세가 게임 위에 보이지 않아, 게임이 꺼져 있을 때 전체창모드로 바꿔 둡니다."),360,348,352,60,1);
  pageLabel(h,i18n::text(L"알아 두기"),24,416,688,28,2);
  pageLabel(h,i18n::text(L"조사 창 오른쪽에 플리마켓 시세, 왼쪽에 필요한 퀘스트, 가운데에 은신처 재료가 나옵니다.\n내구도·잔량·개조 상태에 따라 실제 판매가는 다를 수 있습니다. 화면은 이 PC 안에서만 읽습니다."),24,448,688,48,1);
  pageButton(h,i18n::text(L"글자 인식 언어 설치"),24,556,260,196);ShowWindow(GetDlgItem(h,196),priceWatch.ocrMissing.empty()?SW_HIDE:SW_SHOW);
  pageButton(h,i18n::text(L"닫기"),592,556,120,193);
  priceWatch.render();return 0;
 }
 case WM_COMMAND:{int id=LOWORD(wp);
  if(id==191&&HIWORD(wp)==BN_CLICKED)priceWatch.toggle();
  else if(id==193&&HIWORD(wp)==BN_CLICKED)DestroyWindow(h);
  else if(id==196&&HIWORD(wp)==BN_CLICKED)priceWatch.installOcr();
  else if(id==198&&HIWORD(wp)==BN_CLICKED){tarkovScreen.setEnabled(!tarkovScreen.enabled());priceWatch.render();}
  else if(id==192&&HIWORD(wp)==BN_CLICKED)priceWatch.beginCapture();
  else if(id==195&&HIWORD(wp)==BN_CLICKED){priceWatch.endCapture();priceWatch.bind(VK_ADD,0);}
  return 0;
 }
 case WM_ERASEBKGND:return eraseCarbon(h,(HDC)wp);
 case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
 case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
 case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
 case WM_ACTIVATE:if(LOWORD(wp)==WA_INACTIVE&&priceWatch.capturing)priceWatch.endCapture();return 0;
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:if(priceWatch.capturing)priceWatch.endCapture();priceWatch.dialog=nullptr;editor=nullptr;if(IsWindow(win)){EnableWindow(win,TRUE);SetForegroundWindow(win);}return 0;
 }return DefWindowProcW(h,msg,wp,lp);
}

// Capture only while this settings window has focus; no global keyboard hook.
bool capturePriceKey(const MSG& msg){
 if(!priceWatch.capturing||!priceWatch.dialog)return false;
 if(msg.message!=WM_KEYDOWN&&msg.message!=WM_SYSKEYDOWN&&msg.message!=WM_KEYUP&&msg.message!=WM_SYSKEYUP)return false;
 if(msg.hwnd!=priceWatch.dialog&&!IsChild(priceWatch.dialog,msg.hwnd))return false;
 if(msg.message==WM_KEYUP||msg.message==WM_SYSKEYUP)return true;
 unsigned k=(unsigned)msg.wParam;
 if(k==VK_ESCAPE){priceWatch.endCapture();priceWatch.message=i18n::text(L"단축키 지정을 취소했습니다.");priceWatch.render();return true;}
 if(k==VK_CONTROL||k==VK_SHIFT||k==VK_MENU||k==VK_LWIN||k==VK_RWIN)return true;
 unsigned m=0;if(GetKeyState(VK_CONTROL)<0)m|=MOD_CONTROL;if(GetKeyState(VK_MENU)<0)m|=MOD_ALT;if(GetKeyState(VK_SHIFT)<0)m|=MOD_SHIFT;if(GetKeyState(VK_LWIN)<0||GetKeyState(VK_RWIN)<0)m|=MOD_WIN;
 priceWatch.endCapture();priceWatch.bind(k,m);return true;
}
