#include "game_profile_state.h"
#include <cassert>
int main(){
    using namespace gameprofiles;
    assert(within(L"C:\\Games\\Example\\bin\\game.exe",L"c:/games/example"));
    assert(!within(L"C:\\Games\\Example2\\game.exe",L"C:\\Games\\Example"));
    assert(!within(L"C:\\Games\\Example\\launcher.exe",L"C:\\Games\\Example"));
    assert(choose({L"a",L"b"},L"b",L"a")==L"b");
    assert(choose({L"a",L"b"},L"",L"a")==L"a");
    assert(choose({L"a",L"b"},L"",L"").empty());
    assert(choose({L"a"},L"",L"")==L"a");
    assert(choose({},L"",L"a").empty());
    std::map<std::wstring,settings::Profile> saved;
    saved[L"steam:123"]=settings::defaults()[1];saved[L"steam:123"].volume=42;
    saved[L"epic:game"]=settings::defaults()[3];saved[L"epic:game"].hue=42;
    auto encoded=encodeProfiles(saved);std::map<std::wstring,settings::Profile> loaded;
    assert(decodeProfiles(encoded,loaded));assert(loaded.size()==2);
    assert(loaded[L"steam:123"].volume==42&&loaded[L"epic:game"].hue==42);
    auto before=loaded;assert(!decodeProfiles(encoded+L"broken",loaded));assert(loaded.size()==before.size());
}
