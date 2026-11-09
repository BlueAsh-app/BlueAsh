#pragma once
// Faces in a picture through Windows.Media.FaceAnalysis (the Windows Runtime ABI, as winocr.h does for OCR): gray 8-bit
// pixels in, face boxes out. Used by the 미리보기(예상) face pass (dlss_approx.h); call from a worker thread.
#include "winocr.h"
#include "dlss_approx.h"
namespace dlssface {
using namespace winocr;
struct Box {UINT32 x,y,w,h;};
struct Face:IInspectable {virtual HRESULT STDMETHODCALLTYPE FaceBox(Box*)=0;};
struct Faces:IInspectable {virtual HRESULT STDMETHODCALLTYPE GetAt(UINT32,Face**)=0;virtual HRESULT STDMETHODCALLTYPE Size(UINT32*)=0;};
struct FacesOp:IInspectable {virtual HRESULT STDMETHODCALLTYPE PutCompleted(IUnknown*)=0;virtual HRESULT STDMETHODCALLTYPE GetCompleted(IUnknown**)=0;virtual HRESULT STDMETHODCALLTYPE GetResults(Faces**)=0;};
struct Detector:IInspectable {virtual HRESULT STDMETHODCALLTYPE DetectFacesAsync(IInspectable*,FacesOp**)=0;};
struct DetectorOp:IInspectable {virtual HRESULT STDMETHODCALLTYPE PutCompleted(IUnknown*)=0;virtual HRESULT STDMETHODCALLTYPE GetCompleted(IUnknown**)=0;virtual HRESULT STDMETHODCALLTYPE GetResults(Detector**)=0;};
struct Statics:IInspectable {virtual HRESULT STDMETHODCALLTYPE CreateAsync(DetectorOp**)=0;};
const GUID iidStatics={0xbc042d67,0x9047,0x33f6,{0x88,0x1b,0x67,0x46,0xc1,0xb2,0x18,0xb8}};
// Waits for a Windows Runtime operation (polled, as winocr does). False on failure or timeout.
inline bool wait(IInspectable* op,DWORD ms){Ref<AsyncInfo> info;if(!op||FAILED(op->QueryInterface(iidAsyncInfo,(void**)info.out())))return false;
    INT32 st=0;ULONGLONG until=GetTickCount64()+ms;while(SUCCEEDED(info->Status(&st))&&st==0&&GetTickCount64()<until)Sleep(5);if(st!=1){info->Cancel();}return st==1;}
inline std::vector<dlssapprox::Box> detect(const unsigned char* gray,int w,int h){std::vector<dlssapprox::Box> out;if(w<40||h<40)return out;
    Ref<Statics> s;Ref<BitmapFactory> bitmaps;
    if(FAILED(activation(L"Windows.Media.FaceAnalysis.FaceDetector",iidStatics,(void**)s.out()))||FAILED(activation(L"Windows.Graphics.Imaging.SoftwareBitmap",iidBitmapFactory,(void**)bitmaps.out())))return out;
    Ref<DetectorOp> make;Ref<Detector> det;if(FAILED(s->CreateAsync(make.out()))||!wait(make.p,5000)||FAILED(make->GetResults(det.out()))||!det.p)return out;
    Ref<Bitmap> bitmap;if(FAILED(bitmaps->Create(62,w,h,bitmap.out())))return out; // Gray8
    {Ref<BitmapBuffer> buffer;if(FAILED(bitmap->LockBuffer(2,buffer.out())))return out;PlaneDescription plane={};buffer->PlaneDescription(0,&plane);
     Ref<MemoryBuffer> memory;Ref<IInspectable> ref;Ref<ByteAccess> bytes;BYTE* data=nullptr;UINT32 size=0;
     if(FAILED(buffer->QueryInterface(iidMemoryBuffer,(void**)memory.out()))||FAILED(memory->CreateReference(ref.out()))||FAILED(ref->QueryInterface(iidByteAccess,(void**)bytes.out()))||FAILED(bytes->GetBuffer(&data,&size))){closeIt(ref.p);closeIt(buffer.p);return out;}
     for(int y=0;y<h&&plane.start+(INT64)y*plane.stride+w<=(INT64)size;y++)memcpy(data+plane.start+y*plane.stride,gray+(size_t)y*w,w);
     closeIt(ref.p);closeIt(buffer.p);}
    Ref<FacesOp> op;Ref<Faces> faces;if(FAILED(det->DetectFacesAsync(bitmap.p,op.out()))||!wait(op.p,5000)||FAILED(op->GetResults(faces.out()))||!faces.p)return out;
    UINT32 n=0;faces->Size(&n);for(UINT32 i=0;i<n&&i<16;i++){Ref<Face> f;Box b={};if(SUCCEEDED(faces->GetAt(i,f.out()))&&f.p&&SUCCEEDED(f->FaceBox(&b)))out.push_back({(int)b.x,(int)b.y,(int)b.w,(int)b.h});}
    return out;}
}
