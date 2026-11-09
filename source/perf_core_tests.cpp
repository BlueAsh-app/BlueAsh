#include <cassert>
#include <cstdio>
#include "perf_core.h"
using namespace perf;
static std::vector<double> frames(double ms,int n){return std::vector<double>(n,ms);}
static bool has(const std::vector<Finding>& f,Kind k){for(auto& x:f)if(x.kind==k)return true;return false;}
static Conf confOf(const std::vector<Finding>& f,Kind k){for(auto& x:f)if(x.kind==k)return x.conf;return None;}
int main(){
    // Steady 10 ms frames for 20 s: 100 FPS, 1 % low 100, 20 per-second buckets.
    auto s=stats(frames(10,2000));assert(s.frames==2000&&std::fabs(s.fps-100)<.01&&std::fabs(s.low1-100)<.01&&s.perSec.size()==20&&s.sdMs<1e-9);
    // 1 % low comes from the 99th percentile frame time: 1 % of frames at 50 ms -> 1 % low 20 FPS.
    {auto v=frames(10,1980);for(int i=0;i<20;i++)v.push_back(50);auto t=stats(v);assert(std::fabs(t.low1-20)<.01&&t.fps<100);}
    // Bad input: empty, negative, absurd values are dropped; nothing divides by zero.
    assert(stats({}).frames==0&&stats({-5,0,1e9}).frames==0);
    // Too short: never a bottleneck verdict.
    {auto f=diagnose(stats(frames(10,100)),Sample());assert(f.size()==1&&f[0].kind==Unclear&&f[0].conf==None);}
    // GPU at 98 %: GPU limit, medium on one session, high when an earlier session agreed.
    {Sample x;x.gpuUtil=98;x.cpuMaxCore=60;assert(confOf(diagnose(s,x),GpuLimit)==Medium);assert(confOf(diagnose(s,x,1),GpuLimit)==High);}
    // GPU under 100 % alone is not a CPU limit: needs a saturated core.
    {Sample x;x.gpuUtil=70;x.cpuMaxCore=50;auto f=diagnose(s,x);assert(!has(f,CpuLimit)&&has(f,Unclear));}
    {Sample x;x.gpuUtil=70;x.cpuMaxCore=97;assert(confOf(diagnose(s,x),CpuLimit)==Medium);}
    // Unknown GPU load: undecided, not a guess.
    {auto f=diagnose(s,Sample());assert(has(f,Unclear)&&!has(f,GpuLimit)&&!has(f,CpuLimit));}
    // A frame cap (100 FPS limit) hides the bottleneck: capped, no GPU/CPU claim.
    {Sample x;x.limit=100;x.gpuUtil=99;auto f=diagnose(s,x);assert(has(f,Capped)&&!has(f,GpuLimit));}
    {Sample x;x.vsync=1;x.refresh=100;x.gpuUtil=60;x.cpuMaxCore=99;auto f=diagnose(s,x);assert(has(f,Capped)&&!has(f,CpuLimit));}
    // Frame generation lowers GPU/CPU confidence.
    {Sample x;x.gpuUtil=99;x.frameGen=true;assert(confOf(diagnose(s,x),GpuLimit)==Low);}
    // VRAM, RAM, RAM speed, power plan, background.
    {Sample x;x.gpuUtil=99;x.vramUsed=7.8;x.vramTotal=8;x.ramAvailPct=8;x.ramTotalGB=16;x.ratedMts=6000;x.configuredMts=4800;x.powerSaver=true;x.topProcCpu=35;
     auto f=diagnose(s,x);assert(has(f,Vram)&&has(f,RamCapacity)&&has(f,RamSpeed)&&has(f,PowerThermal)&&confOf(f,Background)==Medium&&confOf(f,RamSpeed)==Low);}
    // Before / after: same numbers = no change; +20 % steady = improved; -20 % = worse; short = insufficient.
    {auto a=stats(frames(10,2000)),b=stats(frames(10,2000));assert(compare(a,b).v==NoChange);
     auto c=stats(frames(1000/120.0,2400));assert(compare(a,c).v==Improved);auto d=stats(frames(1000/80.0,1600));assert(compare(a,d).v==Worse);
     assert(compare(a,stats(frames(10,300))).v==Insufficient);}
    // A 2 % change is inside the 3 % floor: no change.
    {auto a=stats(frames(10,2000)),b=stats(frames(1000/102.0,2040));assert(compare(a,b).v==NoChange);}
    // Target: met, near, missed, capped below, no data.
    assert(target(s,100,0,0,-1)==Met);assert(target(s,105,0,0,-1)==Near);assert(target(s,144,0,0,-1)==Missed);assert(target(s,144,120,0,-1)==CappedBelow);
    assert(target(s,144,0,60,1)==CappedBelow);assert(target(stats(frames(10,50)),60,0,0,-1)==NoData);assert(target(s,0,0,0,-1)==NoData);
    // Advice: target met -> no upgrade; GPU limit -> settings then GPU; quality first -> no settings step; no data -> more data.
    {Sample x;x.gpuUtil=99;auto f=diagnose(s,x);auto a=advise(f,Met,1);assert(a.size()==1&&a[0].kind==NoUpgrade);
     a=advise(f,Missed,1);assert(a[0].kind==TuneSettings&&a[1].kind==GpuUpgrade&&a[1].conf==Low);
     a=advise(f,Missed,0);assert(a[0].kind==GpuUpgrade);}
    {Sample x;x.gpuUtil=60;x.cpuMaxCore=50;auto a=advise(diagnose(s,x),Missed,1);assert(a.back().kind==MoreData);}
    assert(advise({},NoData,1)[0].kind==MoreData);
    {Sample x;x.gpuUtil=60;x.cpuMaxCore=98;auto a=advise(diagnose(s,x),Missed,2);bool cpu=false;for(auto& k:a)cpu=cpu||k.kind==CpuUpgrade;assert(cpu);}
    // History line round trip; tabs/equals in names cannot break the format; garbage is rejected.
    {Session a;a.game=L"steam-1\t=x";a.fps=123.5;a.low1=80;a.frames=2470;a.gpuUtil=97;a.main=GpuLimit;Session b;assert(parse(line(a),b)&&b.game==L"steam-1  x"&&std::fabs(b.fps-123.5)<.01&&b.main==GpuLimit&&b.frames==2470);
     assert(!parse(L"",b)&&!parse(L"hello",b)&&!parse(L"fps=1e9\tgame=x",b));}
    puts("perf core tests passed");
}
