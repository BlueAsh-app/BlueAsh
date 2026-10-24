#pragma once
// Tray right-click menu, modelled on Riot Vanguard's: small and quiet. Still a Windows menu (submenus, keyboard and
// closing behave as usual), but every item is drawn by BlueAsh (owner-draw, WM_MEASUREITEM / WM_DRAWITEM of the main
// window): a near-black panel, the BlueAsh mark and name on top, right under it one "게임 모드" row with an on/off switch
// (a click flips the mode), soft grey items that brighten under the mouse, small cobalt checks for switched-on features,
// thin separators and BlueAsh's own submenu chevron (the system arrow is clipped away).
struct TrayItem{int kind;std::wstring text;bool checked,indent;}; // kind: 0 header, 1 item, 2 separator, 3 submenu, 5 switch row
std::deque<TrayItem> trayItems; // the open menu's items (dwItemData points into it; a deque keeps the pointers)
const COLORREF trayBack=RGB(18,18,20),trayHover=RGB(36,36,40),trayText=RGB(188,188,194),trayBright=RGB(240,240,244),trayDim=RGB(116,116,124),trayRule=RGB(42,42,46),trayOff=RGB(64,64,70);
HBRUSH trayBrush=nullptr;
HFONT trayFont(int kind){
    static HFONT item=CreateFontW(-13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,i18n::fontFamily()),
        head=CreateFontW(-14,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,i18n::fontFamily()),
        icons=[]{LOGFONTW lf={};GetObjectW(iconFont,sizeof(lf),&lf);lf.lfHeight=-11;return CreateFontIndirectW(&lf);}();
    return kind==0?head:kind==9?icons:item;
}
const int trayLeft=30,trayFlush=14,trayRight=30; // text starts at trayLeft in submenus (room for a check), else at trayFlush
void trayMeasure(MEASUREITEMSTRUCT* m){auto* it=(TrayItem*)m->itemData;if(!it){return;}
    if(it->kind==2){m->itemHeight=9;m->itemWidth=0;return;}
    HDC dc=GetDC(win);SIZE a={};auto old=SelectObject(dc,trayFont(it->kind));GetTextExtentPoint32W(dc,it->text.c_str(),(int)it->text.size(),&a);SelectObject(dc,old);ReleaseDC(win,dc);
    int w=(it->kind==0?40:it->indent?trayLeft:trayFlush)+a.cx+(it->kind==5?60:trayRight);
    // Windows adds its check-mark column to an owner-drawn item's width; take it back so the menu is as wide as drawn.
    m->itemWidth=std::max(180,w)-(GetSystemMetrics(SM_CXMENUCHECK)-1);m->itemHeight=it->kind==0?38:28;
}
// Switch animation: the knob position 0 (normal) .. 1 (game) while it slides (trayAnimating), redrawn by a thread timer
// that the menu's own message loop dispatches.
TrayItem* traySwitch=nullptr;int trayRow=-1; // the switch row of the open menu and its position
bool trayAnimating=false,trayPending=false;double trayKnob=0,trayKnobFrom=0;ULONGLONG trayAnimAt=0;UINT_PTR trayTimer=0;HWND trayMenuHwnd=nullptr;
const int traySlideMs=180;
void trayDrawTo(DRAWITEMSTRUCT* ds){auto* it=(TrayItem*)ds->itemData;HDC dc=ds->hDC;RECT r=ds->rcItem;int cy=(r.top+r.bottom)/2;
    FillRect(dc,&r,trayBrush);SetBkMode(dc,TRANSPARENT);
    if(it->kind==2){RECT rule={r.left+10,cy,r.right-10,cy+1};HBRUSH b=CreateSolidBrush(trayRule);FillRect(dc,&rule,b);DeleteObject(b);return;}
    if(it->kind==0){
        if(!painting::drawLogo(dc,{r.left+trayFlush,cy-9,r.left+trayFlush+18,cy+9}))DrawIconEx(dc,r.left+trayFlush,cy-9,logoIcon?logoIcon:icons[0],18,18,0,nullptr,DI_NORMAL);
        textAt(dc,it->text,{r.left+trayFlush+26,r.top,r.right-10,r.bottom},trayFont(0),trayBright);return;}
    bool on=(ds->itemState&ODS_SELECTED)&&(it->kind==5||!(ds->itemState&ODS_DISABLED)); // the switch row is "disabled" only so a click keeps the menu open
    if(on){RECT h={r.left+4,r.top+1,r.right-4,r.bottom-1};roundBox(dc,h,trayHover,trayHover,8);}
    if(it->kind==5){ // the switch: cobalt track with the knob right when on, grey with the knob left when off; slides in between
        double p=trayAnimating?trayKnob:(it->checked?1:0);auto mix=[&](int a,int b){return (int)std::lround(a+(b-a)*p);};COLORREF c=RGB(mix(GetRValue(trayOff),GetRValue(blue)),mix(GetGValue(trayOff),GetGValue(blue)),mix(GetBValue(trayOff),GetBValue(blue)));
        RECT track={r.right-12-32,cy-9,r.right-12,cy+9};roundBox(dc,track,c,c,18);
        int k=(int)std::lround(track.left+2+(track.right-16-(track.left+2))*p);if(!painting::circle(dc,{k,cy-7,k+14,cy+7},RGB(255,255,255),CLR_NONE)){HBRUSH kb=CreateSolidBrush(RGB(255,255,255));auto ob=SelectObject(dc,kb);auto op=SelectObject(dc,GetStockObject(NULL_PEN));Ellipse(dc,k,cy-7,k+15,cy+8);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(kb);}}
    else if(it->checked){glyph(dc,0xE73E,{r.left+6,r.top,r.left+trayLeft-2,r.bottom},trayFont(9),blue);}
    if(it->kind==3){glyph(dc,0xE76C,{r.right-trayRight,r.top,r.right-6,r.bottom},trayFont(9),on?trayBright:trayDim);}
    textAt(dc,it->text,{r.left+(it->indent?trayLeft:trayFlush),r.top,r.right-(it->kind==5?56:trayRight),r.bottom},trayFont(1),on?trayBright:trayText,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS);
}
// Each row is drawn into a memory bitmap and copied in one go (as drawButton does), so no half-drawn row shows.
void trayDraw(DRAWITEMSTRUCT* ds){auto* it=(TrayItem*)ds->itemData;RECT r=ds->rcItem;int w=r.right-r.left,hh=r.bottom-r.top;if(!it||w<=0||hh<=0){return;}
    HDC mem=CreateCompatibleDC(ds->hDC);HBITMAP bmp=mem?CreateCompatibleBitmap(ds->hDC,w,hh):nullptr;
    if(!bmp){if(mem)DeleteDC(mem);trayDrawTo(ds);}
    else{auto old=SelectObject(mem,bmp);DRAWITEMSTRUCT copy=*ds;copy.hDC=mem;OffsetRect(&copy.rcItem,-r.left,-r.top);trayDrawTo(&copy);
        BitBlt(ds->hDC,r.left,r.top,w,hh,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);}
    // Keep Windows' own submenu arrow (drawn after this, black on black) out of the item.
    if(it->kind==3){ExcludeClipRect(ds->hDC,r.right-trayRight,r.top,r.right,r.bottom);}
}
// Owner-drawn menus come up square with a light edge on Windows 11; give each menu window (WM_ENTERIDLE names it) round
// corners and a dark edge like the rest of BlueAsh. Older Windows ignores both.
std::set<HWND> trayStyled; // menu windows already given round corners (cleared per menu)
HWND trayRootWindow=nullptr; // the top menu's window (the first one WM_ENTERIDLE names)
void trayMenuWindow(HWND m){if(!m||!trayStyled.insert(m).second){return;}if(!trayRootWindow){trayRootWindow=m;}int round=2;COLORREF edge=RGB(48,48,54);
    DwmSetWindowAttribute(m,33,&round,sizeof(round));DwmSetWindowAttribute(m,34,&edge,sizeof(edge));}
std::wstring trimmed(std::wstring s){while(!s.empty()&&s.back()==L' ')s.pop_back();return s;}
HMENU trayRoot=nullptr; // the top menu of the open tray menu
void trayAdd(HMENU m,int kind,UINT_PTR id,const std::wstring& text,bool checked=false){
    trayItems.push_back({kind,text,checked,m!=trayRoot});UINT f=MF_OWNERDRAW|(kind==0||kind==5?MF_DISABLED:kind==2?MF_SEPARATOR:kind==3?MF_POPUP:0);
    AppendMenuW(m,f,id,(LPCWSTR)&trayItems.back());
}
// The switch row flips the mode without closing the menu: it is a disabled item (a click on it does not end the menu), and a
// message filter hook on the menu loop catches the click (or Enter / Space on it) and slides the knob; the mode itself
// changes once the slide ends (applying it blocks for a moment), or when the menu closes first.
void trayApply(){if(!trayPending||!traySwitch){return;}trayPending=false;if(traySwitch->checked!=fps&&!changeMode(traySwitch->checked)){showWindow();}traySwitch->checked=fps;}
// Menu windows ignore InvalidateRect for owner-drawn rows, so the switch row is drawn straight onto the menu window.
void trayRedrawSwitch(){if(!trayMenuHwnd||!traySwitch){return;}RECT row;if(!GetMenuItemRect(trayMenuHwnd,trayRoot,trayRow,&row)){return;}
    MapWindowPoints(nullptr,trayMenuHwnd,(POINT*)&row,2);HDC dc=GetDC(trayMenuHwnd);if(!dc){return;}
    DRAWITEMSTRUCT ds={};ds.CtlType=ODT_MENU;ds.itemAction=ODA_DRAWENTIRE;ds.itemState=ODS_DISABLED|((GetMenuState(trayRoot,trayRow,MF_BYPOSITION)&MF_HILITE)?ODS_SELECTED:0);
    ds.hwndItem=(HWND)trayRoot;ds.hDC=dc;ds.rcItem=row;ds.itemData=(ULONG_PTR)traySwitch;trayDraw(&ds);ReleaseDC(trayMenuHwnd,dc);}
void CALLBACK trayTick(HWND,UINT,UINT_PTR,DWORD){double t=std::min(1.0,(GetTickCount64()-trayAnimAt)/(double)traySlideMs),e=1-(1-t)*(1-t)*(1-t);
    double to=traySwitch&&traySwitch->checked?1:0;trayKnob=trayKnobFrom+(to-trayKnobFrom)*e;trayRedrawSwitch();
    if(t>=1){KillTimer(win,trayTimer);trayTimer=0;trayAnimating=false;trayApply();trayRedrawSwitch();}}
void trayFlip(HWND menuWindow){if(!traySwitch){return;}trayKnobFrom=trayAnimating?trayKnob:(traySwitch->checked?1:0);traySwitch->checked=!traySwitch->checked;trayPending=true;
    trayAnimating=true;trayAnimAt=GetTickCount64();trayMenuHwnd=menuWindow;if(!trayTimer){trayTimer=SetTimer(win,0xB1B,10,trayTick);} /* on the main window: the menu loop does not run window-less timers */}
LRESULT CALLBACK trayFilter(int code,WPARAM w,LPARAM l){
    if(code==MSGF_MENU&&traySwitch){MSG* m=(MSG*)l;
        RECT row={};bool click=m->message==WM_LBUTTONUP&&GetMenuItemRect(trayRootWindow,trayRoot,trayRow,&row)&&PtInRect(&row,m->pt);
        bool key=m->message==WM_KEYDOWN&&(m->wParam==VK_RETURN||m->wParam==VK_SPACE)&&(GetMenuState(trayRoot,trayRow,MF_BYPOSITION)&MF_HILITE);
        if((click||key)&&trayRootWindow){trayFlip(trayRootWindow);return 1;}} // m->hwnd is not the menu window (mouse messages name the owner)
    return CallNextHookEx(nullptr,code,w,l);}
void trayMenu(){
    if(editor){showWindow();return;}
    if(!trayBrush){trayBrush=CreateSolidBrush(trayBack);}trayItems.clear();trayStyled.clear();trayRootWindow=nullptr;
    HMENU menu=CreatePopupMenu(),games=CreatePopupMenu(),tarkov=CreatePopupMenu(),pubg=CreatePopupMenu();trayRoot=menu;
    trayAdd(menu,0,0,L"BlueAsh");trayAdd(menu,2,0,L"");
    // One switch row: on = game mode (trayFilter flips it).
    trayRow=GetMenuItemCount(menu);trayAdd(menu,5,0,trimmed(i18n::text(L"게임 모드      ")),fps);traySwitch=&trayItems.back();trayAnimating=trayPending=false;
    trayAdd(menu,2,0,L"");
    trayAdd(menu,1,40,i18n::text(L"대시보드"));trayAdd(menu,1,32,i18n::text(L"프로필"));
    for(int id:{33,37,39}){trayAdd(tarkov,1,id,featureTitle(id),featureOn(id));}trayAdd(tarkov,1,46,featureTitle(46));
    trayAdd(pubg,1,47,featureTitle(47),pubgJoinOn);trayAdd(pubg,1,48,featureTitle(48),pubgMapOn);
    trayAdd(games,3,(UINT_PTR)tarkov,i18n::text(L"타르코프"));trayAdd(games,3,(UINT_PTR)pubg,i18n::text(L"배그"));
    trayAdd(menu,3,(UINT_PTR)games,i18n::text(L"게임 도우미"));trayAdd(menu,1,44,i18n::text(L"알림"));
    trayAdd(menu,1,38,updateReady?i18n::text(L"업데이트 · 새 버전 ")+updateVersion:std::wstring(i18n::text(L"업데이트")));
    trayAdd(menu,1,50,i18n::text(L"설정"));
    trayAdd(menu,2,0,L"");
    trayAdd(menu,1,31,i18n::text(L"완전 종료 (일반 모드로 복귀)"));
    MENUINFO mi={};mi.cbSize=sizeof(mi);mi.fMask=MIM_BACKGROUND|MIM_APPLYTOSUBMENUS;mi.hbrBack=trayBrush;SetMenuInfo(menu,&mi);
    POINT p;GetCursorPos(&p);SetForegroundWindow(win);
    HHOOK hook=SetWindowsHookExW(WH_MSGFILTER,trayFilter,nullptr,GetCurrentThreadId());
    int command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,win,nullptr);
    if(hook){UnhookWindowsHookEx(hook);}if(trayTimer){KillTimer(win,trayTimer);trayTimer=0;}trayAnimating=false;trayMenuHwnd=nullptr;trayApply(); // closed mid-slide
    DestroyMenu(menu);PostMessageW(win,WM_NULL,0,0);traySwitch=nullptr;trayRow=-1;trayRootWindow=nullptr;trayItems.clear();trayStyled.clear();
    if(command)SendMessageW(win,WM_COMMAND,command,0);
}
