#pragma once
// 내 PC 성능 체크 (pure: no Windows calls, unit-tested in pc_check_tests.cpp).
// Grades each part from its name and specs (graphics card, CPU, RAM, storage), says where one part holds the others back
// and what to change first. The scores are rough gaming-relative tiers from the model name (100 = fastest current part),
// not a measurement: the page says so, and an unknown name gets no grade and no claim.
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>
namespace pccheck {
enum Grade {Unknown=-1,Entry=0,Mid=1,Upper=2,High=3};
struct Part {int score=-1;Grade grade=Unknown;};
// level 2 = attention (orange), 1 = note, 0 = fine. key is the Korean source text (translated by the page, %ls = arg).
struct Line {int level;const wchar_t* key;std::wstring arg;};
struct Input {std::wstring gpu,cpu;double vramGB=0,ramGB=0;int ddr=0,rated=0,configured=0,modules=0;std::wstring storage;int width=0;};
struct Report {Part gpu,cpu,ram;int storage=-1;std::vector<Line> findings,upgrades;};

inline std::wstring lower(std::wstring s){for(auto& c:s)c=(wchar_t)towlower(c);return s;}
// The table key found in the name and not followed by another digit or letter of a longer model ("rtx 4070" in "rtx 4070 ti").
inline bool hasModel(const std::wstring& n,const std::wstring& k){for(size_t p=n.find(k);p!=std::wstring::npos;p=n.find(k,p+1)){size_t e=p+k.size();
    if(e>=n.size()){return true;}if(iswdigit(n[e]))continue;if(n[e]==L' '&&(!n.compare(e,3,L" ti")||!n.compare(e,6,L" super")||!n.compare(e,3,L" xt")||!n.compare(e,4,L" gre")))continue;
    if(iswalpha(n[e])){continue;}return true;}return false;}
inline int gpuScore(const std::wstring& name){std::wstring n=lower(name);
    static const std::pair<const wchar_t*,int> t[]={
        {L"rtx 5090",100},{L"rtx 4090",88},{L"rtx 5080",72},{L"rtx 4080 super",66},{L"rtx 4080",64},{L"rtx 5070 ti",62},{L"rtx 4070 ti super",58},{L"rtx 3090 ti",55},
        {L"rtx 4070 ti",54},{L"rtx 3090",50},{L"rtx 5070",50},{L"rtx 4070 super",50},{L"rtx 3080 ti",49},{L"rtx 3080",46},{L"rtx 4070",44},{L"rtx 5060 ti",40},
        {L"rtx 3070 ti",39},{L"rtx 3070",37},{L"rtx 2080 ti",36},{L"rtx 4060 ti",35},{L"rtx 5060",34},{L"rtx 3060 ti",33},{L"rtx 2080 super",30},{L"rtx 4060",29},
        {L"rtx 2080",28},{L"rtx 2070 super",27},{L"rtx 3060",26},{L"gtx 1080 ti",25},{L"rtx 2070",24},{L"rtx 2060 super",22},{L"rtx 2060",20},{L"gtx 1080",20},
        {L"gtx 1070 ti",18},{L"rtx 3050",17},{L"gtx 1660 ti",16},{L"gtx 1660 super",16},{L"gtx 1070",16},{L"gtx 1660",14},{L"gtx 1060",12},{L"gtx 1650",10},{L"gtx 1050 ti",7},
        {L"rx 7900 xtx",66},{L"rx 9070 xt",60},{L"rx 7900 xt",56},{L"rx 9070",52},{L"rx 7900 gre",48},{L"rx 6950 xt",46},{L"rx 7800 xt",44},{L"rx 6900 xt",43},
        {L"rx 6800 xt",40},{L"rx 7700 xt",37},{L"rx 6800",34},{L"rx 9060 xt",32},{L"rx 6750 xt",30},{L"rx 6700 xt",28},{L"rx 7600 xt",25},{L"rx 7600",23},
        {L"rx 6650 xt",23},{L"rx 6600 xt",21},{L"rx 6600",19},{L"rx 5700 xt",18},{L"rx 5700",16},{L"rx 5600 xt",14},{L"rx 580",9},{L"rx 570",8},
        {L"arc b580",24},{L"arc b570",21},{L"arc a770",20},{L"arc a750",18}};
    for(auto& [k,s]:t){if(hasModel(n,k))return n.find(L"laptop")!=std::wstring::npos?s*7/10:s;}
    if(n.find(L"graphics")!=std::wstring::npos&&n.find(L"rtx")==std::wstring::npos&&n.find(L"rx ")==std::wstring::npos){return 3;} // integrated (Intel UHD / Iris, Radeon Graphics)
    return -1;}
// CPU: gaming-relative. Ryzen by generation (first model digit), model tier (second digit) and the X3D cache; Intel Core
// by generation (first one or two digits) and i3..i9; Core Ultra by series. Laptop parts (U / H) lose a step.
inline int cpuScore(const std::wstring& name){std::wstring n=lower(name);size_t p;
    if((p=n.find(L"ryzen "))!=std::wstring::npos){p+=6;while(p<n.size()&&!iswdigit(n[p]))p++;while(p<n.size()&&iswdigit(n[p]))p++;while(p<n.size()&&!iswdigit(n[p]))p++; // skip the 3/5/7/9 class
        if(p+4>n.size()||!iswdigit(n[p+3])){return -1;}int gen=n[p]-L'0',tier=n[p+1]-L'0';std::wstring suf;for(size_t i=p+4;i<n.size()&&iswalnum(n[i]);i++)suf+=n[i];
        static const int base[10]={-1,24,30,40,44,54,-1,68,70,72};int s=base[gen];if(s<0)return -1;s+=tier>=7?5:0;s+=tier>=9?2:0;
        if(suf.find(L"x3d")!=std::wstring::npos)s+=16;else if(suf.find(L'u')!=std::wstring::npos)s-=15;else if(suf.find(L'h')!=std::wstring::npos)s-=6;else if(suf==L"g"||suf==L"ge")s-=4;return s;}
    if((p=n.find(L"ultra "))!=std::wstring::npos){p+=6;if(p+4>n.size()||!iswdigit(n[p]))return -1;int cls=n[p]-L'0';size_t m=p+1;while(m<n.size()&&!iswdigit(n[m]))m++;
        std::wstring suf;size_t e=m;while(e<n.size()&&iswdigit(n[e]))e++;for(;e<n.size()&&iswalpha(n[e]);e++)suf+=n[e];
        bool desk=m<n.size()&&n[m]==L'2'&&(suf.empty()||suf==L"k"||suf==L"f"||suf==L"kf");return (desk?64:48)+(cls>=9?9:cls>=7?6:0);}
    for(const wchar_t* c:{L"i3-",L"i5-",L"i7-",L"i9-"}){if((p=n.find(c))==std::wstring::npos)continue;int cls=c[1]-L'0';p+=3;std::wstring d;while(p<n.size()&&iswdigit(n[p]))d+=n[p++];
        if(d.size()<4){return -1;}int gen=d.size()>=5?std::stoi(d.substr(0,2)):d[0]-L'0';std::wstring suf;while(p<n.size()&&iswalpha(n[p]))suf+=n[p++];
        static const int base[15]={-1,-1,5,7,10,12,16,18,24,28,34,38,56,66,68};if(gen<2||gen>14)return -1;int s=base[gen]+(cls==3?-10:cls==7?6:cls==9?9:0);
        if(suf.find(L'u')!=std::wstring::npos)s-=15;else if(suf.find(L'h')!=std::wstring::npos)s-=6;return s;}
    return -1;}
inline Grade gpuGrade(int s){return s<0?Unknown:s>=60?High:s>=38?Upper:s>=20?Mid:Entry;}
inline Grade cpuGrade(int s){return s<0?Unknown:s>=76?High:s>=52?Upper:s>=36?Mid:Entry;}
// RAM: the lower of capacity and speed (the speed it runs at). DDR4 tops out at 상급: DDR5 is the current high end.
inline bool ddr5(int t){return t==34||t==35;}
inline bool ddr4(int t){return t==26||t==30;}
inline Grade ramGrade(double gb,int ddr,int mts){if(gb<=0)return Unknown;int cap=gb<7.5?0:gb<15.5?1:gb<31.5?2:3,spd=cap; // unknown type or speed: judged by size only
    if(mts>0&&ddr5(ddr))spd=mts>=6000?3:mts>=5200?2:1;else if(mts>0&&ddr4(ddr))spd=mts>=3200?2:mts>=2400?1:0;else if(mts>0&&ddr)spd=0; // DDR3 and older
    return (Grade)std::min(cap,spd);}
inline std::wstring num(double v){wchar_t b[32];swprintf(b,32,L"%.0f",v);return b;}

inline Report check(const Input& in){Report r;
    r.gpu.score=gpuScore(in.gpu);r.gpu.grade=gpuGrade(r.gpu.score);r.cpu.score=cpuScore(in.cpu);r.cpu.grade=cpuGrade(r.cpu.score);
    int mts=in.configured?in.configured:in.rated;r.ram.grade=ramGrade(in.ramGB,in.ddr,mts);r.ram.score=r.ram.grade;
    r.storage=in.storage==L"HDD"?0:in.storage==L"SSD"?1:in.storage==L"NVMe SSD"?2:-1;
    std::wstring c=lower(in.cpu);bool x3d=c.find(L"x3d")!=std::wstring::npos,ryzen=c.find(L"ryzen ")!=std::wstring::npos;
    int gen=0;if(ryzen){size_t p=c.find(L"ryzen ")+6;while(p<c.size()&&iswdigit(c[p]))p++;while(p<c.size()&&!iswdigit(c[p]))p++;if(p<c.size())gen=c[p]-L'0';}
    bool am4=ryzen&&gen>=1&&gen<=5,am5=ryzen&&(gen==7||gen==9),lga1700=r.cpu.score>=0&&!ryzen&&c.find(L"ultra ")==std::wstring::npos&&(c.find(L"-12")!=std::wstring::npos||c.find(L"-13")!=std::wstring::npos||c.find(L"-14")!=std::wstring::npos),
         i9=c.find(L"i9-")!=std::wstring::npos;
    bool xmpOff=in.rated>0&&in.configured>0&&in.configured<in.rated*0.92,single=in.modules==1,small=in.ramGB>0&&in.ramGB<15.5,
         slow=mts>0&&((ddr5(in.ddr)&&mts<5200)||(ddr4(in.ddr)&&mts<3000));
    // Findings: what holds the PC back, most important first.
    int g=r.gpu.grade,k=r.cpu.grade;bool cpuLimit=g>=0&&k>=0&&g>k,gpuLimit=g>=0&&k>=0&&k-g>=2;
    if(cpuLimit){r.findings.push_back({g-k>=2||g==High?2:1,L"CPU가 그래픽카드보다 낮은 급이라, 높은 FPS를 노리는 게임에서는 CPU가 먼저 한계에 닿습니다.",L""});
        if(in.width>=3840){r.findings.push_back({1,L"4K에서는 이 차이가 덜 드러납니다.",L""});}}
    if(gpuLimit)r.findings.push_back({2,L"그래픽카드가 CPU보다 낮은 급이라, 옵션을 올리면 그래픽카드가 먼저 한계에 닿습니다.",L""});
    if(xmpOff)r.findings.push_back({2,L"램이 제품 속도보다 느리게 동작 중입니다 (%ls).",num(in.rated)+L" → "+num(in.configured)+L" MT/s"});
    else if(slow)r.findings.push_back({2,L"램 속도(%ls MT/s)가 느려 CPU 성능을 다 끌어내지 못합니다.",num(mts)});
    if(single)r.findings.push_back({2,L"램이 1개라 싱글 채널로 동작합니다. FPS가 크게 떨어지는 구성입니다.",L""});
    if(small)r.findings.push_back({2,L"램 %ls GB는 요즘 게임에 부족합니다.",num(in.ramGB)});
    if(g>=0&&in.vramGB>0&&in.vramGB<7.5)r.findings.push_back({1,L"그래픽카드 메모리 %ls GB는 최신 게임의 높은 텍스처에 부족합니다.",num(in.vramGB)});
    if(r.storage==0)r.findings.push_back({2,L"저장 장치가 HDD라 로딩이 느리고 끊길 수 있습니다.",L""});
    if(r.findings.empty())r.findings.push_back({0,g>=0&&k>=0?L"그래픽카드와 CPU의 급이 잘 맞고, 큰 병목이 없습니다.":L"확인한 부품에서 큰 병목을 찾지 못했습니다.",L""});
    // Upgrades: free fixes first, then the part that limits the others. A new platform replaces the RAM too.
    bool newPlatform=false;
    if(xmpOff)r.upgrades.push_back({1,L"BIOS에서 XMP/EXPO 켜기 (무료)",L""});
    if(single)r.upgrades.push_back({1,L"같은 램을 하나 더 꽂아 듀얼 채널로",L""});
    if(cpuLimit||(k>=0&&k<=Mid&&g!=Unknown&&g>=Mid)){
        if(am4&&!x3d)r.upgrades.push_back({2,L"CPU: Ryzen 7 5700X3D·5800X3D (메인보드·램 그대로, BIOS 업데이트 먼저)",L""});
        else if(am5&&!x3d)r.upgrades.push_back({2,L"CPU: Ryzen 7 9800X3D (메인보드 그대로, BIOS 업데이트 먼저)",L""});
        else if(lga1700&&!i9)r.upgrades.push_back({2,L"CPU: 14세대 Core i7·i9 (메인보드 그대로, BIOS 업데이트 먼저)",L""});
        else{newPlatform=true;r.upgrades.push_back({2,L"CPU: Ryzen 7 9800X3D 등 새 플랫폼 (메인보드·DDR5 램 함께)",L""});}}
    if(!newPlatform&&!xmpOff&&(slow||small))r.upgrades.push_back({2,ddr5(in.ddr)?L"램: DDR5 6000 32GB (16GB×2)":L"램: DDR4 3600 32GB (16GB×2)",L""});
    if(gpuLimit||(g>=0&&g<=Mid))r.upgrades.push_back({2,g<=Mid?L"그래픽카드: RTX 5070 · RX 9070급 이상":L"그래픽카드: RTX 5070 Ti · RX 9070 XT급 이상",L""});
    if(r.storage==0)r.upgrades.push_back({2,L"게임을 SSD(NVMe)에 설치",L""});
    if(r.upgrades.empty())r.upgrades.push_back({0,L"지금 구성에서 꼭 바꿀 부품은 없습니다.",L""});
    return r;}
}
