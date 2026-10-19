#pragma once
#include "wgc_capture.h"
#include "raid_deploy_read.h"
// Only armed after a new match log. WGC runs off the UI thread and copies just the deployment panel.
class RaidDeployCapture {
 std::thread worker;HANDLE stopEvent=nullptr;std::atomic<bool> active{false};HWND targetWindow=nullptr;std::mutex mutex;std::wstring note;raiddeploy::Reading latest;
 void status(const std::wstring& s){std::lock_guard<std::mutex> l(mutex);note=s;}
 void run(HWND hwnd,HWND owner,unsigned token){
  if(FAILED(RoInitialize(RO_INIT_MULTITHREADED))){status(i18n::text(L"Windows 캡처 초기화 실패"));active=false;return;}
  {using namespace wgc;Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<Pool> pool;Ptr<Session> session;
  try{
   winocr::Reader ocr;if(!ocr.open())throw std::wstring(i18n::text(L"Windows 글자 인식(OCR)을 열지 못했습니다."));
   require(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.out(),nullptr,context.out()));
   Ptr<IDXGIDevice> dxgi;require(device->QueryInterface(__uuidof(IDXGIDevice),(void**)dxgi.out()));
   using Bridge=HRESULT(WINAPI*)(IDXGIDevice*,IInspectable**);Bridge bridge=nullptr;FARPROC proc=GetProcAddress(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice");memcpy(&bridge,&proc,sizeof(bridge));if(!bridge)throw std::wstring(i18n::text(L"Windows 10 1903 이상이 필요합니다."));
   Ptr<IInspectable> raw,rt;require(bridge(dxgi.p,raw.out()));require(raw->QueryInterface(iidDevice,(void**)rt.out()));
   Ptr<Interop> interop;factory(L"Windows.Graphics.Capture.GraphicsCaptureItem",iidInterop,(void**)interop.out());Ptr<Item> item;require(interop->ForWindow(hwnd,iidItem,(void**)item.out()));Size size={};require(item->GetSize(&size));
   if(size.width<320||size.height<200||size.width>8192||size.height>8192)throw std::wstring(i18n::text(L"지원 범위를 벗어난 창 크기입니다."));
   Ptr<PoolFactory> pf;factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",iidPoolFactory,(void**)pf.out());require(pf->FreeThreaded(rt.p,87,2,size,pool.out()));require(pool->CreateSession(item.p,session.out()));
   Ptr<Session2> s2;if(SUCCEEDED(session->QueryInterface(iidSession2,(void**)s2.out())))s2->SetCursor(false);
   Ptr<Session5> s5;if(SUCCEEDED(session->QueryInterface(iidSession5,(void**)s5.out())))s5->SetInterval({1000000});noBorder(session.p);require(session->Start());
   Ptr<ID3D11Texture2D> staging;auto b=raiddeploy::box(size.width,size.height);raiddeploy::Tracker tracker;INT64 lastFrame=-1;
   while(WaitForSingleObject(stopEvent,100)==WAIT_TIMEOUT){
    if(!IsWindow(hwnd))break;
    if(IsIconic(hwnd)){tracker={};status(i18n::text(L"게임이 최소화되어 카운트다운을 읽을 수 없습니다."));continue;}
    FramePtr frame;require(pool->Next(frame.out()));if(!frame)continue;
    for(int n=0;n<3;n++){FramePtr newer;require(pool->Next(newer.out()));if(!newer)break;close(frame.p);frame.reset();frame.p=newer.p;newer.p=nullptr;}
    Span stamp={};require(frame->Time(&stamp));if(stamp.value<=lastFrame)continue;lastFrame=stamp.value;
    Size content={};require(frame->ContentSize(&content));if(content.width!=size.width||content.height!=size.height)throw std::wstring(i18n::text(L"창 크기가 바뀌었습니다."));
    Ptr<IInspectable> surface;require(frame->Surface(surface.out()));Ptr<SurfaceAccess> access;require(surface->QueryInterface(iidAccess,(void**)access.out()));Ptr<ID3D11Texture2D> texture;require(access->GetInterface(__uuidof(ID3D11Texture2D),(void**)texture.out()));
    D3D11_TEXTURE2D_DESC source={};texture->GetDesc(&source);if(source.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||b.x+b.w>(int)source.Width||b.y+b.h>(int)source.Height)throw std::wstring(i18n::text(L"지원하지 않는 캡처 형식입니다."));
    if(!staging){D3D11_TEXTURE2D_DESC d={};d.Width=b.w;d.Height=b.h;d.MipLevels=1;d.ArraySize=1;d.Format=source.Format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&d,nullptr,staging.out()));}
    D3D11_BOX crop={(UINT)b.x,(UINT)b.y,0,(UINT)(b.x+b.w),(UINT)(b.y+b.h),1};context->CopySubresourceRegion(staging.p,0,0,0,0,texture.p,0,&crop);
    D3D11_MAPPED_SUBRESOURCE map={};require(context->Map(staging.p,0,D3D11_MAP_READ,0,&map));
    raiddeploy::Reading r;try{r=raiddeploy::read(ocr,(const unsigned char*)map.pData,map.RowPitch,b,size.height);}catch(...){context->Unmap(staging.p,0);throw;}context->Unmap(staging.p,0);
    {std::lock_guard<std::mutex> l(mutex);latest=r;note=i18n::text(L"배치 화면 확인 중 · 1초 전에 알림");}
    // Timestamp belongs to this frame, so OCR duration cannot make a frozen or old frame count twice.
    if(tracker.feed(stamp.value/10000000.0,r.ms,r.ready)){if(WaitForSingleObject(stopEvent,0)==WAIT_TIMEOUT)PostMessageW(owner,WM_APP+182,token,0);break;}
   }
  }catch(const std::wstring& e){status(e);}catch(...){status(i18n::text(L"화면 읽기 실패"));}
  close(session.p);close(pool.p);}
  RoUninitialize();active=false;
 }
public:
 ~RaidDeployCapture(){stop();}
 void stop(){if(stopEvent)SetEvent(stopEvent);if(worker.joinable())worker.join();if(stopEvent)CloseHandle(stopEvent);stopEvent=nullptr;active=false;targetWindow=nullptr;}
 void start(HWND h,HWND owner,unsigned token){stop();{std::lock_guard<std::mutex> l(mutex);latest={};}stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)return;targetWindow=h;active=true;status(i18n::text(L"배치 화면 확인 중 · 1초 전에 알림"));try{worker=std::thread(&RaidDeployCapture::run,this,h,owner,token);}catch(...){active=false;}}
 bool running()const{return active;}
 HWND target()const{return targetWindow;}
 std::wstring read(){std::lock_guard<std::mutex> l(mutex);return note;}
 raiddeploy::Reading reading(){std::lock_guard<std::mutex> l(mutex);return latest;}
};
