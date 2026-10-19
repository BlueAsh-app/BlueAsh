#pragma once
// 설정 > 문제·건의 보내기 (page 10): a message, an optional contact, up to 3 pictures and, if left on, the diagnostics
// text go to the relay (relay/feedback.js POST /feedback), where only the developer reads them. Paths under the user's
// profile are masked. Pictures (a file, or a screen capture from the clipboard) are shrunk to a JPEG of at most
// 1920 px before sending; the page shows them as thumbnails that are removed with a click.
bool feedbackDiag=true;
struct FeedbackShot{std::string jpeg;Gdiplus::Bitmap* thumb=nullptr;};
std::vector<FeedbackShot> feedbackShots;
const int shotW=104,shotH=64,shotY=364,shotX=410,shotGap=12;
std::string jsonText(const std::wstring& w){
    int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string u(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&u[0],n,nullptr,nullptr);
    std::string o="\"";for(unsigned char c:u){if(c=='"'||c=='\\'){o+='\\';o+=(char)c;}else if(c=='\n')o+="\\n";else if(c=='\r')o+="\\r";else if(c=='\t')o+="\\t";else if(c<0x20){char b[8];snprintf(b,8,"\\u%04x",c);o+=b;}else o+=(char)c;}
    return o+"\"";
}
std::wstring maskProfile(std::wstring s){
    wchar_t home[MAX_PATH]={};if(!GetEnvironmentVariableW(L"USERPROFILE",home,MAX_PATH)||!home[0])return s;std::wstring h=home,low=s;
    auto lower=[](std::wstring& t){CharLowerBuffW(&t[0],(DWORD)t.size());};std::wstring hl=h;lower(hl);lower(low);
    for(size_t at=low.find(hl);at!=std::wstring::npos;at=low.find(hl,at+12)){s.replace(at,h.size(),L"%USERPROFILE%");low.replace(at,h.size(),L"%userprofile%");}
    return s;
}
std::wstring controlText(HWND c){int n=GetWindowTextLengthW(c);std::wstring s(n,L'\0');if(n)GetWindowTextW(c,&s[0],n+1);return s;}
void feedbackStatus(HWND h,const wchar_t* text,bool bad=false){HWND s=GetDlgItem(h,243);SetWindowLongPtrW(s,GWLP_USERDATA,bad?3:1);SetWindowTextW(s,text);InvalidateRect(s,nullptr,TRUE);}
CLSID jpegEncoder(){
    UINT n=0,size=0;Gdiplus::GetImageEncodersSize(&n,&size);std::vector<BYTE> buf(size);auto* e=(Gdiplus::ImageCodecInfo*)buf.data();
    if(size&&Gdiplus::GetImageEncoders(n,size,e)==Gdiplus::Ok)for(UINT i=0;i<n;i++)if(!wcscmp(e[i].MimeType,L"image/jpeg"))return e[i].Clsid;
    return CLSID{};
}
// Longest side at most maxSide, then JPEG at the given quality; "" when GDI+ cannot.
std::string shrinkJpeg(Gdiplus::Bitmap& src,int maxSide,ULONG quality){
    int w=(int)src.GetWidth(),h=(int)src.GetHeight();if(w<1||h<1)return {};double k=std::min(1.0,(double)maxSide/std::max(w,h));int ow=std::max(1,(int)(w*k+.5)),oh=std::max(1,(int)(h*k+.5));
    Gdiplus::Bitmap out(ow,oh,PixelFormat24bppRGB);{Gdiplus::Graphics g(&out);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);g.DrawImage(&src,0,0,ow,oh);}
    IStream* s=nullptr;if(FAILED(CreateStreamOnHGlobal(nullptr,TRUE,&s)))return {};CLSID jpg=jpegEncoder();
    Gdiplus::EncoderParameters p;p.Count=1;p.Parameter[0].Guid=Gdiplus::EncoderQuality;p.Parameter[0].Type=Gdiplus::EncoderParameterValueTypeLong;p.Parameter[0].NumberOfValues=1;p.Parameter[0].Value=&quality;
    std::string bytes;if(out.Save(s,&jpg,&p)==Gdiplus::Ok){STATSTG st={};LARGE_INTEGER zero={};if(SUCCEEDED(s->Stat(&st,STATFLAG_NONAME))&&SUCCEEDED(s->Seek(zero,STREAM_SEEK_SET,nullptr))){bytes.resize((size_t)st.cbSize.QuadPart);ULONG got=0;s->Read(&bytes[0],(ULONG)bytes.size(),&got);bytes.resize(got);}}
    s->Release();return bytes;
}
// Adds one picture (taken from src); the relay takes 1.5 MB each, so a big one is retried smaller.
bool addShot(HWND h,Gdiplus::Bitmap* src){
    if(!src||src->GetLastStatus()!=Gdiplus::Ok||src->GetWidth()<2){delete src;feedbackStatus(h,i18n::text(L"사진을 읽지 못했습니다."),true);return false;}
    std::string jpg=shrinkJpeg(*src,1920,85);if(jpg.size()>1400000)jpg=shrinkJpeg(*src,1440,72);if(jpg.size()>1400000)jpg=shrinkJpeg(*src,1100,65);
    if(jpg.empty()||jpg.size()>1400000){delete src;feedbackStatus(h,i18n::text(L"사진을 읽지 못했습니다."),true);return false;}
    auto* thumb=new Gdiplus::Bitmap(shotW,shotH,PixelFormat32bppARGB);
    {Gdiplus::Graphics g(thumb);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.Clear(Gdiplus::Color(255,GetRValue(well),GetGValue(well),GetBValue(well)));
     double k=std::max((double)shotW/src->GetWidth(),(double)shotH/src->GetHeight());int w=(int)(src->GetWidth()*k+.5),hh=(int)(src->GetHeight()*k+.5);g.DrawImage(src,(shotW-w)/2,(shotH-hh)/2,w,hh);}
    delete src;feedbackShots.push_back({jpg,thumb});
    for(int id:{246,247})EnableWindow(GetDlgItem(h,id),feedbackShots.size()<3);
    RECT area={shotX-4,shotY-4,shotX+3*(shotW+shotGap),shotY+shotH+6};InvalidateRect(h,&area,TRUE);feedbackStatus(h,L"");return true;
}
void clearShots(){for(auto& s:feedbackShots)delete s.thumb;feedbackShots.clear();}
void pickShotFile(HWND h){
    wchar_t file[MAX_PATH]=L"";std::wstring filter=std::wstring(i18n::text(L"사진"))+L" (png, jpg, bmp, gif)";filter.push_back(L'\0');filter+=L"*.png;*.jpg;*.jpeg;*.bmp;*.gif";filter.push_back(L'\0');filter.push_back(L'\0');
    OPENFILENAMEW o={};o.lStructSize=sizeof(o);o.hwndOwner=win;o.lpstrFilter=filter.c_str();o.lpstrFile=file;o.nMaxFile=MAX_PATH;o.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
    if(!GetOpenFileNameW(&o))return;
    WIN32_FILE_ATTRIBUTE_DATA a={};if(GetFileAttributesExW(file,GetFileExInfoStandard,&a)&&(a.nFileSizeHigh||a.nFileSizeLow>40u*1024*1024)){feedbackStatus(h,i18n::text(L"사진 파일이 너무 큽니다 (40MB까지)."),true);return;}
    addShot(h,new Gdiplus::Bitmap(file));
}
// A screen capture (Win+Shift+S, Print Screen) sits in the clipboard as a bitmap.
void pasteShot(HWND h){
    if(!IsClipboardFormatAvailable(CF_BITMAP)||!OpenClipboard(h)){feedbackStatus(h,i18n::text(L"클립보드에 캡처한 화면이 없습니다. Win+Shift+S로 화면을 캡처한 뒤 눌러 주세요."),true);return;}
    HBITMAP b=(HBITMAP)GetClipboardData(CF_BITMAP);Gdiplus::Bitmap* copy=nullptr;
    // Drawn into our own bitmap: a Clone() would keep pointing at source, which is gone after this block.
    if(b){Gdiplus::Bitmap source(b,nullptr);if(source.GetLastStatus()==Gdiplus::Ok&&source.GetWidth()>1){copy=new Gdiplus::Bitmap((INT)source.GetWidth(),(INT)source.GetHeight(),PixelFormat24bppRGB);Gdiplus::Graphics c(copy);c.DrawImage(&source,0,0,(INT)source.GetWidth(),(INT)source.GetHeight());}}
    CloseClipboard();addShot(h,copy);
}
int shotAt(int x,int y){for(int i=0;i<(int)feedbackShots.size();i++){int l=shotX+i*(shotW+shotGap);if(x>=l&&x<l+shotW&&y>=shotY&&y<shotY+shotH)return i;}return -1;}
void paintShots(HDC dc){
    if(!painting::token){return;}Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for(int i=0;i<(int)feedbackShots.size();i++){int l=shotX+i*(shotW+shotGap);
        Gdiplus::GraphicsPath clip;painting::path(clip,{l,shotY,l+shotW,shotY+shotH},12);g.SetClip(&clip);g.DrawImage(feedbackShots[i].thumb,l,shotY,shotW,shotH);g.ResetClip();
        Gdiplus::Pen edge(painting::color(line),1.f);g.DrawPath(&edge,&clip);
        // The remove mark: a small dark disc with an ✕ in the top-right corner.
        int cx=l+shotW-12,cy=shotY+12;Gdiplus::SolidBrush disc(Gdiplus::Color(210,8,10,16));g.FillEllipse(&disc,cx-9,cy-9,18,18);
        Gdiplus::Pen x(painting::color(ink),1.6f);g.DrawLine(&x,cx-4,cy-4,cx+4,cy+4);g.DrawLine(&x,cx+4,cy-4,cx-4,cy+4);}
}
LRESULT CALLBACK feedbackProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        // The page draws its own rounded panel inset 12 px (eraseCarbon): content starts 36 px in and below its top edge.
        clearShots();
        pageLabel(h,i18n::text(L"무엇이 문제였나요? 바라는 점도 편하게 적어 주세요."),36,34,780,26,2);
        // No Windows scroll bar (it looked pasted on): the box still scrolls with the wheel, arrows and while typing.
        HWND text=addSetting(h,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,36,66,780,150,240);SendMessageW(text,EM_SETLIMITTEXT,2000,0);
        pageLabel(h,i18n::text(L"답장 받을 곳 (선택) · 디스코드 아이디나 이메일"),36,232,780,26,2);
        HWND contact=addSetting(h,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,36,264,780,36,241);SendMessageW(contact,EM_SETLIMITTEXT,100,0);
        pageLabel(h,i18n::text(L"사진 (선택) · 3장까지, 문제가 보이는 화면을 넣어 주세요."),36,318,780,26,2);
        pageButton(h,i18n::text(L"사진 추가"),36,364,170,246);
        pageButton(h,i18n::text(L"캡처 붙여넣기"),218,364,176,247);
        pageLabel(h,i18n::text(L"Win+Shift+S로 화면을 캡처하고 캡처 붙여넣기를 누르세요. 사진을 누르면 뺍니다."),36,408,370,40,1);
        addSetting(h,L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,36,452,780,42,242);
        pageLabel(h,i18n::text(L"진단 정보: BlueAsh 버전, Windows 버전, 관리자 권한 여부, 타르코프 화면 모드·지도 키, 도우미 오류 기록. 사용자 폴더 경로는 가려서 보냅니다."),36,500,780,40,1);
        pageLabel(h,L"",36,560,480,40,1,243);
        pageButton(h,i18n::text(L"닫기"),696,556,120,244);
        pageButton(h,i18n::text(L"보내기"),540,556,140,245,true);
        SetFocus(text);return 0;}
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paintShots(dc);EndPaint(h,&ps);return 0;}
    // Without its scroll bar the text box may pass the wheel up to the page: scroll it here (3 lines a notch).
    case WM_MOUSEWHEEL:SendDlgItemMessageW(h,240,EM_LINESCROLL,0,-3*GET_WHEEL_DELTA_WPARAM(wp)/WHEEL_DELTA);return 0;
    case WM_SETCURSOR:if((HWND)wp==h){POINT p;GetCursorPos(&p);ScreenToClient(h,&p);if(shotAt(p.x,p.y)>=0){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}break;
    case WM_LBUTTONDOWN:{int i=shotAt((short)LOWORD(lp),(short)HIWORD(lp));if(i>=0){delete feedbackShots[i].thumb;feedbackShots.erase(feedbackShots.begin()+i);for(int id:{246,247})EnableWindow(GetDlgItem(h,id),TRUE);
        RECT area={shotX-4,shotY-4,shotX+3*(shotW+shotGap),shotY+shotH+6};InvalidateRect(h,&area,TRUE);}return 0;}
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){int id=LOWORD(wp);
        if(id==244)DestroyWindow(h);
        else if(id==246)pickShotFile(h);
        else if(id==247)pasteShot(h);
        else if(id==242){feedbackDiag=!feedbackDiag;InvalidateRect(GetDlgItem(h,242),nullptr,TRUE);}
        else if(id==245){
            std::wstring text=controlText(GetDlgItem(h,240));size_t a=text.find_first_not_of(L" \r\n\t");
            if(a==std::wstring::npos){feedbackStatus(h,i18n::text(L"보낼 내용을 적어 주세요."),true);SetFocus(GetDlgItem(h,240));return 0;}
            auto id32=installId();if(id32.empty()){feedbackStatus(h,i18n::text(L"보내지 못했습니다. 잠시 뒤 다시 시도해 주세요."),true);return 0;}
            std::string images="[";for(size_t i=0;i<feedbackShots.size();i++){auto b=base64(feedbackShots[i].jpeg);images+=(i?",\"":"\"")+std::string(b.begin(),b.end())+"\"";}images+="]";
            std::string body="{\"id\":"+jsonText(id32)+",\"v\":"+jsonText(appVersion)+",\"lang\":"+jsonText(i18n::languageCode())+",\"text\":"+jsonText(text)
                +",\"contact\":"+jsonText(controlText(GetDlgItem(h,241)))+",\"diag\":"+jsonText(feedbackDiag?maskProfile(diagnosticsText()):L"")+",\"images\":"+images+"}";
            for(int c:{245,246,247}){EnableWindow(GetDlgItem(h,c),FALSE);}feedbackStatus(h,feedbackShots.empty()?i18n::text(L"보내는 중…"):i18n::text(L"사진과 함께 보내는 중…"));
            struct Job{HWND page;std::string body;};auto* job=new Job{h,body};
            HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{auto* j=(Job*)p;DWORD s=relayRequest(L"POST",L"/feedback",j->body);PostMessageW(j->page,WM_APP+180,s,0);delete j;return 0;},job,0,nullptr);
            if(t)CloseHandle(t);else{delete job;for(int c:{245,246,247})EnableWindow(GetDlgItem(h,c),TRUE);feedbackStatus(h,i18n::text(L"보내지 못했습니다. 잠시 뒤 다시 시도해 주세요."),true);}
        }}return 0;
    case WM_APP+180:
        EnableWindow(GetDlgItem(h,245),TRUE);
        if(wp==200){SetWindowTextW(GetDlgItem(h,240),L"");clearShots();InvalidateRect(h,nullptr,TRUE);feedbackStatus(h,i18n::text(L"보냈습니다. 고맙습니다! 확인해서 다음 업데이트에 반영할게요."));}
        else if(wp==429)feedbackStatus(h,i18n::text(L"너무 자주 보냈습니다. 한 시간쯤 뒤에 다시 보내 주세요."),true);
        else if(wp==413)feedbackStatus(h,i18n::text(L"사진이 너무 큽니다. 사진을 빼고 다시 보내 주세요."),true);
        else feedbackStatus(h,wp?i18n::text(L"보내지 못했습니다. 잠시 뒤 다시 시도해 주세요."):i18n::text(L"서버에 연결하지 못했습니다. 인터넷 연결을 확인하세요."),true);
        for(int id:{246,247})EnableWindow(GetDlgItem(h,id),feedbackShots.size()<3);
        return 0;
    case WM_ERASEBKGND:return eraseCarbon(h,(HDC)wp);
    case WM_CTLCOLORSTATIC:{if(GetWindowLongPtrW((HWND)lp,GWLP_USERDATA)==3){HDC dc=(HDC)wp;SetTextColor(dc,danger);SetBkColor(dc,panel);return staticBackground(dc,(HWND)lp);}return pageStatic(wp,lp);}
    case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    case WM_DESTROY:clearShots();return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
