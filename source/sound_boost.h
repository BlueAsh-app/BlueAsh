#pragma once
// 배그 · 사운드 부스트 (게임 도우미 > 배그, page 19). Only PUBG's sound is processed; no driver, no extra program, nothing
// injected into the game, no game file or memory touched:
//   1. Windows' own per-process loopback (Windows 10 2004+, the same API the old plane-sound alert used) gives PUBG's mix.
//   2. PUBG's audio session is muted with the volume mixer's own switch (ISimpleAudioVolume::SetMute). Process loopback
//      still hands over the full signal when the session is muted (measured on the dev PC: -18 dBFS before and after),
//      so the original and the processed sound never play together.
//   3. sound_dsp.h's chain (EQ, linked compressor, -1 dBFS limiter) runs on it and BlueAsh plays it on the device PUBG's
//      session is on (else the default one) in its own session: Discord, browsers and music are not touched.
// Stopping (switch off, PUBG closed, BlueAsh closed) fades out, then unmutes PUBG. If anything fails or the chain
// reports a bad sample, the output goes silent first, then PUBG is unmuted (its own sound comes back at its own level;
// no volume is ever raised). A crash leaves PUBG muted, never loud: the registry mark SoundBoostMuted makes the next
// BlueAsh start (or the uninstaller, --sound-restore) unmute it.
// Registry: SoundBoost (1 = on), SoundBoostPreset (0 밸런스 / 1 발소리 집중 / 3 사용자 지정; an old 2 reads as 0), SoundBoostSeen (NEW checked),
// SoundBoostCustom (사용자 지정: six gains, 압축 강도, 조용한 소리 키우기, comma-separated).
#include "sound_dsp.h"
#include <audioclient.h>
namespace soundboost {
enum St {Off,Waiting,Applying,Applied,Failed};
std::atomic<int> state{Off};std::mutex mu;std::wstring reason,device;DWORD toastFor=0;
bool on(){return regString(configPath,L"SoundBoost")==L"1";}
// presets 0 밸런스, 1 발소리 집중, 3 사용자 지정 (2, the dropped 총성 완화, reads as 밸런스)
int presetOk(int p){return p==1||p==3?p:0;}
int presetSaved(){std::wstring v=regString(configPath,L"SoundBoostPreset");return v.empty()?1:presetOk(_wtoi(v.c_str()));} // never picked: 발소리 집중 (the recommended card)
const wchar_t* presetName(int i){const wchar_t* n[]={L"밸런스",L"발소리 집중",L"밸런스",L"사용자 지정"};return i18n::text(n[presetOk(i)]);}
// 사용자 지정: read once, kept here; the engine thread picks up a change by its version number (no lock while it processes).
std::mutex cmu;sbdsp::Custom customVal;bool customRead=false,customSaved=false;std::atomic<int> customVer{0};
sbdsp::Custom custom(){std::lock_guard<std::mutex> l(cmu);if(!customRead){customRead=true;customVal=sbdsp::customFrom(sbdsp::preset(0));double v[8];
    std::wstring t=regString(configPath,L"SoundBoostCustom");if(swscanf(t.c_str(),L"%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",v,v+1,v+2,v+3,v+4,v+5,v+6,v+7)==8){for(int k=0;k<6;k++)customVal.g[k]=v[k];customVal.comp=v[6];customVal.lift=v[7];customSaved=true;}}
    return customVal;}
bool customExists(){custom();std::lock_guard<std::mutex> l(cmu);return customSaved;}
// save false while a point is being dragged (heard at once, written to the registry when the drag ends)
void setCustom(const sbdsp::Custom& c,bool save){custom();{std::lock_guard<std::mutex> l(cmu);customVal=c;}customVer++;if(!save)return;
    wchar_t b[160];swprintf(b,160,L"%g,%g,%g,%g,%g,%g,%g,%g",c.g[0],c.g[1],c.g[2],c.g[3],c.g[4],c.g[5],c.comp,c.lift);saveString(configPath,L"SoundBoostCustom",b);std::lock_guard<std::mutex> l(cmu);customSaved=true;}
std::wstring info(std::wstring& dev){std::lock_guard<std::mutex> l(mu);dev=device;return reason;}
void setInfo(const std::wstring& r,const std::wstring* dev=nullptr){std::lock_guard<std::mutex> l(mu);reason=r;if(dev)device=*dev;}

// ---- PUBG's sessions in the volume mixer -------------------------------------------------------------------------
// Test builds: BLUEASH_SOUND_PID names a stand-in process (a tone player) so the real PUBG and the user's own BlueAsh are not involved.
DWORD testPid(){
#ifdef BLUEASH_TT
    if(const char* p=getenv("BLUEASH_SOUND_PID"))return (DWORD)atol(p);
#endif
    return 0;}
bool isPubg(DWORD pid){if(!pid)return false;if(testPid())return pid==testPid();HANDLE ph=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!ph)return false;wchar_t p[MAX_PATH];DWORD n=MAX_PATH;bool r=false;
    if(QueryFullProcessImageNameW(ph,0,p,&n)){const wchar_t* b=wcsrchr(p,L'\\');r=!_wcsicmp(b?b+1:p,L"TslGame.exe");}CloseHandle(ph);return r;}
// Mutes (or unmutes) every PUBG session on every active output. Returns how many it found; dev: the id of the output
// the active one is on (that is where BlueAsh plays the processed sound).
int muteGame(bool mute,std::wstring* dev=nullptr,bool apply=true){ // apply false: only find the sessions and their output
    int found=0;Com<IMMDeviceEnumerator> en;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.out())))return 0;
    Com<IMMDeviceCollection> all;if(FAILED(en->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,all.out())))return 0;UINT n=0;all->GetCount(&n);
    for(UINT i=0;i<n;i++){Com<IMMDevice> d;if(FAILED(all->Item(i,d.out())))continue;Com<IAudioSessionManager2> m;if(FAILED(d->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,(void**)m.out())))continue;
        Com<IAudioSessionEnumerator> se;if(FAILED(m->GetSessionEnumerator(se.out())))continue;int c=0;se->GetCount(&c);
        for(int k=0;k<c;k++){Com<IAudioSessionControl> sc;if(FAILED(se->GetSession(k,sc.out())))continue;Com<IAudioSessionControl2> s2;if(FAILED(sc->QueryInterface(__uuidof(IAudioSessionControl2),(void**)s2.out())))continue;
            DWORD pid=0;s2->GetProcessId(&pid);if(!isPubg(pid))continue;Com<ISimpleAudioVolume> v;if(FAILED(sc->QueryInterface(__uuidof(ISimpleAudioVolume),(void**)v.out())))continue;
            bool skip=false;
#ifdef BLUEASH_TT
            skip=mute&&getenv("BLUEASH_SOUND_NOMUTE"); // test builds: see whether loopback carries a sound that is not muted
#endif
            if(skip||!apply||SUCCEEDED(v->SetMute(mute,nullptr))){found++;AudioSessionState st=AudioSessionStateInactive;sc->GetState(&st);
                if(dev&&(dev->empty()||st==AudioSessionStateActive)){LPWSTR id=nullptr;if(SUCCEEDED(d->GetId(&id))&&id){*dev=id;CoTaskMemFree(id);}}}}}
    return found;}
// After a crash: the mark says PUBG may still be muted by BlueAsh.
void restoreMark(){if(regString(configPath,L"SoundBoostMuted")!=L"1")return;muteGame(false);saveString(configPath,L"SoundBoostMuted",L"0");}

// ---- process loopback activation (as the old plane-sound alert did it) ---------------------------------------------
struct LoopParams {int type=1;DWORD pid=0;int mode=0;}; // PROCESS_LOOPBACK, include the process tree
static_assert(sizeof(LoopParams)==12,"process loopback ABI");
struct Done final:IActivateAudioInterfaceCompletionHandler,IAgileObject {
    std::atomic<ULONG> refs{1};IUnknown* marshaler=nullptr;HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);IAudioClient* client=nullptr;HRESULT result=E_PENDING;LoopParams params;PROPVARIANT var={};
    explicit Done(DWORD pid){params.pid=pid;var.vt=VT_BLOB;var.blob.cbSize=sizeof(params);var.blob.pBlobData=(BYTE*)&params;}
    ~Done(){if(client)client->Release();if(marshaler)marshaler->Release();if(done)CloseHandle(done);}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release() override{ULONG r=--refs;if(!r)delete this;return r;}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;
        if(id==__uuidof(IUnknown)||id==__uuidof(IActivateAudioInterfaceCompletionHandler)){*out=static_cast<IActivateAudioInterfaceCompletionHandler*>(this);AddRef();return S_OK;}
        if(id==__uuidof(IAgileObject)){*out=static_cast<IAgileObject*>(this);AddRef();return S_OK;}
        if(id==IID_IMarshal&&marshaler){return marshaler->QueryInterface(id,out);}return E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override{HRESULT a=E_FAIL;IUnknown* u=nullptr;result=op->GetActivateResult(&a,&u);
        if(SUCCEEDED(result)){result=a;}if(SUCCEEDED(result)){result=u?u->QueryInterface(__uuidof(IAudioClient),(void**)&client):E_NOINTERFACE;}if(u){u->Release();}SetEvent(done);return S_OK;}
};
WAVEFORMATEX floatFormat(){WAVEFORMATEX f={};f.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=32;f.nBlockAlign=8;f.nAvgBytesPerSec=48000*8;return f;}

// ---- the engine thread ----------------------------------------------------------------------------------------------
std::thread worker;HANDLE stopEvent=nullptr;std::atomic<bool> running{false};std::atomic<int> wantPreset{0};DWORD enginePid=0,lastPid=0;ULONGLONG failedAt=0;int failures=0;
std::wstring deviceName(IMMDevice* d){Com<IPropertyStore> ps;std::wstring n;if(SUCCEEDED(d->OpenPropertyStore(STGM_READ,ps.out()))){
    static const PROPERTYKEY friendly={{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};PROPVARIANT v;PropVariantInit(&v);if(SUCCEEDED(ps->GetValue(friendly,&v))&&v.vt==VT_LPWSTR&&v.pwszVal)n=v.pwszVal;PropVariantClear(&v);}return n;}
void run(DWORD pid){
    HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);std::wstring fail;
    {Com<IAudioClient> cap,ren;Com<IAudioCaptureClient> cc;Com<IAudioRenderClient> rc;HANDLE capEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);bool muted=false;
     std::wstring devId;Com<IMMDeviceEnumerator> en;sbdsp::Chain chain;int seenVer=customVer;chain.setCustom(sbdsp::customPreset(custom()));chain.setPreset(wantPreset);UINT32 renFrames=0;
     auto openRender=[&]()->HRESULT{if(ren.p){ren->Stop();ren.p->Release();ren.p=nullptr;}if(rc.p){rc.p->Release();rc.p=nullptr;}
        std::wstring want;muteGame(muted,&want,muted);Com<IMMDevice> d;HRESULT hr=!want.empty()?en->GetDevice(want.c_str(),d.out()):E_FAIL;if(FAILED(hr))hr=en->GetDefaultAudioEndpoint(eRender,eConsole,d.out());if(FAILED(hr))return hr;
        hr=d->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,(void**)ren.out());if(FAILED(hr))return hr;WAVEFORMATEX f=floatFormat();
        hr=ren->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,400000,0,&f,nullptr);if(FAILED(hr))return hr;
        hr=ren->GetBufferSize(&renFrames);if(SUCCEEDED(hr))hr=ren->GetService(__uuidof(IAudioRenderClient),(void**)rc.out());if(SUCCEEDED(hr))hr=ren->Start();
        if(SUCCEEDED(hr)){LPWSTR id=nullptr;if(SUCCEEDED(d->GetId(&id))&&id){devId=id;CoTaskMemFree(id);}std::wstring nm=deviceName(d.p);setInfo(L"",&nm);}return hr;};
     HRESULT hr=SUCCEEDED(co)&&capEvent?CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.out()):E_FAIL;
     // 1. PUBG's loopback
     if(SUCCEEDED(hr)){Com<Done> cb;cb.p=new Done(pid);Com<IActivateAudioInterfaceAsyncOperation> op;hr=cb->done?CoCreateFreeThreadedMarshaler(static_cast<IActivateAudioInterfaceCompletionHandler*>(cb.p),&cb->marshaler):E_OUTOFMEMORY;
        HMODULE dll=LoadLibraryExW(L"Mmdevapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);using Act=HRESULT(WINAPI*)(LPCWSTR,REFIID,PROPVARIANT*,IActivateAudioInterfaceCompletionHandler*,IActivateAudioInterfaceAsyncOperation**);
        Act act=nullptr;if(dll){FARPROC p=GetProcAddress(dll,"ActivateAudioInterfaceAsync");memcpy(&act,&p,sizeof(act));}if(!act){hr=HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);fail=i18n::text(L"이 Windows 버전은 배그 소리만 따로 받을 수 없습니다 (Windows 10 2004 이상 필요)");}
        if(SUCCEEDED(hr))hr=act(L"VAD\\Process_Loopback",__uuidof(IAudioClient),&cb->var,cb.p,op.out());
        if(SUCCEEDED(hr)){HANDLE w[]={stopEvent,cb->done};DWORD r=WaitForMultipleObjects(2,w,FALSE,10000);hr=r==WAIT_OBJECT_0+1?cb->result:HRESULT_FROM_WIN32(r==WAIT_TIMEOUT?ERROR_TIMEOUT:ERROR_CANCELLED);}
        if(SUCCEEDED(hr)){cap.p=cb->client;cb->client=nullptr;WAVEFORMATEX f=floatFormat();
            hr=cap->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,200000,0,&f,nullptr);}
        if(SUCCEEDED(hr)){hr=cap->GetService(__uuidof(IAudioCaptureClient),(void**)cc.out());}if(SUCCEEDED(hr)){hr=cap->SetEventHandle(capEvent);}if(SUCCEEDED(hr)){hr=cap->Start();}
        if(FAILED(hr)&&fail.empty())fail=i18n::text(L"배그 소리를 받지 못했습니다")+(L" ("+hrtext(hr)+L")");}
     // 2. the output, then 3. PUBG muted (only now: the processed sound is ready to take over)
     if(SUCCEEDED(hr)){hr=openRender();if(FAILED(hr))fail=i18n::text(L"출력 장치를 열지 못했습니다")+(L" ("+hrtext(hr)+L")");}
     ULONGLONG lastCheck=0,reopenWindow=GetTickCount64(),stopAt=0;int reopens=0;std::vector<float> buf;bool stopping=false;
     ULONGLONG heardAt=0,mutedAt=0;double after=0;bool verifying=false;
     if(SUCCEEDED(hr)){state=Applying;} // the chain stays silent (fade 0) until PUBG is muted: never both sounds at once
     while(SUCCEEDED(hr)){
        HANDLE w[]={stopEvent,capEvent};DWORD r=WaitForMultipleObjects(2,w,FALSE,50);
        if(r==WAIT_OBJECT_0&&!stopping){stopping=true;stopAt=GetTickCount64();chain.enable(false);}
        if(int v=customVer;v!=seenVer){seenVer=v;chain.setCustom(sbdsp::customPreset(custom()));}
        if(chain.presetNow()!=wantPreset)chain.setPreset(wantPreset);
        ULONGLONG now=GetTickCount64();
        // after muting, the loopback must still carry sound (measured 0.8-1.6 s after, past the mute's own fade); on Windows builds that silence a muted session's loopback, give the sound back
        if(verifying&&now-mutedAt>1600){verifying=false;if(after<=0){fail=i18n::text(L"이 Windows에서는 음소거한 배그 소리를 받을 수 없어 적용하지 않았습니다");break;}}
        if(now-lastCheck>=1000){lastCheck=now;std::wstring d;int n=stopping?0:muteGame(muted,&d,muted);
            if(n>0&&!muted&&!stopping&&heardAt&&now-heardAt<1500){saveString(configPath,L"SoundBoostMuted",L"1");muteGame(true);muted=true;mutedAt=now;verifying=true;after=0;chain.enable(true);} // mute only once PUBG's sound is seen, so the check below means something
            if(n>0&&muted&&!verifying&&state==Applying){state=Applied;setInfo(L"");}
            if(n==0&&!stopping&&state==Applied)state=Applying; // PUBG closed its stream (it may open another): keep waiting
            if(!d.empty()&&d!=devId&&!stopping){if(now-reopenWindow>60000){reopenWindow=now;reopens=0;}if(++reopens>5){fail=i18n::text(L"출력 장치가 계속 바뀌어 멈췄습니다");break;}
                if(FAILED(openRender())){fail=i18n::text(L"바뀐 출력 장치를 열지 못했습니다");break;}chain=sbdsp::Chain();chain.setCustom(sbdsp::customPreset(custom()));chain.setPreset(wantPreset);chain.enable(muted);}}
        // read what PUBG played, process, write it out (late frames are dropped so the delay stays near 20-40 ms)
        UINT32 next=0;hr=cc->GetNextPacketSize(&next);
        while(SUCCEEDED(hr)&&next){BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;hr=cc->GetBuffer(&data,&frames,&flags,nullptr,nullptr);if(FAILED(hr))break;
            buf.assign(size_t(frames)*2,0.f);if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)&&data)memcpy(buf.data(),data,size_t(frames)*8);cc->ReleaseBuffer(frames);
            {double e=0;for(float v:buf)e+=double(v)*v;if(!muted&&e>frames*1e-9)heardAt=now;if(verifying&&now-mutedAt>800)after+=e; /* the mute fades out over up to ~1 s: judge the tail */
#ifdef BLUEASH_TT
             static double ce=0,cn=0;static ULONGLONG lt=0;ce+=e;cn+=frames;if(const char* lg=getenv("BLUEASH_SOUND_LOG"))if(now-lt>=500){lt=now;if(FILE* f=fopen(lg,"a")){fprintf(f,"%llu muted=%d state=%d in=%.1fdB silentflag=%d\n",now,muted?1:0,state.load(),cn?10*std::log10(ce/cn/2+1e-14):-140,(flags&AUDCLNT_BUFFERFLAGS_SILENT)?1:0);fclose(f);}ce=cn=0;}
#endif
            }
            if(!chain.process(buf.data(),frames)){fail=i18n::text(L"소리 처리 중 이상 값이 감지되어 원래 소리로 되돌렸습니다");hr=E_FAIL;break;}
            UINT32 pad=0;HRESULT rh=ren->GetCurrentPadding(&pad);
            if(SUCCEEDED(rh)&&pad<renFrames&&pad<48000*40/1000){UINT32 room=std::min(frames,renFrames-pad);BYTE* out=nullptr;rh=rc->GetBuffer(room,&out);if(SUCCEEDED(rh)){memcpy(out,buf.data(),size_t(room)*8);rh=rc->ReleaseBuffer(room,0);}}
            if(rh==AUDCLNT_E_DEVICE_INVALIDATED){if(FAILED(openRender())){fail=i18n::text(L"출력 장치가 사라졌습니다");hr=E_FAIL;break;}chain=sbdsp::Chain();chain.setCustom(sbdsp::customPreset(custom()));chain.setPreset(wantPreset);chain.enable(muted);}
            if(SUCCEEDED(hr))hr=cc->GetNextPacketSize(&next);}
        if(FAILED(hr)&&fail.empty())fail=i18n::text(L"배그 소리 연결이 끊겼습니다");
        if(stopping&&(chain.level()<=0||GetTickCount64()-stopAt>300))break;} /* PUBG may be silent (no packets): do not wait for the fade forever */
     // silence first, then PUBG's own sound back
     if(ren.p){ren->Stop();}if(cap.p){cap->Stop();}
     if(muted||regString(configPath,L"SoundBoostMuted")==L"1"){muteGame(false);saveString(configPath,L"SoundBoostMuted",L"0");}
     if(capEvent)CloseHandle(capEvent);}
    if(!fail.empty()){setInfo(fail);state=Failed;}
    if(SUCCEEDED(co)){CoUninitialize();}running=false;}
void stop(){if(worker.joinable()){if(stopEvent)SetEvent(stopEvent);worker.join();}if(stopEvent){CloseHandle(stopEvent);stopEvent=nullptr;}enginePid=0;}
void start(DWORD pid){stop();stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!stopEvent)return;wantPreset=presetSaved();enginePid=pid;running=true;state=Applying;setInfo(L"");
    try{worker=std::thread(run,pid);}catch(...){running=false;state=Failed;setInfo(i18n::text(L"처리를 시작하지 못했습니다"));}}

// ---- the toast: 'activated', once per PUBG run, top right of PUBG, 20 s (PUBG is still loading), fades; click-through, never takes focus
struct Toast {HWND w=nullptr;HDC dc=nullptr;HBITMAP bmp=nullptr;HGDIOBJ old=nullptr;SIZE size={};POINT at={};ULONGLONG shownAt=0,hold=20000; // hold: how long it stays (ms)
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM wp,LPARAM lp);
    void frame(){ULONGLONG t=GetTickCount64()-shownAt;double a=t<220?t/220.0:t+300<hold?1:t<hold?(hold-t)/300.0:0;if(a<=0){ShowWindow(w,SW_HIDE);KillTimer(w,1);return;}
        double e=1-std::pow(1-std::min(1.0,t/220.0),3);POINT p={at.x,at.y-(int)std::lround((1-e)*10)},src={0,0};BLENDFUNCTION bf={AC_SRC_OVER,0,(BYTE)std::lround(255*a),AC_SRC_ALPHA};
        HDC screen=GetDC(nullptr);UpdateLayeredWindow(w,screen,&p,&size,dc,&src,0,&bf,ULW_ALPHA);ReleaseDC(nullptr,screen);}
    void show(HWND game,const std::wstring& l1,const std::wstring& l2){
        ServerBadge::Aware aware;RECT rc={};if(!GetClientRect(game,&rc)||rc.bottom<200)return;POINT org={0,0};ClientToScreen(game,&org);double k=std::max(.8,rc.bottom/1080.0);auto K=[&](double v){return (int)std::lround(v*k);};
        if(!w){WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshSoundToast";RegisterClassW(&wc);
            w=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,wc.hInstance,nullptr);if(!w)return;}
        HDC screen=GetDC(nullptr);if(!dc)dc=CreateCompatibleDC(screen);if(bmp){SelectObject(dc,old);DeleteObject(bmp);bmp=nullptr;}
        HFONT big=CreateFontW(-K(15),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily()),small=CreateFontW(-K(12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,i18n::fontFamily());
        SIZE s1={},s2={};HGDIOBJ of=SelectObject(dc,big);GetTextExtentPoint32W(dc,l1.c_str(),(int)l1.size(),&s1);SelectObject(dc,small);GetTextExtentPoint32W(dc,l2.c_str(),(int)l2.size(),&s2);SelectObject(dc,of);
        int pad=K(16),icon=K(28),gap=K(12),wd=pad*2+icon+gap+std::max(s1.cx,s2.cx),ht=K(13)*2+s1.cy+K(3)+s2.cy;size={wd,ht};
        BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=wd;bi.bmiHeader.biHeight=-ht;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;void* bits=nullptr;bmp=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
        if(bmp&&painting::token){old=SelectObject(dc,bmp);memset(bits,0,size_t(wd)*ht*4);
            {Gdiplus::Bitmap canvas(wd,ht,wd*4,PixelFormat32bppPARGB,(BYTE*)bits);Gdiplus::Graphics g(&canvas);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::GraphicsPath p;painting::path(p,{0,0,wd,ht},K(14));Gdiplus::SolidBrush fill(Gdiplus::Color(236,10,17,36));g.FillPath(&fill,&p);Gdiplus::Pen edge(Gdiplus::Color(170,74,112,224),(float)std::max(1.0,k));g.DrawPath(&edge,&p);
                float cx=pad+icon/2.f,cy=ht/2.f,r=icon/2.f;Gdiplus::SolidBrush b(Gdiplus::Color(255,52,92,255));g.FillEllipse(&b,cx-r,cy-r,r*2,r*2);
                // a small speaker with two waves
                Gdiplus::SolidBrush wht(Gdiplus::Color(255,255,255,255));Gdiplus::PointF spk[]={{cx-r*.5f,cy-r*.18f},{cx-r*.25f,cy-r*.18f},{cx,cy-r*.42f},{cx,cy+r*.42f},{cx-r*.25f,cy+r*.18f},{cx-r*.5f,cy+r*.18f}};g.FillPolygon(&wht,spk,6);
                Gdiplus::Pen wv(Gdiplus::Color(255,255,255,255),(float)(1.6*k));painting::roundCaps(wv);g.DrawArc(&wv,cx-r*.3f,cy-r*.3f,r*.6f,r*.6f,-50.f,100.f);g.DrawArc(&wv,cx-r*.55f,cy-r*.55f,r*1.1f,r*1.1f,-50.f,100.f);}
            void* mbits=nullptr;HBITMAP mdib=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&mbits,nullptr,0);
            if(mdib){HDC mdc=CreateCompatibleDC(screen);HGDIOBJ mob=SelectObject(mdc,mdib);SetBkMode(mdc,TRANSPARENT);unsigned char* px=(unsigned char*)bits;int tx=pad+icon+gap;
                auto text=[&](HFONT ft,const std::wstring& s,int y,COLORREF c){memset(mbits,0,size_t(wd)*ht*4);HGDIOBJ o=SelectObject(mdc,ft);SetTextColor(mdc,RGB(255,255,255));TextOutW(mdc,tx,y,s.c_str(),(int)s.size());SelectObject(mdc,o);GdiFlush();
                    const unsigned char* mk=(const unsigned char*)mbits;for(size_t i=0;i<size_t(wd)*ht;i++){double a=std::max(mk[i*4],std::max(mk[i*4+1],mk[i*4+2]))/255.0;if(a<=0)continue;unsigned char* q=px+i*4;
                        q[0]=(unsigned char)(q[0]*(1-a)+GetBValue(c)*a);q[1]=(unsigned char)(q[1]*(1-a)+GetGValue(c)*a);q[2]=(unsigned char)(q[2]*(1-a)+GetRValue(c)*a);q[3]=(unsigned char)std::max<double>(q[3],255*a);}};
                text(big,l1,K(13),RGB(244,247,255));text(small,l2,K(13)+s1.cy+K(3),RGB(160,176,210));SelectObject(mdc,mob);DeleteDC(mdc);DeleteObject(mdib);}
#ifdef BLUEASH_TT
            if(const char* dump=getenv("BLUEASH_SOUND_DUMP")){FILE* f=fopen(dump,"wb");if(f){int row=(wd*3+3)&~3;BITMAPFILEHEADER fh={};BITMAPINFOHEADER ih={};ih.biSize=sizeof(ih);ih.biWidth=wd;ih.biHeight=-ht;ih.biPlanes=1;ih.biBitCount=24;fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+row*ht;fwrite(&fh,sizeof(fh),1,f);fwrite(&ih,sizeof(ih),1,f);std::vector<unsigned char> rr(row,0);const unsigned char* q=(const unsigned char*)bits;for(int y=0;y<ht;y++){for(int x=0;x<wd;x++){const unsigned char* p=q+(size_t(y)*wd+x)*4;int a=p[3];rr[x*3]=(unsigned char)(p[0]+(60*(255-a))/255);rr[x*3+1]=(unsigned char)(p[1]+(80*(255-a))/255);rr[x*3+2]=(unsigned char)(p[2]+(70*(255-a))/255);}fwrite(rr.data(),1,row,f);}fclose(f);}}
#endif
            at={org.x+rc.right-wd-K(24),org.y+K(20)};shownAt=GetTickCount64();frame();SetWindowPos(w,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);SetTimer(w,1,16,nullptr);}
        DeleteObject(big);DeleteObject(small);ReleaseDC(nullptr,screen);}
};
Toast toast;
LRESULT CALLBACK Toast::proc(HWND h,UINT m,WPARAM wp,LPARAM lp){if(m==WM_NCHITTEST)return HTTRANSPARENT;if(m==WM_TIMER){ServerBadge::Aware a;toast.frame();return 0;}return DefWindowProcW(h,m,wp,lp);}

// ---- 녹음 (temporary, for the coming preset preview): Home while PUBG runs starts recording PUBG's own sound, Home again (or
// 60 s) stops and saves it. The capture is the same process loopback the engine uses, taken before any processing, so it is
// PUBG's original sound whether 사운드 부스트 is on or off (a muted session still hands over its full signal). Files stay on
// this PC: %LOCALAPPDATA%\BlueAsh\sound-samples\rec-<date>-<time>.wav (48 kHz float stereo), beside the folders 발소리 and 총소리
// the player sorts them into. The key is only read (GetAsyncKeyState), never taken from the game.
namespace rec{
std::atomic<bool> recording{false},quit{false};std::atomic<DWORD> gamePid{0};std::thread keys,worker;
// the sorted samples' folders (0 footsteps, 1 shots); the names are on disk, not translated
inline const wchar_t* subName(int kind){return kind?L"\\총소리":L"\\발소리";}
// the player's own samples folder, not created: the Home recorder runs only where it already exists (the developer's PC)
std::wstring samples(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);return std::wstring(d)+dataSub()+L"\\sound-samples";}
std::wstring folder(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+dataSub();CreateDirectoryW(r.c_str(),nullptr);
    r+=L"\\sound-samples";CreateDirectoryW(r.c_str(),nullptr);for(int k=0;k<2;k++)CreateDirectoryW((r+subName(k)).c_str(),nullptr);return r;}
// the WAV header for n float stereo frames at 48 kHz (written first with 0, fixed when the recording ends)
void header(FILE* f,uint32_t frames){uint32_t data=frames*8,riff=36+data;uint16_t fmt=3,ch=2,bits=32,align=8;uint32_t rate=48000,bps=48000*8,sz=16;
    fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&sz,4,1,f);fwrite(&fmt,2,1,f);fwrite(&ch,2,1,f);fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&data,4,1,f);}
void run(DWORD pid){HRESULT co=CoInitializeEx(nullptr,COINIT_MULTITHREADED);uint32_t frames=0;std::wstring path;bool ok=false;
    {HANDLE ev=CreateEventW(nullptr,FALSE,FALSE,nullptr),stopEv=CreateEventW(nullptr,TRUE,FALSE,nullptr);Com<IAudioClient> cap;Com<IAudioCaptureClient> cc;
     Com<Done> cb;cb.p=new Done(pid);Com<IActivateAudioInterfaceAsyncOperation> op;HRESULT hr=ev&&stopEv&&cb->done?CoCreateFreeThreadedMarshaler(static_cast<IActivateAudioInterfaceCompletionHandler*>(cb.p),&cb->marshaler):E_FAIL;
     HMODULE dll=LoadLibraryExW(L"Mmdevapi.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);using Act=HRESULT(WINAPI*)(LPCWSTR,REFIID,PROPVARIANT*,IActivateAudioInterfaceCompletionHandler*,IActivateAudioInterfaceAsyncOperation**);
     Act act=nullptr;if(dll){FARPROC p=GetProcAddress(dll,"ActivateAudioInterfaceAsync");memcpy(&act,&p,sizeof(act));}if(!act)hr=E_NOTIMPL;
     if(SUCCEEDED(hr))hr=act(L"VAD\\Process_Loopback",__uuidof(IAudioClient),&cb->var,cb.p,op.out());
     if(SUCCEEDED(hr)){DWORD w=WaitForSingleObject(cb->done,10000);hr=w==WAIT_OBJECT_0?cb->result:HRESULT_FROM_WIN32(ERROR_TIMEOUT);}
     if(SUCCEEDED(hr)){cap.p=cb->client;cb->client=nullptr;WAVEFORMATEX f=floatFormat();hr=cap->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|AUDCLNT_STREAMFLAGS_EVENTCALLBACK|AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,200000,0,&f,nullptr);}
     if(SUCCEEDED(hr)){hr=cap->GetService(__uuidof(IAudioCaptureClient),(void**)cc.out());}if(SUCCEEDED(hr)){hr=cap->SetEventHandle(ev);}if(SUCCEEDED(hr)){hr=cap->Start();}
     FILE* f=nullptr;if(SUCCEEDED(hr)){SYSTEMTIME t;GetLocalTime(&t);wchar_t n[64];swprintf(n,64,L"\\rec-%04d%02d%02d-%02d%02d%02d.wav",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);path=folder()+n;f=_wfopen(path.c_str(),L"wb");if(f)header(f,0);}
     ULONGLONG start=GetTickCount64();
     while(f&&recording&&!quit&&GetTickCount64()-start<60000){WaitForSingleObject(ev,50);UINT32 next=0;
        while(SUCCEEDED(cc->GetNextPacketSize(&next))&&next){BYTE* data=nullptr;UINT32 n=0;DWORD fl=0;if(FAILED(cc->GetBuffer(&data,&n,&fl,nullptr,nullptr)))break;
            if(fl&AUDCLNT_BUFFERFLAGS_SILENT||!data){std::vector<float> z(size_t(n)*2,0.f);fwrite(z.data(),8,n,f);}else fwrite(data,8,n,f);frames+=n;cc->ReleaseBuffer(n);}}
     if(cap.p){cap->Stop();}if(f){fseek(f,0,SEEK_SET);header(f,frames);fclose(f);ok=frames>48000/4;if(!ok)DeleteFileW(path.c_str());} /* under 0.25 s: nothing worth keeping */
     if(ev){CloseHandle(ev);}if(stopEv){CloseHandle(stopEv);}}
    recording=false;if(SUCCEEDED(co))CoUninitialize();
    if(win)PostMessageW(win,WM_APP+250,ok?2:3,(LPARAM)(frames/48000));}
void stopNow(){quit=true;recording=false;if(worker.joinable())worker.join();if(keys.joinable())keys.join();}
// polled every 30 ms on its own thread (never the UI thread); a press toggles recording while PUBG runs
void watch(){bool was=false;while(!quit){bool down=(GetAsyncKeyState(VK_HOME)&0x8000)!=0;DWORD pid=gamePid;
    if(down&&!was&&pid){if(!recording){if(worker.joinable())worker.join();recording=true;try{worker=std::thread(run,pid);if(win)PostMessageW(win,WM_APP+250,1,0);}catch(...){recording=false;}}else recording=false;}
    was=down;Sleep(30);}}
// The Home-key recorder is off (2.33.0): the preview's recordings ship in the data pack. Turn it back on only for
// making new samples, by starting watch() here again.
void poll(DWORD pid){gamePid=pid;if(!pid&&recording)recording=false;}
// the toast on PUBG: 1 started, 2 saved (lp = seconds), 3 failed
void toastFor(WPARAM what,LPARAM sec){DWORD pid=gamePid;HWND g=pid?wgc::window(pid):nullptr;if(!g)return;toast.hold=what==1?2500:4500;wchar_t b[160];
    if(what==2)swprintf(b,160,i18n::text(L"%d초 · BlueAsh 폴더의 sound-samples에 저장했습니다"),(int)sec);
    toast.show(g,i18n::text(what==1?L"녹음을 시작했습니다":what==2?L"녹음을 저장했습니다":L"녹음하지 못했습니다"),what==2?std::wstring(b):std::wstring(i18n::text(what==1?L"Home 키를 한 번 더 누르면 저장합니다 (최대 60초)":L"배그 소리를 받지 못했습니다")));}
}

// ---- every second from the main window ------------------------------------------------------------------------------
void (*changed)()=nullptr; // the open page refreshes its status line
void poll(){static bool first=true;if(first){first=false;restoreMark();} /* a crash left PUBG muted: give its sound back first */
    rec::poll(testPid()?testPid():pubggame::gamePid());
    int before=state;
    if(!on()){if(running||worker.joinable())stop();state=Off;toastFor=0;failures=0;}
    else{DWORD pid=testPid()?testPid():pubggame::gamePid();
        if(!pid){if(running||worker.joinable())stop();state=Waiting;failures=0;toastFor=0;}
        if(pid&&pid!=lastPid){lastPid=pid;failures=0;failedAt=0;if(state==Failed)state=Waiting;} // a new PUBG run starts clean
        if(pid&&!running){if(worker.joinable())stop();ULONGLONG now=GetTickCount64();
            // a failure is retried after 30 s, three times per PUBG run; then it waits for the switch or the next run
            if(state!=Failed)start(pid);else if(!failedAt)failedAt=now;else if(failures<3&&now-failedAt>30000){failures++;failedAt=0;start(pid);}}
        // the toast: the first time this PUBG run is really processed, once PUBG is the front window
        if(state==Applied&&toastFor!=pid&&(testPid()||pubggame::foreground(pid))){toastFor=pid;HWND g=wgc::window(pid);if(g){toast.hold=20000;}if(g)toast.show(g,i18n::text(L"블루애쉬 사운드 부스트가 활성화되었습니다."),std::wstring(presetName(wantPreset))+L" · "+i18n::text(L"배그에만 적용"));}}
    if(before!=state&&changed)changed();}
void setPreset(int i){i=presetOk(i);saveString(configPath,L"SoundBoostPreset",std::to_wstring(i));wantPreset=i;}
void setOn(bool v){saveString(configPath,L"SoundBoost",v?L"1":L"0");failures=0;if(state==Failed)state=Off;poll();}
void shutdown(){stop();rec::stopNow();restoreMark();}
// 음향 강도 이퀄라이제이션 (Loudness Equalization: 스피커 속성 > 고급 기능) on the default output. Microsoft's own enhancement keeps
// its on/off in the device's effect store as {9c119480-ddc2-4954-a150-5bd240d454ad},7 (VT_BOOL; read back matching the checked box
// on the dev PC); it is read and written through the policy interface the Sound control panel itself uses (no administrator rights
// needed). Not set = off. Returns 1 on, 0 off, -1 unknown.
struct IPolicyConfig:IUnknown{virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR,void**)=0;virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR,INT,void**)=0;virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR)=0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR,void*,void*)=0;virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR,INT,PINT64,PINT64)=0;virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR,PINT64)=0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR,void*)=0;virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR,void*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR,BOOL,const PROPERTYKEY&,PROPVARIANT*)=0;virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR,BOOL,const PROPERTYKEY&,PROPVARIANT*)=0;};
int loudness(const bool* set=nullptr){static const PROPERTYKEY key={{0x9c119480,0xddc2,0x4954,{0xa1,0x50,0x5b,0xd2,0x40,0xd4,0x54,0xad}},7};
    static const CLSID cls={0x870af99c,0x171d,0x4f9e,{0xaf,0x0d,0xe6,0x3d,0xf4,0x0c,0x2b,0xc9}};static const IID iid={0xf8679f50,0x850a,0x41cf,{0x9c,0x72,0x43,0x0f,0x29,0x02,0x90,0xc8}};
    int r=-1;Com<IMMDeviceEnumerator> en;Com<IMMDevice> d;LPWSTR id=nullptr;
    if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)en.out()))||FAILED(en->GetDefaultAudioEndpoint(eRender,eConsole,d.out()))||FAILED(d->GetId(&id))||!id)return -1;
    Com<IPolicyConfig> pc;if(SUCCEEDED(CoCreateInstance(cls,nullptr,CLSCTX_ALL,iid,(void**)pc.out()))){
        if(set){PROPVARIANT v;PropVariantInit(&v);v.vt=VT_BOOL;v.boolVal=*set?VARIANT_TRUE:VARIANT_FALSE;pc->SetPropertyValue(id,TRUE,key,&v);}
        PROPVARIANT g;PropVariantInit(&g);if(SUCCEEDED(pc->GetPropertyValue(id,TRUE,key,&g)))r=g.vt==VT_BOOL&&g.boolVal?1:0;PropVariantClear(&g);}
    CoTaskMemFree(id);return r;}
const wchar_t* stateText(){switch(state.load()){case Waiting:return i18n::text(L"배그 실행 대기");case Applying:return i18n::text(L"적용 중");case Applied:return i18n::text(L"적용됨");case Failed:return i18n::text(L"적용 실패");default:return i18n::text(L"꺼짐");}}
}
