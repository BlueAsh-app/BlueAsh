#pragma once
// PUBG 반동 분석 – the recorder. In PUBG (front window), the player's key (Insert unless changed in 에임랩) starts it:
// a capture thread keeps, per frame, only how far the picture around the crosshair moved (recoil_core.h match()), the
// main thread keeps the raw mouse counts (read only, RIDEV_INPUTSINK on the badge window), and a click-through badge at
// the top middle of PUBG says it is recording. The key again stops it: the clip is analysed at once (no video is kept or
// sent anywhere), the result is shown on the badge and stored under %LOCALAPPDATA%\BlueAsh\aimlab (recoil.tsv, one
// file of traces per capture). The held gun is told by its HUD icon once the player has named it (weapons.tsv).
// Nothing is written to PUBG, nothing is read from its memory, no input is sent.
#include "recoil_core.h"
#include "aim_core.h"
#include <sstream>
namespace recoilcap {
using namespace wgc;
std::wstring folder(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+dataSub();CreateDirectoryW(r.c_str(),nullptr);
#ifdef BLUEASH_TT
    r+=L"\\aimlab-test";
#else
    r+=L"\\aimlab";
#endif
    CreateDirectoryW(r.c_str(),nullptr);return r;}
std::string readFile(const std::wstring& p){std::string s;FILE* f=_wfopen(p.c_str(),L"rb");if(!f){return s;}char b[65536];size_t n;while((n=fread(b,1,sizeof(b),f))>0)s.append(b,n);fclose(f);return s;}
std::string profileValue(const char* key){std::istringstream in(readFile(folder()+L"\\profile.txt"));std::string l,k=std::string(key)+"=";while(std::getline(in,l)){if(!l.empty()&&l.back()=='\r')l.pop_back();if(l.rfind(k,0)==0)return l.substr(k.size());}return "";}
unsigned key(){std::string v=profileValue("rkey");return v.empty()?VK_INSERT:(unsigned)atoi(v.c_str());}
aim::Sens pubgSens(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);return aim::readIni(readFile(std::wstring(d)+L"\\TslGame\\Saved\\Config\\WindowsNoEditor\\GameUserSettings.ini"));}
double qpcNow(){static LARGE_INTEGER f={};if(!f.QuadPart)QueryPerformanceFrequency(&f);LARGE_INTEGER c;QueryPerformanceCounter(&c);return double(c.QuadPart)/f.QuadPart;}

enum State {Idle,Recording,Showing};
State state=Idle;HWND badge=nullptr,game=nullptr;double startedAt=0,shownAt=0;std::wstring title,line;COLORREF tone=0;int mark=0; // mark: 0 recording dot, 1 check, 2 warning
// pre-roll: the last ~3 s the live recorder kept before the key (frames shrunk as it shrinks them, its scale and the
// columns to skip), so the ordinary aim movement before the first shot measures pixels per count (no wiggle needed)
bool(*preroll)(std::vector<std::pair<double,recoil::Gray>>&,std::vector<recoil::Mouse>&,int&,int&,int&)=nullptr;std::vector<std::pair<double,recoil::Gray>> pre;int preScale=1,preL=0,preR=0;
std::mutex mu;std::vector<recoil::Shift> shifts;std::vector<recoil::Mouse> mice;recoil::Icon icon;recoil::StillMap still;bool liveInput=false;void(*liveMouse)(const recoil::Mouse&)=nullptr;std::thread worker;HANDLE stopEvent=nullptr;std::wstring failure;
void(*onSaved)()=nullptr; // the 에임랩 page refreshes its list

// ---- the capture thread -------------------------------------------------------------------------------------------
void capture(HWND hwnd){
    if(FAILED(RoInitialize(RO_INIT_MULTITHREADED))){std::lock_guard<std::mutex> l(mu);failure=L"init";return;}
    {std::vector<recoil::Shift> out;recoil::Pyramid pp;bool hv=false;double ax=0,ay=0;for(auto& f:pre){recoil::Pyramid now=recoil::pyramid(f.second);
        if(hv){recoil::Match mt=recoil::match(pp,now,ax,ay,8,preL,preR);if(mt.score>40){recoil::Match wide=recoil::match(pp,now,0,0,16,preL,preR);if(wide.score<mt.score)mt=wide;}ax=mt.dx;ay=mt.dy;out.push_back({f.first,mt.dx*preScale,mt.dy*preScale});}
        else{out.push_back({f.first,0,0});}pp=std::move(now);hv=true;}
     pre.clear();std::lock_guard<std::mutex> l(mu);shifts.insert(shifts.begin(),out.begin(),out.end());}
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
        // the picture around the crosshair, above the character (third person) and inside a scope: 30..70 % across, 18..46 % down
        int W=size.width,H=size.height,cx=W*30/100,cy=H*18/100,cw=W*40/100,ch=H*28/100,f=std::max(1,cw/320),skip=W*3/100/f,mid=cw/f/2;
        int lv=0;while((48<<(lv+1))<=H*30/100)lv++;int fc=1<<lv,cs=48<<lv,ccx=W/2-cs/2,ccy=H/2-cs/2;unsigned nth=0; /* 48 << lv: the same square the live recorder shrinks on the GPU */
        Ptr<ID3D11Texture2D> crop,full,cen;recoil::Pyramid prev;bool have=false;double gx=0,gy=0,lastT=0;
        while(WaitForSingleObject(stopEvent,1)==WAIT_TIMEOUT){
            if(!IsWindow(hwnd))break;
            FramePtr frame;require(pool->Next(frame.out()));if(!frame)continue;
            Span when={};frame->Time(&when);double t=when.value*1e-7;if(t<=lastT)continue;lastT=t;
            Ptr<IInspectable> surface;require(frame->Surface(surface.out()));Ptr<SurfaceAccess> access;require(surface->QueryInterface(iidAccess,(void**)access.out()));Ptr<ID3D11Texture2D> texture;require(access->GetInterface(__uuidof(ID3D11Texture2D),(void**)texture.out()));
            D3D11_TEXTURE2D_DESC src={};texture->GetDesc(&src);if(src.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||(int)src.Width<W||(int)src.Height<H)throw std::wstring(L"format");
            if(!full){ // once: the whole frame, for the gun's HUD icon
                D3D11_TEXTURE2D_DESC d={};d.Width=W;d.Height=H;d.MipLevels=1;d.ArraySize=1;d.Format=src.Format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&d,nullptr,full.out()));
                D3D11_BOX box={0,0,0,(UINT)W,(UINT)H,1};context->CopySubresourceRegion(full.p,0,0,0,0,texture.p,0,&box);D3D11_MAPPED_SUBRESOURCE m={};
                if(SUCCEEDED(context->Map(full.p,0,D3D11_MAP_READ,0,&m))){recoil::Icon ic=recoil::iconFrom((const uint8_t*)m.pData,(int)m.RowPitch,W,H);context->Unmap(full.p,0);std::lock_guard<std::mutex> l(mu);icon=ic;}}
            if(!crop){D3D11_TEXTURE2D_DESC d={};d.Width=cw;d.Height=ch;d.MipLevels=1;d.ArraySize=1;d.Format=src.Format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&d,nullptr,crop.out()));}
            D3D11_BOX box={(UINT)cx,(UINT)cy,0,(UINT)(cx+cw),(UINT)(cy+ch),1};context->CopySubresourceRegion(crop.p,0,0,0,0,texture.p,0,&box);
            D3D11_MAPPED_SUBRESOURCE m={};require(context->Map(crop.p,0,D3D11_MAP_READ,0,&m));recoil::Gray g=recoil::shrink((const uint8_t*)m.pData,(int)m.RowPitch,cw,ch,f);context->Unmap(crop.p,0);
            if(++nth%3==0){if(!cen){D3D11_TEXTURE2D_DESC d={};d.Width=cs;d.Height=cs;d.MipLevels=1;d.ArraySize=1;d.Format=src.Format;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;require(device->CreateTexture2D(&d,nullptr,cen.out()));}
                D3D11_BOX cb={(UINT)ccx,(UINT)ccy,0,(UINT)(ccx+cs),(UINT)(ccy+cs),1};context->CopySubresourceRegion(cen.p,0,0,0,0,texture.p,0,&cb);D3D11_MAPPED_SUBRESOURCE cm={};
                if(SUCCEEDED(context->Map(cen.p,0,D3D11_MAP_READ,0,&cm))){recoil::Gray cg=recoil::shrink((const uint8_t*)cm.pData,(int)cm.RowPitch,48*fc,48*fc,fc);context->Unmap(cen.p,0);std::lock_guard<std::mutex> l(mu);still.add(cg);}}
            recoil::Pyramid now=recoil::pyramid(std::move(g));
            if(have){recoil::Match mt=recoil::match(prev,now,gx,gy,8,mid-skip,mid+skip);if(mt.score>40){recoil::Match wide=recoil::match(prev,now,0,0,16,mid-skip,mid+skip);if(wide.score<mt.score)mt=wide;} // a fast flick: search wider once
                gx=mt.dx;gy=mt.dy;std::lock_guard<std::mutex> l(mu);shifts.push_back({t,mt.dx*f,mt.dy*f});}
            else{std::lock_guard<std::mutex> l(mu);shifts.push_back({t,0,0});}
            prev=std::move(now);have=true;}
    }catch(const std::wstring& e){std::lock_guard<std::mutex> l(mu);failure=e;}catch(...){std::lock_guard<std::mutex> l(mu);failure=L"capture";}
    close(session.p);close(pool.p);}
    RoUninitialize();}

// ---- the badge: a click-through pill at the top middle of PUBG ------------------------------------------------------
// The result as a card: the gun and how many magazines decided it, then two tiles side by side, 수직 감도 and the
// scope's sensitivity (from -> to, the change in %), so both numbers to type into PUBG are seen at a glance.
struct Card {bool on=false;std::wstring gun,count,vFrom,vTo,vDelta,sLabel,sFrom,sTo,sDelta,hint;};Card card;
void drawCard(){RECT rc={};if(!GetClientRect(game,&rc)||rc.bottom<200)return;POINT org={0,0};ClientToScreen(game,&org);double k=std::max(.8,rc.bottom/1080.0);auto K=[&](double v){return (int)std::lround(v*k);};
    HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
    HFONT head=CreateFontW(-K(17),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily()),val=CreateFontW(-K(26),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily()),
          small=CreateFontW(-K(13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily());
    auto width=[&](HFONT f,const std::wstring& s){SIZE z={};HGDIOBJ o=SelectObject(dc,f);GetTextExtentPoint32W(dc,s.c_str(),(int)s.size(),&z);SelectObject(dc,o);return (int)z.cx;};
    int pad=K(18),tileW=K(230),gap=K(12),w=pad*2+tileW*2+gap,headH=K(30),tileH=K(84),hintH=card.hint.empty()?0:K(22),h=pad+headH+K(10)+tileH+hintH+pad;
    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* bits=nullptr;HBITMAP dib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    int ty=pad+headH+K(10);RECT tile[2]={{pad,ty,pad+tileW,ty+tileH},{pad+tileW+gap,ty,w-pad,ty+tileH}};
    if(dib&&painting::token){HGDIOBJ ob=SelectObject(dc,dib);memset(bits,0,size_t(w)*h*4);
        {Gdiplus::Bitmap canvas(w,h,w*4,PixelFormat32bppPARGB,(BYTE*)bits);Gdiplus::Graphics g(&canvas);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath p;painting::path(p,{0,0,w,h},K(22));Gdiplus::SolidBrush fill(Gdiplus::Color(240,10,17,36));g.FillPath(&fill,&p);Gdiplus::Pen edge(Gdiplus::Color(170,74,112,224),(float)std::max(1.0,1.2*k));g.DrawPath(&edge,&p);
            float r=K(11),cx=pad+r,cy=pad+headH/2.f;Gdiplus::SolidBrush b(Gdiplus::Color(255,52,92,255));g.FillEllipse(&b,cx-r,cy-r,r*2,r*2);Gdiplus::Pen c(Gdiplus::Color(255,255,255,255),(float)(2.2*k));painting::roundCaps(c);g.DrawLine(&c,cx-r*.42f,cy,cx-r*.1f,cy+r*.34f);g.DrawLine(&c,cx-r*.1f,cy+r*.34f,cx+r*.45f,cy-r*.36f);
            for(int i=0;i<2;i++){Gdiplus::GraphicsPath tp;painting::path(tp,tile[i],K(14));Gdiplus::SolidBrush tf(i==0?Gdiplus::Color(255,24,40,82):Gdiplus::Color(255,20,32,64));g.FillPath(&tf,&tp);Gdiplus::Pen te(i==0?Gdiplus::Color(200,72,118,255):Gdiplus::Color(140,58,80,130),(float)std::max(1.0,1.0*k));g.DrawPath(&te,&tp);}}
        void* mbits=nullptr;HBITMAP mdib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&mbits,nullptr,0);
        if(mdib){HDC mdc=CreateCompatibleDC(screen);HGDIOBJ mob=SelectObject(mdc,mdib);SetBkMode(mdc,TRANSPARENT);unsigned char* px=(unsigned char*)bits;
            auto text=[&](HFONT ft,const std::wstring& s,int x,int y,COLORREF col){if(s.empty())return;memset(mbits,0,size_t(w)*h*4);HGDIOBJ o=SelectObject(mdc,ft);SetTextColor(mdc,RGB(255,255,255));TextOutW(mdc,x,y,s.c_str(),(int)s.size());SelectObject(mdc,o);GdiFlush();
                const unsigned char* mk=(const unsigned char*)mbits;for(size_t i=0;i<size_t(w)*h;i++){double a=std::max(mk[i*4],std::max(mk[i*4+1],mk[i*4+2]))/255.0;if(a<=0)continue;unsigned char* q=px+i*4;
                    q[0]=(unsigned char)(q[0]*(1-a)+GetBValue(col)*a);q[1]=(unsigned char)(q[1]*(1-a)+GetGValue(col)*a);q[2]=(unsigned char)(q[2]*(1-a)+GetRValue(col)*a);q[3]=(unsigned char)std::max<double>(q[3],255*a);}};
            int hy=pad+(headH-K(20))/2;text(head,card.gun,pad+K(30),hy,RGB(244,247,255));text(small,card.count,w-pad-width(small,card.count),hy+K(3),RGB(160,176,210));
            auto tileText=[&](int i,const std::wstring& label,const std::wstring& from,const std::wstring& to,const std::wstring& delta){RECT t=tile[i];int x=t.left+K(14);
                text(small,label,x,t.top+K(10),RGB(160,176,210));
                if(to.empty()){text(val,from,x,t.top+K(30),RGB(150,160,184));return;}
                std::wstring arrow=L"  \x2192  ";text(val,from,x,t.top+K(30),RGB(200,210,232));int ax=x+width(val,from);text(val,arrow,ax,t.top+K(30),RGB(110,130,170));text(val,to,ax+width(val,arrow),t.top+K(30),RGB(130,175,255));
                text(small,delta,t.right-K(14)-width(small,delta),t.top+K(10),RGB(96,210,150));};
            tileText(0,i18n::text(L"수직 감도"),card.vFrom,card.vTo,card.vDelta);tileText(1,card.sLabel,card.sFrom,card.sTo,card.sDelta);
            if(!card.hint.empty())text(small,card.hint,pad,ty+tileH+K(6),RGB(160,176,210));
            SelectObject(mdc,mob);DeleteDC(mdc);DeleteObject(mdib);}
        POINT dst={org.x+(rc.right-w)/2,org.y+(int)std::lround(rc.bottom*.085)},from={0,0};SIZE size={w,h};BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
#ifdef BLUEASH_TT
        if(const char* dump=getenv("BLUEASH_RECOIL_DUMP")){static int n=0;std::string path=std::string(dump)+"-card"+std::to_string(n++)+".bmp";FILE* f=fopen(path.c_str(),"wb");if(f){int row=(w*3+3)&~3;BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=-h;ih.biPlanes=1;ih.biBitCount=24;fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+row*h;fwrite(&fh,sizeof(fh),1,f);fwrite(&ih,sizeof(ih),1,f);std::vector<unsigned char> rr(row,0);const unsigned char* q=(const unsigned char*)bits;for(int y=0;y<h;y++){for(int x=0;x<w;x++){const unsigned char* p=q+(size_t(y)*w+x)*4;int a=p[3];rr[x*3]=(unsigned char)(p[0]+(150*(255-a))/255);rr[x*3+1]=(unsigned char)(p[1]+(140*(255-a))/255);rr[x*3+2]=(unsigned char)(p[2]+(120*(255-a))/255);}fwrite(rr.data(),1,row,f);}fclose(f);}}
#endif
        UpdateLayeredWindow(badge,screen,&dst,&size,dc,&from,0,&bf,ULW_ALPHA);SetWindowPos(badge,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);SelectObject(dc,ob);}
    if(dib){DeleteObject(dib);}DeleteObject(head);DeleteObject(val);DeleteObject(small);DeleteDC(dc);ReleaseDC(nullptr,screen);}
void draw(){if(!badge||!game)return;if(state==Showing&&card.on){drawCard();return;}RECT rc={};if(!GetClientRect(game,&rc)||rc.bottom<200)return;POINT org={0,0};ClientToScreen(game,&org);double k=std::max(.8,rc.bottom/1080.0);
    std::wstring t1=title,t2=line;
    if(state==Recording){int s=(int)(qpcNow()-startedAt);wchar_t b[32];swprintf(b,32,L"  %d:%02d",s/60,s%60);t1+=b;}
    HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
    HFONT big=CreateFontW(-(int)std::lround(18*k),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily()),small=CreateFontW(-(int)std::lround(13*k),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily());
    SIZE s1={},s2={};HGDIOBJ of=SelectObject(dc,big);GetTextExtentPoint32W(dc,t1.c_str(),(int)t1.size(),&s1);SelectObject(dc,small);GetTextExtentPoint32W(dc,t2.c_str(),(int)t2.size(),&s2);SelectObject(dc,of);
    int pad=(int)std::lround(18*k),icon=(int)std::lround(22*k),gap=(int)std::lround(12*k),w=pad*2+icon+gap+std::max(s1.cx,s2.cx),h=(int)std::lround(14*k)*2+s1.cy+(t2.empty()?0:s2.cy+(int)std::lround(2*k));
    BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* bits=nullptr;HBITMAP dib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    if(dib&&painting::token){HGDIOBJ ob=SelectObject(dc,dib);memset(bits,0,size_t(w)*h*4);
        {Gdiplus::Bitmap canvas(w,h,w*4,PixelFormat32bppPARGB,(BYTE*)bits);Gdiplus::Graphics g(&canvas);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath p;painting::path(p,{0,0,w,h},std::min(h,(int)std::lround(24*k)));Gdiplus::SolidBrush fill(Gdiplus::Color(236,10,17,36));g.FillPath(&fill,&p);
            Gdiplus::Pen edge(state==Recording?Gdiplus::Color(200,255,80,90):Gdiplus::Color(170,74,112,224),(float)std::max(1.0,1.2*k));g.DrawPath(&edge,&p);
            float cx=pad+icon/2.f,cy=h/2.f,r=icon/2.f;
            if(mark==0){double pulse=(std::sin(qpcNow()*6)+1)/2;float rr=r*(.45f+.12f*(float)pulse);Gdiplus::GraphicsPath halo;halo.AddEllipse(cx-r,cy-r,r*2,r*2);Gdiplus::PathGradientBrush hb(&halo);hb.SetCenterColor(Gdiplus::Color((BYTE)(90+80*pulse),255,70,80));Gdiplus::Color he(0,255,70,80);INT n=1;hb.SetSurroundColors(&he,&n);g.FillPath(&hb,&halo);Gdiplus::SolidBrush dot(Gdiplus::Color(255,255,76,86));g.FillEllipse(&dot,cx-rr,cy-rr,rr*2,rr*2);}
            else if(mark==1){Gdiplus::SolidBrush b(Gdiplus::Color(255,52,92,255));g.FillEllipse(&b,cx-r*.8f,cy-r*.8f,r*1.6f,r*1.6f);Gdiplus::Pen c(Gdiplus::Color(255,255,255,255),(float)(2.2*k));painting::roundCaps(c);g.DrawLine(&c,cx-r*.35f,cy,cx-r*.08f,cy+r*.28f);g.DrawLine(&c,cx-r*.08f,cy+r*.28f,cx+r*.38f,cy-r*.3f);}
            else{Gdiplus::SolidBrush b(Gdiplus::Color(255,240,170,70));g.FillEllipse(&b,cx-r*.8f,cy-r*.8f,r*1.6f,r*1.6f);Gdiplus::Pen c(Gdiplus::Color(255,20,22,30),(float)(2.4*k));painting::roundCaps(c);g.DrawLine(&c,cx,cy-r*.4f,cx,cy+r*.08f);g.DrawLine(&c,cx,cy+r*.36f,cx,cy+r*.38f);}}
        // the words: drawn white on a separate mask, then blended in their colour (GDI+ cannot use the app's own font)
        void* mbits=nullptr;HBITMAP mdib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&mbits,nullptr,0);
        if(mdib){HDC mdc=CreateCompatibleDC(screen);HGDIOBJ mob=SelectObject(mdc,mdib);SetBkMode(mdc,TRANSPARENT);int tx=pad+icon+gap,ty=(int)std::lround(14*k);unsigned char* px=(unsigned char*)bits;
            auto text=[&](HFONT ft,const std::wstring& s,int y,COLORREF c){memset(mbits,0,size_t(w)*h*4);HGDIOBJ o=SelectObject(mdc,ft);SetTextColor(mdc,RGB(255,255,255));TextOutW(mdc,tx,y,s.c_str(),(int)s.size());SelectObject(mdc,o);GdiFlush();
                const unsigned char* mk=(const unsigned char*)mbits;for(size_t i=0;i<size_t(w)*h;i++){double a=std::max(mk[i*4],std::max(mk[i*4+1],mk[i*4+2]))/255.0;if(a<=0)continue;unsigned char* p=px+i*4;
                    p[0]=(unsigned char)(p[0]*(1-a)+GetBValue(c)*a);p[1]=(unsigned char)(p[1]*(1-a)+GetGValue(c)*a);p[2]=(unsigned char)(p[2]*(1-a)+GetRValue(c)*a);p[3]=(unsigned char)std::max<double>(p[3],255*a);}};
            text(big,t1,ty,tone?tone:RGB(244,247,255));if(!t2.empty())text(small,t2,ty+s1.cy+(int)std::lround(2*k),RGB(160,176,210));SelectObject(mdc,mob);DeleteDC(mdc);DeleteObject(mdib);}
        POINT dst={org.x+(rc.right-w)/2,org.y+(int)std::lround(rc.bottom*.085)},from={0,0};SIZE size={w,h};BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
#ifdef BLUEASH_TT
        if(const char* dump=getenv("BLUEASH_RECOIL_DUMP")){static int n=0;std::string path=std::string(dump)+"-"+std::to_string(n++)+".bmp";FILE* f=fopen(path.c_str(),"wb");if(f){int row=(w*3+3)&~3;BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=-h;ih.biPlanes=1;ih.biBitCount=24;fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+row*h;fwrite(&fh,sizeof(fh),1,f);fwrite(&ih,sizeof(ih),1,f);std::vector<unsigned char> r(row,0);const unsigned char* px=(const unsigned char*)bits;for(int y=0;y<h;y++){for(int x=0;x<w;x++){const unsigned char* p=px+(size_t(y)*w+x)*4;int a=p[3];r[x*3]=(unsigned char)(p[0]+(150*(255-a))/255);r[x*3+1]=(unsigned char)(p[1]+(140*(255-a))/255);r[x*3+2]=(unsigned char)(p[2]+(120*(255-a))/255);}fwrite(r.data(),1,row,f);}fclose(f);}} /* test builds: the badge as a picture over a sandy wall colour */
#endif
        UpdateLayeredWindow(badge,screen,&dst,&size,dc,&from,0,&bf,ULW_ALPHA);SetWindowPos(badge,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);SelectObject(dc,ob);}
    if(dib){DeleteObject(dib);}DeleteObject(big);DeleteObject(small);DeleteDC(dc);ReleaseDC(nullptr,screen);}
void hide(){if(badge)ShowWindow(badge,SW_HIDE);state=Idle;}
void finish();
LRESULT CALLBACK badgeProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_NCHITTEST:return HTTRANSPARENT;
    case WM_INPUT:{if(state!=Recording&&!liveMouse)break;RAWINPUT ri;UINT size=sizeof(ri);if(GetRawInputData((HRAWINPUT)l,RID_INPUT,&ri,&size,sizeof(RAWINPUTHEADER))==(UINT)-1||ri.header.dwType!=RIM_TYPEMOUSE)break;const RAWMOUSE& mo=ri.data.mouse;
        long dx=mo.usFlags&MOUSE_MOVE_ABSOLUTE?0:mo.lLastX,dy=mo.usFlags&MOUSE_MOVE_ABSOLUTE?0:mo.lLastY;int b=mo.usButtonFlags&RI_MOUSE_LEFT_BUTTON_DOWN?1:mo.usButtonFlags&RI_MOUSE_LEFT_BUTTON_UP?-1:0;
        if(!(dx||dy||b)){break;}recoil::Mouse mm={qpcNow(),dx,dy,b};if(liveMouse)liveMouse(mm);if(state!=Recording)break;std::lock_guard<std::mutex> lk(mu);mice.push_back(mm);break;}
    case WM_TIMER:{if(state==Recording){DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);DWORD gp=0;GetWindowThreadProcessId(game,&gp);
            if(!IsWindow(game)||IsIconic(game)||pid!=gp||qpcNow()-startedAt>25){finish();return 0;} /* PUBG left the front or 25 s passed: stop and analyse */draw();}
        else if(state==Showing){if(qpcNow()-shownAt>7)hide();else{DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);DWORD gp=0;GetWindowThreadProcessId(game,&gp);if(pid!=gp)ShowWindow(badge,SW_HIDE);}}
        else{KillTimer(h,1);}return 0;}
    }return DefWindowProcW(h,m,w,l);}
void ensureBadge(){if(badge)return;dpi::Raw raw_; // physical pixels
    WNDCLASSW wc={};wc.lpfnWndProc=badgeProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshRecoilBadge";RegisterClassW(&wc);
    badge=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);}
void show(const std::wstring& t,const std::wstring& l2,int mk,COLORREF c=0){title=t;line=l2;mark=mk;tone=c;if(mk!=1)card.on=false;draw();}

// ---- start / stop -------------------------------------------------------------------------------------------------
std::wstring fmtw(const wchar_t* key,...){wchar_t s[512];va_list a;va_start(a,key);vswprintf(s,512,i18n::text(key),a);va_end(a);return s;}
void start(){if(state==Recording)return;DWORD pid=pubggame::gamePid();HWND h=pid?wgc::window(pid):nullptr;if(!h)return;game=h;ensureBadge();if(!badge)return;
    {std::lock_guard<std::mutex> l(mu);shifts.clear();mice.clear();icon=recoil::Icon();failure.clear();still=recoil::StillMap();}card.on=false;
    pre.clear();{std::vector<recoil::Mouse> pm;if(preroll&&preroll(pre,pm,preScale,preL,preR)){std::lock_guard<std::mutex> l(mu);mice=pm;}else pre.clear();}
    RAWINPUTDEVICE rid={1,2,RIDEV_INPUTSINK,badge};if(!RegisterRawInputDevices(&rid,1,sizeof(rid)))return;
    stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);startedAt=qpcNow();state=Recording;
    try{worker=std::thread(capture,h);}catch(...){state=Idle;return;}
    show(i18n::text(L"반동 패턴 캡처 중"),fmtw(L"같은 총·부착물로 5탄창을 쏘세요 · 탄창마다 따로 계산 · 다시 %ls: 끝내기",keyName(key()).c_str()),0);SetTimer(badge,1,80,nullptr);}
// The traces kept for the graph: at most 240 points each.
std::string pts(const std::vector<recoil::Pt>& v){std::string s;size_t step=std::max<size_t>(1,v.size()/240);char b[48];for(size_t i=0;i<v.size();i+=step){snprintf(b,48,"%.1f,%.1f;",v[i].x,v[i].y);s+=b;}return s;}
// set by the Aim Lab page: what the badge says after a saved spray (title, second line)
void(*describe)(long long at,const std::string& weapon,double vm,double ratio,std::wstring& title,std::wstring& line)=nullptr;
// the gun icons shipped with BlueAsh (source/pubg_weapons.tsv, resource 470): named on the developer PC, so a player's
// guns are told without naming them first. shortcut: the icon is read at fixed screen fractions, so a very different
// aspect ratio (21:9) may miss; naming the gun once on the results adds the player's own icon.
std::string shippedWeapons(){HMODULE m=GetModuleHandleW(nullptr);HRSRC r=FindResourceW(m,MAKEINTRESOURCEW(470),RT_RCDATA);if(!r)return "";const char* p=(const char*)LockResource(LoadResource(m,r));DWORD n=SizeofResource(m,r);return p&&n?std::string(p,n):std::string();}
// Pixels per count from the last capture with the same PUBG sensitivity settings (and the same scope when it is known):
// a capture with too little mouse movement of its own uses it, so the aim need not be moved on purpose every time.
// shortcut: the mouse DPI is not in PUBG's file; a DPI change without new settings keeps the old value until a capture moves enough.
struct KCache {double h=0,v=0;int lag=1;};
std::string sensPrint(const aim::Sens& s){char b[96];snprintf(b,96,"%.3f|%.3f|%.3f|%.3f",s.normal,s.targeting,s.scoping,s.vertical);std::string p=b;for(auto& sc:s.scopes){snprintf(b,96,"|%s=%.3f",sc.first.c_str(),sc.second);p+=b;}return p;}
KCache kLookup(const std::string& fp,const std::string& scope){KCache best,any;std::istringstream in(readFile(folder()+L"\\kcache.tsv"));std::string l;
    while(std::getline(in,l)){std::stringstream ss(l);std::string f,sc,a,b,c;if(!std::getline(ss,f,'\t')||!std::getline(ss,sc,'\t')||!std::getline(ss,a,'\t')||!std::getline(ss,b,'\t')||!std::getline(ss,c,'\t')||f!=fp)continue;
        KCache k{atof(a.c_str()),atof(b.c_str()),atoi(c.c_str())};if(k.h<=0||k.v<=0)continue;any=k;if(sc==scope)best=k;}
    if(best.h<=0&&any.h<=0&&readFile(folder()+L"\\kcache.tsv").empty()){ /* before this cache existed: the newest capture's own measurement (recoil.tsv: vm column 4, kV column 10) */
        std::istringstream rin(readFile(folder()+L"\\recoil.tsv"));while(std::getline(rin,l)){std::vector<std::string> c;std::stringstream ss(l);std::string x;while(std::getline(ss,x,'\t')){c.push_back(x);}if(c.size()>=11){double vm=atof(c[4].c_str()),kv=atof(c[10].c_str());if(vm>0&&kv>0){any={kv/vm,kv,1};}}}}
    return best.h>0?best:any;}
void kSave(const std::string& fp,const std::string& scope,double h,double v,int lag){if(h<=0||v<=0)return;std::istringstream in(readFile(folder()+L"\\kcache.tsv"));std::string l,body;std::vector<std::string> keep;
    while(std::getline(in,l)){if(l.empty()||l.rfind(fp+"\t"+scope+"\t",0)==0)continue;keep.push_back(l);}while(keep.size()>40)keep.erase(keep.begin());char b[96];snprintf(b,96,"\t%.5f\t%.5f\t%d",h,v,lag);keep.push_back(fp+"\t"+scope+b);
    for(auto& k:keep){body+=k+"\n";}std::wstring path=folder()+L"\\kcache.tsv",tmp=path+L".tmp";if(FILE* f=_wfopen(tmp.c_str(),L"wb")){bool ok=fwrite(body.data(),1,body.size(),f)==body.size();ok=fclose(f)==0&&ok;if(ok)MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);}}
// The gun by its icon among the ones named before (the player's own weapons.tsv and the shipped ones).
std::string weaponOf(const recoil::Icon& ic){std::string weapon;double best=0;std::istringstream in(readFile(folder()+L"\\weapons.tsv")+"\n"+shippedWeapons());std::string l;
    while(std::getline(in,l)){auto tab=l.find('\t');if(tab==std::string::npos)continue;double q=recoil::same(ic,recoil::unhex(l.substr(tab+1)));if(q>best){best=q;weapon=l.substr(0,tab);}}
    return best<.72?std::string():weapon;} /* best match wins: a few distinct guns overlap up to ~0.8 (AKM/S12K, Kar98k/S686); icons from screenshots differ a little from the live capture */
// The scope by what stayed still at the screen centre: scopes.tsv holds the pictures of scopes the player picked on a result.
// shortcut: learned per player (no shipped reticles yet); a scope never picked is not told.
std::string scopeOf(const recoil::Mask& sig){if(!sig.ok())return "";std::string code;double best=0;std::istringstream in(readFile(folder()+L"\\scopes.tsv"));std::string l;
    while(std::getline(in,l)){auto tab=l.find('\t');if(tab==std::string::npos)continue;double q=recoil::same(sig,recoil::unhexMask(l.substr(tab+1)));if(q>best){best=q;code=l.substr(0,tab);}}
    return best<.55?std::string():code;}
// The player picked a scope for a capture: its still picture joins scopes.tsv (at most six per scope, the newest kept).
void learnScope(long long at,const std::string& code){if(code.empty())return;std::istringstream tr(readFile(folder()+L"\\recoil\\"+std::to_wstring(at)+L".txt"));std::string l,sig;while(std::getline(tr,l)){if(l.rfind("scope=",0)==0)sig=l.substr(6);}
    if(!recoil::unhexMask(sig).ok()){return;}std::vector<std::string> keep;int mine=0;{std::istringstream in(readFile(folder()+L"\\scopes.tsv"));std::vector<std::string> all;while(std::getline(in,l)){if(!l.empty())all.push_back(l);}
        for(int i=(int)all.size()-1;i>=0;i--){bool same=all[i].rfind(code+"\t",0)==0;if(same&&++mine>5)continue;keep.insert(keep.begin(),all[i]);}}
    keep.push_back(code+"\t"+sig);std::string body;for(auto& k:keep)body+=k+"\n";std::wstring path=folder()+L"\\scopes.tsv",tmp=path+L".tmp";if(FILE* f=_wfopen(tmp.c_str(),L"wb")){bool ok=fwrite(body.data(),1,body.size(),f)==body.size();ok=fclose(f)==0&&ok;if(ok)MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);}}
void finish(){if(state!=Recording)return;if(stopEvent)SetEvent(stopEvent);if(worker.joinable())worker.join();if(stopEvent){CloseHandle(stopEvent);stopEvent=nullptr;}
    if(!liveInput){RAWINPUTDEVICE rid={1,2,RIDEV_REMOVE,nullptr};RegisterRawInputDevices(&rid,1,sizeof(rid));} /* the live recorder keeps reading the mouse */
    std::vector<recoil::Shift> s;std::vector<recoil::Mouse> m;recoil::Icon ic;recoil::Mask sig;std::wstring fail;{std::lock_guard<std::mutex> l(mu);s=shifts;m=mice;ic=icon;fail=failure;sig=still.sig();}
    std::string weapon=weaponOf(ic),scope=scopeOf(sig);aim::Sens sens=pubgSens();double vm=sens.found?sens.vertical:1;KCache fb=kLookup(sensPrint(sens),scope);recoil::Result r=recoil::analyse(s,m,vm,recoil::fireInterval(weapon),fb.h,fb.v,fb.lag);if(r.ok&&!r.reused)kSave(sensPrint(sens),scope,r.kH,r.kV,r.lag);
    state=Showing;shownAt=qpcNow();
    // what this clip held, for the next look when an analysis fails (counts only, no picture)
    long moves=0,clicks=0;double counts=0,pix=0;for(auto& q:m){if(q.dx||q.dy){moves++;counts+=std::fabs((double)q.dx)+std::fabs((double)q.dy);}if(q.button>0)clicks++;}for(auto& q:s)pix+=std::fabs(q.dx)+std::fabs(q.dy);
    bool gameAdmin=false;{DWORD gp=0;GetWindowThreadProcessId(game,&gp);if(HANDLE ph=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,gp)){gameAdmin=processElevated(ph);CloseHandle(ph);}else gameAdmin=GetLastError()==ERROR_ACCESS_DENIED;}
    if(FILE* f=_wfopen((folder()+L"\\recoil-last.txt").c_str(),L"wb")){fprintf(f,"why=%s\nframes=%zu\nshiftPixels=%.0f\nmouseMoves=%ld\nmouseCounts=%.0f\nclicks=%ld\nfitH=%.3f\nfitV=%.3f\nmovedH=%.0f\nmovedV=%.0f\nkH=%.4f\nkV=%.4f\ngameAdmin=%d\nselfAdmin=%d\n",r.ok?"ok":r.why.c_str(),s.size(),pix,moves,counts,clicks,r.fitH,r.fitV,r.movedH,r.movedV,r.kH,r.kV,gameAdmin?1:0,selfElevated()?1:0);fclose(f);}
    // no mouse at all: Windows hides an elevated game's input from a normal BlueAsh
    if(!moves&&!clicks&&s.size()>=12){if(gameAdmin&&!selfElevated()){show(i18n::text(L"마우스 움직임을 읽지 못했습니다"),i18n::text(L"배그가 관리자 권한으로 실행 중입니다 · BlueAsh 설정 > 문제 해결에서 관리자 권한으로 다시 실행해 주세요"),2,RGB(240,180,90));}
        else{show(i18n::text(L"마우스 움직임을 읽지 못했습니다"),i18n::text(L"BlueAsh를 다시 켠 뒤 한 번 더 해 주세요"),2,RGB(240,180,90));}return;}
    if(!fail.empty()){show(i18n::text(L"화면을 캡처하지 못했습니다"),i18n::text(L"배그를 테두리 없는 창 모드로 켜 주세요"),2,RGB(240,180,90));return;}
    if(!r.ok){const char* w=r.why.c_str();show(i18n::text(L"분석하지 못했습니다"),i18n::text(!strcmp(w,"still")?L"처음 한 번만 쏘기 전에 조준을 좌우로 움직여 주세요 · 다음부터는 이 값을 씁니다":!strcmp(w,"nofire")?L"사격이 없었습니다 · 왼쪽 버튼으로 쏘세요":!strcmp(w,"nopull")?L"아래로 당기지 않았습니다 · 반동을 당기며 쏘세요":!strcmp(w,"norecoil")?L"반동이 보이지 않았습니다 · 벽을 보고 쏘세요":L"너무 짧습니다 · 한 탄창을 쏘세요"),2,RGB(240,180,90));return;}
    long long at=(long long)time(nullptr);char row[512];snprintf(row,512,"%lld\t%s\t%s\t%s\t%.3f\t%.4f\t%.1f\t%.1f\t%.1f\t%.2f\t%.4f\t%d\n",at,weapon.c_str(),ic.ok()?recoil::hex(ic).c_str():"",scope.c_str(),vm,r.ratio,r.recoil,r.pull,r.drift,r.fireSec,r.kV,r.burstRatios.empty()?r.bursts:(int)r.burstRatios.size());
    if(FILE* f=_wfopen((folder()+L"\\recoil.tsv").c_str(),L"ab")){fputs(row,f);fclose(f);}
    std::wstring tdir=folder()+L"\\recoil";CreateDirectoryW(tdir.c_str(),nullptr);if(FILE* f=_wfopen((tdir+L"\\"+std::to_wstring(at)+L".txt").c_str(),L"wb")){std::string body="aim="+pts(r.aim)+"\nhand="+pts(r.hand)+"\ngun="+pts(r.gun)+"\nlaser="+pts(r.laser)+"\n";
        // each magazine in detail (자세한 분석): its numbers, then its crosshair path (at most ~60 points)
        {char hb[48];snprintf(hb,48,"hcomp=%.3f\n",r.hComp);body+=hb;}if(sig.ok())body+="scope="+recoil::hex(sig)+"\n";
        for(auto& b:r.each){char bl[200];snprintf(bl,200,"b=%.4f,%.2f,%.2f,%.1f,%.3f,%.1f,%.1f,%.1f,%d,%d\n",b.ratio,b.vErr,b.hErr,b.wrong,b.react,b.phase[0],b.phase[1],b.phase[2],b.shots,b.breakAt);body+=bl;std::vector<recoil::Pt> few;size_t st=std::max<size_t>(1,b.aim.size()/60);for(size_t i=0;i<b.aim.size();i+=st){few.push_back(b.aim[i]);}if(!b.aim.empty()&&(b.aim.size()-1)%st){few.push_back(b.aim.back());}body+="ba="+pts(few)+"\n";if(!b.shot.empty())body+="bs="+pts(b.shot)+"\n";}
        fputs(body.c_str(),f);fclose(f);}
    // the badge's words come from the Aim Lab page (it knows the earlier records: the steady value, the gun's scope)
    std::wstring t1,t2;if(describe)describe(at,weapon,vm,r.ratio,t1,t2);else{wchar_t a[16],b2[16];swprintf(a,16,L"%.2f",vm);swprintf(b2,16,L"%.2f",vm*r.ratio);t1=fmtw(L"수직 감도 %ls → %ls 추천",a,b2);}
    if(r.reused&&card.on)card.hint=i18n::text(L"지난 측정의 마우스 값으로 계산했습니다 · 감도를 바꿨다면 좌우로 한 번 움직여 주세요");
    show(t1,t2,1);
    if(onSaved)onSaved();}
void toggle(){if(state==Recording)finish();else start();}
// The key works only while PUBG is the front window (registered then, released otherwise, so Home / End / Insert stay
// normal everywhere else). When it is the mode switch key itself, the mode hotkey hands it over (takes()).
bool keyHeld=false;
void poll(HWND owner){DWORD pid=pubggame::gamePid();bool front=pid&&pubggame::foreground(pid);unsigned k=key();bool shared=k==hotkeyVk&&!hotkeyMods;
    if(front&&!shared&&!keyHeld){keyHeld=RegisterHotKey(owner,4,0,k)!=0;}else if((!front||shared)&&keyHeld){UnregisterHotKey(owner,4);keyHeld=false;}
    if(!front&&state==Recording)finish();}
void rekey(HWND owner){if(keyHeld){UnregisterHotKey(owner,4);keyHeld=false;}poll(owner);}
bool takes(){unsigned k=key();if(k!=hotkeyVk||hotkeyMods)return false;DWORD pid=pubggame::gamePid();if(!pid||!pubggame::foreground(pid))return false;toggle();return true;}
}
#ifdef BLUEASH_TT
// Test builds (BLUEASH_RECOIL_DUMP=<path prefix>): the badge's three looks drawn over the main window and saved as pictures.
namespace recoilcap{inline void demo(HWND w){if(!getenv("BLUEASH_RECOIL_DUMP"))return;game=w;ensureBadge();startedAt=qpcNow()-3;state=Recording;show(i18n::text(L"반동 패턴 캡처 중"),fmtw(L"좌우로 살짝 움직인 뒤 같은 총·부착물로 5탄창을 쏘세요 · 탄창마다 따로 계산 · 다시 %ls: 끝내기",keyName(key()).c_str()),0);
    state=Showing;card.on=true;card.gun=L"M416";card.count=fmtw(L"%d/5탄창 측정",3);card.vFrom=L"0.90";card.vTo=L"1.06";card.vDelta=L"+18%";card.sLabel=fmtw(L"%ls 감도",i18n::text(L"4배"));card.sFrom=L"25";card.sTo=L"29";card.sDelta=L"+4";card.hint=i18n::text(L"둘 중 하나만 바꾸세요 · 스코프 감도는 가로도 함께 바뀝니다");show(L"",L"",1);card.on=false;
    state=Showing;show(L"Beryl M762  ·  "+fmtw(L"수직 감도 %ls → %ls 추천",L"0.90",L"1.18"),i18n::text(L"에임랩에서 스코프를 고르면 스코프 감도도 보여 줍니다"),1);
    show(i18n::text(L"분석하지 못했습니다"),i18n::text(L"처음 한 번만 쏘기 전에 조준을 좌우로 움직여 주세요 · 다음부터는 이 값을 씁니다"),2,RGB(240,180,90));hide();}}
#endif
