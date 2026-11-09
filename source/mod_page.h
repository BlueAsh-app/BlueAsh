#pragma once
// 모드 번역 (page 16, the third card on 게임 도우미): the PC games with mods (store records, as on the profile list),
// a strip of game cards, the chosen game's mods in a scrolling list with what each would get, and 모드 자동번역 /
// 원본으로 되돌리기, with ‹ 게임 도우미 at the bottom left. Finding mods, translating and restoring run on threads (mod_translate.h); the page only draws what they
// left in the globals below. Everything is drawn here (no static labels) into one off-screen picture per paint.
#include <mutex>
#include <atomic>
#include "mod_translate.h"
#include "local_ai.h"
namespace modui {
struct Row {dlsslib::Game g;std::vector<modtr::Mod> mods;int ready=0,lines=0,done=0,native=0;};
std::vector<Row> rows;bool scanning=false,busy=false;HWND page=nullptr;
std::mutex lock;std::vector<Row>* pending=nullptr; // a finished scan waiting for the page
int pick=0,gameScroll=0,modScroll=0,hoverGame=-1,hoverMod=-1;bool barHover=false,barDrag=false;int barGrab=0;double shownK=0;
// While translating: 0 gathering, 1 fetching the AI translator (MB), 2 starting it, 3 translating (lines).
std::atomic<int> phase{0},dlDone{0},dlTotal{0},doneN{0},totalN{0};std::atomic<bool> needDl{false};std::wstring note;COLORREF noteColor=0;
const RECT refreshRect={782,20,816,56},strip={36,60,816,144},list={36,194,816,544};const int cardW=246,cardGap=21,rowH=46;
std::wstring store(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+dataSub();CreateDirectoryW(r.c_str(),nullptr);r+=L"\\mod-translate";CreateDirectoryW(r.c_str(),nullptr);return r;}
int target(){return modtr::uiIndex(i18n::languageCode());}
void count(Row& r,const std::vector<modtr::Record>& recs){r.ready=r.lines=r.done=r.native=0;std::wstring lang=modtr::langs()[std::max(0,target())].ui;
    for(auto& m:r.mods){m.done=modtr::translatedFiles(m,recs,lang);if(m.strings){r.ready++;r.lines+=m.strings;}if(m.done)r.done++;if(m.native)r.native++;}}
// The games and their mods, on a thread. Games without mods are left out; those with something to translate come first.
void scan(){if(scanning)return;scanning=true;std::wstring* folders=new std::wstring(regString(configPath,L"DlssFolders"));
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);std::unique_ptr<std::wstring> f((std::wstring*)p);
        auto* out=new std::vector<Row>;int tg=target();auto recs=modtr::records(store());auto fin=modtr::finishedIn(recs,tg>0?modtr::langs()[tg].ui:L"");
        for(auto& g:dlsslib::scan(*f,false)){if(g.v.status==dlss::Status::Blocked){continue;}Row r;r.g=g;r.mods=modtr::find(g.e.dir);if(r.mods.empty()){continue;}
            for(auto& m:r.mods){modtr::plan(m,tg,fin);}count(r,recs);
            auto rank=[](const modtr::Mod& m){return m.strings?0:m.done?1:m.native?2:3;};std::stable_sort(r.mods.begin(),r.mods.end(),[&](const modtr::Mod& a,const modtr::Mod& b){return rank(a)<rank(b);}); // what can be translated first
            out->push_back(std::move(r));}
        std::stable_sort(out->begin(),out->end(),[](const Row& a,const Row& b){if((a.ready>0)!=(b.ready>0)){return a.ready>0;}return _wcsicmp(a.g.e.name.c_str(),b.g.e.name.c_str())<0;});
        {std::lock_guard<std::mutex> g(lock);delete pending;pending=out;}if(page)PostMessageW(page,WM_APP+202,0,0);return 0;},folders,0,nullptr);
    if(t)CloseHandle(t);else{delete folders;scanning=false;}}
Row* chosen(){return pick>=0&&pick<(int)rows.size()?&rows[pick]:nullptr;}
int cardsWidth(){return (int)rows.size()*(cardW+cardGap)-cardGap;}
int listHeight(){Row* r=chosen();return r?(int)r->mods.size()*rowH:0;}
void clampScroll(){int maxG=std::max(0,cardsWidth()-(int)(strip.right-strip.left));gameScroll=std::clamp(gameScroll,0,maxG);int maxM=std::max(0,listHeight()-(int)(list.bottom-list.top-12));modScroll=std::clamp(modScroll,0,maxM);}
void buttons(){Row* r=chosen();EnableWindow(GetDlgItem(page,261),!busy&&!scanning&&r&&r->ready>0&&target()>0);EnableWindow(GetDlgItem(page,260),!busy&&r&&r->done>0);}
void setNote(const std::wstring& s,COLORREF c){note=s;noteColor=c;if(page)InvalidateRect(page,nullptr,FALSE);}
std::wstring fmt(const wchar_t* key,int a,int b=0){wchar_t s[512];swprintf(s,512,i18n::text(key),a,b);return s;}
std::wstring gameName(const Row& r){return r.g.e.name.empty()?r.g.e.dir.substr(r.g.e.dir.find_last_of(L'\\')+1):r.g.e.name;}
// 다시 찾기: the icon by the game count turns once on a click (0.7 s, eased) and keeps turning while the scan runs.
bool refreshHover=false;ULONGLONG spinAt=0;
double spinAngle(){if(!spinAt){return 0;}double t=(GetTickCount64()-spinAt)/700.0;if(t<1){double u=1-t;return 360*(1-u*u*u);}return scanning?360*(t-1-(int)(t-1)):0;}
void rescan(){if(busy||scanning||!page)return;spinAt=GetTickCount64();note.clear();scan();SetTimer(page,5,15,nullptr);SetTimer(page,4,40,nullptr);}
void repaint(){if(page)PostMessageW(page,WM_APP+203,0,0);}
// On a thread: the game's own words (The Witcher 3), then the AI translator started on the first batch that needs it
// (fetched first if missing) and stopped at the end. A batch whose answer is not a matching array goes line by line.
void translate(){Row* r=chosen();if(!r||busy)return;busy=true;phase=0;doneN=0;totalN=0;shownK=0;needDl=localai::fileSize(localai::modelPath())!=localai::modelSize;setNote(L"",0);buttons();SetTimer(page,4,16,nullptr);
    struct Job {std::vector<modtr::Mod> mods;int tg;std::wstring game,dir;};auto* job=new Job{r->mods,target(),gameName(*r),r->g.e.dir};
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<Job> j((Job*)p);const std::wstring ui=modtr::langs()[j->tg].ui;
        auto dict=modtr::gameWords(j->dir,modtr::langs()[j->tg].w3);localai::Server s;int failed=0;bool started=false;
        auto send=[&](const std::vector<std::wstring>& in,std::vector<std::wstring>& out)->int{
            if(!started){started=true;phase=1;failed=localai::ensure([](ULONGLONG b,ULONGLONG t){dlDone=(int)(b>>20);dlTotal=(int)(t>>20);repaint();});
                if(!failed){phase=2;repaint();failed=localai::start(s);}if(!failed){phase=3;}repaint();}
            if(failed){return failed;}
            std::wstring task=modtr::instructions(j->game,ui,modtr::wordsIn(dict,in));
            if(modtr::arrayOf(localai::chat(s,task,modtr::jsonArray(in)),in.size(),out)){return 0;}
            out.assign(in.size(),L"");bool any=false;
            for(size_t i=0;i<in.size();i++){std::vector<std::wstring> one;std::wstring a=localai::chat(s,task,modtr::jsonArray({in[i]}));if(!a.empty()){any=true;}if(modtr::arrayOf(a,1,one)){out[i]=one[0];}}
            return any?0:(int)localai::Start;};
        auto o=modtr::translate(j->mods,j->tg,store(),send,[](int d,int n){doneN=d;totalN=n;repaint();},&dict);localai::stop(s);
        auto* res=new modtr::Outcome(o);if(!page||!PostMessageW(page,WM_APP+204,0,(LPARAM)res)){delete res;busy=false;}return 0;},job,0,nullptr);
    if(t)CloseHandle(t);else{delete job;busy=false;buttons();}}
void restore(){Row* r=chosen();if(!r||busy)return;
    if(MessageBoxW(win,i18n::text(L"이 게임에서 번역한 모드 파일을 원래대로 되돌릴까요?"),i18n::text(L"모드 번역"),MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    busy=true;buttons();auto* mods=new std::vector<modtr::Mod>(r->mods);
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<std::vector<modtr::Mod>> m((std::vector<modtr::Mod>*)p);auto back=modtr::restoreAll(*m,store());
        if(!page||!PostMessageW(page,WM_APP+205,(WPARAM)back.first,(LPARAM)back.second)){busy=false;}return 0;},mods,0,nullptr);
    if(t)CloseHandle(t);else{delete mods;busy=false;buttons();}}
// One percentage for the whole run (fetching the translator, if needed, is its first 30 %, starting it the next 2 %, the
// lines the rest).
double progress(){int base=needDl?32:2;double k=0;
    if(phase==1)k=dlTotal>0?.30*dlDone/dlTotal:0;else if(phase==2)k=(base-2)/100.0;else if(phase==3)k=(base+(100-base)*(totalN>0?(double)doneN/totalN:1.0))/100.0;return std::clamp(k,0.0,1.0);}
// While translating, 모드 자동번역 itself is the progress bar: a cobalt fill growing from the left with a slow light sweep,
// and the percentage in the middle. Drawn over the button's own picture off screen, then copied in one go.
void drawProgress(DRAWITEMSTRUCT* ds){RECT r=ds->rcItem;int w=r.right-r.left,hh=r.bottom-r.top;if(w<=0||hh<=0)return;
    HDC mem=CreateCompatibleDC(ds->hDC);HBITMAP bmp=CreateCompatibleBitmap(ds->hDC,r.right,r.bottom);auto old=SelectObject(mem,bmp);DRAWITEMSTRUCT copy=*ds;copy.hDC=mem;drawButtonTo(&copy);
    if(painting::token){Gdiplus::Graphics g(mem);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);Gdiplus::GraphicsPath p;painting::path(p,r,14);
        Gdiplus::SolidBrush track(painting::color(RGB(16,27,58)));g.FillPath(&track,&p);g.SetClip(&p);
        float fw=(float)(w*shownK);if(fw>1){Gdiplus::RectF fill((float)r.left,(float)r.top,fw,(float)hh);
            Gdiplus::LinearGradientBrush b(Gdiplus::PointF((float)r.left-1,0),Gdiplus::PointF((float)r.left+std::max(fw,2.f)+1,0),painting::color(RGB(30,82,226)),painting::color(RGB(74,140,255)));g.FillRectangle(&b,fill);
            // the sweep: a soft light band crossing the filled part every 1.6 s
            float t=(GetTickCount64()%1600)/1600.f,cx=r.left-70+(fw+140)*t;Gdiplus::RectF band(cx-70,(float)r.top,140,(float)hh);
            Gdiplus::LinearGradientBrush sh(Gdiplus::PointF(cx-71,0),Gdiplus::PointF(cx+71,0),Gdiplus::Color(0,255,255,255),Gdiplus::Color(0,255,255,255));Gdiplus::Color cs[3]={Gdiplus::Color(0,255,255,255),Gdiplus::Color(46,255,255,255),Gdiplus::Color(0,255,255,255)};Gdiplus::REAL ps[3]={0,.5f,1};sh.SetInterpolationColors(cs,ps,3);
            Gdiplus::Region keep(fill);g.SetClip(&keep,Gdiplus::CombineModeIntersect);g.FillRectangle(&sh,band);
            // a bright edge where the fill ends
            Gdiplus::SolidBrush edge(Gdiplus::Color(150,170,205,255));if(shownK<.995)g.FillRectangle(&edge,(float)r.left+fw-2,(float)r.top,2.f,(float)hh);}
        g.ResetClip();Gdiplus::Pen pen(painting::color(RGB(58,96,190)),1.f);g.DrawPath(&pen,&p);}
    textAt(mem,std::to_wstring((int)std::lround(shownK*100))+L"%",r,boldFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    BitBlt(ds->hDC,r.left,r.top,w,hh,mem,r.left,r.top,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);}
// The mod list's scroll bar (design coordinates): where its thumb is, and the scroll for a thumb top.
bool barRect(RECT& t){int total=listHeight(),view=list.bottom-list.top-12;if(total<=view)return false;int bar=std::max(40,view*view/total),y=list.top+6+(view-bar)*modScroll/std::max(1,total-view);t={list.right-16,y,list.right-1,y+bar};return true;}
void scrollToBar(int top){int total=listHeight(),view=list.bottom-list.top-12,bar=std::max(40,view*view/total);modScroll=(int)std::lround(std::clamp((top-list.top-6)/(double)std::max(1,view-bar),0.0,1.0)*(total-view));clampScroll();}
RECT cardRect(int i){int x=strip.left+i*(cardW+cardGap)-gameScroll;return {x,strip.top,x+cardW,strip.bottom};}
int gameAt(int x,int y){if(y<strip.top||y>=strip.bottom||x<strip.left||x>=strip.right){return -1;}for(int i=0;i<(int)rows.size();i++){RECT c=cardRect(i);if(x>=c.left&&x<c.right){return i;}}return -1;}
int modAt(int x,int y){Row* r=chosen();if(!r||x<list.left||x>=list.right||y<list.top+6||y>=list.bottom-6){return -1;}int i=(y-list.top-6+modScroll)/rowH;return i<(int)r->mods.size()?i:-1;}
// Clipped to a design rectangle (stretched like the rest when the window is large).
struct Clip {HDC dc;Clip(HDC d,RECT r):dc(d){if(painting::stretch.on)r=painting::stretch.box(r);SaveDC(dc);IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);}~Clip(){RestoreDC(dc,-1);}};
void paint(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;int tg=target();
    textAt(dc,i18n::text(L"게임"),{36,24,400,52},sectionFont,ink);
    if(!rows.empty())textAt(dc,fmt(L"모드가 있는 게임 %d개",(int)rows.size()),{400,24,774,52},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    {RECT b=refreshRect;bool live=spinAt||scanning;if(refreshHover&&!busy)roundBox(dc,b,tint,tint,10);COLORREF ic=busy?RGB(70,86,120):live?blue:refreshHover?ink:muted;double deg=spinAngle();
        if(deg!=0){RECT s=painting::stretch.on?painting::stretch.box(b):b;double a=deg*3.14159265358979/180,c=std::cos(a),n=std::sin(a),cx=(s.left+s.right)/2.0,cy=(s.top+s.bottom)/2.0;int mode=SetGraphicsMode(dc,GM_ADVANCED);XFORM keep={};GetWorldTransform(dc,&keep); /* keep the display scale (dpi.h): rotate inside it */
            XFORM x={(FLOAT)c,(FLOAT)n,(FLOAT)-n,(FLOAT)c,(FLOAT)(cx-cx*c+cy*n),(FLOAT)(cy-cx*n-cy*c)};ModifyWorldTransform(dc,&x,MWT_LEFTMULTIPLY);glyph(dc,0xE72C,b,iconFont,ic);SetWorldTransform(dc,&keep);SetGraphicsMode(dc,mode);}
        else glyph(dc,0xE72C,b,iconFont,ic);}
    if(rows.empty()){if(scanning){for(int i=0;i<3;i++)painting::skeleton(dc,{36+i*(cardW+cardGap),strip.top,36+i*(cardW+cardGap)+cardW,strip.bottom},18,36,816,rowFill,RGB(36,54,96));}
        else{roundBox(dc,strip,rowFill,line,18);textAt(dc,i18n::text(L"모드가 설치된 게임을 찾지 못했습니다."),strip,font,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
    {Clip c(dc,strip);for(int i=0;i<(int)rows.size();i++){RECT r=cardRect(i);if(r.right<strip.left||r.left>strip.right){continue;}bool on=i==pick,hv=i==hoverGame;
        roundBox(dc,r,on?tint:rowFill,on?blue:hv?RGB(64,104,196):line,18);RECT art={r.left+12,r.top+10,r.left+52,r.bottom-10};
        if(!painting::art(dc,art,dlssui::coverFor(rows[i].g),10,.3f)){roundBox(dc,art,well,well,10);RECT pad=art;InflateRect(&pad,-6,-16);painting::gamepad(dc,pad,muted,1.6f);}
        std::wstring name=rows[i].g.e.name.empty()?rows[i].g.e.dir.substr(rows[i].g.e.dir.find_last_of(L'\\')+1):rows[i].g.e.name;
        textAt(dc,name,{r.left+64,r.top+14,r.right-12,r.top+40},boldFont,ink,one);
        textAt(dc,rows[i].ready?fmt(L"모드 %d개 · 번역할 모드 %d개",(int)rows[i].mods.size(),rows[i].ready):fmt(L"모드 %d개",(int)rows[i].mods.size()),{r.left+64,r.top+44,r.right-12,r.top+68},smallFont,rows[i].ready?RGB(140,176,255):muted,one);}}
    // ‹ › at the strip's ends when the cards run past it
    if(gameScroll>0){RECT a={strip.left-2,strip.top+26,strip.left+30,strip.top+58};painting::circle(dc,a,RGB(20,30,58),line);glyph(dc,0xE76B,a,iconFont,ink);}
    if(gameScroll<cardsWidth()-(int)(strip.right-strip.left)){RECT a={strip.right-30,strip.top+26,strip.right+2,strip.top+58};painting::circle(dc,a,RGB(20,30,58),line);glyph(dc,0xE76C,a,iconFont,ink);}
    Row* r=chosen();
    textAt(dc,i18n::text(L"모드"),{36,158,400,186},sectionFont,ink);
    if(r)textAt(dc,r->lines?fmt(L"번역할 문장 %d개",r->lines):fmt(L"모드 %d개",(int)r->mods.size()),{400,158,816,186},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    roundBox(dc,list,rowFill,line,16);
    if(r){Clip c(dc,{list.left+1,list.top+6,list.right-1,list.bottom-6});
        for(int i=0;i<(int)r->mods.size();i++){int y=list.top+6+i*rowH-modScroll;if(y+rowH<list.top||y>list.bottom){continue;}auto& m=r->mods[i];RECT row={list.left+8,y,list.right-8,y+rowH};
            if(i==hoverMod)roundBox(dc,{row.left,row.top+3,row.right,row.bottom-3},RGB(26,42,80),RGB(26,42,80),10);
            static HBRUSH sep=CreateSolidBrush(RGB(30,46,84));if(i)painting::hairline(dc,{row.left+10,row.top,row.right-10,row.top+1},sep);
            std::wstring st;COLORREF sc=muted;if(m.strings&&tg>0){st=fmt(L"번역할 문장 %d개",m.strings);sc=RGB(140,176,255);}else if(m.done){st=L"\x2713 "+std::wstring(i18n::text(L"번역됨"));sc=RGB(96,210,150);}
            else if(m.native)st=i18n::text(L"이미 이 언어 파일이 있음");else st=i18n::text(L"번역할 글 없음");
            textAt(dc,m.name,{row.left+14,row.top,row.right-230,row.bottom},font,ink,one);textAt(dc,st,{row.right-224,row.top,row.right-14,row.bottom},smallFont,sc,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        RECT t;if(barRect(t)){bool on=barHover||barDrag;COLORREF c=on?RGB(96,124,186):RGB(60,80,124);roundBox(dc,{list.right-(on?11:8),t.top,list.right-4,t.bottom},c,c,6);}}
    else if(scanning)for(int i=0;i<5;i++)painting::skeleton(dc,{list.left+14,list.top+16+i*rowH,list.right-14,list.top+16+i*rowH+28},10,list.left,list.right,RGB(24,38,72),RGB(40,60,104));
    // the outcome (while working, the progress is in 모드 자동번역)
    if(!busy){std::wstring n=note;COLORREF c=noteColor?noteColor:muted;if(n.empty()){c=muted;n=tg==0?i18n::text(L"영어로 쓰는 중이라 번역할 필요가 없습니다."):localai::fileSize(localai::modelPath())==localai::modelSize?L"":i18n::text(L"처음 한 번 약 2.5GB를 받습니다.");}
        textAt(dc,n,{232,550,424,600},smallFont,c,DT_LEFT|DT_VCENTER|DT_WORDBREAK|DT_END_ELLIPSIS);}
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:page=h;note.clear();noteColor=0;hoverGame=hoverMod=-1;refreshHover=false;spinAt=0;
        pageButton(h,i18n::text(L"‹  게임 도우미"),36,556,180,262);pageButton(h,i18n::text(L"원본으로 되돌리기"),432,556,172,260);pageButton(h,i18n::text(L"모드 자동번역"),616,556,200,261,true);
        {std::lock_guard<std::mutex> g(lock);if(pending){rows=std::move(*pending);delete pending;pending=nullptr;scanning=false;}}
        clampScroll();buttons();scan();if(scanning)SetTimer(h,4,40,nullptr);buttons();
#ifdef BLUEASH_TT
        if(const char* k=getenv("BLUEASH_FAKE_PROGRESS")){busy=true;phase=3;totalN=100;doneN=atoi(k);shownK=atoi(k)/100.0;buttons();SetTimer(h,4,16,nullptr);}
#endif
        return 0;
    case WM_APP+202:{std::vector<Row>* got=nullptr;{std::lock_guard<std::mutex> g(lock);got=pending;pending=nullptr;}scanning=false;
        if(got){std::wstring was=chosen()?chosen()->g.e.dir:L"";rows=std::move(*got);delete got;pick=0;for(int i=0;i<(int)rows.size();i++)if(rows[i].g.e.dir==was)pick=i;}
        clampScroll();buttons();InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_APP+203:InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_APP+204:{std::unique_ptr<modtr::Outcome> o((modtr::Outcome*)lp);busy=false;std::wstring s;COLORREF c=RGB(96,210,150);
        if(o->status){std::wstring why=o->status==localai::Disk?i18n::text(L"디스크 공간이 부족합니다. AI 번역기에 약 3GB가 필요합니다."):o->status==localai::Network||o->status==localai::Mismatch?i18n::text(L"AI 번역기를 받지 못했습니다. 인터넷 연결을 확인하고 다시 시도해 주세요."):i18n::text(L"AI 번역기를 시작하지 못했습니다.");
            s=o->lines?fmt(L"번역이 중간에 멈췄습니다 · 모드 %d개만 번역했습니다",o->mods)+L" · "+why:why;c=RGB(240,120,120);}
        else if(o->lines)s=fmt(L"한국어로 번역되었습니다 · 모드 %d개, 문장 %d개",o->mods,o->lines);
        else{s=i18n::text(L"번역할 글이 없습니다.");c=muted;}
        if(o->failed){s+=L" · "+fmt(L"파일 %d개를 쓰지 못했습니다. 게임을 끄고 다시 시도해 주세요.",o->failed);c=RGB(240,180,90);}
        setNote(s,c);scan();if(scanning)SetTimer(h,4,40,nullptr);buttons();return 0;}
    case WM_APP+205:busy=false;setNote(fmt(L"원래대로 되돌렸습니다 · 파일 %d개",(int)wp)+(lp?L" · "+fmt(L"모드가 바뀐 파일 %d개는 그대로 두었습니다.",(int)lp):L""),muted);scan();buttons();return 0;
    case WM_MOUSEMOVE:{int x=(short)LOWORD(lp),y=(short)HIWORD(lp);if(barDrag){scrollToBar(y-barGrab);InvalidateRect(h,nullptr,FALSE);return 0;}
        {RECT t;bool on=barRect(t)&&x>=t.left&&x<t.right&&y>=t.top&&y<t.bottom;if(on!=barHover){barHover=on;InvalidateRect(h,nullptr,FALSE);}}
        {bool on=x>=refreshRect.left&&x<refreshRect.right&&y>=refreshRect.top&&y<refreshRect.bottom;if(on!=refreshHover){refreshHover=on;InvalidateRect(h,nullptr,FALSE);}}
        int g=gameAt(x,y),m=modAt(x,y);if(g!=hoverGame||m!=hoverMod){hoverGame=g;hoverMod=m;InvalidateRect(h,nullptr,FALSE);}
        TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0};TrackMouseEvent(&t);return 0;}
    case WM_LBUTTONUP:if(barDrag)ReleaseCapture();return 0;
    case WM_CAPTURECHANGED:if(barDrag){barDrag=false;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_MOUSELEAVE:if(hoverGame>=0||hoverMod>=0||barHover||refreshHover){hoverGame=hoverMod=-1;barHover=refreshHover=false;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_SETCURSOR:if(hoverGame>=0||(refreshHover&&!busy)){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}break;
    case WM_LBUTTONDOWN:{int x=(short)LOWORD(lp),y=(short)HIWORD(lp);RECT t;
        if(x>=refreshRect.left&&x<refreshRect.right&&y>=refreshRect.top&&y<refreshRect.bottom){rescan();return 0;}
        // the list's bar: drag it; a click beside it on the track brings its middle there first
        if(barRect(t)&&x>=t.left&&x<t.right&&y>=list.top+6&&y<list.bottom-6){if(y<t.top||y>=t.bottom){scrollToBar(y-(t.bottom-t.top)/2);barRect(t);}barDrag=true;barGrab=y-t.top;SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}
        if(y>=strip.top+26&&y<strip.top+58){if(x<strip.left+30&&gameScroll>0){gameScroll-=cardW+cardGap;clampScroll();InvalidateRect(h,nullptr,FALSE);return 0;}
            if(x>=strip.right-30&&gameScroll<cardsWidth()-(int)(strip.right-strip.left)){gameScroll+=cardW+cardGap;clampScroll();InvalidateRect(h,nullptr,FALSE);return 0;}}
        int g=gameAt(x,y);if(g>=0&&g!=pick&&!busy){pick=g;modScroll=0;note.clear();clampScroll();buttons();InvalidateRect(h,nullptr,FALSE);}return 0;}
    case WM_MOUSEWHEEL:{POINT p={(short)LOWORD(lp),(short)HIWORD(lp)};ScreenToClient(h,&p);int d=GET_WHEEL_DELTA_WPARAM(wp);
        // the strip sits above the page's vertical stretch line, so client y is its design y
        if(p.y>=strip.top&&p.y<strip.bottom)gameScroll-=d*(cardW+cardGap)/WHEEL_DELTA;else modScroll-=d*rowH*3/WHEEL_DELTA;clampScroll();InvalidateRect(h,nullptr,FALSE);return 0;}
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){if(LOWORD(wp)==261)translate();else if(LOWORD(wp)==260)restore();else if(LOWORD(wp)==262)PostMessageW(win,WM_COMMAND,49,0);}return 0;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,std::max(1L,c.right),std::max(1L,c.bottom));auto old=SelectObject(mem,bmp);
        eraseCarbon(h,mem);paint(mem);BitBlt(dc,0,0,c.right,c.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_TIMER:if(wp==5){if(GetTickCount64()-spinAt>=700&&!scanning){spinAt=0;KillTimer(h,5);}InvalidateRect(h,nullptr,FALSE);return 0;}
        if(wp==4){if(!scanning&&!busy){KillTimer(h,4);return 0;}
        // the button's percentage eases toward the real one; only the button repaints unless a list is loading
        if(busy){double k=progress();shownK+=(k-shownK)*.18;if(std::abs(k-shownK)<.002)shownK=k;InvalidateRect(GetDlgItem(h,261),nullptr,FALSE);}if(scanning)InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_ERASEBKGND:return 1; // WM_PAINT draws the whole page off-screen
    case WM_DRAWITEM:{auto* ds=(DRAWITEMSTRUCT*)lp;if(ds->CtlID==261&&busy)drawProgress(ds);else drawButton(ds);return TRUE;}
    case WM_DESTROY:page=nullptr;scanning=false;return 0; // a scan cut short by leaving: the next page scans again
    case WM_CLOSE:DestroyWindow(h);return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
}
