#pragma once
// The phone uses the same relay snapshot; keep checks infrequent and ignore stale snapshots.
ULONGLONG maintenanceNext=0;bool maintenanceBusy=false;ULONGLONG maintenanceMenuAt=0;
bool maintenanceFresh(const std::string& s){int y=0,mo=0,d=0,h=0,mi=0,sec=0;if(sscanf(s.c_str(),"%d-%d-%dT%d:%d:%d",&y,&mo,&d,&h,&mi,&sec)!=6)return false;
 SYSTEMTIME st={};st.wYear=(WORD)y;st.wMonth=(WORD)mo;st.wDay=(WORD)d;st.wHour=(WORD)h;st.wMinute=(WORD)mi;st.wSecond=(WORD)sec;FILETIME f={},now={};if(!SystemTimeToFileTime(&st,&f))return false;GetSystemTimeAsFileTime(&now);
 ULONGLONG a=((ULONGLONG)f.dwHighDateTime<<32)|f.dwLowDateTime,b=((ULONGLONG)now.dwHighDateTime<<32)|now.dwLowDateTime;return a<=b+60ULL*10000000&&b<=a+25ULL*60*10000000;}
void maintenanceDone(int state){maintenanceBusy=false;int event=maintenance.accept(state,state>=0);if(event){const wchar_t* title=i18n::text(event==1?L"타르코프 점검 시작":L"타르코프 점검 끝");const wchar_t* body=i18n::text(event==1?L"지금 타르코프 서버 점검 중입니다. 끝나면 다시 알려 드릴게요.":L"점검이 끝났습니다. 지금 접속할 수 있어요.");
 NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=event==1?NIIF_WARNING:NIIF_INFO;wcsncpy(n.szInfoTitle,title,63);wcsncpy(n.szInfo,body,255);Shell_NotifyIconW(NIM_MODIFY,&n);raidWatch.sound();}
 trayUpdate();InvalidateRect(GetDlgItem(win,87),nullptr,FALSE);if(maintenanceMenu)InvalidateRect(maintenanceMenu,nullptr,FALSE);}
void maintenancePoll(){auto now=GetTickCount64();if(maintenanceBusy||now<maintenanceNext)return;maintenanceBusy=true;maintenanceNext=now+5ULL*60*1000;HWND target=win;
 std::thread([target]{std::string b;int state=-1;if(relayGet(L"/tarkov/status",b)==200&&maintenanceFresh(phonegame::jsonString(b,"checkedAt"))){auto p=b.find("\"state\"");if(p!=std::string::npos&&(p=b.find(':',p))!=std::string::npos){p=b.find_first_not_of(" \t\r\n",p+1);if(p!=std::string::npos&&b[p]>='0'&&b[p]<='3'&&(p+1==b.size()||b[p+1]==','||b[p+1]=='}'||isspace((unsigned char)b[p+1])))state=b[p]-'0';}}PostMessageW(target,WM_APP+184,(WPARAM)state,0);}).detach();}
LRESULT CALLBACK maintenanceProc(HWND h,UINT m,WPARAM w,LPARAM l){switch(m){
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,panelBrush);textAt(dc,i18n::text(L"알림"),{18,12,r.right-18,43},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
 const wchar_t* title=i18n::text(maintenance.current==1?L"타르코프 점검 시작":maintenance.current==0?L"타르코프 서버 정상":maintenance.current==2?L"일부 불안정":maintenance.current==3?L"접속 불가":L"지금은 상태를 가져오지 못했어요. 잠시 뒤 다시 확인합니다.");
 textAt(dc,title,{18,52,r.right-18,84},smallBold,maintenance.current==1?danger:ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
 if(maintenance.current==1){textAt(dc,i18n::text(L"지금 타르코프 서버 점검 중입니다. 끝나면 다시 알려 드릴게요."),{18,88,r.right-18,152},smallFont,muted,DT_LEFT|DT_WORDBREAK);}EndPaint(h,&ps);return 0;}
 case WM_TIMER:{double t=std::min(1.0,(GetTickCount64()-maintenanceMenuAt)/180.0),e=1-(1-t)*(1-t)*(1-t);if(t>=1){KillTimer(h,1);SetWindowRgn(h,nullptr,TRUE);}else SetWindowRgn(h,CreateRoundRectRgn(0,0,321,std::max(2,(int)std::lround(164*e))+1,14,14),TRUE);return 0;}
 case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)DestroyWindow(h);return 0;
 case WM_KEYDOWN:if(w==VK_ESCAPE)DestroyWindow(h);return 0;
 case WM_DESTROY:maintenanceMenu=nullptr;InvalidateRect(GetDlgItem(win,87),nullptr,FALSE);return 0;
 }return DefWindowProcW(h,m,w,l);}
void showMaintenanceMenu(){if(maintenanceMenu){DestroyWindow(maintenanceMenu);return;}static bool registered=false;if(!registered){WNDCLASSW wc={};wc.lpfnWndProc=maintenanceProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshMaintenance";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);registered=true;}
 RECT b;GetWindowRect(GetDlgItem(win,87),&b);maintenanceMenu=CreateWindowExW(WS_EX_TOOLWINDOW,L"BlueAshMaintenance",L"",WS_POPUP,b.right-320,b.bottom+8,320,164,win,nullptr,GetModuleHandleW(nullptr),nullptr);if(!maintenanceMenu)return;
 maintenanceMenuAt=GetTickCount64();SetWindowRgn(maintenanceMenu,CreateRectRgn(0,0,320,2),FALSE);SetTimer(maintenanceMenu,1,10,nullptr);int round=2;DwmSetWindowAttribute(maintenanceMenu,33,&round,sizeof(round));COLORREF edge=line;DwmSetWindowAttribute(maintenanceMenu,34,&edge,sizeof(edge));ShowWindow(maintenanceMenu,SW_SHOW);SetForegroundWindow(maintenanceMenu);InvalidateRect(GetDlgItem(win,87),nullptr,FALSE);}
