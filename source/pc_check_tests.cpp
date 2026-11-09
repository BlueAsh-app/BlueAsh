#include <cassert>
#include <cstdio>
#include "pc_check.h"
using namespace pccheck;
static bool has(const std::vector<Line>& v,const wchar_t* part){for(auto& l:v){if(std::wstring(l.key).find(part)!=std::wstring::npos)return true;}return false;}
int main(){
    // Names as Windows reports them.
    assert(gpuScore(L"NVIDIA GeForce RTX 5080")==72&&gpuGrade(72)==High);
    assert(gpuScore(L"NVIDIA GeForce RTX 4070 Ti SUPER")==58&&gpuScore(L"NVIDIA GeForce RTX 4070 Ti")==54&&gpuScore(L"NVIDIA GeForce RTX 4070")==44);
    assert(gpuScore(L"AMD Radeon RX 7900 XTX")==66&&gpuScore(L"AMD Radeon RX 7900 XT")==56&&gpuScore(L"Radeon RX 580 Series")==9);
    assert(gpuScore(L"NVIDIA GeForce RTX 4060 Laptop GPU")==29*7/10);
    assert(gpuScore(L"Intel(R) UHD Graphics 770")==3&&gpuScore(L"Microsoft Basic Render Driver")==-1);
    assert(cpuScore(L"AMD Ryzen 7 5800X 8-Core Processor")==59&&cpuGrade(59)==Upper);
    assert(cpuScore(L"AMD Ryzen 7 7800X3D 8-Core Processor")==89&&cpuGrade(89)==High);
    assert(cpuScore(L"AMD Ryzen 5 3600 6-Core Processor")==40&&cpuGrade(40)==Mid);
    assert(cpuScore(L"12th Gen Intel(R) Core(TM) i7-12700K")==62&&cpuScore(L"Intel(R) Core(TM) i9-14900K")==77);
    assert(cpuScore(L"Intel(R) Core(TM) i5-9600K CPU @ 3.70GHz")==28&&cpuScore(L"Intel(R) Core(TM) Ultra 9 285K")==73);
    assert(cpuScore(L"AMD Ryzen Threadripper 3970X")==-1&&cpuScore(L"Something else")==-1);
    assert(ramGrade(32,26,2666)==Mid&&ramGrade(32,26,3600)==Upper&&ramGrade(32,34,6000)==High&&ramGrade(8,34,6000)==Mid&&ramGrade(16,0,0)==Upper);
    // This PC: RTX 5080 + Ryzen 7 5800X + 4 x 8 GB DDR4-2666, 4K screen.
    {Input in;in.gpu=L"NVIDIA GeForce RTX 5080";in.vramGB=16;in.cpu=L"AMD Ryzen 7 5800X 8-Core Processor";in.ramGB=32;in.ddr=26;in.rated=2666;in.configured=2666;in.modules=4;in.storage=L"NVMe SSD";in.width=3840;
     auto r=check(in);assert(r.gpu.grade==High&&r.cpu.grade==Upper&&r.ram.grade==Mid&&r.storage==2);
     assert(has(r.findings,L"CPU가 그래픽카드보다")&&r.findings[0].level==2&&has(r.findings,L"4K")&&has(r.findings,L"램 속도"));
     assert(has(r.upgrades,L"5700X3D")&&has(r.upgrades,L"DDR4 3600")&&!has(r.upgrades,L"그래픽카드"));}
    // XMP off and one stick: the free fixes come first, no RAM purchase advice.
    {Input in;in.gpu=L"NVIDIA GeForce RTX 4070";in.cpu=L"AMD Ryzen 7 7700X";in.ramGB=16;in.ddr=34;in.rated=6000;in.configured=4800;in.modules=1;in.storage=L"SSD";
     auto r=check(in);assert(has(r.findings,L"제품 속도")&&has(r.findings,L"싱글 채널"));assert(std::wstring(r.upgrades[0].key).find(L"XMP")!=std::wstring::npos&&has(r.upgrades,L"듀얼 채널")&&!has(r.upgrades,L"램: DDR"));}
    // Balanced high-end PC: nothing to change.
    {Input in;in.gpu=L"NVIDIA GeForce RTX 4090";in.vramGB=24;in.cpu=L"AMD Ryzen 7 9800X3D 8-Core Processor";in.ramGB=32;in.ddr=34;in.rated=6000;in.configured=6000;in.modules=2;in.storage=L"NVMe SSD";
     auto r=check(in);assert(r.findings.size()==1&&r.findings[0].level==0&&r.upgrades.size()==1&&r.upgrades[0].level==0);}
    // Weak card with a strong CPU, HDD.
    {Input in;in.gpu=L"NVIDIA GeForce GTX 1660 SUPER";in.vramGB=6;in.cpu=L"Intel(R) Core(TM) i7-13700K";in.ramGB=32;in.ddr=34;in.rated=5600;in.configured=5600;in.modules=2;in.storage=L"HDD";
     auto r=check(in);assert(has(r.findings,L"그래픽카드가 CPU보다")&&has(r.findings,L"HDD")&&has(r.findings,L"그래픽카드 메모리"));assert(has(r.upgrades,L"RTX 5070")&&has(r.upgrades,L"SSD")&&!has(r.upgrades,L"CPU:"));}
    // Old Intel with a fast card: a new platform, and no separate DDR4 advice.
    {Input in;in.gpu=L"NVIDIA GeForce RTX 4080";in.cpu=L"Intel(R) Core(TM) i7-8700K CPU @ 3.70GHz";in.ramGB=16;in.ddr=26;in.rated=2666;in.configured=2666;in.modules=2;
     auto r=check(in);assert(has(r.upgrades,L"새 플랫폼")&&!has(r.upgrades,L"램: DDR4"));}
    // Unknown parts: no grades, no claims about them.
    {Input in;in.gpu=L"Virtual GPU";in.cpu=L"Virtual CPU";in.ramGB=16;auto r=check(in);assert(r.gpu.grade==Unknown&&r.cpu.grade==Unknown&&!has(r.findings,L"CPU가")&&!has(r.upgrades,L"CPU:"));}
    puts("pc check tests passed");
}
