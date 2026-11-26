#pragma once
// 배그 사운드 부스트 – the pure audio chain (no Windows calls; tested by sound_dsp_tests.cpp). 48 kHz stereo float.
//   EQ (RBJ biquads, the same filters on both channels) -> a linked compressor (one gain for both channels, from the
//   louder one, so a sound never moves left or right) -> make-up gain -> a look-ahead peak limiter (ceiling: the game's own level, -1 dBFS at most) -> a hard
//   clamp at -1 dBFS. No upward compression: quiet sounds are only raised by the fixed, bounded make-up.
// Safety: every preset is validated (boosts capped), NaN / infinity in the input is replaced by silence, and a
// non-finite value anywhere in the chain trips fault(): the chain then outputs silence until reset (the engine stops
// and hands the game its own sound back). Switching on, off and between presets fades (no clicks, no jumps).
// shortcut: the presets are reasoned starting points, not tuned against recorded PUBG audio yet; tune them with
// captures before claiming anything about footstep detection.
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <complex>
namespace sbdsp {
const double Rate=48000,Ceiling=0.891,Pi=3.14159265358979323846; // ceiling: -1 dBFS
struct Band {int type;double f,q,db;}; // 0 high-pass, 1 peak, 2 low shelf, 3 high shelf
struct Preset {std::vector<Band> bands;double threshold,ratio,attackMs,releaseMs,makeup;};
// 0 밸런스: small, broad moves; light compression (keeps loudness differences, so distance cues stay).
// 1 발소리 집중: presence (2-5 kHz, where steps on most surfaces carry) lifted, low rumble trimmed, a bit more compression.
// (2 was 총성 완화; dropped: every preset already keeps loud sounds at or under the game's own level, so it only took the
// footsteps' clarity away. Index 2 stays unused so 3 still means 사용자 지정 in saved settings.)
inline const Preset& preset(int i){
    static const Preset p[2]={
        {{{0,25,.707,0},{1,250,1.0,-1.5},{1,2200,1.0,2.5},{3,7000,.707,-1.5}},-18,2.0,5,120,2.0},
        {{{0,30,.707,0},{2,120,.707,-2.0},{1,600,1.0,-1.0},{1,2500,.9,4.0},{1,5000,1.2,1.5}},-22,2.5,4,150,3.5}};
    return p[i==1?1:0];}
// 사용자 지정 (preset 3): six bands at fixed frequencies (the page drags their points), each -12..+6 dB, plus 압축 강도
// (0..100: threshold -12 -> -30 dB, ratio 1 -> 5) and 조용한 소리 키우기 (make-up, 0..6 dB). Built through valid() like the others.
struct Custom {double g[6]={0,0,0,0,0,0};double comp=30,lift=2;};
const double CustomHz[6]={100,250,1000,2500,5000,9000};
inline Preset customPreset(const Custom& c){Preset p;p.bands.push_back({0,30,.707,0});
    for(int k=0;k<6;k++)p.bands.push_back({k==0?2:k==5?3:1,CustomHz[k],k==0||k==5?.707:1.0,std::clamp(c.g[k],-12.0,6.0)});
    double s=std::clamp(c.comp,0.0,100.0)/100;p.threshold=-12-18*s;p.ratio=1+4*s;p.attackMs=3;p.releaseMs=150;p.makeup=std::clamp(c.lift,0.0,6.0);return p;}
// What a preset may do at most: boosts up to +6 dB per band and +6 dB make-up, cuts down to -12 dB, sane compressor.
inline bool valid(const Preset& p){
    for(auto& b:p.bands){if(b.type<0||b.type>3||!(b.f>=20&&b.f<=20000)||!(b.q>=.3&&b.q<=4)||!(b.db>=-12&&b.db<=6))return false;}
    return p.threshold>=-40&&p.threshold<=0&&p.ratio>=1&&p.ratio<=10&&p.attackMs>=.5&&p.attackMs<=50&&p.releaseMs>=20&&p.releaseMs<=1000&&p.makeup>=0&&p.makeup<=6;}
struct Biquad {double b0=1,b1=0,b2=0,a1=0,a2=0,z1[2]={0,0},z2[2]={0,0};
    void set(const Band& b){double A=std::pow(10,b.db/40),w=2*Pi*b.f/Rate,c=std::cos(w),s=std::sin(w),al=s/(2*b.q),a0=1;
        if(b.type==0){b0=(1+c)/2;b1=-(1+c);b2=(1+c)/2;a0=1+al;a1=-2*c;a2=1-al;}
        else if(b.type==1){b0=1+al*A;b1=-2*c;b2=1-al*A;a0=1+al/A;a1=-2*c;a2=1-al/A;}
        else{double sq=2*std::sqrt(A)*al;if(b.type==2){b0=A*((A+1)-(A-1)*c+sq);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-sq);a0=(A+1)+(A-1)*c+sq;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-sq;}
            else{b0=A*((A+1)+(A-1)*c+sq);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-sq);a0=(A+1)-(A-1)*c+sq;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-sq;}}
        b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;}
    double run(int ch,double x){double y=b0*x+z1[ch];z1[ch]=b1*x-a1*y+z2[ch];z2[ch]=b2*x-a2*y;return y;}
    bool finite()const{return std::isfinite(z1[0])&&std::isfinite(z1[1])&&std::isfinite(z2[0])&&std::isfinite(z2[1]);}
    void reset(){z1[0]=z1[1]=z2[0]=z2[1]=0;}};
// The EQ curve the page draws: the bands' combined gain at f (boosts at full, i.e. for a quiet sound; loud ones get less).
inline double responseDb(const Preset& p,double f){double w=2*Pi*f/Rate,db=0;std::complex<double> z=std::polar(1.0,-w),z2=z*z;
    for(auto& b:p.bands){Biquad q;q.set(b);db+=20*std::log10(std::abs((q.b0+q.b1*z+q.b2*z2)/(1.0+q.a1*z+q.a2*z2)));}return db;}
// A built-in preset as a 사용자 지정 starting point: its curve sampled at the six points, and its compressor and make-up.
inline Custom customFrom(const Preset& p){Custom c;for(int k=0;k<6;k++)c.g[k]=std::round(std::clamp(responseDb(p,CustomHz[k]),-12.0,6.0)*2)/2;
    c.comp=std::round(std::clamp((p.ratio-1)/4,0.0,1.0)*100);c.lift=std::clamp(p.makeup,0.0,6.0);return c;}
// Ear safety (the 2026-11-26 fix: 발소리 집중 made gunshots hurt): three things keep a boost from making loud sounds louder.
//  1. Loud sounds never get louder than the game made them: the final limiter's ceiling follows the game's own level (the
//     dry input's peak) and only quiet input may come out above it (up to +Allow dB at -36 dBFS and below, none at -16 dBFS
//     and above). A gunshot leaves at most as loud as it went in, whatever the EQ and make-up did.
//  2. Boosting EQ bands are dynamic: their gain follows how quiet the input is (full at -30 dBFS and below, none at -14 dBFS
//     and above), so a footstep gets the presence lift and the crack of a shot in the same band does not. Cuts stay fixed.
//  3. The compressor looks 2 ms ahead (it acts on a delayed signal), so the first milliseconds of a shot are already turned
//     down instead of slipping through before the gain reduction starts.
class Chain {
    static const int Look=96; // 2 ms, used by the compressor and again by the limiter
    static constexpr double Allow=9; // dB above the game's own level that quiet sounds may reach
    std::vector<Biquad> fixedEq,dynEq;std::vector<Band> dynBands;double dynQ=-1;int dynTick=0;Preset custom=customPreset(Custom());
    const Preset& pick(int i)const{return i==3?custom:preset(i);}
    double thr=0,slope=0,att=0,rel=0,makeup=1,env=-120;int cur=-1,want=0;
    double sideEnv=-120,sideAtt=0,sideRel=0,quiet=1; // the dry input's level, for the dynamic EQ
    double cdl[Look][2]={},cdry[Look]={};int cpos=0; // compressor delay line (and the dry peak travelling with it)
    double dl[Look][2]={},need[Look]={};int pos=0;double lg=1,lrel=0,capEnv=0,capRel=0; // limiter
    double loud=0,loudRel=0;int loudHold=0; // the loudest recent input, held 350 ms: no extra room right after a shot
    double fade=0,fadeTo=0,fadeStep=1/(.12*Rate);bool bad=false;
    // the high-pass is fixed; every other band is tuned by tuneDyn (its boost scaled by how quiet the input is, a cut kept whole)
    void apply(int i){if(!valid(pick(i)))i=0;const Preset& q=pick(i);fixedEq.clear();dynEq.clear();dynBands.clear();
        for(auto& b:q.bands){if(b.type!=0){dynBands.push_back(b);dynEq.emplace_back();}else{fixedEq.emplace_back();fixedEq.back().set(b);}}
        dynQ=-1;comp(q);env=-120;cur=i;}
    void comp(const Preset& q){thr=q.threshold;slope=1-1/q.ratio;att=std::exp(-1/(q.attackMs*.001*Rate));rel=std::exp(-1/(q.releaseMs*.001*Rate));makeup=std::pow(10,q.makeup/20);}
    void tuneDyn(){if(std::fabs(quiet-dynQ)<.01)return;dynQ=quiet;for(size_t k=0;k<dynBands.size();k++){Band b=dynBands[k];if(b.db>0)b.db*=quiet;dynEq[k].set(b);}}
public:
    Chain(){lrel=std::exp(-1/(.06*Rate));capRel=std::exp(-1/(.03*Rate));loudRel=std::exp(-1/(.4*Rate));sideAtt=std::exp(-1/(.002*Rate));sideRel=std::exp(-1/(.15*Rate));for(int i=0;i<Look;i++)need[i]=1;apply(0);}
    // on: fade in over 120 ms (from silence) or out over 80 ms. A preset change dips to silence, swaps, and comes back.
    void enable(bool on){fadeTo=on?1:0;}
    void setPreset(int i){want=std::clamp(i,0,3);}
    // 사용자 지정 changed while it plays: the same bands get new gains in place (no fade, no reset of the filters' state), so
    // dragging a point is heard at once without clicks. An invalid preset is ignored.
    void setCustom(const Preset& p){if(!valid(p))return;bool same=p.bands.size()==custom.bands.size();for(size_t k=0;same&&k<p.bands.size();k++)same=p.bands[k].type==custom.bands[k].type;custom=p;
        if(cur!=3){return;}if(!same){cur=-1;return;} /* a new shape: dip and rebuild like a preset change */
        size_t f=0,d=0;for(auto& b:p.bands){if(b.type!=0)dynBands[d++]=b;else fixedEq[f++].set(b);}dynQ=-1;comp(p);}
    int presetNow()const{return cur;}
    bool fault()const{return bad;}
    double level()const{return fade;}
    // In place, n frames of interleaved stereo. Returns false after a fault (the buffer is then silence). Latency 4 ms.
    bool process(float* s,size_t n){
        if(bad){std::fill(s,s+n*2,0.f);return false;}
        for(size_t i=0;i<n;i++){double l=s[i*2],r=s[i*2+1];if(!std::isfinite(l))l=0;if(!std::isfinite(r))r=0;
            double target=want!=cur?0:fadeTo; // a preset switch waits for the fade to reach silence
            if(fade<target)fade=std::min(target,fade+fadeStep);else if(fade>target)fade=std::max(target,fade-fadeStep*1.5);
            if(want!=cur&&fade<=0)apply(want);
            double dry=std::max(std::fabs(l),std::fabs(r)),ddb=dry>1e-6?20*std::log10(dry):-120;
            sideEnv=ddb>sideEnv?sideAtt*sideEnv+(1-sideAtt)*ddb:sideRel*sideEnv+(1-sideRel)*ddb;
            quiet=std::clamp((-14-sideEnv)/16,0.0,1.0);if(dynTick--<=0){dynTick=32;tuneDyn();}
            for(auto& b:fixedEq){l=b.run(0,l);r=b.run(1,r);}for(auto& b:dynEq){l=b.run(0,l);r=b.run(1,r);}
            // compressor: the level is measured as the sample enters the delay line; the gain is applied to the sample leaving it
            double peak=std::max(std::fabs(l),std::fabs(r)),db=peak>1e-6?20*std::log10(peak):-120;
            env=db>env?att*env+(1-att)*db:rel*env+(1-rel)*db;double gr=env>thr?(env-thr)*slope:0,g=std::pow(10,-gr/20)*makeup*fade;
            double cl=cdl[cpos][0],cr=cdl[cpos][1],cd=cdry[cpos];cdl[cpos][0]=l;cdl[cpos][1]=r;cdry[cpos]=dry;cpos=(cpos+1)%Look;
            l=cl*g;r=cr*g;
            // the ceiling for this sample: the game's own level (peak-held, released over 30 ms so a shot's tail cannot be lifted
            // up to the shot), plus room only when it has been quiet for a while (a shot's echo stays at its own level)
            capEnv=cd>capEnv?cd:capRel*capEnv+(1-capRel)*cd;
            if(cd>=loud){loud=cd;loudHold=(int)(.35*Rate);}else if(loudHold>0)loudHold--;else loud*=loudRel;
            double ldb=loud>1e-6?20*std::log10(loud):-120;
            double cap=std::min(Ceiling,std::max(capEnv*std::pow(10,std::clamp((-16-ldb)/20,0.0,1.0)*Allow/20),1e-5));
            // look-ahead limiter: the sample leaving the delay line gets the smallest gain any sample in the window needs
            double pk=std::max(std::fabs(l),std::fabs(r));need[pos]=pk>cap?cap/pk:1;double ol=dl[pos][0],orr=dl[pos][1];dl[pos][0]=l;dl[pos][1]=r;pos=(pos+1)%Look;
            double mn=1;for(int k=0;k<Look;k++)mn=std::min(mn,need[k]);lg=mn<lg?mn:std::min(mn,lrel*lg+(1-lrel));
            ol*=lg;orr*=lg;
            if(!std::isfinite(ol)||!std::isfinite(orr)||!std::isfinite(env)||!std::isfinite(sideEnv)){bad=true;std::fill(s,s+n*2,0.f);return false;}
            s[i*2]=(float)std::clamp(ol,-Ceiling,Ceiling);s[i*2+1]=(float)std::clamp(orr,-Ceiling,Ceiling);}
        for(auto& b:fixedEq)if(!b.finite()){bad=true;std::fill(s,s+n*2,0.f);return false;}
        for(auto& b:dynEq)if(!b.finite()){bad=true;std::fill(s,s+n*2,0.f);return false;}
        return true;}
};
}
