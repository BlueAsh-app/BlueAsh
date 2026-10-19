#pragma once
#include "i18n.h"
#include "mapfit.h"
#include <fstream>
#include <sstream>
#include <memory>
// PUBG world map labels: Korean place names, callouts and secret rooms drawn over the game's own map, so the blue zone,
// pings and markers stay visible. The map key comes from GameUserSettings.ini (ToggleMap). While the map is open the
// PUBG window is captured (Windows.Graphics.Capture) about four times a second and mapfit.h finds the visible part of
// the map, so labels follow zoom and drag. The overlay is a transparent click-through window; nothing is sent to the game.
// Data: maps/pubg/<Map>.txt (labels) and maps/pubg/refs/<Map>.gray (273x273 gray of the official map, for matching).
namespace pubgmap {
struct Label {int kind=0;double u=0,v=0;std::wstring name,note;}; // kind 0 place, 1 callout, 2 secret room, 3 boat, 4 car
struct MapInfo {const wchar_t* id;const wchar_t* korean;int cells;};
const MapInfo maps[]={{L"Erangel",L"에란겔",8},{L"Miramar",L"미라마",8},{L"Taego",L"태이고",8},{L"Deston",L"데스턴",8},{L"Vikendi",L"비켄디",8},
    {L"Rondo",L"론도",8},{L"Sanhok",L"사녹",4},{L"Karakin",L"카라킨",2},{L"Paramo",L"파라모",3},{L"Haven",L"헤이븐",1}};
const int mapCount=sizeof(maps)/sizeof(maps[0]);
inline std::wstring widen8(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,0);if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n);return w;}
inline std::string readAll(const std::wstring& path){std::string s;FILE* f=_wfopen(path.c_str(),L"rb");if(!f){return s;}char b[65536];size_t n;while((n=fread(b,1,sizeof(b),f))>0)s.append(b,n);fclose(f);return s;}
inline std::vector<Label> readLabels(const std::wstring& path){
    std::vector<Label> out;std::stringstream f(readAll(path));std::string line;
    while(std::getline(f,line)){if(line.size()>=3&&(unsigned char)line[0]==0xEF)line.erase(0,3);if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line.empty()||line[0]=='#'){continue;}std::vector<std::string> p;std::stringstream ss(line);std::string cell;while(std::getline(ss,cell,'\t'))p.push_back(cell);
        if(p.size()<4){continue;}Label l;l.kind=p[0]=="secret"?2:p[0]=="callout"?1:p[0]=="boat"?3:p[0]=="car"?4:0;l.u=atof(p[1].c_str());l.v=atof(p[2].c_str());l.name=widen8(p[3]);if(p.size()>4)l.note=widen8(p[4]);
        if(l.u>=0&&l.u<=1&&l.v>=0&&l.v<=1)out.push_back(l);}
    return out;
}
// Unreal key name -> virtual key ("M", "Tab", "F1", "One", "NumPadOne", "SpaceBar"...); 0 when unknown.
inline int ueKey(const std::string& k){
    if(k.size()==1&&k[0]>='A'&&k[0]<='Z'){return k[0];}
    if(k.size()>=2&&k[0]=='F'&&isdigit((unsigned char)k[1])){int n=atoi(k.c_str()+1);if(n>=1&&n<=24){return VK_F1+n-1;}}
    const char* digits[]={"Zero","One","Two","Three","Four","Five","Six","Seven","Eight","Nine"};
    for(int i=0;i<10;i++){if(k==digits[i]){return '0'+i;}if(k==std::string("NumPad")+digits[i]){return VK_NUMPAD0+i;}}
    struct N {const char* n;int vk;};const N named[]={{"Tab",VK_TAB},{"SpaceBar",VK_SPACE},{"Enter",VK_RETURN},{"CapsLock",VK_CAPITAL},{"Tilde",VK_OEM_3},
        {"Comma",VK_OEM_COMMA},{"Period",VK_OEM_PERIOD},{"Slash",VK_OEM_2},{"Semicolon",VK_OEM_1},{"Apostrophe",VK_OEM_7},{"LeftBracket",VK_OEM_4},{"RightBracket",VK_OEM_6},
        {"Backslash",VK_OEM_5},{"Hyphen",VK_OEM_MINUS},{"Equals",VK_OEM_PLUS},{"Insert",VK_INSERT},{"Delete",VK_DELETE},{"Home",VK_HOME},{"End",VK_END},
        {"PageUp",VK_PRIOR},{"PageDown",VK_NEXT},{"Add",VK_ADD},{"Subtract",VK_SUBTRACT},{"Multiply",VK_MULTIPLY},{"Divide",VK_DIVIDE},{"Decimal",VK_DECIMAL},
        {"MiddleMouseButton",VK_MBUTTON},{"ThumbMouseButton",VK_XBUTTON1},{"ThumbMouseButton2",VK_XBUTTON2}};
    for(auto& n:named){if(k==n.n)return n.vk;}return 0;
}
struct Key {int vk=0;bool ctrl=false,alt=false,shift=false;};
// Bindings of ToggleMap from the game's settings text; M when none can be read.
inline std::vector<Key> mapKeys(const std::string& ini){
    std::vector<Key> out;size_t at=0;const std::string tag="ActionName=\"ToggleMap\",Keys=(";
    while((at=ini.find(tag,at))!=std::string::npos){at+=tag.size();size_t end=ini.find("))",at);if(end==std::string::npos){break;}std::string list=ini.substr(at,end-at+1);size_t k=0;
        while((k=list.find("(Key=",k))!=std::string::npos){k+=5;size_t e=list.find(')',k);std::string one=list.substr(k,e-k);Key key;size_t c=one.find(',');key.vk=ueKey(one.substr(0,c));
            key.ctrl=one.find("bCtrl=True")!=std::string::npos;key.alt=one.find("bAlt=True")!=std::string::npos;key.shift=one.find("bShift=True")!=std::string::npos;
            if(key.vk&&std::none_of(out.begin(),out.end(),[&](const Key& o){return o.vk==key.vk&&o.ctrl==key.ctrl&&o.alt==key.alt&&o.shift==key.shift;}))out.push_back(key);}
        at=end;}
    if(out.empty()){out.push_back({'M',false,false,false});}return out;
}
}
#ifndef PUBGMAP_NO_CAPTURE
#include <winhttp.h>
#include <bcrypt.h>
// The 4096 px official map for deep zoom (mapfit::Gray8): built by source/build-pubg-hires.ps1, served by the relay
// (/pubg/manifest.txt: id width size sha256, /pubg/<id>.u8), fetched for the map in play the first time it is identified,
// checked against the manifest's SHA-256 and kept in %LOCALAPPDATA%\ModeSwitch\pubg-maps. One map at a time in memory.
// The loader thread is detached and the object never destroyed (it may still be downloading when BlueAsh closes).
class HiresMaps {
    std::mutex mutex;std::shared_ptr<const mapfit::Pyr> map;int mapIndex=-1;bool tried[pubgmap::mapCount]={};std::atomic<bool> busy{false};std::wstring done;
    static std::wstring dir(){wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){return L"";}std::wstring d=std::wstring(local)+L"\\ModeSwitch\\pubg-maps\\";CreateDirectoryW(d.c_str(),nullptr);return d;}
    static bool get(const std::wstring& path,std::string& body,size_t limit){HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;DWORD status=0,size=sizeof(status);bool ok=false;
        if(s){WinHttpSetTimeouts(s,10000,10000,20000,20000);c=WinHttpConnect(s,L"modepulse-relay.modepulse.workers.dev",INTERNET_DEFAULT_HTTPS_PORT,0);}
        if(c)r=WinHttpOpenRequest(c,L"GET",path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE);
        if(r&&WinHttpSendRequest(r,nullptr,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)&&status==200){
            std::vector<char> buf(1<<16);DWORD n=0;ok=true;while(true){if(!WinHttpReadData(r,buf.data(),(DWORD)buf.size(),&n)){ok=false;break;}if(!n){break;}body.append(buf.data(),n);if(body.size()>limit){ok=false;break;}}}
        for(HINTERNET h:{r,c,s}){if(h)WinHttpCloseHandle(h);}return ok;}
    static std::string sha256(const std::string& data){BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;unsigned char d[32]={};std::string hex;
        if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0){if(BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)==0){BCryptHashData(h,(PUCHAR)data.data(),(ULONG)data.size(),0);if(BCryptFinishHash(h,d,32,0)==0){char x[3];for(int i=0;i<32;i++){snprintf(x,3,"%02x",d[i]);hex+=x;}}BCryptDestroyHash(h);}BCryptCloseAlgorithmProvider(alg,0);}
        return hex;}
    void load(int i){std::string id;for(const wchar_t* c=pubgmap::maps[i].id;*c;c++){id+=(char)*c;}std::wstring d=dir(),file=d+pubgmap::maps[i].id+L".u8";std::string body=pubgmap::readAll(file),note;
        if(body.size()!=4096u*4096){body.clear();std::string manifest;
            if(!d.empty()&&get(L"/pubg/manifest.txt",manifest,65536)){std::stringstream f(manifest);std::string line;while(std::getline(f,line)){std::stringstream ls(line);std::string m,hash;int w=0;size_t n=0;ls>>m>>w>>n>>hash;
                if(m!=id||w!=4096||n!=4096u*4096){continue;}std::string data;std::wstring path=L"/pubg/"+std::wstring(pubgmap::maps[i].id)+L".u8";
                if(!get(path,data,n+1)||data.size()!=n){note="download failed";break;}if(sha256(data)!=hash){note="checksum mismatch";break;}
                if(FILE* out=_wfopen((file+L".part").c_str(),L"wb")){fwrite(data.data(),1,data.size(),out);fclose(out);MoveFileExW((file+L".part").c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING);}body.swap(data);note="downloaded";break;}
                if(note.empty()){note="not on the server";}}
            else{note="manifest unavailable";}}
        else{note="from cache";}
        std::shared_ptr<mapfit::Pyr> g;if(body.size()==4096u*4096){g=std::make_shared<mapfit::Pyr>();g->L.resize(1);g->L[0].w=g->L[0].h=4096;g->L[0].p.assign(body.begin(),body.end());mapfit::blur8(g->L[0]);mapfit::pyramid(*g);} // + halves down to 128 px (5 MB)
        std::lock_guard<std::mutex> l(mutex);if(g){map=g;mapIndex=i;}else{tried[i]=false;} // tried again on the next match
        done=L"deep zoom map "+std::wstring(pubgmap::maps[i].id)+L": "+std::wstring(note.begin(),note.end())+(g?L"":L" (not used)");}
public:
    // The map in play: loads it in the background unless it is there or on the way.
    void want(int i){if(i<0||i>=pubgmap::mapCount){return;}{std::lock_guard<std::mutex> l(mutex);if(mapIndex==i||tried[i]||busy){return;}tried[i]=true;}busy=true;
        try{std::thread([this,i]{load(i);busy=false;}).detach();}catch(...){busy=false;}}
    std::shared_ptr<const mapfit::Pyr> of(int i){std::lock_guard<std::mutex> l(mutex);return mapIndex==i?map:nullptr;}
    std::wstring takeNote(){std::lock_guard<std::mutex> l(mutex);std::wstring n;n.swap(done);return n;}
};
HiresMaps& pubgHires=*new HiresMaps;
// Background capture + fit while the map is open.
class MapCapture {
    std::thread worker;HANDLE stopEvent=nullptr;std::mutex mutex;std::atomic<bool> active{false};HWND targetWindow=nullptr;
    struct State {bool ok=false,grid=false;POINT cursor={0,0};double ox=0,oy=0,S=0,score=0;int map=-1,w=0,h=0;unsigned seq=0;ULONGLONG lastOk=0;int nOk=0,nWeak=0,nFail=0,nNoGrid=0;std::wstring status=i18n::text(L"대기");} state; // n*: frames of this opening
    std::wstring note; // one-off message for pubg-board.log, taken by the main thread
    // For an offline benchmark: the full-resolution green channel (what the matcher reads) as an 8-bit gray BMP and a .txt with
    // this frame's state, in %LOCALAPPDATA%\ModeSwitch\pubg-map\<kind>-NN. Fixed slots, the oldest one overwritten.
    static std::wstring keepFrame(const mapfit::Gray& full,const wchar_t* kind,int slots,const std::string& info){wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){return L"";}
        std::wstring dir=std::wstring(local)+L"\\ModeSwitch\\pubg-map\\";CreateDirectoryW(dir.c_str(),nullptr);int best=0;ULONGLONG oldest=~0ull;wchar_t n[64];
        for(int i=0;i<slots;i++){swprintf(n,64,L"%ls-%02d.bmp",kind,i);WIN32_FILE_ATTRIBUTE_DATA a;ULONGLONG t=0;if(GetFileAttributesExW((dir+n).c_str(),GetFileExInfoStandard,&a)){t=(ULONGLONG(a.ftLastWriteTime.dwHighDateTime)<<32)|a.ftLastWriteTime.dwLowDateTime;}if(t<oldest){oldest=t;best=i;}}
        swprintf(n,64,L"%ls-%02d",kind,best);std::wstring base=dir+n;FILE* out=_wfopen((base+L".bmp").c_str(),L"wb");if(!out){return L"";}int w=full.w,h=full.h,row=(w+3)&~3;
        BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};ih.biSize=sizeof(ih);ih.biWidth=w;ih.biHeight=-h;ih.biPlanes=1;ih.biBitCount=8;ih.biClrUsed=256;fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih)+1024;fh.bfSize=fh.bfOffBits+row*h;
        fwrite(&fh,sizeof(fh),1,out);fwrite(&ih,sizeof(ih),1,out);RGBQUAD pal[256];for(int i=0;i<256;i++){pal[i]={(BYTE)i,(BYTE)i,(BYTE)i,0};}fwrite(pal,sizeof(pal),1,out);
        std::vector<unsigned char> r(row,0);for(int y=0;y<h;y++){const float* p=&full.p[size_t(y)*w];for(int x=0;x<w;x++){r[x]=(unsigned char)std::clamp(p[x],0.0f,255.0f);}fwrite(r.data(),1,row,out);}fclose(out);
        if(FILE* t=_wfopen((base+L".txt").c_str(),L"wb")){fwrite(info.data(),1,info.size(),t);fclose(t);}return n;}
    void status(const std::wstring& s){std::lock_guard<std::mutex> l(mutex);state.status=s;}
    mapfit::Fit saved;int savedF=0;POINT savedCursor={0,0}; // last good placement, kept between openings (the game reopens the map as it was)
    std::shared_ptr<const mapfit::Pyr> ultra;int ultraMap=-1; // 4096 px map of the map in play, once loaded (HiresMaps)
    void run(HWND hwnd,const std::vector<mapfit::Gray>& refs,const std::vector<mapfit::Gray>& hi,const std::vector<int>& cells,int known){
        if(FAILED(RoInitialize(RO_INIT_MULTITHREADED))){status(i18n::text(L"캡처 초기화 실패"));active=false;return;}
        // The cursor is read in physical pixels (the captured frame's own), so this thread is per-monitor DPI aware.
        {using Set=HANDLE(WINAPI*)(HANDLE);Set set=nullptr;FARPROC sp=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetThreadDpiAwarenessContext");memcpy(&set,&sp,sizeof(set));if(set){set((HANDLE)-4);}}
        {using namespace wgc;Ptr<ID3D11Device> device;Ptr<ID3D11DeviceContext> context;Ptr<Pool> pool;Ptr<Session> session;
        try {
            require(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.out(),nullptr,context.out()));
            Ptr<IDXGIDevice> dxgi;require(device->QueryInterface(__uuidof(IDXGIDevice),(void**)dxgi.out()));
            using Bridge=HRESULT(WINAPI*)(IDXGIDevice*,IInspectable**);Bridge bridge=nullptr;FARPROC proc=GetProcAddress(GetModuleHandleW(L"d3d11.dll"),"CreateDirect3D11DeviceFromDXGIDevice");memcpy(&bridge,&proc,sizeof(bridge));if(!bridge)throw std::wstring(i18n::text(L"Windows 10 1903 이상이 필요합니다."));
            Ptr<IInspectable> raw,rt;require(bridge(dxgi.p,raw.out()));require(raw->QueryInterface(iidDevice,(void**)rt.out()));
            Ptr<Interop> interop;factory(L"Windows.Graphics.Capture.GraphicsCaptureItem",iidInterop,(void**)interop.out());
            Ptr<Item> item;require(interop->ForWindow(hwnd,iidItem,(void**)item.out()));Size size={};require(item->GetSize(&size));
            if(size.width<320||size.height<200||size.width>8192||size.height>8192)throw std::wstring(i18n::text(L"지원 범위를 벗어난 창 크기입니다."));
            Ptr<PoolFactory> pf;factory(L"Windows.Graphics.Capture.Direct3D11CaptureFramePool",iidPoolFactory,(void**)pf.out());require(pf->FreeThreaded(rt.p,87,2,size,pool.out()));require(pool->CreateSession(item.p,session.out()));
            Ptr<Session2> s2;if(SUCCEEDED(session->QueryInterface(iidSession2,(void**)s2.out())))s2->SetCursor(false);
            // At most 20 frames a second are copied (the loop reads every 0.06 s anyway), not one per game frame.
            Ptr<Session5> s5;if(SUCCEEDED(session->QueryInterface(iidSession5,(void**)s5.out())))s5->SetInterval({500000});
            noBorder(session.p);require(session->Start());Ptr<ID3D11Texture2D> staging;
            // Coordinates: the matching runs on the frame shrunk by f, where pixel i covers frame pixels [i*f, i*f+f), so
            // frame x = shrunk x * f + h0.
            int f=std::max(1,(int)std::lround(size.height/540.0));const double h0=(f-1)/2.0;
            mapfit::Gray full;full.w=size.width;full.h=size.height;full.p.resize(size_t(full.w)*full.h);mapfit::Gray small;small.w=size.width/f;small.h=size.height/f;small.p.resize(size_t(small.w)*small.h);
            ULONGLONG dumpAt=0,okDumpAt=0,wholeAt=0;const ULONGLONG startAt=GetTickCount64();std::vector<float> prevSmall;mapfit::Fit last,good;mapfit::Grid lastGrid;ULONGLONG goodAt=0;POINT goodCursor={0,0};
            mapfit::Fit reopen=saved.ok&&savedF==f&&known>=0&&saved.map==known?saved:mapfit::Fit{};DWORD wait=0;int idle=0,wholeMiss=0;
            while(WaitForSingleObject(stopEvent,wait)==WAIT_TIMEOUT){
                wait=15; // no frame yet: look again soon (the first one comes within a few frames of the game)
                if(!IsWindow(hwnd)){break;}if(IsIconic(hwnd)){wait=200;continue;}
                FramePtr frame;require(pool->Next(frame.out()));if(!frame){continue;}
                for(int drain=0;drain<3;drain++){FramePtr newer;require(pool->Next(newer.out()));if(!newer){break;}close(frame.p);frame.reset();frame.p=newer.p;newer.p=nullptr;}
                Size content={};require(frame->ContentSize(&content));if(content.width!=size.width||content.height!=size.height)throw std::wstring(i18n::text(L"창 크기가 바뀌었습니다."));
                Ptr<IInspectable> surface;require(frame->Surface(surface.out()));Ptr<SurfaceAccess> access;require(surface->QueryInterface(iidAccess,(void**)access.out()));Ptr<ID3D11Texture2D> texture;require(access->GetInterface(__uuidof(ID3D11Texture2D),(void**)texture.out()));
                D3D11_TEXTURE2D_DESC source={};texture->GetDesc(&source);if(source.Format!=DXGI_FORMAT_B8G8R8A8_UNORM)throw std::wstring(i18n::text(L"지원하지 않는 캡처 형식입니다."));
                if(!staging){D3D11_TEXTURE2D_DESC d=source;d.MipLevels=1;d.ArraySize=1;d.SampleDesc.Count=1;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.BindFlags=0;d.MiscFlags=0;require(device->CreateTexture2D(&d,nullptr,staging.out()));}
                context->CopyResource(staging.p,texture.p);D3D11_MAPPED_SUBRESOURCE map={};require(context->Map(staging.p,0,D3D11_MAP_READ,0,&map));
                // Shrunk copy first (green channel, f x f average); an unchanged screen skips the grid and the search.
                // Green, not the brightest channel: the blue zone's blue overlay (most of the map late in a match) lifted
                // blue everywhere and washed the terrain out, so matches failed later in the game; the references are green too.
                for(int y=0;y<small.h;y++){for(int x=0;x<small.w;x++){int s=0;for(int dy=0;dy<f;dy++){const unsigned char* p=(const unsigned char*)map.pData+size_t(y*f+dy)*map.RowPitch+size_t(x*f)*4;for(int dx=0;dx<f;dx++,p+=4)s+=p[1];}small.p[size_t(y)*small.w+x]=float(s)/(f*f);}}
                int moved=0;bool changed=prevSmall.size()!=small.p.size();if(!changed){for(size_t i=0;i<small.p.size();i++){if(std::fabs(small.p[i]-prevSmall[i])>10)moved++;}changed=moved>(int)small.p.size()/400;}
                if(!changed&&(last.ok||++idle%4)){context->Unmap(staging.p,0);wait=150;if(last.ok){std::lock_guard<std::mutex> l(mutex);state.lastOk=GetTickCount64();}continue;}
                prevSmall=small.p;idle=0;
                for(int y=0;y<full.h;y++){const unsigned char* p=(const unsigned char*)map.pData+size_t(y)*map.RowPitch;float* o=&full.p[size_t(y)*full.w];for(int x=0;x<full.w;x++,p+=4)o[x]=p[1];}
                context->Unmap(staging.p,0);
                // Grid lines at full resolution (1-2 px, half transparent in the game), refined to sub-pixel; matching on 1/f.
                mapfit::Grid coarse,g;mapfit::gridFullBoth(full,f,coarse,g);mapfit::Gray blurred=mapfit::blur(small,2);mapfit::Fit fit;
                std::shared_ptr<const mapfit::Pyr> ul;int ulMap=-1;{std::lock_guard<std::mutex> l(mutex);ul=ultra;ulMap=ultraMap;}
                auto R=[&](int m){return mapfit::Refs{&refs[m],&hi[m],m==ulMap&&ul?&ul->L[0]:nullptr};};
                POINT cursor={0,0};GetCursorPos(&cursor);ScreenToClient(hwnd,&cursor);double cx=(cursor.x-h0)/f,cy=(cursor.y-h0)/f;
                // 1) Unchanged grid: the same placement. 2) Reopened map: the placement it was closed with (the game keeps it).
                if(last.ok&&g.ok&&lastGrid.ok&&std::fabs(g.d-lastGrid.d)<0.3&&std::fabs(g.px-lastGrid.px)<0.6&&std::fabs(g.py-lastGrid.py)<0.6){
                    double s=mapfit::score(blurred,R(last.map),last.ox,last.oy,last.S,48,27);if(s>mapfit::need(R(last.map),blurred,last.S,0.45)){fit=last;fit.score=s;}}
                // 1b) Zoomed in with the 4096 px map: follow from this match's last placement by the picture itself (mapfit::deepFit,
                // no grid snapping): the one of this opening within 1.5 s, else the one the map was closed with (the game reopens
                // the map as it was). Anchors: nothing moved / zoomed around the cursor / dragged with the cursor.
                const mapfit::Pyr* pyr=known>=0&&known==ulMap&&ul?ul.get():nullptr;bool byPicture=false;auto stopped=[&]{return WaitForSingleObject(stopEvent,0)==WAIT_OBJECT_0;};
                if(!fit.ok&&pyr){bool fromGood=good.ok&&good.map==known&&GetTickCount64()-goodAt<1500;const mapfit::Fit* prior=fromGood?&good:saved.ok&&saved.map==known&&savedF==f?&saved:nullptr;
                    if(prior&&(prior->S>=small.h*1.3||(g.ok&&g.d*cells[known]>=small.h*1.3))){const mapfit::Fit& p=*prior;POINT pc=fromGood?goodCursor:savedCursor;double lx=(pc.x-h0)/f,ly=(pc.y-h0)/f;
                        std::vector<mapfit::Anchor> an={{(small.w/2.0-p.ox)/p.S,(small.h/2.0-p.oy)/p.S,small.w/2.0,small.h/2.0},{(cx-p.ox)/p.S,(cy-p.oy)/p.S,cx,cy},{(lx-p.ox)/p.S,(ly-p.oy)/p.S,cx,cy}};
                        fit=mapfit::deepFit(blurred,g,*pyr,cells[known],known,prior,an,false,stopped);byPicture=fit.ok;}}
                if(!fit.ok&&reopen.ok){fit=mapfit::resume(blurred,g,refs[reopen.map],cells[reopen.map],reopen,&hi[reopen.map],R(reopen.map).ultra);reopen.ok=false;}
                // 3) Zoomed or dragged: follow from the last good placement (up to 1.5 s back, so a blurred frame mid-zoom
                // does not lose it); the map point that was under the cursor is under the cursor now.
                if(!fit.ok&&g.ok&&good.ok&&GetTickCount64()-goodAt<1500){
                    fit=mapfit::track(blurred,g,refs[good.map],cells[good.map],good,(goodCursor.x-h0)/f,(goodCursor.y-h0)/f,cx,cy,&hi[good.map],R(good.map).ultra);}
                // 3b) Lost (a fast zoom, blurred frames, reopened elsewhere): search around the last placement of this match,
                // 1.5 km around the map point that was under the cursor. Deep in, a search over the whole map finds look-alike
                // places (a 250 m view is one of 6400 cells of 100 m); this one, with the 4096 px map, does not.
                if(!fit.ok&&g.ok&&known>=0){bool fromGood=good.ok&&good.map==known;const mapfit::Fit& prior=fromGood?good:saved;POINT pc=fromGood?goodCursor:savedCursor;
                    if(prior.ok&&prior.map==known&&(fromGood||savedF==f)){auto rs=R(known);double lx=(pc.x-h0)/f,ly=(pc.y-h0)/f;
                        auto a=mapfit::nearby(blurred,g,rs,cells[known],cells[known],prior,lx,ly,cx,cy),b=mapfit::nearby(blurred,coarse,rs,cells[known],cells[known],prior,lx,ly,cx,cy);
                        fit=(b.ok&&(!a.ok||b.score>a.score))?b:a;}}
                if(!fit.ok&&g.ok){
                    // 4) Search: known map first (cheaper, steadier when zoomed in), then all maps, in case the match changed
                    // without a join line in the log.
                    // Each with the refined and the coarse grid, the better match wins. A map not yet known needs 60 %
                    // (a real Erangel screen once came out as Rondo at 57 %).
                    auto both=[&](const std::vector<mapfit::Gray>& r,const std::vector<mapfit::Gray>& h,const std::vector<int>& c,double minScore,double maxS,const std::vector<const mapfit::Gray8*>* u=nullptr){auto a=mapfit::fit(blurred,g,r,c,minScore,&h,maxS,u),b=mapfit::fit(blurred,coarse,r,c,minScore,&h,maxS,u);return (b.ok&&(!a.ok||b.score>a.score))?b:a;};
                    // The map is identified once per match, near the whole view (map at most twice the screen height) and at
                    // 70 %; after that only that map is searched. Zoomed in, a part of another map can look alike by chance
                    // (a real Erangel match drifted through Paramo, Sanhok, Vikendi... at 60-80 %).
                    if(known>=0){std::vector<mapfit::Gray> one={refs[known]},oneHi={hi[known]};std::vector<int> c={cells[known]};std::vector<const mapfit::Gray8*> oneU={R(known).ultra};fit=both(one,oneHi,c,0.45,0,&oneU);if(fit.ok){fit.map=known;}
                        // Deep in, the whole-map search has look-alikes: only a clear winner (0.1 over the runner-up).
                        if(fit.ok&&fit.S>small.h*2.5&&fit.score-fit.second<0.1){fit.ok=false;}
                        // Five misses in a row near the whole view: the match changed without a join line. Identify again.
                        bool whole=g.ok&&g.d*cells[known]<=small.h*2.0;wholeMiss=fit.ok?0:wholeMiss+(whole?1:0);if(wholeMiss>=5){known=-1;wholeMiss=0;}}
                    if(known<0){fit=both(refs,hi,cells,0.7,small.h*2.0);if(fit.ok){known=fit.map;}}
                }
                // 5) Zoomed in with the 4096 px map: a placement found by the grid is kept only if the picture agrees there (and is
                // refined to it); lost altogether, the picture is searched over the whole map (up to a second), at most every 2 s.
                if(pyr&&known==ulMap&&fit.ok&&!byPicture&&fit.map==known&&fit.S>=small.h*1.2){fit=mapfit::verify(blurred,*pyr,fit,g,cells[known]);}
                if(pyr&&known==ulMap&&!fit.ok&&g.ok&&GetTickCount64()-wholeAt>2000){wholeAt=GetTickCount64();fit=mapfit::deepFit(blurred,g,*pyr,cells[known],known,nullptr,{},true,stopped);}
                // Counted and kept only with a map grid on screen (none = the map is closed or still opening). Weak or failed
                // frames at most every 2 s (24 kept), good ones every 20 s (6 kept, to check a fix does not break them).
                {ULONGLONG t=GetTickCount64();bool weak=fit.ok&&fit.score<0.65,bad=g.ok&&(!fit.ok||weak);
                 {std::lock_guard<std::mutex> l(mutex);if(!g.ok){state.nNoGrid++;}else if(!fit.ok){state.nFail++;}else if(weak){state.nWeak++;}else{state.nOk++;}}
                 if(g.ok&&t-startAt>400&&((bad&&t-dumpAt>2000)||(!bad&&t-okDumpAt>20000))){(bad?dumpAt:okDumpAt)=t;char info[1024];
                     snprintf(info,sizeof(info),"f %d\nknown %d\nfit %d %d %.3f %.3f %.3f %.4f\ngrid %d %.3f %.3f %.3f\ncoarse %d %.3f %.3f %.3f\ncursor %ld %ld\ngood %d %d %.3f %.3f %.3f %llu\ngoodCursor %ld %ld\nlast %d %d %.3f %.3f %.3f\n",
                         f,known,fit.ok,fit.map,fit.ox,fit.oy,fit.S,fit.score,g.ok,g.d,g.px,g.py,coarse.ok,coarse.d,coarse.px,coarse.py,cursor.x,cursor.y,good.ok,good.map,good.ox,good.oy,good.S,(unsigned long long)(t-goodAt),goodCursor.x,goodCursor.y,last.ok,last.map,last.ox,last.oy,last.S);
                     std::wstring name=keepFrame(full,bad?L"fail":L"ok",bad?24:6,info);
                     if(bad&&!name.empty()){std::lock_guard<std::mutex> l(mutex);note=L"map "+std::wstring(fit.ok?L"weak ":L"fail ")+std::to_wstring(int(fit.score*100))+L"% · grid "+std::to_wstring(int(g.d*f))+L"px · saved pubg-map\\"+name+L".bmp";}}}
                last=fit;lastGrid=g;if(fit.ok){good=fit;goodAt=GetTickCount64();goodCursor=cursor;saved=fit;savedF=f;savedCursor=cursor;}
                wait=60;
                {std::lock_guard<std::mutex> l(mutex);state.ok=fit.ok;state.score=fit.score;state.w=size.width;state.h=size.height;
                 state.grid=g.ok;if(fit.ok){state.cursor=cursor;state.ox=fit.ox*f+h0;state.oy=fit.oy*f+h0;state.S=fit.S*f;state.map=fit.map;state.lastOk=GetTickCount64();}
                 state.seq++;state.status=fit.ok?i18n::text(L"지도 위치 확인"):g.ok?i18n::text(L"지도 대조 실패"):i18n::text(L"격자 없음 (지도가 닫혀 있음)");}
            }
        }catch(const std::wstring& e){status(e);}catch(...){status(i18n::text(L"화면 읽기 실패"));}
        close(session.p);close(pool.p);}
        RoUninitialize();active=false;
    }
public:
    ~MapCapture(){stop();}
    void stop(){if(stopEvent)SetEvent(stopEvent);if(worker.joinable())worker.join();if(stopEvent)CloseHandle(stopEvent);stopEvent=nullptr;active=false;targetWindow=nullptr;}
    void start(HWND h,const std::vector<mapfit::Gray>& refs,const std::vector<mapfit::Gray>& hi,const std::vector<int>& cells,int known){stop();{std::lock_guard<std::mutex> l(mutex);state={};state.lastOk=GetTickCount64();}
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent){return;}targetWindow=h;active=true;
        try{worker=std::thread([this,h,&refs,&hi,&cells,known]{run(h,refs,hi,cells,known);});}catch(...){active=false;}}
    void setUltra(int m,std::shared_ptr<const mapfit::Pyr> u){std::lock_guard<std::mutex> l(mutex);if(!u){m=-1;}ultra=std::move(u);ultraMap=m;}
    bool running()const{return active;}
    HWND target()const{return targetWindow;}
    State read(){std::lock_guard<std::mutex> l(mutex);return state;}
    std::wstring takeNote(){std::lock_guard<std::mutex> l(mutex);std::wstring n;n.swap(note);return n;}
};
#endif
// Transparent, click-through, topmost window over the PUBG client area. Drawn in physical pixels (per-monitor DPI
// aware while creating and updating, as BlueAsh itself is DPI unaware).
struct MapOverlay {
    HWND window=nullptr;std::vector<unsigned char> pixels;int w=0,h=0;POINT base={0,0};HBITMAP dib=nullptr;HDC dibDC=nullptr;void* dibBits=nullptr;int dibW=0,dibH=0; // dib kept between redraws
    struct Aware {using Set=HANDLE(WINAPI*)(HANDLE);Set set=nullptr;HANDLE old=nullptr;
        Aware(){FARPROC p=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetThreadDpiAwarenessContext");memcpy(&set,&p,sizeof(set));if(set)old=set((HANDLE)-4);}
        ~Aware(){if(set&&old)set(old);}};
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_NCHITTEST){return HTTRANSPARENT;}return DefWindowProcW(h,m,w,l);}
    bool ensure(){if(window){return true;}Aware a;WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshPubgMap";RegisterClassW(&wc);
        window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);return window!=nullptr;}
    void hide(){if(window)ShowWindow(window,SW_HIDE);}
    void destroy(){if(window)DestroyWindow(window);window=nullptr;pixels.clear();w=h=0;if(dibDC)DeleteDC(dibDC);if(dib)DeleteObject(dib);dib=nullptr;dibDC=nullptr;dibBits=nullptr;dibW=dibH=0;}
    // Premultiplied "over" of one stamp: coverage masks for the outline and the fill, fill colour c.
    void stamp(int x0,int y0,int sw,int sh,const std::vector<unsigned char>& outline,const std::vector<unsigned char>& fill,COLORREF c){
        for(int y=0;y<sh;y++){int ty=y0+y;if(ty<0||ty>=h){continue;}for(int x=0;x<sw;x++){int tx=x0+x;if(tx<0||tx>=w){continue;}size_t i=size_t(y)*sw+x;
            int f=fill[i],a=std::max<int>(outline[i],f);if(!a){continue;}unsigned char* p=&pixels[(size_t(ty)*w+tx)*4];
            int r=GetRValue(c)*f/255,g=GetGValue(c)*f/255,b=GetBValue(c)*f/255; // source premultiplied (outline is black)
            p[0]=(unsigned char)(b+p[0]*(255-a)/255);p[1]=(unsigned char)(g+p[1]*(255-a)/255);p[2]=(unsigned char)(r+p[2]*(255-a)/255);p[3]=(unsigned char)(a+p[3]*(255-a)/255);}}}
    // Text (and an optional ring above it) centred on x,y, black outline like the game's own labels.
    void label(HDC dc,const std::wstring& source,int px,int x,int y,COLORREF c,int ring=0){
        auto text=i18n::text(source);const wchar_t* family=i18n::fontFamily();
        HFONT font=CreateFontW(-px,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,family);HGDIOBJ old=SelectObject(dc,font);
        SIZE ts={0,0};if(!text.empty())GetTextExtentPoint32W(dc,text.c_str(),(int)text.size(),&ts);int pad=3,sw=std::max<int>(ts.cx,ring*2+2)+pad*2+4,sh=ts.cy+(ring?ring*2+4:0)+pad*2+4;
        BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=sw;bi.bmiHeader.biHeight=-sh;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
        void* bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);HDC mem=CreateCompatibleDC(dc);HGDIOBJ ob=SelectObject(mem,bmp);SelectObject(mem,font);SetBkMode(mem,TRANSPARENT);SetTextColor(mem,RGB(255,255,255));
        int tx=(sw-ts.cx)/2,ty=pad+2+(ring?ring*2+4:0);std::vector<unsigned char> outline(size_t(sw)*sh),fill(size_t(sw)*sh);
        auto grab=[&](std::vector<unsigned char>& to){GdiFlush();const unsigned char* b=(const unsigned char*)bits;for(size_t i=0;i<to.size();i++)to[i]=std::max(b[i*4],std::max(b[i*4+1],b[i*4+2]));};
        auto ringAt=[&](int grow){if(!ring){return;}HPEN pen=CreatePen(PS_SOLID,std::max(2,ring/9)+grow*2,RGB(255,255,255));HGDIOBJ op=SelectObject(mem,pen);HGDIOBJ obr=SelectObject(mem,GetStockObject(NULL_BRUSH));
            Ellipse(mem,sw/2-ring,pad+2,sw/2+ring,pad+2+ring*2);SelectObject(mem,op);SelectObject(mem,obr);DeleteObject(pen);};
        memset(bits,0,size_t(sw)*sh*4);for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)if(dx||dy)TextOutW(mem,tx+dx,ty+dy,text.c_str(),(int)text.size());ringAt(1);grab(outline);
        memset(bits,0,size_t(sw)*sh*4);TextOutW(mem,tx,ty,text.c_str(),(int)text.size());ringAt(0);grab(fill);
        stamp(x-sw/2,y-(ring?pad+2+ring:sh/2),sw,sh,outline,fill,c);
        SelectObject(mem,ob);DeleteObject(bmp);DeleteDC(mem);SelectObject(dc,old);DeleteObject(font);
    }
    // Filled dot with a black rim (fixed boat / car spawns).
    void dot(int x,int y,int r,COLORREF c){int sw=r*2+8,sh=sw;std::vector<unsigned char> outline(size_t(sw)*sh),fill(size_t(sw)*sh);double cx=sw/2.0,cy=sh/2.0;
        for(int yy=0;yy<sh;yy++)for(int xx=0;xx<sw;xx++){double d=std::hypot(xx+0.5-cx,yy+0.5-cy);size_t i=size_t(yy)*sw+xx;
            fill[i]=(unsigned char)(255*std::clamp(r-d+0.5,0.0,1.0));outline[i]=(unsigned char)(255*std::clamp(r+2-d+0.5,0.0,1.0));}
        stamp(x-sw/2,y-sh/2,sw,sh,outline,fill,c);}
    // Side-view vehicle pictogram centred on x,y, height r*2, drawn at alpha (0..1) so the map shows through:
    // car = rounded body + cabin on two wheels (a thin gap around each wheel), boat = hull with a raked bow + windshield.
    // Shapes in units of the icon height (width 1.8), 4x4 samples per pixel, thin black rim.
    static bool roundRect(double u,double v,double x0,double y0,double x1,double y1,double rad){double cx=std::clamp(u,x0+rad,x1-rad),cy=std::clamp(v,y0+rad,y1-rad);return u>=x0&&u<=x1&&v>=y0&&v<=y1&&std::hypot(u-cx,v-cy)<=rad;}
    static bool carCabin(double u,double v){if(v<0.14||v>0.46)return false;double t=(0.46-v)/0.32;return roundRect(u,v,0.40+t*0.22,0.14,1.30-t*0.26,0.50,0.08);}
    static bool carBody(double u,double v){
        bool wheel=std::hypot(u-0.44,v-0.78)<0.17||std::hypot(u-1.36,v-0.78)<0.17,gap=std::hypot(u-0.44,v-0.78)<0.23||std::hypot(u-1.36,v-0.78)<0.23;
        if(wheel){return true;}if(gap){return false;}return roundRect(u,v,0.04,0.42,1.76,0.76,0.12)||carCabin(u,v);}
    static bool boatBody(double u,double v){
        if(v>=0.52&&v<=0.84){double t=(v-0.52)/0.32;if(u>=0.10+t*0.12&&u<=1.76-t*0.40)return true;}
        if(v>=0.26&&v<0.52){double t=(0.52-v)/0.26;return u>=0.62&&u<=1.12-t*0.26;}return false;}
    void vehicle(int x,int y,int r,COLORREF c,bool car,double alpha){int ih=r*2,iw=(int)std::lround(ih*1.8),pad=3,sw=iw+pad*2,sh=ih+pad*2;
        std::vector<unsigned char> body(size_t(sw)*sh),outline(size_t(sw)*sh),fill(size_t(sw)*sh);
        for(int yy=0;yy<sh;yy++)for(int xx=0;xx<sw;xx++){int f=0;for(int sy=0;sy<4;sy++)for(int sx=0;sx<4;sx++){double u=(xx-pad+(sx+0.5)/4)/ih,v=(yy-pad+(sy+0.5)/4)/ih;f+=car?carBody(u,v):boatBody(u,v);}
            body[size_t(yy)*sw+xx]=(unsigned char)(255*f/16);}
        int grow=std::max(1,r/6); // black rim width
        for(int yy=0;yy<sh;yy++)for(int xx=0;xx<sw;xx++){int m=0;for(int dy=-grow;dy<=grow;dy++)for(int dx=-grow;dx<=grow;dx++){int ty=yy+dy,tx=xx+dx;if(ty<0||tx<0||ty>=sh||tx>=sw||dx*dx+dy*dy>grow*grow+1){continue;}m=std::max<int>(m,body[size_t(ty)*sw+tx]);}
            size_t i=size_t(yy)*sw+xx;outline[i]=(unsigned char)(m*alpha*0.8);fill[i]=(unsigned char)(body[i]*alpha);}
        stamp(x-sw/2,y-sh/2,sw,sh,outline,fill,c);}
    // ox,oy,S in captured-frame pixels (frame = client area for a borderless window).
    void draw(HWND game,const std::vector<pubgmap::Label>& labels,double ox,double oy,double S,int frameW,int frameH,const std::wstring& mapName,bool all=true){
        if(!ensure()){return;}Aware a;RECT rc={};if(!GetClientRect(game,&rc)){return;}POINT org={0,0};ClientToScreen(game,&org);int cw=rc.right,ch=rc.bottom;if(cw<100||ch<100){return;}
        render(cw,ch,all?labels:std::vector<pubgmap::Label>{},ox,oy,S,double(cw)/std::max(1,frameW),double(ch)/std::max(1,frameH),mapName+(all?i18n::text(L" · L: 표시 끄기"):i18n::text(L" · 표시 꺼짐 (L: 켜기)")));HDC screen=GetDC(nullptr);
        BLENDFUNCTION bf={AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if(!dib||dibW!=w||dibH!=h){if(dibDC)DeleteDC(dibDC);if(dib)DeleteObject(dib);BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
            dib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&dibBits,nullptr,0);dibDC=dib?CreateCompatibleDC(screen):nullptr;if(dibDC)SelectObject(dibDC,dib);dibW=w;dibH=h;}
        if(dib&&dibDC){GdiFlush();memcpy(dibBits,pixels.data(),pixels.size());POINT src={0,0};SIZE sz={w,h};UpdateLayeredWindow(window,screen,&org,&sz,dibDC,&src,0,&bf,ULW_ALPHA);}base=org;
        ReleaseDC(nullptr,screen);ShowWindow(window,SW_SHOWNOACTIVATE);SetWindowPos(window,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    }
    // Labels into the premultiplied BGRA buffer (cw x ch); sx,sy = client pixels per captured-frame pixel.
    void render(int cw,int ch,const std::vector<pubgmap::Label>& labels,double ox,double oy,double S,double sx,double sy,const std::wstring& mapName){
        if(w!=cw||h!=ch){w=cw;h=ch;pixels.assign(size_t(w)*h*4,0);}else std::fill(pixels.begin(),pixels.end(),0);
        HDC screen=GetDC(nullptr);double mapPx=S*sx;
        // Sizes follow the screen (k = 1 at 1080p, 2 at 4K; on the whole 4K map a dot is 7 px, a ring 18 px across the radius,
        // the user found bigger too much). Rings and dots never grow: full size on the whole map, shrinking as it is
        // zoomed in (1/sqrt of the zoom, down to 35 %), so close up they mark the spot instead of covering it. Secret-room
        // rings stop at 60 % (smaller was hard to find), vehicle icons at 75 %.
        double k=h/1080.0,zoom=std::max(1.0,mapPx/h),shrink=std::max(0.35,1/std::sqrt(zoom));int place=(int)std::clamp(mapPx/105,14*k,34*k),callout=(int)std::clamp(mapPx/115,13*k,30*k),ring=std::max(4,int(9*k*std::max(0.6,shrink))),secretText=int(12*k),icon=std::max(4,int(4.5*k*std::max(0.75,shrink))); // icons need a readable size even close up
        for(auto& l:labels){int x=(int)std::lround((ox+l.u*S)*sx),y=(int)std::lround((oy+l.v*S)*sy);if(x<-50||y<-50||x>w+50||y>h+50){continue;}
            if(l.kind==0)label(screen,l.name,place,x,y,RGB(255,236,150));
            else if(l.kind==1)label(screen,l.name,callout,x,y,RGB(70,240,225));
            else if(l.kind==3)vehicle(x,y,icon,RGB(255,220,40),false,0.82); // boat: yellow
            else if(l.kind==4)vehicle(x,y,icon,RGB(255,140,30),true,0.82); // car: orange
            else label(screen,mapPx>=h*1.5?l.name:L"",secretText,x,y,RGB(200,110,255),ring);} // secret room: purple
        label(screen,L"BlueAsh · "+mapName,int(13*k),int(150*k),h-int(20*k),RGB(200,200,200));ReleaseDC(nullptr,screen);
    }
};
#ifndef PUBGMAP_NO_CAPTURE
// Main-thread control: PUBG in front -> map key polled every 30 ms (timer 77 on the main window).
struct PubgMapWatch {
    MapCapture capture;MapOverlay overlay;bool loaded=false,open=false,timer=false,shown=false,visible=true,toggleDown=true;double drawn[5]={-1,0,0,0,1};std::vector<pubgmap::Key> keys;std::vector<bool> down;bool escDown=false,tabDown=false;
    std::vector<mapfit::Gray> refs,hiRefs;std::vector<int> cells;std::vector<std::vector<pubgmap::Label>> labels;int known=-1;unsigned seenSeq=0;ULONGLONG nextCheck=0,openedAt=0,openStart=0;DWORD pid=0;HWND game=nullptr;
    ULONGLONG matchSeen=0;
    void load(){pubgMapOn=regString(configPath,L"PubgMapLabels")!=L"0";visible=regString(configPath,L"PubgMapVisible")!=L"0";}
    void toggleSetting(){pubgMapOn=!pubgMapOn;saveString(configPath,L"PubgMapLabels",pubgMapOn?L"1":L"0");if(!pubgMapOn){close(L"turned off");}}
    // The reference maps come with the data pack (datapack.h): until it has arrived, try again every 30 s.
    ULONGLONG retryAt=0;
    bool loadData(){if(loaded&&(!refs.empty()||GetTickCount64()<retryAt)){return !refs.empty();}loaded=true;retryAt=GetTickCount64()+30000;refs.clear();hiRefs.clear();cells.clear();labels.clear();auto root=bundleRoot();if(root.empty()){return false;}
        for(int i=0;i<pubgmap::mapCount;i++){std::wstring base=root+L"\\maps\\pubg\\";std::string b=pubgmap::readAll(base+L"refs\\"+pubgmap::maps[i].id+L".gray");
            mapfit::Gray g;g.w=g.h=273;if(b.size()!=273*273){continue;}g.p.resize(b.size());for(size_t k=0;k<b.size();k++){g.p[k]=(unsigned char)b[k];}std::string bh=pubgmap::readAll(base+L"refs\\"+pubgmap::maps[i].id+L".hi.gray");if(bh.size()!=819*819){continue;}mapfit::Gray gh;gh.w=gh.h=819;gh.p.resize(bh.size());for(size_t k=0;k<bh.size();k++){gh.p[k]=(unsigned char)bh[k];}
            refs.push_back(mapfit::blur(g,2));hiRefs.push_back(mapfit::blur(gh,1));cells.push_back(pubgmap::maps[i].cells);labels.push_back(pubgmap::readLabels(base+pubgmap::maps[i].id+L".txt"));}
        if(refs.size()!=(size_t)pubgmap::mapCount){refs.clear();pubgNote(i18n::text(L"배그 지도 데이터를 읽지 못함"));return false;}
        return true;}
    void readKeys(){wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){return;}
        std::string ini=pubgmap::readAll(std::wstring(local)+L"\\TslGame\\Saved\\Config\\WindowsNoEditor\\GameUserSettings.ini");
        keys=pubgmap::mapKeys(ini);down.assign(keys.size(),true);}
    // Hide first: stopping waits for the worker. One summary line per opening (frames with a map grid that matched / weakly / not).
    void close(const wchar_t* why=L""){bool was=open;overlay.hide();shown=false;open=false;capture.stop();if(!was){return;}
        {auto n=capture.takeNote();if(!n.empty()){pubgNote(n);}}auto s=capture.read();wchar_t b[256];
        swprintf(b,256,L"map closed (%ls) after %.1f s · ok %d · weak %d · fail %d · no grid %d",why,(GetTickCount64()-openStart)/1000.0,s.nOk,s.nWeak,s.nFail,s.nNoGrid);pubgNote(b);}
    // Every second: PUBG in front starts the fast key timer; anything else stops everything.
    void poll(){
        if(pubgMatchAt!=matchSeen){matchSeen=pubgMatchAt;known=-1;} // a new match may be on another map
        ULONGLONG now=GetTickCount64();if(now>=nextCheck){nextCheck=now+3000;DWORD was=pid;pid=pubgMapOn?pubggame::gamePid():0;if(pid!=was){keys.clear();}} // re-read the map key for each PUBG run
        bool front=pid&&pubggame::foreground(pid);
        if(front&&!timer){if(keys.empty()){readKeys();std::wstring k;for(auto& x:keys){k+=(k.empty()?L"":L", ")+std::to_wstring(x.vk);}pubgNote(i18n::text(L"지도 키 읽음 (가상키 ")+k+L")");}timer=SetTimer(win,77,30,nullptr)!=0;escDown=tabDown=true;down.assign(keys.size(),true);}
        if(!front&&timer){KillTimer(win,77);timer=false;close(L"PUBG not in front");}
        if(!pid&&loaded&&!open)overlay.destroy();
    }
    static bool pressed(int vk){return (GetAsyncKeyState(vk)&0x8000)!=0;}
    void fast(){
        if(!pid||!pubggame::foreground(pid)){KillTimer(win,77);timer=false;close(L"PUBG not in front");return;}
        bool ctrl=pressed(VK_CONTROL),alt=pressed(VK_MENU),shift=pressed(VK_SHIFT);
        for(size_t i=0;i<keys.size();i++){bool d=pressed(keys[i].vk)&&ctrl==keys[i].ctrl&&alt==keys[i].alt&&shift==keys[i].shift;if(d&&!down[i])toggle();down[i]=d;}
        // Esc or Tab (inventory) closes the game's map: stop reading at once instead of waiting for the screen to show it.
        // Tab only when it is not itself a map key.
        bool esc=pressed(VK_ESCAPE),tab=pressed(VK_TAB)&&!alt&&std::none_of(keys.begin(),keys.end(),[](const pubgmap::Key& k){return k.vk==VK_TAB;});
        bool closing=open&&((esc&&!escDown)||(tab&&!tabDown));escDown=esc;tabDown=tab;if(closing){close(esc?L"Esc":L"Tab");return;}
        // L (not bound in PUBG): show/hide the labels while the map is open.
        bool tg=pressed('L')&&!ctrl&&!alt&&!shift;if(tg&&!toggleDown&&open){visible=!visible;saveString(configPath,L"PubgMapVisible",visible?L"1":L"0");shown=false;seenSeq=0;}toggleDown=tg;
        if(!open){return;}
        auto s=capture.read();ULONGLONG now=GetTickCount64();{auto n=capture.takeNote();if(!n.empty()){pubgNote(n);}}{auto n=pubgHires.takeNote();if(!n.empty()){pubgNote(n);}}
        if(known>=0){pubgHires.want(known);capture.setUltra(known,pubgHires.of(known));}
        if(s.seq!=seenSeq||!shown){seenSeq=s.seq;
            if(s.ok&&s.map>=0&&s.map<(int)labels.size()){if(known!=s.map){known=s.map;pubgNote(std::wstring(i18n::text(L"지도 인식: "))+pubgmap::maps[s.map].korean+i18n::text(L" (일치 ")+std::to_wstring(int(s.score*100))+L"%)");}
                // Same placement as on screen: nothing to redraw (a 4K redraw costs the main thread tens of ms).
                double now4[5]={double(s.map),s.ox,s.oy,s.S,visible?1.0:0.0};if(!shown||now4[0]!=drawn[0]||std::fabs(now4[1]-drawn[1])>0.5||std::fabs(now4[2]-drawn[2])>0.5||std::fabs(now4[3]-drawn[3])>0.5||now4[4]!=drawn[4]){
                    overlay.draw(game,labels[s.map],s.ox,s.oy,s.S,s.w,s.h,pubgmap::maps[s.map].korean,visible);std::copy(now4,now4+5,drawn);shown=true;}}
            // A frame that did not match while the map moves is not the map closing: keep the labels 0.4 s.
            else if(shown&&(!s.grid||now-s.lastOk>400)){overlay.hide();shown=false;}}
        if(shown&&!s.ok&&(!s.grid||now-s.lastOk>400)){overlay.hide();shown=false;}
        // Map not seen for 3 s after opening (closed some other way, or the key was typed in chat): stop reading.
        if(now-s.lastOk>3000&&now-openedAt>3000){pubgNote(i18n::text(L"지도 화면이 보이지 않아 표시 중지 · ")+s.status);close(L"not seen 3 s");}
        if(!capture.running()&&now-openedAt>1000)close(L"capture ended");
    }
    // The map key does not blindly flip an open/closed state: the game also closes the map by itself (inventory, Esc, ...) and
    // a flipped state went the wrong way round, so labels were missing on every other opening. Instead the screen decides:
    // map seen just now (labels drawn, or a match within 0.6 s) -> this key closes it, stop at once; otherwise it opens it,
    // start (or keep) reading. The screen stays the safety net (no grid -> hidden at once, 3 s without -> stop).
    void toggle(){
        if(open){auto s=capture.read();ULONGLONG now=GetTickCount64();if(shown||(s.ok&&now-s.lastOk<600)){close(L"map key");return;}openedAt=now;return;}
        if(!pubgMapOn||!loadData()){return;}game=pid?wgc::window(pid):nullptr;if(!game||IsIconic(game)){pubgNote(i18n::text(L"지도 키: 배그 창을 찾지 못함"));return;}
        pubgNote(i18n::text(L"지도 키 · 지도 위치 찾기 시작"));open=true;openedAt=openStart=GetTickCount64();seenSeq=0;capture.start(game,refs,hiRefs,cells,known);
    }
} pubgMap;
#endif
