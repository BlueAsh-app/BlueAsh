// Graphics.ini screen-mode edit: finds the one digit and changes nothing else.
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <cstdio>
HWND win=nullptr;const wchar_t* configPath=L"";std::wstring regString(const wchar_t*,const wchar_t*){return L"";}bool saveString(const wchar_t*,const wchar_t*,const std::wstring&){return true;}bool tarkovProcess(){return false;}
void reportProblem(const wchar_t*){}
#include "tarkov_screen.h"
int fail=0;void check(bool ok,const char* what){if(!ok){std::printf("FAIL: %s\n",what);fail++;}}
int main(){
    std::string a="{\r\n  \"Display\": 0,\r\n    \"FullScreenMode\": 0,\r\n    \"Resolution\": {\r\n";
    size_t at=TarkovScreen::modeAt(a);check(at!=std::string::npos&&a[at]=='0',"finds fullscreen digit");
    std::string b=a;b[at]='1';check(b.size()==a.size()&&b.find("\"FullScreenMode\": 1,")!=std::string::npos&&b.find("\"Display\": 0,")!=std::string::npos,"only the mode digit changes");
    check(TarkovScreen::modeAt("{\"FullScreenMode\":2}")!=std::string::npos,"compact json");
    check(TarkovScreen::modeAt("{\"FullScreenMode\": 10}")==std::string::npos,"two-digit value left alone");
    check(TarkovScreen::modeAt("{\"Other\": 0}")==std::string::npos,"missing key");
    // The real file (read only): the digit is found where the game wrote it.
    std::string real;if(TarkovScreen::read(TarkovScreen::path(),real)){size_t r=TarkovScreen::modeAt(real);check(r!=std::string::npos,"real Graphics.ini parsed");std::printf("real file mode: %c\n",r!=std::string::npos?real[r]:'?');}
    if(!fail){std::printf("PASS: tarkov screen mode edit\n");}
    return fail;
}
