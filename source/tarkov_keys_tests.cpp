// Control.ini edits: one-time key moves and the map key as second screenshot key, nothing else changes.
// Uses a fixture in the game's own layout (CRLF, 2-space indent); the real file is only parsed, never written.
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <cstdio>
HWND win=nullptr;const wchar_t* configPath=L"";std::wstring regString(const wchar_t*,const wchar_t*){return L"";}bool saveString(const wchar_t*,const wchar_t*,const std::wstring&){return true;}bool tarkovProcess(){return true;}
void reportProblem(const wchar_t*){}
#include "tarkov_screen.h"
#include "tarkov_keys.h"
int fail=0;void check(bool ok,const char* what){if(!ok){std::printf("FAIL: %s\n",what);fail++;}}
std::string binding(const char* name,const char* first,const char* press){
    std::string keys=first[0]?std::string("\r\n            \"")+first+"\"\r\n          ]":"]";
    if(std::string(first)=="M\",\r\n            \"LeftControl")keys=std::string("\r\n            \"M\",\r\n            \"LeftControl\"\r\n          ]");
    return std::string("    {\r\n      \"keyName\": \"")+name+"\",\r\n      \"variants\": [\r\n        {\r\n          \"keyCode\": ["+keys+"\r\n        },\r\n        {\r\n          \"keyCode\": []\r\n        }\r\n      ],\r\n      \"pressType\": \""+press+"\"\r\n    }";
}
int main(){
    std::string fixture="{\r\n  \"keyBindings\": [\r\n"+binding("WeaponMounting","M","Release")+",\r\n"+binding("ToggleBipods","M\",\r\n            \"LeftControl","Press")+",\r\n"
        +binding("Recorder","M","Release")+",\r\n"+binding("MakeScreenshot","SysReq","Press")+"\r\n  ]\r\n}\r\n";
    std::vector<std::string> k;
    check(gamekeys::get(fixture,"WeaponMounting",0,k)&&k.size()==1&&k[0]=="M","reads WeaponMounting = M");
    check(gamekeys::get(fixture,"MakeScreenshot",0,k)&&k.size()==1&&k[0]=="SysReq","reads screenshot = SysReq");
    check(gamekeys::get(fixture,"MakeScreenshot",1,k)&&k.empty(),"second screenshot slot empty");
    check(gamekeys::get(fixture,"ToggleBipods",0,k)&&k.size()==2,"two-key binding read");
    std::string t=fixture,added;std::string left=gamekeys::apply(t,"WeaponMounting:M>Comma;NoSuchBinding:A>B;","M",added);
    check(gamekeys::get(t,"WeaponMounting",0,k)&&k.size()==1&&k[0]=="Comma","mounting moved to Comma");
    check(gamekeys::get(t,"MakeScreenshot",1,k)&&k.size()==1&&k[0]=="M"&&added=="M","map key added as second screenshot key");
    check(gamekeys::get(t,"MakeScreenshot",0,k)&&k.size()==1&&k[0]=="SysReq","first screenshot key kept");
    check(left=="NoSuchBinding:A>B;","unknown binding stays pending");
    check(gamekeys::get(t,"Recorder",0,k)&&k.size()==1&&k[0]=="M","other bindings untouched");
    check(t.find("\"keyCode\": [\r\n            \"M\"\r\n          ]\r\n        }\r\n      ],\r\n      \"pressType\": \"Press\"")!=std::string::npos,"written in the game's layout");
    // Undo restores the screenshot slot exactly; the move is not repeated.
    std::string u=t;gamekeys::apply(u,"","",added);std::string back=fixture;std::vector<std::string> c={"Comma"};gamekeys::set(back,"WeaponMounting",0,c);
    check(u==back&&added.empty(),"turning the option off restores the file byte for byte");
    // A user's own second screenshot key is never replaced.
    std::string own=fixture;std::vector<std::string> f9={"F9"};gamekeys::set(own,"MakeScreenshot",1,f9);std::string mine;gamekeys::apply(own,"","M",mine);
    check(gamekeys::get(own,"MakeScreenshot",1,k)&&k.size()==1&&k[0]=="F9"&&mine.empty(),"user's own binding kept");
    // The real file (when present) parses: read only.
    std::string real;if(TarkovScreen::read(TarkovKeys::path(),real))check(gamekeys::get(real,"MakeScreenshot",0,k),"real Control.ini parses");
    if(!fail){std::printf("PASS: tarkov key edits\n");}
    return fail;
}
