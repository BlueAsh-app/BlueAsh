#pragma once
// Windows.Media.Ocr through the public Windows Runtime ABI (no WinRT headers, same approach as wgc_capture.h).
// Gray 8-bit pixels in, recognized text out. Call from a thread that did RoInitialize.
#include <inspectable.h>
#include <roapi.h>
#include <winstring.h>
#include <string>
#include <vector>
namespace winocr {
struct PlaneDescription {INT32 start,width,height,stride;};
struct AsyncInfo:IInspectable {virtual HRESULT STDMETHODCALLTYPE Id(UINT32*)=0;virtual HRESULT STDMETHODCALLTYPE Status(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE ErrorCode(HRESULT*)=0;virtual HRESULT STDMETHODCALLTYPE Cancel()=0;virtual HRESULT STDMETHODCALLTYPE Close()=0;};
struct Result:IInspectable {virtual HRESULT STDMETHODCALLTYPE Lines(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE TextAngle(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE Text(HSTRING*)=0;};
struct Operation:IInspectable {virtual HRESULT STDMETHODCALLTYPE PutCompleted(IUnknown*)=0;virtual HRESULT STDMETHODCALLTYPE GetCompleted(IUnknown**)=0;virtual HRESULT STDMETHODCALLTYPE GetResults(Result**)=0;};
struct Engine:IInspectable {virtual HRESULT STDMETHODCALLTYPE RecognizeAsync(IInspectable*,Operation**)=0;virtual HRESULT STDMETHODCALLTYPE Language(IInspectable**)=0;};
struct EngineStatics:IInspectable {virtual HRESULT STDMETHODCALLTYPE MaxImageDimension(UINT32*)=0;virtual HRESULT STDMETHODCALLTYPE Languages(IInspectable**)=0;virtual HRESULT STDMETHODCALLTYPE IsLanguageSupported(IInspectable*,boolean*)=0;virtual HRESULT STDMETHODCALLTYPE FromLanguage(IInspectable*,Engine**)=0;virtual HRESULT STDMETHODCALLTYPE FromUserProfileLanguages(Engine**)=0;};
struct BitmapBuffer:IInspectable {virtual HRESULT STDMETHODCALLTYPE PlaneCount(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE PlaneDescription(INT32,PlaneDescription*)=0;};
struct MemoryBuffer:IInspectable {virtual HRESULT STDMETHODCALLTYPE CreateReference(IInspectable**)=0;};
struct ByteAccess:IUnknown {virtual HRESULT STDMETHODCALLTYPE GetBuffer(BYTE**,UINT32*)=0;};
struct Bitmap:IInspectable {virtual HRESULT STDMETHODCALLTYPE Format(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE Alpha(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE Width(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE Height(INT32*)=0;virtual HRESULT STDMETHODCALLTYPE ReadOnly(boolean*)=0;virtual HRESULT STDMETHODCALLTYPE PutDpiX(double)=0;virtual HRESULT STDMETHODCALLTYPE DpiX(double*)=0;virtual HRESULT STDMETHODCALLTYPE PutDpiY(double)=0;virtual HRESULT STDMETHODCALLTYPE DpiY(double*)=0;virtual HRESULT STDMETHODCALLTYPE LockBuffer(INT32,BitmapBuffer**)=0;};
struct BitmapFactory:IInspectable {virtual HRESULT STDMETHODCALLTYPE Create(INT32,INT32,INT32,Bitmap**)=0;};
struct Closable:IInspectable {virtual HRESULT STDMETHODCALLTYPE Close()=0;};
const GUID iidStatics={0x5bffa85a,0x3384,0x3540,{0x99,0x40,0x69,0x91,0x20,0xd4,0x28,0xa8}};
const GUID iidBitmapFactory={0xc99feb69,0x2d62,0x4d47,{0xa6,0xb3,0x4f,0xdb,0x6a,0x07,0xfd,0xf8}};
const GUID iidMemoryBuffer={0xfbc4dd2a,0x245b,0x11e4,{0xaf,0x98,0x68,0x94,0x23,0x26,0x0c,0xf8}};
const GUID iidByteAccess={0x5b0d3235,0x4dba,0x4d44,{0x86,0x5e,0x8f,0x1d,0x0e,0x4f,0xd0,0x4d}};
const GUID iidAsyncInfo={0x00000036,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
const GUID iidClosable={0x30d5a829,0x7fa4,0x4026,{0x83,0xbb,0xd7,0x5b,0xae,0x4e,0xa9,0x9e}};
template<class T>struct Ref {T* p=nullptr;Ref()=default;Ref(const Ref&)=delete;Ref& operator=(const Ref&)=delete;~Ref(){if(p)p->Release();}T** out(){if(p)p->Release();p=nullptr;return &p;}T* operator->(){return p;}};
inline void closeIt(IUnknown* u){if(!u)return;Ref<Closable> c;if(SUCCEEDED(u->QueryInterface(iidClosable,(void**)c.out())))c->Close();}
inline HRESULT activation(const wchar_t* name,REFIID iid,void** out){HSTRING s=nullptr;HRESULT h=WindowsCreateString(name,(UINT32)wcslen(name),&s);if(FAILED(h))return h;h=RoGetActivationFactory(s,iid,out);WindowsDeleteString(s);return h;}
class Reader {
    Ref<Engine> engine;Ref<BitmapFactory> bitmaps;
public:
    // Engine for the user's languages (Korean here; digits read fine in any language).
    bool open(){if(engine.p)return true;Ref<EngineStatics> s;
        if(FAILED(activation(L"Windows.Media.Ocr.OcrEngine",iidStatics,(void**)s.out()))||FAILED(s->FromUserProfileLanguages(engine.out()))||!engine.p)return false;
        return SUCCEEDED(activation(L"Windows.Graphics.Imaging.SoftwareBitmap",iidBitmapFactory,(void**)bitmaps.out()));}
    // Recognize gray pixels (w*h, row stride w). Waits up to timeoutMs. Empty on failure.
    std::wstring read(const unsigned char* gray,int w,int h,DWORD timeoutMs=2000){
        if(!open()||w<40||h<40)return L"";
        Ref<Bitmap> bitmap;if(FAILED(bitmaps->Create(62,w,h,bitmap.out())))return L"";
        {Ref<BitmapBuffer> buffer;if(FAILED(bitmap->LockBuffer(2,buffer.out())))return L"";PlaneDescription plane={};buffer->PlaneDescription(0,&plane);
         Ref<MemoryBuffer> memory;Ref<IInspectable> ref;Ref<ByteAccess> bytes;BYTE* data=nullptr;UINT32 size=0;
         if(FAILED(buffer->QueryInterface(iidMemoryBuffer,(void**)memory.out()))||FAILED(memory->CreateReference(ref.out()))||FAILED(ref->QueryInterface(iidByteAccess,(void**)bytes.out()))||FAILED(bytes->GetBuffer(&data,&size))){closeIt(ref.p);closeIt(buffer.p);return L"";}
         for(int y=0;y<h&&plane.start+(INT64)y*plane.stride+w<=(INT64)size;y++)memcpy(data+plane.start+y*plane.stride,gray+(size_t)y*w,w);
         closeIt(ref.p);closeIt(buffer.p);}
        Ref<Operation> op;if(FAILED(engine->RecognizeAsync(bitmap.p,op.out())))return L"";
        Ref<AsyncInfo> info;if(FAILED(op->QueryInterface(iidAsyncInfo,(void**)info.out())))return L"";
        INT32 status=0;ULONGLONG until=GetTickCount64()+timeoutMs;while(SUCCEEDED(info->Status(&status))&&status==0&&GetTickCount64()<until)Sleep(5);
        if(status!=1){info->Cancel();info->Close();return L"";}
        Ref<Result> result;HSTRING text=nullptr;std::wstring out;
        if(SUCCEEDED(op->GetResults(result.out()))&&result.p&&SUCCEEDED(result->Text(&text))){UINT32 n=0;const wchar_t* s=WindowsGetStringRawBuffer(text,&n);out.assign(s,n);WindowsDeleteString(text);}
        info->Close();return out;
    }
};
}
