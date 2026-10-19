#pragma once
#include "i18n.h"
// Public Windows Runtime ABI. GUIDs/vtable order from Microsoft's windows-rs metadata.
// No injection, game-memory access, input simulation or desktop capture. The captures themselves are in pubg_board.h / pubg_map.h.
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <d3d11.h>
#include <dxgi.h>
#include <thread>
#include <mutex>
#include <atomic>
namespace wgc {
struct Size {INT32 width,height;};
struct Span {INT64 value;};
struct Token {INT64 value;};
struct Item;struct Session;
struct Frame: IInspectable {virtual HRESULT STDMETHODCALLTYPE Surface(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE Time(Span*)=0;virtual HRESULT STDMETHODCALLTYPE ContentSize(Size*)=0;};
struct Pool:IInspectable {virtual HRESULT STDMETHODCALLTYPE Recreate(IInspectable*,INT32,INT32,Size)=0;virtual HRESULT STDMETHODCALLTYPE Next(Frame**)=0;virtual HRESULT STDMETHODCALLTYPE AddArrived(IUnknown*,Token*)=0;virtual HRESULT STDMETHODCALLTYPE RemoveArrived(Token)=0;virtual HRESULT STDMETHODCALLTYPE CreateSession(Item*,Session**)=0;virtual HRESULT STDMETHODCALLTYPE Dispatcher(IInspectable**)=0;};
struct PoolFactory:IInspectable {virtual HRESULT STDMETHODCALLTYPE FreeThreaded(IInspectable*,INT32,INT32,Size,Pool**)=0;};
struct Item:IInspectable {virtual HRESULT STDMETHODCALLTYPE Name(HSTRING*)=0;virtual HRESULT STDMETHODCALLTYPE GetSize(Size*)=0;virtual HRESULT STDMETHODCALLTYPE AddClosed(IUnknown*,Token*)=0;virtual HRESULT STDMETHODCALLTYPE RemoveClosed(Token)=0;};
struct Session:IInspectable {virtual HRESULT STDMETHODCALLTYPE Start()=0;};
struct Session2:IInspectable {virtual HRESULT STDMETHODCALLTYPE GetCursor(boolean*)=0;virtual HRESULT STDMETHODCALLTYPE SetCursor(boolean)=0;};
struct Session3:IInspectable {virtual HRESULT STDMETHODCALLTYPE GetBorder(boolean*)=0;virtual HRESULT STDMETHODCALLTYPE SetBorder(boolean)=0;};
struct Session5:IInspectable {virtual HRESULT STDMETHODCALLTYPE GetInterval(Span*)=0;virtual HRESULT STDMETHODCALLTYPE SetInterval(Span)=0;};
struct Interop:IUnknown {virtual HRESULT STDMETHODCALLTYPE ForWindow(HWND,REFIID,void**)=0;virtual HRESULT STDMETHODCALLTYPE ForMonitor(HMONITOR,REFIID,void**)=0;};
struct SurfaceAccess:IUnknown {virtual HRESULT STDMETHODCALLTYPE GetInterface(REFIID,void**)=0;};
struct Closable:IInspectable {virtual HRESULT STDMETHODCALLTYPE Close()=0;};
const GUID iidPoolFactory={0x589b103f,0x6bbc,0x5df5,{0xa9,0x91,0x02,0xe2,0x8b,0x3b,0x66,0xd5}};
const GUID iidInterop={0x3628e81b,0x3cac,0x4c60,{0xb7,0xf4,0x23,0xce,0x0e,0x0c,0x33,0x56}};
const GUID iidItem={0x79c3f95b,0x31f7,0x4ec2,{0xa4,0x64,0x63,0x2e,0xf5,0xd3,0x07,0x60}};
const GUID iidDevice={0xa37624ab,0x8d5f,0x4650,{0x9d,0x3e,0x9e,0xae,0x3d,0x9b,0xc6,0x70}};
const GUID iidAccess={0xa9b3d012,0x3df2,0x4ee3,{0xb8,0xd1,0x86,0x95,0xf4,0x57,0xd3,0xc1}};
const GUID iidClose={0x30d5a829,0x7fa4,0x4026,{0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e}};
const GUID iidSession2={0x2c39ae40,0x7d2e,0x5044,{0x80,0x4e,0x8b,0x67,0x99,0xd4,0xcf,0x9e}};
const GUID iidSession3={0xf2cdd966,0x22ae,0x5ea1,{0x95,0x96,0x3a,0x28,0x93,0x44,0xc3,0xbe}};
const GUID iidSession5={0x67c0ea62,0x1f85,0x5061,{0x92,0x5a,0x23,0x9b,0xe0,0xac,0x09,0xcb}};
template<class T>struct Ptr {T* p=nullptr;Ptr()=default;Ptr(const Ptr&)=delete;Ptr& operator=(const Ptr&)=delete;~Ptr(){reset();}void reset(){if(p)p->Release();p=nullptr;}T** out(){reset();return &p;}T* operator->(){return p;}operator bool()const{return p!=nullptr;}};
inline void close(IUnknown* p){if(!p)return;Ptr<Closable> c;if(SUCCEEDED(p->QueryInterface(iidClose,(void**)c.out())))c->Close();}
struct FramePtr:Ptr<Frame>{~FramePtr(){close(p);}};
// Windows 11 draws a yellow frame around a captured window unless the session turns it off (checked: S_OK, border 1 -> 0).
inline void noBorder(Session* s){Ptr<Session3> s3;if(SUCCEEDED(s->QueryInterface(iidSession3,(void**)s3.out())))s3->SetBorder(false);}
inline void require(HRESULT h){if(FAILED(h))throw std::wstring(i18n::text(L"화면 캡처 오류 "))+hrtext(h);}
inline void factory(const wchar_t* name,REFIID iid,void** value){HSTRING s=nullptr;require(WindowsCreateString(name,(UINT32)wcslen(name),&s));HRESULT h=RoGetActivationFactory(s,iid,value);WindowsDeleteString(s);require(h);}
inline HWND window(DWORD pid){struct Find {DWORD pid;HWND result;LONG64 area;};Find f={pid,nullptr,0};EnumWindows([](HWND h,LPARAM l)->BOOL{auto& f=*(Find*)l;DWORD p=0;GetWindowThreadProcessId(h,&p);RECT r={};if(p==f.pid&&IsWindowVisible(h)&&!GetWindow(h,GW_OWNER)&&GetWindowRect(h,&r)){LONG64 a=LONG64(r.right-r.left)*(r.bottom-r.top);if(a>f.area){f.area=a;f.result=h;}}return TRUE;},(LPARAM)&f);return f.result;}
}
