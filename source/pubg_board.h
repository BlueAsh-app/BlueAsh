#pragma once
#include "i18n.h"
#include "wgc_capture.h"
#include "winocr.h"
#include "countdown.h"
// PUBG plane boarding alert. TslGame.log only says a match was joined (pubg_alert.h PubgJoin), so from 10 s after
// that the PUBG window is read once a second (Windows.Graphics.Capture, works behind other windows but not when
// minimized), the countdown below the screen centre is OCR'd (white text only) and tracked (countdown.h); the alarm
// comes 2 s after it shows 1, or 2 s after its label disappears at 5 or less. Minimized, nothing can be read (a one-time tip says so).
// Each match's readings go to %LOCALAPPDATA%\ModeSwitch\pubg-countdown.log for checking. Screen pixels stay local.
// One frame of the starting-island countdown (real game, 4K screenshots): "게임 시작까지 남은 시간" at 50~57 % of the
// height, +-12 % around the centre; the number under it at 56~64 %, +-6 %. Only white text counts, and in the digit
// crop only digit-sized blobs (3.5~7.5 % of the height), so sky and clouds drop out. Windows OCR misses the tall,
// condensed digits as drawn and reads them squashed (0.8 x 0.4, 0.5 x 0.25, 0.5 x 0.4 at 2160 px height); the text
// must be a bare 1-2 digit number.
namespace countread {
struct Box {int x,y,w,h,split,labelEnd,dx0,dw;};
inline Box box(int W,int H){Box b;b.x=std::max(0,W/2-H*12/100);b.y=H*50/100;b.w=std::min(W-b.x,H*24/100);b.h=H*14/100;b.split=H*56/100-b.y;b.labelEnd=H*7/100;
    b.dx0=std::max(0,b.w/2-H*6/100);b.dw=std::min(b.w-b.dx0,H*12/100);return b;}
struct Result {bool label=false;int blobs=0,value=-1;double ratio=0;std::wstring text;std::vector<unsigned char> digits;};
// A lone narrow blob is "1": width/height 0.21~0.36 in condensed fonts against 0.45~0.60 for other digits (real 2/8: 0.49).
inline bool isOne(const Result& r){return r.blobs==1&&r.ratio>0&&r.ratio<0.40;}
// px: BGRA rows of the box (pitch bytes apart); H: full frame height.
inline Result read(winocr::Reader& ocr,const unsigned char* px,size_t pitch,const Box& b,int H){
    Result r;std::vector<unsigned char> white(size_t(b.w)*b.h);
    for(int yy=0;yy<b.h;yy++){const unsigned char* p=px+size_t(yy)*pitch;for(int xx=0;xx<b.w;xx++,p+=4){int mx=std::max(p[0],std::max(p[1],p[2])),mn=std::min(p[0],std::min(p[1],p[2]));white[size_t(yy)*b.w+xx]=(mn>200&&mx-mn<40)?0:255;}}
    // Black text on white with a margin; columns [c0,c1) and rows [r0,r1) of src resampled by sx,sy.
    auto page=[](const std::vector<unsigned char>& src,int sw,int c0,int c1,int r0,int r1,double sx,double sy,std::vector<unsigned char>& out,int& ow,int& oh){int cw=int((c1-c0)*sx),chh=int((r1-r0)*sy),pad=50;ow=cw+pad*2;oh=chh+pad*2;out.assign(size_t(ow)*oh,255);
        for(int yy=0;yy<chh;yy++)for(int xx=0;xx<cw;xx++)out[size_t(yy+pad)*ow+xx+pad]=src[size_t(r0+int(yy/sy))*sw+c0+int(xx/sx)];};
    std::vector<unsigned char> img;int iw=0,ih=0;double k=2160.0/H;
    page(white,b.w,0,b.w,0,b.labelEnd,1.0,1.0,img,iw,ih);auto label=ocr.read(img.data(),iw,ih);r.label=label.find(L"시작")!=std::wstring::npos||label.find(L"남은")!=std::wstring::npos;
    int dh=b.h-b.split;r.digits.resize(size_t(b.dw)*dh);for(int yy=0;yy<dh;yy++)memcpy(&r.digits[size_t(yy)*b.dw],&white[size_t(yy+b.split)*b.w+b.dx0],b.dw);
    std::vector<std::vector<int>> boxes;r.blobs=countdown::keepDigitBlobs(r.digits,b.dw,dh,H*35/1000,H*75/1000,&boxes);
    if(r.blobs==1){r.ratio=double(boxes[0][2]-boxes[0][0]+1)/(boxes[0][3]-boxes[0][1]+1);}
    const double squash[3][2]={{0.8,0.4},{0.5,0.25},{0.5,0.4}};
    // Windows OCR rarely reads a lone digit: one blob is read twice side by side ("5" -> "55") and must come back doubled.
    std::vector<unsigned char> src=r.digits;int sw=b.dw;bool doubled=r.blobs==1;
    if(doubled){int x0=b.dw,x1=-1;for(int yy=0;yy<dh;yy++)for(int xx=0;xx<b.dw;xx++){if(!r.digits[size_t(yy)*b.dw+xx]){x0=std::min(x0,xx);x1=std::max(x1,xx);}}
        int gw=x1-x0+1,gap=std::max(4,gw/3);sw=gw*2+gap+gw;src.assign(size_t(sw)*dh,255);
        for(int yy=0;yy<dh;yy++)for(int xx=0;xx<gw;xx++){unsigned char v=r.digits[size_t(yy)*b.dw+x0+xx];src[size_t(yy)*sw+gw/2+xx]=v;src[size_t(yy)*sw+gw/2+gw+gap+xx]=v;}}
    if(r.blobs>=1&&r.blobs<=2){for(auto& q:squash){page(src,sw,0,sw,0,dh,q[0]*k,q[1]*k,img,iw,ih);r.text=ocr.read(img.data(),iw,ih);int v=countdown::strictNumber(r.text);
        if(doubled){v=(v>=11&&v%11==0)?v/11:-1;}if(v>0){r.value=v;break;}}}
    return r;
}
}
class BoardCapture {
    std::thread worker;HANDLE stopEvent=nullptr;std::mutex mutex;std::atomic<bool> active{false};HWND targetWindow=nullptr;
    struct State {bool locked=false;double end=0,labelAt=0,numAt=0,oneAt=0,singleAt=0;int labels=0,lastNum=0,singles=0;std::wstring status=i18n::text(L"대기");} state;
    // Digit crop as a 24-bit BMP (for checking what the reader saw in a real match).
    static void saveMask(const std::wstring& path,const std::vector<unsigned char>& m,int w,int h){FILE* f=_wfopen(path.c_str(),L"wb");if(!f){return;}int row=(w*3+3)&~3;
        BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=-h;ih.biPlanes=1;ih.biBitCount=24;fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+row*h;
        fwrite(&fh,sizeof(fh),1,f);fwrite(&ih,sizeof(ih),1,f);std::vector<unsigned char> r(row,0);for(int y=0;y<h;y++){for(int x=0;x<w;x++){r[x*3]=r[x*3+1]=r[x*3+2]=m[size_t(y)*w+x];}fwrite(r.data(),1,row,f);}fclose(f);}
    void status(const std::wstring& s){std::lock_guard<std::mutex> l(mutex);state.status=s;}
    static double now(){return GetTickCount64()/1000.0;}
    void run(HWND hwnd,std::wstring logPath){
        if(FAILED(RoInitialize(RO_INIT_MULTITHREADED))){status(i18n::text(L"Windows 캡처 초기화 실패"));active=false;return;}
        FILE* log=_wfopen(logPath.c_str(),L"wb");std::wstring maskDir=logPath.substr(0,logPath.find_last_of(L'\\')+1)+L"pubg-digits-";int saved=0,frames=0;
        {using namespace wgc;Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<Pool> pool;Ptr<Session> session;winocr::Reader ocr;countdown::Tracker tracker;
        try {
            if(!ocr.open())throw std::wstring(i18n::text(L"Windows 글자 인식(OCR)을 열지 못했습니다."));
            require(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.out(),nullptr,context.out()));
            Ptr<IDXGIDevice> dxgi;require(device->QueryInterface(__uuidof(IDXGIDevice),(void**)dxgi.out()));
            using Bridge=HRESULT(WINAPI*)(IDXGIDevice*,IInspectable**);Bridge bridge=nullptr;FARPROC proc=GetProcAddress(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice");memcpy(&bridge,&proc,sizeof(bridge));if(!bridge)throw std::wstring(i18n::text(L"Windows 10 1903 이상이 필요합니다."));
            Ptr<IInspectable> raw,rt;require(bridge(dxgi.p,raw.out()));require(raw->QueryInterface(iidDevice,(void**)rt.out()));
            Ptr<Interop> interop;factory(L"Windows.Graphics.Capture.GraphicsCaptureItem",iidInterop,(void**)interop.out());
            Ptr<Item> item;require(interop->ForWindow(hwnd,iidItem,(void**)item.out()));Size size={};require(item->GetSize(&size));
            if(size.width<320||size.height<200||size.width>8192||size.height>8192)throw std::wstring(i18n::text(L"지원 범위를 벗어난 창 크기입니다."));
            Ptr<PoolFactory> pf;factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",iidPoolFactory,(void**)pf.out());require(pf->FreeThreaded(rt.p,87,2,size,pool.out()));require(pool->CreateSession(item.p,session.out()));
            Ptr<Session2> s2;if(SUCCEEDED(session->QueryInterface(iidSession2,(void**)s2.out())))s2->SetCursor(false);
            Ptr<Session5> s5;if(SUCCEEDED(session->QueryInterface(iidSession5,(void**)s5.out())))s5->SetInterval({5000000});
            noBorder(session.p);require(session->Start());Ptr<ID3D11Texture2D> staging;
            // Countdown box and reading: countread above.
            int H=size.height;auto bx=countread::box(size.width,H);int x=bx.x,y=bx.y,w=bx.w,h=bx.h;
            status(i18n::text(L"카운트다운 읽는 중"));
            while(WaitForSingleObject(stopEvent,1000)==WAIT_TIMEOUT){
                if(!IsWindow(hwnd))break;
                if(IsIconic(hwnd)){status(i18n::text(L"배그 최소화됨 · 화면을 읽을 수 없음"));continue;}
                FramePtr frame;require(pool->Next(frame.out()));if(!frame)continue;
                for(int drain=0;drain<3;drain++){FramePtr newer;require(pool->Next(newer.out()));if(!newer)break;close(frame.p);frame.reset();frame.p=newer.p;newer.p=nullptr;}
                Size content={};require(frame->ContentSize(&content));if(content.width!=size.width||content.height!=size.height)throw std::wstring(i18n::text(L"창 크기가 바뀌었습니다."));
                Ptr<IInspectable> surface;require(frame->Surface(surface.out()));Ptr<SurfaceAccess> access;require(surface->QueryInterface(iidAccess,(void**)access.out()));Ptr<ID3D11Texture2D> texture;require(access->GetInterface(__uuidof(ID3D11Texture2D),(void**)texture.out()));
                D3D11_TEXTURE2D_DESC source={};texture->GetDesc(&source);if(source.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||x+w>(int)source.Width||y+h>(int)source.Height)throw std::wstring(i18n::text(L"지원하지 않는 캡처 형식입니다."));
                if(!staging){D3D11_TEXTURE2D_DESC d={};d.Width=w;d.Height=h;d.MipLevels=1;d.ArraySize=1;d.Format=source.Format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&d,nullptr,staging.out()));}
                D3D11_BOX box={(UINT)x,(UINT)y,0,(UINT)(x+w),(UINT)(y+h),1};context->CopySubresourceRegion(staging.p,0,0,0,0,texture.p,0,&box);
                D3D11_MAPPED_SUBRESOURCE map={};require(context->Map(staging.p,0,D3D11_MAP_READ,0,&map));
                auto r=countread::read(ocr,(const unsigned char*)map.pData,map.RowPitch,bx,H);context->Unmap(staging.p,0);
                bool hasLabel=r.label;int blobs=r.blobs;std::wstring text=r.text;std::vector<int> vals;if(r.value>0){vals.push_back(r.value);}
                if(hasLabel&&saved<12&&(frames++%3==0)){saveMask(maskDir+std::to_wstring(saved)+L".bmp",r.digits,bx.dw,bx.h-bx.split);saved++;}
                double t=now();tracker.feed(t,vals);
                {std::lock_guard<std::mutex> l(mutex);state.locked=tracker.locked;state.end=tracker.end;if(hasLabel){state.labelAt=t;state.labels++;}
                 if(!vals.empty()){state.lastNum=vals.back();state.numAt=t;}
                 // Under the label: one blob = 9 or less (first seen -> singleAt); a narrow one = "1", the last second.
                 if(hasLabel&&blobs==1){if(!state.singleAt)state.singleAt=t;state.singles++;if(!state.oneAt&&(countread::isOne(r)||(!vals.empty()&&vals.back()==1)))state.oneAt=t;}
                 state.status=tracker.locked?i18n::text(L"카운트다운 확인 · 탑승 대기"):i18n::text(L"카운트다운 찾는 중");}
                if(log){std::wstring line=L"t="+std::to_wstring(t)+(hasLabel?i18n::text(L" 문구O"):i18n::text(L" 문구X"))+i18n::text(L" 덩어리")+std::to_wstring(blobs)+(blobs==1?i18n::text(L" 비율")+std::to_wstring(int(r.ratio*100)):L"")+(countread::isOne(r)?i18n::text(L" [1로 판단]"):L"")+L" ["+text+L"] ->";for(int v:vals)line+=L" "+std::to_wstring(v);if(tracker.locked)line+=i18n::text(L" | 끝 ")+std::to_wstring(tracker.end);line+=L"\r\n";
                    int n=WideCharToMultiByte(CP_UTF8,0,line.c_str(),(int)line.size(),nullptr,0,nullptr,nullptr);std::string u(n,0);WideCharToMultiByte(CP_UTF8,0,line.c_str(),(int)line.size(),&u[0],n,nullptr,nullptr);fwrite(u.data(),1,u.size(),log);fflush(log);}
            }
        }catch(const std::wstring& e){status(e);}catch(...){status(i18n::text(L"화면 읽기 실패"));}
        close(session.p);close(pool.p);}
        if(log){fclose(log);}RoUninitialize();active=false;
    }
public:
    ~BoardCapture(){stop();}
    // State is cleared on stop: a finished match's countdown must not trigger the next match (seen: alarm 10 s after a join).
    void stop(){if(stopEvent)SetEvent(stopEvent);if(worker.joinable())worker.join();if(stopEvent)CloseHandle(stopEvent);stopEvent=nullptr;active=false;targetWindow=nullptr;std::lock_guard<std::mutex> l(mutex);state={};}
    void start(HWND h,const std::wstring& logPath){stop();{std::lock_guard<std::mutex> l(mutex);state={};}stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)return;targetWindow=h;active=true;
        try{worker=std::thread(&BoardCapture::run,this,h,logPath);}catch(...){active=false;}}
    bool running()const{return active;}
    HWND target()const{return targetWindow;}
    State read(){std::lock_guard<std::mutex> l(mutex);return state;}
};
struct BoardWatch {
    BoardCapture capture;ULONGLONG retryAt=0;bool warned=false,noWindow=false;ULONGLONG seenMatch=0;
    std::wstring logPath(){wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local)))return L"";return std::wstring(local)+L"\\ModeSwitch\\pubg-countdown.log";}
    void poll(){
        if(seenMatch!=pubgMatchAt){seenMatch=pubgMatchAt;warned=false;retryAt=0;capture.stop();}
        ULONGLONG now=GetTickCount64();
        if(!pubgJoinOn||!pubgMatchAt||pubgBoarded||now-pubgMatchAt>300000){if(capture.running())capture.stop();return;}
        if(now-pubgMatchAt<10000)return; // starting island loads first; a late join can land mid-countdown
        auto s=capture.read();double nowS=now/1000.0;
        // "1" on screen -> 2 s later (the plane boards right after the countdown).
        if(s.oneAt>0&&nowS>=s.oneAt+2){board(i18n::text(L"숫자 1 확인 후 2초"));return;}
        if(s.locked&&nowS>=s.end+1){board(i18n::text(L"카운트다운 끝"));return;}
        // Backups: 10 s after the single-digit phase began (9 -> 0, +1 s); or the label gone for 2 s after it.
        if(s.singleAt>0&&nowS>=s.singleAt+10){board(i18n::text(L"한 자리 숫자 시작 후 10초"));return;}
        if(s.singles>=2&&nowS-s.labelAt>=2){board(i18n::text(L"한 자리 숫자 뒤 문구 사라짐"));return;}
        DWORD pid=pubggame::gamePid();HWND h=pid?wgc::window(pid):nullptr;
        if(!h){if(capture.running()||!noWindow){noWindow=true;pubgNote(pid?i18n::text(L"배그 창을 찾지 못함"):i18n::text(L"배그 프로세스 없음"));}capture.stop();return;}noWindow=false;
        if(IsIconic(h)&&!warned){warned=true;
            balloon::show(i18n::text(L"배그 탑승 알림"),i18n::text(L"배그가 최소화돼 있어 카운트다운을 읽을 수 없습니다. 최소화 대신 다른 창 뒤에 두세요."),L"TslGame.exe");}
        if((!capture.running()||capture.target()!=h)&&now>=retryAt){
            if(retryAt){pubgNote(i18n::text(L"화면 읽기 다시 시작 · 이전 상태: ")+s.status);}retryAt=now+5000;RECT r={};GetClientRect(h,&r);
            pubgNote(i18n::text(L"카운트다운 화면 읽기 시작 · 창 ")+std::to_wstring(r.right)+L"x"+std::to_wstring(r.bottom)+(IsIconic(h)?i18n::text(L" (최소화)"):L""));capture.start(h,logPath());}
    }
    void board(const wchar_t* how){
        pubgNote(std::wstring(i18n::text(L"탑승 알림 · "))+how);pubgMarkBoarded();capture.stop();
        // Away from the PC (no input for the helper's 자리 비움 기준): the paired phone too, even with the game in front.
        phoneAlert("pubg.board.away");
        // Playing in front of the game already: nothing to call the player back to.
        if(pubggame::foreground(pubggame::gamePid())){pubgNote(i18n::text(L"배그가 앞에 있어 알림 생략"));return;}
        if(!pubgSound())pubgNote(i18n::text(L"알림음 재생 실패"));
        balloon::show(i18n::text(L"배그 비행기 탑승"),i18n::text(L"비행기에 탑승했습니다. 자리로 돌아오세요!"),L"TslGame.exe");
    }
} boardWatch;
