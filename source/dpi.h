#pragma once
#include <windows.h>
#include <commctrl.h> // TOOLTIPS_CLASSW, TTM_SETMAXTIPWIDTH
// Our own display scaling. BlueAsh is laid out in 96-DPI "design pixels" everywhere (1160x780 window, 852x622 pages,
// fonts in pixel heights). It used to stay DPI-unaware with gdiScaling, letting Windows redraw GDI at the real
// resolution, but Windows rebuilt that scaled surface on every resize and the whole window flashed the background
// colour. Now the process is DPI-aware and this header does the scaling itself:
//  - every coordinate that crosses into Windows (window rects, client sizes, mouse points, regions, menu points,
//    control item heights) is multiplied by s on the way out and divided on the way in, so the rest of the code keeps
//    its design numbers;
//  - every paint DC (BeginPaint, GetDC of our windows, WM_DRAWITEM / WM_ERASEBKGND DCs) gets a world transform of s, and
//    memory bitmaps made from them are allocated at the real size and carry the same transform, so GDI and GDI+ draw
//    text and shapes sharp at full resolution and copies between them stay 1:1;
//  - window procedures (classes we register, controls we subclass) receive design coordinates through one thunk.
// Code that works in physical pixels on purpose (game windows, the PUBG overlay, the server badge, screen capture)
// runs inside dpi::Raw, and other processes' top-level windows are left alone.
// Included once, by ModeSwitch.cpp, after the system headers; the function-like macros at the bottom route the calls.
#include <unordered_map>
#include <unordered_set>
#include <cmath>
namespace dpi {
inline double s=1;inline thread_local int raw=0;
struct Raw {Raw(){raw++;}~Raw(){raw--;}};
inline void init(){HDC d=(GetDC)(nullptr);int v=GetDeviceCaps(d,LOGPIXELSX);(ReleaseDC)(nullptr,d);s=v>0?v/96.0:1;}
inline int up(int v){return (int)std::lround(v*s);}
inline int down(int v){return (int)std::floor(v/s+.5);}
inline void up(RECT& r){r={up(r.left),up(r.top),up(r.right),up(r.bottom)};}
inline void down(RECT& r){r={down(r.left),down(r.top),down(r.right),down(r.bottom)};}
inline void downOut(RECT& r){r={(LONG)std::floor(r.left/s),(LONG)std::floor(r.top/s),(LONG)std::ceil(r.right/s),(LONG)std::ceil(r.bottom/s)};}
inline void up(POINT& p){p={up(p.x),up(p.y)};}inline void down(POINT& p){p={down(p.x),down(p.y)};}
inline std::unordered_set<HWND> rawWindows; // made inside dpi::Raw (physical-pixel windows)
// True when h's coordinates are scaled: ours or a child inside our windows (the helper's embedded pages), not raw.
inline bool virt(HWND h){if(s==1||raw)return false;if(!h)return true;if(rawWindows.count(h))return false;DWORD self=GetCurrentProcessId();
    for(HWND w=h;w;w=GetAncestor(w,GA_PARENT)){if(w==GetDesktopWindow())break;DWORD pid=0;GetWindowThreadProcessId(w,&pid);if(pid==self)return true;}return false;}
// ---- DC transforms and the memory bitmaps that carry them.
inline std::unordered_set<HBITMAP> scaled;
inline bool scaledDc(HDC dc){if(!dc||GetGraphicsMode(dc)!=GM_ADVANCED)return false;XFORM x={};return GetWorldTransform(dc,&x)&&std::fabs(x.eM11-s)<1e-6&&x.eM11!=1;}
inline void setScale(HDC dc,bool on){if(!dc)return;if(on){SetGraphicsMode(dc,GM_ADVANCED);XFORM x={(FLOAT)s,0,0,(FLOAT)s,0,0};SetWorldTransform(dc,&x);}
    else if(GetGraphicsMode(dc)==GM_ADVANCED){ModifyWorldTransform(dc,nullptr,MWT_IDENTITY);SetGraphicsMode(dc,GM_COMPATIBLE);}}
inline HDC beginPaint(HWND h,PAINTSTRUCT* ps){HDC dc=(BeginPaint)(h,ps);if(dc&&virt(h)){setScale(dc,true);downOut(ps->rcPaint);}return dc;}
inline HDC getDC(HWND h){HDC dc=(GetDC)(h);if(dc&&h&&virt(h))setScale(dc,true);return dc;}
inline HDC getWindowDC(HWND h){HDC dc=(GetWindowDC)(h);if(dc&&virt(h))setScale(dc,true);return dc;}
inline HBITMAP compatibleBitmap(HDC dc,int w,int h){if(scaledDc(dc)){HBITMAP b=(CreateCompatibleBitmap)(dc,std::max(1,(int)std::ceil(w*s)),std::max(1,(int)std::ceil(h*s)));if(b)scaled.insert(b);return b;}return (CreateCompatibleBitmap)(dc,w,h);}
inline HGDIOBJ selectObject(HDC dc,HGDIOBJ o){HGDIOBJ r=(SelectObject)(dc,o);if(o&&GetObjectType(o)==OBJ_BITMAP&&GetObjectType(dc)==OBJ_MEMDC){setScale(dc,scaled.count((HBITMAP)o)>0);}return r;}
inline BOOL deleteObject(HGDIOBJ o){if(o&&GetObjectType(o)==OBJ_BITMAP)scaled.erase((HBITMAP)o);return (DeleteObject)(o);}
// ---- Window geometry.
inline BOOL getClientRect(HWND h,RECT* r){BOOL ok=(GetClientRect)(h,r);if(ok&&virt(h))down(*r);return ok;}
inline BOOL getWindowRect(HWND h,RECT* r){BOOL ok=(GetWindowRect)(h,r);if(ok&&virt(h))down(*r);return ok;}
inline BOOL setWindowPos(HWND h,HWND after,int x,int y,int cx,int cy,UINT f){if(virt(h)){x=up(x);y=up(y);cx=up(cx);cy=up(cy);}return (SetWindowPos)(h,after,x,y,cx,cy,f);}
inline BOOL moveWindow(HWND h,int x,int y,int w,int hh,BOOL repaint){if(virt(h)){x=up(x);y=up(y);w=up(w);hh=up(hh);}return (MoveWindow)(h,x,y,w,hh,repaint);}
inline HWND createWindowEx(DWORD ex,LPCWSTR cls,LPCWSTR name,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE inst,LPVOID param){
    bool v=!raw&&s!=1&&(!parent||virt(parent));auto u=[&](int& a){if(v&&a!=CW_USEDEFAULT)a=up(a);};u(x);u(y);u(w);u(h);
    HWND r=(CreateWindowExW)(ex,cls,name,style,x,y,w,h,parent,menu,inst,param);if(r&&raw)rawWindows.insert(r);return r;}
inline BOOL screenToClient(HWND h,POINT* p){bool v=virt(h);if(v)up(*p);BOOL ok=(ScreenToClient)(h,p);if(v)down(*p);return ok;}
inline BOOL clientToScreen(HWND h,POINT* p){bool v=virt(h);if(v)up(*p);BOOL ok=(ClientToScreen)(h,p);if(v)down(*p);return ok;}
inline int mapWindowPoints(HWND from,HWND to,POINT* p,UINT n){bool a=virt(from),b=virt(to);if(a)for(UINT i=0;i<n;i++)up(p[i]);int r=(MapWindowPoints)(from,to,p,n);if(b)for(UINT i=0;i<n;i++)down(p[i]);return r;}
inline BOOL getCursorPos(POINT* p){BOOL ok=(GetCursorPos)(p);if(ok&&s!=1&&!raw)down(*p);return ok;}
inline BOOL trackPopupMenu(HMENU m,UINT f,int x,int y,int res,HWND h,const RECT* r){if(s!=1&&!raw){x=up(x);y=up(y);}return (TrackPopupMenu)(m,f,x,y,res,h,r);}
inline BOOL invalidateRect(HWND h,const RECT* r,BOOL e){if(r&&virt(h)){RECT c=*r;up(c);InflateRect(&c,1,1);return (InvalidateRect)(h,&c,e);}return (InvalidateRect)(h,r,e);}
inline BOOL redrawWindow(HWND h,const RECT* r,HRGN g,UINT f){if(r&&virt(h)){RECT c=*r;up(c);InflateRect(&c,1,1);return (RedrawWindow)(h,&c,g,f);}return (RedrawWindow)(h,r,g,f);}
inline BOOL setViewportOrgEx(HDC dc,int x,int y,POINT* old){if(scaledDc(dc)){x=up(x);y=up(y);}BOOL ok=(SetViewportOrgEx)(dc,x,y,old);if(old&&scaledDc(dc))down(*old);return ok;}
inline BOOL getMenuItemRect(HWND h,HMENU m,UINT i,RECT* r){BOOL ok=(GetMenuItemRect)(h,m,i,r);if(ok&&s!=1&&!raw)down(*r);return ok;}
inline HRGN createRectRgn(int a,int b,int c,int d){if(s!=1&&!raw){a=up(a);b=up(b);c=up(c);d=up(d);}return (CreateRectRgn)(a,b,c,d);}
inline HRGN createRoundRectRgn(int a,int b,int c,int d,int w,int h){if(s!=1&&!raw){a=up(a);b=up(b);c=up(c);d=up(d);w=up(w);h=up(h);}return (CreateRoundRectRgn)(a,b,c,d,w,h);}
inline int getSystemMetrics(int i){int v=(GetSystemMetrics)(i);if(s==1||raw)return v;
    switch(i){case SM_CXFRAME:case SM_CYFRAME:case SM_CXPADDEDBORDER:case SM_CXMENUCHECK:case SM_CYMENUCHECK:case SM_CXSCREEN:case SM_CYSCREEN:case SM_CXICON:case SM_CYICON:case SM_CXSMICON:case SM_CYSMICON:return down(v);default:return v;}}
// ---- Messages. Fonts given to controls Windows draws itself (static text, edit boxes, tooltips) are swapped for a
// copy at the real size: those controls paint without our transform.
inline std::unordered_map<HFONT,HFONT> bigFonts;
inline HFONT bigFont(HFONT f){if(!f||s==1)return f;auto it=bigFonts.find(f);if(it!=bigFonts.end())return it->second;LOGFONTW lf={};if(!GetObjectW(f,sizeof(lf),&lf))return f;lf.lfHeight=up(lf.lfHeight);lf.lfWidth=up(lf.lfWidth);HFONT b=CreateFontIndirectW(&lf);bigFonts[f]=b?b:f;return bigFonts[f];}
inline bool drawnByWindows(HWND h){wchar_t c[32]={};GetClassNameW(h,c,32);if(!_wcsicmp(c,L"Static"))return (GetWindowLongW(h,GWL_STYLE)&SS_TYPEMASK)!=SS_OWNERDRAW;return !_wcsicmp(c,L"Edit")||!_wcsicmp(c,TOOLTIPS_CLASSW);}
inline LRESULT sendMessage(HWND h,UINT m,WPARAM w,LPARAM l){
    if(virt(h)){switch(m){
        case WM_SETFONT:if(drawnByWindows(h))w=(WPARAM)bigFont((HFONT)w);break;
        case CB_SETITEMHEIGHT:l=up((int)l);break;
        case EM_SETMARGINS:l=MAKELPARAM(up(LOWORD(l)),up(HIWORD(l)));break;
        case TTM_SETMAXTIPWIDTH:l=up((int)l);break;
        case CB_GETDROPPEDCONTROLRECT:{LRESULT r=(SendMessageW)(h,m,w,l);if(l)down(*(RECT*)l);return r;}
        case CB_GETITEMHEIGHT:return down((int)(SendMessageW)(h,m,w,l));}}
    return (SendMessageW)(h,m,w,l);}
inline LRESULT sendDlgItemMessage(HWND h,int id,UINT m,WPARAM w,LPARAM l){return sendMessage(GetDlgItem(h,id),m,w,l);}
// Packed and struct coordinates of a message, converted in place. dir>0: physical -> design (into our procedures);
// dir<0: design -> physical (from our procedures into Windows). Returns the lParam to pass on.
inline LPARAM convert(UINT m,WPARAM w,LPARAM l,int dir){
    auto pt=[&](LPARAM v){int x=(short)LOWORD(v),y=(short)HIWORD(v);x=dir>0?down(x):up(x);y=dir>0?down(y):up(y);return MAKELPARAM((WORD)(short)x,(WORD)(short)y);};
    auto r=[&](RECT& c){if(dir>0)down(c);else up(c);};auto p=[&](POINT& q){if(dir>0)down(q);else up(q);};auto n=[&](int v){return dir>0?down(v):up(v);};
    switch(m){
    case WM_MOUSEMOVE:case WM_LBUTTONDOWN:case WM_LBUTTONUP:case WM_LBUTTONDBLCLK:case WM_RBUTTONDOWN:case WM_RBUTTONUP:case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:case WM_MBUTTONUP:case WM_MBUTTONDBLCLK:case WM_XBUTTONDOWN:case WM_XBUTTONUP:case WM_XBUTTONDBLCLK:case WM_MOUSEHOVER:
    case WM_NCHITTEST:case WM_MOUSEWHEEL:case WM_MOUSEHWHEEL:case WM_MOVE:
    case WM_NCMOUSEMOVE:case WM_NCLBUTTONDOWN:case WM_NCLBUTTONUP:case WM_NCLBUTTONDBLCLK:case WM_NCRBUTTONDOWN:case WM_NCRBUTTONUP:case WM_NCMBUTTONDOWN:case WM_NCMBUTTONUP:
        return pt(l);
    case WM_CONTEXTMENU:return l==-1?l:pt(l);
    case WM_SIZE:return MAKELPARAM((WORD)n(LOWORD(l)),(WORD)n(HIWORD(l)));
    case WM_GETMINMAXINFO:{auto* i=(MINMAXINFO*)l;p(i->ptMaxSize);p(i->ptMaxPosition);p(i->ptMinTrackSize);p(i->ptMaxTrackSize);return l;}
    case WM_WINDOWPOSCHANGING:case WM_WINDOWPOSCHANGED:{auto* q=(WINDOWPOS*)l;q->x=n(q->x);q->y=n(q->y);q->cx=n(q->cx);q->cy=n(q->cy);return l;}
    case WM_NCCALCSIZE:if(w){auto* c=(NCCALCSIZE_PARAMS*)l;for(auto& x:c->rgrc)r(x);}else r(*(RECT*)l);return l;
    case WM_DRAWITEM:r(((DRAWITEMSTRUCT*)l)->rcItem);return l;
    case WM_MEASUREITEM:{auto* q=(MEASUREITEMSTRUCT*)l;q->itemWidth=n((int)q->itemWidth);q->itemHeight=n((int)q->itemHeight);return l;}
    }return l;}
inline bool hasStruct(UINT m){return m==WM_GETMINMAXINFO||m==WM_WINDOWPOSCHANGING||m==WM_WINDOWPOSCHANGED||m==WM_NCCALCSIZE||m==WM_DRAWITEM||m==WM_MEASUREITEM;}
// Our procedures: those of the classes we register and the subclass procedures we install.
inline std::unordered_set<WNDPROC> ours;inline std::unordered_map<std::wstring,WNDPROC> classProcs;inline std::unordered_map<HWND,WNDPROC> windowProcs;
LRESULT CALLBACK thunk(HWND h,UINT m,WPARAM w,LPARAM l);
inline WNDPROC procOf(HWND h){auto it=windowProcs.find(h);if(it!=windowProcs.end())return it->second;wchar_t c[256]={};GetClassNameW(h,c,256);auto k=classProcs.find(c);if(k==classProcs.end())return DefWindowProcW;windowProcs[h]=k->second;return k->second;}
// Runs one of our procedures with design coordinates; DCs handed to it carry the transform while it runs.
inline LRESULT callOurs(WNDPROC p,HWND h,UINT m,WPARAM w,LPARAM l){
    if(!virt(h))return p(h,m,w,l);
    HDC dc=m==WM_ERASEBKGND||m==WM_PRINTCLIENT?(HDC)w:m==WM_DRAWITEM?((DRAWITEMSTRUCT*)l)->hDC:nullptr;bool had=dc&&scaledDc(dc);
    if(dc&&!had)setScale(dc,true);
    LPARAM l2=convert(m,w,l,+1);LRESULT r=p(h,m,w,l2);if(hasStruct(m))convert(m,w,l,-1);
    if(dc&&!had){setScale(dc,false);}
    return r;}
LRESULT CALLBACK thunk(HWND h,UINT m,WPARAM w,LPARAM l){WNDPROC p=procOf(h);LRESULT r=p==DefWindowProcW?(DefWindowProcW)(h,m,w,l):callOurs(p,h,m,w,l);if(m==WM_NCDESTROY){windowProcs.erase(h);rawWindows.erase(h);}return r;}
// Windows' own procedures (DefWindowProc, a control's original procedure) get physical coordinates back.
inline LRESULT toWindows(WNDPROC base,HWND h,UINT m,WPARAM w,LPARAM l){
    if(!virt(h))return base?(CallWindowProcW)(base,h,m,w,l):(DefWindowProcW)(h,m,w,l);
    LPARAM l2=convert(m,w,l,-1);LRESULT r=base?(CallWindowProcW)(base,h,m,w,l2):(DefWindowProcW)(h,m,w,l2);if(hasStruct(m))convert(m,w,l,+1);
    return r;}
inline LRESULT defWindowProc(HWND h,UINT m,WPARAM w,LPARAM l){return toWindows(nullptr,h,m,w,l);}
inline LRESULT callWindowProc(WNDPROC p,HWND h,UINT m,WPARAM w,LPARAM l){if(ours.count(p))return p(h,m,w,l);if(p==thunk){WNDPROC q=procOf(h);return q==DefWindowProcW?defWindowProc(h,m,w,l):q(h,m,w,l);}return toWindows(p,h,m,w,l);}
inline ATOM registerClass(const WNDCLASSW* wc){WNDCLASSW c=*wc;if(c.lpfnWndProc&&c.lpfnWndProc!=thunk){ours.insert(c.lpfnWndProc);classProcs[c.lpszClassName]=c.lpfnWndProc;c.lpfnWndProc=thunk;}return (RegisterClassW)(&c);}
inline LONG_PTR setWindowLongPtr(HWND h,int i,LONG_PTR v){
    if(i!=GWLP_WNDPROC){return (SetWindowLongPtrW)(h,i,v);}
    WNDPROC p=(WNDPROC)v;ours.insert(p);
    WNDPROC prev=(WNDPROC)(GetWindowLongPtrW)(h,GWLP_WNDPROC);WNDPROC before=prev==thunk?procOf(h):prev; // what the new procedure chains to
    windowProcs[h]=p;if(prev!=thunk)(SetWindowLongPtrW)(h,GWLP_WNDPROC,(LONG_PTR)thunk);return (LONG_PTR)before;}
}
#define BeginPaint(h,ps) dpi::beginPaint(h,ps)
#define GetWindowDC(h) dpi::getWindowDC(h)
#define CreateCompatibleBitmap(d,w,h) dpi::compatibleBitmap(d,w,h)
#define SelectObject(d,o) dpi::selectObject(d,o)
#define DeleteObject(o) dpi::deleteObject(o)
#define GetClientRect(h,r) dpi::getClientRect(h,r)
#define GetWindowRect(h,r) dpi::getWindowRect(h,r)
#define SetWindowPos(h,a,x,y,cx,cy,f) dpi::setWindowPos(h,a,x,y,cx,cy,f)
#define MoveWindow(h,x,y,w,hh,r) dpi::moveWindow(h,x,y,w,hh,r)
#undef CreateWindowEx
#define CreateWindowExW(...) dpi::createWindowEx(__VA_ARGS__)
#define ScreenToClient(h,p) dpi::screenToClient(h,p)
#define ClientToScreen(h,p) dpi::clientToScreen(h,p)
#define MapWindowPoints(a,b,p,n) dpi::mapWindowPoints(a,b,p,n)
#define GetCursorPos(p) dpi::getCursorPos(p)
#define TrackPopupMenu(m,f,x,y,r,h,c) dpi::trackPopupMenu(m,f,x,y,r,h,c)
#define InvalidateRect(h,r,e) dpi::invalidateRect(h,r,e)
#define RedrawWindow(h,r,g,f) dpi::redrawWindow(h,r,g,f)
#define SetViewportOrgEx(d,x,y,o) dpi::setViewportOrgEx(d,x,y,o)
#define GetMenuItemRect(h,m,i,r) dpi::getMenuItemRect(h,m,i,r)
#define CreateRectRgn(a,b,c,d) dpi::createRectRgn(a,b,c,d)
#define CreateRoundRectRgn(a,b,c,d,w,h) dpi::createRoundRectRgn(a,b,c,d,w,h)
#define GetSystemMetrics(i) dpi::getSystemMetrics(i)
#define SendMessageW(h,m,w,l) dpi::sendMessage(h,m,w,l)
#define SendDlgItemMessageW(h,i,m,w,l) dpi::sendDlgItemMessage(h,i,m,w,l)
#define DefWindowProcW(h,m,w,l) dpi::defWindowProc(h,m,w,l)
#define CallWindowProcW(p,h,m,w,l) dpi::callWindowProc(p,h,m,w,l)
#define RegisterClassW(c) dpi::registerClass(c)
#define SetWindowLongPtrW(h,i,v) dpi::setWindowLongPtr(h,i,v)
