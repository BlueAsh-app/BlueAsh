#pragma once
// 사운드 부스트 · 미리 듣기: the player's own recordings (sound-samples\발소리 and \총소리, the first WAV in each) played from
// one random direction and distance at a time, through the preset picked on the page, so presets can be heard without
// starting PUBG. A recording of the player's own footstep or shot is dry and centred, so the direction added here is the
// only one in it. Direction: the far ear hears the sound up to 0.66 ms later, quieter and duller (head shadow); from
// behind both ears are duller. Distance: quieter, less treble (air) and a few late echoes.
// shortcut: a simple head model, not an HRTF; enough to hear left/right, front/back and near/far. Use an HRTF if users
// say front and back are hard to tell apart.
#include <mmsystem.h>
#include <random>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <functional>
namespace sbprev{
// mono float at 48 kHz from a WAV file (PCM 16/24/32 bit or float, any channel count, at most 20 s); empty if unreadable
inline std::vector<float> readWav(const std::wstring& path){
    std::vector<float> out;FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return out;std::vector<uint8_t> b;
    fseek(f,0,SEEK_END);long n=ftell(f);fseek(f,0,SEEK_SET);if(n>12&&n<64*1024*1024){b.resize((size_t)n);if(fread(b.data(),1,b.size(),f)!=b.size())b.clear();}fclose(f);
    if(b.size()<12||memcmp(b.data(),"RIFF",4)||memcmp(b.data()+8,"WAVE",4))return out;
    auto u16=[&](size_t p){return (unsigned)(b[p]|b[p+1]<<8);};auto u32=[&](size_t p){return (uint32_t)b[p]|(uint32_t)b[p+1]<<8|(uint32_t)b[p+2]<<16|(uint32_t)b[p+3]<<24;};
    unsigned tag=0,ch=0,bits=0;uint32_t rate=0;size_t data=0,len=0;
    for(size_t p=12;p+8<=b.size();){uint32_t sz=u32(p+4);size_t body=p+8,end=std::min(b.size(),body+(size_t)sz);
        if(!memcmp(b.data()+p,"fmt ",4)&&end-body>=16){tag=u16(body);ch=u16(body+2);rate=u32(body+4);bits=u16(body+14);if(tag==0xFFFE&&end-body>=26)tag=u16(body+24);}
        else if(!memcmp(b.data()+p,"data",4)){data=body;len=end-body;}
        p=body+(size_t)sz+(sz&1);}
    unsigned bytes=bits/8;bool pcm=tag==1&&(bits==16||bits==24||bits==32),flt=tag==3&&bits==32;
    if(!data||!ch||ch>8||rate<8000||rate>192000||!(pcm||flt))return out;
    size_t frames=std::min<size_t>(len/(ch*bytes),(size_t)rate*20);if(!frames)return out;std::vector<float> m(frames);
    for(size_t i=0;i<frames;i++){double s=0;for(unsigned c=0;c<ch;c++){const uint8_t* q=b.data()+data+(i*ch+c)*bytes;double v;
            if(flt){float x;memcpy(&x,q,4);v=std::isfinite(x)?x:0;}
            else if(bits==16)v=(int16_t)(q[0]|q[1]<<8)/32768.0;
            else if(bits==24){int32_t x=q[0]|q[1]<<8|q[2]<<16;if(x&0x800000)x-=0x1000000;v=x/8388608.0;}
            else{int32_t x;memcpy(&x,q,4);v=x/2147483648.0;}
            s+=v;}
        m[i]=(float)(s/ch);}
    if(rate==48000)return m;
    size_t n48=(size_t)((double)frames*48000/rate);out.resize(n48);
    for(size_t i=0;i<n48;i++){double t=(double)i*rate/48000;size_t k=(size_t)t;double fr=t-k;float a=m[std::min(k,frames-1)],c=m[std::min(k+1,frames-1)];out[i]=(float)(a+(c-a)*fr);}
    return out;}
// The sound placed around the listener: az 0 = ahead, clockwise in radians; dist in metres; gain for that distance.
// Interleaved stereo with 150 ms of silence first (the chain fades in over 120 ms) and the echoes' tail after.
inline std::vector<float> place(const std::vector<float>& m,double az,double dist,double gain){
    const double R=sbdsp::Rate;double s=std::sin(az),c=std::cos(az),side=std::fabs(s),behind=std::max(0.0,-c);
    auto coef=[&](double hz){return 1-std::exp(-2*sbdsp::Pi*hz/R);}; // one-pole low-pass
    double air=std::max(6000.0,17000-dist*70),back=16000-11000*behind,nearCut=std::min({18000.0,back,air}),farCut=std::min({18000-15500*side,back,air});
    double kn=coef(nearCut),kf=coef(farCut),ka=coef(air*.6),farGain=1-.45*side;
    int itd=(int)std::lround(side*.00066*R),pre=(int)(.15*R);
    const double tapMs[]={31,47,71,97};double stretch=1+dist/60,wet=std::min(.35,.05+dist*.004);int maxTap=(int)(tapMs[3]*stretch*.001*R)+1;
    size_t len=m.size(),total=pre+len+itd+maxTap+(size_t)(.3*R);std::vector<float> out(total*2,0.f);
    bool rightNear=s>=0;double n1=0,n2=0,f1=0,f2=0,a1=0;
    for(size_t i=0;i<len;i++){double x=m[i]*gain;
        n1+=kn*(x-n1);n2+=kn*(n1-n2);f1+=kf*(x*farGain-f1);f2+=kf*(f1-f2);a1+=ka*(x-a1);
        size_t nearAt=pre+i,farAt=pre+i+itd;out[nearAt*2+(rightNear?1:0)]+=(float)n2;out[farAt*2+(rightNear?0:1)]+=(float)f2;
        for(int k=0;k<4;k++){size_t d=pre+i+(size_t)(tapMs[k]*stretch*.001*R);double e=a1*wet*std::pow(.6,k);out[d*2+(k&1)]+=(float)e;out[d*2+1-(k&1)]+=(float)(e*.8);}}
    return out;}
// kind 0 footstep (2..25 m), 1 shot (20..150 m): how much quieter it is there than in the recording. The nearest footstep is
// the recording's own level (what PUBG played); farther ones fall off from there.
inline double gainAt(int kind,double dist){return kind?std::min(1.0,12/dist):dist<=2?1:1/(1+(dist-2)/4);}
inline double minDist(int kind){return kind?20:2;}
inline double maxDist(int kind){return kind?150:25;}
// the distance test's three steps per kind
inline double testDist(int kind,int i){static const double d[2][3]={{2,10,25},{20,80,150}};return d[kind?1:0][std::clamp(i,0,2)];}
// the recording as it is (no direction or distance), with the same lead-in silence the chain's fade-in needs
inline std::vector<float> asIs(const std::vector<float>& m){size_t pre=(size_t)(.15*sbdsp::Rate);std::vector<float> out((pre+m.size()+(size_t)(.1*sbdsp::Rate))*2,0.f);
    for(size_t i=0;i<m.size();i++){out[(pre+i)*2]=m[i];out[(pre+i)*2+1]=m[i];}return out;}
// The sound through a preset (3 = 사용자 지정, from custom; -1 = 적용 안 함, left as it is), as 16-bit PCM WAV bytes.
// dist <= 0: the recording as it is; otherwise placed at az, dist.
inline std::vector<char> render(const std::vector<float>& mono,int kind,double az,double dist,int preset,const sbdsp::Preset& custom){
    std::vector<float> st=dist<=0?asIs(mono):place(mono,az,dist,gainAt(kind,dist));
    if(preset>=0){auto chain=std::make_unique<sbdsp::Chain>();chain->setCustom(custom);chain->setPreset(preset);chain->enable(true);chain->process(st.data(),st.size()/2);}
    uint32_t data=(uint32_t)(st.size()*2);std::vector<char> w(44+data);auto put32=[&](size_t p,uint32_t v){memcpy(&w[p],&v,4);};auto put16=[&](size_t p,uint16_t v){memcpy(&w[p],&v,2);};
    memcpy(&w[0],"RIFF",4);put32(4,36+data);memcpy(&w[8],"WAVEfmt ",8);put32(16,16);put16(20,1);put16(22,2);put32(24,48000);put32(28,48000*4);put16(32,4);put16(34,16);memcpy(&w[36],"data",4);put32(40,data);
    for(size_t i=0;i<st.size();i++){int16_t v=(int16_t)std::lround(std::clamp((double)st[i],-1.0,1.0)*32767);memcpy(&w[44+i*2],&v,2);}
    return w;}

// Playing: one sound at a time. The file is read and rendered on a worker thread (the page never waits), then played
// on one waveOut device; the bytes stay alive until the next sound replaces them (cut first).
struct Result{int kind=0,error=0;double az=0,dist=0;int ms=0;}; // error 1 no WAV in the folder, 2 unreadable, 3 stopped before it played
inline std::mutex lock;inline std::atomic<bool> busy{false};inline std::atomic<int> generation{0}; // stop() bumps it: a sound still being rendered is then dropped
// One output device for every click: a new sound first cuts the one playing (PlaySound from a worker thread did not stop a
// sound another thread had started, so quick clicks stacked up several gunshots).
inline std::mutex outLock;inline HWAVEOUT out=nullptr;inline WAVEHDR hdr={};inline std::vector<char> playing;
inline void cut(){if(!out)return;waveOutReset(out);if(hdr.dwFlags&WHDR_PREPARED)waveOutUnprepareHeader(out,&hdr,sizeof(hdr));hdr={};} // outLock held
inline void outPlay(std::vector<char>& w,int gen){std::lock_guard<std::mutex> g(outLock);if(gen!=generation)return;cut();
    if(!out){WAVEFORMATEX f={WAVE_FORMAT_PCM,2,48000,48000*4,4,16,0};if(waveOutOpen(&out,WAVE_MAPPER,&f,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){out=nullptr;return;}}
    playing.swap(w);if(playing.size()<=44)return;hdr.lpData=playing.data()+44;hdr.dwBufferLength=(DWORD)(playing.size()-44); // past the WAV header
    if(waveOutPrepareHeader(out,&hdr,sizeof(hdr))==MMSYSERR_NOERROR)waveOutWrite(out,&hdr,sizeof(hdr));}
inline std::wstring firstWav(const std::wstring& dir){WIN32_FIND_DATAW d;HANDLE f=FindFirstFileW((dir+L"\\*.wav").c_str(),&d);if(f==INVALID_HANDLE_VALUE)return L"";
    std::wstring r;do{if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){r=dir+L"\\"+d.cFileName;break;}}while(FindNextFileW(f,&d));FindClose(f);return r;}
// the decoded recording for a kind, read again only when the file changed
inline std::vector<float> sample(const std::wstring& path){static std::wstring had[2];static FILETIME at[2];static std::vector<float> cache[2];static int next=0;
    WIN32_FILE_ATTRIBUTE_DATA a={};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&a))return {};
    for(int i=0;i<2;i++)if(had[i]==path&&!CompareFileTime(&at[i],&a.ftLastWriteTime))return cache[i];
    int i=next;next^=1;cache[i]=readWav(path);had[i]=path;at[i]=a.ftLastWriteTime;return cache[i];}
// Returns false while the previous click is still being rendered. The answer arrives as WM_APP+212 (lParam: new Result).
// dir: the player's own recordings; fallback (asked on the worker thread): the data pack's recordings in the runtime folder
// dist: how far the sound is, in a random direction (0 = the recording as it is)
inline bool play(HWND to,int kind,const std::wstring& dir,std::function<std::wstring()> fallback,int preset,const sbdsp::Preset& custom,double dist){
    if(busy.exchange(true))return false;
    int gen=generation;std::thread([=]{auto* r=new Result;r->kind=kind;std::wstring path=firstWav(dir);if(path.empty()&&fallback)path=firstWav(fallback());
        if(path.empty())r->error=1;
        else{std::lock_guard<std::mutex> g(lock);std::vector<float> m=sample(path);
            if(m.empty())r->error=2;
            else{static std::mt19937 rng{(unsigned)GetTickCount64()};r->az=dist>0?std::uniform_real_distribution<double>(0,2*sbdsp::Pi)(rng):0;r->dist=dist; /* a random direction at the chosen distance */
                r->ms=(int)(m.size()*1000/48000);std::vector<char> w=render(m,kind,r->az,r->dist,preset,custom);
                if(gen!=generation)r->error=3;else outPlay(w,gen);}}
        busy=false;if(!PostMessageW(to,WM_APP+212,0,(LPARAM)r))delete r;}).detach();
    return true;}
inline void stop(){generation++;std::lock_guard<std::mutex> g(outLock);cut();} // not the render lock: the page never waits for a render in progress (it sees the new generation and drops its sound)
}
