#pragma once
// Tarkov · the server a PvP / season raid was matched on (the NetworkGameCreate line's Sid, e.g. "JP-TK"; PvE raids have
// none). A small badge at the top right of the Tarkov window from the match until 30 s after the raid starts, only while
// Tarkov is in front. Click-through, never takes focus. Registry TarkovServerBadge ("0" = off, default on).
namespace serverinfo {
struct Name {const char* code;const wchar_t* name;};
// "JP-TK" -> L"일본 · Tokyo", "SN-SIN" in English -> L"Singapore". Country names go through i18n, city names stay in English; an unknown code is shown as it is.
inline std::wstring place(const std::string& sid){
    static const Name countries[]={{"JP",L"일본"},{"KR",L"한국"},{"CN",L"중국"},{"SN",L"싱가포르"},{"SG",L"싱가포르"},{"US",L"미국"},{"CA",L"캐나다"},
        {"RU",L"러시아"},{"DE",L"독일"},{"GB",L"영국"},{"UK",L"영국"},{"FR",L"프랑스"},{"NL",L"네덜란드"},{"PL",L"폴란드"},{"FI",L"핀란드"},{"SE",L"스웨덴"},
        {"IT",L"이탈리아"},{"ES",L"스페인"},{"TR",L"튀르키예"},{"AU",L"호주"},{"BR",L"브라질"},{"AE",L"아랍에미리트"},{"KZ",L"카자흐스탄"}};
    static const Name cities[]={{"TK",L"Tokyo"},{"HK",L"Hong Kong"},{"SIN",L"Singapore"},{"SL",L"Seoul"},{"SEA",L"Seattle"},{"LA",L"Los Angeles"},
        {"HBR",L"Khabarovsk"},{"VAN",L"Vancouver"},{"STL",L"St. Louis"},{"SJC",L"San Jose"},{"PHX",L"Phoenix"},{"DEN",L"Denver"},{"NY",L"New York"},
        {"CHI",L"Chicago"},{"DAL",L"Dallas"},{"MIA",L"Miami"},{"ATL",L"Atlanta"},{"MSK",L"Moscow"},{"MOW",L"Moscow"},{"SPB",L"St. Petersburg"},
        {"NSK",L"Novosibirsk"},{"YEK",L"Yekaterinburg"},{"FRA",L"Frankfurt"},{"LON",L"London"},{"AMS",L"Amsterdam"},{"PAR",L"Paris"},{"WAW",L"Warsaw"},
        {"HEL",L"Helsinki"},{"IST",L"Istanbul"},{"SYD",L"Sydney"},{"SAO",L"São Paulo"},{"STO",L"Stockholm"},{"MIL",L"Milan"},{"MAD",L"Madrid"},
        {"DXB",L"Dubai"},{"TOR",L"Toronto"},{"MTL",L"Montreal"}};
    auto dash=sid.find('-');if(dash==std::string::npos){return std::wstring(sid.begin(),sid.end());}
    std::string c=sid.substr(0,dash),t=sid.substr(dash+1);std::wstring country,city;
    for(auto& n:countries){if(c==n.code){country=i18n::text(n.name);}}for(auto& n:cities){if(t==n.code){city=n.name;}}
    if(t=="HK"){country=i18n::text(L"홍콩");} // CN-HK: Hong Kong, not mainland China
    if(country.empty()){country.assign(c.begin(),c.end());}if(city.empty()){city.assign(t.begin(),t.end());}
    return country==city?country:country+L" · "+city;} // Hong Kong, Singapore: once
}
#ifndef SERVERINFO_NO_UI
bool serverBadgeOn=true;
struct ServerBadge {
    HWND window=nullptr;std::string shown;unsigned serial=0;ULONGLONG seenAt=0,startedAt=0;
    struct Aware {using Set=HANDLE(WINAPI*)(HANDLE);Set set=nullptr;HANDLE old=nullptr;
        dpi::Raw raw_; // physical pixels here: not scaled by dpi.h
        Aware(){FARPROC p=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetThreadDpiAwarenessContext");memcpy(&set,&p,sizeof(set));if(set)old=set((HANDLE)-4);}
        ~Aware(){if(set&&old)set(old);}};
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_NCHITTEST){return HTTRANSPARENT;}return DefWindowProcW(h,m,w,l);}
    void load(){serverBadgeOn=regString(configPath,L"TarkovServerBadge")!=L"0";}
    void hide(){if(window&&IsWindowVisible(window)){ShowWindow(window,SW_HIDE);}shown.clear();}
    // Draws the pill (navy, hairline edge, cobalt dot, a small caption and the place) for a Tarkov window of height ch.
    void draw(HWND game,const std::wstring& where){
        Aware a;RECT rc={};if(!GetClientRect(game,&rc)||rc.bottom<200){return;}POINT org={0,0};ClientToScreen(game,&org);double k=std::max(0.75,rc.bottom/1080.0);
        if(!window){WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshServerBadge";RegisterClassW(&wc);
            window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);if(!window){return;}}
        HDC screen=GetDC(nullptr);HDC dc=CreateCompatibleDC(screen);
        HFONT cap=CreateFontW(-(int)std::lround(12*k),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily());
        HFONT big=CreateFontW(-(int)std::lround(17*k),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily());
        std::wstring caption=i18n::text(L"매칭 서버");SIZE s1={},s2={};HGDIOBJ of=SelectObject(dc,cap);GetTextExtentPoint32W(dc,caption.c_str(),(int)caption.size(),&s1);SelectObject(dc,big);GetTextExtentPoint32W(dc,where.c_str(),(int)where.size(),&s2);SelectObject(dc,of);
        int pad=(int)std::lround(16*k),dot=(int)std::lround(8*k),w=pad*2+dot+(int)std::lround(10*k)+std::max(s1.cx,s2.cx),h=(int)std::lround(12*k)*2+s1.cy+s2.cy;
        BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* bits=nullptr;
        HBITMAP dib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(dib){HGDIOBJ ob=SelectObject(dc,dib);unsigned char* px=(unsigned char*)bits;memset(px,0,size_t(w)*h*4);
            // The pill: coverage from 4x4 samples of a rounded rectangle (radius h/2), premultiplied navy at 92 %, edge line lighter.
            double r=h/2.0;auto inside=[&](double x,double y,double inset){double cx=std::clamp(x,r,w-r),cy=std::clamp(y,r,h-r);return std::hypot(x-cx,y-cy)<=r-inset;};
            for(int y=0;y<h;y++)for(int x=0;x<w;x++){int in=0,core=0;for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++){double fx=x+(sx+0.5)/4,fy=y+(sy+0.5)/4;in+=inside(fx,fy,0);core+=inside(fx,fy,1.2*k);}
                double cov=in/16.0,edge=(in-core)/16.0,al=cov*0.92;unsigned char* p=px+(size_t(y)*w+x)*4;
                double R=13*(al-edge)+70*edge,G=22*(al-edge)+100*edge,B=44*(al-edge)+170*edge;p[0]=(unsigned char)B;p[1]=(unsigned char)G;p[2]=(unsigned char)R;p[3]=(unsigned char)std::lround(255*al);}
            // Cobalt dot, then the two lines: drawn white on a separate mask so ClearType-free coverage tints them.
            int dx=pad+dot/2,dy=h/2;for(int y=dy-dot;y<=dy+dot;y++)for(int x=dx-dot;x<=dx+dot;x++){if(x<0||y<0||x>=w||y>=h){continue;}int c=0;for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++){c+=std::hypot(x+(sx+0.5)/4-dx-0.5,y+(sy+0.5)/4-dy-0.5)<=dot/2.0;}
                double a=c/16.0;unsigned char* p=px+(size_t(y)*w+x)*4;p[0]=(unsigned char)(p[0]*(1-a)+255*a);p[1]=(unsigned char)(p[1]*(1-a)+92*a);p[2]=(unsigned char)(p[2]*(1-a)+52*a);p[3]=(unsigned char)std::max<double>(p[3],255*a);}
            std::vector<unsigned char> mask(size_t(w)*h*4,0);void* mbits=nullptr;HBITMAP mdib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&mbits,nullptr,0);
            if(mdib){HDC mdc=CreateCompatibleDC(screen);HGDIOBJ mob=SelectObject(mdc,mdib);memset(mbits,0,size_t(w)*h*4);SetBkMode(mdc,TRANSPARENT);
                int tx=pad+dot+(int)std::lround(10*k),ty=(int)std::lround(12*k);
                auto line=[&](HFONT f,const std::wstring& t,int y,COLORREF c){memset(mbits,0,size_t(w)*h*4);HGDIOBJ o=SelectObject(mdc,f);SetTextColor(mdc,RGB(255,255,255));TextOutW(mdc,tx,y,t.c_str(),(int)t.size());SelectObject(mdc,o);GdiFlush();
                    const unsigned char* m=(const unsigned char*)mbits;for(size_t i=0;i<size_t(w)*h;i++){double a=std::max(m[i*4],std::max(m[i*4+1],m[i*4+2]))/255.0;if(a<=0){continue;}unsigned char* p=px+i*4;
                        p[0]=(unsigned char)(p[0]*(1-a)+GetBValue(c)*a);p[1]=(unsigned char)(p[1]*(1-a)+GetGValue(c)*a);p[2]=(unsigned char)(p[2]*(1-a)+GetRValue(c)*a);p[3]=(unsigned char)std::max<double>(p[3],255*a);}};
                line(cap,caption,ty,RGB(150,168,205));line(big,where,ty+s1.cy,RGB(240,244,255));SelectObject(mdc,mob);DeleteDC(mdc);DeleteObject(mdib);}
            POINT dst={org.x+rc.right-w-(int)std::lround(24*k),org.y+(int)std::lround(20*k)},src={0,0};SIZE size={w,h};BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
            UpdateLayeredWindow(window,screen,&dst,&size,dc,&src,0,&bf,ULW_ALPHA);SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
            SelectObject(dc,ob);DeleteObject(dib);}
        DeleteObject(cap);DeleteObject(big);DeleteDC(dc);ReleaseDC(nullptr,screen);}
    // Every second: a new match's server shows until 30 s after GameStarted (at most 10 min), while Tarkov is the front window.
    void poll(const raid::Context& c){
        ULONGLONG now=GetTickCount64();
        if(c.server.empty()||!serverBadgeOn){hide();serial=0;return;}
        if(c.serial!=serial){serial=c.serial;seenAt=now;startedAt=0;shown.clear();}
        if(c.phase=="started"&&!startedAt){startedAt=now;}
        bool due=now-seenAt<600000&&(!startedAt||now-startedAt<30000)&&c.phase!="ended";
        HWND front=GetForegroundWindow();DWORD pid=0;GetWindowThreadProcessId(front,&pid);
        if(!due||!tarkovPid||pid!=tarkovPid||IsIconic(front)){hide();return;}
        if(shown!=c.server){draw(front,serverinfo::place(c.server));shown=c.server;}}
};
ServerBadge serverBadge;
#endif
