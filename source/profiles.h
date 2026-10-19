#pragma once
#include <string>
#include <array>
#include <vector>
namespace settings {
struct Profile {
    unsigned gamma=100,volume=100;
    std::array<unsigned,3> brightness{{100,100,100}}, contrast{{100,100,100}};
    unsigned greenGamma=100,blueGamma=100,vibrance=50,hue=0;
    bool extended=true;
};
inline unsigned channelGamma(const Profile& p,int c){return c==0?p.gamma:c==1?p.greenGamma:p.blueGamma;}
inline void setGamma(Profile& p,int c,unsigned v){if(c==0)p.gamma=v;else if(c==1)p.greenGamma=v;else p.blueGamma=v;}
inline void allGamma(Profile& p,unsigned v){p.gamma=p.greenGamma=p.blueGamma=v;}
inline const wchar_t* profileName(int i){const wchar_t* n[]={L"일반",L"공통 게임",L"배틀그라운드",L"타르코프"};return n[i];}
using Profiles=std::array<Profile,4>;
inline Profiles defaults(){
    Profiles p{};allGamma(p[1],155);p[1].volume=10;
    allGamma(p[2],120);p[2].volume=10;p[2].vibrance=60;
    allGamma(p[3],155);p[3].volume=10;p[3].vibrance=55;return p;
}
inline std::wstring gammaText(unsigned v){return std::to_wstring(v/100)+L"."+(v%100<10?L"0":L"")+std::to_wstring(v%100);}
inline bool integer(const std::wstring& s,unsigned& out){
    if(s.empty()||s.size()>4)return false;
    unsigned n=0;for(wchar_t c:s){if(c<L'0'||c>L'9')return false;n=n*10+(c-L'0');}out=n;return true;
}
inline bool parseGamma(const std::wstring& s,unsigned& out){
    auto p=s.find(L'.');unsigned whole=0,part=0;
    if(p==std::wstring::npos){if(!integer(s,whole))return false;}
    else {auto fraction=s.substr(p+1);if(fraction.empty()||fraction.size()>2||!integer(s.substr(0,p),whole)||!integer(fraction,part))return false;if(fraction.size()==1)part*=10;}
    unsigned n=whole*100+part;if(n<50||n>300)return false;out=n;return true;
}
inline bool parseVolume(const std::wstring& s,unsigned& out){unsigned n;if(!integer(s,n)||n>100)return false;out=n;return true;}
inline std::wstring encode(const Profiles& p){return gammaText(p[0].gamma)+L","+std::to_wstring(p[0].volume)+L","+gammaText(p[1].gamma)+L","+std::to_wstring(p[1].volume);}
inline bool decode(const std::wstring& s,Profiles& out){
    std::array<std::wstring,4> fields;size_t begin=0;
    for(int i=0;i<3;i++){auto pos=s.find(L',',begin);if(pos==std::wstring::npos)return false;fields[i]=s.substr(begin,pos-begin);begin=pos+1;}
    fields[3]=s.substr(begin);Profiles candidate=defaults();
    if(!parseGamma(fields[0],candidate[0].gamma)||!parseVolume(fields[1],candidate[0].volume)||!parseGamma(fields[2],candidate[1].gamma)||!parseVolume(fields[3],candidate[1].volume))return false;
    for(int i=0;i<2;i++){allGamma(candidate[i],candidate[i].gamma);candidate[i].extended=false;}
    out=candidate;return true;
}

inline std::wstring encodeV2(const Profiles& ps,bool autoGame){
    std::wstring out=autoGame?L"1":L"0";
    for(const auto& p:ps){
        auto add=[&](unsigned v){out+=L","+std::to_wstring(v);};
        add(p.gamma);add(p.volume);
        for(auto v:p.brightness){add(v);}
        for(auto v:p.contrast){add(v);}
        add(p.greenGamma);add(p.blueGamma);add(p.vibrance);add(p.hue);add(p.extended?1:0);
    }return out;
}
inline bool decodeV2(const std::wstring& s,Profiles& out,bool& automatic){
    std::vector<unsigned> f;size_t begin=0;
    while(true){auto end=s.find(L',',begin);unsigned n;
        if(!integer(s.substr(begin,end==std::wstring::npos?end:end-begin),n))return false;
        f.push_back(n);if(end==std::wstring::npos)break;begin=end+1;
        if(f.size()>53)return false;
    }
    if(f.size()!=53||f[0]>1)return false;
    Profiles p=defaults();size_t k=1;
    for(auto& v:p){v.gamma=f[k++];v.volume=f[k++];
        for(auto& n:v.brightness){n=f[k++];}
        for(auto& n:v.contrast){n=f[k++];}
        v.greenGamma=f[k++];v.blueGamma=f[k++];v.vibrance=f[k++];v.hue=f[k++];unsigned ext=f[k++];
        if(v.gamma<50||v.gamma>300||v.greenGamma<50||v.greenGamma>300||v.blueGamma<50||v.blueGamma>300||v.volume>100||v.vibrance>100||v.hue>359||ext>1)return false;
        for(auto n:v.brightness)if(n>199)return false;
        for(auto n:v.contrast)if(n>199)return false;
        v.extended=ext!=0;
    }
    if(f[0]&&!p[0].extended)return false;
    out=p;automatic=f[0]!=0;return true;
}
// Prefer the foreground game; use a sole running game when the desktop is focused.
inline int chooseGame(int foreground,bool pubg,bool tarkov){
    if(foreground==2||foreground==3)return foreground;
    if(pubg!=tarkov)return pubg?2:3;
    return 1;
}
}
