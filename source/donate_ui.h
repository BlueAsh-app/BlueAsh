#pragma once
// 커피 한 잔 후원 (sidebar coffee icon, page 12): pick an amount, scan the QR with the phone camera and the payment page
// (now PayPal for every language; a won side for the Korean UI if one is set up later) opens with that amount. The relay keeps the links (relay/donate.js, wrangler vars
// DONATE_KR / DONATE_EN) and draws the QR; BlueAsh never sees any payment. While the side for this language is not set
// up on the relay the icon stays hidden. Giving is optional and changes nothing in the app.
// Which side: won (kr) only in the Korean UI and only while the relay has it (registry DonateKr), else dollars (en).
const wchar_t* donateKind(){return i18n::languageCode()==L"ko"&&regString(configPath,L"DonateKr")==L"1"?L"kr":L"en";}
const int donateAmounts[2][4]={{5000,10000,20000,30000},{5,10,15,20}};
std::wstring donateLabel(int i){bool kr=donateKind()[0]==L'k';int a=donateAmounts[kr?0:1][i];
    // Dollars in the Korean UI also show about how much in won (1 USD ~ 1,400 won).
    if(!kr){std::wstring d=L"$"+std::to_wstring(a);if(i18n::languageCode()!=L"ko"){return d;}std::wstring w=std::to_wstring(a*1400);for(int at=(int)w.size()-3;at>0;at-=3)w.insert(at,L",");return d+L" (≈"+w+i18n::text(L"원")+L")";}std::wstring n=std::to_wstring(a);for(int at=(int)n.size()-3;at>0;at-=3)n.insert(at,L",");return n+i18n::text(L"원");}
// GET from the relay into body (worker thread); the HTTP status, 0 when unreachable.
DWORD relayGet(const std::wstring& path,std::string& body){
    HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;DWORD status=0,size=sizeof(status);
    if(s){WinHttpSetTimeouts(s,5000,5000,10000,10000);c=WinHttpConnect(s,updateHost,INTERNET_DEFAULT_HTTPS_PORT,0);}
    if(c)r=WinHttpOpenRequest(c,L"GET",path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE|WINHTTP_FLAG_REFRESH);
    if(r&&WinHttpSendRequest(r,nullptr,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)&&status==200){
        char buf[8192];DWORD n=0;while(WinHttpReadData(r,buf,sizeof(buf),&n)&&n&&body.size()<1000000)body.append(buf,n);}
    for(HINTERNET h:{r,c,s}){if(h)WinHttpCloseHandle(h);}return status;
}
struct DonateWatch {
    bool shown=false;ULONGLONG next=0;
    // At start the last answer (registry DonateShown) shows the icon at once; the relay is asked 5 s later and every 6 hours.
    void poll(){ULONGLONG now=GetTickCount64();if(!next){next=now+5000;answer(regString(configPath,L"DonateShown")==L"1");return;}if(now<next){return;}next=now+6*3600*1000ULL;
        HANDLE t=CreateThread(nullptr,0,[](LPVOID)->DWORD{std::string b;DWORD s=relayGet(L"/donate/config",b);
            if(s==200){auto has=[&](const char* k){return b.find(k)!=std::string::npos;};PostMessageW(win,WM_APP+183,(has("kr=1")?1:0)|(has("en=1")?2:0)|(has("krSingle=1")?4:0)|(has("enSingle=1")?8:0),0);}return 0;},nullptr,0,nullptr);if(t)CloseHandle(t);}
    // The icon is always there now; the answer only says what the page shows (amount buttons or one page, or "not set").
    void answer(bool on){shown=on;}
    // sides: bit 0 won, bit 1 dollars, bits 2/3 that side is one page for any amount (Ko-fi). The Korean UI shows the icon
    // for either side, the others only for dollars.
    void remember(WPARAM sides){bool kr=sides&1,on=(sides&2)||(kr&&i18n::languageCode()==L"ko");saveString(configPath,L"DonateKr",kr?L"1":L"0");
        saveString(configPath,L"DonateSingle",std::wstring(1,L'0'+int((sides>>2)&3)));saveString(configPath,L"DonateShown",on?L"1":L"0");answer(on);}
} donateWatch;
int donatePick=1;unsigned donateSeq=0;Gdiplus::Bitmap* donateQr=nullptr;
// The last QR fetched (its request path and GIF bytes): reopening the page shows it at once, without asking the relay again.
std::wstring donateCachedPath;std::string donateCachedGif;
Gdiplus::Bitmap* donateDecode(const std::string& gif){
    Gdiplus::Bitmap* copy=nullptr;if(gif.empty()){return nullptr;}IStream* s=nullptr;HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,gif.size());
    if(mem){if(void* p=GlobalLock(mem)){memcpy(p,gif.data(),gif.size());GlobalUnlock(mem);}if(FAILED(CreateStreamOnHGlobal(mem,TRUE,&s))){GlobalFree(mem);s=nullptr;}}
    // Drawn into our own bitmap: one made straight from the stream keeps reading it.
    if(s){{Gdiplus::Bitmap src(s);if(src.GetLastStatus()==Gdiplus::Ok&&src.GetWidth()>8){copy=new Gdiplus::Bitmap((INT)src.GetWidth(),(INT)src.GetHeight(),PixelFormat24bppRGB);Gdiplus::Graphics c(copy);c.DrawImage(&src,0,0,(INT)src.GetWidth(),(INT)src.GetHeight());}}s->Release();}
    return copy;
}
// One page for any amount (Ko-fi): one QR, no amount buttons (registry DonateSingle: bit 0 won side, bit 1 dollar side).
bool donateSingle(){auto v=regString(configPath,L"DonateSingle");if(v.empty()){return true;} // before the first answer: the relay's one Ko-fi page
    int b=_wtoi(v.c_str());return (b>>(donateKind()[0]==L'k'?0:1))&1;}
RECT donatePlate={36,134,316,414};
void donateStatus(HWND h,const wchar_t* text){SetWindowTextW(GetDlgItem(h,254),text);InvalidateRect(GetDlgItem(h,254),nullptr,TRUE);}
void donateLoad(HWND h){
    for(int i=0;i<4;i++){HWND b=GetDlgItem(h,250+i);SetWindowLongPtrW(b,GWLP_USERDATA,i==donatePick?1:0);InvalidateRect(b,nullptr,TRUE);}
    std::wstring path=std::wstring(L"/donate/qr.gif?kind=")+donateKind()+L"&amount="+std::to_wstring(donateAmounts[donateKind()[0]==L'k'?0:1][donatePick]);
    delete donateQr;donateQr=nullptr;++donateSeq;
    if(path==donateCachedPath&&(donateQr=donateDecode(donateCachedGif))){InvalidateRect(h,&donatePlate,TRUE);donateStatus(h,L"");return;}
    InvalidateRect(h,&donatePlate,TRUE);donateStatus(h,i18n::text(L"QR을 불러오는 중…"));
    struct Job{HWND page;unsigned seq;std::wstring path;std::string body;DWORD status=0;};
    auto* j=new Job{h,donateSeq,path,{},0};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(Job*)p;j->status=relayGet(j->path,j->body);if(!PostMessageW(j->page,WM_APP+181,0,(LPARAM)j))delete j;return 0;},j,0,nullptr);
    if(t)CloseHandle(t);else{delete j;donateStatus(h,i18n::text(L"QR을 불러오지 못했습니다. 잠시 뒤 다시 시도해 주세요."));}
}
LRESULT CALLBACK donateProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        bool one=donateSingle();int y=one?-58:0;donatePlate={36,134+y,316,414+y};
        if(one){pageLabel(h,i18n::text(L"휴대폰 카메라로 QR을 찍으세요."),36,34,780,26,2);}
        else{pageLabel(h,i18n::text(L"금액을 고르고 휴대폰 카메라로 QR을 찍으세요."),36,34,780,26,2);for(int i=0;i<4;i++)pageButton(h,donateLabel(i).c_str(),36+i*164,76,150,250+i);}
        bool kr=donateKind()[0]==L'k';
        if(one)pageLabel(h,i18n::text(L"찍으면 Ko-fi 후원 페이지가 열립니다. 금액은 원하는 만큼 정하고 응원 메시지도 남길 수 있습니다. 카드나 페이팔로 결제합니다."),344,140+y,470,80,0);
        else pageLabel(h,kr?i18n::text(L"찍으면 토스가 열리고 금액이 들어가 있습니다. 토스 화면에서 금액을 바꿀 수도 있습니다."):i18n::text(L"찍으면 페이팔 결제 화면이 그 금액으로 열립니다. 페이팔 계정이 없어도 카드나 Apple Pay로 결제할 수 있습니다."),344,140,470,80,0);
        pageLabel(h,i18n::text(L"후원은 선택입니다. 후원하지 않아도 모든 기능을 그대로 쓸 수 있습니다. 보내 주신 마음은 서버 비용과 새 기능에 씁니다."),344,232+y,470,80,1);
        pageLabel(h,L"",344,330+y,470,40,1,254);
        pageButton(h,i18n::text(L"이 PC 브라우저로 열기"),36,556,220,255);
        pageButton(h,i18n::text(L"닫기"),696,556,120,256);
        donateLoad(h);return 0;}
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);
        // A white plate around the QR (phone cameras read dark-on-light), the code drawn with hard pixels. The plate is
        // finished off-screen over the page background and copied in one go.
        RECT pr=donatePlate;int pw=pr.right-pr.left,ph=pr.bottom-pr.top;HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,pw,ph);auto old=SelectObject(mem,bmp);
        BitBlt(mem,0,0,pw,ph,dc,pr.left,pr.top,SRCCOPY);
        if(painting::token){Gdiplus::Graphics g(mem);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::GraphicsPath p;painting::path(p,{0,0,pw,ph},22);Gdiplus::SolidBrush white(Gdiplus::Color(255,255,255,255));g.FillPath(&white,&p);
            if(donateQr){g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);int side=pw-28;g.DrawImage(donateQr,14,14,side,side);}}
        BitBlt(dc,pr.left,pr.top,pw,ph,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);
        EndPaint(h,&ps);return 0;}
    case WM_APP+181:{struct Job{HWND page;unsigned seq;std::wstring path;std::string body;DWORD status=0;};std::unique_ptr<Job> j((Job*)lp);if(j->seq!=donateSeq){return 0;}
        Gdiplus::Bitmap* copy=j->status==200?donateDecode(j->body):nullptr;if(copy){donateCachedPath=j->path;donateCachedGif=j->body;}
        delete donateQr;donateQr=copy;InvalidateRect(h,&donatePlate,TRUE);
        donateStatus(h,copy?L"":j->status==404?i18n::text(L"송금 링크가 아직 등록되지 않았습니다. (미리보기)"):i18n::text(L"QR을 불러오지 못했습니다. 잠시 뒤 다시 시도해 주세요."));return 0;}
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){int id=LOWORD(wp);
        if(id>=250&&id<=253){donatePick=id-250;donateLoad(h);}
        else if(id==255){std::wstring u=std::wstring(L"https://")+updateHost+L"/donate/go?kind="+donateKind()+L"&amount="+std::to_wstring(donateAmounts[donateKind()[0]==L'k'?0:1][donatePick]);ShellExecuteW(nullptr,L"open",u.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
        else if(id==256)DestroyWindow(h);}return 0;
    case WM_ERASEBKGND:return eraseCarbon(h,(HDC)wp);
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:delete donateQr;donateQr=nullptr;donateSeq++;return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
