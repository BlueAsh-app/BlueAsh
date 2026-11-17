#pragma once
// PUBG 실전 기록 – the live recorder. While PUBG is the front window (and 에임랩 > 실전 기록 is on), it keeps the last
// 1.5 s of two small pictures per frame (the area above the crosshair, shrunk on the GPU, and the square at the screen
// centre) and reads the mouse (read only). Nothing heavy runs until the left button is held: then the gun is read from
// its HUD icon, and when the button is released after 0.7 s or more, the frames from 1.2 s before the press to 0.3 s
// after the release are analysed on a low-priority thread (recoil_core.h, the same analysis as the Insert recorder).
// Every press is logged (the gun, the shots: hold time / the gun's fire interval + 1); long ones also get the numbers.
// Nothing is kept but the numbers (live.tsv); no picture leaves the PC. In a real fight the player also follows the
// enemy, so these numbers are a reference, not a sensitivity suggestion.
#include <deque>
#include <atomic>
namespace livecap {
using namespace wgc;
struct Frame {double t;recoil::Gray crop,center;};
std::mutex mu;std::deque<recoil::Mouse> mice;bool down=false;double pressT=0,releaseT=0;unsigned pressSeq=0,releaseSeq=0;
std::deque<Frame> ring;std::mutex ringMu;int ringScale=1,ringL=0,ringR=0; // the recent frames (shared with the Insert recorder's pre-roll)
std::thread worker;HANDLE stopEv=nullptr;bool running=false;std::atomic<int> jobs{0};std::atomic<bool> ended{false};std::mutex fileMu;
const double before=1.2,after=.3,longHold=.7,maxHold=6.0;
void onMouse(const recoil::Mouse& m){std::lock_guard<std::mutex> l(mu);mice.push_back(m);if(m.button>0&&!down){down=true;pressT=m.t;pressSeq++;}else if(m.button<0&&down){down=false;releaseT=m.t;releaseSeq++;}
    while(!mice.empty()&&mice.front().t<m.t-(down?maxHold+3:3)&&!(down&&mice.front().t>=pressT-before-.2))mice.pop_front();}
// the match the shots belong to: the join time of the current PUBG match (pubg_alert.h), 0 outside a match
long long matchStamp(){static ULONGLONG seen=0;static long long stamp=0;if(!pubgMatchAt)return 0;if(seen!=pubgMatchAt){seen=pubgMatchAt;stamp=(long long)time(nullptr)-(long long)((GetTickCount64()-pubgMatchAt)/1000);}return stamp;}
bool enabled(){return recoilcap::profileValue("live")!="0";}
// one press: at, match, gun, scope, shots, hold s, analysed, ratio, vErr, hErr, wrong %, first pull s, break shot, why
void row(long long match,const std::string& weapon,const std::string& scope,int shots,double hold,const recoil::Result* r){char b[400];
    if(r&&r->ok){recoil::Summary sm=recoil::summarize(r->each);snprintf(b,400,"%lld\t%lld\t%s\t%s\t%d\t%.2f\t1\t%.4f\t%.2f\t%.2f\t%.1f\t%.3f\t%d\t\n",(long long)time(nullptr),match,weapon.c_str(),scope.c_str(),shots,hold,r->ratio,sm.vErr,sm.hErr,sm.wrong,sm.react,sm.breakAt);}
    else snprintf(b,400,"%lld\t%lld\t%s\t%s\t%d\t%.2f\t0\t0\t0\t0\t-1\t0\t-1\t%s\n",(long long)time(nullptr),match,weapon.c_str(),scope.c_str(),shots,hold,r?r->why.c_str():"short");
    std::lock_guard<std::mutex> l(fileMu);if(FILE* f=_wfopen((recoilcap::folder()+L"\\live.tsv").c_str(),L"ab")){fputs(b,f);fclose(f);}}
int shotsFor(const std::string& weapon,double hold){double dt=recoil::fireInterval(weapon);return dt>0?(int)std::floor(hold/dt+1e-9)+1:1;}
// a long press: picture motion between the frames, the scope's still picture, then the same analysis as a recorded spray
void analyse(std::vector<Frame> fr,std::vector<recoil::Mouse> ms,recoil::Icon ic,double hold,long long match,int scale,int skipL,int skipR){
    SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    try{std::vector<recoil::Shift> s;recoil::StillMap st;recoil::Pyramid prev;bool have=false;double gx=0,gy=0;
        for(auto& x:fr){st.add(x.center);recoil::Pyramid now=recoil::pyramid(std::move(x.crop));
            if(have){recoil::Match mt=recoil::match(prev,now,gx,gy,8,skipL,skipR);if(mt.score>40){recoil::Match wide=recoil::match(prev,now,0,0,16,skipL,skipR);if(wide.score<mt.score)mt=wide;}gx=mt.dx;gy=mt.dy;s.push_back({x.t,mt.dx*scale,mt.dy*scale});}
            else{s.push_back({x.t,0,0});}prev=std::move(now);have=true;}
        std::string weapon=recoilcap::weaponOf(ic),scope=recoilcap::scopeOf(st.sig());aim::Sens sens=recoilcap::pubgSens();
        recoil::Result r=recoil::analyse(s,ms,sens.found?sens.vertical:1,recoil::fireInterval(weapon));int shots=shotsFor(weapon,hold);if(r.ok&&!r.each.empty()&&r.each[0].shots>0)shots=r.each[0].shots;
        row(match,weapon,scope,shots,hold,&r);}catch(...){}
    jobs--;}
// ---- the capture thread -------------------------------------------------------------------------------------------
void capture(HWND hwnd){
    if(FAILED(RoInitialize(RO_INIT_MULTITHREADED))){ended=true;return;}
    {Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<Pool> pool;Ptr<Session> session;
    try{
        require(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.out(),nullptr,context.out()));
        Ptr<IDXGIDevice> dxgi;require(device->QueryInterface(__uuidof(IDXGIDevice),(void**)dxgi.out()));
        using Bridge=HRESULT(WINAPI*)(IDXGIDevice*,IInspectable**);Bridge bridge=nullptr;FARPROC proc=GetProcAddress(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice");memcpy(&bridge,&proc,sizeof(bridge));if(!bridge)throw std::wstring(L"os");
        Ptr<IInspectable> raw,rt;require(bridge(dxgi.p,raw.out()));require(raw->QueryInterface(iidDevice,(void**)rt.out()));
        Ptr<Interop> interop;factory(L"Windows.Graphics.Capture.GraphicsCaptureItem",iidInterop,(void**)interop.out());
        Ptr<Item> item;require(interop->ForWindow(hwnd,iidItem,(void**)item.out()));Size size={};require(item->GetSize(&size));if(size.width<640||size.height<360||size.width>8192||size.height>8192)throw std::wstring(L"size");
        Ptr<PoolFactory> pf;factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",iidPoolFactory,(void**)pf.out());require(pf->FreeThreaded(rt.p,87,2,size,pool.out()));require(pool->CreateSession(item.p,session.out()));
        Ptr<Session2> s2;if(SUCCEEDED(session->QueryInterface(iidSession2,(void**)s2.out())))s2->SetCursor(false);
        noBorder(session.p);require(session->Start());
        // the same area as the Insert recorder (30..70 % across, 18..46 % down), shrunk on the GPU to >= 256 wide; the
        // centre square is 48 << lv so its mip lv is exactly 48 x 48 (the same picture the Insert recorder keeps)
        int W=size.width,H=size.height,cx=W*30/100,cy=H*18/100,cw=W*40/100,ch=H*28/100,L=0;while((cw>>(L+1))>=256)L++;
        int lv=0;while((48<<(lv+1))<=H*30/100)lv++;int cs=48<<lv,ccx=W/2-cs/2,ccy=H/2-cs/2,gw=cw>>L,skip=(W*3/100)>>L,mid=gw/2;
        int ix0,iy0;recoil::iconCorner(W,H,ix0,iy0);
        auto mipTex=[&](int w,int h,int levels,Ptr<ID3D11Texture2D>& t,Ptr<ID3D11ShaderResourceView>& v,Ptr<ID3D11Texture2D>& stg,int sw,int sh){D3D11_TEXTURE2D_DESC d={};d.Width=w;d.Height=h;d.MipLevels=levels+1;d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_DEFAULT;
            d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;d.MiscFlags=levels?D3D11_RESOURCE_MISC_GENERATE_MIPS:0;require(device->CreateTexture2D(&d,nullptr,t.out()));if(levels)require(device->CreateShaderResourceView(t.p,nullptr,v.out()));
            D3D11_TEXTURE2D_DESC sd=d;sd.Width=sw;sd.Height=sh;sd.MipLevels=1;sd.Usage=D3D11_USAGE_STAGING;sd.BindFlags=0;sd.MiscFlags=0;sd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&sd,nullptr,stg.out()));};
        Ptr<ID3D11Texture2D> cropM,cropS,cenM,cenS,corner;Ptr<ID3D11ShaderResourceView> cropV,cenV;mipTex(cw,ch,L,cropM,cropV,cropS,cw>>L,ch>>L);mipTex(cs,cs,lv,cenM,cenV,cenS,48,48);
        {D3D11_TEXTURE2D_DESC d={};d.Width=W-ix0;d.Height=H-iy0;d.MipLevels=1;d.ArraySize=1;d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&d,nullptr,corner.out()));}
        {std::lock_guard<std::mutex> l(ringMu);ring.clear();ringScale=1<<L;ringL=mid-skip;ringR=mid+skip;}unsigned seenPress,seenRelease;{std::lock_guard<std::mutex> l(mu);seenPress=pressSeq;seenRelease=releaseSeq;}bool collecting=false;double clipPress=0,lastT=0;recoil::Icon clipIcon;
        while(WaitForSingleObject(stopEv,1)==WAIT_TIMEOUT){
            if(!IsWindow(hwnd))break;
            FramePtr frame;require(pool->Next(frame.out()));if(!frame)continue;
            Span when={};frame->Time(&when);double t=when.value*1e-7;if(t-lastT<1/125.0)continue;lastT=t; // at most ~125 frames a second
            Ptr<IInspectable> surface;require(frame->Surface(surface.out()));Ptr<SurfaceAccess> access;require(surface->QueryInterface(iidAccess,(void**)access.out()));Ptr<ID3D11Texture2D> texture;require(access->GetInterface(__uuidof(ID3D11Texture2D),(void**)texture.out()));
            D3D11_TEXTURE2D_DESC src={};texture->GetDesc(&src);if(src.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||(int)src.Width<W||(int)src.Height<H)break; // resized: poll() starts again
            D3D11_BOX box={(UINT)cx,(UINT)cy,0,(UINT)(cx+cw),(UINT)(cy+ch),1};context->CopySubresourceRegion(cropM.p,0,0,0,0,texture.p,0,&box);if(L)context->GenerateMips(cropV.p);context->CopySubresourceRegion(cropS.p,0,0,0,0,cropM.p,L,nullptr);
            D3D11_BOX cb={(UINT)ccx,(UINT)ccy,0,(UINT)(ccx+cs),(UINT)(ccy+cs),1};context->CopySubresourceRegion(cenM.p,0,0,0,0,texture.p,0,&cb);if(lv)context->GenerateMips(cenV.p);context->CopySubresourceRegion(cenS.p,0,0,0,0,cenM.p,lv,nullptr);
            Frame fr;fr.t=t;D3D11_MAPPED_SUBRESOURCE m={};require(context->Map(cropS.p,0,D3D11_MAP_READ,0,&m));fr.crop=recoil::shrink((const uint8_t*)m.pData,(int)m.RowPitch,cw>>L,ch>>L,1);context->Unmap(cropS.p,0);
            require(context->Map(cenS.p,0,D3D11_MAP_READ,0,&m));fr.center=recoil::shrink((const uint8_t*)m.pData,(int)m.RowPitch,48,48,1);context->Unmap(cenS.p,0);
            {std::lock_guard<std::mutex> l(ringMu);ring.push_back(std::move(fr));}
            bool isDown;double pT,rT;unsigned pS,rS;{std::lock_guard<std::mutex> l(mu);isDown=down;pT=pressT;rT=releaseT;pS=pressSeq;rS=releaseSeq;}
            if(pS!=seenPress){seenPress=pS;collecting=true;clipPress=pT; // a press: the gun from its icon in this frame
                D3D11_BOX ib={(UINT)ix0,(UINT)iy0,0,(UINT)W,(UINT)H,1};context->CopySubresourceRegion(corner.p,0,0,0,0,texture.p,0,&ib);if(SUCCEEDED(context->Map(corner.p,0,D3D11_MAP_READ,0,&m))){clipIcon=recoil::iconFromCorner((const uint8_t*)m.pData,(int)m.RowPitch,W,H);context->Unmap(corner.p,0);}}
            bool cut=collecting&&isDown&&t-clipPress>maxHold;double rel=cut?clipPress+maxHold:rT;
            if(collecting&&(cut||(rS!=seenRelease&&rT>=clipPress))&&t>=rel+after){collecting=false;if(!cut)seenRelease=rS;double hold=rel-clipPress;long long match=matchStamp();std::string weapon=recoilcap::weaponOf(clipIcon);
                if(hold>=longHold&&jobs<2){std::vector<Frame> clip;{std::lock_guard<std::mutex> l(ringMu);for(auto& x:ring)if(x.t>=clipPress-before&&x.t<=rel+after)clip.push_back(x);}std::vector<recoil::Mouse> ms;{std::lock_guard<std::mutex> l(mu);for(auto& q:mice)if(q.t>=clipPress-before-.1&&q.t<=rel+after+.05)ms.push_back(q);}
                    if(cut)ms.push_back({rel,0,0,-1}); // a very long hold: analysed as if released at 6 s
                    jobs++;try{std::thread(analyse,std::move(clip),std::move(ms),clipIcon,hold,match,1<<L,mid-skip,mid+skip).detach();}catch(...){jobs--;}}
                else row(match,weapon,"",shotsFor(weapon,hold),hold,nullptr);}
            {std::lock_guard<std::mutex> l(ringMu);while(!ring.empty()&&ring.front().t<t-(collecting?maxHold+before+1:3.2))ring.pop_front();}}
    }catch(...){}
    close(session.p);close(pool.p);}
    RoUninitialize();ended=true;}
// The Insert recorder's pre-roll: the last 3 s of frames and mouse (empty when the live recorder is not running).
bool preroll(std::vector<std::pair<double,recoil::Gray>>& fr,std::vector<recoil::Mouse>& ms,int& scale,int& skipL,int& skipR){if(!running)return false;double now=recoilcap::qpcNow();
    {std::lock_guard<std::mutex> l(ringMu);for(auto& x:ring)if(x.t>=now-3.0)fr.push_back({x.t,x.crop});scale=ringScale;skipL=ringL;skipR=ringR;}
    {std::lock_guard<std::mutex> l(mu);for(auto& q:mice)if(q.t>=now-3.1&&q.button==0)ms.push_back(q);}return fr.size()>=12;}
const bool prerollHook=(recoilcap::preroll=preroll,true);
// ---- on / off: PUBG in front, the option on, the Insert recorder not running -----------------------------------------
void stop(){if(!running)return;if(stopEv)SetEvent(stopEv);if(worker.joinable())worker.join();if(stopEv){CloseHandle(stopEv);stopEv=nullptr;}running=false;
    recoilcap::liveMouse=nullptr;recoilcap::liveInput=false;if(recoilcap::state!=recoilcap::Recording&&recoilcap::badge){RAWINPUTDEVICE rid={1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&rid,1,sizeof(rid));}
    std::lock_guard<std::mutex> l(mu);mice.clear();down=false;}
void start(HWND game){recoilcap::ensureBadge();if(!recoilcap::badge)return;RAWINPUTDEVICE rid={1,2,RIDEV_INPUTSINK,recoilcap::badge};if(!RegisterRawInputDevices(&rid,1,sizeof(rid)))return;
    recoilcap::liveInput=true;recoilcap::liveMouse=onMouse;stopEv=CreateEventW(nullptr,TRUE,FALSE,nullptr);ended=false;running=true;try{worker=std::thread(capture,game);}catch(...){stop();}}
void poll(){
#ifdef BLUEASH_TT
    if(const char* e=getenv("BLUEASH_LIVE_HWND")){HWND h=(HWND)(uintptr_t)strtoull(e,nullptr,16);if(running&&ended)stop();if(!running&&IsWindow(h))start(h);return;} /* test builds: record any window, to measure the cost */
#endif
    DWORD pid=pubggame::gamePid();bool front=pid&&pubggame::foreground(pid);bool want=front&&enabled()&&recoilcap::state!=recoilcap::Recording;
    if(running&&ended)stop(); // the capture ended by itself (window resized or closed): start again next time
    if(want&&!running){HWND h=wgc::window(pid);if(h)start(h);}else if(!want&&running)stop();}
}
