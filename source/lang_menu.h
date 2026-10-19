#pragma once
// Language list under the top-right "KO ⌄" pill (button 82), drawn like the rest of BlueAsh: a rounded dark panel, each
// language in its own name with a short code on the right, the current one in cobalt, a soft row under the mouse.
// No search box (13 languages fit). Picking one does what the 설정 drop-down does: save and restart (WM_APP+179).
// It closes on a pick, on Esc, or when anything else is clicked (it loses activation). It opens by unrolling downward:
// the full-size window is revealed through a region that grows from a sliver to its full height in 180 ms (ease-out).
// Closing rolls it back up the same way (140 ms) before the window goes; a pick is applied once it has closed.
const wchar_t* const languageCodes[]={L"ko",L"zh-CN",L"ja",L"en",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};
const wchar_t* const languageNames[]={L"한국어",L"简体中文",L"日本語",L"English",L"Русский",L"Español",L"Português (Brasil)",L"Deutsch",L"Français",L"Italiano",L"Polski",L"Türkçe",L"繁體中文"};
const wchar_t* const languageShort[]={L"KO",L"ZH",L"JA",L"EN",L"RU",L"ES",L"PT",L"DE",L"FR",L"IT",L"PL",L"TR",L"TW"};
int languageHover=-1; // languageMenu: declared before drawButton (ModeSwitch.cpp)
ULONGLONG languageClosedAt=0;
const int langRow=40,langPad=8,langWidth=248,langFull=langPad*2+13*langRow,langOpenMs=180;
int langX=0,langY=0;ULONGLONG langOpened=0,langClosing=0;int langPick=-1;const int langCloseMs=140;
// Starts the roll-up (once); the window is destroyed and any pick applied when it ends (WM_TIMER).
void closeLanguageMenu(HWND h,int pick=-1){if(langClosing){return;}langClosing=GetTickCount64();langPick=pick;SetTimer(h,1,10,nullptr);}
int languageAt(int y){int i=(y-langPad)/langRow;return y>=langPad&&i>=0&&i<13?i:-1;}
void applyLanguage(int i){
    if(i<0||i>=13||i18n::languageCode()==languageCodes[i]){return;}std::wstring previous=i18n::languageCode();
    if(!saveString(configPath,L"Language",languageCodes[i])){problem(i18n::text(L"언어 설정을 저장하지 못했습니다."));return;}
    PostMessageW(win,WM_APP+179,1,(LPARAM)new std::wstring(previous)); // 1: reopen on the dashboard, not on 설정
}
LRESULT CALLBACK languageProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
        RECT full={0,0,r.right,langFull};HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,langFull);auto old=SelectObject(mem,bmp);FillRect(mem,&full,panelBrush);
        auto oldFont=SelectObject(mem,font);SetBkMode(mem,TRANSPARENT);
        for(int i=0;i<13;i++){RECT row={langPad,langPad+i*langRow,r.right-langPad,langPad+(i+1)*langRow};bool now=i18n::languageCode()==languageCodes[i];
            if(now)roundBox(mem,row,tint,tint,12);else if(i==languageHover)roundBox(mem,row,well,well,12);
            // Names are drawn as they are (not through i18n::text): each language in its own script.
            // Segoe UI here: Windows fills in the Chinese and Japanese letters for it (the built-in UI font has none).
            static HFONT names=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI"),namesBold=CreateFontW(-16,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
            SelectObject(mem,now?namesBold:names);SetTextColor(mem,now?RGB(120,160,255):ink);RECT name={row.left+14,row.top,row.right-50,row.bottom};DrawTextW(mem,languageNames[i],-1,&name,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
            SelectObject(mem,smallBold);SetTextColor(mem,now?RGB(120,160,255):muted);RECT code={row.right-54,row.top,row.right-14,row.bottom};DrawTextW(mem,languageShort[i],-1,&code,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
        SelectObject(mem,oldFont);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_TIMER:{double e;
        if(langClosing){double t=std::min(1.0,(GetTickCount64()-langClosing)/(double)langCloseMs);e=1-t*t*t; // ease-in back up
            if(t>=1){KillTimer(h,1);int pick=langPick;DestroyWindow(h);applyLanguage(pick);return 0;}}
        else{double t=std::min(1.0,(GetTickCount64()-langOpened)/(double)langOpenMs);e=1-(1-t)*(1-t)*(1-t);if(t>=1)KillTimer(h,1);}
        // The window stays at its full size and is revealed through a region: resizing it every frame left parts of the
        // list unpainted on scaled displays (2.21.2, GDI scaling).
        if(!langClosing&&e>=1){SetWindowRgn(h,nullptr,TRUE);}else{SetWindowRgn(h,CreateRoundRectRgn(0,0,langWidth+1,std::max(2,(int)std::lround(langFull*e))+1,14,14),TRUE);}return 0;}
    case WM_MOUSEMOVE:{int i=languageAt((short)HIWORD(l));if(i!=languageHover){languageHover=i;InvalidateRect(h,nullptr,FALSE);}TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_MOUSELEAVE:languageHover=-1;InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_SETCURSOR:if(languageHover>=0){SetCursor(LoadCursor(nullptr,IDC_HAND));return TRUE;}break;
    case WM_LBUTTONUP:closeLanguageMenu(h,languageAt((short)HIWORD(l)));return 0;
    case WM_KEYDOWN:if(w==VK_ESCAPE)closeLanguageMenu(h);return 0;
    case WM_ACTIVATE:if(LOWORD(w)==WA_INACTIVE)closeLanguageMenu(h);return 0;
    case WM_DESTROY:if(languageMenu==h){languageMenu=nullptr;languageClosedAt=GetTickCount64();InvalidateRect(GetDlgItem(win,82),nullptr,TRUE);}return 0;
    }return DefWindowProcW(h,m,w,l);
}
void showLanguageMenu(){
    // A second click on the pill closes the list (that click already took activation away and closed it).
    if(languageMenu){closeLanguageMenu(languageMenu);return;}if(GetTickCount64()-languageClosedAt<400){return;}
    static bool registered=false;if(!registered){WNDCLASSW wc={};wc.lpfnWndProc=languageProc;/* no CS_DROPSHADOW: it would show at full size ahead of the unrolling rows */wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshLanguage";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);registered=true;}
    RECT b;GetWindowRect(GetDlgItem(win,82),&b);languageHover=-1;langClosing=0;langPick=-1;langX=b.right-langWidth;langY=b.bottom+8;langOpened=GetTickCount64();
    languageMenu=CreateWindowExW(WS_EX_TOOLWINDOW,L"BlueAshLanguage",L"",WS_POPUP,langX,langY,langWidth,langFull,win,nullptr,GetModuleHandleW(nullptr),nullptr);if(!languageMenu)return;
    SetWindowRgn(languageMenu,CreateRectRgn(0,0,langWidth,2),FALSE);SetTimer(languageMenu,1,10,nullptr);
    int round=2;DwmSetWindowAttribute(languageMenu,33,&round,sizeof(round));COLORREF edge=line;DwmSetWindowAttribute(languageMenu,34,&edge,sizeof(edge));
    ShowWindow(languageMenu,SW_SHOW);SetForegroundWindow(languageMenu);InvalidateRect(GetDlgItem(win,82),nullptr,TRUE);
}
