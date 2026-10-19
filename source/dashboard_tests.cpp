#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <algorithm>
#include <cassert>
#include <iostream>
#include "profiles.h"
#include "i18n.h"
settings::Profiles profiles=settings::defaults();
bool active=false;int appliedProfileIndex=0;HWND win=nullptr;
HFONT smallFont=nullptr,smallBold=nullptr,iconFont=nullptr;HBRUSH panelBrush=nullptr;
COLORREF well=0,blue=0,ink=0,muted=0;
const wchar_t* configPath=L"Software\\ModeSwitch";bool autoGame=false;
bool applyOkay=true,saveOkay=true;int applyCalls=0,saveCalls=0,lastIndex=-1;std::wstring error;
bool changeMode(bool,int index=-1){applyCalls++;lastIndex=index;return applyOkay;}
bool saveString(const wchar_t*,const wchar_t*,const std::wstring&){saveCalls++;return saveOkay;}
void problem(const std::wstring& s){error=s;}
void refreshStatus(){}
void textAt(HDC,const std::wstring&,RECT,HFONT,COLORREF,UINT=DT_LEFT|DT_VCENTER|DT_SINGLELINE){}
void roundBox(HDC,RECT,COLORREF,COLORREF,int,HBRUSH=nullptr){}
void glyph(HDC,wchar_t,RECT,HFONT,COLORREF){}
namespace painting {bool circle(HDC,RECT,COLORREF,COLORREF,float=1.f){return false;}void outline(HDC,RECT,COLORREF,int){}}
#include "dashboard_ui.h"
int main(){
 assert(dashboard::fromPoint(62,-100,376)==50);
 assert(dashboard::fromPoint(62,1000,376)==300);
 assert(dashboard::fromPoint(61,-100,376)==0);
 assert(dashboard::fromPoint(61,1000,376)==100);
 assert(dashboard::fromPoint(62,10,0)==50);
 profiles[3].extended=true;profiles[3].brightness[1]=123;profiles[3].contrast[2]=88;
 active=true;appliedProfileIndex=3;
 assert(dashboard::profileIndex()==3);
 assert(dashboard::commit(62,155));
 assert(lastIndex==3&&profiles[3].gamma==155&&profiles[3].greenGamma==155&&profiles[3].blueGamma==155);
 assert(profiles[3].brightness[1]==123&&profiles[3].contrast[2]==88);
 auto before=profiles[3];applyOkay=false;saveCalls=0;
 assert(!dashboard::commit(61,12));assert(profiles[3].volume==before.volume&&saveCalls==0);
 applyOkay=true;saveOkay=false;applyCalls=0;
 assert(!dashboard::commit(61,12));assert(profiles[3].volume==before.volume&&applyCalls==2&&!error.empty());
 saveOkay=true;appliedProfileIndex=0;applyCalls=0; // a mode switch during a drag must not apply the old mode again.
 assert(dashboard::commit(61,21,3));assert(profiles[3].volume==21&&applyCalls==0);
 active=false;saveCalls=0;
 assert(dashboard::commit(61,84));assert(profiles[0].volume==84&&saveCalls==1);
 std::cout<<"Dashboard bounds, profile identity, persistence and failure rollback PASS\n";
}
