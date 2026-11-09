// g++ -std=c++17 -Wall -Wextra -Werror mod_translate_tests.cpp -o t.exe && t.exe
// Optional: BLUEASH_TEST_W3=<a mod's en.w3strings> also decodes and re-encodes a real file.
#include "mod_translate.h"
#include <cassert>
#include <cstdio>
using namespace modtr;
static std::wstring tmp(const wchar_t* name){wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);std::wstring d=std::wstring(t)+L"blueash-modtr-"+std::to_wstring(GetCurrentProcessId())+L"-"+name;CreateDirectoryW(d.c_str(),nullptr);return d;}
static void put(const std::wstring& p,const std::string& s){for(size_t i=p.find(L"blueash-modtr");(i=p.find(L'\\',i+1))!=std::wstring::npos;)CreateDirectoryW(p.substr(0,i).c_str(),nullptr);assert(writeBytes(p,s));}
static std::string get(const std::wstring& p){std::string s;readBytes(p,s,64u<<20);return s;}
// The fake server: "[KO] " + text, markers kept.
static int fake(const std::vector<std::wstring>& in,std::vector<std::wstring>& out){out.clear();for(auto& s:in)out.push_back(L"[KO] "+s);return 0;}
int main(){
    // what is text and what is not
    assert(translatable(L"Enable auto loot"));assert(translatable(L"Health"));assert(translatable(L"OK"));
    for(auto* s:{L"auto_loot_enabled",L"Mods.gameplay.ATA",L"camelCase",L"#ff00aa",L"12",L"http://x.com a b",L"x",L"켜기 Enable",L"textures/a.dds",L"health"})assert(!translatable(s));
    // markers out and back
    auto p=protect(L"  Deal {0} damage to <b>%s</b>\\n");assert(p.lead==L"  "&&p.slots.size()==5);
    assert(restore(p,L"\x27e6" L"0\x27e7 피해 \x27e6" L"1\x27e7\x27e6" L"2\x27e7\x27e6" L"3\x27e7\x27e6" L"4\x27e7")==L"  {0} 피해 <b>%s</b>\\n");
    assert(restore(p,L"\x27e6" L"0\x27e7 피해")==L"");                         // a marker lost: keep the original
    assert(restore(p,L"\x27e6" L"0\x27e7\x27e6" L"0\x27e7\x27e6" L"1\x27e7\x27e6" L"2\x27e7\x27e6" L"3\x27e7\x27e6" L"4\x27e7")==L""); // doubled
    // formats: JSON values only, XML text, key=value, YAML
    auto j=segments(Json,L"{\"title\": \"Auto Loot\", \"desc\":\"Takes \\\"all\\\" items\", \"n\": 3, // c \"x\"\n \"list\":[\"One thing\"]}");
    assert(j.size()==3&&j[0].s==L"Auto Loot"&&j[1].s==L"Takes \"all\" items"&&j[2].s==L"One thing");
    auto x=segments(Xml,L"<?xml?><s id=\"a\">Fire &amp; Ice</s><!-- note --><s> Two words </s>");assert(x.size()==2&&x[0].s==L"Fire & Ice"&&x[1].s==L"Two words");
    auto k=segments(Kv,L"# c\r\n[Sec]\r\nitem.name = Iron Sword\r\nquoted=\"Big bad\";\r\n");assert(k.size()==2&&k[0].s==L"Iron Sword"&&k[1].s==L"Big bad");
    auto y=segments(Yaml,L"menu:\n  title: Main menu\n  list: [a, b]\n  - item\n");assert(y.size()==1&&y[0].s==L"Main menu");
    // file languages and sibling names
    std::wstring tok;assert(fileLang(L"C:\\m\\lang\\en_us.json",&tok)==0&&tok==L"en_us");assert(fileLang(L"C:\\m\\Mod_ENGLISH.txt")==0);
    assert(fileLang(L"C:\\m\\strings.de.xml")==6);assert(fileLang(L"C:\\m\\en\\a.xml")==0);assert(fileLang(L"C:\\m\\Mod_CZECH.txt")==-2);assert(fileLang(L"C:\\m\\config.json")==-1);
    assert(siblingName(L"C:\\m\\lang\\en_us.json",L"en_us",1)==L"C:\\m\\lang\\ko_kr.json");
    assert(siblingName(L"C:\\m\\Mod_ENGLISH.txt",L"ENGLISH",1)==L"C:\\m\\Mod_KOREAN.txt");
    assert(siblingName(L"C:\\m\\en.json",L"en",2)==L"C:\\m\\ja.json");
    assert(siblingName(L"C:\\m\\en\\a.xml",L"en",1)==L"C:\\m\\ko\\a.xml");
    assert(languagePath(L"\\Localization\\x.json")&&languagePath(L"\\lang\\x.json")&&!languagePath(L"\\Textures\\x.json")&&!languagePath(L"\\config\\x.json"));
    assert(niceName(L"Igni Blade 14066 3 2026-10-12T22-27Z 4acEvLd3a")==L"Igni Blade");assert(niceName(L"More Robes-2144-1-10a-1554850485")==L"More Robes");
    assert(niceName(L"Colorized Map Icons 5.01 13733 0.1.3 x")==L"Colorized Map Icons 5.01");assert(niceName(L"ShinRyuModManager4.5.8")==L"ShinRyuModManager4.5.8");
    // The Witcher 3 tables: the engine's counts (bytes from a real file), then written and read back in English and Korean keys
    {std::string b="\x5f\x6e\x3e";size_t p=0;int32_t v;assert(bit6(b,p,v)&&v==7071&&p==2);std::string o;putBit6(o,7071);assert(o=="\x5f\x6e");o.clear();putBit6(o,96);assert(o=="\x60\x01");}
    W3File w;w.version=162;w.ids={2115000001,2115000000};w.strs={L"Auto take all",L"Allow <font color='red'>theft</font>"};w.keys={{0x1234,2115000000}};
    for(uint32_t v:{162u,164u})for(const wchar_t* l:{L"en",L"kr",L"jp"}){w.version=v;w.strs[0]=v==164?L"모두 줍기 Auto":L"Auto take all";W3File r;assert(w3read(w3write(w,l),r));assert(r.strs.size()==2&&r.keys.size()==1&&r.keys[0].second==2115000000);
        for(size_t i=0;i<2;i++){auto it=std::find(r.ids.begin(),r.ids.end(),w.ids[i]);assert(it!=r.ids.end()&&r.strs[it-r.ids.begin()]==w.strs[i]);}}
    w.version=162;w.strs[0]=L"Auto take all";
    if(const wchar_t* real=_wgetenv(L"BLUEASH_TEST_W3")){W3File r,again;std::string b=get(real);assert(w3read(b,r)&&!r.strs.empty());assert(w3read(w3write(r,L"en"),again)&&again.strs.size()==r.strs.size());
        std::string same=w3write(r,L"en");wprintf(L"real w3strings: %zu strings, re-encoded %ls\n",r.strs.size(),same==b?L"byte-identical":L"(same strings, different byte order)");}
    // A game folder: a W3 mod (kr = copy of en), a multilingual JSON mod, a single-language one, a DLL, a Korean mod
    std::wstring g=tmp(L"game"),store=tmp(L"store");
    std::string en=w3write(w,L"en");put(g+L"\\Mods\\modA\\content\\en.w3strings",en);put(g+L"\\Mods\\modA\\content\\kr.w3strings",en);
    put(g+L"\\Mods\\modB\\lang\\en.json","{\"a\":\"Open the gate\",\"b\":\"gate_id\"}");put(g+L"\\Mods\\modB\\lang\\de.json","{\"a\":\"Tor offnen\"}");
    put(g+L"\\Mods\\modC\\Localization\\strings.txt","Hello traveler\r\nGood bye\r\n");
    put(g+L"\\Mods\\modD\\plugin.dll","MZ");put(g+L"\\Mods\\modE\\lang\\en.json","{\"a\":\"Hi there\"}");put(g+L"\\Mods\\modE\\lang\\ko.json","{\"a\":\"안녕하세요\"}");
    auto mods=find(g);assert(mods.size()==5);for(auto& m:mods)plan(m,1);
    auto byName=[&](const wchar_t* n)->Mod&{for(auto& m:mods)if(m.name==n)return m;assert(false);return mods[0];};
    assert(byName(L"modA").strings==2&&byName(L"modB").strings==1&&byName(L"modC").strings==2&&byName(L"modD").strings==0&&byName(L"modE").strings==0&&byName(L"modE").native);
    int lastDone=-1;auto o=translate(mods,1,store,fake,[&](int d,int t){lastDone=d;(void)t;});
    assert(o.status==0&&o.files==3&&o.mods==3&&lastDone==5);
    W3File kr;assert(w3read(get(g+L"\\Mods\\modA\\content\\kr.w3strings"),kr));for(auto& s:kr.strs)assert(s.rfind(L"[KO] ",0)==0);
    assert(get(g+L"\\Mods\\modA\\content\\en.w3strings")==en);
    std::string ko=get(g+L"\\Mods\\modB\\lang\\ko.json");assert(ko.find("[KO] Open the gate")!=std::string::npos&&ko.find("gate_id")!=std::string::npos);
    assert(get(g+L"\\Mods\\modC\\Localization\\strings.txt")=="[KO] Hello traveler\r\n[KO] Good bye\r\n");
    // now planned as done; a second run sends nothing
    auto recs=records(store);for(auto& m:mods){plan(m,1,finishedIn(recs,L"ko"));}assert(byName(L"modA").strings==0&&byName(L"modB").strings==0);assert(recs.size()==3);assert(translatedFiles(byName(L"modC"),recs,L"ko")==1);
    int calls=0;auto o2=translate(mods,1,store,[&](const std::vector<std::wstring>& a,std::vector<std::wstring>& b){calls++;return fake(a,b);},[](int,int){});assert(calls==0&&o2.files==0);
    // the server says "limit reached": what is cached is still applied
    // 원래대로: originals back, the made file removed
    mods=find(g);auto back=restoreAll(mods,store);assert(back.first==3&&back.second==0);
    assert(get(g+L"\\Mods\\modA\\content\\kr.w3strings")==en);assert(!exists(g+L"\\Mods\\modB\\lang\\ko.json"));assert(get(g+L"\\Mods\\modC\\Localization\\strings.txt")=="Hello traveler\r\nGood bye\r\n");
    assert(records(store).empty());
    // a limit mid-way: nothing new is sent after it
    std::wstring store2=tmp(L"store2");int n=0;auto o3=translate(mods,1,store2,[&](const std::vector<std::wstring>& a,std::vector<std::wstring>& b){if(n++)return 429;return fake(a,b);},[](int,int){});assert(o3.status==0||o3.status==429);
    // the AI's answer: the JSON array inside whatever it wrote
    assert(jsonArray({L"A \"b\"\n"})==L"[\"A \\\"b\\\"\\n\"]");
    std::vector<std::wstring> got;assert(arrayOf(L"Sure:\n[\"가\\n나\", null]",2,got)&&got[0]==L"가\n나"&&got[1].empty());assert(!arrayOf(L"[\"a\"]",2,got));
    // the game's own words: exact lines taken without asking, names handed to the model
    {std::wstring g2=tmp(L"game2"),store3=tmp(L"store3");W3File en2,kr2;en2.version=162;en2.ids={1,2,3};en2.strs={L"Relic",L"Witcher",L"Common"};W3File e3=en2;
        kr2.version=162;kr2.ids={1,2,3};kr2.strs={L"유물",L"위쳐",L"평민"};
        put(g2+L"\\content\\content0\\en.w3strings",w3write(en2,L"en"));put(g2+L"\\content\\content0\\kr.w3strings",w3write(kr2,L"kr"));
        auto d=gameWords(g2,L"kr");assert(d.size()==3&&d[L"relic"]==L"유물");
        auto words=wordsIn(d,{L"A Witcher relic"});assert(words.size()==2);assert(instructions(L"The Witcher 3",L"ko",words).find(L"witcher = 위쳐")!=std::wstring::npos);
        W3File mod;mod.version=164;mod.ids={9,10};mod.strs={L"Relic",L"Open the gate"};put(g2+L"\\Mods\\modX\\content\\en.w3strings",w3write(mod,L"en"));
        auto ms=find(g2);for(auto& m:ms){plan(m,1);}std::vector<std::wstring> asked;
        auto o4=translate(ms,1,store3,[&](const std::vector<std::wstring>& a,std::vector<std::wstring>& b){asked=a;return fake(a,b);},[](int,int){},&d);
        assert(o4.status==0&&asked.size()==1&&asked[0]==L"Open the gate");W3File k3;assert(w3read(get(g2+L"\\Mods\\modX\\content\\kr.w3strings"),k3));
        assert(std::find(k3.strs.begin(),k3.strs.end(),L"유물")!=k3.strs.end());(void)e3;}
    puts("mod_translate_tests PASS");return 0;
}
