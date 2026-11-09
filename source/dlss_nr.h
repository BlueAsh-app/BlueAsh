#pragma once
// DLSS 5 Neural Rendering settings for the OptiScaler DLSS-NR route (Dagherbou's GPL build): pure logic, no Win32
// (dlss_nr_tests.cpp includes it alone). The keys and their meanings are the [DlssNr] section of that build's
// OptiScaler.ini; nothing here is invented. Ranges are kept inside what the build documents, and WorkingScale stops at
// 1.0 (above it the model supersamples and its cost grows with the square — the freezes the user saw), with one pass.
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace dlssnr {
// One adjustable value: its OptiScaler.ini key, the BlueAsh label, the allowed range and the build's default.
struct Knob {const char* key;const wchar_t* name;const wchar_t* help;double lo,hi,def,step;};
inline const std::vector<Knob>& knobs(){static const std::vector<Knob> k={
    {"TransferStrength",L"적용 강도",L"화면이 모델의 그림 쪽으로 얼마나 옮겨 갈지. 0은 원래 화면 그대로, 1은 모델의 그림입니다.",0,3,1.0,0.05},
    {"ColourStrength",L"색 반영",L"모델이 바꾼 색까지 가져올지. 0은 게임의 색을 그대로 두고 밝기만, 1은 모델의 색까지 씁니다.",0,2,1.0,0.05},
    {"Intensity",L"모델 강도",L"모델 효과의 전체 세기입니다.",0,2,1.0,0.05},
    {"LocalStructure",L"구조 강도",L"표면 디테일, 그림자 접촉, 반사처럼 잘게 바뀌는 부분의 세기입니다.",0,2,1.0,0.05},
    {"LocalTone",L"톤 강도",L"조명과 색 반응처럼 넓게 바뀌는 부분의 세기입니다.",0,2,1.0,0.05},
    {"MaxRatio",L"최대 밝기 배율",L"한 픽셀을 최대 몇 배까지 밝게 할 수 있는지. 밝은 빛이 번지는 것을 막습니다.",1,8,2.0,0.1},
    {"WorkingScale",L"작업 해상도",L"모델이 일하는 해상도 비율. 낮추면 빨라지고(비용은 제곱으로 줄어듦) 화면 자체는 줄지 않습니다. BlueAsh는 1.0까지만 허용합니다.",0.5,1.0,1.0,0.05}};return k;}
struct Settings {int preset=2;double v[7]={1.0,1.0,1.0,1.0,1.0,2.0,1.0};bool autoMask=true;}; // preset 1 = model A, 2 = model B
inline double clampTo(const Knob& k,double x){if(!(x==x))return k.def;x=std::round(x/k.step)*k.step;return x<k.lo?k.lo:x>k.hi?k.hi:x;}
inline Settings defaults(){Settings s;for(size_t i=0;i<knobs().size();i++)s.v[i]=knobs()[i].def;return s;}
// Built-in starting points. They only use documented keys and are shown with their values; they are not tuned
// results (no measurements behind them), and the UI says so.
inline Settings builtin(int which){Settings s=defaults(); // 1 balanced, 2 realistic, 3 cinematic
    if(which==1){s.preset=2;s.v[0]=0.6;s.v[1]=0.5;}       // keeps the game's own colour, half way to the model
    else if(which==3){s.preset=1;s.v[0]=1.3;s.v[5]=2.5;}  // model A pushed past the model's picture (the strong "cinematic" look DLSS 5 Swapper users aim for: faces visibly reshaped)
    return s;}                                            // realistic: model B at the build's defaults
inline bool modelsAB(const std::string& version){int a=0,b=0;std::sscanf(version.c_str(),"%d.%d",&a,&b);return a>310||(a==310&&b>=9);} // 310.9+: A and B
// "key=value" lines (the format BlueAsh saves and exports). Unknown keys, non-numbers and out-of-range values are
// refused: an imported preset can only carry numbers for these keys, never a path or a command.
inline std::string serialize(const Settings& s){std::string out="BlueAshDlssNr=1\nPreset="+std::to_string(s.preset)+"\nAutoMask="+(s.autoMask?"true":"false")+"\n";
    char b[64];for(size_t i=0;i<knobs().size();i++){std::snprintf(b,64,"%s=%.2f\n",knobs()[i].key,s.v[i]);out+=b;}return out;}
inline bool parse(const std::string& text,Settings& out,std::string* error=nullptr){
    Settings s=defaults();bool header=false;size_t at=0;auto fail=[&](const std::string& e){if(error)*error=e;return false;};
    if(text.size()>4096)return fail("too large");
    while(at<text.size()){size_t e=text.find('\n',at);std::string l=text.substr(at,e==std::string::npos?std::string::npos:e-at);at=e==std::string::npos?text.size():e+1;
        while(!l.empty()&&(l.back()=='\r'||l.back()==' ')){l.pop_back();}if(l.empty()||l[0]=='#'||l[0]==';')continue;size_t eq=l.find('=');if(eq==std::string::npos)return fail("line without =");
        std::string k=l.substr(0,eq),v=l.substr(eq+1);if(k=="BlueAshDlssNr"){header=v=="1";continue;}
        if(k=="Preset"){if(v!="1"&&v!="2")return fail("Preset must be 1 or 2");s.preset=v[0]-'0';continue;}
        if(k=="AutoMask"){if(v!="true"&&v!="false")return fail("AutoMask must be true or false");s.autoMask=v=="true";continue;}
        bool known=false;for(size_t i=0;i<knobs().size();i++)if(k==knobs()[i].key){char* end=nullptr;double x=std::strtod(v.c_str(),&end);
            if(v.empty()||*end||x<knobs()[i].lo-1e-9||x>knobs()[i].hi+1e-9){return fail(k+" out of range");}s.v[i]=clampTo(knobs()[i],x);known=true;}
        if(!known)return fail("unknown key "+k);}
    if(!header){return fail("not a BlueAsh DLSS 5 preset");}out=s;return true;}
// ---- The build's own capture (dlssnr-capture\before_NN.raw / after_NN.raw + manifest.txt: "<which> width W height H
// format F rowPitch P"): the frame as the upscaler produced it and the same frame after the model's edit. Turned into
// 8-bit BGRA for the viewer. Display-ready formats are copied; linear HDR formats get one exposure taken from the
// *before* frame and used for both, then the same curve, so the pair differs only by what the model did.
struct Frame {int w=0,h=0,format=0,pitch=0;};
inline bool parseManifest(const std::string& text,Frame& before,Frame& after){
    auto line=[&](const char* which,Frame& f){size_t at=text.find(std::string(which)+" width ");if(at==std::string::npos)return false;
        unsigned long long w=0;unsigned h=0,p=0;int fmt=0;if(std::sscanf(text.c_str()+at+std::string(which).size(),"%*s %llu height %u format %d rowPitch %u",&w,&h,&fmt,&p)!=4)return false;
        f.w=(int)w;f.h=(int)h;f.format=fmt;f.pitch=(int)p;return w>0&&w<16384&&h>0&&h<16384&&p>=w;};
    return line("before",before)&&line("after",after);}
inline int bytesPerPixel(int format){switch(format){case 2:return 16;case 10:return 8;case 24:case 26:case 28:case 29:case 87:case 91:return 4;}return 0;}
inline float half(unsigned short h){unsigned s=(h>>15)&1,e=(h>>10)&31,m=h&1023;float v=e==0?std::ldexp((float)m,-24):e==31?65504.f:std::ldexp((float)(m|1024),(int)e-25);return s?-v:v;}
inline float small(unsigned bits,int mbits){unsigned e=bits>>mbits,m=bits&((1u<<mbits)-1);return e==0?std::ldexp((float)m,-14-mbits):e==31?65504.f:std::ldexp((float)(m|(1u<<mbits)),(int)e-15-mbits);}
inline void rgbAt(const unsigned char* p,int format,float& r,float& g,float& b){
    switch(format){
    case 2:{float f[4];std::memcpy(f,p,16);r=f[0];g=f[1];b=f[2];break;}
    case 10:{unsigned short s[4];std::memcpy(s,p,8);r=half(s[0]);g=half(s[1]);b=half(s[2]);break;}
    case 26:{unsigned v;std::memcpy(&v,p,4);r=small(v&0x7FF,6);g=small((v>>11)&0x7FF,6);b=small(v>>22,5);break;}
    case 24:{unsigned v;std::memcpy(&v,p,4);r=(v&1023)/1023.f;g=((v>>10)&1023)/1023.f;b=((v>>20)&1023)/1023.f;break;}
    case 87:case 91:r=p[2]/255.f;g=p[1]/255.f;b=p[0]/255.f;break;
    default:r=p[0]/255.f;g=p[1]/255.f;b=p[2]/255.f;}}
inline bool linearHdr(int format){return format==2||format==10||format==26;}
// Mean luminance of a frame (every 4th pixel each way), for the shared exposure.
inline double meanLuma(const Frame& f,const std::vector<unsigned char>& data){double sum=0;long n=0;int bpp=bytesPerPixel(f.format);
    for(int y=0;y<f.h;y+=4)for(int x=0;x<f.w;x+=4){size_t o=(size_t)y*f.pitch+(size_t)x*bpp;if(o+bpp>data.size())continue;float r,g,b;rgbAt(&data[o],f.format,r,g,b);double l=0.2126*r+0.7152*g+0.0722*b;if(l==l&&l>=0&&l<1e6){sum+=l;n++;}}
    return n?sum/n:0;}
inline unsigned char encode(float v){if(!(v==v)||v<=0)return 0;v=v<0.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-0.055f;return (unsigned char)std::lround((v>1?1:v)*255);}
// -> width*height*4 bytes BGRA. scale: exposure for linear formats (0.18 / mean luminance of the before frame).
inline bool toDisplay(const Frame& f,const std::vector<unsigned char>& data,double scale,std::vector<unsigned char>& out){
    int bpp=bytesPerPixel(f.format);if(!bpp||data.size()<(size_t)f.pitch*(f.h-1)+(size_t)f.w*bpp)return false;out.assign((size_t)f.w*f.h*4,255);
    for(int y=0;y<f.h;y++)for(int x=0;x<f.w;x++){float r,g,b;rgbAt(&data[(size_t)y*f.pitch+(size_t)x*bpp],f.format,r,g,b);unsigned char* o=&out[((size_t)y*f.w+x)*4];
        if(linearHdr(f.format)){r=(float)(r*scale);g=(float)(g*scale);b=(float)(b*scale);r/=1+r;g/=1+g;b/=1+b;o[0]=encode(b);o[1]=encode(g);o[2]=encode(r);}
        else{o[0]=(unsigned char)std::lround(b*255);o[1]=(unsigned char)std::lround(g*255);o[2]=(unsigned char)std::lround(r*255);}}
    return true;}
// Sets key=value inside [section] of an ini text, keeping everything else (comments included); adds what is missing.
inline std::string setIni(std::string text,const std::string& section,const std::string& key,const std::string& value){
    std::string nl=text.find("\r\n")!=std::string::npos?"\r\n":"\n";size_t start=std::string::npos,pos=0;
    while(pos<text.size()){size_t e=text.find('\n',pos);std::string l=text.substr(pos,(e==std::string::npos?text.size():e)-pos);if(!l.empty()&&l.back()=='\r')l.pop_back();
        if(l=="["+section+"]"){start=e==std::string::npos?text.size():e+1;break;}pos=e==std::string::npos?text.size():e+1;}
    if(start==std::string::npos){if(!text.empty()&&text.back()!='\n')text+=nl;return text+"["+section+"]"+nl+key+"="+value+nl;}
    size_t p=start,end=text.size();
    while(p<text.size()){size_t e=text.find('\n',p);size_t le=e==std::string::npos?text.size():e;std::string l=text.substr(p,le-p);if(!l.empty()&&l.back()=='\r')l.pop_back();
        if(!l.empty()&&l[0]=='['){end=p;break;}
        if(l.compare(0,key.size()+1,key+"=")==0){size_t vs=p+key.size()+1,ve=le;if(ve>vs&&text[ve-1]=='\r')ve--;text.replace(vs,ve-vs,value);return text;}
        p=e==std::string::npos?text.size():e+1;}
    std::string add=key+"="+value+nl;if(end>0&&text[end-1]!='\n')add=nl+add;text.insert(end,add);return text;}
inline std::string getIni(const std::string& text,const std::string& section,const std::string& key){
    std::string sec;size_t pos=0;while(pos<text.size()){size_t e=text.find('\n',pos);std::string l=text.substr(pos,(e==std::string::npos?text.size():e)-pos);if(!l.empty()&&l.back()=='\r')l.pop_back();
        if(!l.empty()&&l[0]=='[')sec=l;else if(sec=="["+section+"]"&&l.compare(0,key.size()+1,key+"=")==0)return l.substr(key.size()+1);pos=e==std::string::npos?text.size():e+1;}return "";}
// The OptiScaler.ini BlueAsh writes: DLSS-NR on with these settings, one process, frame generation explicitly off (no
// stacking with NR), no ASI plugins, no GPU spoofing, the game's DLSS as the DX12 upscaler and OptiScaler's documented
// D3D12 bridge on DX11/Vulkan, logging on so a failure can be read afterwards. enabled=false keeps the files but stops the pass.
// Only the [DlssNr] look: what BlueAsh writes into an OptiScaler.ini another tool installed (its other choices stay).
inline std::string tune(std::string ini,const Settings& s,bool enabled=true){
    char b[32];ini=setIni(ini,"DlssNr","Enabled",enabled?"true":"false");ini=setIni(ini,"DlssNr","Preset",std::to_string(s.preset));ini=setIni(ini,"DlssNr","AutoMask",s.autoMask?"true":"false");
    for(size_t i=0;i<knobs().size();i++){std::snprintf(b,32,"%.2f",s.v[i]);ini=setIni(ini,"DlssNr",knobs()[i].key,b);}return ini;}
// The same look in a RenoDX DLSS 5 add-on's settings (ReShade.ini [RenoDX.DLSS5], same meanings under NR* names). It has no
// max-ratio key and its NRPreset numbering is not documented, so the model and the ratio stay as the add-on has them.
inline std::string renoTune(std::string ini,const Settings& s){const char* keys[]={"NRTransferStrength","NRColorStrength","NRIntensity","NRLocalStructure","NRLocalTone",nullptr,"NRResolutionScale"};
    char b[32];for(size_t i=0;i<knobs().size()&&i<7;i++){if(!keys[i])continue;std::snprintf(b,32,"%g",std::round(s.v[i]*100)/100);ini=setIni(ini,"RenoDX.DLSS5",keys[i],b);}
    return setIni(ini,"RenoDX.DLSS5","NRAutoMask",s.autoMask?"1":"0");}
inline std::string configure(std::string ini,const Settings& s,const std::string& exeName,bool enabled=true){
    ini=tune(ini,s,enabled);
    ini=setIni(ini,"DlssNr","DebugView","0");ini=setIni(ini,"DlssNr","AutoCapture","false");
    for(auto* kv:{"Enabled=false","FGInput=nofg","FGOutput=nofg"}){std::string p=kv;size_t eq=p.find('=');ini=setIni(ini,"FrameGen",p.substr(0,eq),p.substr(eq+1));}
    ini=setIni(ini,"Upscalers","Dx12Upscaler","dlss");ini=setIni(ini,"Upscalers","Dx11Upscaler","ffx_12");ini=setIni(ini,"Upscalers","VulkanUpscaler","ffx_12");
    ini=setIni(ini,"Spoofing","Dxgi","false");ini=setIni(ini,"Plugins","LoadAsiPlugins","false");ini=setIni(ini,"Log","LogToFile","true");ini=setIni(ini,"Log","LogLevel","2");
    ini=setIni(ini,"ProcessFilter","TargetProcessName",exeName);return ini;}
// The build's frame limit ([Framerate] FramerateLimit, Reflex-based where the game has Reflex): 0 = off ("auto").
// Read once at game start like the rest of the file. Accepts 0 or 30..500 only.
inline std::string frameLimit(std::string ini,int fps){if(fps!=0&&(fps<30||fps>500))fps=0;return setIni(ini,"Framerate","FramerateLimit",fps?std::to_string(fps)+".0":"auto");}
}
