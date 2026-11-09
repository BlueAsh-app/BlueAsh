#pragma once
// 모드 번역 (게임 도우미 > 모드 번역, page 16): the mods installed for a PC game, and the English text in them put into
// BlueAsh's UI language. Only text a mod keeps in plain language files is touched:
//   - The Witcher 3 string tables (<lang>.w3strings): a mod's file for the target language is made from its English one,
//     or replaced when it is only a copy of the English (mods often ship kr.w3strings = en.w3strings). Format: RTSW,
//     XOR-obfuscated UTF-16, as in the GPL-3.0 w3strings-rust (github.com/Odashikonbu/w3strings-rust); official kr/cn/br
//     files use key 0.
//   - Language files (json, xml, key=value, yaml, plain lines) under a folder or file name that says language/strings,
//     or named for English: a multilingual mod gets a sibling file for the target language (en.json -> ko.json), a
//     single-language one is translated in place.
// Mods whose text is compiled in (DLLs, game archives, Yakuza .par) are listed but left alone. Every file BlueAsh writes
// is recorded (store\applied.tsv: file, backup or "-" when BlueAsh made it, language, hash of what was written) with the
// original kept in store\backup, so 원래대로 puts it back; a file the mod has since replaced (hash differs) is left as is.
// The text goes out with values and markup (placeholders, tags, line breaks) swapped for ⟦n⟧ and must come back with
// every one of them, else the original line stays. Pure logic (files, no window): tested by mod_translate_tests.cpp.
#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cwctype>
namespace modtr {
inline std::wstring lower(std::wstring s){for(auto& c:s)c=(wchar_t)towlower(c);return s;}
// --- languages --------------------------------------------------------------------------------------------------------
struct Lang {const wchar_t* ui;const wchar_t* w3;std::vector<const wchar_t*> tokens;};
inline const std::vector<Lang>& langs(){static const std::vector<Lang> v={
    {L"en",L"en",{L"en",L"eng",L"english",L"en_us",L"en-us",L"en_gb",L"en-gb",L"enus"}},
    {L"ko",L"kr",{L"ko",L"kr",L"kor",L"korean",L"ko_kr",L"ko-kr",L"kokr"}},
    {L"ja",L"jp",{L"ja",L"jp",L"jpn",L"japanese",L"ja_jp",L"ja-jp"}},
    {L"zh-CN",L"cn",{L"cn",L"chs",L"schinese",L"zh_cn",L"zh-cn",L"zh_hans",L"zhcn",L"chinese"}},
    {L"zh-TW",L"zh",{L"zh",L"cht",L"tchinese",L"zh_tw",L"zh-tw",L"zh_hant",L"zhtw"}},
    {L"ru",L"ru",{L"ru",L"rus",L"russian",L"ru_ru",L"ru-ru"}},
    {L"de",L"de",{L"de",L"ger",L"deu",L"german",L"de_de",L"de-de"}},
    {L"es",L"es",{L"es",L"spa",L"spanish",L"es_es",L"es-es"}},
    {L"fr",L"fr",{L"fr",L"fra",L"fre",L"french",L"fr_fr",L"fr-fr"}},
    {L"pt-BR",L"br",{L"br",L"pt",L"pt_br",L"pt-br",L"ptbr",L"brazilian",L"portuguese"}},
    {L"it",L"it",{L"it",L"ita",L"italian",L"it_it",L"it-it"}},
    {L"pl",L"pl",{L"pl",L"pol",L"polish",L"pl_pl",L"pl-pl"}},
    {L"tr",L"tr",{L"tr",L"tur",L"turkish",L"tr_tr",L"tr-tr"}}};return v;}
// Languages BlueAsh has no UI for: a file named for one is "another language", not English.
inline bool otherLanguage(const std::wstring& t){static const std::set<std::wstring> o={L"ar",L"arabic",L"cz",L"cs",L"czech",L"hu",L"hungarian",L"esmx",L"es_mx",L"es-mx",L"uk",L"ukrainian",L"nl",L"dutch",L"sv",L"swedish",L"th",L"thai",L"vi",L"vietnamese",L"id",L"indonesian",L"no",L"norwegian",L"da",L"danish",L"fi",L"finnish",L"el",L"greek",L"ro",L"romanian"};return o.count(t)>0;}
inline int uiIndex(const std::wstring& ui){auto& v=langs();for(size_t i=0;i<v.size();i++)if(ui==v[i].ui){return (int)i;}return -1;}
// The language a token names: index into langs(), -2 another language, -1 none.
inline int tokenLang(std::wstring t){t=lower(t);auto& v=langs();for(size_t i=0;i<v.size();i++)for(auto* k:v[i].tokens)if(t==k){return (int)i;}return otherLanguage(t)?-2:-1;}
// A file's language from its name ("en.json", "strings.en.json", "ModName_ENGLISH.txt", "lang/en_us.lang") or its folder ("en/strings.xml").
inline int fileLang(const std::wstring& path,std::wstring* token=nullptr){
    size_t slash=path.find_last_of(L"\\/");std::wstring name=path.substr(slash==std::wstring::npos?0:slash+1);size_t dot=name.find_last_of(L'.');std::wstring stem=dot==std::wstring::npos?name:name.substr(0,dot);
    std::vector<std::wstring> c={stem};size_t p=0;for(size_t q;(q=stem.find(L'.',p))!=std::wstring::npos;p=q+1)c.push_back(stem.substr(p,q-p));if(p)c.push_back(stem.substr(p));
    size_t u=stem.find_last_of(L"_-");if(u!=std::wstring::npos&&u+1<stem.size()){c.push_back(stem.substr(u+1));size_t u2=stem.find_last_of(L"_-",u-1);if(u>0&&u2!=std::wstring::npos)c.push_back(stem.substr(u2+1));}
    if(slash!=std::wstring::npos){size_t ps=path.find_last_of(L"\\/",slash-1);c.push_back(path.substr(ps==std::wstring::npos?0:ps+1,slash-(ps==std::wstring::npos?0:ps+1)));}
    for(auto& t:c){int l=tokenLang(t);if(l!=-1){if(token)*token=t;return l;}}return -1;}
// The sibling name for the target language in the same style as the English one ("en"->"ko", "ENGLISH"->"KOREAN", "en_US"->"ko_KR").
inline std::wstring siblingName(const std::wstring& path,const std::wstring& token,int target){
    size_t slash=path.find_last_of(L"\\/");std::wstring dir=path.substr(0,slash+1),name=path.substr(slash+1);
    std::wstring lt=lower(token),rep;auto& T=langs()[target];
    std::wstring two=T.tokens[0],full;for(auto* k:T.tokens){std::wstring s=k;if(s.size()>4&&s.find(L'_')==std::wstring::npos&&s.find(L'-')==std::wstring::npos){full=s;break;}}
    std::wstring region;for(auto* k:T.tokens){std::wstring s=k;if(s.size()==5&&s[2]==L'_'){region=s;break;}}
    if(lt.size()>4&&lt.find(L'_')==std::wstring::npos&&lt.find(L'-')==std::wstring::npos)rep=full.empty()?two:full;
    else if(lt.size()==5&&(lt[2]==L'_'||lt[2]==L'-')){rep=region.empty()?two:region;rep[2]=lt[2];}
    else rep=two;
    bool upper=true,cap=iswupper(token[0])!=0;for(wchar_t ch:token)if(iswalpha(ch)&&!iswupper(ch))upper=false;
    if(upper)for(auto& ch:rep)ch=(wchar_t)towupper(ch);else if(cap)rep[0]=(wchar_t)towupper(rep[0]);
    if(lt.size()==5&&!upper&&iswupper(token[3]))for(size_t i=3;i<rep.size();i++)rep[i]=(wchar_t)towupper(rep[i]);
    size_t at=name.rfind(token);if(at!=std::wstring::npos){return dir+name.substr(0,at)+rep+name.substr(at+token.size());}
    // the token was the folder's name ("en/strings.xml")
    std::wstring parent=dir.substr(0,dir.size()-1);size_t ps=parent.find_last_of(L"\\/");return parent.substr(0,ps+1)+rep+L"\\"+name;}
// --- text: what to translate and what to keep -------------------------------------------------------------------------
inline bool foreignScript(wchar_t c){return (c>=0x0400&&c<=0x04FF)||(c>=0x3040&&c<=0x30FF)||(c>=0x3400&&c<=0x9FFF)||(c>=0xAC00&&c<=0xD7A3)||(c>=0x1100&&c<=0x11FF)||(c>=0x0600&&c<=0x06FF)||(c>=0x0E00&&c<=0x0E7F);}
// English text a player reads, not an identifier, number, path, key or colour.
inline bool translatable(const std::wstring& s){
    int letters=0,run=0,best=0;bool space=false;for(wchar_t c:s){if(foreignScript(c)){return false;}if(c<128&&iswalpha(c)){letters++;run++;best=std::max(best,run);}else run=0;if(c==L' ')space=true;}
    if(letters<2||best<2){return false;}
    std::wstring l=lower(s);if(l.find(L"http://")!=std::wstring::npos||l.find(L"https://")!=std::wstring::npos||l.find(L"www.")==0){return false;}
    if(!space){ // one word: only a Capitalised or ALL-CAPS label ("Health", "Enabled", "OK"); never snake_case, dotted, camelCase, paths
        if(s.find_first_of(L"_./\\:#@=$%{}<>[]|")!=std::wstring::npos){return false;}
        bool capital=iswupper(s[0])!=0,rest=true,allCaps=true;for(size_t i=1;i<s.size();i++){if(iswupper(s[i]))rest=false;if(iswlower(s[i]))allCaps=false;if(iswdigit(s[i])){return false;}}
        if(!capital){return false;}if(!rest&&!allCaps){return false;}if(allCaps&&s.size()>12){return false;}}
    else if(s.find(L' ')==std::wstring::npos){return false;}
    return true;}
// Values and markup the translation must keep, swapped for ⟦n⟧: {0} {name}, %s %d %1$s, <tags>, [b] [/color], $VAR$,
// line breaks (real or "\n" written out), and leading / trailing spaces (kept outside the text sent).
struct Protected {std::wstring text,lead,trail;std::vector<std::wstring> slots;};
inline Protected protect(const std::wstring& s){
    Protected p;size_t a=0,b=s.size();while(a<b&&(s[a]==L' '||s[a]==L'\t'))a++;while(b>a&&(s[b-1]==L' '||s[b-1]==L'\t'))b--;p.lead=s.substr(0,a);p.trail=s.substr(b);
    auto put=[&](const std::wstring& t){p.text+=L"\x27e6"+std::to_wstring(p.slots.size())+L"\x27e7";p.slots.push_back(t);};
    for(size_t i=a;i<b;){wchar_t c=s[i];size_t e=std::wstring::npos;
        if(c==L'{'){size_t q=s.find(L'}',i);if(q!=std::wstring::npos&&q<b&&q-i<=40)e=q+1;}
        else if(c==L'<'){size_t q=s.find(L'>',i);if(q!=std::wstring::npos&&q<b&&q-i<=120)e=q+1;}
        else if(c==L'['){size_t q=s.find(L']',i);if(q!=std::wstring::npos&&q<b&&q-i<=24&&s.substr(i,q-i).find(L' ')==std::wstring::npos)e=q+1;}
        else if(c==L'$'){size_t q=s.find(L'$',i+1);if(q!=std::wstring::npos&&q<b&&q-i<=40&&s.substr(i,q-i).find(L' ')==std::wstring::npos)e=q+1;}
        else if(c==L'%'){size_t q=i+1;while(q<b&&(iswdigit(s[q])||s[q]==L'$'||s[q]==L'.'||s[q]==L'-'||s[q]==L'+'||s[q]==L'#'||s[q]==L'l'))q++;if(q<b&&wcschr(L"sdifuxXeEgGcpS%@",s[q]))e=q+1;}
        else if(c==L'\\'&&i+1<b&&wcschr(L"nrt",s[i+1]))e=i+2;
        else if(c==L'\n'||c==L'\r'||c==L'\t'){e=i+1;while(e<b&&(s[e]==L'\n'||s[e]==L'\r'))e++;}
        else if(c==L'&'){size_t q=s.find(L';',i);if(q!=std::wstring::npos&&q<b&&q-i<=8&&s.substr(i,q-i).find(L' ')==std::wstring::npos)e=q+1;}
        if(e!=std::wstring::npos&&e>i){put(s.substr(i,e-i));i=e;}else{p.text+=c;i++;}}
    return p;}
// The translation with every ⟦n⟧ back in its place; empty when one is missing or doubled (the original line stays then).
inline std::wstring restore(const Protected& p,const std::wstring& t){
    if(t.empty()){return L"";}std::wstring out;std::vector<int> seen(p.slots.size(),0);
    for(size_t i=0;i<t.size();){if(t[i]==0x27e6){size_t q=t.find(0x27e7,i);if(q==std::wstring::npos){return L"";}std::wstring n=t.substr(i+1,q-i-1);
            if(n.empty()||n.size()>4||n.find_first_not_of(L"0123456789")!=std::wstring::npos){return L"";}size_t k=std::stoul(n);if(k>=p.slots.size()||seen[k]++){return L"";}out+=p.slots[k];i=q+1;}
        else if(t[i]==0x27e7){return L"";}else out+=t[i++];}
    for(int v:seen){if(v!=1){return L"";}}return p.lead+out+p.trail;}
// --- files -------------------------------------------------------------------------------------------------------------
inline bool readBytes(const std::wstring& path,std::string& out,size_t cap=4u<<20){HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE){return false;}
    LARGE_INTEGER sz={};bool ok=GetFileSizeEx(f,&sz)&&sz.QuadPart>0&&(size_t)sz.QuadPart<=cap;if(ok){out.resize((size_t)sz.QuadPart);DWORD n=0;ok=ReadFile(f,&out[0],(DWORD)out.size(),&n,nullptr)&&n==out.size();}CloseHandle(f);return ok;}
// Written over the existing file (a mod manager's symbolic link keeps pointing at its own copy, which changes).
inline bool writeBytes(const std::wstring& path,const std::string& data){HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE){return false;}
    DWORD n=0;bool ok=WriteFile(f,data.data(),(DWORD)data.size(),&n,nullptr)&&n==data.size();ok=FlushFileBuffers(f)&&ok;CloseHandle(f);return ok;}
inline uint64_t fnv(const std::string& s){uint64_t h=1469598103934665603ull;for(unsigned char c:s){h^=c;h*=1099511628211ull;}return h;}
inline std::wstring hex(uint64_t v){wchar_t b[17];swprintf(b,17,L"%016llx",(unsigned long long)v);return b;}
inline std::string utf8(const std::wstring& w){if(w.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,w.data(),(int)w.size(),&s[0],n,nullptr,nullptr);return s;}
inline bool fromUtf8(const std::string& s,std::wstring& w){if(s.empty()){w.clear();return true;}int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),(int)s.size(),nullptr,0);if(n<=0){return false;}w.assign(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n);return true;}
// Text in its own encoding: 0 UTF-8, 1 UTF-8 with BOM, 2 UTF-16 LE with BOM. Anything else (old code pages) is not read.
inline bool decodeText(const std::string& b,std::wstring& t,int& enc){
    if(b.size()>=2&&(unsigned char)b[0]==0xFF&&(unsigned char)b[1]==0xFE){enc=2;t.assign((const wchar_t*)(b.data()+2),(b.size()-2)/2);return true;}
    if(b.size()>=3&&(unsigned char)b[0]==0xEF&&(unsigned char)b[1]==0xBB&&(unsigned char)b[2]==0xBF){enc=1;return fromUtf8(b.substr(3),t);}
    enc=0;return fromUtf8(b,t);}
inline std::string encodeText(const std::wstring& t,int enc){if(enc==2){std::string s("\xFF\xFE",2);s.append((const char*)t.data(),t.size()*2);return s;}return (enc==1?std::string("\xEF\xBB\xBF"):std::string())+utf8(t);}
// --- formats ------------------------------------------------------------------------------------------------------------
enum Fmt {Json,Xml,Kv,Yaml,Lines,W3,None};
struct Seg {size_t a,b;std::wstring s;}; // raw range [a,b) in the text and its decoded string
inline Fmt formatOf(const std::wstring& path){std::wstring l=lower(path);size_t d=l.find_last_of(L'.');std::wstring e=d==std::wstring::npos?L"":l.substr(d+1);
    if(e==L"w3strings"){return W3;}if(e==L"json"){return Json;}if(e==L"xml"){return Xml;}if(e==L"yml"||e==L"yaml"){return Yaml;}
    if(e==L"lang"||e==L"properties"||e==L"ini"||e==L"cfg"||e==L"strings"||e==L"loc"){return Kv;}if(e==L"txt"){return Lines;}return None;}
inline std::wstring jsonUnescape(const std::wstring& r){std::wstring o;for(size_t i=0;i<r.size();i++){if(r[i]!=L'\\'||i+1>=r.size()){o+=r[i];continue;}wchar_t c=r[++i];
        if(c==L'n')o+=L'\n';else if(c==L't')o+=L'\t';else if(c==L'r')o+=L'\r';else if(c==L'b')o+=L'\b';else if(c==L'f')o+=L'\f';
        else if(c==L'u'&&i+4<r.size()){o+=(wchar_t)wcstoul(r.substr(i+1,4).c_str(),nullptr,16);i+=4;}else o+=c;}return o;}
inline std::wstring jsonEscape(const std::wstring& s){std::wstring o;for(wchar_t c:s){if(c==L'"')o+=L"\\\"";else if(c==L'\\')o+=L"\\\\";else if(c==L'\n')o+=L"\\n";else if(c==L'\r')o+=L"\\r";else if(c==L'\t')o+=L"\\t";
        else if(c<0x20){wchar_t b[8];swprintf(b,8,L"\\u%04x",c);o+=b;}else o+=c;}return o;}
inline std::wstring xmlUnescape(const std::wstring& r){std::wstring o;for(size_t i=0;i<r.size();i++){if(r[i]==L'&'){size_t q=r.find(L';',i);if(q!=std::wstring::npos&&q-i<=8){std::wstring e=r.substr(i+1,q-i-1);
                wchar_t c=e==L"amp"?L'&':e==L"lt"?L'<':e==L"gt"?L'>':e==L"quot"?L'"':e==L"apos"?L'\'':0;if(!c&&!e.empty()&&e[0]==L'#')c=(wchar_t)(e.size()>1&&(e[1]==L'x'||e[1]==L'X')?wcstoul(e.c_str()+2,nullptr,16):wcstoul(e.c_str()+1,nullptr,10));
                if(c){o+=c;i=q;continue;}}}o+=r[i];}return o;}
inline std::wstring xmlEscape(const std::wstring& s){std::wstring o;for(wchar_t c:s){if(c==L'&')o+=L"&amp;";else if(c==L'<')o+=L"&lt;";else if(c==L'>')o+=L"&gt;";else o+=c;}return o;}
inline std::wstring encodeFor(Fmt f,const std::wstring& s){return f==Json?jsonEscape(s):f==Xml?xmlEscape(s):s;}
inline std::vector<Seg> segments(Fmt f,const std::wstring& t){std::vector<Seg> v;
    if(f==Json){for(size_t i=0;i<t.size();i++){wchar_t c=t[i];
            if(c==L'/'&&i+1<t.size()&&t[i+1]==L'/'){size_t q=t.find(L'\n',i);if(q==std::wstring::npos){break;}i=q;continue;}
            if(c==L'/'&&i+1<t.size()&&t[i+1]==L'*'){size_t q=t.find(L"*/",i+2);if(q==std::wstring::npos){break;}i=q+1;continue;}
            if(c!=L'"'){continue;}size_t j=i+1;while(j<t.size()&&t[j]!=L'"'){if(t[j]==L'\\')j++;j++;}if(j>=t.size()){break;}
            size_t k=j+1;while(k<t.size()&&iswspace(t[k]))k++;bool key=k<t.size()&&t[k]==L':';if(!key)v.push_back({i+1,j,jsonUnescape(t.substr(i+1,j-i-1))});i=j;}}
    else if(f==Xml){size_t i=0;while(i<t.size()){if(t.compare(i,4,L"<!--")==0){size_t q=t.find(L"-->",i);if(q==std::wstring::npos){break;}i=q+3;continue;}
            if(t.compare(i,9,L"<![CDATA[")==0){size_t q=t.find(L"]]>",i);if(q==std::wstring::npos){break;}i=q+3;continue;}
            if(t[i]==L'<'){size_t q=t.find(L'>',i);if(q==std::wstring::npos){break;}i=q+1;continue;}
            size_t q=t.find(L'<',i);if(q==std::wstring::npos)q=t.size();size_t a=i,b=q;while(a<b&&iswspace(t[a]))a++;while(b>a&&iswspace(t[b-1]))b--;if(b>a)v.push_back({a,b,xmlUnescape(t.substr(a,b-a))});i=q;}}
    else{ // line formats: key=value (Kv, and Lines when most lines have one), key: value (Yaml), else whole lines
        size_t lines=0,eqs=0;for(size_t p=0;p<t.size();){size_t q=t.find(L'\n',p);if(q==std::wstring::npos)q=t.size();std::wstring l=t.substr(p,q-p);if(l.find_first_not_of(L" \t\r")!=std::wstring::npos){lines++;if(l.find(L'=')!=std::wstring::npos)eqs++;}p=q+1;}
        bool kv=f==Kv||(f==Lines&&lines&&eqs*2>=lines);
        for(size_t p=0;p<t.size();){size_t q=t.find(L'\n',p);if(q==std::wstring::npos)q=t.size();size_t e=q;if(e>p&&t[e-1]==L'\r')e--;
            size_t a=p;while(a<e&&(t[a]==L' '||t[a]==L'\t'))a++;
            if(a<e&&!wcschr(L"#;[",t[a])&&!(t[a]==L'/'&&a+1<e&&t[a+1]==L'/')){size_t s=a;
                if(f==Yaml){size_t c=t.find(L": ",a);if(c==std::wstring::npos||c>=e||t[a]==L'-')s=std::wstring::npos;else s=c+2;}
                else if(kv){size_t c=t.find(L'=',a);s=c==std::wstring::npos||c>=e?std::wstring::npos:c+1;}
                if(s!=std::wstring::npos){while(s<e&&(t[s]==L' '||t[s]==L'\t'))s++;size_t b=e;while(b>s&&(t[b-1]==L' '||t[b-1]==L'\t'||t[b-1]==L';'))b--;
                    if(b>s+1&&(t[s]==L'"'||t[s]==L'\'')&&t[b-1]==t[s]){s++;b--;}
                    if(f==Yaml&&s<b&&wcschr(L"|>&*[{",t[s])){s=b;}if(b>s)v.push_back({s,b,t.substr(s,b-s)});}}
            p=q+1;}}
    return v;}
// --- The Witcher 3 string tables ---------------------------------------------------------------------------------------
struct W3Key {const wchar_t* lang;uint32_t key,magic;};
inline const std::vector<W3Key>& w3keys(){static const std::vector<W3Key> v={{L"pl",0x83496237,0x73946816},{L"en",0x43975139,0x79321793},{L"de",0x75886138,0x42791159},{L"it",0x45931894,0x12375973},
    {L"fr",0x23863176,0x75921975},{L"cz",0x24987354,0x21793217},{L"es",0x18796651,0x42387566},{L"zh",0x18632176,0x16875467},{L"ru",0x63481486,0x42386347},{L"hu",0x42378932,0x67823218},{L"jp",0x54834893,0x59825646}};return v;}
inline uint32_t w3magic(uint32_t key,bool& known){known=true;if(key==0){return 0;}for(auto& k:w3keys())if(k.key==key){return k.magic;}known=false;return 0;}
inline uint32_t w3keyFor(const std::wstring& lang){for(auto& k:w3keys())if(lang==k.lang){return k.key;}return 0;} // kr, cn, br, tr, ar, esMX: key 0 as in the game's own files
struct W3File {uint32_t version=0;std::vector<uint32_t> ids;std::vector<std::wstring> strs;std::vector<std::pair<uint32_t,uint32_t>> keys;};
// Counts are the engine's compressed integers: first byte 6 bits of value + 0x40 "more", then 7 bits + 0x80 "more"
// (read from the game's own files: 0x5f 0x6e = 7071).
inline bool bit6(const std::string& b,size_t& p,int32_t& out){if(p>=b.size()){return false;}unsigned char c=(unsigned char)b[p++];uint32_t v=c&0x3f;int shift=6;
    if(c&0x40){do{if(p>=b.size()||shift>27){return false;}c=(unsigned char)b[p++];v|=(uint32_t)(c&0x7f)<<shift;shift+=7;}while(c&0x80);}
    out=(int32_t)v;return true;}
inline void putBit6(std::string& o,int32_t c){uint32_t v=(uint32_t)c;o+=(char)((v&0x3f)|(v>0x3f?0x40:0));v>>=6;while(v){unsigned char x=v&0x7f;v>>=7;if(v){x|=0x80;}o+=(char)x;}}
inline uint32_t u32(const std::string& b,size_t p){return (uint32_t)(unsigned char)b[p]|(uint32_t)(unsigned char)b[p+1]<<8|(uint32_t)(unsigned char)b[p+2]<<16|(uint32_t)(unsigned char)b[p+3]<<24;}
inline void put32(std::string& o,uint32_t v){for(int i=0;i<4;i++)o+=(char)(v>>(8*i));}
// Version 162 keeps UTF-16 text (2 bytes a unit, key bytes low and high); the next-gen game's 164 keeps UTF-8 bytes,
// each XORed with the key's low byte (offsets, lengths and the text size count bytes then). Read from both kinds of real files.
inline bool w3read(const std::string& b,W3File& f){
    if(b.size()<14||b.compare(0,4,"RTSW")){return false;}f.version=u32(b,4);bool narrow=f.version>=164;uint32_t key=((uint32_t)(unsigned char)b[8]|(uint32_t)(unsigned char)b[9]<<8)<<16|((uint32_t)(unsigned char)b[b.size()-2]|(uint32_t)(unsigned char)b[b.size()-1]<<8);
    bool known;uint32_t magic=w3magic(key,known);if(!known){return false;}size_t p=10;int32_t n1,n2,n3;if(!bit6(b,p,n1)||n1<0||(size_t)n1>b.size()/12){return false;}
    struct E{uint32_t id,off,len;};std::vector<E> e;for(int i=0;i<n1;i++){if(p+12>b.size()){return false;}e.push_back({u32(b,p)^magic,u32(b,p+4),u32(b,p+8)});p+=12;}
    if(!bit6(b,p,n2)||n2<0||(size_t)n2>b.size()/8){return false;}for(int i=0;i<n2;i++){if(p+8>b.size()){return false;}f.keys.push_back({u32(b,p),u32(b,p+4)^magic});p+=8;}
    if(!bit6(b,p,n3)){return false;}size_t start=p,unit=narrow?1:2;uint16_t init=(uint16_t)((magic>>8)&0xffff);
    for(auto& x:e){size_t o=start+(size_t)x.off*unit;if(o+(size_t)x.len*unit>b.size()-2){return false;}uint16_t k=init;std::wstring s;std::string bytes;
        for(uint32_t i=0;i<x.len;i++){uint16_t ck=(uint16_t)(((x.len+1)*(uint32_t)k)&0xffff);
            if(narrow){bytes+=(char)((unsigned char)b[o+i]^(ck&0xff));}else{s+=(wchar_t)(uint16_t)(((unsigned char)b[o+2*i]^(ck&0xff))|(((unsigned char)b[o+2*i+1]^(ck>>8))<<8));}
            k=(uint16_t)((k<<1)|(k>>15));}
        if(narrow&&!fromUtf8(bytes,s)){return false;}f.ids.push_back(x.id);f.strs.push_back(s);}
    return true;}
inline std::string w3write(const W3File& f,const std::wstring& lang){
    uint32_t key=w3keyFor(lang);bool known;uint32_t magic=w3magic(key,known);uint32_t version=f.version?f.version:162;bool narrow=version>=164;size_t unit=narrow?1:2;
    std::vector<size_t> order(f.ids.size());for(size_t i=0;i<order.size();i++){order[i]=i;}
    std::sort(order.begin(),order.end(),[&](size_t a,size_t b){return (f.ids[a]^magic)<(f.ids[b]^magic);});auto keys=f.keys;std::sort(keys.begin(),keys.end());
    std::string strs;std::vector<uint32_t> off(f.ids.size()),len(f.ids.size());uint16_t init=(uint16_t)((magic>>8)&0xffff);
    for(size_t i:order){off[i]=(uint32_t)(strs.size()/unit);uint16_t k=init;
        if(narrow){std::string u=utf8(f.strs[i]);len[i]=(uint32_t)u.size();for(char ch:u){uint16_t ck=(uint16_t)(((len[i]+1)*(uint32_t)k)&0xffff);strs+=(char)((unsigned char)ch^(ck&0xff));k=(uint16_t)((k<<1)|(k>>15));}strs+='\0';}
        else{len[i]=(uint32_t)f.strs[i].size();for(wchar_t ch:f.strs[i]){uint16_t ck=(uint16_t)(((len[i]+1)*(uint32_t)k)&0xffff);strs+=(char)(((uint16_t)ch&0xff)^(ck&0xff));strs+=(char)((((uint16_t)ch)>>8)^(ck>>8));k=(uint16_t)((k<<1)|(k>>15));}strs+=std::string(2,'\0');}}
    std::string o="RTSW";put32(o,version);o+=(char)((key>>16)&0xff);o+=(char)((key>>24)&0xff);
    putBit6(o,(int32_t)order.size());for(size_t i:order){put32(o,f.ids[i]^magic);put32(o,off[i]);put32(o,len[i]);}
    putBit6(o,(int32_t)keys.size());for(auto& k:keys){put32(o,k.first);put32(o,k.second^magic);}
    putBit6(o,(int32_t)(strs.size()/unit));o+=strs;o+=(char)(key&0xff);o+=(char)((key>>8)&0xff);return o;}
inline bool mostlyEnglish(const std::vector<std::wstring>& v){int en=0,other=0;for(auto& s:v){bool f=false;for(wchar_t c:s)if(foreignScript(c)){f=true;break;}if(f)other++;else if(translatable(s))en++;}return en>0&&en>=other*4;}
// --- mods ----------------------------------------------------------------------------------------------------------------
struct Job {std::wstring src,dst;Fmt fmt;bool create;}; // create: dst is a new file (no original to keep)
struct Mod {std::wstring name,root;std::vector<std::wstring> files;std::vector<Job> jobs;int strings=0,done=0;bool native=false;};
inline bool isDir(const std::wstring& p){DWORD a=GetFileAttributesW(p.c_str());return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY);}
inline bool exists(const std::wstring& p){return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES;}
inline void listFiles(const std::wstring& dir,std::vector<std::wstring>& out,int depth=6,size_t cap=4000){if(depth<0||out.size()>=cap)return;WIN32_FIND_DATAW d;HANDLE h=FindFirstFileW((dir+L"\\*").c_str(),&d);if(h==INVALID_HANDLE_VALUE)return;
    do{std::wstring n=d.cFileName;if(n==L"."||n==L".."){continue;}std::wstring p=dir+L"\\"+n;if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(!(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))listFiles(p,out,depth-1,cap);}else out.push_back(p);}while(out.size()<cap&&FindNextFileW(h,&d));FindClose(h);}
inline std::wstring jsonField(const std::wstring& t,size_t from,const wchar_t* key,size_t* at=nullptr){size_t k=t.find(std::wstring(L"\"")+key+L"\"",from);if(k==std::wstring::npos){return L"";}size_t c=t.find(L':',k);size_t q=c==std::wstring::npos?c:t.find(L'"',c);if(q==std::wstring::npos){return L"";}
    size_t e=q+1;while(e<t.size()&&t[e]!=L'"'){if(t[e]==L'\\')e++;e++;}if(at)*at=e;return jsonUnescape(t.substr(q+1,e-q-1));}
// Folders a game keeps its mods in, found within four levels of the install folder.
inline bool modFolder(const std::wstring& name,const std::wstring& parent){std::wstring n=lower(name),p=lower(parent);return n==L"mods"||n==L"mod"||n==L"lml"||n==L"~mods"||(n==L"plugins"&&p==L"bepinex");}
inline void findFolders(const std::wstring& dir,int depth,std::vector<std::wstring>& out,std::vector<std::wstring>& manifests,int& budget){
    if(depth<0||--budget<0){return;}WIN32_FIND_DATAW d;HANDLE h=FindFirstFileW((dir+L"\\*").c_str(),&d);if(h==INVALID_HANDLE_VALUE){return;}std::wstring parent=dir.substr(dir.find_last_of(L'\\')+1);
    do{std::wstring n=d.cFileName;if(n==L"."||n==L".."){continue;}std::wstring p=dir+L"\\"+n;
        if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){if(lower(n).rfind(L"vortex.deployment",0)==0&&lower(n).size()>5&&lower(n).substr(lower(n).size()-5)==L".json")manifests.push_back(p);continue;}
        if(d.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT){continue;}if(modFolder(n,parent))out.push_back(p);else findFolders(p,depth-1,out,manifests,budget);}while(FindNextFileW(h,&d));FindClose(h);}
inline bool skippedLoose(const std::wstring& n){std::wstring l=lower(n);for(const wchar_t* e:{L".txt",L".ini",L".cfg",L".log",L".md",L".json"})if(l.size()>wcslen(e)&&l.substr(l.size()-wcslen(e))==e){return l.find(L"vortex")!=std::wstring::npos||std::wstring(e)!=L".json";}return false;}
// A mod manager's download name without its Nexus id, version and date: "Igni Blade 14066 3 2026-10-12T22-27Z 4acEvLd3a"
// -> "Igni Blade", "More Robes-2144-1-10a-1554850485" -> "More Robes".
inline std::wstring niceName(const std::wstring& s){std::wstring n=s;
    for(size_t i=0;i+1<n.size();i++){if(n[i]==L'-'&&iswdigit(n[i+1])&&i>0){n=n.substr(0,i);break;}}
    size_t p=0;while((p=n.find(L' ',p))!=std::wstring::npos){size_t q=n.find(L' ',p+1);std::wstring w=n.substr(p+1,q==std::wstring::npos?std::wstring::npos:q-p-1);
        if(w.size()>=3&&w.find_first_not_of(L"0123456789")==std::wstring::npos){n=n.substr(0,p);break;}p++;}
    while(!n.empty()&&(n.back()==L' '||n.back()==L'-'||n.back()==L'_')){n.pop_back();}return n.empty()?s:n;}
inline std::vector<Mod> find(const std::wstring& game){
    std::vector<std::wstring> folders,manifests;int budget=3000;findFolders(game,4,folders,manifests,budget);std::vector<Mod> mods;
    for(auto& f:folders){WIN32_FIND_DATAW d;HANDLE h=FindFirstFileW((f+L"\\*").c_str(),&d);if(h==INVALID_HANDLE_VALUE){continue;}
        do{std::wstring n=d.cFileName;if(n==L"."||n==L".."){continue;}std::wstring p=f+L"\\"+n;Mod m;m.name=n;m.root=p;
            if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){if(n.size()>1&&n[0]==L'.'){continue;}listFiles(p,m.files);}
            else{if(lower(n).rfind(L"vortex.deployment",0)==0){manifests.push_back(p);continue;}if(skippedLoose(n)){continue;}size_t dot=n.find_last_of(L'.');m.name=dot==std::wstring::npos?n:n.substr(0,dot);m.files.push_back(p);}
            mods.push_back(m);}while(FindNextFileW(h,&d));FindClose(h);}
    // A mod manager's records (Vortex): its name for a mod folder above ("Evils of Rivia" for modEvilsOfRivia), and the
    // same mod's files elsewhere (a Witcher 3 mod's dlc part) joined to it; files of mods it placed outside such folders
    // make mods of their own.
    auto covering=[&](const std::wstring& p){std::wstring l=lower(p);for(size_t i=0;i<mods.size();i++){std::wstring r=lower(mods[i].root);if(l==r||l.rfind(r+L"\\",0)==0){return (int)i;}}return -1;};
    std::vector<std::pair<std::wstring,std::wstring>> placed; // file, Vortex mod
    for(auto& mf:manifests){std::string b;std::wstring t;int enc;if(!readBytes(mf,b,16u<<20)||!decodeText(b,t,enc)){continue;}std::wstring target=jsonField(t,0,L"targetPath");std::wstring base=!target.empty()&&isDir(target)?target:mf.substr(0,mf.find_last_of(L'\\'));
        for(size_t at=t.find(L"\"files\"");at!=std::wstring::npos;){size_t e=0;std::wstring rel=jsonField(t,at,L"relPath",&e);if(rel.empty()){break;}std::wstring src=jsonField(t,e,L"source",&at);if(src.empty()){break;}placed.push_back({base+L"\\"+rel,src});}}
    std::map<std::wstring,int> alias;std::set<int> named;
    for(auto& [p,src]:placed){int i=covering(p);if(i>=0&&!alias.count(src)){alias[src]=i;if(named.insert(i).second){mods[i].name=niceName(src);}}}
    std::map<std::wstring,Mod> extra;
    for(auto& [p,src]:placed){if(covering(p)>=0){continue;}auto a=alias.find(src);if(a!=alias.end()){mods[a->second].files.push_back(p);continue;}
        auto& m=extra[src];m.name=niceName(src);m.files.push_back(p);} // root stays empty: its files are all it has
    for(auto& [k,m]:extra)mods.push_back(m);
    std::sort(mods.begin(),mods.end(),[](const Mod& a,const Mod& b){return _wcsicmp(a.name.c_str(),b.name.c_str())<0;});return mods;}
// Mod-relative path hints that a file holds text a player reads.
// (whole folder or file names: "lang", "Localization", "translations", "strings"; not "texture" or "Slang")
inline bool languagePath(const std::wstring& rel){std::wstring l=lower(rel);size_t p=0;
    for(;;){size_t q=l.find_first_of(L"\\/",p);std::wstring c=l.substr(p,q==std::wstring::npos?std::wstring::npos:q-p);if(q==std::wstring::npos){size_t d=c.find_last_of(L'.');if(d!=std::wstring::npos)c=c.substr(0,d);}
        for(const wchar_t* k:{L"lang",L"langs",L"text",L"texts",L"strings",L"messages",L"i18n",L"l10n",L"loc"})if(c==k){return true;}
        for(const wchar_t* k:{L"language",L"locale",L"locali",L"translation",L"localis"})if(c.rfind(k,0)==0){return true;}
        if(q==std::wstring::npos){return false;}p=q+1;}}
inline bool readDoc(const std::wstring& path,Fmt f,std::wstring& text,int& enc,W3File& w3){std::string b;if(!readBytes(path,b)){return false;}if(f==W3){return w3read(b,w3);}return decodeText(b,text,enc);}
inline std::vector<std::wstring> stringsOf(const std::wstring& path,Fmt f){std::wstring t;int enc;W3File w;std::vector<std::wstring> v;if(!readDoc(path,f,t,enc,w)){return v;}if(f==W3){return w.strs;}for(auto& s:segments(f,t))v.push_back(s.s);return v;}
// What would be written for a mod into the target language (target = index into langs()), and how many lines are left.
// finished(dst): a file BlueAsh already translated completely into this language (left out).
inline void plan(Mod& m,int target,const std::function<bool(const std::wstring&)>& finished={}){m.jobs.clear();m.strings=0;m.native=false;if(target<=0)return;std::set<std::wstring> seen;
    std::map<std::wstring,std::vector<std::wstring>> byDir;for(auto& f:m.files)byDir[lower(f.substr(0,f.find_last_of(L'\\')))].push_back(f);
    const std::wstring tw3=langs()[target].w3;
    for(auto& [dir,files]:byDir){
        // The Witcher 3: <lang>.w3strings beside each other.
        std::wstring en,dst;for(auto& f:files){if(formatOf(f)!=W3){continue;}std::wstring stem=lower(f.substr(f.find_last_of(L'\\')+1));stem=stem.substr(0,stem.size()-10);
            if(stem==L"en"){en=f;}if(stem==lower(tw3)){dst=f;}}
        if(!en.empty()){std::wstring out=dst.empty()?en.substr(0,en.find_last_of(L'\\')+1)+tw3+L".w3strings":dst;bool create=dst.empty();
            if(!create){auto have=stringsOf(dst,W3);if(!mostlyEnglish(have)){if(!have.empty())m.native=true;continue;}}
            m.jobs.push_back({en,out,W3,create});continue;}
        int langsHere=0;for(auto& f:files)if(fileLang(f)>=0||fileLang(f)==-2)langsHere++;
        for(auto& f:files){Fmt fmt=formatOf(f);if(fmt==None||fmt==W3){continue;}size_t s1=f.find_last_of(L'\\'),s2=s1==std::wstring::npos||!s1?std::wstring::npos:f.find_last_of(L'\\',s1-1);
            std::wstring rel=m.root.empty()||lower(f).rfind(lower(m.root)+L"\\",0)!=0?f.substr(s2==std::wstring::npos?0:s2+1):f.substr(std::min(m.root.size(),f.size()));std::wstring token;int l=fileLang(f,&token);
            if(l==target){m.native=true;}if(l!=0&&!(l==-1&&languagePath(rel))){continue;}
            if(l==0&&langsHere>1){std::wstring out=siblingName(f,token,target);if(seen.count(lower(out))){continue;}seen.insert(lower(out));
                if(exists(out)){if(!mostlyEnglish(stringsOf(out,formatOf(out)))){m.native=true;continue;}m.jobs.push_back({f,out,fmt,false});}else m.jobs.push_back({f,out,fmt,true});}
            else m.jobs.push_back({f,f,fmt,false});}}
    if(finished){std::vector<Job> left;for(auto& j:m.jobs){if(!finished(j.dst))left.push_back(j);}m.jobs=left;}
    for(auto& j:m.jobs){std::set<std::wstring> u;for(auto& s:stringsOf(j.src,j.fmt))if(translatable(s))u.insert(s);m.strings+=(int)u.size();}
    if(!m.strings)m.jobs.clear();}
// --- records --------------------------------------------------------------------------------------------------------------
struct Record {std::wstring file,backup,lang,hash,full;}; // full "1": every line translated, "0": some left in English (today's limit)
inline std::wstring esc(const std::wstring& s){std::wstring o;for(wchar_t c:s){if(c==L'\\')o+=L"\\\\";else if(c==L'\t')o+=L"\\t";else if(c==L'\n')o+=L"\\n";else if(c==L'\r')o+=L"\\r";else o+=c;}return o;}
inline std::wstring unesc(const std::wstring& s){std::wstring o;for(size_t i=0;i<s.size();i++){if(s[i]==L'\\'&&i+1<s.size()){wchar_t c=s[++i];o+=c==L't'?L'\t':c==L'n'?L'\n':c==L'r'?L'\r':c;}else o+=s[i];}return o;}
inline std::vector<std::wstring> splitTabs(const std::wstring& l){std::vector<std::wstring> v;size_t p=0;for(size_t q;(q=l.find(L'\t',p))!=std::wstring::npos;p=q+1)v.push_back(unesc(l.substr(p,q-p)));v.push_back(unesc(l.substr(p)));return v;}
inline std::vector<std::wstring> readLines(const std::wstring& path){std::string b;std::wstring t;int enc;std::vector<std::wstring> v;if(!readBytes(path,b,64u<<20)||!decodeText(b,t,enc)){return v;}
    for(size_t p=0;p<t.size();){size_t q=t.find(L'\n',p);if(q==std::wstring::npos)q=t.size();std::wstring l=t.substr(p,q-p);if(!l.empty()&&l.back()==L'\r')l.pop_back();if(!l.empty())v.push_back(l);p=q+1;}return v;}
inline bool saveLines(const std::wstring& path,const std::vector<std::wstring>& lines){std::wstring t;for(auto& l:lines)t+=l+L"\n";std::wstring tmp=path+L".tmp";return writeBytes(tmp,utf8(t))&&MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);}
inline std::vector<Record> records(const std::wstring& store){std::vector<Record> v;for(auto& l:readLines(store+L"\\applied.tsv")){auto f=splitTabs(l);if(f.size()==4||f.size()==5)v.push_back({f[0],f[1],f[2],f[3],f.size()==5?f[4]:L"1"});}return v;}
inline bool saveRecords(const std::wstring& store,const std::vector<Record>& v){std::vector<std::wstring> lines;for(auto& r:v)lines.push_back(esc(r.file)+L"\t"+esc(r.backup)+L"\t"+esc(r.lang)+L"\t"+esc(r.hash)+L"\t"+r.full);return saveLines(store+L"\\applied.tsv",lines);}
inline std::wstring fileHash(const std::wstring& p){std::string b;return readBytes(p,b,64u<<20)?hex(fnv(b)):L"";}
inline std::function<bool(const std::wstring&)> finishedIn(const std::vector<Record>& recs,const std::wstring& lang){return [recs,lang](const std::wstring& f){for(auto& r:recs){if(r.lang==lang&&r.full==L"1"&&lower(r.file)==lower(f))return fileHash(f)==r.hash;}return false;};}
// Files of this mod BlueAsh has translated into lang and that are still as written.
inline bool inside(const Mod& m,const std::wstring& file){std::wstring l=lower(file),root=lower(m.root);if(!root.empty()&&(l==root||l.rfind(root+L"\\",0)==0)){return true;}for(auto& f:m.files)if(lower(f)==l){return true;}return false;}
inline int translatedFiles(const Mod& m,const std::vector<Record>& recs,const std::wstring& lang){int n=0;for(auto& r:recs)if(r.lang==lang&&inside(m,r.file)&&fileHash(r.file)==r.hash)n++;return n;}
// --- translating -------------------------------------------------------------------------------------------------------------
// One batch of texts to the server; fills out (same size, "" = not translated). 0 ok, 429 today's limit, else an error.
using Translator=std::function<int(const std::vector<std::wstring>& in,std::vector<std::wstring>& out)>;
struct Outcome {int mods=0,files=0,lines=0,missing=0,failed=0,status=0;};
inline std::map<std::wstring,std::wstring> loadCache(const std::wstring& path){std::map<std::wstring,std::wstring> c;for(auto& l:readLines(path)){auto f=splitTabs(l);if(f.size()==2)c[f[0]]=f[1];}return c;}
inline void saveCache(const std::wstring& path,const std::map<std::wstring,std::wstring>& c){std::vector<std::wstring> lines;for(auto& [k,v]:c)lines.push_back(esc(k)+L"\t"+esc(v));saveLines(path,lines);}
inline Outcome translate(std::vector<Mod>& mods,int target,const std::wstring& store,const Translator& send,const std::function<void(int,int)>& progress,const std::map<std::wstring,std::wstring>* dict=nullptr,const std::atomic<bool>* stop=nullptr){
    Outcome out;const std::wstring lang=langs()[target].ui,w3=langs()[target].w3;CreateDirectoryW(store.c_str(),nullptr);CreateDirectoryW((store+L"\\backup").c_str(),nullptr);
    std::wstring cachePath=store+L"\\cache-"+lang+L".tsv";auto cache=loadCache(cachePath);
    // every line to translate, protected, once; a line the game itself says in its official text (dict) is taken from there
    std::vector<std::wstring> todo;std::set<std::wstring> want;
    for(auto& m:mods)for(auto& j:m.jobs)for(auto& s:stringsOf(j.src,j.fmt))if(translatable(s)){auto p=protect(s);if(dict&&p.slots.empty()&&!cache.count(p.text)){auto d=dict->find(lower(p.text));if(d!=dict->end()){cache[p.text]=d->second;continue;}}if(p.text.size()<=1500&&!cache.count(p.text)&&want.insert(p.text).second)todo.push_back(p.text);}
    size_t done=0;progress(0,(int)todo.size());
    for(size_t i=0;i<todo.size()&&!out.status;){std::vector<std::wstring> batch;size_t chars=0;while(i<todo.size()&&batch.size()<20&&chars+todo[i].size()<=2500){chars+=todo[i].size();batch.push_back(todo[i++]);}
        if(batch.empty())batch.push_back(todo[i++].substr(0,1500));
        if(stop&&*stop){out.status=-1;break;}std::vector<std::wstring> got;int s=send(batch,got);if(s){out.status=s;break;}
        for(size_t k=0;k<batch.size()&&k<got.size();k++){if(!got[k].empty())cache[batch[k]]=got[k];}done+=batch.size();saveCache(cachePath,cache);progress((int)done,(int)todo.size());}
    auto recs=records(store);
    for(auto& m:mods){bool touched=false;for(auto& j:m.jobs){std::wstring text;int enc=0;W3File w;if(!readDoc(j.src,j.fmt,text,enc,w)){out.failed++;continue;}int lines=0,missing=out.missing;
            auto tr=[&](const std::wstring& s){if(!translatable(s)){return s;}auto p=protect(s);auto c=cache.find(p.text);if(c==cache.end()){out.missing++;return s;}std::wstring r=restore(p,c->second);if(r.empty()){out.missing++;return s;}lines++;return r;};
            std::string bytes;if(j.fmt==W3){for(auto& s:w.strs)s=tr(s);bytes=w3write(w,w3);}
            else{auto segs=segments(j.fmt,text);std::wstring o;size_t at=0;for(auto& g:segs){o+=text.substr(at,g.a-at);std::wstring r=tr(g.s);o+=r==g.s?text.substr(g.a,g.b-g.a):encodeFor(j.fmt,r);at=g.b;}o+=text.substr(at);bytes=encodeText(o,enc);}
            if(!lines){continue;}
            // the original kept once per file; a file changed since BlueAsh wrote it (a mod update) is kept again
            auto rec=std::find_if(recs.begin(),recs.end(),[&](const Record& r){return lower(r.file)==lower(j.dst);});std::wstring now=fileHash(j.dst);
            std::wstring backup=L"-";bool ours=rec!=recs.end()&&rec->hash==now; // already BlueAsh's: the first original stays the backup
            if(ours)backup=rec->backup;else if(exists(j.dst)){backup=store+L"\\backup\\"+hex(fnv(utf8(lower(j.dst))))+L".bak";std::string orig;if(!readBytes(j.dst,orig,64u<<20)||!writeBytes(backup,orig)){out.failed++;continue;}}
            if(!writeBytes(j.dst,bytes)){out.failed++;continue;}
            Record r{j.dst,backup,lang,hex(fnv(bytes)),out.missing==missing?L"1":L"0"};if(rec!=recs.end())*rec=r;else recs.push_back(r);saveRecords(store,recs);out.files++;out.lines+=lines;touched=true;}
        if(touched)out.mods++;}
    return out;}
// 원래대로: every file BlueAsh wrote inside these mods back as it was (a file it made itself is removed); files the mod has
// replaced since are left alone. Returns how many came back, how many were skipped.
inline std::pair<int,int> restoreAll(const std::vector<Mod>& mods,const std::wstring& store){auto recs=records(store);int back=0,skipped=0;std::vector<Record> keep;
    for(auto& r:recs){bool mine=false;for(auto& m:mods)if(inside(m,r.file)){mine=true;break;}
        if(!mine){keep.push_back(r);continue;}if(fileHash(r.file)!=r.hash){skipped++;continue;}
        bool ok;if(r.backup==L"-")ok=DeleteFileW(r.file.c_str())!=0;else{std::string orig;ok=readBytes(r.backup,orig,64u<<20)&&writeBytes(r.file,orig);}
        if(ok)back++;else{skipped++;keep.push_back(r);}}
    saveRecords(store,keep);return {back,skipped};}
// --- the AI translator (local_ai.h) --------------------------------------------------------------------------------------
inline const wchar_t* langName(const std::wstring& ui){static const std::map<std::wstring,const wchar_t*> n={{L"ko",L"Korean"},{L"ja",L"Japanese"},{L"zh-CN",L"Simplified Chinese"},{L"zh-TW",L"Traditional Chinese (Taiwan)"},
    {L"ru",L"Russian"},{L"de",L"German"},{L"es",L"Spanish"},{L"fr",L"French"},{L"pt-BR",L"Brazilian Portuguese"},{L"it",L"Italian"},{L"pl",L"Polish"},{L"tr",L"Turkish"},{L"en",L"English"}};auto i=n.find(ui);return i==n.end()?L"English":i->second;}
inline std::wstring jsonArray(const std::vector<std::wstring>& v){std::wstring j=L"[";for(size_t i=0;i<v.size();i++){if(i){j+=L",";}j+=L"\""+jsonEscape(v[i])+L"\"";}return j+L"]";}
// The model's JSON array of strings (anything around it ignored); false when it is not one of n strings.
inline bool arrayOf(const std::wstring& t,size_t n,std::vector<std::wstring>& out){size_t i=t.find(L'[');if(i==std::wstring::npos){return false;}out.clear();
    for(i++;i<t.size();){while(i<t.size()&&(iswspace(t[i])||t[i]==L',')){i++;}if(i>=t.size()){return false;}if(t[i]==L']'){break;}
        if(t.compare(i,4,L"null")==0){out.push_back(L"");i+=4;continue;}if(t[i]!=L'"'){return false;}size_t e=i+1;while(e<t.size()&&t[e]!=L'"'){if(t[e]==L'\\'){e++;}e++;}if(e>=t.size()){return false;}out.push_back(jsonUnescape(t.substr(i+1,e-i-1)));i=e+1;}
    return out.size()==n;}
// The game's own words: its official English and target-language string tables side by side (The Witcher 3:
// content\content0\en.w3strings and kr.w3strings ...), lower-case English -> target. An English line the game translates
// two ways is left out. Empty for other games.
inline std::map<std::wstring,std::wstring> gameWords(const std::wstring& game,const std::wstring& w3){std::map<std::wstring,std::wstring> d;std::string a,b;W3File en,tg;
    std::wstring dir=game+L"\\content\\content0\\";if(w3==L"en"||!readBytes(dir+L"en.w3strings",a,64u<<20)||!readBytes(dir+w3+L".w3strings",b,64u<<20)||!w3read(a,en)||!w3read(b,tg)){return d;}
    std::map<uint32_t,const std::wstring*> t;for(size_t i=0;i<tg.ids.size();i++){t[tg.ids[i]]=&tg.strs[i];}std::set<std::wstring> twice;
    for(size_t i=0;i<en.ids.size();i++){auto it=t.find(en.ids[i]);if(it==t.end()||en.strs[i].empty()||en.strs[i].size()>80||it->second->empty()){continue;}std::wstring k=lower(en.strs[i]);
        auto have=d.find(k);if(have!=d.end()&&have->second!=*it->second){twice.insert(k);}else{d[k]=*it->second;}}
    for(auto& k:twice){d.erase(k);}return d;}
// Official names in these lines (runs of one to three words), for the model to use: "Witcher = 위쳐".
inline std::vector<std::pair<std::wstring,std::wstring>> wordsIn(const std::map<std::wstring,std::wstring>& d,const std::vector<std::wstring>& lines,size_t most=30){
    std::vector<std::pair<std::wstring,std::wstring>> out;std::set<std::wstring> seen;
    for(auto& l:lines){std::vector<std::wstring> w;std::wstring cur;for(wchar_t c:lower(l)+L" "){if(iswalnum(c)||c==L'\''||c==L'-'){cur+=c;}else if(!cur.empty()){w.push_back(cur);cur.clear();}}
        for(size_t i=0;i<w.size();i++){std::wstring k;for(size_t n=0;n<3&&i+n<w.size();n++){k+=(n?L" ":L"")+w[i+n];if(k.size()<3||seen.count(k)){continue;}auto f=d.find(k);if(f!=d.end()){seen.insert(k);out.push_back(*f);if(out.size()>=most){return out;}}}}}
    return out;}
inline std::wstring instructions(const std::wstring& game,const std::wstring& ui,const std::vector<std::pair<std::wstring,std::wstring>>& words){std::wstring lang=langName(ui);
    std::wstring s=L"You translate the user-interface text of a mod for the PC game \""+game+L"\" (menu options, settings, item names and descriptions, dialogue) from English into "+lang+L" for players. "
        L"Use the words the game itself uses in its official "+lang+L" version, and write natural "+lang+L". You get a JSON array of strings; answer with only a JSON array of the same length, in the same order, each element the translation of the element at that position. "
        L"Keep every marker like \x27e6" L"0\x27e7 exactly once, unchanged, where it belongs in the sentence. Keep key names, units and short codes as they are. Keep a short label short.";
    if(!words.empty()){s+=L" The game's official names: ";for(size_t i=0;i<words.size();i++){s+=(i?L"; ":L"")+words[i].first+L" = "+words[i].second;}s+=L".";}
    return s;}
}
