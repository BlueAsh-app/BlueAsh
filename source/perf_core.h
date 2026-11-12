#pragma once
// Performance analysis rules (pure: no Windows calls, unit-tested in perf_core_tests.cpp).
//  - stats(): frame times (ms between the game's presents) -> average FPS (frames / time), 1 % low (1000 / the 99th
//    percentile frame time), frame-time spread and per-second FPS (for the before/after noise estimate).
//  - diagnose(): what may limit the frame rate, each with its evidence and a confidence. No single reading decides a
//    bottleneck: GPU load, the busiest CPU core, VRAM, free RAM, power/thermal state and frame caps are read together,
//    and one session never gives "high" (that needs an earlier session of the same game with the same finding).
//  - compare(), target(): before/after verdict and target FPS state. Nothing here predicts a
//    frame rate: there is no verified benchmark data in BlueAsh, so advice is qualitative and says when data is missing.
// Texts are Korean keys (translated by the UI through i18n::text); numbers travel separately (Line.a / Line.b).
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
namespace perf {
struct Stats {int frames=0;double seconds=0,fps=0,low1=0,avgMs=0,sdMs=0,p99Ms=0;std::vector<double> perSec;};
inline Stats stats(const std::vector<double>& ms){Stats s;std::vector<double> v;for(double m:ms)if(m>0&&m<2000)v.push_back(m);if(v.empty())return s;
    double sum=0;for(double m:v)sum+=m;s.frames=(int)v.size();s.seconds=sum/1000;s.fps=s.frames/s.seconds;s.avgMs=sum/s.frames;
    double var=0;for(double m:v)var+=(m-s.avgMs)*(m-s.avgMs);s.sdMs=std::sqrt(var/s.frames);
    std::vector<double> sorted=v;std::sort(sorted.begin(),sorted.end());size_t k=(size_t)(.99*sorted.size()); /* the slowest 1 % starts here */s.p99Ms=sorted[std::min(k,sorted.size()-1)];s.low1=1000/s.p99Ms;
    double t=0;int n=0;for(double m:v){t+=m;n++;if(t>=1000){s.perSec.push_back(n*1000/t);t=0;n=0;}}return s;}
inline bool enough(const Stats& s){return s.seconds>=10&&s.frames>=200;}
// What was seen around the frames (-1 / empty = not measured).
struct Sample {double gpuUtil=-1,cpuTotal=-1,cpuMaxCore=-1,cpuPerf=-1,vramUsed=-1,vramTotal=-1,ramAvailPct=-1,ramTotalGB=-1,gpuTemp=-1;
    int thermal=-1,power=-1;std::wstring topProc;double topProcCpu=-1;bool powerSaver=false,battery=false,frameGen=false;int vsync=-1,limit=0,refresh=0,ratedMts=0,configuredMts=0;};
enum Kind {GpuLimit,CpuLimit,RamCapacity,RamSpeed,Vram,Background,PowerThermal,Capped,Unclear};
enum Conf {High,Medium,Low,None};
struct Line {const wchar_t* fmt;double a=0,b=0;};
struct Finding {Kind kind;Conf conf;std::vector<Line> why;const wchar_t* next=L"";const wchar_t* fix=L"";};
inline Conf lower(Conf c){return c==High?Medium:c==Medium?Low:c;}
// agreeing: earlier sessions of this game whose main finding was the same kind (raises GPU/CPU findings to "high").
inline std::vector<Finding> diagnose(const Stats& s,const Sample& x,int agreeing=0){std::vector<Finding> out;
    if(!enough(s)){out.push_back({Unclear,None,{{L"측정 %.0f초 · 프레임 %.0f개 (10초·200프레임 이상 필요)",s.seconds,(double)s.frames}},L"게임 안에서 20초 이상 다시 측정",L""});return out;}
    bool capped=false;
    if(x.limit>0&&s.fps>=x.limit*.95&&s.fps<=x.limit*1.03){capped=true;out.push_back({Capped,Medium,{{L"평균 %.0f FPS가 FPS 제한 %.0f에 맞춰져 있습니다",s.fps,(double)x.limit}},L"FPS 제한을 끄고 다시 측정",L"제한을 올리거나 끄면 남는 성능을 볼 수 있습니다"});}
    else if(x.vsync>0&&x.refresh>0){double cap=(double)x.refresh/x.vsync;if(s.fps>=cap*.95&&s.fps<=cap*1.03){capped=true;out.push_back({Capped,Medium,{{L"수직 동기화가 켜져 있고 평균 %.0f FPS가 화면 주사율 %.0f Hz에 맞춰져 있습니다",s.fps,cap}},L"수직 동기화를 끄고 다시 측정",L"게임의 수직 동기화를 끄면 남는 성능을 볼 수 있습니다"});}}
    Conf top=agreeing>0?High:Medium;
    if(!capped){
        if(x.gpuUtil>=95){out.push_back({GpuLimit,top,{{L"GPU 사용률 평균 %.0f%%",x.gpuUtil}},L"그래픽 옵션이나 해상도를 낮춰 FPS가 오르는지 확인",L"해상도·그림자·광원 등 GPU 부담이 큰 옵션을 낮춥니다"});}
        else if(x.gpuUtil>=88){out.push_back({GpuLimit,Low,{{L"GPU 사용률 평균 %.0f%%",x.gpuUtil}},L"그래픽 옵션이나 해상도를 낮춰 FPS가 오르는지 확인",L"해상도·그림자·광원 등 GPU 부담이 큰 옵션을 낮춥니다"});}
        else if(x.gpuUtil>=0&&x.cpuMaxCore>=90){out.push_back({CpuLimit,top,{{L"GPU 사용률 평균 %.0f%%",x.gpuUtil},{L"가장 바쁜 CPU 코어 평균 %.0f%%",x.cpuMaxCore}},L"그래픽 옵션을 낮춰도 FPS가 그대로인지 확인",L"시야 거리·인구·물리 등 CPU 부담 옵션을 낮추고 백그라운드 프로그램을 닫습니다"});}
        else if(x.gpuUtil>=0&&x.cpuMaxCore>=0){out.push_back({Unclear,Low,{{L"GPU 사용률 평균 %.0f%%",x.gpuUtil},{L"가장 바쁜 CPU 코어 평균 %.0f%%",x.cpuMaxCore}},L"다른 장면에서 다시 측정하고 그래픽 옵션을 바꿔 비교",L"GPU도 CPU 코어도 한계가 아닙니다 (게임 엔진·메인 스레드 대기·로딩일 수 있음)"});}
        else{out.push_back({Unclear,None,{{L"GPU 사용률을 읽지 못했습니다",0}},L"GPU 사용률을 읽을 수 있는 상태에서 다시 측정",L""});}}
    if(x.vramTotal>0&&x.vramUsed>=0){double r=x.vramUsed/x.vramTotal;if(r>=.9)out.push_back({Vram,r>=.95?Medium:Low,{{L"VRAM %.1f / %.1f GB 사용",x.vramUsed,x.vramTotal}},L"텍스처 품질을 한 단계 낮춰 1% Low가 오르는지 확인",L"텍스처 품질·해상도를 낮춥니다"});}
    if(x.ramAvailPct>=0&&x.ramAvailPct<15){out.push_back({RamCapacity,x.ramAvailPct<10?Medium:Low,{{L"남은 메모리 %.0f%% (전체 %.0f GB)",x.ramAvailPct,x.ramTotalGB}},L"다른 프로그램을 닫고 다시 측정",L"게임 중 쓰지 않는 프로그램을 닫습니다"});}
    if(x.configuredMts>0&&x.ratedMts>0&&x.configuredMts<x.ratedMts*.9){out.push_back({RamSpeed,Low,{{L"메모리 설정 속도 %.0f MT/s · 모듈 정격 %.0f MT/s",(double)x.configuredMts,(double)x.ratedMts}},L"BIOS에서 XMP/EXPO 상태 확인",L"BIOS의 XMP/EXPO를 켜면 모듈 정격 속도로 동작합니다 (FPS 변화는 게임마다 다르고 예측하지 않습니다)"});}
    if(x.thermal==1){out.push_back({PowerThermal,Medium,{{L"측정 중 GPU 온도 제한이 걸렸습니다 (최고 %.0f°C)",x.gpuTemp}},L"케이스 공기 흐름을 확인하고 다시 측정",L"먼지 청소·팬 곡선 확인"});}
    if(x.powerSaver){out.push_back({PowerThermal,Medium,{{L"절전 전원 계획 사용 중",0}},L"스마트 부스트로 고성능 전원 계획을 적용하고 다시 측정",L"고성능 전원 계획"});}
    if(x.battery){out.push_back({PowerThermal,Medium,{{L"배터리로 동작 중",0}},L"전원을 연결하고 다시 측정",L"전원 어댑터 연결"});}
    if(x.cpuPerf>=0&&x.cpuPerf<80&&x.cpuTotal>=40){out.push_back({PowerThermal,Low,{{L"CPU가 기본 클럭의 %.0f%%로 동작",x.cpuPerf}},L"전원 계획과 CPU 온도 확인",L"고성능 전원 계획"});}
    if(x.topProcCpu>=15){out.push_back({Background,x.topProcCpu>=30?Medium:Low,{{L"게임 외 가장 바쁜 프로그램이 CPU %.0f%% 사용",x.topProcCpu}},L"그 프로그램을 닫고 다시 측정",L"게임 중 쓰지 않는 프로그램을 닫습니다"});}
    // Frame generation adds frames the game did not render: present counts are not the render rate, so the GPU/CPU
    // reading is shaky. Mark it and lower the confidence.
    if(x.frameGen){for(auto& f:out){if(f.kind==GpuLimit||f.kind==CpuLimit||f.kind==Unclear){f.conf=lower(f.conf);f.why.push_back({L"프레임 생성 지원 게임: 측정한 FPS에 생성 프레임이 섞였을 수 있어 렌더 FPS로 보지 않습니다",0});}}}
    std::stable_sort(out.begin(),out.end(),[](const Finding& a,const Finding& b){return a.conf<b.conf;});return out;}
// Before / after: a difference counts only past the noise (2 standard errors of the per-second FPS) and 3 %.
enum Verdict {Improved,NoChange,Worse,Insufficient};
struct Compare {Verdict v=Insufficient;double diff=0,pct=0,noise=0;bool shaky=false;};
inline double meanOf(const std::vector<double>& v){double s=0;for(double x:v)s+=x;return v.empty()?0:s/v.size();}
inline double sdOf(const std::vector<double>& v){double m=meanOf(v),s=0;for(double x:v)s+=(x-m)*(x-m);return v.size()<2?0:std::sqrt(s/(v.size()-1));}
inline Compare compare(const Stats& a,const Stats& b){Compare c;if(!enough(a)||!enough(b)||a.perSec.size()<5||b.perSec.size()<5)return c;
    c.diff=b.fps-a.fps;c.pct=a.fps>0?c.diff/a.fps*100:0;double sa=sdOf(a.perSec),sb=sdOf(b.perSec);c.noise=2*std::sqrt(sa*sa/a.perSec.size()+sb*sb/b.perSec.size());
    c.shaky=(meanOf(a.perSec)>0&&sa/meanOf(a.perSec)>.25)||(meanOf(b.perSec)>0&&sb/meanOf(b.perSec)>.25); // a scene that moved a lot: low confidence
    double need=std::max(c.noise,a.fps*.03);c.v=std::fabs(c.diff)<need?NoChange:c.diff>0?Improved:Worse;return c;}
// Target FPS: met = average and 1 % low both near the target; capped = a frame limit or V-Sync sits below it.
enum TargetState {Met,Near,Missed,CappedBelow,NoData};
inline TargetState target(const Stats& s,int want,int limit,int refresh,int vsync){if(want<=0||!enough(s))return NoData;
    if(limit>0&&limit<want*.98){return CappedBelow;}if(vsync>0&&refresh>0&&(double)refresh/vsync<want*.98)return CappedBelow;
    if(s.fps>=want*.98&&s.low1>=want*.75){return Met;}if(s.fps>=want*.9){return Near;}return Missed;}
// A measured session, one line in perf-history.tsv (tab-separated key=value; unknown keys are kept out of the way).
struct Session {std::wstring game,when,cpu,gpu,res,method;double ramGB=0,seconds=0,fps=0,low1=0,avgMs=0,sdMs=0,gpuUtil=-1,cpuMaxCore=-1;int frames=0,frameGen=0,main=-1,mainConf=3,boost=0;};
inline std::wstring clean(std::wstring v){for(auto& c:v)if(c==L'\t'||c==L'\n'||c==L'\r'||c==L'=')c=L' ';return v;}
inline std::wstring line(const Session& s){wchar_t b[1024];swprintf(b,1024,L"game=%ls\twhen=%ls\tcpu=%ls\tgpu=%ls\tres=%ls\tmethod=%ls\tram=%.1f\tsec=%.1f\tfps=%.2f\tlow1=%.2f\tms=%.3f\tsd=%.3f\tgpu%%=%.1f\tcore=%.1f\tframes=%d\tfg=%d\tmain=%d\tconf=%d\tboost=%d",
    clean(s.game).c_str(),clean(s.when).c_str(),clean(s.cpu).c_str(),clean(s.gpu).c_str(),clean(s.res).c_str(),clean(s.method).c_str(),s.ramGB,s.seconds,s.fps,s.low1,s.avgMs,s.sdMs,s.gpuUtil,s.cpuMaxCore,s.frames,s.frameGen,s.main,s.mainConf,s.boost);return b;}
inline bool parse(const std::wstring& l,Session& s){s=Session();size_t i=0;bool any=false;
    while(i<l.size()){size_t e=l.find(L'\t',i);if(e==std::wstring::npos)e=l.size();std::wstring kv=l.substr(i,e-i);i=e+1;size_t q=kv.find(L'=');if(q==std::wstring::npos)continue;
        std::wstring k=kv.substr(0,q),v=kv.substr(q+1);double d=_wtof(v.c_str());any=true;
        if(k==L"game")s.game=v;else if(k==L"when")s.when=v;else if(k==L"cpu")s.cpu=v;else if(k==L"gpu")s.gpu=v;else if(k==L"res")s.res=v;else if(k==L"method")s.method=v;
        else if(k==L"ram")s.ramGB=d;else if(k==L"sec")s.seconds=d;else if(k==L"fps")s.fps=d;else if(k==L"low1")s.low1=d;else if(k==L"ms")s.avgMs=d;else if(k==L"sd")s.sdMs=d;
        else if(k==L"gpu%")s.gpuUtil=d;else if(k==L"core")s.cpuMaxCore=d;else if(k==L"frames")s.frames=(int)d;else if(k==L"fg")s.frameGen=(int)d;else if(k==L"main")s.main=(int)d;
        else if(k==L"conf")s.mainConf=(int)d;else if(k==L"boost")s.boost=(int)d;}
    return any&&!s.game.empty()&&std::isfinite(s.fps)&&s.fps>=0&&s.fps<100000;}
}
