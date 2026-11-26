#pragma once
// 배그 · 사운드 부스트 settings (page 19, the gear beside the row on 게임 도우미; that row's switch turns it on and off).
// Four preset cards with an icon, one picked (밸런스 recommended: it keeps the direction and distance cues); the picked
// preset's EQ curve drawn like Equalizer APO's graph; under it, a built-in preset's pros and cautions (자세히 보기 in the
// graph's corner shows its values, read-only), or 사용자 지정's two sliders. 사용자 지정 starts from the preset picked
// before it the first time and is edited on the graph (drag its six points), heard at once while PUBG runs. A banner says
// when the sound changed. Below: Windows' 음향 강도 이퀄라이제이션 (the same box as in the speaker's properties). Everything is drawn off-screen and copied at once.
// The box's second tab, 미리 듣기: a radar with the listener (a white headset) in the middle; 발소리 and 총소리 each play
// the player's own recording once from a random direction and distance, through the picked preset (sound_preview.h),
// and the radar shows where it came from.
extern HWND pageWindow;
namespace soundpage{
HWND page=nullptr;int sel=0,drag=-1,hoverCard=-1,hoverPoint=-1;bool details=false,hoverLink=false,enhKnown=false;std::wstring note;COLORREF noteColor=0;ULONGLONG noteAt=0;RECT linkRect={};
const RECT graph={36,138,816,320},plot={92,188,800,290};
const COLORREF good=RGB(70,208,146),warn=RGB(244,178,84);
// 미리 듣기: the tab (0 graph, 1 radar), the radar disc, the two play cards, the last sound and when it started
int view=0,hoverTab=-1,hoverPlay=-1,waiting=-1;RECT tabRects[2]={};sbprev::Result shot;ULONGLONG heardAt=0;
const RECT enhCard={36,436,816,500},infoRect={36,510,672,552}; // Windows' 음향 강도 이퀄라이제이션 card; a failure note beside 닫기
// the little ! marks (0 압축 강도, 1 조용한 소리 키우기, 2 음향 강도 이퀄라이제이션, 3 프리셋) and the one the pointer is on
int hoverInfo=-1;RECT infoIcons[4]={};
const RECT radarBox={36,138,816,424};const int rcx=184,rcy=292,rr=106;const RECT disc={rcx-rr-4,rcy-rr-4,rcx+rr+4,rcy+rr+4};
// the distance test: row k (0 footsteps, 1 gunshots), button i (near, middle, far); play codes 0..5 = k*3+i
RECT testRow(int k){return {344,252+k*76,800,316+k*76};}
RECT testBtn(int k,int i){RECT r=testRow(k);int w=86,x=r.left+170+i*(w+8);return {x,r.top+12,x+w,r.top+52};}
// 사용자 지정: two pills in the graph box that play the recording as it is through the custom preset (codes 10, 11)
RECT quickRects[2]={};
bool radarLive(){return heardAt&&!shot.error&&GetTickCount64()-heardAt<(ULONGLONG)shot.ms+1600;}
// four cards: 적용 안 함, 밸런스, 발소리 집중, 사용자 지정 (card k shows cardPreset[k]; 2, the dropped 총성 완화's slot, is
// 적용 안 함 here: picking it turns sound boost off, picking any other card turns it on with that preset)
const int Off=2,cardPreset[4]={Off,0,1,3};
RECT cardRect(int k){return {36+k*198,58,36+k*198+186,126};}
const wchar_t* cardName(int i){return i==Off?i18n::text(L"적용 안 함"):soundboost::presetName(i);}
RECT sliderRow(int k){return {36,334+k*44,816,370+k*44};} // 0 압축 강도, 1 조용한 소리 키우기
const int trackL=250,trackR=700;
double fx(double f){return plot.left+(plot.right-plot.left)*std::log10(f/20)/3;} // 20 Hz .. 20 kHz
double fy(double db){return plot.top+(plot.bottom-plot.top)*(12-db)/24;} // +12 .. -12 dB
sbdsp::Preset shown(){return sel==3?sbdsp::customPreset(soundboost::custom()):sbdsp::preset(sel);}
std::wstring fmt(const wchar_t* key,...){wchar_t s[512];va_list a;va_start(a,key);vswprintf(s,512,i18n::text(key),a);va_end(a);return s;}
bool live(){return soundboost::state==soundboost::Applied;}
void tell(const std::wstring& s,COLORREF c=good){note=s;noteColor=c;noteAt=GetTickCount64();if(page){SetTimer(page,2,3000,nullptr);InvalidateRect(page,&graph,FALSE);}}
void buttons(){if(page)ShowWindow(GetDlgItem(page,78),sel==3&&view==0?SW_SHOW:SW_HIDE);}
void choose(int i){if(i==sel)return;int was=sel;sel=i;details=false;sbprev::stop();heardAt=0;waiting=-1; /* the sound of the previous preset stops */
    if(i==Off){soundboost::setOn(false);tell(i18n::text(L"적용 안 함 · 배그 원래 소리로 돌아갑니다"),muted);buttons();InvalidateRect(page,nullptr,FALSE);return;}
    bool fresh=i==3&&!soundboost::customExists();
    if(fresh)soundboost::setCustom(sbdsp::customFrom(sbdsp::preset(was==Off?1:was)),true); /* 사용자 지정 starts from the preset picked before it */
    soundboost::setPreset(i);if(!soundboost::on())soundboost::setOn(true);
    tell(fresh?std::wstring(i18n::text(L"나만의 프리셋을 만들었습니다 · 그래프의 점을 끌어 조절하세요")):live()?fmt(L"사운드를 바꿨습니다 · %ls · 배그에 바로 적용됐습니다",soundboost::presetName(i)):fmt(L"%ls 선택됨 · 배그를 켜면 적용됩니다",soundboost::presetName(i)));
    buttons();InvalidateRect(page,nullptr,FALSE);}
// the value a drag puts in: a point's gain from y (0.5 dB steps), a slider's value from x
void dragTo(int x,int y){sbdsp::Custom c=soundboost::custom();
    if(drag<6){double db=12-24.0*(y-plot.top)/(plot.bottom-plot.top);c.g[drag]=std::round(std::clamp(db,-12.0,6.0)*2)/2;}
    else{double k=std::clamp(double(x-trackL)/(trackR-trackL),0.0,1.0);if(drag==6)c.comp=std::round(k*100);else c.lift=std::round(k*12)/2;}
    soundboost::setCustom(c,false);InvalidateRect(page,nullptr,FALSE);}
int pointAt(int x,int y){if(sel!=3||view)return -1;sbdsp::Custom c=soundboost::custom();for(int k=0;k<6;k++){double dx=x-fx(sbdsp::CustomHz[k]),dy=y-fy(c.g[k]);if(dx*dx+dy*dy<=14*14)return k;}return -1;}
int cardAt(int x,int y){for(int k=0;k<4;k++){RECT r=cardRect(k);if(x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom)return cardPreset[k];}return -1;}
bool inside(const RECT& r,int x,int y){return x>=r.left&&x<r.right&&y>=r.top&&y<r.bottom;}
int infoAt(int x,int y){for(int k=0;k<4;k++)if(inside(infoIcons[k],x,y))return k;return -1;}
// the app's own ? (the circled question mark of the dashboard and the Aim Lab table), glowing while the pointer is on it
void infoMark(HDC dc,int k,int x,int cy){RECT r={x,cy-12,x+24,cy+12};infoIcons[k]=r;bool on=hoverInfo==k;
    if(on){painting::iconGlow(dc,r,RGB(120,160,255),3,1.6,[&](HDC d,RECT){glyph(d,0xE9CE,r,footFont,RGB(255,255,255));});}
    glyph(dc,0xE9CE,r,footFont,on?ink:muted);}
// the tip for the ! under the pointer: a card above it (below when there is no room), drawn last so it covers the page
void paintTip(HDC dc){
#ifdef BLUEASH_TT
    if(hoverInfo<0&&_wgetenv(L"BLUEASH_TIP"))hoverInfo=_wtoi(_wgetenv(L"BLUEASH_TIP")); /* test build: show one tip without a pointer */
#endif
    if(hoverInfo<0)return;
    static const wchar_t* tips[]={L"큰 소리와 작은 소리의 차이를 얼마나 줄일지 정합니다. 높일수록 작은 소리가 상대적으로 커지고, 낮추면 원래 소리의 강약이 살아납니다. 큰 소리는 어느 값이든 게임 원래 크기를 넘지 않습니다.",
        L"조용할 때 작은 소리 전체를 얼마나 키울지 정합니다(최대 +6 dB). 발소리가 멀게 느껴지면 올리고, 잡음이 거슬리면 내리세요. 큰 소리가 난 직후에는 키우지 않습니다.",
        L"Windows 스피커 속성 > 고급 기능의 같은 이름 설정입니다. 배그만이 아니라 이 스피커로 나오는 모든 소리에 적용되며, 여기서 바꾸면 Windows 설정도 함께 바뀝니다.",
        L"처리하는 동안 Windows 볼륨 믹서에는 배그가 음소거로 보입니다(원래 소리 대신 처리한 소리를 내보내기 때문). 끄면 원래대로 돌아옵니다. 독점 전체 화면에서는 적용 알림이 보이지 않을 수 있습니다."};
    std::wstring t=i18n::text(tips[hoverInfo]);const int w=360;RECT a=infoIcons[hoverInfo];
    auto o=SelectObject(dc,smallFont);RECT c={0,0,w-36,0};std::wstring wr=wrapWords(dc,t,w-36);DrawTextW(dc,wr.c_str(),-1,&c,DT_CALCRECT|DT_WORDBREAK|DT_NOPREFIX);SelectObject(dc,o);
    int hgt=c.bottom+28,left=std::clamp((int)a.left-24,8,816-w),top=a.top-8-hgt;if(top<8)top=a.bottom+8;
    RECT box={left,top,left+w,top+hgt},sh=box;OffsetRect(&sh,0,4);roundBox(dc,sh,RGB(4,8,20),RGB(4,8,20),14);roundBox(dc,box,RGB(22,36,72),RGB(64,100,176),14);
    textAt(dc,t,{box.left+18,box.top+14,box.right-18,box.bottom-12},smallFont,ink,DT_LEFT|DT_TOP|DT_WORDBREAK);}
// a running figure facing right (like an exit sign), stroked so it stays crisp at any size; 24-unit design box
void runner(HDC dc,RECT r,COLORREF c){if(!painting::token)return;Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    float k=std::min(r.right-r.left,r.bottom-r.top)/24.f,ox=(r.left+r.right)/2.f-12*k,oy=(r.top+r.bottom)/2.f-12*k;auto P=[&](float x,float y){return Gdiplus::PointF(ox+x*k,oy+y*k);};
    Gdiplus::Color col(255,GetRValue(c),GetGValue(c),GetBValue(c));Gdiplus::SolidBrush b(col);Gdiplus::Pen pen(col,3.0f*k);pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);pen.SetLineJoin(Gdiplus::LineJoinRound);
    g.FillEllipse(&b,ox+13.4f*k,oy+1.2f*k,5.2f*k,5.2f*k); /* head */
    Gdiplus::PointF torso[]={P(14.6f,8.4f),P(11.2f,14.6f)},armF[]={P(14.2f,9.2f),P(17.2f,11.6f),P(20.2f,10.2f)},armB[]={P(13.8f,9.0f),P(10.2f,9.8f),P(7.6f,12.2f)},
        legF[]={P(11.2f,14.6f),P(15.4f,16.8f),P(14.6f,21.6f)},legB[]={P(11.2f,14.6f),P(8.4f,18.4f),P(4.2f,19.4f)};
    g.DrawLines(&pen,torso,2);g.DrawLines(&pen,armF,3);g.DrawLines(&pen,armB,3);g.DrawLines(&pen,legF,3);g.DrawLines(&pen,legB,3);}
int tabAt(int x,int y){for(int i=0;i<2;i++)if(inside(tabRects[i],x,y))return i;return -1;}
int playAt(int x,int y){if(view){for(int c=0;c<6;c++)if(inside(testBtn(c/3,c%3),x,y))return c;return -1;}
    if(sel==3){for(int k=0;k<2;k++)if(inside(quickRects[k],x,y))return 10+k;}
    return -1;}
bool playingCode(int c){if(waiting==c)return true;if(!heardAt||shot.error||GetTickCount64()-heardAt>=(ULONGLONG)shot.ms)return false;
    return c>=10?shot.dist<=0&&shot.kind==c-10:shot.dist>0&&shot.kind==c/3&&shot.dist==sbprev::testDist(c/3,c%3);}
bool onLink(int x,int y){return sel<2&&!view&&x>=linkRect.left&&x<linkRect.right&&y>=linkRect.top&&y<linkRect.bottom;}
std::wstring hz(double f){wchar_t b[24];if(f>=1000)swprintf(b,24,f>=10000||std::fmod(f,1000)==0?L"%.0fk":L"%.1fk",f/1000);else swprintf(b,24,L"%.0f",f);return b;}
std::wstring signedDb(double v){wchar_t b[24];swprintf(b,24,L"%+.1f dB",v);return b;}

// the box's two tabs (소리 변화 · 미리 듣기), a segmented pill in its top-left corner
int paintTabs(HDC dc){const wchar_t* names[]={L"소리 변화",L"거리별 테스트"};int x=graph.left+14,top=graph.top+8;int w[2];
    for(int i=0;i<2;i++)w[i]=textWidth(dc,smallBold,i18n::text(names[i]))+30;
    RECT all={x,top,x+w[0]+w[1]+6,top+30};roundBox(dc,all,RGB(10,18,40),line,15);
    for(int i=0,at=x+3;i<2;at+=w[i],i++){tabRects[i]={at,top+3,at+w[i],top+27};bool on=i==view;if(on)roundBox(dc,tabRects[i],blue,blue,12);else if(i==hoverTab)roundBox(dc,tabRects[i],RGB(24,40,80),RGB(24,40,80),12);
        textAt(dc,i18n::text(names[i]),tabRects[i],smallBold,on?RGB(255,255,255):i==hoverTab?ink:muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    return all.right;}
// the banner (what just changed, for 3 s), centred on cx
void paintNote(HDC dc,int cx,int top,int maxW){if(note.empty()||GetTickCount64()-noteAt>=3000)return;int w=std::min<int>(textWidth(dc,smallBold,note.c_str())+44,maxW);RECT b={cx-w/2,top,cx+w/2,top+32};
    roundBox(dc,b,RGB(14,38,34),noteColor,16);textAt(dc,note,b,smallBold,noteColor,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
void paintGraph(HDC dc){
    roundBox(dc,graph,rowFill,line,16);const UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE;
    int tabsRight=paintTabs(dc);
    linkRect={};int noteRight=graph.right-18;quickRects[0]=quickRects[1]={};
    // 사용자 지정: 발소리 / 총소리 pills that play the recording as it is through this preset
    if(sel==3){int x=graph.right-14;for(int k=1;k>=0;k--){const wchar_t* n=i18n::text(k?L"총소리":L"발소리");int w=textWidth(dc,smallBold,n)+46;quickRects[k]={x-w,graph.top+8,x,graph.top+36};x-=w+8;
            int c=10+k;bool busy=playingCode(c),hov=hoverPlay==c;COLORREF col=k?warn:good;RECT q=quickRects[k];
            roundBox(dc,q,busy?painting::mixColor(rowFill,col,.18):hov?RGB(28,52,110):rowFill,busy?col:hov?blue:line,14);
            if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float px=q.left+17.f,py=(q.top+q.bottom)/2.f;Gdiplus::PointF tri[]={{px-4,py-5.5f},{px+5.5f,py},{px-4,py+5.5f}};
                Gdiplus::SolidBrush b(Gdiplus::Color(255,GetRValue(col),GetGValue(col),GetBValue(col)));g.FillPolygon(&b,tri,3);}
            textAt(dc,n,{q.left+28,q.top,q.right-10,q.bottom},smallBold,busy||hov?ink:RGB(170,196,255),DT_LEFT|DT_VCENTER|DT_SINGLELINE);}
        noteRight=quickRects[0].left-14;}
    if(sel<2){std::wstring t=details?i18n::text(L"접기"):i18n::text(L"자세히 보기");int w=textWidth(dc,smallBold,t.c_str())+28;linkRect={graph.right-14-w,graph.top+8,graph.right-14,graph.top+36};
        roundBox(dc,linkRect,hoverLink||details?RGB(28,52,110):rowFill,hoverLink||details?blue:line,14);textAt(dc,t,linkRect,smallBold,hoverLink||details?ink:RGB(140,176,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);noteRight=linkRect.left-14;}
    textAt(dc,i18n::text(L"조용한 소리 기준 · 큰 소리는 원래 크기를 넘지 않습니다"),{tabsRight+14,graph.top+10,noteRight,graph.top+34},captionFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    static HBRUSH grid=CreateSolidBrush(RGB(30,46,84)),zero=CreateSolidBrush(RGB(58,82,136));
    for(int db=-12;db<=12;db+=6){int y=(int)std::lround(fy(db));painting::hairline(dc,{plot.left,y,plot.right,y+1},db==0?zero:grid);
        textAt(dc,db>0?L"+"+std::to_wstring(db):std::to_wstring(db),{graph.left+8,y-9,plot.left-10,y+9},captionFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
    for(double f:{50.0,100.0,200.0,500.0,1000.0,2000.0,5000.0,10000.0}){int x=(int)std::lround(fx(f));painting::hairline(dc,{x,plot.top,x+1,plot.bottom},grid);
        textAt(dc,hz(f),{x-26,plot.bottom+6,x+26,plot.bottom+24},captionFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    if(!painting::token)return;
    sbdsp::Preset p=shown();std::vector<Gdiplus::PointF> pts;
    for(int i=0;i<=240;i++){double f=20*std::pow(10.0,3.0*i/240),db=sel==Off?0:std::clamp(sbdsp::responseDb(p,f),-12.0,12.0);pts.push_back({(float)fx(f),(float)fy(db)});}
    sbdsp::Custom c=soundboost::custom();int act=sel==3?(drag>=0&&drag<6?drag:hoverPoint):-1;
    {Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
     {std::vector<Gdiplus::PointF> area=pts;float z=(float)fy(0);area.push_back({pts.back().X,z});area.push_back({pts.front().X,z});
      Gdiplus::LinearGradientBrush fill(Gdiplus::PointF(0,(float)plot.top),Gdiplus::PointF(0,(float)plot.bottom),Gdiplus::Color(86,60,124,255),Gdiplus::Color(14,47,108,255));g.FillPolygon(&fill,area.data(),(INT)area.size());}
     Gdiplus::Pen glow(Gdiplus::Color(60,96,150,255),7.f);glow.SetLineJoin(Gdiplus::LineJoinRound);g.DrawLines(&glow,pts.data(),(INT)pts.size());
     Gdiplus::Pen pen(Gdiplus::Color(255,120,168,255),2.4f);pen.SetLineJoin(Gdiplus::LineJoinRound);g.DrawLines(&pen,pts.data(),(INT)pts.size());
     if(sel==3){for(int k=0;k<6;k++){float x=(float)fx(sbdsp::CustomHz[k]),y=(float)fy(c.g[k]),r=k==act?9.f:7.f;
            Gdiplus::SolidBrush halo(Gdiplus::Color(k==act?90:0,47,108,255));g.FillEllipse(&halo,x-r-5,y-r-5,(r+5)*2,(r+5)*2);
            Gdiplus::SolidBrush dot(Gdiplus::Color(255,247,249,255));g.FillEllipse(&dot,x-r,y-r,r*2,r*2);Gdiplus::Pen edge(Gdiplus::Color(255,47,108,255),2.4f);g.DrawEllipse(&edge,x-r,y-r,r*2,r*2);}}}
    // GDI text only after the GDI+ Graphics above is gone (while it holds the DC, GDI drawing loses the page's scale)
    if(act>=0){int x=(int)std::lround(fx(sbdsp::CustomHz[act])),y=(int)std::lround(fy(c.g[act]));std::wstring t=hz(sbdsp::CustomHz[act])+L"Hz  "+signedDb(c.g[act]);
        int w=textWidth(dc,smallBold,t.c_str())+22;RECT b={x-w/2,y-42,x+w/2,y-16};if(b.left<plot.left)OffsetRect(&b,plot.left-b.left,0);if(b.right>plot.right)OffsetRect(&b,plot.right-b.right,0);if(b.top<graph.top+4)OffsetRect(&b,0,58);
        roundBox(dc,b,RGB(24,44,96),blue,12);textAt(dc,t,b,smallBold,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
    // 자세히 보기: a built-in preset's values over the lower part of the curve (they cannot be changed)
    if(details&&sel<2){const sbdsp::Preset& q=sbdsp::preset(sel);std::wstring bands;const wchar_t* kind[]={L"",L"",L"↓",L"↑"};
        for(auto& b:q.bands){if(b.type==0)continue;if(!bands.empty())bands+=L" · ";bands+=hz(b.f)+L"Hz"+kind[b.type]+L" "+signedDb(b.db);}
        RECT box={plot.left+10,plot.bottom-82,plot.right-10,plot.bottom-6};roundBox(dc,box,RGB(10,18,40),RGB(46,72,128),12);
        textAt(dc,bands,{box.left+16,box.top+8,box.right-16,box.top+30},smallFont,ink,one|DT_END_ELLIPSIS);
        textAt(dc,fmt(L"압축 %.1f:1 · 기준 %d dB · 조용한 소리 +%.1f dB",q.ratio,(int)q.threshold,q.makeup),{box.left+16,box.top+30,box.right-16,box.top+52},smallFont,ink,one|DT_END_ELLIPSIS);
        textAt(dc,i18n::text(L"기본 프리셋은 바꿀 수 없습니다 · 바꾸려면 사용자 지정을 고르세요"),{box.left+16,box.top+52,box.right-16,box.bottom-4},captionFont,muted,one|DT_END_ELLIPSIS);}
    paintNote(dc,(plot.left+plot.right)/2,plot.top+4,plot.right-plot.left);
}
// Under the graph: a built-in preset's pros and cautions, or 사용자 지정's sliders.
void paintBelow(HDC dc){const UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE;
    if(sel==3){sbdsp::Custom c=soundboost::custom();const wchar_t* names[]={L"압축 강도",L"조용한 소리 키우기"};
        for(int k=0;k<2;k++){RECT r=sliderRow(k);double v=k?c.lift/6:c.comp/100;int lw=std::min(textWidth(dc,font,i18n::text(names[k])),(int)(trackL-r.left-46));textAt(dc,i18n::text(names[k]),{r.left,r.top,r.left+lw,r.bottom},font,ink,one|DT_END_ELLIPSIS);infoMark(dc,k,r.left+lw+8,(r.top+r.bottom)/2);
            int cy=(r.top+r.bottom)/2,kx=trackL+(int)std::lround(v*(trackR-trackL));roundBox(dc,{trackL,cy-3,trackR,cy+3},well,well,3);roundBox(dc,{trackL,cy-3,std::max(trackL+6,kx),cy+3},blue,blue,3);
            painting::circle(dc,{kx-9,cy-9,kx+9,cy+9},RGB(247,249,255),drag==6+k?blue:RGB(247,249,255));
            wchar_t t[24];if(k)swprintf(t,24,L"+%.1f dB",c.lift);else swprintf(t,24,L"%.0f%%",c.comp);textAt(dc,t,{trackR+12,r.top,r.right,r.bottom},boldFont,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
        return;}
    const wchar_t* pros[]={L"방향·거리감이 거의 그대로라 오래 들어도 편합니다.",L"작은 발소리와 다가오는 차량 소리가 또렷하게 들립니다. 총소리는 원래 크기보다 커지지 않습니다.",L"게임이 내는 소리를 그대로 듣습니다."};
    const wchar_t* cons[]={L"발소리가 확 커지지는 않습니다.",L"중저음이 조금 줄고, 시끄러운 교전 중에는 효과가 작습니다.",L"작은 발소리나 먼 차량 소리를 놓치기 쉽습니다."};
    for(int k=0;k<2;k++){RECT r={36+k*396,332,36+k*396+384,424};COLORREF c=k?warn:good;roundBox(dc,r,rowFill,line,14);
        // the mark is drawn as shapes, not a font glyph, so it sits in the exact middle of its disc
        RECT dot={r.left+16,r.top+14,r.left+40,r.top+38};COLORREF disc=k?RGB(64,48,22):RGB(20,58,44);painting::circle(dc,dot,disc,disc);
        RECT mark=dot;InflateRect(&mark,-4,-4);if(k)painting::exclaim(dc,mark,c);else painting::check(dc,mark,c);
        textAt(dc,i18n::text(k?L"주의할 점":L"장점"),{dot.right+12,dot.top,r.right-16,dot.bottom},smallBold,c,one);
        textAt(dc,i18n::text(k?cons[sel]:pros[sel]),{dot.right+12,dot.bottom+8,r.right-18,r.bottom-10},smallFont,mint,DT_LEFT|DT_TOP|DT_WORDBREAK);}}
// 미리 듣기: the radar (front is up, clockwise), the listener in the middle, the last sound as a dot that ripples while it
// plays and fades after; on the right the two play cards and where the sound came from.
void paintRadar(HDC dc){const UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE;
    roundBox(dc,radarBox,rowFill,line,16);paintTabs(dc);
    ULONGLONG now=GetTickCount64();double age=heardAt?(double)(now-heardAt):1e9;bool showDot=heardAt&&!shot.error&&age<shot.ms+1600.0;
    double fade=!showDot?0:age<shot.ms?1:1-(age-shot.ms)/1600;COLORREF tone=shot.kind?warn:good;
    if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        float cx=(float)rcx,cy=(float)rcy,R=(float)rr;
        {Gdiplus::GraphicsPath p;p.AddEllipse(cx-R,cy-R,R*2,R*2);Gdiplus::PathGradientBrush b(&p);b.SetCenterColor(Gdiplus::Color(255,24,42,86));Gdiplus::Color edge(255,11,19,42);int n=1;b.SetSurroundColors(&edge,&n);g.FillPath(&b,&p);}
        // the sweep, only while a sound plays: a fading wedge trailing the beam
        if(showDot){float beam=(float)std::fmod(age*.24,360.0)-90;for(int k=0;k<24;k++){Gdiplus::SolidBrush w(Gdiplus::Color((BYTE)(2+k*2*fade),96,150,255));g.FillPie(&w,cx-R,cy-R,R*2,R*2,beam-60+k*2.5f,2.6f);}}
        Gdiplus::Pen ring(Gdiplus::Color(255,36,58,104),1.2f),outer(Gdiplus::Color(255,58,88,150),1.6f),cross(Gdiplus::Color(110,58,88,150),1.f);
        for(float f:{1/3.f,2/3.f}){g.DrawEllipse(&ring,cx-R*f,cy-R*f,R*f*2,R*f*2);}g.DrawEllipse(&outer,cx-R,cy-R,R*2,R*2);
        g.DrawLine(&cross,cx,cy-R,cx,cy-30);g.DrawLine(&cross,cx,cy+30,cx,cy+R);g.DrawLine(&cross,cx-R,cy,cx-30,cy);g.DrawLine(&cross,cx+30,cy,cx+R,cy);
        for(int k=0;k<12;k++){double a=k*sbdsp::Pi/6;float s=(float)std::sin(a),c=(float)std::cos(a),in=k%3?R-5:R-9;g.DrawLine(&outer,cx+s*in,cy-c*in,cx+s*R,cy-c*R);}
        if(showDot&&shot.dist>0){double t=(shot.dist-sbprev::minDist(shot.kind))/(sbprev::maxDist(shot.kind)-sbprev::minDist(shot.kind));float rb=R*(float)(.44+.50*std::clamp(t,0.0,1.0));
            float x=cx+(float)std::sin(shot.az)*rb,y=cy-(float)std::cos(shot.az)*rb;BYTE r=GetRValue(tone),gg=GetGValue(tone),b=GetBValue(tone);
            Gdiplus::Pen path(Gdiplus::Color((BYTE)(70*fade),r,gg,b),1.4f);path.SetDashStyle(Gdiplus::DashStyleDash);g.DrawLine(&path,cx+(x-cx)*30/rb,cy+(y-cy)*30/rb,x,y);
            if(age<shot.ms)for(int k=0;k<3;k++){double q=std::fmod(age/900+k/3.0,1.0);float rad=7+(float)q*26;Gdiplus::Pen w(Gdiplus::Color((BYTE)(170*(1-q)),r,gg,b),2.f);g.DrawEllipse(&w,x-rad,y-rad,rad*2,rad*2);}
            Gdiplus::SolidBrush halo(Gdiplus::Color((BYTE)(70*fade),r,gg,b)),core(Gdiplus::Color((BYTE)(255*fade),r,gg,b)),hot(Gdiplus::Color((BYTE)(255*fade),255,255,255));
            g.FillEllipse(&halo,x-15,y-15,30.f,30.f);g.FillEllipse(&core,x-7,y-7,14.f,14.f);g.FillEllipse(&hot,x-2.5f,y-2.5f,5.f,5.f);}
        Gdiplus::SolidBrush hub(Gdiplus::Color(255,16,28,60));Gdiplus::Pen hubEdge(Gdiplus::Color(255,70,104,176),1.4f);g.FillEllipse(&hub,cx-26,cy-26,52.f,52.f);g.DrawEllipse(&hubEdge,cx-26,cy-26,52.f,52.f);}
    painting::headphones(dc,{rcx-15,rcy-15,rcx+15,rcy+15},RGB(255,255,255),2.4f);
    textAt(dc,i18n::text(L"앞"),{rcx-40,rcy-rr+10,rcx+40,rcy-rr+28},captionFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,i18n::text(L"뒤"),{rcx-40,rcy+rr-28,rcx+40,rcy+rr-10},captionFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,i18n::text(L"왼쪽"),{rcx-rr+12,rcy-9,rcx-30,rcy+9},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,i18n::text(L"오른쪽"),{rcx+30,rcy-9,rcx+rr-12,rcy+9},captionFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    // the right side: what this is, then one row per sound with its three distances (straight ahead), and what played
    const int L=344,Rt=800;textAt(dc,i18n::text(L"거리별 소리 테스트"),{L,190,Rt,216},boldFont,ink,one|DT_END_ELLIPSIS);
    textAt(dc,sel==Off?std::wstring(i18n::text(L"헤드셋을 쓰고 들어 보세요 · 처리하지 않은 원래 소리입니다")):fmt(L"헤드셋을 쓰고 들어 보세요 · %ls 프리셋으로 처리한 소리입니다",soundboost::presetName(sel)),{L,218,Rt,240},captionFont,muted,one|DT_END_ELLIPSIS);
    const wchar_t* names[]={L"발소리",L"총소리"};
    static const wchar_t* dirs[]={L"앞",L"앞 오른쪽",L"오른쪽",L"뒤 오른쪽",L"뒤",L"뒤 왼쪽",L"왼쪽",L"앞 왼쪽"};
    for(int k=0;k<2;k++){RECT r=testRow(k);COLORREF c=k?warn:good;roundBox(dc,r,RGB(14,24,52),line,16);
        RECT tile={r.left+12,r.top+12,r.left+52,r.top+52};
        if(k)painting::feature(dc,tile,4,c,rowFill);else{roundBox(dc,tile,painting::mixColor(rowFill,c,.2),painting::mixColor(rowFill,c,.45),14);RECT in=tile;InflateRect(&in,-6,-6);runner(dc,in,c);}
        textAt(dc,i18n::text(names[k]),{tile.right+12,r.top,r.left+164,r.bottom},boldFont,ink,one|DT_END_ELLIPSIS);
        for(int i=0;i<3;i++){int code=k*3+i;RECT q=testBtn(k,i);bool busy=playingCode(code),hov=hoverPlay==code;
            roundBox(dc,q,busy?painting::mixColor(rowFill,c,.2):hov?RGB(28,52,110):RGB(20,34,68),busy?c:hov?blue:RGB(40,62,110),12);
            if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float px=q.left+18.f,py=(q.top+q.bottom)/2.f;Gdiplus::PointF tri[]={{px-4,py-5.5f},{px+5.5f,py},{px-4,py+5.5f}};
                Gdiplus::SolidBrush b(busy||hov?Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c)):Gdiplus::Color(255,150,180,240));g.FillPolygon(&b,tri,3);}
            wchar_t t[16];swprintf(t,16,L"%dm",(int)sbprev::testDist(k,i));textAt(dc,t,{q.left+28,q.top,q.right-8,q.bottom},smallBold,busy||hov?ink:RGB(190,206,240),DT_LEFT|DT_VCENTER|DT_SINGLELINE);}}
    RECT res={L,398,Rt,422};
    if(shot.error){std::wstring t=shot.error==1?std::wstring(i18n::text(L"미리 듣기 소리를 받지 못했습니다. 인터넷 연결을 확인하고 다시 눌러 주세요.")):std::wstring(i18n::text(L"소리 파일을 읽지 못했습니다"));textAt(dc,t,res,smallBold,danger,one|DT_END_ELLIPSIS);}
    else if(heardAt&&shot.dist>0){RECT d={L,res.top+6,L+12,res.top+18};painting::circle(dc,d,tone,tone);
        textAt(dc,fmt(L"%ls · %ls · %dm",i18n::text(names[shot.kind]),i18n::text(dirs[(int)std::lround(shot.az/(2*sbdsp::Pi)*8)%8]),(int)shot.dist),{L+22,res.top,Rt,res.bottom},smallBold,ink,one|DT_END_ELLIPSIS);}
    else textAt(dc,i18n::text(L"거리를 누르면 무작위 방향에서 그만큼 떨어진 소리가 들립니다. 멀수록 작고 먹먹해집니다."),res,smallFont,muted,one|DT_END_ELLIPSIS);
    paintNote(dc,(L+Rt)/2,146,Rt-L);}
void paint(HDC dc){const UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE;for(auto& r:infoIcons)r={};
    textAt(dc,i18n::text(L"프리셋"),{36,18,300,48},sectionFont,ink,one);infoMark(dc,3,36+textWidth(dc,sectionFont,i18n::text(L"프리셋"))+10,33);
    {std::wstring dev,why=soundboost::info(dev);int st=soundboost::state;std::wstring s=soundboost::stateText();
     if(st==soundboost::Applied&&!dev.empty()){s+=L" · "+dev;}COLORREF sc=st==soundboost::Applied?good:st==soundboost::Failed?danger:muted;
     int w=std::min(textWidth(dc,smallFont,s.c_str()),440);RECT t={816-w,22,816,46};textAt(dc,s,t,smallFont,sc,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
     RECT d={t.left-16,30,t.left-8,38};painting::circle(dc,d,sc,sc);}
    const wchar_t icons[]={0xE767,0xE805,0xE74F,0xE9E9};const wchar_t* says[]={L"원래 소리에 가깝게",L"발소리를 또렷하게",L"게임 원래 소리",L"그래프로 직접 조절"};
    for(int k=0;k<4;k++){int i=cardPreset[k];RECT r=cardRect(k);bool on=i==sel,hov=i==hoverCard&&!on;roundBox(dc,r,on?tint:hov?RGB(20,34,68):rowFill,on?blue:hov?RGB(58,84,140):line,16);
        RECT tile={r.left+12,r.top+18,r.left+44,r.top+50};roundBox(dc,tile,on?blue:RGB(24,40,78),on?blue:RGB(36,58,104),10);if(i==1){RECT in=tile;InflateRect(&in,-4,-4);runner(dc,in,on?RGB(255,255,255):RGB(140,176,255));}else glyph(dc,icons[i],tile,iconFont,on?RGB(255,255,255):RGB(140,176,255));
        textAt(dc,cardName(i),{tile.right+10,r.top+12,r.right-12,r.top+36},boldFont,on||hov?ink:mint,one|DT_END_ELLIPSIS);
        textAt(dc,i18n::text(says[i]),{tile.right+10,r.top+37,r.right-12,r.top+57},captionFont,on?RGB(170,196,255):muted,one|DT_END_ELLIPSIS);
        if(i==1){int w=textWidth(dc,captionFont,i18n::text(L"추천"))+20;RECT pill={r.right-16-w,r.top-10,r.right-16,r.top+10};roundBox(dc,pill,good,good,10);textAt(dc,i18n::text(L"추천"),pill,captionFont,RGB(6,30,20),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
    if(view)paintRadar(dc);else{paintGraph(dc);paintBelow(dc);}
    std::wstring dev,why=soundboost::info(dev);bool failed=soundboost::state==soundboost::Failed;
    // Windows' own 음향 강도 이퀄라이제이션 as one clean row: the Windows mark, the name with a "Windows 기본 기능" pill and a !,
    // one line saying it is switched right here; the switch control sits on the card's right
    {RECT card=enhCard;roundBox(dc,card,rowFill,line,16);RECT tile={card.left+16,card.top+12,card.left+56,card.top+52};
     roundBox(dc,tile,RGB(16,30,64),RGB(36,58,104),12);
     int cx=(tile.left+tile.right)/2,cy=(tile.top+tile.bottom)/2;COLORREF w=RGB(110,170,255); /* the Windows mark: four tiles */
     for(int k=0;k<4;k++){int x=k%2?cx+1:cx-8,y=k/2?cy+1:cy-8;roundBox(dc,{x,y,x+7,y+7},w,w,2);}
     int tx=tile.right+16;const wchar_t* name=i18n::text(L"음향 강도 이퀄라이제이션");int nw=std::min(textWidth(dc,boldFont,name),280);
     textAt(dc,name,{tx,card.top+10,tx+nw,card.top+34},boldFont,ink,one|DT_END_ELLIPSIS);
     const wchar_t* tag=i18n::text(L"Windows 기본 기능");int pw=textWidth(dc,captionFont,tag)+20;RECT pill={tx+nw+10,card.top+12,tx+nw+10+pw,card.top+32};
     roundBox(dc,pill,RGB(22,40,84),RGB(52,84,150),10);textAt(dc,tag,pill,captionFont,RGB(150,190,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);infoMark(dc,2,pill.right+8,(pill.top+pill.bottom)/2);
     textAt(dc,i18n::text(L"여기서 바로 켜고 끌 수 있습니다 · 큰 소리와 작은 소리를 고르게 맞춥니다"),{tx,card.top+36,card.right-100,card.top+56},smallFont,muted,one|DT_END_ELLIPSIS);}
    if(failed)textAt(dc,why+L" · "+i18n::text(L"프리셋을 다시 고르거나 배그를 다시 켜면 다시 시도합니다."),infoRect,captionFont,danger,DT_LEFT|DT_TOP|DT_WORDBREAK);
    paintTip(dc);}
// 음향 강도 이퀄라이제이션 is read and written on a worker thread (the Settings call can take a moment); the answer comes back here.
void enh(HWND h,int want){std::thread([h,want]{HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);bool v=want==1;int r=soundboost::loudness(want<0?nullptr:&v);if(SUCCEEDED(co))CoUninitialize();PostMessageW(h,WM_APP+211,(WPARAM)(r+1),want);}).detach();}
}
LRESULT CALLBACK soundPageProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){using namespace soundpage;
    switch(msg){
    case WM_CREATE:page=h;sel=soundboost::on()?soundboost::presetSaved():Off;details=hoverLink=false;drag=hoverCard=hoverPoint=hoverTab=hoverPlay=hoverInfo=waiting=-1;view=0;heardAt=0;shot={};note.clear();
#ifdef BLUEASH_TT
        if(_wgetenv(L"BLUEASH_SOUND_VIEW"))view=1; /* test build: open on the distance test (its tab moves with the label's length) */
#endif
        {HWND sw=pageSwitch(h,i18n::text(L"음향 강도 이퀄라이제이션"),enhCard.right-76,enhCard.top+14,56,36,75);SetPropW(sw,L"BlueAsh.OnRow",(HANDLE)1);} /* the switch only (56 wide); the card draws the name */enhKnown=false;enh(h,-1);
        pageButton(h,i18n::text(L"기본값으로"),36,556,140,78);pageButton(h,i18n::text(L"닫기"),696,556,120,180);buttons();
        SetTimer(h,1,500,nullptr);soundboost::changed=[]{if(pageWindow&&mainPage==19)InvalidateRect(pageWindow,nullptr,FALSE);};return 0;
    case WM_APP+211:{int r=(int)wp-1;if(r>=0)checkBox(h,75,r==1);
        if(lp>=0){bool ok=r>=0&&r==(int)lp;tell(!ok?std::wstring(i18n::text(L"음향 강도 이퀄라이제이션을 바꾸지 못했습니다")):std::wstring(i18n::text(r?L"음향 강도 이퀄라이제이션을 켰습니다":L"음향 강도 이퀄라이제이션을 껐습니다")),ok?good:danger);}
        enhKnown=r>=0;return 0;}
    case WM_APP+212:{auto* r=(sbprev::Result*)lp;if(r->error==3){delete r;return 0;}shot=*r;delete r;waiting=-1;heardAt=GetTickCount64();if(!shot.error)SetTimer(h,3,16,nullptr);
        if(shot.error&&!view)tell(std::wstring(i18n::text(shot.error==1?L"미리 듣기 소리를 받지 못했습니다. 인터넷 연결을 확인하고 다시 눌러 주세요.":L"소리 파일을 읽지 못했습니다")),danger);
        InvalidateRect(h,view?&radarBox:&graph,FALSE);return 0;}
    case WM_TIMER:if(wp==2){KillTimer(h,2);note.clear();InvalidateRect(h,view?&radarBox:&graph,FALSE);return 0;}
        if(wp==3){if(!radarLive())KillTimer(h,3);if(view){InvalidateRect(h,&disc,FALSE);RECT rows={344,252,800,424};InvalidateRect(h,&rows,FALSE);}else{RECT top={graph.left,graph.top,graph.right,graph.top+44};InvalidateRect(h,&top,FALSE);}return 0;}
        {RECT st={300,18,816,50},info=infoRect;InvalidateRect(h,&st,FALSE);InvalidateRect(h,&info,FALSE);}return 0;
    case WM_MOUSEMOVE:{int x=(short)LOWORD(lp),y=(short)HIWORD(lp);if(drag>=0){dragTo(x,y);return 0;}
        int c=cardAt(x,y),p=pointAt(x,y),tb=tabAt(x,y),k=playAt(x,y),in=infoAt(x,y);bool l=onLink(x,y);if(c!=hoverCard||p!=hoverPoint||l!=hoverLink||tb!=hoverTab||k!=hoverPlay||in!=hoverInfo){hoverCard=c;hoverPoint=p;hoverLink=l;hoverTab=tb;hoverPlay=k;hoverInfo=in;InvalidateRect(h,nullptr,FALSE);}
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_MOUSELEAVE:if(hoverCard>=0||hoverPoint>=0||hoverLink||hoverTab>=0||hoverPlay>=0||hoverInfo>=0){hoverCard=hoverPoint=hoverTab=hoverPlay=hoverInfo=-1;hoverLink=false;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_SETCURSOR:if((hoverCard>=0&&hoverCard!=sel)||hoverLink||(hoverTab>=0&&hoverTab!=view)||hoverPlay>=0){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}if(hoverPoint>=0||drag>=0){SetCursor(LoadCursorW(nullptr,IDC_SIZENS));return TRUE;}break;
    case WM_LBUTTONDOWN:{int x=(short)LOWORD(lp),y=(short)HIWORD(lp);int c=cardAt(x,y);if(c>=0){choose(c);return 0;}
        int t=tabAt(x,y);if(t>=0&&t!=view){view=t;details=false;buttons();InvalidateRect(h,nullptr,FALSE);return 0;}
        int code=playAt(x,y);if(code>=0){int k=code>=10?code-10:code/3;double dist=code>=10?0:sbprev::testDist(k,code%3);std::wstring dir=soundboost::rec::samples()+soundboost::rec::subName(k);
#ifdef BLUEASH_TT
            if(_wgetenv(L"BLUEASH_NO_SAMPLES"))dir=L"?"; /* test build: act like a PC without the developer's recordings */
#endif
            auto shipped=[k]{ /* the data pack's own folder (downloaded at start; it reaches the runtime folder only when a helper starts) */
                if(!datapack::ready()){datapack::fetch();}return datapack::ready()?datapack::cacheDir()+(k?L"\\sound\\gunshot":L"\\sound\\footstep"):std::wstring();};
            if(sbprev::play(h,k,dir,shipped,sel==Off?-1:sel,sbdsp::customPreset(soundboost::custom()),dist)){waiting=code;InvalidateRect(h,view?&radarBox:&graph,FALSE);}return 0;}
        if(view)return 0;
        if(onLink(x,y)){details=!details;InvalidateRect(h,&graph,FALSE);return 0;}
        int p=pointAt(x,y);if(p>=0){drag=p;SetCapture(h);dragTo(x,y);return 0;}
        if(sel==3)for(int k=0;k<2;k++){RECT r=sliderRow(k);if(y>=r.top&&y<r.bottom&&x>=trackL-12&&x<=trackR+12){drag=6+k;SetCapture(h);dragTo(x,y);return 0;}}
        return 0;}
    case WM_LBUTTONUP:if(drag>=0){drag=-1;ReleaseCapture();soundboost::setCustom(soundboost::custom(),true);
            tell(live()?i18n::text(L"나만의 프리셋을 저장했습니다 · 배그에 바로 적용됐습니다"):i18n::text(L"나만의 프리셋을 저장했습니다 · 배그를 켜면 적용됩니다"));InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_CAPTURECHANGED:if(drag>=0){drag=-1;soundboost::setCustom(soundboost::custom(),true);InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_COMMAND:{int id=LOWORD(wp);if(HIWORD(wp)!=BN_CLICKED)return 0;
        if(id==78){soundboost::setCustom(sbdsp::customFrom(sbdsp::preset(0)),true);tell(i18n::text(L"나만의 프리셋을 기본값으로 되돌렸습니다"));InvalidateRect(h,nullptr,FALSE);}
        else if(id==75&&enhKnown){bool v=!checked(h,75);checkBox(h,75,v);enhKnown=false;enh(h,v?1:0);}
        else if(id==180)DestroyWindow(h);
        return 0;}
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,std::max(1L,c.right),std::max(1L,c.bottom));auto old=SelectObject(mem,bmp);
        eraseCarbon(h,mem);paint(mem);BitBlt(dc,0,0,c.right,c.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1; // WM_PAINT draws the whole page off-screen
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_DESTROY:KillTimer(h,1);KillTimer(h,2);KillTimer(h,3);sbprev::stop();soundboost::changed=nullptr;page=nullptr;return 0;
    case WM_CLOSE:DestroyWindow(h);return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
