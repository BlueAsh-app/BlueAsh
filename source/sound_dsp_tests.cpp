// g++ -std=c++17 -Wall -Wextra -Werror sound_dsp_tests.cpp -o t.exe && ./t.exe
#include "sound_dsp.h"
#include <cstdio>
#include <random>
using namespace sbdsp;
static int fails=0;
#define CHECK(c,msg) do{if(!(c)){printf("FAIL %s (line %d)\n",msg,__LINE__);fails++;}}while(0)
static std::vector<float> tone(double hz,double amp,size_t n,double panL=1,double panR=1){std::vector<float> s(n*2);for(size_t i=0;i<n;i++){double v=amp*std::sin(2*Pi*hz*i/Rate);s[i*2]=(float)(v*panL);s[i*2+1]=(float)(v*panR);}return s;}
static double peak(const std::vector<float>& s,size_t from=0){double m=0;for(size_t i=from*2;i<s.size();i++)m=std::max(m,(double)std::fabs(s[i]));return m;}
static double rms(const std::vector<float>& s,int ch,size_t from){double a=0;size_t n=0;for(size_t i=from;i<s.size()/2;i++){a+=s[i*2+ch]*s[i*2+ch];n++;}return std::sqrt(a/std::max<size_t>(1,n));}
int main(){
    for(int p=0;p<2;p++){char m[48];snprintf(m,48,"preset %d valid",p);CHECK(valid(preset(p)),m);}
    {Preset bad=preset(0);bad.bands[1].db=20;CHECK(!valid(bad),"+20 dB band refused");bad=preset(0);bad.makeup=12;CHECK(!valid(bad),"+12 dB make-up refused");}
    // 1. a +20 dB burst (amplitude 10) never leaves the ceiling, in every preset, also at 1 kHz and 60 Hz
    for(int p=0;p<2;p++)for(double hz:{60.0,1000.0,3000.0}){Chain c;c.setPreset(p);c.enable(true);auto s=tone(hz,10,48000);c.process(s.data(),48000);char m[64];snprintf(m,64,"ceiling p%d %.0f Hz peak %.3f",p,hz,peak(s));CHECK(peak(s)<=Ceiling+1e-6,m);}
    // 2. bad input: NaN / infinity become silence (no fault); a value that blows the filters up trips the fault -> silence
    {Chain c;c.enable(true);auto s=tone(1000,.1,4800);s[100]=NAN;s[201]=INFINITY;CHECK(c.process(s.data(),4800)&&!c.fault(),"NaN input is replaced");CHECK(std::isfinite(peak(s)),"output finite");
     std::vector<float> big(9600,3e38f);Chain d;d.enable(true);bool ok=d.process(big.data(),4800);char m[64];snprintf(m,64,"huge input ok=%d fault=%d peak=%g",ok,d.fault(),peak(big));CHECK(peak(big)<=Ceiling+1e-6,m);
     if(d.fault())CHECK(peak(big)==0,"after a fault the output is silence");}
    // 3. linked: a sound on the left only stays on the left; a panned sound keeps its balance through compression
    {Chain c;c.setPreset(1);c.enable(true);auto s=tone(800,.9,48000,1,0);c.process(s.data(),48000);CHECK(rms(s,1,24000)==0,"right stays silent");
     Chain d;d.setPreset(1);d.enable(true);auto t=tone(800,.9,48000,1,.5);d.process(t.data(),48000);double ratio=rms(t,1,24000)/rms(t,0,24000);char m[64];snprintf(m,64,"pan ratio %.3f",ratio);CHECK(std::fabs(ratio-.5)<.01,m);}
    // 4. fades: starts from silence, and switching off ends in silence
    {Chain c;c.enable(true);auto s=tone(1000,.3,24000);c.process(s.data(),24000);CHECK(std::fabs(s[2])<.01,"fade in starts quiet");CHECK(peak(s,12000)>.2,"then plays");
     c.enable(false);auto t=tone(1000,.3,24000);c.process(t.data(),24000);CHECK(peak(t,9600)==0,"off ends silent");}
    // 5. a preset change passes through silence and never jumps over the ceiling
    {Chain c;c.enable(true);auto s=tone(2500,.5,48000);c.process(s.data(),24000);c.setPreset(1);c.process(s.data()+48000,24000);CHECK(c.presetNow()==1,"switched");CHECK(peak(s)<=Ceiling+1e-6,"switch stays under the ceiling");
     double mn=1;for(size_t i=24000;i<30000;i++)mn=std::min(mn,(double)std::fabs(s[i*2]));CHECK(mn<.01,"dips to silence during the switch");}
    // 6. no heavy levelling: quiet noise (-30 dBFS) comes out within +-6 dB in every preset
    for(int p=0;p<2;p++){std::mt19937 rng(7);std::normal_distribution<double> nd(0,.0316);std::vector<float> s(96000*2);for(auto& v:s)v=(float)nd(rng);double in=rms(s,0,48000);
        Chain c;c.setPreset(p);c.enable(true);c.process(s.data(),96000);double out=rms(s,0,48000),db=20*std::log10(out/in);char m[64];snprintf(m,64,"quiet level p%d %+.1f dB",p,db);CHECK(std::fabs(db)<6,m);}
    // 7. ear safety: a loud shot-like burst (noise, after quiet footstep-level sound) never leaves louder than it came in
    for(int p=0;p<2;p++){std::mt19937 rng(3);std::normal_distribution<double> nd(0,1);std::vector<float> s(72000*2);
        for(size_t i=0;i<72000;i++){double a=i<48000?.01:(i<52800?.35*std::exp(-(double)(i-48000)/1200):.01);double v=a*nd(rng);s[i*2]=(float)v;s[i*2+1]=(float)v;}
        double in=peak(s,48000);Chain c;c.setPreset(p);c.enable(true);c.process(s.data(),72000);double out=peak(s,48000);
        char m[64];snprintf(m,64,"shot p%d in %.3f out %.3f",p,in,out);CHECK(out<=in*1.02,m);}
    // 8. the presence lift is for quiet sounds only: a quiet 2.5 kHz tone gets louder in 발소리 집중, a loud one does not
    {auto q=tone(2500,.01,48000);Chain c;c.setPreset(1);c.enable(true);c.process(q.data(),48000);double up=20*std::log10(rms(q,0,24000)/(.01/std::sqrt(2)));
     auto L=tone(2500,.5,48000);Chain d;d.setPreset(1);d.enable(true);d.process(L.data(),48000);double lu=20*std::log10(rms(L,0,24000)/(.5/std::sqrt(2)));
     char m[80];snprintf(m,80,"quiet tone %+.1f dB, loud tone %+.1f dB",up,lu);CHECK(up>3&&lu<=.2,m);}
    // 9. a shot's tail is not lifted toward the shot (it made a shot sound like several): the tail 40..300 ms after a loud
    //    burst comes out under 2 dB above its own level (it was +3.6) in every preset
    for(int p=0;p<2;p++){std::mt19937 rng(5);std::normal_distribution<double> nd(0,1);std::vector<float> s(72000*2),in(72000*2);
        for(size_t i=0;i<72000;i++){double a=i<48000?0:(i<48480?.6:.016);double v=a*nd(rng);s[i*2]=s[i*2+1]=in[i*2]=in[i*2+1]=(float)v;}
        Chain c;c.setPreset(p);c.enable(true);c.process(s.data(),72000);auto win=[](const std::vector<float>& v){double a=0;for(size_t i=48000+1920;i<48000+14400;i++)a+=v[i*2]*v[i*2];return std::sqrt(a);};
        double lift=20*std::log10(win(s)/win(in));char m[64];snprintf(m,64,"shot tail p%d %+.1f dB",p,lift);CHECK(lift<2,m);}
    // 9. 사용자 지정: valid at its limits, the curve the page draws follows the points, and changing it while it plays
    //    neither faults, jumps over the ceiling, nor dips to silence (the gains change in place)
    {Custom f;CHECK(std::fabs(responseDb(customPreset(f),1000))<.2,"flat custom is flat");
     Custom hi;for(double& g:hi.g)g=99;hi.comp=500;hi.lift=99;CHECK(valid(customPreset(hi)),"custom clamps to the limits");
     Custom c;c.g[3]=6;double at=responseDb(customPreset(c),2500);char m[64];snprintf(m,64,"2.5 kHz point +6 -> curve %+.1f dB",at);CHECK(at>5&&at<7.5,m);
     for(int p=0;p<2;p++)CHECK(valid(customPreset(customFrom(preset(p)))),"built-in -> custom is valid");
     Chain ch;ch.setCustom(customPreset(Custom()));ch.setPreset(3);ch.enable(true);auto s=tone(2500,.01,96000);ch.process(s.data(),48000);CHECK(ch.presetNow()==3,"custom selected");
     ch.setCustom(customPreset(c));ch.process(s.data()+96000,48000);CHECK(!ch.fault()&&peak(s)<=Ceiling+1e-6,"live custom change is safe");
     double mn=1;for(size_t i=48000;i<52000;i++)mn=std::min(mn,(double)std::fabs(s[i*2])+std::fabs(s[i*2+2]));CHECK(mn>1e-4,"no dip to silence on a live change");}
    if(!fails){printf("sound_dsp ok\n");}
    return fails?1:0;}
