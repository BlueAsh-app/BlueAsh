#pragma once
// DLSS 5 (Neural Rendering) game safety policy: pure logic, no Win32 (dlss_policy_tests.cpp includes it alone).
// A game is shown on the DLSS 5 page only when it is VERIFIED: an official DLSS 5 title from the catalog below. Everything
// else is UNSUPPORTED (known, not usable yet), BLOCKED (never touched: account risk) or UNKNOWN (no safety information).
// The hard block list wins over everything, and it matches on several signals at once (store id, file and folder names
// anywhere in the install, the exe's product / company name), so renaming an exe or adding the folder by hand does not
// get past it. Only the DLSS 5 page uses this; the Tarkov / PUBG helpers elsewhere are unaffected.
#include <string>
#include <vector>
#include <algorithm>
namespace dlss {
enum class Status {Verified,Unsupported,Blocked,Unknown};
// What the library scan found about one installed game. All strings lower case.
struct Evidence {std::string platform,id;std::wstring name,dir;std::vector<std::string> files,products;};
// How DLSS 5 is turned on for a verified game. "ingame" = the game's own graphics menu (official DLSS 5 titles).
// "optiscaler" = the community OptiScaler DLSS-NR route (dlss_optiscaler.h) with the person's own signed runtime; exeRel
// is the executable it goes beside. Community titles are listed only when single-player, without anti-cheat, with DLSS
// in the game (the route takes the game's DLSS output) and with the route seen working (NGX logs in public reports).
struct Title {const char* platform;const char* id;const char* exe;const wchar_t* name;Status status;const char* method;const wchar_t* note;const wchar_t* exeRel=nullptr;};
// Sources (2026-10-25): NVIDIA GeForce news (NBA 2K27 live in Video Settings, RTX Remix runtime Oct 23, Hogwarts Legacy Nov 5),
// METRO Redux next-gen update (Oct 22, official per 4A / press). Remix games need the newest runtime, which their Steam
// builds are not confirmed to carry, so they stay UNSUPPORTED until checked.
inline const std::vector<Title>& catalog(){static const std::vector<Title> t={
    {"steam","4356430","nba2k27.exe",L"NBA 2K27",Status::Verified,"ingame",L"게임 안 비디오 설정에서 DLSS 5를 켭니다."},
    {"steam","286690","metro2033.exe",L"METRO 2033 Redux",Status::Verified,"ingame",L"게임 안 그래픽 설정에서 DLSS 5를 켭니다."},
    {"steam","287390","metroll.exe",L"METRO Last Light Redux",Status::Verified,"ingame",L"게임 안 그래픽 설정에서 DLSS 5를 켭니다."},
    // Single-player, no anti-cheat, DLSS 3.x in the DX12 build; the OptiScaler DLSS-NR route was logged working with
    // models A and B (DLSS AiO Swapper notes, NGX logs, 2026-10-21).
    {"steam","292030","witcher3.exe",L"The Witcher 3: Wild Hunt",Status::Verified,"optiscaler",L"커뮤니티 경로(OptiScaler DLSS-NR, 비공식) · 싱글플레이 · DX12 실행 파일",L"bin\\x64_dx12\\witcher3.exe"},
    {"steam","990080","hogwartslegacy.exe",L"Hogwarts Legacy",Status::Unsupported,"",L"DLSS 5 업데이트가 2026년 11월 5일로 예정돼 있습니다."},
    {"steam","2012840","portal_rtx.exe",L"Portal with RTX",Status::Unsupported,"",L"최신 RTX Remix 런타임이 필요합니다. Steam 버전에 들어 있는지는 확인되지 않았습니다."},
    {"steam","2477290","",L"Half-Life 2 RTX",Status::Unsupported,"",L"최신 RTX Remix 런타임이 필요합니다. Steam 버전에 들어 있는지는 확인되지 않았습니다."}};return t;}
// Never listed, whatever else matches. Store ids, then names that appear as files or folders inside the install or as the
// exe's product / company name.
struct Block {const char* ids;const char* marks;const wchar_t* reason;};
inline const std::vector<Block>& blocked(){static const std::vector<Block> b={
    {"steam:3932890","escapefromtarkov|escapefromtarkov_data|battlestate|escape from tarkov",L"타르코프는 DLSS 5 기능에서 제외됩니다 (계정 제재 위험)."},
    {"steam:578080","tslgame|pubg|krafton|battlegrounds",L"배틀그라운드는 DLSS 5 기능에서 제외됩니다 (계정 제재 위험)."},
    {"","valorant|riot vanguard|vgc.exe|vgk.sys",L"발로란트(뱅가드)는 외부 렌더링 모드를 허용하지 않습니다."},
    {"epic:fortnite","fortniteclient|fortnite",L"포트나이트는 외부 렌더링 모드를 허용하지 않습니다."}};return b;}
// Anti-cheat in the install: not proof of a ban, but safety cannot be confirmed, so such games are excluded (BLOCKED).
inline const std::vector<std::string>& anticheat(){static const std::vector<std::string> a={"battleye","beservice","easyanticheat","easyanticheat_eos","eaanticheat","gameguard","xigncode","anticheatexpert","faceit","mhyprot","vanguard","nprotect"};return a;}
inline std::string lower(std::string s){for(auto& c:s)c=(char)tolower((unsigned char)c);return s;}
inline bool has(const std::vector<std::string>& v,const std::string& token){
    for(const auto& x:v){if(x==token||x==token+".exe"||x==token+".dll"||x==token+".sys"||x.find(token)!=std::string::npos)return true;}return false;}
struct Verdict {Status status=Status::Unknown;const Title* title=nullptr;std::wstring reason;};
inline Verdict classify(const Evidence& e){
    Verdict v;std::string key=e.platform+":"+lower(e.id);
    // Names gathered from the install: files, folders and exe product / company names (a renamed exe keeps its resources).
    std::vector<std::string> seen=e.files;seen.insert(seen.end(),e.products.begin(),e.products.end());
    std::string name;for(wchar_t c:e.name){if(c<128)name+=(char)tolower((int)c);}seen.push_back(name);
    for(const auto& b:blocked()){std::string ids=b.ids,marks=b.marks;bool hit=!e.id.empty()&&!ids.empty()&&("|"+ids+"|").find("|"+key+"|")!=std::string::npos;
        size_t at=0;while(!hit&&at<=marks.size()){size_t bar=marks.find('|',at);std::string m=marks.substr(at,bar==std::string::npos?std::string::npos:bar-at);if(!m.empty()&&has(seen,m))hit=true;if(bar==std::string::npos)break;at=bar+1;}
        if(hit){v.status=Status::Blocked;v.reason=b.reason;return v;}}
    for(const auto& t:catalog()){if(e.platform==t.platform&&e.id==t.id){v.status=t.status;v.title=&t;v.reason=t.note;return v;}}
    for(const auto& a:anticheat()){if(has(e.files,a)){v.status=Status::Blocked;v.reason=L"안티치트가 설치돼 있어 외부 렌더링 모드의 안전을 확인할 수 없습니다.";return v;}}
    // A verified title found without its store record (folder added by hand): the exe name alone is not enough to verify it.
    for(const auto& t:catalog()){if(*t.exe&&has(e.files,t.exe)){v.status=Status::Unknown;v.title=&t;v.reason=L"스토어 정보로 확인되지 않아 자동 적용하지 않습니다.";return v;}}
    v.reason=L"DLSS 5 지원과 안전성이 확인되지 않은 게임입니다.";return v;
}
// The built-in presets. Values are not invented: a preset only carries settings the game's DLSS 5 path really exposes,
// and for "ingame" titles that is nothing (the player picks DLSS 5 in the game menu), so only ORIGINAL and the game's own
// DLSS 5 can be compared there.
enum class Preset {Original,Balanced,Realistic,Cinematic,Custom};
inline const char* presetKey(Preset p){const char* k[]={"original","balanced","realistic","cinematic","custom"};return k[(int)p];}
inline bool presetSupported(const Title* t,Preset p){if(p==Preset::Original)return true;if(!t||t->status!=Status::Verified)return false;
    std::string m=t->method;return m=="optiscaler"?true:m=="ingame"?p==Preset::Balanced:false;} // "ingame": BALANCED = the game's own DLSS 5 (its default tuning)
}
