#pragma once
// "?" buttons beside a dashboard panel title (75 화면과 소리, 76 게임 알림): a small card under the button explains
// the panel. It closes when clicked, on Esc, or as soon as anything else is clicked (it loses activation).
std::wstring helpTitle,helpBody;ULONGLONG helpClosedAt=0; // helpCard, helpFor: declared before drawButton (ModeSwitch.cpp)
std::wstring helpText(int id,std::wstring& title){
    if(id==75){title=i18n::text(L"화면과 소리");
        return std::wstring(i18n::text(L"일반 모드와 게임 모드마다 고른 모니터의 감마와 고른 브라우저의 소리 크기를 따로 기억합니다."))+L"\n\n"
            +i18n::text(L"모드 카드나 단축키를 누르면 한 번에 바뀌고, BlueAsh를 완전히 끄면 일반 모드로 돌아갑니다.")+L"\n\n"
            +i18n::text(L"밝기·대비·선명도·색조와 게임별 프로필은 세부 설정에서 정합니다.");}
    title=i18n::text(L"게임 알림");
    return std::wstring(i18n::text(L"타르코프 · 배치 카운트다운 1초 전에 소리로 알려 줍니다."))+L"\n\n"
        +i18n::text(L"배그 · 비행기 탑승 카운트다운이 끝나면 알려 줍니다. 배그 창을 보고 있을 때는 울리지 않습니다.")+L"\n\n"
        +i18n::text(L"자리 비움 알림 · 게임이 시작됐는데 PC 앞에 없으면 휴대폰으로 알려 줍니다.");
}
LRESULT CALLBACK helpProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
        // The height measured before the card existed can come out short: measure again on this DC and grow to fit.
        {RECT need={0,0,r.right-40,0};auto old=SelectObject(dc,smallFont);DrawTextW(dc,helpBody.c_str(),-1,&need,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);
         if(need.bottom+70>r.bottom){EndPaint(h,&ps);SetWindowPos(h,nullptr,0,0,r.right,need.bottom+70,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);InvalidateRect(h,nullptr,FALSE);return 0;}}
        FillRect(dc,&r,panelBrush);
        textAt(dc,helpTitle,{20,16,r.right-20,42},boldFont,ink);RECT b={20,50,r.right-20,r.bottom-16};textAt(dc,helpBody,b,smallFont,muted,DT_LEFT|DT_WORDBREAK);
        EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)DestroyWindow(h);return 0;
    case WM_LBUTTONDOWN:case WM_KEYDOWN:DestroyWindow(h);return 0;
    case WM_DESTROY:if(helpCard==h){helpCard=nullptr;helpClosedAt=GetTickCount64();for(int id:{75,76})InvalidateRect(GetDlgItem(win,id),nullptr,TRUE);}return 0;
    }return DefWindowProcW(h,m,w,l);
}
void showHelp(int id){
    // Clicking the same "?" again closes the card: the click itself already took activation away and closed it.
    if(helpCard){bool same=helpFor==id;DestroyWindow(helpCard);if(same)return;}else if(helpFor==id&&GetTickCount64()-helpClosedAt<400){helpFor=0;return;}
    static bool registered=false;if(!registered){WNDCLASSW wc={};wc.style=CS_DROPSHADOW;wc.lpfnWndProc=helpProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshHelp";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);registered=true;}
    helpBody=helpText(id,helpTitle);helpFor=id;
    // Height from the wrapped text; the card's right edge lines up with the button, just below it.
    const int width=380;HDC dc=GetDC(win);RECT calc={0,0,width-40,0};auto old=SelectObject(dc,smallFont);DrawTextW(dc,helpBody.c_str(),-1,&calc,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,old);ReleaseDC(win,dc);
    RECT b;GetWindowRect(GetDlgItem(win,id),&b);int height=calc.bottom+66;int x=b.right-width,y=b.bottom+8;
    helpCard=CreateWindowExW(WS_EX_TOOLWINDOW,L"BlueAshHelp",L"",WS_POPUP,x,y,width,height,win,nullptr,GetModuleHandleW(nullptr),nullptr);if(!helpCard)return;
    int round=2;DwmSetWindowAttribute(helpCard,33,&round,sizeof(round));COLORREF edge=line;DwmSetWindowAttribute(helpCard,34,&edge,sizeof(edge));
    ShowWindow(helpCard,SW_SHOW);SetForegroundWindow(helpCard);
}
// 완전 종료 from the sidebar asks once (it sits beside 설정 and 버그 제보, and quitting stops every alert): a small card
// above the button with 취소 / 종료. Enter quits, Esc or a click elsewhere cancels. The tray menu's 완전 종료 quits directly.
HWND quitCard=nullptr;int quitHover=0;ULONGLONG quitClosedAt=0;
RECT quitButton(HWND h,int which){RECT r;GetClientRect(h,&r);return which==1?RECT{r.right-20-110,r.bottom-20-38,r.right-20,r.bottom-20}:RECT{r.right-20-110-10-96,r.bottom-20-38,r.right-20-110-10,r.bottom-20};}
LRESULT CALLBACK quitProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);FillRect(dc,&r,panelBrush);
        textAt(dc,i18n::text(L"BlueAsh를 끌까요?"),{22,18,r.right-22,44},boldFont,ink);
        textAt(dc,i18n::text(L"끄면 게임 알림·지도·시세가 모두 멈춥니다."),{22,50,r.right-22,r.bottom-70},smallFont,muted,DT_LEFT|DT_WORDBREAK);
        RECT c=quitButton(h,0),q=quitButton(h,1);roundBox(dc,c,quitHover==2?well:panel,line,12);textAt(dc,i18n::text(L"취소"),c,font,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        COLORREF red=quitHover==1?RGB(235,72,88):RGB(214,58,74);roundBox(dc,q,red,red,12);textAt(dc,i18n::text(L"종료"),q,boldFont,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEMOVE:{POINT p={(short)LOWORD(l),(short)HIWORD(l)};RECT c=quitButton(h,0),q=quitButton(h,1);int now=PtInRect(&q,p)?1:PtInRect(&c,p)?2:0;
        if(now!=quitHover){quitHover=now;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_MOUSELEAVE:if(quitHover){quitHover=0;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_SETCURSOR:{POINT p;GetCursorPos(&p);ScreenToClient(h,&p);RECT c=quitButton(h,0),q=quitButton(h,1);SetCursor(LoadCursorW(nullptr,PtInRect(&q,p)||PtInRect(&c,p)?IDC_HAND:IDC_ARROW));return TRUE;}
    case WM_LBUTTONUP:{POINT p={(short)LOWORD(l),(short)HIWORD(l)};RECT q=quitButton(h,1),c=quitButton(h,0);if(PtInRect(&q,p)){DestroyWindow(h);quit();}else if(PtInRect(&c,p))DestroyWindow(h);return 0;}
    case WM_KEYDOWN:if(w==VK_RETURN){DestroyWindow(h);quit();}else if(w==VK_ESCAPE)DestroyWindow(h);return 0;
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)DestroyWindow(h);return 0;
    case WM_DESTROY:if(quitCard==h){quitCard=nullptr;quitHover=0;quitClosedAt=GetTickCount64();}return 0;
    }return DefWindowProcW(h,m,w,l);
}
void confirmQuit(){
    // A second click on 완전 종료 while the card is up just closes it (that click already took its focus away).
    if(quitCard){DestroyWindow(quitCard);return;}if(GetTickCount64()-quitClosedAt<400)return;
    static bool registered=false;if(!registered){WNDCLASSW wc={};wc.style=CS_DROPSHADOW;wc.lpfnWndProc=quitProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshQuit";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);registered=true;}
    RECT b;GetWindowRect(GetDlgItem(win,31),&b);const int width=360,height=150;int x=b.left-8,y=b.top-height-10;
    quitCard=CreateWindowExW(WS_EX_TOOLWINDOW,L"BlueAshQuit",L"",WS_POPUP,x,y,width,height,win,nullptr,GetModuleHandleW(nullptr),nullptr);if(!quitCard)return;
    int round=2;DwmSetWindowAttribute(quitCard,33,&round,sizeof(round));COLORREF edge=line;DwmSetWindowAttribute(quitCard,34,&edge,sizeof(edge));
    ShowWindow(quitCard,SW_SHOW);SetForegroundWindow(quitCard);SetFocus(quitCard);
}
