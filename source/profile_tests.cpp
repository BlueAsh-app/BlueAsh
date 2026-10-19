#include "profiles.h"
#include <cassert>
int main(){
    using namespace settings;
    unsigned v=0;
    assert(parseGamma(L"0.50",v)&&v==50);assert(parseGamma(L"3",v)&&v==300);
    assert(parseGamma(L"1.55",v)&&v==155);assert(parseGamma(L"1.5",v)&&v==150);
    for(auto s:{L"",L"NaN",L"inf",L"-1",L"3.01",L"0.49",L"1.555",L"1,55",L"1.2x",L"1e2"})assert(!parseGamma(s,v));
    assert(parseVolume(L"0",v)&&v==0);assert(parseVolume(L"100",v)&&v==100);
    for(auto s:{L"-1",L"101",L"10.5",L"",L"10%"})assert(!parseVolume(s,v));
    auto p=defaults();assert(encode(p)==L"1.00,100,1.55,10");
    assert(decode(L"1.25,80,2.10,0",p));assert(p[0].gamma==125&&p[0].volume==80&&p[1].gamma==210&&p[1].volume==0);
    auto encoded=encode(p);Profiles q;assert(decode(encoded,q)&&encode(q)==encoded);
    assert(!decode(L"1.25,80,2.10,101",p));assert(encode(p)==encoded);
    assert(!decode(L"1.25,80,2.10,0,5",p));assert(encode(p)==encoded);

    // Preserve legacy values while initializing new game profiles.
    assert(!p[0].extended&&!p[1].extended);
    assert(p[0].greenGamma==125&&p[1].blueGamma==210);
    assert(p[2].gamma==120&&p[3].gamma==155);
    p[0].extended=true;p[0].brightness={98,101,103};p[1].contrast={100,110,120};
    p[2].greenGamma=133;p[2].blueGamma=177;p[3].hue=359;p[3].vibrance=0;
    bool automatic=false;auto v2=encodeV2(p,true);
    assert(decodeV2(v2,q,automatic)&&automatic&&encodeV2(q,automatic)==v2);
    assert(q[0].brightness[2]==103&&q[2].greenGamma==133&&q[3].hue==359);
    auto before=encodeV2(q,automatic);
    for(const auto& bad:{v2+L",5",v2.substr(0,v2.rfind(L',')),L"2"+v2.substr(1),std::wstring(L""),std::wstring(L"1,NaN")}){
        assert(!decodeV2(bad,q,automatic));assert(encodeV2(q,automatic)==before);
    }
    auto invalid=p;invalid[2].contrast[1]=200;assert(!decodeV2(encodeV2(invalid,true),q,automatic));
    invalid=p;invalid[1].gamma=301;assert(!decodeV2(encodeV2(invalid,true),q,automatic));
    invalid=p;invalid[3].hue=360;assert(!decodeV2(encodeV2(invalid,true),q,automatic));
    invalid=p;invalid[0].extended=false;assert(!decodeV2(encodeV2(invalid,true),q,automatic));
    assert(chooseGame(2,true,true)==2&&chooseGame(3,true,true)==3);
    assert(chooseGame(0,true,false)==2&&chooseGame(0,false,true)==3);
    assert(chooseGame(0,true,true)==1&&chooseGame(0,false,false)==1);
}
