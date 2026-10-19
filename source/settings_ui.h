#pragma once
#include "i18n.h"
// Included after the shared Win32 drawing helpers.
HWND addSetting(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int height,int id=0){
    if(!_wcsicmp(cls,L"COMBOBOX"))style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS; // items drawn in the theme (drawComboItem)
    HWND c=CreateWindowExW(0,cls,i18n::text(text),WS_CHILD|WS_VISIBLE|style,x,y,w,height,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);darkControl(c,cls);return c;
}
// Page building blocks shared by every settings page (a child 736x622 under the main header; content x 24~712).
// Label kinds: 0 text, 1 muted hint (small), 2 section heading (bold). WM_CTLCOLORSTATIC reads the kind back (pageStatic).
HWND pageLabel(HWND parent,const wchar_t* text,int x,int y,int w,int height,int kind=0,int id=0){
    HWND c=addSetting(parent,L"STATIC",text,0,x,y,w,height,id);SetWindowLongPtrW(c,GWLP_USERDATA,kind);
    if(kind){SendMessageW(c,WM_SETFONT,(WPARAM)(kind==1?smallFont:boldFont),TRUE);}return c;
}
// Bottom-row buttons sit at y 556; the page's main action (저장) is marked primary and drawn cobalt.
HWND pageButton(HWND parent,const wchar_t* text,int x,int y,int w,int id,bool primary=false){
    HWND c=addSetting(parent,L"BUTTON",text,BS_OWNERDRAW|WS_TABSTOP,x,y,w,40,id);if(primary)SetWindowLongPtrW(c,GWLP_USERDATA,1);return c;
}
LRESULT pageStatic(WPARAM wp,LPARAM lp){HDC dc=(HDC)wp;SetTextColor(dc,GetWindowLongPtrW((HWND)lp,GWLP_USERDATA)==1?muted:ink);SetBkColor(dc,panel);return staticBackground(dc,(HWND)lp);}
// Switches (2.21): owner-drawn buttons whose state is GWLP_USERDATA 2 (off) / 3 (on); drawButton draws the track and the
// label. checkBox/checked work for them and for ordinary check boxes.
HWND pageSwitch(HWND parent,const wchar_t* text,int x,int y,int w,int height,int id){HWND c=addSetting(parent,L"BUTTON",text,BS_OWNERDRAW|WS_TABSTOP,x,y,w,height,id);SetWindowLongPtrW(c,GWLP_USERDATA,2);return c;}
bool isSwitch(HWND c){LONG_PTR v=GetWindowLongPtrW(c,GWLP_USERDATA);return (v==2||v==3)&&(GetWindowLongW(c,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW;}
void checkBox(HWND h,int id,bool on){HWND c=GetDlgItem(h,id);if(c&&isSwitch(c)){if(GetWindowLongPtrW(c,GWLP_USERDATA)!=(on?3:2)){SetWindowLongPtrW(c,GWLP_USERDATA,on?3:2);InvalidateRect(c,nullptr,FALSE);}return;}SendDlgItemMessageW(h,id,BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);}
bool checked(HWND h,int id){HWND c=GetDlgItem(h,id);if(c&&isSwitch(c))return GetWindowLongPtrW(c,GWLP_USERDATA)==3;return SendDlgItemMessageW(h,id,BM_GETCHECK,0,0)==BST_CHECKED;}
// Profile sliders (2.21): each mirrors the edit box id-100. It reads the value from the box and writes it back when
// dragged or moved with the arrow keys; an empty box (the channels differ) shows no knob.
namespace valueSlider{
struct Range{int low,high;bool gamma;};
inline Range range(int id){switch(id){case 150:case 151:return {0,199,false};case 152:return {50,300,true};case 154:return {0,359,false};default:return {0,100,false};}}
inline bool read(HWND s,int& v){wchar_t b[32]={};GetDlgItemTextW(GetParent(s),GetDlgCtrlID(s)-100,b,32);unsigned u=0;auto r=range(GetDlgCtrlID(s));if(!(r.gamma?settings::parseGamma(b,u):settings::integer(b,u)))return false;v=std::clamp((int)u,r.low,r.high);return true;}
inline void write(HWND s,int v){auto r=range(GetDlgCtrlID(s));v=std::clamp(v,r.low,r.high);SetDlgItemTextW(GetParent(s),GetDlgCtrlID(s)-100,(r.gamma?settings::gammaText((unsigned)v):std::to_wstring(v)).c_str());}
constexpr int pad=9;
inline int at(HWND s,int x){RECT c;GetClientRect(s,&c);auto r=range(GetDlgCtrlID(s));int span=std::max(1,(int)c.right-2*pad);return r.low+(int)std::lround((double)(x-pad)*(r.high-r.low)/span);}
inline LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,std::max(1L,c.right),std::max(1L,c.bottom));auto o=SelectObject(mem,bmp);FillRect(mem,&c,panelBrush);
  int cy=c.bottom/2,v=0;bool has=read(h,v);auto r=range(GetDlgCtrlID(h));int x=pad+(int)((long long)(v-r.low)*(c.right-2*pad)/std::max(1,r.high-r.low));
  roundBox(mem,{pad,cy-2,c.right-pad,cy+3},well,well,5);
  if(has){if(x>pad){roundBox(mem,{pad,cy-2,x,cy+3},blue,blue,5);}painting::circle(mem,{x-8,cy-8,x+8,cy+8},ink,GetFocus()==h?blue:CLR_NONE,2.f);}
  BitBlt(dc,0,0,c.right,c.bottom,mem,0,0,SRCCOPY);SelectObject(mem,o);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
 if(m==WM_ERASEBKGND)return 1;
 if(m==WM_GETDLGCODE)return DLGC_WANTARROWS;
 if(m==WM_LBUTTONDOWN){SetFocus(h);SetCapture(h);}
 if(m==WM_LBUTTONDOWN||(m==WM_MOUSEMOVE&&GetCapture()==h)){write(h,at(h,(short)LOWORD(l)));InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_LBUTTONUP&&GetCapture()==h){ReleaseCapture();return 0;}
 if(m==WM_KEYDOWN&&(w==VK_LEFT||w==VK_RIGHT)){int v=0;auto r=range(GetDlgCtrlID(h));if(!read(h,v)){int id=GetDlgCtrlID(h);v=id>=150&&id<=152?100:r.low;}/* empty box: brightness/contrast 100, gamma 1.00, others their minimum */write(h,v+(w==VK_RIGHT?1:-1));InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_SETFOCUS||m==WM_KILLFOCUS)InvalidateRect(h,nullptr,FALSE);
 return DefWindowProcW(h,m,w,l);
}
inline void registerClass(){static bool done=false;if(done){return;}done=true;WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshValueSlider";wc.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&wc);}
}
settings::Profiles draft;
int editProfile=0,editChannel=0;
bool filling=false,colorDirty[3]={};
void fillColorEditor(HWND h){
    filling=true;const auto& p=draft[editProfile];int c=editChannel?editChannel-1:0;
    SetDlgItemTextW(h,50,std::to_wstring(p.brightness[c]).c_str());
    SetDlgItemTextW(h,51,std::to_wstring(p.contrast[c]).c_str());
    SetDlgItemTextW(h,52,settings::gammaText(settings::channelGamma(p,c)).c_str());
    if(!editChannel)for(int i=0;i<3;i++){
        bool mixed=i==0?(p.brightness[0]!=p.brightness[1]||p.brightness[0]!=p.brightness[2]):i==1?(p.contrast[0]!=p.contrast[1]||p.contrast[0]!=p.contrast[2]):(p.gamma!=p.greenGamma||p.gamma!=p.blueGamma);
        if(mixed){SetDlgItemTextW(h,50+i,L"");SendDlgItemMessageW(h,50+i,EM_SETCUEBANNER,TRUE,(LPARAM)i18n::text(L"채널별 값 다름"));}
    }
    SetDlgItemTextW(h,53,std::to_wstring(p.vibrance).c_str());
    SetDlgItemTextW(h,54,std::to_wstring(p.hue).c_str());
    SetDlgItemTextW(h,55,std::to_wstring(p.volume).c_str());
    checkBox(h,57,p.extended);
    for(auto& dirty:colorDirty){dirty=false;}
    filling=false;
}
bool collectColorEditor(HWND h){
    auto p=draft[editProfile];unsigned values[3]={};
    for(int i=0;i<3;i++)if(colorDirty[i]){
        bool ok=i==2?settings::parseGamma(field(h,50+i),values[i]):settings::integer(field(h,50+i),values[i])&&values[i]<=199;
        if(!ok){SetDlgItemTextW(h,59,i18n::text(L"밝기·대비는 0~199, 감마는 0.50~3.00 사이로 넣어 주세요."));return false;}
    }
    if(!settings::parseVolume(field(h,53),p.vibrance)||!settings::integer(field(h,54),p.hue)||p.hue>359||!settings::parseVolume(field(h,55),p.volume)){
        SetDlgItemTextW(h,59,i18n::text(L"선명도·볼륨은 0~100, 색조는 0~359 사이로 넣어 주세요."));return false;
    }
    for(int c=0;c<3;c++)if(!editChannel||c==editChannel-1){
        if(colorDirty[0])p.brightness[c]=values[0];
        if(colorDirty[1])p.contrast[c]=values[1];
        if(colorDirty[2])settings::setGamma(p,c,values[2]);
    }
    p.extended=checked(h,57);if(!p.extended)settings::allGamma(p,p.gamma);
    draft[editProfile]=p;return true;
}
void storeColorEditor(HWND h){
    if(!collectColorEditor(h))return;
    bool automatic=checked(h,58);
    if(automatic){draft[0].extended=true;draft[2].extended=true;draft[3].extended=true;}
    if(!saveString(configPath,L"ProfilesV2",settings::encodeV2(draft,automatic))){SetDlgItemTextW(h,59,i18n::text(L"저장하지 못했습니다. 기존 설정은 그대로입니다."));return;}
    profiles=draft;autoGame=automatic;fillColorEditor(h);
    for(int id:{1,2,61,62}){InvalidateRect(GetDlgItem(win,id),nullptr,TRUE);}
    bool ok=!active||changeMode(fps,appliedProfileIndex);
    SetDlgItemTextW(h,59,ok?(active?i18n::text(L"저장했습니다. 지금 모드에도 바로 적용했습니다."):i18n::text(L"저장했습니다. 모드를 바꿀 때 적용됩니다.")):i18n::text(L"저장은 했지만 적용하지 못했습니다. 컨트롤 센터의 오류 문구를 확인하세요."));
}
// 모드 설정 has two tabs, each its own page: switching asks the main window to open the other one (90 -> 32, 91 -> 34).
// The tabs sit above the page's panels, on the window backdrop (2.21).
void modeTabs(HWND h){addSetting(h,L"BUTTON",i18n::text(L"컬러 & 사운드"),BS_OWNERDRAW|WS_TABSTOP,12,2,180,38,90);addSetting(h,L"BUTTON",i18n::text(L"단축키"),BS_OWNERDRAW|WS_TABSTOP,202,2,150,38,91);}
// Both tab pages paint the backdrop and their own panels (no picture behind them), off-screen and copied once.
void pagePanels(HWND h,HDC target,const std::function<void(HDC)>& draw){
    RECT r={};GetClientRect(h,&r);HDC dc=CreateCompatibleDC(target);HBITMAP bmp=CreateCompatibleBitmap(target,std::max(1L,r.right),std::max(1L,r.bottom));auto old=SelectObject(dc,bmp);
    POINT o={};MapWindowPoints(h,win,&o,1);if(!copyBackdrop(dc,r,o)){FillRect(dc,&r,backgroundBrush);}draw(dc);
    BitBlt(target,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bmp);DeleteDC(dc);
}
// The status line (59) of the tab pages sits on the backdrop below the panels. The label stays hidden and only keeps
// the text (SetDlgItemTextW as before); setting it repaints that spot of the page, which draws the text (drawStatus).
WNDPROC statusBase=nullptr;
LRESULT CALLBACK statusProc(HWND h,UINT m,WPARAM w,LPARAM l){LRESULT r=CallWindowProcW(statusBase,h,m,w,l);if(m==WM_SETTEXT){RECT q;GetWindowRect(h,&q);MapWindowPoints(nullptr,GetParent(h),(POINT*)&q,2);InvalidateRect(GetParent(h),&q,TRUE);}return r;}
void canvasStatus(HWND page){HWND c=pageLabel(page,L"",204,556,356,40,1,59);ShowWindow(c,SW_HIDE);WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)statusProc);if(!statusBase)statusBase=old;}
void drawStatus(HWND page,HDC dc){wchar_t t[512]={};GetDlgItemTextW(page,59,t,512);if(*t)textAt(dc,t,{204,558,560,598},smallFont,muted,DT_LEFT|DT_WORDBREAK);}
// Layout (2.21.2): one quiet row for profile and channel, then the two colour panels and the options; nothing else.
void paintProfilePage(HDC dc){
    COLORREF ac=RGB(84,140,255),ri=RGB(196,208,232);HBRUSH sep=CreateSolidBrush(line);
    for(RECT p:std::initializer_list<RECT>{{12,48,840,120},{12,134,422,378},{430,134,840,378},{12,392,840,500}})roundBox(dc,p,panel,line,18);
    for(RECT d:std::initializer_list<RECT>{{426,64,427,104},{28,210,406,211},{446,210,824,211},{28,264,406,265},{28,316,406,317},{446,264,824,265},{446,316,824,317}}){FillRect(dc,&d,sep);}DeleteObject(sep);
    glyph(dc,0xE81E,{26,66,62,102},tileFont,ac);textAt(dc,i18n::text(L"프로필"),{72,66,190,102},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    glyph(dc,0xE701,{442,66,478,102},tileFont,ac);textAt(dc,i18n::text(L"채널"),{488,66,600,102},boldFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    glyph(dc,0xE7F4,{24,150,62,190},tileFont,ac);textAt(dc,i18n::text(L"디스플레이 컬러"),{74,148,410,172},sectionFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);textAt(dc,i18n::text(L"게임 화면의 색감과 밝기를 조정합니다."),{74,175,410,197},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    glyph(dc,0xE790,{442,150,480,190},tileFont,ac);textAt(dc,i18n::text(L"고급 컬러"),{492,148,828,172},sectionFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);textAt(dc,i18n::text(L"더 세밀한 화면 표현을 조정합니다."),{492,175,828,197},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen pen(painting::color(ri),1.6f);painting::roundCaps(pen);Gdiplus::SolidBrush fill(painting::color(ri));
        // contrast: a circle half filled; sharpness: a half-filled triangle; hue: a circle split in three
        g.DrawEllipse(&pen,33.f,281.f,18.f,18.f);g.FillPie(&fill,33.f,281.f,18.f,18.f,90.f,180.f);
        Gdiplus::PointF tri[]={{460.f,229.f},{470.f,247.f},{450.f,247.f}},half[]={{460.f,229.f},{470.f,247.f},{460.f,247.f}};g.DrawPolygon(&pen,tri,3);g.FillPolygon(&fill,half,3);
        g.DrawEllipse(&pen,451.f,281.f,18.f,18.f);g.FillPie(&fill,451.f,281.f,18.f,18.f,-90.f,120.f);g.DrawLine(&pen,460.f,290.f,460.f,281.f);g.DrawLine(&pen,460.f,290.f,468.f,295.f);g.DrawLine(&pen,460.f,290.f,452.f,295.f);}
    glyph(dc,0xE706,{26,224,58,252},iconFont,ri);textAt(dc,L"γ",{26,328,58,356},sectionFont,ri,DT_CENTER|DT_VCENTER|DT_SINGLELINE);glyph(dc,0xE995,{444,328,476,356},iconFont,ri);
    glyph(dc,0xE713,{22,402,58,438},tileFont,ac);std::wstring more=i18n::text(L"추가 옵션");SIZE ts={};auto of=SelectObject(dc,boldFont);GetTextExtentPoint32W(dc,more.c_str(),(int)more.size(),&ts);SelectObject(dc,of);
    textAt(dc,more,{72,402,72+ts.cx+4,426},boldFont,ink);textAt(dc,i18n::text(L"환경에 맞게 자동 적용 기능을 설정합니다."),{86+ts.cx,402,828,426},smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
inline bool modeTabClick(int id){if(id!=90&&id!=91)return false;if(id-90!=modeTab)PostMessageW(win,WM_COMMAND,MAKEWPARAM(id==90?32:34,BN_CLICKED),0);return true;}
LRESULT CALLBACK editorProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        draft=profiles;editProfile=0;editChannel=0;
        auto add=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int height,int id=0){return addSetting(h,cls,text,style,x,y,w,height,id);};
        modeTabs(h);valueSlider::registerClass();
        HWND box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,196,68,210,180,60);
        for(int i=0;i<4;i++){SendMessageW(box,CB_ADDSTRING,0,(LPARAM)i18n::text(settings::profileName(i)));}
        SendMessageW(box,CB_SETCURSEL,0,0);SendMessageW(box,CB_SETITEMHEIGHT,(WPARAM)-1,26);
        box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,610,68,214,180,61);
        for(auto label:{i18n::text(L"모든 채널"),i18n::text(L"적색 (R)"),i18n::text(L"녹색 (G)"),i18n::text(L"청색 (B)")}){SendMessageW(box,CB_ADDSTRING,0,(LPARAM)label);}
        SendMessageW(box,CB_SETCURSEL,0,0);SendMessageW(box,CB_SETITEMHEIGHT,(WPARAM)-1,26);
        // Rows: label, slider, value box (two panels of three; the row icons are painted by paintProfilePage).
        const wchar_t* labels[]={i18n::text(L"밝기"),i18n::text(L"대비"),i18n::text(L"감마"),i18n::text(L"디지털 선명도 (%)"),i18n::text(L"색조 (°)"),i18n::text(L"브라우저 볼륨 (%)")};
        for(int i=0;i<6;i++){
            int col=i/3,cy=238+(i%3)*52,x=col?418:0;
            HWND label=add(L"STATIC",labels[i],SS_LEFTNOWORDWRAP|SS_ENDELLIPSIS|SS_CENTERIMAGE,x+64,cy-14,col?150:90,28);SendMessageW(label,WM_SETFONT,(WPARAM)smallFont,TRUE);
            add(L"BlueAshValueSlider",L"",WS_TABSTOP,x+(col?218:158),cy-14,col?110:170,28,150+i);
            HWND input=add(L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL|ES_CENTER,x+336,cy-16,70,32,50+i);SendMessageW(input,EM_SETLIMITTEXT,12,0);
        }
        pageSwitch(h,i18n::text(L"밝기·대비·선명도·색조도 바꾸기 (끄면 감마만 바꿉니다)"),70,434,758,30,57);
        pageSwitch(h,i18n::text(L"게임 모드에서 실행 중인 게임(배그·타르코프) 프로필 자동 사용"),70,466,758,30,58);checkBox(h,58,autoGame);
        canvasStatus(h);
        pageButton(h,i18n::text(L"기본값으로"),12,556,178,41);
        pageButton(h,i18n::text(L"닫기"),572,556,120,42);
        pageButton(h,i18n::text(L"저장"),704,556,136,40,true);
        fillColorEditor(h);return 0;
    }
    case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);
        if(event==EN_CHANGE&&!filling){
            if(id>=50&&id<=52)colorDirty[id-50]=true;
            if(id==50||id==51||id==53||id==54||(id==52&&editChannel))checkBox(h,57,true);
        }
        if(event==EN_CHANGE&&id>=50&&id<=55){InvalidateRect(GetDlgItem(h,id+100),nullptr,FALSE);}
        if(event==CBN_SELCHANGE&&(id==60||id==61)){
            int previous=id==60?editProfile:editChannel;
            int next=(int)SendDlgItemMessageW(h,id,CB_GETCURSEL,0,0);
            if(!collectColorEditor(h)){SendDlgItemMessageW(h,id,CB_SETCURSEL,previous,0);return 0;}
            if(id==60)editProfile=next;else editChannel=next;fillColorEditor(h);
        }
        if(event==BN_CLICKED){
            if(modeTabClick(id))return 0;
            if(id==57||id==58){checkBox(h,id,!checked(h,id));return 0;}
            if(id==40||id==IDOK)storeColorEditor(h);
            else if(id==41){draft[editProfile]=settings::defaults()[editProfile];fillColorEditor(h);SetDlgItemTextW(h,59,i18n::text(L"기본값으로 되돌렸습니다. 저장을 눌러야 반영됩니다."));}
            else if(id==42||id==IDCANCEL)DestroyWindow(h);
        }return 0;
    }
    case WM_ERASEBKGND:pagePanels(h,(HDC)wp,[&](HDC dc){paintProfilePage(dc);drawStatus(h,dc);});return 1;
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:editor=nullptr;if(IsWindow(win)){EnableWindow(win,TRUE);SetForegroundWindow(win);}return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
std::vector<unsigned> shortcutKeys;
LRESULT CALLBACK shortcutProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        auto add=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int height,int id=0){return addSetting(h,cls,text,style,x,y,w,height,id);};
        modeTabs(h);
        pageLabel(h,i18n::text(L"키"),36,86,80,26);
        HWND box=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,122,80,300,240,60);
        shortcutKeys={VK_SUBTRACT,VK_ADD,VK_MULTIPLY,VK_DIVIDE,VK_DECIMAL};
        for(unsigned k=VK_NUMPAD0;k<=VK_NUMPAD9;k++)shortcutKeys.push_back(k);
        // F1~F11 only (F13~F24 are not on ordinary keyboards); a key already chosen outside the list stays in it.
        for(unsigned k=VK_F1;k<=VK_F11;k++)shortcutKeys.push_back(k);
        for(unsigned k='A';k<='Z';k++)shortcutKeys.push_back(k);
        for(unsigned k='0';k<='9';k++)shortcutKeys.push_back(k);
        for(unsigned k:{VK_PAUSE,VK_SCROLL,VK_INSERT,VK_DELETE,VK_HOME,VK_END,VK_PRIOR,VK_NEXT,VK_LEFT,VK_RIGHT,VK_UP,VK_DOWN})shortcutKeys.push_back(k);
        if(std::find(shortcutKeys.begin(),shortcutKeys.end(),hotkeyVk)==shortcutKeys.end())shortcutKeys.push_back(hotkeyVk);
        unsigned oldVk=hotkeyVk,oldMods=hotkeyMods;int selected=0;hotkeyMods=0;
        for(size_t i=0;i<shortcutKeys.size();i++){hotkeyVk=shortcutKeys[i];auto name=hotkeyName();SendMessageW(box,CB_ADDSTRING,0,(LPARAM)name.c_str());if(hotkeyVk==oldVk)selected=(int)i;}
        hotkeyVk=oldVk;hotkeyMods=oldMods;SendMessageW(box,CB_SETCURSEL,selected,0);
        pageLabel(h,i18n::text(L"조합 키"),36,144,80,26);
        const wchar_t* names[]={L"Ctrl",L"Alt",L"Shift",L"Win"};unsigned mods[]={MOD_CONTROL,MOD_ALT,MOD_SHIFT,MOD_WIN};
        for(int i=0;i<4;i++){pageSwitch(h,names[i],122+i*130,140,124,32,70+i);checkBox(h,70+i,(hotkeyMods&mods[i])!=0);}
        pageLabel(h,i18n::text(L"기본값: 숫자패드 − (조합 키 없음)  ·  F12는 Windows가 쓰는 키라 목록에 없습니다."),36,198,780,24,1);
        pageLabel(h,i18n::text(L"다른 프로그램이 쓰는 키를 고르면 저장되지 않고 지금 키가 그대로 남습니다."),36,224,780,24,1);
        canvasStatus(h);
        pageButton(h,i18n::text(L"기본값으로"),12,556,178,41);
        pageButton(h,i18n::text(L"닫기"),572,556,120,42);
        pageButton(h,i18n::text(L"저장"),704,556,136,40,true);
        return 0;
    }
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){int id=LOWORD(wp);
        if(modeTabClick(id))return 0;
        if(id>=70&&id<=73){checkBox(h,id,!checked(h,id));return 0;}
        if(id==41){SendDlgItemMessageW(h,60,CB_SETCURSEL,0,0);for(int i=70;i<74;i++)checkBox(h,i,false);}
        else if(id==42||id==IDCANCEL)DestroyWindow(h);
        else if(id==40||id==IDOK){
            int selection=(int)SendDlgItemMessageW(h,60,CB_GETCURSEL,0,0);if(selection<0||selection>=(int)shortcutKeys.size())return 0;
            unsigned mods=0,bits[]={MOD_CONTROL,MOD_ALT,MOD_SHIFT,MOD_WIN};for(int i=0;i<4;i++)if(checked(h,70+i))mods|=bits[i];
            unsigned key=shortcutKeys[selection],oldKey=hotkeyVk,oldMods=hotkeyMods;
            if(hotkeyRegistered){UnregisterHotKey(win,1);hotkeyRegistered=false;}
            if(!RegisterHotKey(win,1,MOD_NOREPEAT|mods,key)){
                registerShortcut();SetDlgItemTextW(h,59,i18n::text(L"이 키는 다른 프로그램이나 Windows가 쓰고 있습니다. 지금 키를 그대로 두었습니다."));return 0;
            }
            hotkeyRegistered=true;
            if(!saveString(configPath,L"HotkeyBindingV1",std::to_wstring(mods)+L","+std::to_wstring(key))){
                hotkeyVk=oldKey;hotkeyMods=oldMods;registerShortcut();SetDlgItemTextW(h,59,i18n::text(L"저장하지 못했습니다. 지금 키를 그대로 두었습니다."));return 0;
            }
            hotkeyVk=key;hotkeyMods=mods;
            setText(hotkeyLabel,hotkeyName());
            SetDlgItemTextW(h,59,(i18n::text(L"저장했습니다: ")+hotkeyName()).c_str());
        }
        }return 0;
    case WM_ERASEBKGND:pagePanels(h,(HDC)wp,[&](HDC dc){roundBox(dc,{12,48,840,384},panel,line,18);drawStatus(h,dc);});return 1;
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:editor=nullptr;if(IsWindow(win)){EnableWindow(win,TRUE);SetForegroundWindow(win);}return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
