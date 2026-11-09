#include <cassert>
#include <cstdio>
#include "dlss_policy.h"
using namespace dlss;
static Evidence ev(const char* platform,const char* id,const wchar_t* name,std::vector<std::string> files,std::vector<std::string> products={}){Evidence e;e.platform=platform;e.id=id;e.name=name;e.files=files;e.products=products;return e;}
int main(){
    // TEST 1: Tarkov and PUBG never reach the list, by store id or by what is inside the install.
    assert(classify(ev("steam","578080",L"PUBG: BATTLEGROUNDS",{"tslgame","binaries"})).status==Status::Blocked);
    assert(classify(ev("steam","3932890",L"Escape from Tarkov",{})).status==Status::Blocked);
    assert(classify(ev("folder","",L"Escape from Tarkov",{"escapefromtarkov.exe","escapefromtarkov_data","battleye"})).status==Status::Blocked);
    // TEST 2: renamed exe + folder added by hand + harmless display name: the data folder or the exe resources still give it away.
    assert(classify(ev("folder","",L"My Game",{"game.exe","escapefromtarkov_data"})).status==Status::Blocked);
    assert(classify(ev("folder","",L"My Game",{"launcher.exe"},{"krafton, inc.","playerunknown's battlegrounds"})).status==Status::Blocked);
    assert(classify(ev("folder","",L"x",{"x.exe"},{"battlestate games"})).status==Status::Blocked);
    // Even a stolen store id of a verified game does not unblock a blocked install.
    assert(classify(ev("steam","4356430",L"NBA 2K27",{"tslgame.exe"})).status==Status::Blocked);
    assert(classify(ev("epic","fortnite",L"Fortnite",{})).status==Status::Blocked);
    assert(classify(ev("folder","",L"Valorant",{"valorant.exe"})).status==Status::Blocked);
    // Anti-cheat in an unknown game: excluded (cannot confirm), with a reason.
    auto ac=classify(ev("steam","1",L"Some Shooter",{"easyanticheat","game.exe"}));assert(ac.status==Status::Blocked&&!ac.reason.empty());
    // Official DLSS 5 titles are verified by store id only.
    auto nba=classify(ev("steam","4356430",L"NBA 2K27",{"nba2k27.exe"}));assert(nba.status==Status::Verified&&nba.title&&std::string(nba.title->method)=="ingame");
    assert(classify(ev("steam","286690",L"METRO 2033 Redux",{"metro2033.exe"})).status==Status::Verified);
    assert(classify(ev("steam","990080",L"Hogwarts Legacy",{})).status==Status::Unsupported);
    // The same exe in a hand-added folder is not verified (identification incomplete -> no automatic apply).
    assert(classify(ev("folder","",L"NBA",{"nba2k27.exe"})).status==Status::Unknown);
    // The community route title: verified by store id, all presets; a single-player game without catalog data stays unknown.
    auto w3=classify(ev("steam","292030",L"The Witcher 3: Wild Hunt",{"bin","witcher3.exe"}));assert(w3.status==Status::Verified&&std::string(w3.title->method)=="optiscaler"&&w3.title->exeRel);
    assert(presetSupported(w3.title,Preset::Cinematic)&&presetSupported(w3.title,Preset::Custom));
    assert(classify(ev("steam","374320",L"DARK SOULS III",{"darksoulsiii.exe"})).status==Status::Unknown);
    assert(classify(ev("steam","",L"",{})).status==Status::Unknown);
    // Presets: only what the game's DLSS 5 path exposes.
    assert(presetSupported(nba.title,Preset::Original)&&presetSupported(nba.title,Preset::Balanced));
    assert(!presetSupported(nba.title,Preset::Realistic)&&!presetSupported(nba.title,Preset::Cinematic)&&!presetSupported(nba.title,Preset::Custom));
    assert(!presetSupported(nullptr,Preset::Balanced));
    puts("DLSS policy tests passed");
}
