#pragma once
#include "security_notice.h"
// The phone uses the same relay snapshot; keep checks infrequent and ignore stale snapshots.
ULONGLONG maintenanceNext=0;bool maintenanceBusy=false;ULONGLONG maintenanceMenuAt=0;int securityCount=-1;ULONGLONG noticeRequestLast=0;
std::mutex noticeTextLock;std::wstring noticeText,noticeTitle;bool noticeHasSecurityLink=true;
// The bell's notices come from the relay (/notices, relay/notices.json): posted and changed without a release. Until the
// relay answers, the single notice of 2.30-2.31 (/security/notice + security-notice.txt) stands in.
struct BellNotice{std::wstring kind,title,body,link,linkText;};
std::vector<BellNotice> bellNotices;bool bellListed=false;int bellScroll=0;
std::wstring securityNoticeTitle(){std::lock_guard<std::mutex> guard(noticeTextLock);return noticeTitle.empty()?securitynotice::title():noticeTitle;}
bool securityNoticeLink(){std::lock_guard<std::mutex> guard(noticeTextLock);return noticeHasSecurityLink;}
std::wstring securityNoticeBody(){std::lock_guard<std::mutex> guard(noticeTextLock);return noticeText.empty()?securitynotice::result(securityCount):noticeText;}
const wchar_t* securityLinkUrl=L"https://www.virustotal.com/gui/file/3aadf541644e94cf29fe23b789c574c06a2db62bb6eaada057bea6ffa3f05e65/detection";
// What the bell shows now: the relay's list, or the old single notice.
std::vector<BellNotice> bellItems(){
 {std::lock_guard<std::mutex> guard(noticeTextLock);if(bellListed){return bellNotices;}}
 if(!securityNotice){return {};}bool link=securityNoticeLink();return {BellNotice{link?L"security":L"general",securityNoticeTitle(),securityNoticeBody(),link?securityLinkUrl:L"",link?securitynotice::link():L""}};}
int bellCount(){return (int)bellItems().size();}
// key=value lines per notice, notices split by a blank line (relay/notices.js).
std::vector<BellNotice> parseBellNotices(const std::wstring& text){std::vector<BellNotice> out;BellNotice cur;bool any=false;size_t at=0;
 auto flush=[&]{if(any&&!cur.title.empty()&&!cur.body.empty()&&out.size()<10){if(cur.link.rfind(L"https://",0)!=0)cur.link.clear();out.push_back(cur);}cur=BellNotice{};any=false;};
 while(at<=text.size()){size_t e=text.find(L'\n',at);if(e==std::wstring::npos)e=text.size();std::wstring line=text.substr(at,e-at);at=e+1;if(!line.empty()&&line.back()==L'\r')line.pop_back();
  if(line.empty()){flush();continue;}auto eq=line.find(L'=');if(eq==std::wstring::npos)continue;auto k=line.substr(0,eq),v=line.substr(eq+1);any=true;
  if(k==L"kind")cur.kind=v;else if(k==L"title")cur.title=v;else if(k==L"body")cur.body=v;else if(k==L"link")cur.link=v;else if(k==L"linktext")cur.linkText=v;}
 flush();return out;}
bool maintenanceFresh(const std::string& s){int y=0,mo=0,d=0,h=0,mi=0,sec=0;if(sscanf(s.c_str(),"%d-%d-%dT%d:%d:%d",&y,&mo,&d,&h,&mi,&sec)!=6)return false;
 SYSTEMTIME st={};st.wYear=(WORD)y;st.wMonth=(WORD)mo;st.wDay=(WORD)d;st.wHour=(WORD)h;st.wMinute=(WORD)mi;st.wSecond=(WORD)sec;FILETIME f={},now={};if(!SystemTimeToFileTime(&st,&f))return false;GetSystemTimeAsFileTime(&now);
 ULONGLONG a=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime,b=((ULONGLONG)now.dwHighDateTime<<32)|now.dwLowDateTime;return a<=b+60ULL*10000000&&b<=a+25ULL*60*10000000;}
// The menu's cards: heights follow the measured text, so any number of notices fits (the list scrolls past 600 px).
struct BellCard{RECT box,linkRect;BellNotice n;};
int bellTop(){return maintenance.current>=1&&maintenance.current<=3?154:52;}
std::vector<BellCard> bellLayout(HDC dc,int& total){std::vector<BellCard> cards;int y=bellTop();
 for(auto& n:bellItems()){RECT m={0,0,268,0};HGDIOBJ o=SelectObject(dc,smallFont);DrawTextW(dc,n.body.c_str(),-1,&m,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,o);int bodyH=(int)(m.bottom-m.top)+8;
  int h=12+28+6+bodyH+(n.link.empty()?14:40);BellCard c{{12,y,308,y+h},{0,0,0,0},n};if(!n.link.empty())c.linkRect={26,y+h-36,294,y+h-10};cards.push_back(c);y+=h+10;}
 total=y+2;return cards;}
int bellHeight(){HDC dc=dpi::getDC(win);int total=0;bellLayout(dc,total);(ReleaseDC)(win,dc);bool issue=maintenance.current>=1&&maintenance.current<=3;int empty=issue?154:108;return std::min(600,std::max(empty,bellItems().empty()?empty:total));}
void maintenanceDone(int state,int notice,int count){maintenanceBusy=false;securityCount=count;if(notice>=0)securityNotice=notice==1;{std::lock_guard<std::mutex> guard(noticeTextLock);if(bellListed)securityNotice=!bellNotices.empty();}int event=maintenance.accept(state,state>=0);if(event){const wchar_t* title=i18n::text(event==1?L"타르코프 점검 시작":L"타르코프 점검 끝");const wchar_t* body=i18n::text(event==1?L"지금 타르코프 서버 점검 중입니다. 끝나면 다시 알려 드릴게요.":L"점검이 끝났습니다. 지금 접속할 수 있어요.");
 NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=event==1?NIIF_WARNING:NIIF_INFO;wcsncpy(n.szInfoTitle,title,63);wcsncpy(n.szInfo,body,255);Shell_NotifyIconW(NIM_MODIFY,&n);raidWatch.sound();}
 trayUpdate();if(securityNotice||maintenance.current==1)SetTimer(win,0xB1F,60,nullptr);else KillTimer(win,0xB1F);InvalidateRect(GetDlgItem(win,87),nullptr,FALSE);if(maintenanceMenu){SetWindowPos(maintenanceMenu,nullptr,0,0,320,bellHeight(),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);InvalidateRect(maintenanceMenu,nullptr,FALSE);}}
std::wstring utf8Text(const std::string& s,size_t limit){if(s.empty()||s.size()>limit)return L"";int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(size<=0)return L"";std::wstring w(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),&w[0],size);return w;}
void maintenancePoll(){auto now=GetTickCount64();if(maintenanceBusy||now<maintenanceNext)return;maintenanceBusy=true;noticeRequestLast=now;maintenanceNext=now+5ULL*60*1000;HWND target=win;std::wstring lang=i18n::languageCode();
 std::thread([target,lang]{std::string b;int state=-1,notice=-1,count=-1;if(relayGet(L"/tarkov/status",b)==200&&maintenanceFresh(phonegame::jsonString(b,"checkedAt"))){auto p=b.find("\"state\"");if(p!=std::string::npos&&(p=b.find(':',p))!=std::string::npos){p=b.find_first_not_of(" \t\r\n",p+1);if(p!=std::string::npos&&b[p]>='0'&&b[p]<='3'&&(p+1==b.size()||b[p+1]==','||b[p+1]=='}'||isspace((unsigned char)b[p+1])))state=b[p]-'0';}}
  std::string list;if(relayGet(L"/notices?lang="+lang,list)==200&&list.size()<=40000){auto parsed=parseBellNotices(utf8Text(list,40000));std::lock_guard<std::mutex> guard(noticeTextLock);bellNotices=parsed;bellListed=true;}
  std::string n;if(relayGet(L"/security-notice.txt",n)==200){if(n=="active=1\n"||n=="active=1\r\n")notice=1;else if(n=="active=0\n"||n=="active=0\r\n")notice=0;}std::string s;if(relayGet(L"/security/status",s)==200&&phonegame::jsonString(s,"hash")=="3aadf541644e94cf29fe23b789c574c06a2db62bb6eaada057bea6ffa3f05e65"){std::string v=phonegame::jsonString(s,"count");if(!v.empty()&&v.size()<=3&&v.find_first_not_of("0123456789")==std::string::npos){int x=std::stoi(v);if(x<=100)count=x;}}std::string remote;if(relayGet(L"/security/notice?lang="+lang,remote)==200&&!remote.empty()&&remote.size()<=8000){std::wstring text=utf8Text(remote,8000);if(!text.empty()&&text.size()<=2000){auto a=text.find(L'\n'),b=a==std::wstring::npos?a:text.find(L'\n',a+1);if(a!=std::wstring::npos&&b!=std::wstring::npos&&b>a+1&&b+1<text.size()){std::wstring kind=text.substr(0,a);if(kind==L"security"||kind==L"general"){std::lock_guard<std::mutex> guard(noticeTextLock);noticeTitle=text.substr(a+1,b-a-1);noticeText=text.substr(b+1);noticeHasSecurityLink=kind==L"security";}}}}PostMessageW(target,WM_APP+184,(WPARAM)((state+1)|((count+1)<<8)),(LPARAM)notice);}).detach();}
LRESULT CALLBACK maintenanceProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,panelBrush);
 bool issue=maintenance.current>=1&&maintenance.current<=3;int total=0;auto cards=bellLayout(dc,total);int maxScroll=std::max(0,total-(int)r.bottom);bellScroll=std::clamp(bellScroll,0,maxScroll);
 // cards scroll under a fixed header
 {int saved=SaveDC(dc);IntersectClipRect(dc,0,48,r.right,r.bottom);
  if(issue){const wchar_t* title=i18n::text(maintenance.current==1?L"타르코프 점검 시작":maintenance.current==2?L"일부 불안정":L"접속 불가");textAt(dc,title,{18,52-bellScroll,r.right-18,84-bellScroll},smallBold,maintenance.current==1?danger:ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);if(maintenance.current==1)textAt(dc,i18n::text(L"지금 타르코프 서버 점검 중입니다. 끝나면 다시 알려 드릴게요."),{18,88-bellScroll,r.right-18,146-bellScroll},smallFont,muted,DT_LEFT|DT_WORDBREAK);}
  for(auto& c:cards){RECT b=c.box;OffsetRect(&b,0,-bellScroll);bool sec=c.n.kind==L"security";COLORREF accent=sec?RGB(78,131,255):RGB(70,208,146);
   roundBox(dc,b,sec?RGB(20,36,69):RGB(17,40,48),sec?RGB(64,105,195):RGB(52,128,110),12);roundBox(dc,{b.left+8,b.top+14,b.left+12,b.top+37},accent,accent,2);
   textAt(dc,c.n.title.c_str(),{b.left+20,b.top+12,b.right-14,b.top+40},smallBold,sec?RGB(155,190,255):RGB(140,232,196),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   textAt(dc,c.n.body.c_str(),{b.left+14,b.top+46,b.right-14,b.bottom-(c.n.link.empty()?10:40)},smallFont,ink,DT_LEFT|DT_WORDBREAK);
   if(!c.n.link.empty()){RECT k=c.linkRect;OffsetRect(&k,0,-bellScroll);std::wstring label=c.n.linkText.empty()?std::wstring(sec?securitynotice::link():i18n::text(L"자세히 보기 ↗")):c.n.linkText;textAt(dc,label,k,smallBold,sec?RGB(150,190,255):RGB(120,220,180),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}}
  if(cards.empty()&&!issue){textAt(dc,securitynotice::empty(),{18,52,r.right-18,95},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}RestoreDC(dc,saved);}
 {RECT head={0,0,r.right,48};FillRect(dc,&head,panelBrush);}textAt(dc,i18n::text(L"알림"),{18,12,r.right-18,43},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE);EndPaint(h,&ps);return 0;}
 case WM_MOUSEWHEEL:{bellScroll-=GET_WHEEL_DELTA_WPARAM(w)/2;InvalidateRect(h,nullptr,FALSE);return 0;}
 case WM_LBUTTONUP:{POINT p={(short)LOWORD(l),(short)HIWORD(l)};HDC dc=dpi::getDC(h);int total=0;auto cards=bellLayout(dc,total);(ReleaseDC)(h,dc);
  for(auto& c:cards){RECT k=c.linkRect;OffsetRect(&k,0,-bellScroll);if(!c.n.link.empty()&&p.y>=48&&PtInRect(&k,p)){ShellExecuteW(h,L"open",c.n.link.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;}}break;}
 case WM_SETCURSOR:{POINT p;GetCursorPos(&p);ScreenToClient(h,&p);HDC dc=dpi::getDC(h);int total=0;auto cards=bellLayout(dc,total);(ReleaseDC)(h,dc);for(auto& c:cards){RECT k=c.linkRect;OffsetRect(&k,0,-bellScroll);if(!c.n.link.empty()&&p.y>=48&&PtInRect(&k,p)){SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;}}break;}
 case WM_TIMER:{RECT r;GetClientRect(h,&r);double t=std::min(1.0,(GetTickCount64()-maintenanceMenuAt)/180.0),e=1-(1-t)*(1-t)*(1-t);if(t>=1){KillTimer(h,1);SetWindowRgn(h,nullptr,TRUE);}else SetWindowRgn(h,CreateRoundRectRgn(0,0,r.right+1,std::max(2,(int)std::lround(r.bottom*e))+1,14,14),TRUE);return 0;}
 case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)DestroyWindow(h);return 0;
 case WM_KEYDOWN:if(w==VK_ESCAPE)DestroyWindow(h);return 0;
 case WM_DESTROY:maintenanceMenu=nullptr;InvalidateRect(GetDlgItem(win,87),nullptr,FALSE);return 0;
 }return DefWindowProcW(h,m,w,l);}
void showMaintenanceMenu(){if(maintenanceMenu){DestroyWindow(maintenanceMenu);return;}static bool registered=false;if(!registered){WNDCLASSW wc={};wc.lpfnWndProc=maintenanceProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshMaintenance";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);registered=true;}
 if(GetTickCount64()-noticeRequestLast>=15000){maintenanceNext=0;maintenancePoll();}RECT b;GetWindowRect(GetDlgItem(win,87),&b);bellScroll=0;int height=bellHeight();maintenanceMenu=CreateWindowExW(WS_EX_TOOLWINDOW,L"BlueAshMaintenance",L"",WS_POPUP,b.right-320,b.bottom+8,320,height,win,nullptr,GetModuleHandleW(nullptr),nullptr);if(!maintenanceMenu)return;
 maintenanceMenuAt=GetTickCount64();SetWindowRgn(maintenanceMenu,CreateRectRgn(0,0,320,2),FALSE);SetTimer(maintenanceMenu,1,10,nullptr);int round=2;DwmSetWindowAttribute(maintenanceMenu,33,&round,sizeof(round));COLORREF edge=line;DwmSetWindowAttribute(maintenanceMenu,34,&edge,sizeof(edge));ShowWindow(maintenanceMenu,SW_SHOW);SetForegroundWindow(maintenanceMenu);InvalidateRect(GetDlgItem(win,87),nullptr,FALSE);}
