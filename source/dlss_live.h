#pragma once
// Live preview of a running game window for the DLSS 5 page: Windows Graphics Capture on a worker thread, the frame
// shrunk on the GPU (copy the client area into a mip-chained texture, GenerateMips, read back one small level), so only a
// preview-sized image (at most 1920 wide, kept sharp for comparing) crosses to the CPU, at most ~30 times a second. Nothing is loaded into the
// game. The frame pool is rebuilt when the window size changes, the capture restarts after a lost device (3 tries), and a
// minimized, closed or blocked window is reported as such. Frames come in BGRA8: an HDR game is shown as Windows
// converts it for capture (SDR), which the page states.
// Timing: WGC hands over composed frames, so the arrival rate is what reaches the screen (bounded by the refresh rate),
// not the game's render rate. The page labels it as an estimate; render FPS needs a present-event trace.
#include "wgc_capture.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <algorithm>
namespace dlsslive {
enum State {Idle,Starting,Live,Minimized,Ended,Failed};
struct Stats {double fps=0,low1=0;int samples=0;};
class Capture {
    std::thread worker;std::atomic<bool> stopping{false},finished{true};std::mutex m;std::vector<unsigned char> frame;int fw=0,fh=0;unsigned seq=0;State st=Idle;std::wstring why;std::deque<double> arrivals;
    void set(State s,const std::wstring& w=L""){std::lock_guard<std::mutex> l(m);st=s;why=w;}
    // One capture session; returns false when it should be retried (lost device).
    bool session(HWND game,HWND notify,UINT msg){
        using namespace wgc;Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<Pool> pool;Ptr<Session> session;Ptr<IInspectable> rt;bool retry=false;
        try{require(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.out(),nullptr,context.out()));
            Ptr<IDXGIDevice> dxgi;require(device->QueryInterface(__uuidof(IDXGIDevice),(void**)dxgi.out()));
            using Bridge=HRESULT(WINAPI*)(IDXGIDevice*,IInspectable**);Bridge bridge=nullptr;FARPROC proc=GetProcAddress(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice");memcpy(&bridge,&proc,sizeof(bridge));
            if(!bridge)throw std::wstring(i18n::text(L"Windows 10 1903 이상이 필요합니다."));
            Ptr<IInspectable> raw;require(bridge(dxgi.p,raw.out()));require(raw->QueryInterface(iidDevice,(void**)rt.out()));
            Ptr<Interop> interop;factory(L"Windows.Graphics.Capture.GraphicsCaptureItem",iidInterop,(void**)interop.out());Ptr<Item> item;require(interop->ForWindow(game,iidItem,(void**)item.out()));
            Size size={};require(item->GetSize(&size));Ptr<PoolFactory> pf;factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",iidPoolFactory,(void**)pf.out());
            require(pf->FreeThreaded(rt.p,87,2,size,pool.out()));require(pool->CreateSession(item.p,session.out()));
            Ptr<Session2> s2;if(SUCCEEDED(session->QueryInterface(iidSession2,(void**)s2.out())))s2->SetCursor(false);noBorder(session.p);require(session->Start());
            Ptr<ID3D11Texture2D> mips,staging;Ptr<ID3D11ShaderResourceView> srv;int level=0,cw=0,ch=0,cx=0,cy=0;Size made={0,0};ULONGLONG lastShown=0;INT64 lastStamp=-1;
            set(Starting);
            while(!stopping){
                if(!IsWindow(game)){set(Ended,i18n::text(L"게임 창이 닫혔습니다."));break;}
                if(IsIconic(game)){set(Minimized,i18n::text(L"게임이 최소화돼 있어 화면을 받을 수 없습니다. 게임 창을 다른 창 뒤에 두세요."));Sleep(40);continue;}
                FramePtr fr;require(pool->Next(fr.out()));if(!fr){Sleep(4);continue;}
                Span stamp={};fr->Time(&stamp);if(stamp.value!=lastStamp){lastStamp=stamp.value;std::lock_guard<std::mutex> l(m);arrivals.push_back(stamp.value/1e7);while(arrivals.size()>2400)arrivals.pop_front();}
                Size content={};require(fr->ContentSize(&content));
                if(content.width!=size.width||content.height!=size.height){size=content;require(pool->Recreate(rt.p,87,2,size));mips.reset();continue;}
                ULONGLONG now=GetTickCount64();if(now-lastShown<33)continue; // preview at most ~30 times a second; timing above counts every frame
                Ptr<IInspectable> surface;require(fr->Surface(surface.out()));Ptr<SurfaceAccess> access;require(surface->QueryInterface(iidAccess,(void**)access.out()));
                Ptr<ID3D11Texture2D> tex;require(access->GetInterface(__uuidof(ID3D11Texture2D),(void**)tex.out()));D3D11_TEXTURE2D_DESC d={};tex->GetDesc(&d);
                if(!mips||made.width!=(INT32)d.Width||made.height!=(INT32)d.Height){
                    // Client area inside the captured window (physical pixels), then a mip chain down to <= 1280 wide.
                    RECT frameBox={},client={};POINT org={0,0};{using Set=HANDLE(WINAPI*)(HANDLE);Set setDpi=nullptr;FARPROC sp=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetThreadDpiAwarenessContext");memcpy(&setDpi,&sp,sizeof(setDpi));HANDLE old=setDpi?setDpi((HANDLE)-4):nullptr;
                        if(FAILED(DwmGetWindowAttribute(game,DWMWA_EXTENDED_FRAME_BOUNDS,&frameBox,sizeof(frameBox)))){GetWindowRect(game,&frameBox);}GetClientRect(game,&client);ClientToScreen(game,&org);if(setDpi&&old)setDpi(old);}
                    cx=std::clamp((int)(org.x-frameBox.left),0,(int)d.Width-1);cy=std::clamp((int)(org.y-frameBox.top),0,(int)d.Height-1);cw=std::min((int)client.right,(int)d.Width-cx);ch=std::min((int)client.bottom,(int)d.Height-cy);
                    if(cw<64||ch<64){cx=cy=0;cw=d.Width;ch=d.Height;}
                    level=0;while((cw>>level)>1920)level++;
                    D3D11_TEXTURE2D_DESC md={};md.Width=cw;md.Height=ch;md.MipLevels=level+1;md.ArraySize=1;md.Format=DXGI_FORMAT_B8G8R8A8_UNORM;md.SampleDesc.Count=1;md.Usage=D3D11_USAGE_DEFAULT;
                    md.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;md.MiscFlags=level?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;require(device->CreateTexture2D(&md,nullptr,mips.out()));
                    if(level)require(device->CreateShaderResourceView(mips.p,nullptr,srv.out()));
                    D3D11_TEXTURE2D_DESC sd=md;sd.Width=std::max(1,cw>>level);sd.Height=std::max(1,ch>>level);sd.MipLevels=1;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.MiscFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                    require(device->CreateTexture2D(&sd,nullptr,staging.out()));made={(INT32)d.Width,(INT32)d.Height};}
                if(d.Format!=DXGI_FORMAT_B8G8R8A8_UNORM)throw std::wstring(i18n::text(L"지원하지 않는 캡처 형식입니다."));
                D3D11_BOX box={(UINT)cx,(UINT)cy,0,(UINT)(cx+cw),(UINT)(cy+ch),1};context->CopySubresourceRegion(mips.p,0,0,0,0,tex.p,0,&box);
                if(level)context->GenerateMips(srv.p);
                context->CopySubresourceRegion(staging.p,0,0,0,0,mips.p,level,nullptr);
                D3D11_MAPPED_SUBRESOURCE map={};require(context->Map(staging.p,0,D3D11_MAP_READ,0,&map));int w=std::max(1,cw>>level),h=std::max(1,ch>>level);
                {std::lock_guard<std::mutex> l(m);frame.resize((size_t)w*h*4);for(int y=0;y<h;y++)memcpy(&frame[(size_t)y*w*4],(const unsigned char*)map.pData+(size_t)y*map.RowPitch,(size_t)w*4);fw=w;fh=h;seq++;st=Live;why.clear();}
                context->Unmap(staging.p,0);lastShown=now;PostMessageW(notify,msg,0,0);}
        }catch(const std::wstring& e){set(Failed,e);}
        catch(...){set(Failed,i18n::text(L"화면 캡처 오류"));}
        // A removed device (driver reset, GPU hang) shows up as a failed call: try a new session.
        if(device&&FAILED(device->GetDeviceRemovedReason()))retry=true;
        close(session.p);close(pool.p);return !retry;}
public:
    ~Capture(){stop();}
    void stop(){stopping=true;if(worker.joinable())worker.join();stopping=false;std::lock_guard<std::mutex> l(m);if(st==Live||st==Starting||st==Minimized){st=Idle;}arrivals.clear();}
    void start(HWND game,HWND notify,UINT msg){stop();set(Starting);finished=false;worker=std::thread([=]{if(FAILED(RoInitialize(RO_INIT_MULTITHREADED))){set(Failed,i18n::text(L"Windows 캡처 초기화 실패"));finished=true;return;}
        for(int tries=0;tries<3&&!stopping;tries++){if(session(game,notify,msg))break;set(Starting,i18n::text(L"그래픽 장치가 초기화돼 캡처를 다시 시작합니다."));Sleep(500);}
        RoUninitialize();finished=true;PostMessageW(notify,msg,0,0);});}
    bool running(){return worker.joinable()&&!finished;}
    State state(std::wstring* reason=nullptr){std::lock_guard<std::mutex> l(m);if(reason)*reason=why;return st;}
    // Copies the latest frame if it is newer than *have; returns false when nothing new.
    bool latest(std::vector<unsigned char>& out,int& w,int& h,unsigned& have){std::lock_guard<std::mutex> l(m);if(seq==have||frame.empty())return false;out=frame;w=fw;h=fh;have=seq;return true;}
    // Frame arrivals: average rate over the last two seconds, and the 1 % worst interval of the last ten seconds as a
    // rate (needs >= 100 intervals); samples = intervals in that ten-second window.
    Stats stats(){std::lock_guard<std::mutex> l(m);Stats s;if(arrivals.size()<3)return s;double end=arrivals.back(),recent=0;int nRecent=0;std::vector<double> gaps;
        for(size_t i=1;i<arrivals.size();i++){double gap=arrivals[i]-arrivals[i-1];if(end-arrivals[i]<=10.0)gaps.push_back(gap);if(end-arrivals[i]<=2.0){recent+=gap;nRecent++;}}
        if(gaps.empty()){return s;}s.samples=(int)gaps.size();s.fps=recent>0?nRecent/recent:0;
        if(gaps.size()>=100){std::sort(gaps.begin(),gaps.end());double worst=gaps[std::min(gaps.size()-1,(size_t)(gaps.size()*0.99))];s.low1=worst>0?1/worst:0;}return s;}
};
}
