#pragma once
#include "i18n.h"
#include <winhttp.h>
#include <thread>
// Updates come from the BlueAsh relay (relay/worker.js): /update/check answers in plain text (version, date, size,
// one "note=" line per change) and counts installs by a random id; the updater (updater/Update.ps1) downloads
// /update/manifest.json's ZIP, checks its SHA-256 and swaps the files.
const wchar_t* appVersion=L"2.24.0";
const wchar_t* updateHost=L"modepulse-relay.modepulse.workers.dev";
HANDLE updaterProcess=nullptr;
struct UpdateInfo{std::wstring version,date,error;std::vector<std::wstring> notes;long long size=0;};
// "1.2.3" -> comparable number; anything malformed is 0 (never newer).
long long versionNumber(const std::wstring& v){long long n=0;int parts=0;size_t i=0;
 while(parts<3){size_t j=i;while(j<v.size()&&iswdigit(v[j]))j++;if(j==i||j-i>4)return 0;n=n*10000+std::stoll(v.substr(i,j-i));parts++;if(parts<3){if(j>=v.size()||v[j]!=L'.')return 0;j++;}i=j;}
 return i==v.size()?n:0;}
std::wstring installId(){
 // Random, made on this PC, never tied to a name or account: lets the relay count users without knowing who they are.
 auto id=regString(configPath,L"InstallId");if(id.size()==32)return id;
 GUID g;if(FAILED(CoCreateGuid(&g)))return L"";wchar_t b[33];swprintf(b,33,L"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",g.Data1,g.Data2,g.Data3,g.Data4[0],g.Data4[1],g.Data4[2],g.Data4[3],g.Data4[4],g.Data4[5],g.Data4[6],g.Data4[7]);
 saveString(configPath,L"InstallId",b);return b;}
// Release channel: "beta" (registry UpdateChannel, the developer's PC) gets a build first through /update/beta.json;
// everyone else follows /update/manifest.json once source/release.ps1 -Promote moves the build there.
bool betaChannel(){return regString(configPath,L"UpdateChannel")==L"beta";}
bool usageStats(){return regString(configPath,L"UsageStats")!=L"0";}
// One request to the relay (blocking: call it on a worker thread). Returns the HTTP status, 0 when unreachable.
DWORD relayRequest(const wchar_t* verb,const std::wstring& path,const std::string& body=std::string()){
 HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;DWORD status=0,size=sizeof(status);
 if(s){WinHttpSetTimeouts(s,5000,5000,10000,10000);c=WinHttpConnect(s,updateHost,INTERNET_DEFAULT_HTTPS_PORT,0);}
 if(c)r=WinHttpOpenRequest(c,verb,path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH);
 const wchar_t* type=body.empty()?nullptr:L"Content-Type: application/json; charset=utf-8";
 if(r&&WinHttpSendRequest(r,type,type?(DWORD)-1L:0,body.empty()?nullptr:(LPVOID)body.data(),(DWORD)body.size(),(DWORD)body.size(),0)&&WinHttpReceiveResponse(r,nullptr))
  WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr);
 for(HINTERNET h:{r,c,s}){if(h)WinHttpCloseHandle(h);}return status;
}
// Anonymous problem count (relay/feedback.js PROBLEMS): only the kind, version and the random install id, once per kind
// per run, and only with 익명 사용 통계 on. It tells the developer that something breaks before anyone writes in.
void reportProblem(const wchar_t* kind){
 static std::set<std::wstring> sent;if(!usageStats()||!sent.insert(kind).second)return;
 auto id=installId();if(id.empty())return;auto* path=new std::wstring(std::wstring(L"/problem?id=")+id+L"&v="+appVersion+L"&k="+kind);
 HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* s=(std::wstring*)p;relayRequest(L"GET",*s);delete s;return 0;},path,0,nullptr);if(t)CloseHandle(t);else delete path;
}
// Runs on a worker thread: everything it needs is copied in, the result goes back to the main window (WM_APP+178).
UpdateInfo fetchUpdate(std::wstring path){
 UpdateInfo u;HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;std::string body;
 if(s){WinHttpSetTimeouts(s,5000,5000,8000,8000);c=WinHttpConnect(s,updateHost,INTERNET_DEFAULT_HTTPS_PORT,0);}
 if(c)r=WinHttpOpenRequest(c,L"GET",path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH);
 DWORD status=0,size=sizeof(status);
 if(r&&WinHttpSendRequest(r,nullptr,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)&&status==200){
  char buf[4096];DWORD n=0;while(WinHttpReadData(r,buf,sizeof(buf),&n)&&n&&body.size()<65536)body.append(buf,n);
 }else u.error=status?i18n::text(L"업데이트 서버 응답 ")+std::to_wstring(status)+i18n::text(L" · 잠시 뒤 다시 확인해 주세요"):i18n::text(L"업데이트 서버에 연결하지 못했습니다 · 인터넷 연결을 확인하세요");
 for(HINTERNET h:{r,c,s})if(h)WinHttpCloseHandle(h);
 int wn=MultiByteToWideChar(CP_UTF8,0,body.data(),(int)body.size(),nullptr,0);std::wstring text(wn,0);if(wn)MultiByteToWideChar(CP_UTF8,0,body.data(),(int)body.size(),&text[0],wn);
 size_t at=0;while(at<text.size()){size_t e=text.find(L'\n',at);if(e==std::wstring::npos)e=text.size();std::wstring line=text.substr(at,e-at);at=e+1;
  if(!line.empty()&&line.back()==L'\r'){line.pop_back();}auto eq=line.find(L'=');if(eq==std::wstring::npos){continue;}auto k=line.substr(0,eq),v=line.substr(eq+1);
  if(k==L"version")u.version=v;else if(k==L"date")u.date=v;else if(k==L"size")u.size=wcstoll(v.c_str(),nullptr,10);else if(k==L"note"&&u.notes.size()<12)u.notes.push_back(v.substr(0,200));}
 if(u.error.empty()&&!versionNumber(u.version))u.error=i18n::text(L"업데이트 정보를 읽지 못했습니다");
 return u;
}
// The updater (updater/Update.ps1) has no window: it writes update-progress.txt (state, step, part, detail, version)
// and the 업데이트 page draws it. state: working | done | latest | error | cancel (cancel is ours).
struct UpdateRun{std::wstring state,step,detail,version;double part=-1;};
std::wstring progressPath(){wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local)))return L"";return std::wstring(local)+L"\\ModeSwitch\\update-progress.txt";}
bool readRun(UpdateRun& r,std::wstring* locale=nullptr,ULONGLONG since=0){
 HANDLE f=CreateFileW(progressPath().c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
 FILETIME written={};if(since&&(!GetFileTime(f,nullptr,nullptr,&written)||(((ULONGLONG)written.dwHighDateTime<<32)|written.dwLowDateTime)<since)){CloseHandle(f);return false;}
 char data[4096];DWORD n=0;BOOL ok=ReadFile(f,data,sizeof(data),&n,nullptr);CloseHandle(f);if(!ok||!n)return false;
 int wn=MultiByteToWideChar(CP_UTF8,0,data,(int)n,nullptr,0);std::wstring text(wn,0);MultiByteToWideChar(CP_UTF8,0,data,(int)n,&text[0],wn);
 UpdateRun next;size_t at=0;while(at<text.size()){size_t e=text.find(L'\n',at);if(e==std::wstring::npos){e=text.size();}auto line=text.substr(at,e-at);at=e+1;auto eq=line.find(L'=');if(eq==std::wstring::npos){continue;}
  auto k=line.substr(0,eq),v=line.substr(eq+1);if(k==L"state")next.state=v;else if(k==L"step")next.step=v;else if(k==L"detail")next.detail=v;else if(k==L"version")next.version=v;else if(k==L"part")next.part=wcstod(v.c_str(),nullptr);else if(k==L"language"&&locale)*locale=v;}
 if(next.state.empty()){return false;}r=next;return true;
}
struct UpdateWatch {
 // info = the last answer that worked (a failed check keeps it, so a short outage does not hide a known update).
 UpdateInfo info;std::wstring error;bool checking=false,checked=false,started=false;SYSTEMTIME checkedAt={};ULONGLONG next=0;HWND page=nullptr;
 UpdateRun run;ULONGLONG statusSince=0;
 bool available(){return versionNumber(info.version)>versionNumber(appVersion);}
 bool running(){return updaterProcess&&WaitForSingleObject(updaterProcess,0)==WAIT_TIMEOUT;}
 bool showingRun(){return running()||run.state==L"error"||run.state==L"cancel";}
 void check(){
    if(busy()){return;}checking=true;next=GetTickCount64()+30*60*1000ULL;lastCheck=GetTickCount64();repaint();
  // The relay's free plan runs the worker 100,000 times a day: at 48 checks a day per install that is about 2,000 users.
  // So the regular check reads a static file (served without the worker, unlimited); the worker's /update/check, which
  // counts the install, only once per UTC day (and when usage stats are on).
  SYSTEMTIME st;GetSystemTime(&st);int day=st.wYear*10000+st.wMonth*100+st.wDay;std::wstring path=betaChannel()?L"/update/check-beta.txt":L"/update/check.txt";
  if(usageStats()&&countedDay!=day){auto id=installId();if(!id.empty()){path=std::wstring(L"/update/check?v=")+appVersion+(betaChannel()?L"&channel=beta":L"")+L"&id="+id;countedDay=day;}}
  HWND target=win;std::thread([path,target]{auto* u=new UpdateInfo(fetchUpdate(path));if(!PostMessageW(target,WM_APP+178,0,(LPARAM)u))delete u;}).detach();
 }
 // First check 20 s after start (the network may not be up yet at login), then every 30 minutes while auto check is on,
 // and again when the window is opened (at most once a minute): a release shows up as the "!" on 업데이트 without a button.
 ULONGLONG lastCheck=0;int countedDay=0;
 void soon(){if(started&&!busy()&&GetTickCount64()-lastCheck>60000)check();}
 // Checking, and not stuck: a check whose answer never came back (no WM_APP+178) must not block every later one.
 bool busy(){return checking&&GetTickCount64()-lastCheck<60000;}
 void poll(){ULONGLONG now=GetTickCount64();
  if(!started){started=true;next=now+20000;
   // Right after an update the new version says so once; an update that failed after closing BlueAsh says why.
   if(regString(configPath,L"LastVersion")!=appVersion){if(!regString(configPath,L"LastVersion").empty())notify((std::wstring(L"BlueAsh ")+appVersion+i18n::text(L"로 업데이트했습니다")).c_str(),i18n::text(L"바뀐 점은 업데이트 화면에서 볼 수 있습니다."));saveString(configPath,L"LastVersion",appVersion);}
   UpdateRun last;std::wstring locale;WIN32_FILE_ATTRIBUTE_DATA a={};auto p=progressPath();
   if(readRun(last,&locale)&&locale==i18n::languageCode()&&last.state==L"error"&&GetFileAttributesExW(p.c_str(),GetFileExInfoStandard,&a)){FILETIME t;GetSystemTimeAsFileTime(&t);ULONGLONG age=((((ULONGLONG)t.dwHighDateTime<<32)|t.dwLowDateTime)-(((ULONGLONG)a.ftLastWriteTime.dwHighDateTime<<32)|a.ftLastWriteTime.dwLowDateTime))/10000000ULL;
    if(age<600){run=last;reportProblem(L"update-fail");notify(i18n::text(L"업데이트하지 못했습니다"),last.step.c_str());}}
   }
  if(now>=next)check();}
 // While the updater runs: re-read its progress every 100 ms (timer 79 on the main window).
 void tick(){UpdateRun r;if(statusSince&&readRun(r,nullptr,statusSince)){bool changed=r.state!=run.state||r.step!=run.step||r.detail!=run.detail||r.part!=run.part;run=r;if(changed)repaint();}
  if(!running()){KillTimer(win,79);if(run.state==L"working"){run.state=L"error";run.step=i18n::text(L"업데이트가 중간에 멈췄습니다.");run.detail=i18n::text(L"기존 프로그램과 설정은 그대로입니다.");}
   else if(run.state==L"latest"){run={};checked=false;check();}
   if(run.state==L"error"){reportProblem(L"update-fail");}repaint();}}
 void notify(const wchar_t* title,const wchar_t* text){
  NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=NIIF_INFO;wcsncpy(n.szInfoTitle,title,63);wcsncpy(n.szInfo,text,255);Shell_NotifyIconW(NIM_MODIFY,&n);balloonUpdateAt=GetTickCount64();
 }
 void done(UpdateInfo* u){
  bool was=available();if(u->error.empty())info=*u;error=u->error;delete u;
  checking=false;checked=true;GetLocalTime(&checkedAt);
  // Tell once per new version (not on every 6-hour check).
  if(available()&&!was&&regString(configPath,L"UpdateNotified")!=info.version){saveString(configPath,L"UpdateNotified",info.version);notify((i18n::text(L"새 버전 ")+info.version+i18n::text(L"이 나왔습니다")).c_str(),i18n::text(L"눌러서 바뀐 점을 보고 바로 업데이트하세요."));}
  updateReady=available();repaint();
 }
 // Only what shows the update: the sidebar entry (its ! badge) and the page, drawn without erasing first (no flicker
 // while the progress bar moves every 100 ms).
 void repaint(){InvalidateRect(GetDlgItem(win,38),nullptr,FALSE);if(page){layout();InvalidateRect(page,nullptr,FALSE);}}
 // Bottom row: [닫기][지금 업데이트] — 지금 업데이트 only when something newer exists; while updating only 취소 (219).
 // Checking runs every 30 minutes, when the window opens and when this page opens; the refresh icon on the card checks now.
 int laidOut=-1; // buttons are moved only when what they show changes, not on every progress tick
 void layout(){if(!page)return;bool a=available()&&!showingRun(),busy=running();int key=(a?1:0)|(busy?2:0);if(key==laidOut)return;laidOut=key;auto show=[&](int id,bool on){ShowWindow(GetDlgItem(page,id),on?SW_SHOW:SW_HIDE);};
  show(212,a);show(214,!busy);show(219,busy);
  SetWindowPos(GetDlgItem(page,214),nullptr,a?540:696,556,0,0,SWP_NOSIZE|SWP_NOZORDER);}
} updateWatch;
void launchUpdater(){
 if(updateWatch.running())return;
 if(updaterProcess){CloseHandle(updaterProcess);updaterProcess=nullptr;}
 auto fail=[](const wchar_t* why){updateWatch.run={L"error",why,i18n::text(L"기존 프로그램과 설정은 그대로입니다."),L"",-1};updateWatch.repaint();};
 wchar_t exe[32768]={},temp[MAX_PATH]={},system[MAX_PATH]={};GetModuleFileNameW(nullptr,exe,32768);GetTempPathW(MAX_PATH,temp);GetSystemDirectoryW(system,MAX_PATH);
 std::wstring root=exe;root=root.substr(0,root.find_last_of(L"\\/"));auto bundle=bundleRoot();if(bundle.empty()){fail(i18n::text(L"내장 업데이트 파일을 풀지 못했습니다."));return;}
 std::wstring folder=std::wstring(temp)+L"ModeSwitch-updater-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
 if(!CreateDirectoryW(folder.c_str(),nullptr)){fail(i18n::text(L"업데이트 작업 폴더를 만들지 못했습니다."));return;}
 for(auto name:{L"Update.ps1",L"UpdateCore.ps1",L"UpdateLanguage.ps1"}){auto from=bundle+L"\\updater\\"+name,to=folder+L"\\"+name;if(!CopyFileW(from.c_str(),to.c_str(),TRUE)){fail(i18n::text(L"내장 업데이트 파일을 찾지 못했습니다."));return;}}
 std::wstring shell=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
 std::wstring args=L"\""+shell+L"\" -NoProfile -ExecutionPolicy Bypass -File \""+folder+L"\\Update.ps1\" -ParentId "+std::to_wstring(GetCurrentProcessId())+L" -Window "+std::to_wstring((LONG_PTR)win)+L" -Install \""+root+L"\" -CurrentVersion "+appVersion+L" -Mode online -Feed \"https://"+updateHost+(betaChannel()?L"/update/beta.json\"":L"/update/manifest.json\"");
 FILETIME started={};GetSystemTimeAsFileTime(&started);updateWatch.statusSince=((ULONGLONG)started.dwHighDateTime<<32)|started.dwLowDateTime;updateWatch.run={L"working",i18n::text(L"업데이트를 준비하는 중…"),L"",L"",-1};
 STARTUPINFOW startup={};startup.cb=sizeof(startup);PROCESS_INFORMATION p={};
 if(!CreateProcessW(shell.c_str(),&args[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,folder.c_str(),&startup,&p)){fail(i18n::text(L"업데이트 도우미를 실행하지 못했습니다."));return;}
 CloseHandle(p.hThread);updaterProcess=p.hProcess;SetTimer(win,79,100,nullptr);updateWatch.repaint();
}
// 취소 is offered while downloading; once BlueAsh is asked to close, the updater no longer listens to it anyway.
void cancelUpdater(){if(!updateWatch.running())return;TerminateProcess(updaterProcess,1);updateWatch.run={L"cancel",i18n::text(L"업데이트를 취소했습니다."),i18n::text(L"기존 프로그램과 설정은 그대로입니다."),L"",-1};updateWatch.repaint();}
std::wstring megabytes(long long b){wchar_t s[32];swprintf(s,32,L"%.1f MB",b/1048576.0);return s;}
// The page paints its version card and, below it, either the change list or the running update; controls are only the toggles and buttons.
RECT refreshButton(){return {766,116,804,154};}
bool refreshHover=false;ULONGLONG refreshSpinAt=0; // set by a click: the icon turns once (and keeps turning while the check runs)
// Angle of the refresh icon: one eased turn in 0.7 s; if the check is still running after that, steady turns until it ends.
double refreshAngle(){if(!refreshSpinAt){return 0;}double t=(GetTickCount64()-refreshSpinAt)/700.0;if(t<1){double u=1-t;return 360*(1-u*u*u);}return updateWatch.checking?360*(t-1-(int)(t-1)):0;}
void paintUpdate(HDC dc){
 auto& w=updateWatch;bool a=w.available(),r=w.showingRun();
 roundBox(dc,{36,40,816,166},well,a?blue:line,16);
 textAt(dc,i18n::text(L"지금 버전"),{58,54,330,76},smallFont,muted);
 textAt(dc,appVersion,{58,76,330,116},titleFont,ink);
 if(a){std::wstring tag=i18n::text(L"새 버전 ")+w.info.version;RECT pill={340,58,340+30+(int)tag.size()*12,86};roundBox(dc,pill,tint,blue,14);textAt(dc,tag,pill,boldFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  std::wstring meta=(w.info.date.empty()?L"":w.info.date+i18n::text(L" 출시"))+(w.info.size>0?(w.info.date.empty()?L"":L"  ·  ")+megabytes(w.info.size):L"");textAt(dc,meta,{340,96,804,120},smallFont,muted);}
 else if(w.checking)textAt(dc,i18n::text(L"새 버전을 확인하는 중…"),{340,66,804,96},boldFont,ink);
 else if(!w.error.empty()){textAt(dc,i18n::text(L"확인하지 못했습니다"),{340,62,804,88},boldFont,ink);textAt(dc,w.error,{340,92,804,116},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
 else if(w.checked)textAt(dc,i18n::text(L"최신 버전입니다"),{340,66,804,96},boldFont,ink);
 else textAt(dc,i18n::text(L"아직 확인하지 않았습니다"),{340,66,804,96},boldFont,ink);
 wchar_t when[64]=L"";if(w.checked){auto& t=w.checkedAt;swprintf(when,64,i18n::text(L"마지막 확인  %ls %d:%02d"),t.wHour<12?i18n::text(L"오전"):i18n::text(L"오후"),t.wHour%12?t.wHour%12:12,t.wMinute);}
 textAt(dc,when,{58,128,804,152},smallFont,muted);
 // Check now: a refresh icon in the card's bottom-right corner (blue while a check runs).
 if(!w.running()){RECT b=refreshButton();if(refreshHover&&!w.checking)roundBox(dc,b,tint,tint,10);double deg=refreshAngle();
  if(deg!=0){double a=deg*3.14159265358979/180,c=std::cos(a),n=std::sin(a),cx=(b.left+b.right)/2.0,cy=(b.top+b.bottom)/2.0;int mode=SetGraphicsMode(dc,GM_ADVANCED);XFORM x={(FLOAT)c,(FLOAT)n,(FLOAT)-n,(FLOAT)c,(FLOAT)(cx-cx*c+cy*n),(FLOAT)(cy-cx*n-cy*c)};SetWorldTransform(dc,&x);
   glyph(dc,0xE72C,b,iconFont,w.checking?blue:refreshHover?ink:muted);ModifyWorldTransform(dc,nullptr,MWT_IDENTITY);SetGraphicsMode(dc,mode);}
  else glyph(dc,0xE72C,b,iconFont,w.checking?blue:refreshHover?ink:muted);}
 if(r){
  bool bad=w.run.state==L"error"||w.run.state==L"cancel";
  textAt(dc,bad?(w.run.state==L"cancel"?i18n::text(L"업데이트 취소됨"):i18n::text(L"업데이트하지 못했습니다")):i18n::text(L"업데이트 중"),{36,182,816,208},boldFont,bad?danger:ink);
  roundBox(dc,{36,216,816,432},well,bad?danger:line,16);
  textAt(dc,w.run.step,{60,240,792,306},boldFont,ink,DT_LEFT|DT_WORDBREAK);
  if(!bad){RECT track={60,316,792,326};roundBox(dc,track,line,line,8);
   if(w.run.part>0){RECT fill=track;fill.right=fill.left+std::max(10,(int)((track.right-track.left)*std::min(1.0,w.run.part)));roundBox(dc,fill,blue,blue,8);}
   if(w.run.part>=0){wchar_t pct[16];swprintf(pct,16,L"%d%%",(int)(std::min(1.0,w.run.part)*100+0.5));textAt(dc,pct,{60,336,792,360},boldFont,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}}
  textAt(dc,w.run.detail,{60,bad?316:336,700,bad?346:360},smallFont,muted);
  if(!bad)textAt(dc,i18n::text(L"다 받으면 BlueAsh가 잠깐 꺼졌다가 새 버전으로 다시 켜집니다."),{60,392,792,416},smallFont,muted);
  return;
 }
 textAt(dc,a?i18n::text(L"바뀐 점"):i18n::text(L"업데이트는 이렇게 됩니다"),{36,182,816,208},boldFont,ink);
 roundBox(dc,{36,216,816,432},well,line,16);
 std::vector<std::wstring> lines=a?w.info.notes:std::vector<std::wstring>{i18n::text(L"30분마다 새 버전을 확인하고, 새 버전이 나오면 알림과 함께 왼쪽 메뉴의 업데이트에 ! 가 표시됩니다."),i18n::text(L"지금 업데이트를 누르면 이 화면에서 받고 확인한 뒤, 잠깐 꺼졌다가 새 버전으로 다시 켜집니다."),i18n::text(L"설정은 그대로 남고, 바꾸다 실패하면 이전 파일로 되돌립니다.")};
 if(a&&lines.empty())lines.push_back(i18n::text(L"이번 버전의 바뀐 점 설명이 없습니다."));
 int y=232;for(auto& source:lines){auto line=i18n::text(source);RECT t={78,y,796,420};RECT m=t;auto old=SelectObject(dc,smallFont);DrawTextW(dc,line.c_str(),-1,&m,DT_CALCRECT|DT_WORDBREAK);SelectObject(dc,old);
  if(y+(m.bottom-m.top)>420){break;}HBRUSH b=CreateSolidBrush(blue);RECT dot={60,y+8,66,y+14};FillRect(dc,&dot,b);DeleteObject(b);
  textAt(dc,line,t,smallFont,a?ink:muted,DT_LEFT|DT_WORDBREAK);y+=m.bottom-m.top+10;}
}
LRESULT CALLBACK updateProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:
  updateWatch.page=h;
  addSetting(h,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,36,450,780,42,217);
  pageButton(h,i18n::text(L"닫기"),696,556,120,214);
  pageButton(h,i18n::text(L"지금 업데이트"),676,556,140,212,true);
  pageButton(h,i18n::text(L"취소"),696,556,120,219);
  // A failed or cancelled update is retried from 지금 업데이트 when the page opens again.
  if(!updateWatch.running()&&(updateWatch.run.state==L"error"||updateWatch.run.state==L"cancel"))updateWatch.run={};
  updateWatch.layout();if(!updateWatch.checked&&!updateWatch.checking)updateWatch.check();else updateWatch.soon();return 0;
 // Drawn off screen (background + card + progress) and copied in one go.
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT rc;GetClientRect(h,&rc);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,rc.right,rc.bottom);auto old=SelectObject(mem,bmp);
  eraseCarbon(h,mem);paintUpdate(mem);BitBlt(dc,0,0,rc.right,rc.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
 case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){int id=LOWORD(wp);
  if(id==212)launchUpdater();else if(id==219)cancelUpdater();else if(id==214)DestroyWindow(h);
  else if(id==217){saveString(configPath,L"UsageStats",featureOn(id)?L"0":L"1");InvalidateRect(GetDlgItem(h,id),nullptr,TRUE);}
 }return 0;
 case WM_MOUSEMOVE:{POINT p={(short)LOWORD(lp),(short)HIWORD(lp)};RECT b=refreshButton();bool on=PtInRect(&b,p)!=FALSE;if(on!=refreshHover){refreshHover=on;InvalidateRect(h,&b,FALSE);}
  if(on){TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);}return 0;}
 case WM_MOUSELEAVE:if(refreshHover){refreshHover=false;RECT b=refreshButton();InvalidateRect(h,&b,FALSE);}return 0;
 case WM_SETCURSOR:if((HWND)wp==h){POINT p;GetCursorPos(&p);ScreenToClient(h,&p);RECT b=refreshButton();if(PtInRect(&b,p)&&!updateWatch.running()){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}break;
 case WM_LBUTTONUP:{POINT p={(short)LOWORD(lp),(short)HIWORD(lp)};RECT b=refreshButton();if(PtInRect(&b,p)&&!updateWatch.running()&&!updateWatch.busy()){refreshSpinAt=GetTickCount64();SetTimer(h,7,15,nullptr);updateWatch.check();}return 0;}
 case WM_TIMER:if(wp==7){RECT b=refreshButton();if(GetTickCount64()-refreshSpinAt>=700&&!updateWatch.checking){refreshSpinAt=0;KillTimer(h,7);}InvalidateRect(h,&b,FALSE);}return 0;
 case WM_ERASEBKGND:return 1; // the background is part of WM_PAINT's off-screen frame
 case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
 case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:refreshSpinAt=0;KillTimer(h,7);updateWatch.page=nullptr;updateWatch.laidOut=-1;editor=nullptr;if(IsWindow(win)){EnableWindow(win,TRUE);SetForegroundWindow(win);}return 0;
 }return DefWindowProcW(h,msg,wp,lp);
}
void updateOnShow(){updateWatch.soon();}
