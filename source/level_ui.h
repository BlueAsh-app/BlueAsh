#pragma once
// A volume slider for settings pages (레이드 시작 알림, 비행기 탑승 알림): a speaker icon, a track, and the number in a
// bubble only while it is being changed, like the dashboard sliders. 0~100. The page reads it with levelValue() and
// sets it with levelSet(); every change is sent to the parent as WM_COMMAND (id, LEVEL_CHANGED).
namespace level {
const int LEVEL_CHANGED=0x4C56;
struct State{int value=0;bool drag=false;ULONGLONG keyUntil=0;};
inline State* of(HWND h){return (State*)GetWindowLongPtrW(h,GWLP_USERDATA);}
inline int fromPoint(int x,int width){int left=40,span=std::max(1,width-10-left);return std::clamp((x-left)*100/span,0,100);}
inline void changed(HWND h){InvalidateRect(h,nullptr,FALSE);SendMessageW(GetParent(h),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(h),LEVEL_CHANGED),(LPARAM)h);}
inline void paint(HWND h,HDC dc){
 auto s=of(h);if(!s)return;RECT r;GetClientRect(h,&r);FillRect(dc,&r,panelBrush);
 int v=s->value,cy=r.bottom-14,left=40,span=std::max(1,(int)r.right-10-left),x=left+v*span/100;bool live=s->drag||GetTickCount64()<s->keyUntil;
 glyph(dc,v==0?0xE74F:v<34?0xE993:v<67?0xE994:0xE995,{0,cy-14,30,cy+14},iconFont,live||GetFocus()==h?ink:muted);
 roundBox(dc,{left,cy-2,r.right-10,cy+3},well,well,5);if(x>left)roundBox(dc,{left,cy-2,x,cy+3},blue,blue,5);painting::circle(dc,{x-7,cy-7,x+7,cy+7},ink,CLR_NONE);
 if(live){std::wstring t=std::to_wstring(v)+L"%";int bw=(int)t.size()*8+20,bx=std::clamp(x-bw/2,0,(int)r.right-bw);RECT pill={bx,cy-34,bx+bw,cy-12};roundBox(dc,pill,blue,blue,10);textAt(dc,t,pill,smallBold,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
}
inline LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto s=of(h);
 if(m==WM_NCCREATE){SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)new State);return TRUE;}
 if(!s)return DefWindowProcW(h,m,w,l);
 switch(m){
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paint(h,dc);EndPaint(h,&ps);return 0;}
 case WM_PRINT:case WM_PRINTCLIENT:paint(h,(HDC)w);return 0;
 case WM_ERASEBKGND:return 1;
 case WM_GETDLGCODE:return DLGC_WANTARROWS;
 case WM_LBUTTONDOWN:{SetFocus(h);SetCapture(h);s->drag=true;RECT r;GetClientRect(h,&r);s->value=fromPoint((short)LOWORD(l),r.right);changed(h);return 0;}
 case WM_MOUSEMOVE:if(s->drag){RECT r;GetClientRect(h,&r);int v=fromPoint((short)LOWORD(l),r.right);if(v!=s->value){s->value=v;changed(h);}}return 0;
 case WM_LBUTTONUP:if(s->drag){s->drag=false;ReleaseCapture();InvalidateRect(h,nullptr,FALSE);}return 0;
 case WM_CAPTURECHANGED:s->drag=false;InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_KEYDOWN:if(w==VK_LEFT||w==VK_RIGHT||w==VK_HOME||w==VK_END){s->value=w==VK_HOME?0:w==VK_END?100:std::clamp(s->value+(w==VK_RIGHT?5:-5),0,100);s->keyUntil=GetTickCount64()+1200;SetTimer(h,1,1300,nullptr);changed(h);return 0;}break;
 case WM_TIMER:KillTimer(h,1);InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_SETFOCUS:case WM_KILLFOCUS:InvalidateRect(h,nullptr,FALSE);return 0;
 case WM_NCDESTROY:delete s;SetWindowLongPtrW(h,GWLP_USERDATA,0);break;
 }return DefWindowProcW(h,m,w,l);
}
inline void registerControl(){WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshLevel";wc.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&wc);}
}
inline int levelValue(HWND page,int id){auto s=level::of(GetDlgItem(page,id));return s?s->value:0;}
inline void levelSet(HWND page,int id,int v){HWND c=GetDlgItem(page,id);if(auto s=level::of(c)){s->value=std::clamp(v,0,100);InvalidateRect(c,nullptr,FALSE);}}
