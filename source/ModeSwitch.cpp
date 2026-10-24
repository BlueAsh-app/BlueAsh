// ModeSwitch - GPL-3.0-or-later. See LICENSE.txt and NOTICE.txt.
// NVIDIA private ABI and color curve adapted from nvBrightness:
// Copyright (c) 2025 Pete Batard <pete@akeo.ie>, GPL-3.0-or-later.
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <objidl.h>
#include <gdiplus.h>
#include <commctrl.h>

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <map>
#include <deque>
#include <set>
#include <functional>
#include "profiles.h"
#include "i18n.h"
#include "ui_paint.h"

template<class T> struct Com {
    T* p = nullptr;
    ~Com() { if(p) p->Release(); }
    T** out() { return &p; }
    T* operator->() { return p; }
};
std::wstring hrtext(HRESULT h) { wchar_t b[32]; swprintf(b,32,L"0x%08lX",(unsigned long)h); return b; }
void check(HRESULT h) { if(FAILED(h)) throw std::wstring(i18n::text(L"Windows 오디오 오류: "))+hrtext(h); }

struct AudioResult { int count=0; int failed=0; };
AudioResult volume(const std::wstring& process, float level) {
    Com<IMMDeviceEnumerator> e;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,__uuidof(IMMDeviceEnumerator),(void**)e.out()));
    Com<IMMDeviceCollection> devices;
    check(e->EnumAudioEndpoints(eRender,DEVICE_STATE_ACTIVE,devices.out()));
    UINT n=0; check(devices->GetCount(&n)); AudioResult result;
    for(UINT i=0;i<n;i++) {
        Com<IMMDevice> d; if(FAILED(devices->Item(i,d.out()))) { result.failed++; continue; }
        Com<IAudioSessionManager2> manager;
        if(FAILED(d->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,(void**)manager.out()))) { result.failed++; continue; }
        Com<IAudioSessionEnumerator> sessions;
        if(FAILED(manager->GetSessionEnumerator(sessions.out()))) { result.failed++; continue; }
        int count=0; if(FAILED(sessions->GetCount(&count))) { result.failed++; continue; }
        for(int j=0;j<count;j++) {
            Com<IAudioSessionControl> control;
            if(FAILED(sessions->GetSession(j,control.out()))) continue;
            Com<IAudioSessionControl2> detail;
            if(FAILED(control->QueryInterface(__uuidof(IAudioSessionControl2),(void**)detail.out()))) continue;
            DWORD pid=0; if(FAILED(detail->GetProcessId(&pid)) || !pid) continue;
            HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
            if(!p) continue;
            wchar_t path[32768]; DWORD len=32768;
            BOOL ok=QueryFullProcessImageNameW(p,0,path,&len); CloseHandle(p);
            if(!ok) continue;
            const wchar_t* name=wcsrchr(path,L'\\'); name=name?name+1:path;
            if(_wcsicmp(name,process.c_str())!=0) continue;
            Com<ISimpleAudioVolume> v;
            if(FAILED(control->QueryInterface(__uuidof(ISimpleAudioVolume),(void**)v.out()))) { result.failed++; continue; }
            GUID context={0x45cdb2e8,0xf215,0x4220,{0x90,0x1b,0x4a,0x72,0x81,0x77,0x91,0x10}};
            float old=0; HRESULT h=v->GetMasterVolume(&old);
            if(SUCCEEDED(h) && fabs(old-level)>0.0001f) h=v->SetMasterVolume(level,&context);
            float actual=0;
            if(FAILED(h) || FAILED(v->GetMasterVolume(&actual)) || fabs(actual-level)>0.001f) result.failed++;
            else result.count++;
        }
    }
    return result;
}

struct Ramp { unsigned version; float rgb[3072]; unsigned enabled; };
static_assert(sizeof(Ramp)==12296,"NVAPI ABI");
using Query=void* (__cdecl*)(unsigned);
using Init=int (__cdecl*)();
using SetRamp=int (__cdecl*)(unsigned,Ramp*);
using DisplayId=int (__cdecl*)(const char*,unsigned*);
using Luid=int (__cdecl*)(unsigned,unsigned,GUID*);
using Error=int (__cdecl*)(int,char*);
struct Display { std::wstring name,label; };
struct DvcInfo {unsigned version=20|(1u<<16);int current=0,minimum=0,maximum=0,neutral=0;};
struct HueInfo {unsigned version=12|(1u<<16),current=0,neutral=0;};
static_assert(sizeof(DvcInfo)==20 && sizeof(HueInfo)==12,"Color ABI");
using DisplayHandle=int (__cdecl*)(const char*,void**);
using DvcCall=int (__cdecl*)(void*,unsigned,DvcInfo*);
using GetHue=int (__cdecl*)(void*,unsigned,HueInfo*);
using SetHue=int (__cdecl*)(void*,unsigned,unsigned);
class Nvidia {
    HMODULE module=nullptr; SetRamp set=nullptr; DisplayId id=nullptr; Luid luid=nullptr; Error error=nullptr;
    DisplayHandle handle=nullptr;DvcCall getDvc=nullptr,setDvc=nullptr;GetHue getHue=nullptr;SetHue setHue=nullptr;
    void status(int s) {
        if(s==0) return;
        char b[64]={}; if(error) error(s,b);
        std::string a(b); throw i18n::text(L"NVIDIA 오류 ")+std::to_wstring(s)+(i18n::languageCode()==L"en"&&!a.empty()?L": "+std::wstring(a.begin(),a.end()):std::wstring());
    }
    struct Registry {
        HKEY key=nullptr;
        ~Registry(){if(key) RegCloseKey(key);}
    };
    struct Value { bool exists; DWORD number; };
    Value read(HKEY key,const std::wstring& name) {
        DWORD v=100,t=0,size=4;
        LONG r=RegQueryValueExW(key,name.c_str(),nullptr,&t,(BYTE*)&v,&size);
        if(r==ERROR_FILE_NOT_FOUND) return {false,100};
        if(r!=ERROR_SUCCESS || t!=REG_DWORD || size!=4) throw std::wstring(i18n::text(L"NVIDIA 컬러 설정 형식이 예상과 달라 중단했습니다."));
        return {true,v};
    }
    Ramp ramp(const float (&c)[3][3]) {
        Ramp r={}; r.version=sizeof(Ramp)|(1u<<16); r.enabled=1;
        for(int i=0;i<1024;i++) for(int k=0;k<3;k++) {
            float contrast=(c[1][k]-100.f)/100.f;
            float x=(float)i/1023.f-0.5f;
            x=contrast<=0 ? (contrast+1)*x : x/(1-contrast);
            x=std::clamp((c[0][k]-100.f)/100.f+x+0.5f,0.f,1.f);
            r.rgb[3*i+k]=(float)std::pow((double)x,100.0/c[2][k]);
        }
        return r;
    }
public:
    void initialize() {
        wchar_t system[MAX_PATH]; GetSystemDirectoryW(system,MAX_PATH);
        module=LoadLibraryW((std::wstring(system)+L"\\nvapi64.dll").c_str());
        if(!module) throw std::wstring(i18n::text(L"NVIDIA 64비트 드라이버를 찾지 못했습니다."));
        FARPROC address=GetProcAddress(module,"nvapi_QueryInterface");
        Query q=nullptr; static_assert(sizeof(q)==sizeof(address),"function pointer size");
        memcpy(&q,&address,sizeof(q));
        if(!q) throw std::wstring(i18n::text(L"NVAPI를 사용할 수 없습니다."));
        Init init=(Init)q(0x0150E828); set=(SetRamp)q(0x7082A053); id=(DisplayId)q(0xAE457190);
        luid=(Luid)q(0xD4A859F2); error=(Error)q(0x6C2D048C);
        if(!init||!set||!id||!luid) throw std::wstring(i18n::text(L"현재 드라이버는 필요한 NVIDIA 기능을 제공하지 않습니다."));
        status(init());
        handle=(DisplayHandle)q(0x35C29134);getDvc=(DvcCall)q(0x0E45002D);setDvc=(DvcCall)q(0x4A82C2B1);
        getHue=(GetHue)q(0x95B64341);setHue=(SetHue)q(0xF5A0F22C);
    }
    void apply(const std::wstring& display, const settings::Profile& profile) {
        if(!set) throw std::wstring(i18n::text(L"NVIDIA 초기화에 실패했습니다."));
        std::string ascii(display.begin(),display.end()); unsigned target=0; status(id(ascii.c_str(),&target));
        GUID guid={}; status(luid(target,1,&guid));
        unsigned parts[4]; memcpy(parts,&guid,16); unsigned keyId=parts[1]^0xf0000000;
        if(!keyId) throw std::wstring(i18n::text(L"NVIDIA 디스플레이 설정을 찾지 못했습니다."));
        std::wstring path=L"Software\\NVIDIA Corporation\\Global\\NVTweak\\Devices\\"+std::to_wstring(keyId)+L"-0\\Color";
        Registry reg;
        if(RegOpenKeyExW(HKEY_CURRENT_USER,path.c_str(),0,KEY_QUERY_VALUE|KEY_SET_VALUE,&reg.key)!=ERROR_SUCCESS)
            throw std::wstring(i18n::text(L"NVIDIA 컬러 설정을 찾지 못했습니다. NVIDIA 앱에서 감마를 한 번 변경 후 1로 되돌리고 다시 시도하세요."));
        Value original[10]; float colors[3][3];
        for(int i=0;i<9;i++) {
            original[i]=read(reg.key,std::to_wstring(3538946+i));
            DWORD v=original[i].number;
            if((i<6 && v>199) || (i>=6 && (v<10||v>1000))) throw std::wstring(i18n::text(L"NVIDIA 컬러 값이 예상 범위를 벗어났습니다."));
            colors[i/3][i%3]=(float)v;
        }
        original[9]=read(reg.key,L"NvCplGammaSet");
        Ramp before=ramp(colors);
        void* displayHandle=nullptr;DvcInfo oldDvc,newDvc;HueInfo oldHue;
        if(profile.extended){
            if(!handle||!getDvc||!setDvc||!getHue||!setHue)
                throw std::wstring(i18n::text(L"이 드라이버에서 전체 컬러 제어를 사용할 수 없습니다. 모드 설정에서 추가 컬러 적용을 끄면 감마만 사용할 수 있습니다."));
            status(handle(ascii.c_str(),&displayHandle));status(getDvc(displayHandle,0,&oldDvc));status(getHue(displayHandle,0,&oldHue));
            if(oldDvc.minimum>=oldDvc.maximum||oldDvc.neutral<oldDvc.minimum||oldDvc.neutral>oldDvc.maximum||oldDvc.current<oldDvc.minimum||oldDvc.current>oldDvc.maximum||oldHue.current>360)
                throw std::wstring(i18n::text(L"드라이버의 컬러 범위를 확인하지 못했습니다. 컬러는 변경하지 않았습니다."));
            // 50% maps to the driver's neutral, not an assumed numerical midpoint.
            double fraction=profile.vibrance/50.0;
            newDvc.current=(int)std::lround(profile.vibrance<=50?
                oldDvc.minimum+(oldDvc.neutral-oldDvc.minimum)*fraction:
                oldDvc.neutral+(oldDvc.maximum-oldDvc.neutral)*(fraction-1.0));
        }
        for(int k=0;k<3;k++){
            colors[2][k]=(float)(profile.extended?settings::channelGamma(profile,k):profile.gamma);
            if(profile.extended){colors[0][k]=(float)profile.brightness[k];colors[1][k]=(float)profile.contrast[k];}
        }
        Ramp after=ramp(colors);
        bool rampAttempted=false,dvcAttempted=false,hueAttempted=false,registryAttempted=false;
        try{
            rampAttempted=true;status(set(target,&after));
            if(profile.extended){
                dvcAttempted=true;status(setDvc(displayHandle,0,&newDvc));
                hueAttempted=true;status(setHue(displayHandle,0,profile.hue));
                DvcInfo actualDvc;HueInfo actualHue;
                status(getDvc(displayHandle,0,&actualDvc));status(getHue(displayHandle,0,&actualHue));
                if(actualDvc.current!=newDvc.current||actualHue.current%360!=profile.hue)
                    throw std::wstring(i18n::text(L"드라이버가 선명도/색조 요청값을 적용하지 않았습니다."));
            }
            registryAttempted=true;
            for(int i=profile.extended?0:6;i<10;i++){
                std::wstring name=i==9?L"NvCplGammaSet":std::to_wstring(3538946+i);
                DWORD value=i==9?1:(DWORD)colors[i/3][i%3];
                if(RegSetValueExW(reg.key,name.c_str(),0,REG_DWORD,(BYTE*)&value,4)!=ERROR_SUCCESS)
                    throw std::wstring(i18n::text(L"컬러 설정 저장에 실패했습니다."));
                auto actual=read(reg.key,name);
                if(!actual.exists||actual.number!=value)throw std::wstring(i18n::text(L"컬러 저장값 확인에 실패했습니다."));
            }
        }catch(const std::wstring& reason){
            bool restored=true;
            if(hueAttempted&&setHue(displayHandle,0,oldHue.current)!=0)restored=false;
            if(dvcAttempted&&setDvc(displayHandle,0,&oldDvc)!=0)restored=false;
            if(rampAttempted&&set(target,&before)!=0)restored=false;
            if(registryAttempted)for(int i=profile.extended?0:6;i<10;i++){
                std::wstring name=i==9?L"NvCplGammaSet":std::to_wstring(3538946+i);
                LONG r=original[i].exists?RegSetValueExW(reg.key,name.c_str(),0,REG_DWORD,(BYTE*)&original[i].number,4):RegDeleteValueW(reg.key,name.c_str());
                if(r!=ERROR_SUCCESS&&r!=ERROR_FILE_NOT_FOUND)restored=false;
            }
            throw reason+(restored?i18n::text(L" 변경 전 값으로 복원 요청했습니다."):i18n::text(L" 복원에도 실패했습니다. NVIDIA 앱에서 컬러를 확인하세요."));
        }
    }
};

Nvidia nv;
settings::Profiles profiles=settings::defaults();
settings::Profile appliedProfile{100,100};
int appliedProfileIndex=0;
HWND editor=nullptr;
bool autoGame=false;
unsigned hotkeyVk=VK_SUBTRACT,hotkeyMods=0;
std::wstring hotkeyName(){
    std::wstring s;if(hotkeyMods&MOD_CONTROL)s+=L"Ctrl + ";if(hotkeyMods&MOD_ALT)s+=L"Alt + ";
    if(hotkeyMods&MOD_SHIFT)s+=L"Shift + ";
    if(hotkeyMods&MOD_WIN)s+=L"Win + ";
    if(hotkeyVk>=VK_NUMPAD0&&hotkeyVk<=VK_NUMPAD9)return s+i18n::text(L"숫자패드 ")+std::to_wstring(hotkeyVk-VK_NUMPAD0);
    if(hotkeyVk==VK_SUBTRACT)return s+i18n::text(L"숫자패드 −");
    if(hotkeyVk==VK_ADD)return s+i18n::text(L"숫자패드 +");
    if(hotkeyVk==VK_MULTIPLY)return s+i18n::text(L"숫자패드 *");
    if(hotkeyVk==VK_DIVIDE)return s+i18n::text(L"숫자패드 /");
    if(hotkeyVk==VK_DECIMAL)return s+i18n::text(L"숫자패드 .");
    // F13~F24 (often mouse side buttons or macro keys) have no name on most keyboards: "F13" rather than "키 124".
    if(hotkeyVk>=VK_F1&&hotkeyVk<=VK_F24)return s+L"F"+std::to_wstring(hotkeyVk-VK_F1+1);
    if(hotkeyVk==VK_PAUSE)return s+L"Pause"; // Windows gives this key no name ("키 19")
    wchar_t name[80]={};LONG scan=(LONG)MapVirtualKeyW(hotkeyVk,MAPVK_VK_TO_VSC)<<16;
    if(hotkeyVk==VK_LEFT||hotkeyVk==VK_RIGHT||hotkeyVk==VK_UP||hotkeyVk==VK_DOWN||hotkeyVk==VK_DELETE||hotkeyVk==VK_INSERT||hotkeyVk==VK_HOME||hotkeyVk==VK_END||hotkeyVk==VK_PRIOR||hotkeyVk==VK_NEXT||hotkeyVk==VK_DIVIDE)scan|=1<<24;
    if(GetKeyNameTextW(scan,name,80))return s+name;
    return s+i18n::text(L"키 ")+std::to_wstring(hotkeyVk);
}
int gameId(const wchar_t* name){return _wcsicmp(name,L"TslGame.exe")==0?2:_wcsicmp(name,L"EscapeFromTarkov.exe")==0?3:0;}
int detectedGame(){
    DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
    bool pubg=false,tarkov=false;int foreground=0;
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE)return 1;
    PROCESSENTRY32W entry={};entry.dwSize=sizeof(entry);
    if(Process32FirstW(snapshot,&entry))do{int game=gameId(entry.szExeFile);
        pubg=pubg||game==2;tarkov=tarkov||game==3;if(entry.th32ProcessID==pid)foreground=game;
    }while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot);return settings::chooseGame(foreground,pubg,tarkov);
}
std::wstring profileText(const settings::Profile& p){return i18n::text(L"감마 ")+settings::gammaText(p.gamma)+i18n::text(L" / 볼륨 ")+std::to_wstring(p.volume)+L"%";}
std::vector<Display> displays;
HWND win,monitorBox,browserBox,modeLabel,gammaLabel,audioLabel,hotkeyLabel;
HFONT font,smallFont,titleFont,cardFont,gearFont,boldFont,iconFont,iconBig,headFont,navFont,navBold,sectionFont,tileFont,footFont,footSmall,smallBold,captionFont;
HBRUSH backgroundBrush,panelBrush,sidebarBrush,rowBrush;
HICON icons[3]={},logoIcon=nullptr,appSmallIcon=nullptr,appLargeIcon=nullptr;
// BlueAsh palette (2.21, the chosen preview): deep navy canvas lit by soft blue light, navy panels with a thin blue
// edge, rows and tiles one step lighter (rowFill), vivid cobalt; shared with the companion theme.
COLORREF bg=RGB(7,12,27),panel=RGB(13,22,44),ink=RGB(247,249,255),muted=RGB(158,174,204),mint=RGB(225,233,250),blue=RGB(47,108,255),tint=RGB(22,46,104),line=RGB(33,53,96),well=RGB(21,34,66),danger=RGB(255,100,110),rowFill=RGB(17,29,58),sideFill=RGB(8,14,31);
bool active=false,fps=false,gammaTouched=false,ready=false,trayPresent=false,hotkeyEnabled=true,hotkeyRegistered=false,startHidden=false,closeToTray=true;
int dashLabelRight=414; // dashboard: where the 디스플레이 / 브라우저 볼륨 labels end (sized to the longest translation)
bool languageRestart=false,languageToSettings=true;HWND languageBox=nullptr; // languageToSettings: reopen on 설정 (its drop-down) or on the dashboard (top-right pill)
int mainPage=0,selectedGame=0;bool pubgJoinOn=true,pubgMapOn=true;
// Set by the update check (update_ui.h): a newer version exists; the sidebar and tray show it. balloonUpdateAt = when the "new version" notice went up.
bool updateReady=false;ULONGLONG balloonUpdateAt=0;std::wstring updateVersion;
UINT taskbarCreated=0;
constexpr UINT trayMessage=WM_APP+1;
std::wstring activeDisplay,activeBrowser,lastProblem;
// A non-error note for the dashboard status line (e.g. after the display or browser changed, the mode has to be picked again).
std::wstring statusNotice;
const wchar_t* browsers[]={L"brave.exe",L"chrome.exe",L"msedge.exe",L"firefox.exe"};
const wchar_t* configPath=L"Software\\ModeSwitch";
const wchar_t* runPath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
std::wstring executable() {wchar_t p[32768];DWORD n=GetModuleFileNameW(nullptr,p,32768);return std::wstring(p,n);}
std::wstring regString(const wchar_t* path,const wchar_t* name) {
    wchar_t value[32768]={};DWORD size=sizeof(value);
    if(RegGetValueW(HKEY_CURRENT_USER,path,name,RRF_RT_REG_SZ,nullptr,value,&size)!=ERROR_SUCCESS)return L"";
    return value;
}
bool saveString(const wchar_t* path,const wchar_t* name,const std::wstring& value) {
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,path,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return false;
    LONG r=RegSetValueExW(key,name,0,REG_SZ,(BYTE*)value.c_str(),(DWORD)((value.size()+1)*sizeof(wchar_t)));
    RegCloseKey(key);return r==ERROR_SUCCESS;
}
std::wstring startupCommand(){return L"\""+executable()+L"\" --tray";}
bool startupOn(){return regString(runPath,L"ModeSwitch")==startupCommand();}
void setText(HWND h,const std::wstring& s){
    int n=GetWindowTextLengthW(h);std::wstring old(n+1,L'\0');GetWindowTextW(h,&old[0],n+1);old.resize(n);
    if(old!=s){SetWindowTextW(h,i18n::text(s).c_str());InvalidateRect(h,nullptr,TRUE);}
}
void updateOnShow();
void showWindow(){if(IsIconic(win))ShowWindow(win,SW_RESTORE);else if(!IsWindowVisible(win))ShowWindow(win,SW_SHOW);SetForegroundWindow(editor?editor:win);updateOnShow();}
void trayUpdate(bool add=false) {
    NOTIFYICONDATAW data={};data.cbSize=sizeof(data);data.hWnd=win;data.uID=1;
    data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=trayMessage;
    data.hIcon=icons[active?(fps?2:1):0];
    std::wstring tip=L"BlueAsh · "+(active?std::wstring(fps?i18n::text(L"게임 | "):i18n::text(L"일반 | "))+profileText(appliedProfile):i18n::text(L"대기 | 단축키로 게임 모드 전환"));
    if(!lastProblem.empty())tip=i18n::text(L"BlueAsh · 오류: 창을 열어 확인하세요");
    wcsncpy(data.szTip,tip.c_str(),127);
    BOOL ok=Shell_NotifyIconW(add?NIM_ADD:NIM_MODIFY,&data);
    if(add)trayPresent=ok!=FALSE;
    else if(!ok)trayPresent=false;
    SendMessageW(win,WM_SETICON,ICON_SMALL,(LPARAM)(appSmallIcon?appSmallIcon:icons[0]));
    SendMessageW(win,WM_SETICON,ICON_BIG,(LPARAM)(appLargeIcon?appLargeIcon:icons[0]));
}
void problem(const std::wstring& text){lastProblem=text;setText(gammaLabel,text);trayUpdate();if(win){RECT note={252,636,1120,684};InvalidateRect(win,&note,FALSE);}}
void refreshStatus(); // after featureOn below
bool saveSettings(){
    int d=(int)SendMessageW(monitorBox,CB_GETCURSEL,0,0),b=(int)SendMessageW(browserBox,CB_GETCURSEL,0,0);
    bool ok=true;
    if(d>=0 && d<(int)displays.size())ok=saveString(configPath,L"Display",displays[d].name)&&ok;
    if(b>=0&&b<4)ok=saveString(configPath,L"Browser",browsers[b])&&ok;
    ok=saveString(configPath,L"Hotkey",hotkeyEnabled?L"1":L"0")&&ok;
    if(!ok)problem(i18n::text(L"설정을 저장하지 못했습니다. 다음 실행에 유지되지 않을 수 있습니다."));
    return ok;
}
void audioUpdate() {
    if(!active)return;
    try {
        auto r=volume(activeBrowser,appliedProfile.volume/100.f);
        std::wstring s=i18n::text(L"브라우저 볼륨 ")+std::to_wstring(appliedProfile.volume)+L"%  ·  ";
        s+=r.count?i18n::text(L"재생 중인 탭 ")+std::to_wstring(r.count)+i18n::text(L"개에 적용"):i18n::text(L"재생이 시작되면 적용");
        if(r.failed)s+=i18n::text(L" (일부 실패)");
        setText(audioLabel,s);
    } catch(const std::wstring& e){setText(audioLabel,e);}
}
std::wstring footerHint(); // below registerShortcut
bool changeMode(bool targetFps,int pinnedProfile=-1) {
    if(!ready){problem(i18n::text(L"NVIDIA 드라이버 준비 실패. 프로그램을 다시 실행해 주세요."));return false;}
    int d=(int)SendMessageW(monitorBox,CB_GETCURSEL,0,0),b=(int)SendMessageW(browserBox,CB_GETCURSEL,0,0);
    if(d<0||d>=(int)displays.size()||b<0||b>=4){problem(i18n::text(L"디스플레이와 브라우저를 선택해 주세요."));return false;}
    // Never change the mode until the native gamma operation commits successfully.
    const std::wstring display=displays[d].name;
    int profileIndex=targetFps?(pinnedProfile>=1&&pinnedProfile<=3?pinnedProfile:(autoGame?detectedGame():1)):0;
    auto effective=profiles[profileIndex];
    if(!effective.extended&&active&&appliedProfile.extended){
        // A gamma-only target must not leave the previous game's vibrance/hue behind.
        auto gamma=effective.gamma;auto vol=effective.volume;
        effective=profiles[0];effective.extended=true;settings::allGamma(effective,gamma);effective.volume=vol;
    }
    try{nv.apply(display,effective);}catch(const std::wstring& e){problem(e);return false;}
    appliedProfile=effective;appliedProfileIndex=profileIndex;
    active=true;fps=targetFps;gammaTouched=true;activeDisplay=display;activeBrowser=browsers[b];lastProblem.clear();statusNotice.clear();
    EnableWindow(monitorBox,!fps);EnableWindow(browserBox,!fps);
    setText(modeLabel,std::wstring(i18n::text(L"지금: "))+(fps?i18n::text(L"게임 모드"):i18n::text(L"일반 모드"))+(fps&&profileIndex!=1?std::wstring(L" · ")+settings::profileName(profileIndex)+i18n::text(L" 프로필"):L""));
    setText(gammaLabel,footerHint());
    for(int id:{1,2,61,62}){InvalidateRect(GetDlgItem(win,id),nullptr,TRUE);}
    audioUpdate();trayUpdate();saveSettings();refreshStatus();return true;
}
// The dashboard footer's second line: which key switches modes (the dashboard no longer has its own shortcut row).
std::wstring footerHint(){return hotkeyRegistered?std::wstring(i18n::text(L"모드 전환 단축키 · "))+hotkeyName():std::wstring(i18n::text(L"모드 전환 단축키를 등록하지 못했습니다 · 프로필 > 단축키에서 다른 키를 고르세요"));}
void registerShortcut() {
    if(hotkeyRegistered)UnregisterHotKey(win,1);
    hotkeyRegistered=false;
    if(hotkeyEnabled)hotkeyRegistered=RegisterHotKey(win,1,MOD_NOREPEAT|hotkeyMods,hotkeyVk)!=FALSE;
    // hotkeyLabel is the 컨트롤 센터 shortcut row (button 34); its text is the right-hand value.
    setText(hotkeyLabel,hotkeyRegistered?hotkeyName():i18n::text(L"등록 실패 · 눌러서 다른 키 선택"));
    if(lastProblem.empty())setText(gammaLabel,footerHint());
}
void updateStartupButton(){setText(GetDlgItem(win,20),startupOn()?i18n::text(L"Windows 로그인 시 자동 시작     ON"):i18n::text(L"Windows 로그인 시 자동 시작     OFF"));}
void toggleStartup(){
    bool ok=false;
    if(startupOn()){
        HKEY key=nullptr;
        if(RegOpenKeyExW(HKEY_CURRENT_USER,runPath,0,KEY_SET_VALUE,&key)==ERROR_SUCCESS){LONG r=RegDeleteValueW(key,L"ModeSwitch");ok=r==ERROR_SUCCESS||r==ERROR_FILE_NOT_FOUND;RegCloseKey(key);}
    }else{
        auto command=startupCommand();
        if(command.size()>259){problem(i18n::text(L"프로그램 경로가 너무 깁니다. 짧은 폴더로 옮긴 후 다시 켜 주세요."));return;}
        ok=saveString(runPath,L"ModeSwitch",command);
    }
    updateStartupButton();if(!ok)problem(i18n::text(L"자동 실행 설정을 변경하지 못했습니다."));
}
void hideToTray(){
    if(!trayPresent)trayUpdate(true);
    if(trayPresent)ShowWindow(win,SW_HIDE);
    else problem(i18n::text(L"트레이 아이콘을 만들지 못해 창을 유지합니다. 잠시 후 다시 시도하세요."));
}
void quit(){
    // Only a deliberate Quit exits; ordinary close/minimize never restores gamma.
    if(active&&gammaTouched&&!changeMode(false)){showWindow();return;}
    if(active){
        try{auto a=volume(activeBrowser,profiles[0].volume/100.f);if(a.failed){problem(i18n::text(L"볼륨 복귀에 일부 실패했습니다. 일반 모드를 다시 눌러 확인하세요."));showWindow();return;}}
        catch(const std::wstring& e){problem(e);showWindow();return;}
    }
    DestroyWindow(win);
}
bool featureOn(int id);
// One name (and one-line description) per feature, shared by the game page rows and the tray menu.
const wchar_t* featureTitle(int id){
    switch(id){case 33:return i18n::text(L"레이드 시작 알림");case 37:return i18n::text(L"아이템 시세");case 39:return i18n::text(L"인게임 지도");case 46:return i18n::text(L"은신처 진행도");case 45:return i18n::text(L"관련 사이트");
    case 47:return i18n::text(L"비행기 탑승 알림");case 48:return i18n::text(L"지도 표시");case 20:return i18n::text(L"Windows 시작 시 자동 실행");case 21:return i18n::text(L"창을 닫으면 트레이로");case 34:return i18n::text(L"단축키");case 198:return i18n::text(L"전체창모드 자동 맞춤");case 217:return i18n::text(L"익명 사용 통계 보내기 · 임의 번호·버전·문제 종류만 (사용자 수·오류 파악용)");case 242:return i18n::text(L"진단 정보 같이 보내기");}return L"";
}
const wchar_t* featureNote(int id){
    switch(id){case 33:return i18n::text(L"배치 카운트다운 1초 전에 소리로 알려 줍니다");case 37:return i18n::text(L"아이템을 조사하면 플리마켓 시세·필요한 퀘스트·은신처 재료를 띄웁니다");
    case 39:return i18n::text(L"게임 중 지도 키로 지도를 열어 탈출구·퀘스트 위치를 봅니다");case 46:return i18n::text(L"지은 시설 레벨을 넣으면 시세 창에 남은 재료만 보여 줍니다");case 45:return i18n::text(L"기본 브라우저로 열립니다");
    case 47:return i18n::text(L"출발 카운트다운이 끝나 비행기에 타면 소리로 알려 줍니다");case 48:return i18n::text(L"M 지도 위에 비밀의 방·고정 차·고정 보트 표시 (지도에서 L로 켜고 끄기)");}return nullptr;
}
void darkControl(HWND c,const wchar_t* cls); // drop-downs drawn in the app's own style (after glyph below)
HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0){
    if(!_wcsicmp(cls,L"COMBOBOX"))style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS; // items drawn in the theme (drawComboItem)
    HWND c=CreateWindowExW(0,cls,i18n::text(text),WS_CHILD|WS_VISIBLE|style,x,y,w,h,win,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);darkControl(c,cls);return c;
}
// Keep the material assets for a later replacement; the current theme uses a solid black brush.
std::map<std::pair<int,int>,HBRUSH> carbonBrushes;
HBRUSH carbonBrush(int width=940,int height=720){
    auto key=std::make_pair(width,height);auto cached=carbonBrushes.find(key);if(cached!=carbonBrushes.end())return cached->second;
    return carbonBrushes[key]=CreateSolidBrush(bg);
}
HBRUSH carbonFor(HWND h){RECT r={};if(!GetClientRect(h,&r)||r.right<1||r.bottom<1)return backgroundBrush;return carbonBrush(r.right,r.bottom);}
LRESULT staticBackground(HDC dc,HWND child,HBRUSH brush=nullptr){
    POINT origin={};MapWindowPoints(child,GetParent(child),&origin,1);SetBrushOrgEx(dc,-origin.x,-origin.y,nullptr);
    HBRUSH fill=brush?brush:panelBrush;SetBkMode(dc,fill==panelBrush?OPAQUE:TRANSPARENT);if(fill==panelBrush)SetBkColor(dc,panel);return (LRESULT)fill;
}
void roundBox(HDC dc,RECT r,COLORREF fill,COLORREF edge,int radius,HBRUSH texture=nullptr){
    if(!texture&&painting::box(dc,r,fill,edge,radius)){return;}
    HBRUSH b=texture?texture:CreateSolidBrush(fill);HPEN p=CreatePen(PS_SOLID,1,edge);
    auto oldb=SelectObject(dc,b);auto oldp=SelectObject(dc,p);RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
    SelectObject(dc,oldb);SelectObject(dc,oldp);if(!texture)DeleteObject(b);DeleteObject(p);
}
// The window's backdrop: navy, a soft blue light top right and a fainter one low on the left, the sidebar and its edge.
// Painted once and kept; the window paint starts from it, and buttons and pages sitting on the bare canvas copy their
// own piece of it (a flat fill showed as squares over the light).
HBITMAP backdropBitmap=nullptr;int backdropW=0,backdropH=0;
void paintBackdrop(HDC dc,int w,int h){
    RECT all={0,0,w,h};FillRect(dc,&all,backgroundBrush);
    painting::glow(dc,{560,-430,1520,330},RGB(38,92,255),72);painting::glow(dc,{200,400,1020,1140},RGB(30,72,215),34);
    RECT side={0,0,220,h};FillRect(dc,&side,sidebarBrush);painting::glow(dc,{-200,-180,250,250},RGB(40,90,255),24);
    HBRUSH edge=CreateSolidBrush(line);RECT divider={220,0,221,h};FillRect(dc,&divider,edge);DeleteObject(edge);
}
// Copies the backdrop under r (coordinates in dc) for a child whose (0,0) is at origin in the main window.
bool copyBackdrop(HDC dc,RECT r,POINT origin){
    RECT c={};if(!win||!GetClientRect(win,&c)||c.right<1||c.bottom<1)return false;
    if(!backdropBitmap||backdropW!=c.right||backdropH!=c.bottom){if(backdropBitmap){DeleteObject(backdropBitmap);}HDC screen=GetDC(win);backdropBitmap=CreateCompatibleBitmap(screen,c.right,c.bottom);HDC m=CreateCompatibleDC(screen);ReleaseDC(win,screen);
        if(!backdropBitmap){DeleteDC(m);return false;}auto o=SelectObject(m,backdropBitmap);paintBackdrop(m,c.right,c.bottom);SelectObject(m,o);DeleteDC(m);backdropW=c.right;backdropH=c.bottom;}
    HDC m=CreateCompatibleDC(dc);auto o=SelectObject(m,backdropBitmap);BOOL ok=BitBlt(dc,r.left,r.top,r.right-r.left,r.bottom-r.top,m,origin.x+r.left,origin.y+r.top,SRCCOPY);SelectObject(m,o);DeleteDC(m);return ok!=FALSE;
}
LRESULT eraseCarbon(HWND h,HDC target){
    // Painted off-screen and copied once (the fill, then the rounded panel on top, used to show in two steps).
    RECT r={};if(!GetClientRect(h,&r))return 0;HDC dc=CreateCompatibleDC(target);HBITMAP bmp=CreateCompatibleBitmap(target,std::max(1L,r.right),std::max(1L,r.bottom));auto oldBmp=SelectObject(dc,bmp);
    if((GetWindowLongW(h,GWL_STYLE)&WS_CHILD)&&GetParent(h)==win){POINT o={};MapWindowPoints(h,win,&o,1);if(!copyBackdrop(dc,r,o)){SetBrushOrgEx(dc,-o.x,-o.y,nullptr);FillRect(dc,&r,carbonFor(win));}}
    else{SetBrushOrgEx(dc,0,0,nullptr);FillRect(dc,&r,carbonFor(h));}
    if(r.right>24&&r.bottom>24)roundBox(dc,{12,12,r.right-12,r.bottom-12},panel,line,20);
    BitBlt(target,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,oldBmp);DeleteObject(bmp);DeleteDC(dc);
    return 1;
}
void textAt(HDC dc,const std::wstring& text,RECT r,HFONT f,COLORREF color,UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE){
    auto old=SelectObject(dc,f);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,i18n::text(text).c_str(),-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);
}
// The dashboard headline: white, with the words after the last space in a sky-to-cobalt gradient (all of it for
// languages written without spaces). Drawn with plain GDI text straight on the paint DC, so Windows' GDI scaling renders it
// at the screen's resolution like every other text (an off-screen DIB was stretched and looked broken at 150 %): the white
// part through one clip, the gradient part as thin vertical slices, each clipped and drawn in its own colour.
void headline(HDC dc,const std::wstring& text,RECT r,HFONT f){
    if(r.right-r.left<4||text.empty()){return;}auto of=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);
    size_t cut=text.find_last_of(L' ');SIZE full={},head={};GetTextExtentPoint32W(dc,text.c_str(),(int)text.size(),&full);if(cut!=std::wstring::npos)GetTextExtentPoint32W(dc,text.c_str(),(int)cut+1,&head);
    int x0=r.left+head.cx,x1=std::max(x0+1,(int)std::min(r.right,r.left+full.cx));const UINT flags=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS;
    auto slice=[&](int left,int right,COLORREF c){int saved=SaveDC(dc);IntersectClipRect(dc,left,r.top,right,r.bottom);SetTextColor(dc,c);RECT t=r;DrawTextW(dc,text.c_str(),-1,&t,flags);RestoreDC(dc,saved);};
    if(x0>r.left)slice(r.left,x0,ink);
    for(int x=x0;x<r.right;x+=3){double t=std::clamp((x-x0)/(double)(x1-x0),0.0,1.0);slice(x,x+3,RGB((int)(96+(52-96)*t),(int)(214+(116-214)*t),255));}
    SelectObject(dc,of);
}
// Tarkov feature switches (33 raid alert, 37 price, 39 map; defined after the feature headers). Their gear buttons are 133/137/139.
bool featureOn(int id);
// 모드 설정 page tabs (90 컬러 & 사운드 / 91 단축키): which one is open.
int modeTab=0;
// One icon from Segoe Fluent Icons (Windows 11) or Segoe MDL2 Assets (Windows 10), centred in r.
void glyph(HDC dc,wchar_t g,RECT r,HFONT f,COLORREF c){wchar_t s[2]={g,0};textAt(dc,s,r,f,c,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
// Sidebar icons per menu id: home, sliders, gamepad, bell, download, power.
wchar_t navGlyph(int id){return id==40?0xE80F:id==32?0xE9E9:id==41?0xE7FC:id==44?0xEA8F:id==38?0xE896:id==31?0xE7E8:id==50?0xE713:0;}
void footerTooltips(){
 INITCOMMONCONTROLSEX init={sizeof(init),ICC_WIN95_CLASSES};if(!InitCommonControlsEx(&init))return;
 HWND tip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,L"",WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,win,nullptr,GetModuleHandleW(nullptr),nullptr);if(!tip)return;
 if(HMODULE ux=LoadLibraryExW(L"uxtheme.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){using ThemeFn=HRESULT(WINAPI*)(HWND,LPCWSTR,LPCWSTR);FARPROC address=GetProcAddress(ux,"SetWindowTheme");ThemeFn theme=nullptr;memcpy(&theme,&address,sizeof(theme));if(theme)theme(tip,L"",L"");FreeLibrary(ux);}
 SendMessageW(tip,WM_SETFONT,(WPARAM)smallFont,FALSE);SendMessageW(tip,TTM_SETTIPBKCOLOR,panel,0);SendMessageW(tip,TTM_SETTIPTEXTCOLOR,ink,0);SendMessageW(tip,TTM_SETMAXTIPWIDTH,0,320);SendMessageW(tip,TTM_SETDELAYTIME,TTDT_INITIAL,400);
 // This app keeps the existing common-controls version; its tooltip accepts the V2 structure on both v5 and v6.
 for(int id:{81,50,74,31}){TOOLINFOW tool={};tool.cbSize=TTTOOLINFOW_V2_SIZE;tool.uFlags=TTF_IDISHWND|TTF_SUBCLASS;tool.hwnd=win;tool.uId=(UINT_PTR)GetDlgItem(win,id);tool.lpszText=(LPWSTR)i18n::text(id==81?L"커피 한 잔 후원":id==50?L"설정":id==74?L"버그 제보":L"완전 종료");SendMessageW(tip,TTM_ADDTOOLW,0,(LPARAM)&tool);}
}
// Drop-downs keep the system list but draw their closed box themselves: a rounded dark field, the choice, a chevron
// (the themed control paints a light frame that no colour message reaches). The open list uses WM_CTLCOLORLISTBOX.
// Parent surface is explicit: dashboard rows and settings switches sit inside panels, profile/game cards sit on the canvas.
HBRUSH buttonSurface(HWND child){
 HWND parent=GetParent(child);if(parent!=win)return panelBrush;POINT p={};MapWindowPoints(child,win,&p,1);
 if(p.x<220)return sidebarBrush;
 int cid=GetDlgCtrlID(child);if(cid>=82&&cid<=84)return backgroundBrush; // window controls sit on the bare background on every page
 int id=GetDlgCtrlID(child);if(mainPage==0&&(id==63||id==64||id==65||id==66||id==69))return rowBrush; // game rows sit on their own cards
 if(mainPage==9||(mainPage==0&&(id==271||id==272||id==273||id==274||id==34||id==63||id==64||id==65||id==66||id==69||id==75||id==76||id==67||id==68||id==70||id==10||id==11||id==61||id==62))||(id>=95&&id<=97))return panelBrush;
 return backgroundBrush;
}
HWND hoverControl=nullptr;WNDPROC buttonBase=nullptr;
LRESULT CALLBACK buttonProc(HWND h,UINT m,WPARAM w,LPARAM l){
 // No erase: WM_DRAWITEM paints the whole button off-screen (drawButton). Filling here first showed as a flicker on every
 // click, hover and switch.
 if(m==WM_ERASEBKGND){return 1;}
 if(m==WM_SETCURSOR&&GetParent(h)==win){int id=GetDlgCtrlID(h);if(id==81||id==82||id==83||id==84||id==50||id==74||id==31||id==75||id==76){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}
 if(m==WM_MOUSEMOVE&&IsWindowEnabled(h)&&hoverControl!=h){HWND previous=hoverControl;hoverControl=h;if(previous)InvalidateRect(previous,nullptr,FALSE);TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,h,0};TrackMouseEvent(&track);InvalidateRect(h,nullptr,FALSE);}
 if(m==WM_MOUSELEAVE&&hoverControl==h){hoverControl=nullptr;InvalidateRect(h,nullptr,FALSE);}
 if(m==WM_NCDESTROY&&hoverControl==h)hoverControl=nullptr;
 return CallWindowProcW(buttonBase,h,m,w,l);
}
// Drop-down lists unroll like the language list (lang_menu.h): when a list opens, its window region grows from the top
// to its full height over 180 ms (ease-out), so it rolls down from the box instead of appearing at once.
WNDPROC listBase=nullptr;std::map<HWND,ULONGLONG> unrolling;
void unrollStep(HWND h){
    auto it=unrolling.find(h);if(it==unrolling.end())return;RECT wr;GetWindowRect(h,&wr);int w=wr.right-wr.left,hh=wr.bottom-wr.top;
    double t=std::min(1.0,(GetTickCount64()-it->second)/180.0),k=1-(1-t)*(1-t)*(1-t);
    if(t>=1){unrolling.erase(it);KillTimer(h,0xB1A);SetWindowRgn(h,nullptr,TRUE);return;}
    SetWindowRgn(h,CreateRectRgn(0,0,w,std::max(2,(int)(hh*k))),TRUE);
}
LRESULT CALLBACK listProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_WINDOWPOSCHANGING&&(((WINDOWPOS*)l)->flags&SWP_SHOWWINDOW)){unrolling[h]=GetTickCount64();SetWindowRgn(h,CreateRectRgn(0,0,1,2),FALSE);SetTimer(h,0xB1A,10,nullptr);}
    if(m==WM_TIMER&&w==0xB1A){unrollStep(h);return 0;}
    if((m==WM_WINDOWPOSCHANGED&&(((WINDOWPOS*)l)->flags&SWP_HIDEWINDOW))||m==WM_NCDESTROY){if(unrolling.erase(h)){KillTimer(h,0xB1A);SetWindowRgn(h,nullptr,FALSE);}}
    LRESULT x=CallWindowProcW(listBase,h,m,w,l);
    // The list's frame in the border colour instead of the light system line.
    if(m==WM_NCPAINT){RECT wr;GetWindowRect(h,&wr);OffsetRect(&wr,-wr.left,-wr.top);HDC dc=GetWindowDC(h);HBRUSH b=CreateSolidBrush(line);FrameRect(dc,&wr,b);DeleteObject(b);ReleaseDC(h,dc);}
    return x;
}
// Drop-down items (combo boxes are created owner-drawn, see control/addSetting): navy rows, the highlighted one in a
// cobalt tint, text in the UI font. The closed box itself is painted by comboProc.
void drawComboItem(DRAWITEMSTRUCT* ds){
    HDC dc=ds->hDC;RECT r=ds->rcItem;if(ds->itemState&ODS_COMBOBOXEDIT){InvalidateRect(ds->hwndItem,nullptr,FALSE);return;}
    FillRect(dc,&r,panelBrush);if((int)ds->itemID<0)return;bool on=(ds->itemState&ODS_SELECTED)!=0;
    if(on){RECT b=r;InflateRect(&b,-4,-2);roundBox(dc,b,tint,tint,10);}
    int n=(int)SendMessageW(ds->hwndItem,CB_GETLBTEXTLEN,ds->itemID,0);std::wstring s;if(n>0){s.resize(n+1);SendMessageW(ds->hwndItem,CB_GETLBTEXT,ds->itemID,(LPARAM)&s[0]);s.resize(n);}
    RECT t=r;t.left+=14;t.right-=10;textAt(dc,s,t,font,on?ink:RGB(206,216,236),DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
}
WNDPROC comboBase=nullptr;
LRESULT CALLBACK comboProc(HWND h,UINT m,WPARAM w,LPARAM l){
    if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
        FillRect(dc,&r,buttonSurface(h));roundBox(dc,r,well,GetFocus()==h?blue:line,12);
        int i=(int)SendMessageW(h,CB_GETCURSEL,0,0);std::wstring s;if(i>=0){int n=(int)SendMessageW(h,CB_GETLBTEXTLEN,i,0);if(n>0){s.resize(n+1);SendMessageW(h,CB_GETLBTEXT,i,(LPARAM)&s[0]);s.resize(n);}}
        RECT t=r;t.left+=12;t.right-=38;textAt(dc,s,t,font,IsWindowEnabled(h)?ink:muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);glyph(dc,0xE70D,{r.right-36,r.top,r.right-10,r.bottom},iconFont,muted);
        EndPaint(h,&ps);return 0;}
    if(m==WM_ERASEBKGND)return 1;
    LRESULT x=CallWindowProcW(comboBase,h,m,w,l);
    if(m==WM_SETFOCUS||m==WM_KILLFOCUS||m==CB_SETCURSEL||m==WM_MOUSELEAVE)InvalidateRect(h,nullptr,FALSE);
    return x;
}
// Text boxes keep their system frame but paint it in the border colour (cobalt while typing) instead of light grey
// (themed edits draw that frame inside the client area, so it is painted over after every paint).
WNDPROC editBase=nullptr;
LRESULT CALLBACK editProc(HWND h,UINT m,WPARAM w,LPARAM l){
    LRESULT x=CallWindowProcW(editBase,h,m,w,l);
    RECT wr;if((m==WM_NCPAINT||m==WM_PAINT||m==WM_SETFOCUS||m==WM_KILLFOCUS||m==WM_MOUSEMOVE)&&GetWindowRect(h,&wr)){HDC dc=GetWindowDC(h);RECT r=wr;OffsetRect(&r,-r.left,-r.top);
        HBRUSH b=CreateSolidBrush(GetFocus()==h?blue:line),in=CreateSolidBrush(panel);FrameRect(dc,&r,b);InflateRect(&r,-1,-1);FrameRect(dc,&r,in);DeleteObject(b);DeleteObject(in);ReleaseDC(h,dc);}
    return x;
}
void darkControl(HWND c,const wchar_t* cls){if(!c)return;
    if(!_wcsicmp(cls,L"BUTTON")&&(GetWindowLongW(c,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW){WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)buttonProc);if(!buttonBase)buttonBase=old;}
    if(!_wcsicmp(cls,L"COMBOBOX")){WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)comboProc);if(!comboBase)comboBase=old;
        COMBOBOXINFO ci={};ci.cbSize=sizeof(ci);if(GetComboBoxInfo(c,&ci)&&ci.hwndList){WNDPROC base=(WNDPROC)SetWindowLongPtrW(ci.hwndList,GWLP_WNDPROC,(LONG_PTR)listProc);if(!listBase)listBase=base;
            // No system drop shadow: it is drawn at the list's full size at once and would show ahead of the unrolling rows
            // (the class is this process's own copy of ComboLBox, so only BlueAsh's lists change).
            ULONG_PTR cs=GetClassLongPtrW(ci.hwndList,GCL_STYLE);if(cs&CS_DROPSHADOW)SetClassLongPtrW(ci.hwndList,GCL_STYLE,cs&~(ULONG_PTR)CS_DROPSHADOW);}
        SendMessageW(c,CB_SETITEMHEIGHT,0,32);SendMessageW(c,CB_SETITEMHEIGHT,(WPARAM)-1,28);}
    else if(!_wcsicmp(cls,L"EDIT")){ // the edit class drops WS_BORDER from its style but keeps the frame: hook them all
        WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)editProc);if(!editBase)editBase=old;SendMessageW(c,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,MAKELONG(6,6));
        // Multi-line boxes: Windows' dark scroll bar instead of the light one.
        if(GetWindowLongW(c,GWL_STYLE)&WS_VSCROLL)if(HMODULE ux=LoadLibraryExW(L"uxtheme.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){using ThemeFn=HRESULT(WINAPI*)(HWND,LPCWSTR,LPCWSTR);FARPROC address=GetProcAddress(ux,"SetWindowTheme");ThemeFn theme=nullptr;memcpy(&theme,&address,sizeof(theme));if(theme)theme(c,L"DarkMode_Explorer",nullptr);FreeLibrary(ux);}
        SetWindowPos(c,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);}
}
// Feature rows: a title and, for game tools, a one-line note under it; toggles end in an ON/OFF pill, rows that open something in "value ›".
namespace gameart{Gdiplus::Bitmap* get(int game);extern float anchor[2];void release();} // game_art.h
HWND helpCard=nullptr;int helpFor=0; // help_ui.h: the open "?" card and its button
HWND languageMenu=nullptr; // lang_menu.h: the open language list under button 82
void drawRow(HDC dc,RECT r,int id,const std::wstring& value,int pill){
    bool dashboardRow=id==63||id==64;const wchar_t* title=dashboardRow?i18n::text(id==63?L"타르코프":L"배그"):featureTitle(id);
    const wchar_t* note=dashboardRow?featureTitle(id==63?33:47):featureNote(id);RECT t={r.left+18,r.top,r.right-(pill>=0?84:96),r.bottom};
    int mid=(r.top+r.bottom)/2;
    if(dashboardRow){RECT tile={r.left+10,mid-28,r.left+66,mid+28};int game=id==63?0:1;
        if(!painting::art(dc,tile,gameart::get(game),16,gameart::anchor[game])){roundBox(dc,tile,tint,tint,14);RECT pad=tile;InflateRect(&pad,-12,-12);if(!painting::gamepad(dc,pad,blue,2.f))glyph(dc,0xE7FC,tile,tileFont,blue);}t.left=r.left+82;}
    if(id==34)t.left=r.left+4;
    if(note&&r.bottom-r.top>=50){RECT a=t;a.top=dashboardRow?mid-26:a.top+9;a.bottom=a.top+24;textAt(dc,title,a,dashboardRow?sectionFont:boldFont,ink);a.top=a.bottom+2;a.bottom=dashboardRow?a.top+26:r.bottom-8;textAt(dc,note,a,smallFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
    else textAt(dc,title,t,smallFont,ink);
    int cy=(r.top+r.bottom)/2;
    // Toggles are a switch (cobalt track, knob right when on); rows that open something end in "value  >".
    if(pill>=0){RECT track={r.right-70,cy-12,r.right-20,cy+12};roundBox(dc,track,pill?blue:well,pill?blue:line,24);
        int k=pill?track.right-21:track.left+3;if(painting::circle(dc,{k,cy-9,k+18,cy+9},pill?ink:muted,CLR_NONE)){return;}HBRUSH kb=CreateSolidBrush(pill?RGB(255,255,255):muted);HPEN kp=CreatePen(PS_SOLID,1,pill?RGB(255,255,255):muted);auto ob=SelectObject(dc,kb);auto op=SelectObject(dc,kp);
        Ellipse(dc,k,cy-9,k+18,cy+9);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(kb);DeleteObject(kp);}
    else{RECT v={r.left+18,r.top,r.right-44,r.bottom};textAt(dc,value,v,smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);glyph(dc,0xE76C,{r.right-42,r.top,r.right-16,r.bottom},iconFont,muted);}
}
void drawButtonTo(DRAWITEMSTRUCT* ds);
// Every owner-drawn button is drawn into a memory bitmap and copied in one go: drawn straight on screen, its background,
// glow and text appeared one after another, which flickered.
void drawButton(DRAWITEMSTRUCT* ds){RECT r=ds->rcItem;int w=r.right-r.left,hh=r.bottom-r.top;if(w<=0||hh<=0){return;}
    if(ds->CtlType==ODT_COMBOBOX&&(ds->itemState&ODS_COMBOBOXEDIT)){InvalidateRect(ds->hwndItem,nullptr,FALSE);return;} // the closed box: comboProc paints it
    HDC mem=CreateCompatibleDC(ds->hDC);HBITMAP bmp=CreateCompatibleBitmap(ds->hDC,r.right,r.bottom);if(!mem||!bmp){if(bmp)DeleteObject(bmp);if(mem)DeleteDC(mem);drawButtonTo(ds);return;}
    auto old=SelectObject(mem,bmp);DRAWITEMSTRUCT copy=*ds;copy.hDC=mem;drawButtonTo(&copy);BitBlt(ds->hDC,r.left,r.top,w,hh,mem,r.left,r.top,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);}
void drawButtonTo(DRAWITEMSTRUCT* ds){
    if(ds->CtlType==ODT_COMBOBOX){drawComboItem(ds);return;}
    HDC dc=ds->hDC;RECT r=ds->rcItem;POINT origin={};MapWindowPoints(ds->hwndItem,GetParent(ds->hwndItem),&origin,1);
    int id=(int)ds->CtlID;bool home=GetParent(ds->hwndItem)==win;
    // Buttons on the bare canvas copy the backdrop under them: the window's own, and the profile tabs (90/91, children of a page).
    {HBRUSH surface=buttonSurface(ds->hwndItem);POINT inWin={};MapWindowPoints(ds->hwndItem,win,&inWin,1);bool canvas=(home&&(surface==backgroundBrush||surface==sidebarBrush))||(!home&&(id==90||id==91));
        if(!(canvas&&copyBackdrop(dc,r,inWin))){SetBrushOrgEx(dc,-origin.x,-origin.y,nullptr);FillRect(dc,&r,surface);}}
    bool gameCard=home&&(id==42||id==43),card=id==1||id==2||gameCard;
    int navPage=id==40?0:id==41?1:id==32?2:id==38?4:id==44?5:id==50?9:id==74?10:id==81?12:-1;bool tab=!home&&(id==90||id==91);
    // GWLP_USERDATA 1 marks a page's main action (저장): filled cobalt.
    bool nav=(home&&navPage>=0)||tab,primary=!home&&GetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA)==1;
    bool selected=tab?modeTab==id-90:gameCard?(selectedGame==(id==43?1:0)):(nav?(mainPage==navPage||(navPage==1&&((mainPage>=6&&mainPage<=8)||mainPage==11))):(card&&(active?(id==2)==fps:id==1)));COLORREF accent=blue;
    bool pressed=(ds->itemState&ODS_SELECTED)!=0,hover=hoverControl==ds->hwndItem;
    if(home&&(id==75||id==76)){ // "?" help: a thin ring that lights up on hover
        bool on=hover||pressed||(helpCard&&helpFor==id);int cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;RECT ring={cx-10,cy-10,cx+10,cy+10};COLORREF c=on?ink:muted;
        // The Windows icon font's circled question mark (as in the preview): thin ring, light "?".
        auto mark=[&](HDC d,RECT q,COLORREF k){(void)q;glyph(d,0xE9CE,{ring.left-4,ring.top-4,ring.right+4,ring.bottom+4},footFont,k);};
        if(on)painting::iconGlow(dc,r,RGB(120,160,255),3,1.6,[&](HDC d,RECT q){mark(d,q,RGB(255,255,255));});
        mark(dc,r,c);return;
    }
    if(home&&(id==82||id==83||id==84)){
        // Window controls drawn by BlueAsh (no Windows title bar): language pill "KO ⌄", minimize, close.
        bool on=hover||pressed||(id==82&&languageMenu);COLORREF k=on?RGB(242,246,255):RGB(170,178,196);
        // The language pill: white, cobalt while the mouse is on it (or its list is open).
        if(id==82)k=on?RGB(110,150,255):RGB(236,240,250);
        if(id==82){roundBox(dc,r,on?well:panel,line,r.bottom-r.top);std::wstring code=i18n::languageCode();for(auto& c:code)c=towupper(c);
            textAt(dc,code,{r.left+14,r.top,r.right-26,r.bottom},boldFont,k,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen p(painting::color(k),1.6f);p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);
                float x=(float)r.right-20,y=(float)(r.top+r.bottom)/2;g.DrawLine(&p,x-4,y-2,x,y+2);g.DrawLine(&p,x,y+2,x+4,y-2);}return;}
        if(on)roundBox(dc,r,id==84?RGB(196,43,52):well,id==84?RGB(196,43,52):line,12);
        if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen p(painting::color(id==84&&on?RGB(255,255,255):k),1.5f);p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);
            float cx=(float)(r.left+r.right)/2,cy=(float)(r.top+r.bottom)/2;if(id==83)g.DrawLine(&p,cx-6,cy,cx+6,cy);else{g.DrawLine(&p,cx-5,cy-5,cx+5,cy+5);g.DrawLine(&p,cx+5,cy-5,cx-5,cy+5);}}
        return;
    }
    if(home&&(id==81||id==50||id==74||id==31)){
        // Footer icons: one 20 px symbol centred in the 48 px button, all the same quiet grey. Hover makes it glow white
        // (완전 종료 glows red, the one action that quits); the open page keeps a quiet tile.
        COLORREF c=RGB(150,159,178),lit=id==31?RGB(255,128,138):id==81?RGB(255,214,168):RGB(242,246,255);
        bool on=hover||pressed;
        int cx=(r.left+r.right)/2,cy=(r.top+r.bottom)/2;RECT symbol={cx-12,cy-12,cx+12,cy+12};
        // The bug and the power sign drew larger than the gear and the cup: drawn a little smaller to match them.
        RECT small={symbol.left+1,symbol.top+1,symbol.right-1,symbol.bottom-1};
        auto icon=[&](HDC d,RECT q,COLORREF k){if(id==74)painting::bug(d,small,k);else if(id==81)painting::coffee(d,symbol,k);else glyph(d,navGlyph(id),q,id==31?footSmall:footFont,k);};
        // The icon itself gives off light: its own shape, blurred, in its glow colour under the crisp icon.
        // The open page (설정, 문제·건의, 후원) keeps the same glow as hover, no tile or focus box around it.
        if(on||selected)painting::iconGlow(dc,r,id==31?RGB(255,70,90):id==81?RGB(255,150,70):RGB(150,185,255),4,id==31||id==81?2.2:1.9,[&](HDC d,RECT q){icon(d,q,RGB(255,255,255));});
        icon(dc,r,on||selected?lit:c);return;
    }
    if(!home&&(GetWindowLongW(ds->hwndItem,GWL_STYLE)&BS_TYPEMASK)==BS_OWNERDRAW&&(GetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA)==2||GetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA)==3)){
        // A page switch (settings_ui.h pageSwitch): the dashboard's track and knob, then its label.
        bool on=GetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA)==3,hover=hoverControl==ds->hwndItem;int cy=(r.top+r.bottom)/2;RECT track={r.left+2,cy-12,r.left+52,cy+12};
        roundBox(dc,track,on?blue:well,on?blue:hover?RGB(64,104,196):line,24);int k=on?track.right-21:track.left+3;painting::circle(dc,{k,cy-9,k+18,cy+9},on?RGB(255,255,255):muted,CLR_NONE);
        wchar_t label[256];GetWindowTextW(ds->hwndItem,label,256);textAt(dc,label,{r.left+66,r.top,r.right-4,r.bottom},smallFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if((ds->itemState&ODS_FOCUS)&&!(ds->itemState&ODS_NOFOCUSRECT)){RECT focus=track;InflateRect(&focus,3,3);painting::outline(dc,focus,blue,28);}
        return;
    }
    if(home&&id>=271&&id<=274){
        // 현재 상태 tiles (dashboard footer): mode (with its shortcut) · screen (gamma) · browser volume · game alerts on.
        // The mode tile is lit like the chosen card; each tile opens its page.
        bool lit=id==271,on=hover||pressed;const auto& now=active?appliedProfile:profiles[0];
        roundBox(dc,r,lit?RGB(18,38,90):on?well:rowFill,lit?RGB(50,96,210):on?RGB(64,104,196):line,16);
        int mid=(r.top+r.bottom)/2;RECT icon={r.left+10,mid-18,r.left+48,mid+18};COLORREF ic=lit?RGB(96,150,255):RGB(214,222,240);
        std::wstring title,sub;
        if(id==271){title=active&&fps?i18n::text(L"게임 모드"):i18n::text(L"일반 모드");sub=hotkeyRegistered?std::wstring(i18n::text(L"단축키"))+L" · "+hotkeyName():std::wstring(i18n::text(L"등록 실패 · 눌러서 다른 키 선택"));
            if(!(active&&fps?painting::gamepad(dc,{icon.left+2,icon.top+6,icon.right-2,icon.bottom-6},ic,2.2f):painting::headphones(dc,{icon.left+5,icon.top+3,icon.right-5,icon.bottom-3},ic,2.2f)))glyph(dc,0xE7F6,icon,footFont,ic);}
        else if(id==272){title=i18n::text(L"화면 설정");sub=i18n::text(L"감마 ")+settings::gammaText(now.gamma);glyph(dc,0xE7F4,icon,footFont,ic);}
        else if(id==273){title=i18n::text(L"브라우저 볼륨");sub=std::to_wstring(now.volume)+L"%";glyph(dc,now.volume==0?0xE74F:0xE995,icon,footFont,ic);}
        else{int n=(featureOn(33)?1:0)+(pubgJoinOn?1:0);title=i18n::text(L"게임 알림");sub=i18n::text(L"켜진 알림 · ")+std::to_wstring(n);glyph(dc,0xEA8F,icon,footFont,ic);}
        int textRight=r.right-30;
        textAt(dc,title,{r.left+54,mid-21,textRight,mid},smallBold,lit?RGB(118,160,255):ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        textAt(dc,sub,{r.left+54,mid+1,textRight,mid+21},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        if(lit){int cx=r.right-20;painting::glow(dc,{cx-12,mid-12,cx+12,mid+12},RGB(70,130,255),150);painting::circle(dc,{cx-4,mid-4,cx+4,mid+4},RGB(110,160,255),CLR_NONE);}
        else glyph(dc,0xE76C,{r.right-30,r.top,r.right-8,r.bottom},iconFont,on?ink:muted);
        if((ds->itemState&ODS_FOCUS)&&!(ds->itemState&ODS_NOFOCUSRECT)){RECT focus=r;InflateRect(&focus,-3,-3);painting::outline(dc,focus,blue,14);}
        return;
    }
    COLORREF fill=primary||(tab&&selected)?blue:selected?tint:panel; // the open tab is filled cobalt like 저장
    if(hover&&!selected)fill=primary?RGB(67,119,255):well;
    if(pressed)fill=primary?RGB(25,76,215):tint;
    // Sidebar items are plain text with an icon; the open one gets a cobalt tint and a bar on its left edge.
    // Cards keep a cobalt outline when chosen.
    bool side=home&&origin.x<220;
    bool quiet=home&&(id==63||id==64||id==65||id==66||id==69||id==70||id==34||id==20||id==21);
    if(side){if(selected||pressed||hover)roundBox(dc,r,selected||pressed?tint:well,selected||pressed?tint:well,14);if(selected){HBRUSH b=CreateSolidBrush(blue);RECT bar={r.left,r.top+11,r.left+3,r.bottom-11};FillRect(dc,&bar,b);DeleteObject(b);}}
    else if(quiet){if((hover||pressed)&&id!=70)roundBox(dc,r,fill,fill,12);} // 세부 설정 is a text link: no box
    else if(card){
        // The game-mode card shows a game cover already on this PC (Steam's library cache), blurred and faded in from the
        // right; an exe icon (the fallback picture) is not used there.
        Gdiplus::Bitmap* cover=nullptr;float anchor=.5f;if(id==2)for(int game:{1,0})if(auto* b=gameart::get(game))if(gameart::anchor[game]<.5f){cover=b;anchor=gameart::anchor[game];break;}
        if(!painting::modeCard(dc,r,selected,hover||pressed,cover,anchor,panel,line,RGB(70,130,255)))roundBox(dc,r,fill,selected?accent:line,24);}
    else roundBox(dc,r,fill,selected||hover?accent:primary?blue:line,card?22:14);
    if(card&&!gameCard){
        // Mode cards (2.21 preview): a large icon, the name, one line on what the mode is for and its gamma / volume. The
        // chosen card says 사용 중 in a pill with a lit knob; the other shows an empty ring.
        RECT c=r;InflateRect(&c,-6,-6);RECT art={c.left+24,c.top+28,c.left+100,c.bottom-28};COLORREF ic=selected?RGB(80,140,255):RGB(214,222,240);
        bool drawn=id==1?painting::headphones(dc,{art.left+6,art.top,art.right-6,art.bottom},ic,3.2f):painting::gamepad(dc,{art.left-2,art.top+4,art.right+2,art.bottom-4},ic,3.2f);
        if(!drawn)glyph(dc,id==1?0xE7F6:0xE7FC,art,iconBig,ic);
        int right=c.right-20;
        if(selected){std::wstring use=i18n::text(L"사용 중");SIZE ts={};auto of=SelectObject(dc,smallFont);GetTextExtentPoint32W(dc,use.c_str(),(int)use.size(),&ts);SelectObject(dc,of);
            RECT pill={c.right-18-(ts.cx+52),c.top+16,c.right-18,c.top+46};roundBox(dc,pill,RGB(24,54,130),RGB(56,100,214),30);
            textAt(dc,use,{pill.left+14,pill.top,pill.right-34,pill.bottom},smallFont,RGB(176,204,255));int kx=pill.right-19,ky=(pill.top+pill.bottom)/2;
            painting::circle(dc,{kx-9,ky-9,kx+9,ky+9},RGB(255,255,255),CLR_NONE);painting::circle(dc,{kx-4,ky-4,kx+4,ky+4},blue,CLR_NONE);right=pill.left-8;}
        else{int cx=c.right-30,cy=c.top+30;painting::circle(dc,{cx-11,cy-11,cx+11,cy+11},CLR_NONE,hover?ink:muted,1.8f);right=cx-20;}
        int x=c.left+124;COLORREF note=selected?RGB(196,210,238):muted;
        textAt(dc,id==1?i18n::text(L"일반 모드"):i18n::text(L"게임 모드"),{x,c.top+20,right,c.top+52},cardFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        textAt(dc,id==1?i18n::text(L"일상적인 사용에 최적화된 설정입니다."):i18n::text(L"게임 플레이에 최적화된 설정입니다."),{x,c.top+56,c.right-18,c.top+80},smallFont,note,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        textAt(dc,profileText(active&&id==2&&fps?appliedProfile:profiles[id-1]),{x,c.top+86,c.right-18,c.top+110},smallFont,note,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    }else if(card){
        // Mode cards lead with an icon (headphones = everyday, gamepad = FPS); every card ends in a radio mark.
        int left=r.left+23;if(gameCard){int game=id==43?1:0;RECT tile={r.left+20,r.top+20,r.left+104,r.bottom-20};if(painting::art(dc,tile,gameart::get(game),20,gameart::anchor[game]))left=r.left+124;}
        if(!gameCard){RECT art={r.left+22,r.top+30,r.left+90,r.bottom-30};COLORREF ic=selected?blue:muted;// Headphones and controller drawn with the same pen and colour so the two cards read as a pair.
            bool drawn=id==1?painting::headphones(dc,{art.left+6,art.top+2,art.right-6,art.bottom-2},ic,3.f):painting::gamepad(dc,{art.left-4,art.top+4,art.right+4,art.bottom-4},ic,3.f);
            if(!drawn){glyph(dc,id==1?0xE7F6:0xE7FC,art,iconBig,ic);}left=r.left+110;}
        RECT t={left,r.top+(gameCard?24:32),r.right-50,r.top+(gameCard?57:64)};
        textAt(dc,gameCard?(id==42?i18n::text(L"타르코프"):i18n::text(L"배그")):(id==1?i18n::text(L"일반 모드"):i18n::text(L"게임 모드")),t,cardFont,ink);
        t.top=r.top+(gameCard?65:76);t.bottom=r.bottom-18;
        textAt(dc,gameCard?(id==42?i18n::text(L"레이드 알림 · 시세 · 지도 · 은신처"):i18n::text(L"탑승 알림 · 지도 표시")):profileText(active&&id==2&&fps?appliedProfile:profiles[id-1]),t,gameCard?smallFont:font,muted,DT_LEFT|DT_WORDBREAK);
        int cx=r.right-30,cy=r.top+30;painting::circle(dc,{cx-11,cy-11,cx+11,cy+11},selected?blue:CLR_NONE,selected?blue:muted,2.f);
        if(selected)painting::circle(dc,{cx-6,cy-6,cx+6,cy+6},ink,CLR_NONE);
    }else if(id==133||id==137||id==139||id==147||id==66||id==69){
        glyph(dc,0xE713,r,iconFont,ink);
    }else if(id==63||id==64){drawRow(dc,r,id,L"",id==63?(featureOn(33)?1:0):(pubgJoinOn?1:0));
    }else if(home&&id==65){glyph(dc,0xEA8F,{r.left+10,r.top,r.left+58,r.bottom},tileFont,blue);textAt(dc,i18n::text(L"자리 비움 알림"),{r.left+78,r.top,r.right-38,r.bottom},boldFont,ink);glyph(dc,0xE76C,{r.right-32,r.top,r.right-8,r.bottom},iconFont,muted);
    }else if(id==20||id==21||id==47||id==48||id==33||id==37||id==39||id==198||id==217||id==242){
        drawRow(dc,r,id,L"",(id==20?startupOn():id==21?closeToTray:id==47?pubgJoinOn:id==48?pubgMapOn:featureOn(id))?1:0);
    }else if(home&&(id==34||id==46)){
        wchar_t value[128]={};GetWindowTextW(ds->hwndItem,value,128);drawRow(dc,r,id,value,-1);
    }else{
        // Sidebar items read left-aligned like a menu, after their icon; every other button centres its label.
        wchar_t label[256];GetWindowTextW(ds->hwndItem,label,256);RECT t=r;t.left+=16;t.right-=16;
        if(side){COLORREF c=selected?ink:id==31?muted:RGB(196,202,214);int my=(r.top+r.bottom)/2;
            // 게임 도우미 uses the same controller as the game-mode card; the other entries are icon-font glyphs.
            if(id!=41||!painting::gamepad(dc,{r.left+13,my-9,r.left+35,my+9},selected?blue:c,1.7f)){glyph(dc,navGlyph(id),{r.left+10,r.top,r.left+38,r.bottom},iconFont,selected?blue:c);}t.left=r.left+44;t.right=r.right-4;textAt(dc,label,t,selected?navBold:navFont,c,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        else if(home&&id==70){COLORREF k=hover||pressed?RGB(150,184,255):RGB(96,146,255);RECT link=r;link.right-=22;textAt(dc,label,link,smallFont,k,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);glyph(dc,0xE76C,{r.right-22,r.top,r.right-2,r.bottom},iconFont,k);} // 세부 설정: text against the "?" beside it
        else textAt(dc,label,t,smallFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        // A new version waiting: a "!" badge on 업데이트 (cobalt disc, white mark), seen without opening the page.
        if(home&&id==38&&updateReady){int cy=(r.top+r.bottom)/2;RECT disc={r.right-34,cy-10,r.right-14,cy+10};
            if(!painting::circle(dc,disc,blue,CLR_NONE)||!painting::exclaim(dc,disc,RGB(255,255,255))){textAt(dc,L"!",disc,boldFont,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
    }
    // The focus outline only while the keyboard is in use (Tab): a mouse click or bringing the window back from the taskbar
    // gives a button focus too, and the outline flashed on it.
    if((ds->itemState&ODS_FOCUS)&&!(ds->itemState&ODS_NOFOCUSRECT)&&!(home&&id==70)){RECT focus=r;InflateRect(&focus,-4,-4);painting::outline(dc,focus,blue,12);}
}
std::wstring field(HWND h,int id){wchar_t b[64]={};GetDlgItemTextW(h,id,b,64);return b;}
void reportProblem(const wchar_t* kind); // update_ui.h: anonymous problem count
#include "settings_ui.h"
#include "level_ui.h"

#include "raid_context.h"
#include "bundle.h"
#include "companion_ui.h"
#include "tarkov_screen.h"
#include "tarkov_keys.h"
#include "raid_alert.h"
#include "pubg_alert.h"
#include "pubg_board.h"
#include "pubg_map.h"
#include "price_ui.h"
#include "update_ui.h"
#include "datapack.h"
#include "phone_game.h"
#include "tools_ui.h"
#include "dashboard_ui.h"
#include "support_ui.h"
#include "feedback_ui.h"
#include "donate_ui.h"
#include "game_art.h"
#include "help_ui.h"
#include "lang_menu.h"
#include "tray_menu.h"
#include "pubg_page.h"
bool tarkovMapOn(){return regString(configPath,L"MapEnabled")!=L"0";}
bool featureOn(int id){return id==33?raidWatch.enabled:id==37?priceWatch.autoMode:id==39?tarkovMapOn():id==198?tarkovScreen.enabled():id==217?usageStats():id==242?feedbackDiag:false;}
// The dashboard's 현재 상태 tiles and status line: redrawn when what they show changes (checked every second and after
// a mode change or switch).
void refreshStatus(){
    static std::wstring last;std::wstring now=std::to_wstring(active)+std::to_wstring(fps)+L','+std::to_wstring(appliedProfile.gamma)+L','+std::to_wstring(appliedProfile.volume)+L','+std::to_wstring(profiles[0].gamma)+L','+std::to_wstring(profiles[0].volume)+L','+std::to_wstring(raidWatch.enabled)+std::to_wstring(pubgJoinOn)+std::to_wstring(hotkeyRegistered)+L','+std::to_wstring(hotkeyVk)+L','+std::to_wstring(hotkeyMods)+L','+lastProblem+L'|'+statusNotice;
    if(now==last||!win){return;}last=now;for(int id=271;id<=274;id++)if(HWND t=GetDlgItem(win,id)){InvalidateRect(t,nullptr,FALSE);} // never InvalidateRect(nullptr): that repaints every window
    RECT note={252,636,1120,684};InvalidateRect(win,&note,FALSE);
}
// Row click switches the feature; the gear opens its settings. The map switch is read live by the helper (Companion.ps1).
void toggleFeature(int id){
    if(id==33){raidWatch.enabled=!raidWatch.enabled;raidWatch.reset();if(!raidWatch.save())problem(i18n::text(L"설정을 저장하지 못했습니다."));raidWatch.status=raidWatch.enabled?i18n::text(L"출격 신호 감시 중"):i18n::text(L"알림 꺼짐");raidWatch.nextDiscovery=0;}
    else if(id==37){bool on=!priceWatch.autoMode;priceWatch.setAuto(on);if(!on&&priceWatch.running())priceWatch.stop();}
    else if(id==39){if(!saveString(configPath,L"MapEnabled",tarkovMapOn()?L"0":L"1"))problem(i18n::text(L"설정을 저장하지 못했습니다."));}
    InvalidateRect(GetDlgItem(win,id),nullptr,TRUE);refreshStatus();
}

void closePage();
// mainPage: 0 컨트롤 센터, 1 게임별 기능, 2 모드 설정 (탭: 컬러 & 사운드 / 단축키), 4 업데이트, 5 디스코드 알림,
// 6~8 타르코프 도구 설정 (레이드 시작 알림 / 아이템 시세 / 인게임 지도, opened by the gears; 닫기 returns to 게임별 기능).
// Every page shares the header box drawn by the main window: a title and one line saying what the page is for.
const wchar_t* pageTitle(){const wchar_t* t[]={i18n::text(L"게임에 집중하세요."),i18n::text(L"게임 도우미"),i18n::text(L"프로필"),L"",i18n::text(L"업데이트"),i18n::text(L"자리 비움 알림"),i18n::text(L"레이드 시작 알림"),i18n::text(L"아이템 시세"),i18n::text(L"인게임 지도"),i18n::text(L"설정"),i18n::text(L"문제·건의 보내기"),i18n::text(L"비행기 탑승 알림"),i18n::text(L"커피 한 잔 후원")};return t[mainPage];}
const wchar_t* pageNote(){
    switch(mainPage){case 0:return i18n::text(L"화면과 소리, 게임 알림을 한곳에서 관리하세요.");case 1:return i18n::text(L"게임을 고르고, 쓰고 싶은 도구를 켜세요.");
    case 2:return modeTab?i18n::text(L"일반 ↔ 게임 모드를 바꾸는 단축키를 정합니다."):i18n::text(L"모드마다 화면 색감과 브라우저 볼륨을 정합니다.");case 4:return i18n::text(L"새 버전으로 바꿔도 설정은 그대로 남습니다.");
    case 5:return i18n::text(L"게임이 시작될 때 자리에 없으면 휴대폰으로 알려 줍니다.");case 6:return i18n::text(L"타르코프 · 배치 카운트다운 1초 전에 소리로 알려 줍니다.");
    case 7:return i18n::text(L"타르코프 · 아이템을 조사하면 시세·필요한 퀘스트·은신처 재료를 띄웁니다.");case 8:return i18n::text(L"타르코프 · 지도 키로 지도를 열어 탈출구와 퀘스트 위치를 봅니다.");case 9:return i18n::text(L"언어와 프로그램 실행 방식을 설정합니다.");case 10:return i18n::text(L"버그나 바라는 점을 개발자에게 바로 보냅니다.");case 11:return i18n::text(L"배그 · 탑승 카운트다운이 끝나면 소리로 알려 줍니다.");case 12:return i18n::text(L"BlueAsh가 도움이 됐다면 커피 한 잔으로 응원해 주세요.");}return L"";
}
void selectMainPage(int page,int game=-1){
    if(page<2||page==9){closePage();}mainPage=page;if(game>=0)selectedGame=game;
    bool redraw=IsWindowVisible(win)!=FALSE;if(redraw)SendMessageW(win,WM_SETREDRAW,FALSE,0);
    for(int id:{1,2,10,11,61,62,63,64,65,66,69,70,75,76,271,272,273,274})ShowWindow(GetDlgItem(win,id),page==0?SW_SHOW:SW_HIDE);
    // Game tools stay together on the game page; the dashboard only exposes mode, devices and alerts.
    for(int id:{67,68})ShowWindow(GetDlgItem(win,id),SW_HIDE);
    for(int id:{20,21,60,72})ShowWindow(GetDlgItem(win,id),page==9?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(win,73),page==9&&!selfElevated()?SW_SHOW:SW_HIDE);
    // The old footer lines (mode, shortcut / problem), the browser status line and the shortcut row stay hidden: the
    // 현재 상태 tiles and the status note (which shows a problem in red) say the same.
    for(HWND c:{modeLabel,gammaLabel,audioLabel,hotkeyLabel})if(c)ShowWindow(c,SW_HIDE);
    for(int id:{42,43})ShowWindow(GetDlgItem(win,id),page==1?SW_SHOW:SW_HIDE);
    for(int id:{33,37,39,46,95,96,97,133,137,139})ShowWindow(GetDlgItem(win,id),page==1&&selectedGame==0?SW_SHOW:SW_HIDE);
    for(int id:{47,48,147})ShowWindow(GetDlgItem(win,id),page==1&&selectedGame==1?SW_SHOW:SW_HIDE);
    ShowWindow(GetDlgItem(win,49),(page>=6&&page<=8)||page==11?SW_SHOW:SW_HIDE);
    // The embedded pages (5, 8) close inside the helper (it only hides its form): watch for that every 100 ms so 닫기 feels instant.
    if(page==5||page==8)SetTimer(win,78,100,nullptr);else KillTimer(win,78);
    if(redraw){SendMessageW(win,WM_SETREDRAW,TRUE,0);RedrawWindow(win,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);}else InvalidateRect(win,nullptr,FALSE);
}
// Settings pages open inside the main window under the shared header: the settings procs run as a child filling
// the box right of the sidebar. Their 닫기 destroys the child, which returns to 컨트롤 센터 (게임별 기능 for 6~8).
HWND pageWindow=nullptr;bool embedSeen=false;
// 디스코드 알림 (5) and 인게임 지도 (8) are the companion's windows, re-parented into ours (they live in the helper process).
HWND embeddedWindow(int page){for(HWND c=GetWindow(win,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT))if(GetPropW(c,page==8?L"BlueAsh.Page.Map":L"BlueAsh.Page.Discord"))return c;return nullptr;}
void closePage(){if(pageWindow){HWND old=pageWindow;pageWindow=nullptr;DestroyWindow(old);}for(int p:{5,8})if(HWND d=embeddedWindow(p))ShowWindow(d,SW_HIDE);embedSeen=false;}
// Older pages were laid out for content x 24~712 while the page (and its rounded panel, eraseCarbon) is 852 wide:
// they sat to the left with a wide empty strip on the right. Stretch them across the panel (x 36 ~ width-36):
// wide controls (text, rows, boxes) take the new width, small ones (buttons, short fields) keep their size and stay
// anchored left, right or proportionally between. Pages already laid out wide (anything past x 720) are left alone.
void fitPage(HWND page){
    RECT pr;GetClientRect(page,&pr);int L=36,R=pr.right-36;if(R-L<700)return;std::vector<std::pair<HWND,RECT>> kids;
    for(HWND c=GetWindow(page,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){RECT r;GetWindowRect(c,&r);MapWindowPoints(nullptr,page,(POINT*)&r,2);if(r.right>720)return;kids.push_back({c,r});}
    double k=(R-L)/688.0;
    for(auto& [c,r]:kids){int w=r.right-r.left,h=r.bottom-r.top,nl=L+(int)((r.left-24)*k+.5),nr=L+(int)((r.right-24)*k+.5);
        if(w<240){if(r.right>=708)nl=nr-w;else if(r.left>24){int mid=(nl+nr)/2;nl=mid-w/2;}nr=nl+w;}
        SetWindowPos(c,nullptr,nl,r.top,nr-nl,h,SWP_NOZORDER|SWP_NOACTIVATE);}
}
void openPage(WNDPROC callback,const wchar_t* className,int page){
    // The same page is already open (its icon clicked again): keep it as it is (typed text, a loaded QR) instead of rebuilding it.
    if(pageWindow&&mainPage==page){wchar_t open[64]={};GetClassNameW(pageWindow,open,64);if(!wcscmp(open,className)){showWindow();return;}}
    closePage();
    WNDCLASSW wc={};wc.lpfnWndProc=callback;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=className;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=backgroundBrush;RegisterClassW(&wc);
    selectMainPage(page);showWindow();
    pageWindow=CreateWindowExW(WS_EX_CONTROLPARENT,className,L"",WS_CHILD|WS_CLIPCHILDREN,260,118,852,622,win,nullptr,wc.hInstance,nullptr);
    if(!pageWindow){selectMainPage(0);problem(i18n::text(L"설정 화면을 열지 못했습니다."));return;}
    fitPage(pageWindow);
    ShowWindow(pageWindow,SW_SHOW);
}
void openEmbeddedPage(int page){
    closePage();selectMainPage(page);showWindow();
    HWND d=embeddedWindow(page);bool running=companionProcess&&WaitForSingleObject(companionProcess,0)==WAIT_TIMEOUT;
    if(d&&running){ShowWindow(d,SW_SHOW);return;}
    openCompanionPage(((page==8?L"map@":L"discord@")+std::to_wstring((LONG_PTR)win)).c_str());
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    if(taskbarCreated&&msg==taskbarCreated){trayPresent=false;trayUpdate(true);return 0;}
    switch(msg){
    case WM_CREATE:{
        win=h;
        dashboard::registerControls();level::registerControl();
        settings::decode(regString(configPath,L"ProfilesV1"),profiles);
        settings::decodeV2(regString(configPath,L"ProfilesV2"),profiles,autoGame);
        std::wstring binding=regString(configPath,L"HotkeyBindingV1");auto comma=binding.find(L',');unsigned mods=0,vk=0;
        if(comma!=std::wstring::npos&&settings::integer(binding.substr(0,comma),mods)&&settings::integer(binding.substr(comma+1),vk)&&mods<=15&&vk>=8&&vk<=254){hotkeyMods=mods;hotkeyVk=vk;}
        raidWatch.load();pubgJoin.load();pubgMap.load();priceWatch.load();
        font=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        smallFont=CreateFontW(-14,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        titleFont=CreateFontW(-26,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        cardFont=CreateFontW(-22,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        gearFont=CreateFontW(-24,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Symbol");
        boldFont=CreateFontW(-16,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        smallBold=CreateFontW(-14,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        captionFont=CreateFontW(-13,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        sectionFont=CreateFontW(-18,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        {LOGFONTW lf={};lf.lfCharSet=DEFAULT_CHARSET;wcscpy(lf.lfFaceName,L"Segoe Fluent Icons");HDC sdc=GetDC(nullptr);bool fluent=false;
            EnumFontFamiliesExW(sdc,&lf,[](const LOGFONTW*,const TEXTMETRICW*,DWORD,LPARAM p)->int{*(bool*)p=true;return 0;},(LPARAM)&fluent,0);ReleaseDC(nullptr,sdc);
            const wchar_t* face=fluent?L"Segoe Fluent Icons":L"Segoe MDL2 Assets";
            iconFont=CreateFontW(-18,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);iconBig=CreateFontW(-48,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);tileFont=CreateFontW(-26,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);footFont=CreateFontW(-20,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);footSmall=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,face);}
        navFont=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));navBold=CreateFontW(-16,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        headFont=CreateFontW(-38,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,(i18n::fontFamily()));
        control(L"BUTTON",i18n::text(L"일반 모드"),BS_OWNERDRAW|WS_TABSTOP,246,126,434,140,1); // the card is drawn 6 px inside (room for its glow): its edges line up with the panels below
        control(L"BUTTON",i18n::text(L"게임 모드"),BS_OWNERDRAW|WS_TABSTOP,692,126,434,140,2);
        modeLabel=control(L"STATIC",i18n::text(L"지금: 일반 모드"),SS_LEFTNOWORDWRAP|SS_ENDELLIPSIS,252,714,520,20);SendMessageW(modeLabel,WM_SETFONT,(WPARAM)smallBold,TRUE);
        gammaLabel=control(L"STATIC",i18n::text(L"게임 모드 카드나 단축키를 누르면 바로 바뀝니다."),0,252,737,868,20);SendMessageW(gammaLabel,WM_SETFONT,(WPARAM)captionFont,TRUE);
        audioLabel=control(L"STATIC",L"",SS_LEFTNOWORDWRAP|SS_ENDELLIPSIS,700,720,398,24);
        // The two labels get the width the longer one needs in this language (100~190 px); the drop-downs take the rest.
        {HDC sdc=GetDC(h);auto of=SelectObject(sdc,font);int need=0;for(const wchar_t* k:{L"디스플레이",L"브라우저 볼륨"}){std::wstring t=i18n::text(k);SIZE ts={};GetTextExtentPoint32W(sdc,t.c_str(),(int)t.size(),&ts);need=std::max(need,(int)ts.cx);}SelectObject(sdc,of);ReleaseDC(h,sdc);
            dashLabelRight=306+std::clamp(need+4,100,190);}
        monitorBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,dashLabelRight+6,361,646-dashLabelRight,220,10);
        browserBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,dashLabelRight+6,491,646-dashLabelRight,200,11);
        for(HWND c:{monitorBox,browserBox})SendMessageW(c,CB_SETITEMHEIGHT,(WPARAM)-1,28); // one row with its icon and label
        std::wstring savedDisplay=regString(configPath,L"Display");int selected=0;
        DISPLAY_DEVICEW dd={};dd.cb=sizeof(dd);
        for(DWORD i=0;EnumDisplayDevicesW(nullptr,i,&dd,0);i++){
            if(!(dd.StateFlags&DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)||(dd.StateFlags&DISPLAY_DEVICE_MIRRORING_DRIVER))continue;
            DISPLAY_DEVICEW m={};m.cb=sizeof(m);EnumDisplayDevicesW(dd.DeviceName,0,&m,0);
            std::wstring device=dd.DeviceName,number=device.substr(device.find_last_not_of(L"0123456789")+1);
            std::wstring label=(number.empty()?device:i18n::text(L"모니터 ")+number)+L" · "+m.DeviceString;
            if(dd.StateFlags&DISPLAY_DEVICE_PRIMARY_DEVICE){if(savedDisplay.empty())selected=(int)displays.size();label+=i18n::text(L" (주 화면)");}
            if(savedDisplay==dd.DeviceName)selected=(int)displays.size();
            displays.push_back({dd.DeviceName,label});SendMessageW(monitorBox,CB_ADDSTRING,0,(LPARAM)label.c_str());
        }
        SendMessageW(monitorBox,CB_SETCURSEL,selected,0);
        int b=0;std::wstring savedBrowser=regString(configPath,L"Browser");
        for(auto name:{L"Brave",L"Chrome",L"Edge",L"Firefox"})SendMessageW(browserBox,CB_ADDSTRING,0,(LPARAM)name);
        for(int i=0;i<4;i++)if(savedBrowser==browsers[i])b=i;
        SendMessageW(browserBox,CB_SETCURSEL,b,0);
        control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,278,364,816,54,20);
        control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,278,430,816,54,21);
        hotkeyLabel=control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,274,642,380,34,34);
        control(L"BUTTON",i18n::text(L"대시보드"),BS_OWNERDRAW|WS_TABSTOP,12,112,196,48,40);
        control(L"BUTTON",i18n::text(L"게임 도우미"),BS_OWNERDRAW|WS_TABSTOP,12,168,196,48,41);
        // Alerts are shared by every game, so they sit in the sidebar rather than under one game.
        control(L"BUTTON",i18n::text(L"알림"),BS_OWNERDRAW|WS_TABSTOP,12,224,196,48,44);
        control(L"BUTTON",i18n::text(L"프로필"),BS_OWNERDRAW|WS_TABSTOP,12,280,196,48,32);
        control(L"BUTTON",i18n::text(L"업데이트"),BS_OWNERDRAW|WS_TABSTOP,12,336,196,48,38);
        control(L"BUTTON",i18n::text(L"타르코프"),BS_OWNERDRAW|WS_TABSTOP,246,128,434,124,42);
        control(L"BUTTON",i18n::text(L"배그"),BS_OWNERDRAW|WS_TABSTOP,692,128,434,124,43);
        // Tarkov rows: three switches with a gear (settings page), the hideout window, and the site links in one row.
        int rowY[]={282,348,414};int rowId[]={33,37,39};
        for(int i=0;i<3;i++){control(L"BUTTON",featureTitle(rowId[i]),BS_OWNERDRAW|WS_TABSTOP,252,rowY[i],806,58,rowId[i]);control(L"BUTTON",(std::wstring(featureTitle(rowId[i]))+i18n::text(L" 설정")).c_str(),BS_OWNERDRAW|WS_TABSTOP,1070,rowY[i],50,58,100+rowId[i]);}
        control(L"BUTTON",i18n::text(L"열기"),BS_OWNERDRAW|WS_TABSTOP,252,484,868,66,46);
        control(L"BUTTON",i18n::text(L"위키"),BS_OWNERDRAW|WS_TABSTOP,278,618,220,44,95);
        control(L"BUTTON",i18n::text(L"보스 스폰 확률"),BS_OWNERDRAW|WS_TABSTOP,514,618,278,44,96);
        control(L"BUTTON",i18n::text(L"플리마켓 시세"),BS_OWNERDRAW|WS_TABSTOP,808,618,286,44,97);
        control(L"BUTTON",featureTitle(47),BS_OWNERDRAW|WS_TABSTOP,252,282,806,82,47);
        control(L"BUTTON",(std::wstring(featureTitle(47))+i18n::text(L" 설정")).c_str(),BS_OWNERDRAW|WS_TABSTOP,1070,282,50,82,147);
        control(L"BUTTON",featureTitle(48),BS_OWNERDRAW|WS_TABSTOP,252,382,868,82,48);
        // Tool settings pages (6~8) lead back to 게임별 기능 from the header.
        control(L"BUTTON",i18n::text(L"‹  게임별 기능"),BS_OWNERDRAW|WS_TABSTOP,906,56,214,40,49);
        // Top right, in place of the Windows title bar: language, minimize, close.
        {std::wstring code=i18n::languageCode();for(auto& c:code)c=towupper(c);HDC sdc=GetDC(h);auto of=SelectObject(sdc,boldFont);SIZE ts={};GetTextExtentPoint32W(sdc,code.c_str(),(int)code.size(),&ts);SelectObject(sdc,of);ReleaseDC(h,sdc);
            int w=std::max(88,(int)ts.cx+56);control(L"BUTTON",i18n::text(L"언어"),BS_OWNERDRAW|WS_TABSTOP,1054-w,12,w,34,82);} // as wide as its code (ZH-CN, PT-BR), right edge fixed
        control(L"BUTTON",L"",BS_OWNERDRAW,1064,12,40,34,83);control(L"BUTTON",L"",BS_OWNERDRAW,1108,12,40,34,84);
        // Sidebar footer, always the same four: 커피 한 잔 후원 · 설정 · 버그 제보 · 완전 종료. (The coffee icon used to appear a few
        // seconds after start and shift the others, which left half-drawn icons behind.)
        control(L"BUTTON",i18n::text(L"커피 한 잔 후원"),BS_OWNERDRAW|WS_TABSTOP,10,690,48,48,81);
        control(L"BUTTON",i18n::text(L"설정"),BS_OWNERDRAW|WS_TABSTOP,62,690,48,48,50);
        control(L"BUTTON",i18n::text(L"버그 제보"),BS_OWNERDRAW|WS_TABSTOP,114,690,48,48,74);
        control(L"BUTTON",i18n::text(L"완전 종료"),BS_OWNERDRAW|WS_TABSTOP,166,690,48,48,31);
        // 설정 > 문제 해결 (support_ui.h): restart as administrator, diagnostics file.
        control(L"BUTTON",i18n::text(L"관리자 권한으로 다시 실행"),BS_OWNERDRAW|WS_TABSTOP,518,632,320,40,73);
        control(L"BUTTON",i18n::text(L"진단 파일 만들기"),BS_OWNERDRAW|WS_TABSTOP,850,632,244,40,72);
        control(L"BUTTON",i18n::text(L"레이드 시작 알림"),BS_OWNERDRAW|WS_TABSTOP,716,354,334,76,63);
        control(L"BUTTON",i18n::text(L"비행기 탑승 알림"),BS_OWNERDRAW|WS_TABSTOP,716,446,334,76,64);
        control(L"BUTTON",i18n::text(L"배그 도구"),BS_OWNERDRAW|WS_TABSTOP,1050,446,50,76,69);
        control(L"BUTTON",i18n::text(L"자리 비움 알림"),BS_OWNERDRAW|WS_TABSTOP,716,538,386,56,65);
        control(L"BUTTON",i18n::text(L"레이드 알림 설정"),BS_OWNERDRAW|WS_TABSTOP,1050,354,50,76,66);
        control(L"BUTTON",i18n::text(L"타르코프 도구"),BS_OWNERDRAW|WS_TABSTOP,714,592,188,44,67);
        control(L"BUTTON",i18n::text(L"배그 도구"),BS_OWNERDRAW|WS_TABSTOP,914,592,188,44,68);
        // 세부 설정 > ends at the panel's right edge and is as wide as its text; the "?" sits right before it (any language).
        {std::wstring more=i18n::text(L"세부 설정");HDC sdc=GetDC(h);auto of=SelectObject(sdc,smallFont);SIZE ts={};GetTextExtentPoint32W(sdc,more.c_str(),(int)more.size(),&ts);SelectObject(sdc,of);ReleaseDC(h,sdc);
            int w=std::min(200,(int)ts.cx+30),x=652-w;control(L"BUTTON",more.c_str(),BS_OWNERDRAW|WS_TABSTOP,x,296,w,34,70);
            control(L"BUTTON",i18n::text(L"화면과 소리 도움말"),BS_OWNERDRAW|WS_TABSTOP,x-38,293,40,40,75);}
        control(L"BUTTON",i18n::text(L"게임 알림 도움말"),BS_OWNERDRAW|WS_TABSTOP,1066,293,40,40,76);
        control(L"BlueAshSlider",L"",WS_TABSTOP,274,408,378,44,62);
        control(L"BlueAshSlider",L"",WS_TABSTOP,274,538,378,44,61);
        // 현재 상태 tiles: mode · screen · browser volume · game alerts (each opens its page).
        {const wchar_t* names[]={L"일반 모드",L"화면 설정",L"브라우저 볼륨",L"게임 알림"};for(int i=0;i<4;i++)control(L"BUTTON",i18n::text(names[i]),BS_OWNERDRAW|WS_TABSTOP,268+i*212,686,200,58,271+i);}
        languageBox=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,278,258,816,520,60);SendMessageW(languageBox,0x1701/*CB_SETMINVISIBLE*/,13,0);
        for(auto name:{L"한국어",L"简体中文",L"日本語",L"English",L"Русский",L"Español",L"Português (Brasil)",L"Deutsch",L"Français",L"Italiano",L"Polski",L"Türkçe",L"繁體中文"})SendMessageW(languageBox,CB_ADDSTRING,0,(LPARAM)name);
        const wchar_t* codes[]={L"ko",L"zh-CN",L"ja",L"en",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};int languageIndex=0;for(int i=0;i<13;i++)if(i18n::languageCode()==codes[i])languageIndex=i;SendMessageW(languageBox,CB_SETCURSEL,languageIndex,0);
        footerTooltips();selectMainPage(0);
        if(!regString(runPath,L"ModeSwitch").empty())saveString(runPath,L"ModeSwitch",startupCommand());
        hotkeyEnabled=true;closeToTray=regString(configPath,L"CloseToTray")!=L"0";registerShortcut();updateStartupButton();
        // Tray: ash-white symbol (not applied yet / normal), cobalt symbol (FPS). Title bar: the cobalt symbol; Alt+Tab: the tile.
        for(int i=0;i<3;i++)icons[i]=(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(i?101+i:102),IMAGE_ICON,32,32,LR_DEFAULTCOLOR);
        logoIcon=(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101),IMAGE_ICON,48,48,LR_DEFAULTCOLOR);
        appSmallIcon=(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(105),IMAGE_ICON,16,16,LR_DEFAULTCOLOR);appLargeIcon=(HICON)LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(101),IMAGE_ICON,48,48,LR_DEFAULTCOLOR);
        taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");trayUpdate(true);
        try{nv.initialize();ready=true;}catch(const std::wstring& e){problem(e);}
        BOOL dark=TRUE;DwmSetWindowAttribute(h,20,&dark,sizeof(dark));
        // Windows 11: rounded corners, a title bar in the window's own colour and a faint edge, so the frame reads as one
        // smooth piece (older Windows ignores these).
        {int round=2;COLORREF caption=bg,title=RGB(214,222,238),edge=RGB(30,46,82);DwmSetWindowAttribute(h,33,&round,sizeof(round));DwmSetWindowAttribute(h,35,&caption,sizeof(caption));DwmSetWindowAttribute(h,36,&title,sizeof(title));DwmSetWindowAttribute(h,34,&edge,sizeof(edge));}
        // Dark tray and context menus: uxtheme ordinal 135 (SetPreferredAppMode, ForceDark = 2; 1809 reads it as AllowDarkModeForApp)
        // and 136 (FlushMenuThemes), Windows 10 1809+; elsewhere menus stay light.
        if(HMODULE ux=LoadLibraryExW(L"uxtheme.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){typedef int(WINAPI*AppMode)(int);typedef void(WINAPI*Flush)();
            FARPROC a=GetProcAddress(ux,MAKEINTRESOURCEA(135)),f=GetProcAddress(ux,MAKEINTRESOURCEA(136));AppMode setMode=nullptr;Flush flush=nullptr;memcpy(&setMode,&a,sizeof(a));memcpy(&flush,&f,sizeof(f));
            if(setMode){setMode(2);}if(flush){flush();}}
        SetTimer(h,1,1000,nullptr);std::thread(datapack::fetch).detach(); // the data pack (map pictures etc.), once
        if(companionEnabled()){openCompanion(true);}
        return 0;
    }
    case WM_PAINT:{
        PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);
        HDC mem=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bitmap);
        if(!copyBackdrop(mem,r,{0,0})){FillRect(mem,&r,backgroundBrush);RECT side={0,0,220,r.bottom};FillRect(mem,&side,sidebarBrush);}
        if(!painting::drawLogo(mem,{26,36,76,86}))DrawIconEx(mem,26,36,logoIcon?logoIcon:icons[0],50,50,0,nullptr,DI_NORMAL);
        textAt(mem,L"BlueAsh",{88,36,210,86},titleFont,ink);
        textAt(mem,appVersion,{158,746,222,768},captionFont,RGB(108,118,138),DT_CENTER|DT_VCENTER|DT_SINGLELINE); // under the 완전 종료 button; a new version shows as the ! on 업데이트
        if(mainPage==0)headline(mem,pageTitle(),{252,30,904,78},headFont);else textAt(mem,pageTitle(),{252,30,904,78},headFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        textAt(mem,pageNote(),{253,84,1120,112},font,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        // The helper's pages (5, 8) are filled with the panel colour; this rounded panel frames them on the backdrop.
        if(mainPage==5||mainPage==8){roundBox(mem,mainPage==8?RECT{252,108,1120,728}:RECT{252,110,1120,748},panel,line,18);textAt(mem,i18n::text(L"불러오는 중…"),{278,134,1094,192},smallFont,muted);}
        if(mainPage==0){
            // Two panels (화면과 소리 · 게임 알림) over the 현재 상태 strip; the game rows sit on their own cards.
            COLORREF accentIcon=RGB(84,140,255);
            roundBox(mem,{252,280,674,620},panel,line,20);roundBox(mem,{698,280,1120,620},panel,line,20);roundBox(mem,{252,636,1120,756},panel,line,20);
            glyph(mem,0xE7F4,{270,296,302,330},footFont,accentIcon);{RECT q={476,0,0,0};if(HWND help=GetDlgItem(h,75)){GetWindowRect(help,&q);MapWindowPoints(nullptr,h,(POINT*)&q,2);} // the title stops where the "?" starts
                textAt(mem,i18n::text(L"화면과 소리"),{310,296,std::max(330L,q.left+4),330},sectionFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
            glyph(mem,0xEA8F,{716,296,748,330},footFont,accentIcon);textAt(mem,i18n::text(L"게임 알림"),{756,296,1060,330},sectionFont,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            HBRUSH separator=CreateSolidBrush(line);for(RECT divide:std::initializer_list<RECT>{{272,344,654,345},{272,474,654,475}})FillRect(mem,&divide,separator);DeleteObject(separator);
            glyph(mem,0xE7F4,{272,361,300,397},iconFont,accentIcon);textAt(mem,i18n::text(L"디스플레이"),{306,361,dashLabelRight,397},font,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            glyph(mem,0xE774,{272,491,300,527},iconFont,accentIcon);textAt(mem,i18n::text(L"브라우저 볼륨"),{306,491,dashLabelRight,527},font,ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
            for(RECT card:std::initializer_list<RECT>{{714,352,1104,432},{714,444,1104,524},{714,536,1104,596}})roundBox(mem,card,rowFill,line,16);
            // 현재 상태: a heartbeat, the title, then one quiet line, or the last problem in red when there is one.
            painting::pulse(mem,{270,650,298,676},accentIcon,2.f);std::wstring status=i18n::text(L"현재 상태");SIZE ts={};auto of=SelectObject(mem,sectionFont);GetTextExtentPoint32W(mem,status.c_str(),(int)status.size(),&ts);SelectObject(mem,of);
            textAt(mem,status,{306,648,306+ts.cx+4,678},sectionFont,ink);
            textAt(mem,!lastProblem.empty()?lastProblem:!statusNotice.empty()?statusNotice:std::wstring(i18n::text(L"지금 실행 중인 기능을 한눈에 확인하세요.")),{306+ts.cx+18,648,1100,678},smallFont,!lastProblem.empty()?danger:!statusNotice.empty()?RGB(120,170,255):muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }else if(mainPage==1&&selectedGame==0){
            roundBox(mem,{252,578,1120,680},panel,line,18);
            textAt(mem,featureTitle(45),{278,582,1094,612},boldFont,ink);
        }else if(mainPage==1){
            roundBox(mem,{252,504,1120,636},panel,line,18);
            textAt(mem,i18n::text(L"게임 창은 다른 창 뒤에 두세요."),{278,522,1094,554},boldFont,ink);
            textAt(mem,i18n::text(L"탑승 알림은 카운트다운을 읽습니다. 배그를 최소화하면 화면을 읽을 수 없습니다."),{278,568,1094,622},font,muted,DT_LEFT|DT_WORDBREAK);
        }else if(mainPage==9){
            roundBox(mem,{252,148,1120,318},panel,line,18);
            textAt(mem,i18n::text(L"언어"),{278,168,1094,202},boldFont,ink);
            textAt(mem,i18n::text(L"언어를 바꾸면 프로그램이 다시 시작됩니다. 기존 설정은 유지됩니다."),{278,206,1094,246},smallFont,muted,DT_LEFT|DT_WORDBREAK);
            roundBox(mem,{252,338,1120,512},panel,line,18);
            {HBRUSH separator=CreateSolidBrush(line);RECT divide={278,424,1094,425};FillRect(mem,&divide,separator);DeleteObject(separator);}
            roundBox(mem,{252,534,1120,688},panel,line,18);
            textAt(mem,i18n::text(L"문제 해결"),{278,548,1094,578},sectionFont,ink);
            bool admin=adminMismatch();textAt(mem,admin?i18n::text(L"타르코프가 관리자 권한으로 실행 중이라 지도 키와 시세 클릭을 못 읽습니다. 관리자 권한으로 다시 실행하세요."):i18n::text(L"지도나 시세가 동작하지 않으면 왼쪽 아래 벌레 아이콘으로 알려 주세요."),{278,584,1094,626},smallFont,admin?danger:muted,DT_LEFT|DT_WORDBREAK);
        }
        BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);
        SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);EndPaint(h,&ps);return 0;
    }
    case WM_ERASEBKGND:return 1;
    // Activated by the mouse or the taskbar: hide the keyboard focus outline until Tab or an arrow key is used.
    case WM_ACTIVATE:{LRESULT r=DefWindowProcW(h,msg,wp,lp);if(LOWORD(wp)!=WA_INACTIVE)SendMessageW(h,WM_CHANGEUISTATE,MAKEWPARAM(UIS_SET,UISF_HIDEFOCUS),0);return r;}
    case WM_CTLCOLORSTATIC:{
        HDC dc=(HDC)wp;SetTextColor(dc,(HWND)lp==modeLabel?ink:muted);
        bool footer=(HWND)lp==modeLabel||(HWND)lp==gammaLabel||(HWND)lp==audioLabel;SetBkColor(dc,footer?bg:panel);return staticBackground(dc,(HWND)lp,footer?backgroundBrush:panelBrush);
    }
    case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    case WM_ENTERIDLE:if(wp==MSGF_MENU){trayMenuWindow((HWND)lp);}break;
    case WM_MEASUREITEM:if(((MEASUREITEMSTRUCT*)lp)->CtlType==ODT_MENU){trayMeasure((MEASUREITEMSTRUCT*)lp);return TRUE;}break;
    case WM_DRAWITEM:if(((DRAWITEMSTRUCT*)lp)->CtlType==ODT_MENU){trayDraw((DRAWITEMSTRUCT*)lp);return TRUE;}drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_COMMAND:{
        int id=LOWORD(wp);
        if(id==60&&HIWORD(wp)==CBN_SELCHANGE){
            const wchar_t* codes[]={L"ko",L"zh-CN",L"ja",L"en",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};int choice=(int)SendMessageW(languageBox,CB_GETCURSEL,0,0);if(choice<0||choice>=13)return 0;
            std::wstring previous=i18n::languageCode();if(previous==codes[choice])return 0;
            auto resetChoice=[&](){for(int i=0;i<13;i++)if(previous==codes[i])SendMessageW(languageBox,CB_SETCURSEL,i,0);InvalidateRect(languageBox,nullptr,FALSE);};
            if(!saveString(configPath,L"Language",codes[choice])){resetChoice();problem(i18n::text(L"언어 설정을 저장하지 못했습니다."));return 0;}
            // Quit after the drop-down has finished its own notification (destroying it inside CBN_SELCHANGE could crash first).
            PostMessageW(h,WM_APP+179,0,(LPARAM)new std::wstring(previous));return 0;
        }
        if((id==10||id==11)&&HIWORD(wp)==CBN_SELCHANGE){
            // Changing target is allowed only outside FPS mode.
            if(active&&!fps){active=false;gammaTouched=false;setText(modeLabel,i18n::text(L"대상 변경 · 모드를 선택하세요"));setText(gammaLabel,i18n::text(L"선택한 대상에 적용하려면 모드 버튼을 누르세요."));setText(audioLabel,i18n::text(L"볼륨 자동 적용 대기"));statusNotice=i18n::text(L"선택한 대상에 적용하려면 모드 버튼을 누르세요.");trayUpdate();InvalidateRect(GetDlgItem(h,1),nullptr,TRUE);refreshStatus();}
            saveSettings();return 0;
        }
        if(HIWORD(wp)==BN_CLICKED){
            if(id==1||id==2){if(!changeMode(id==2))showWindow();}
            else if(id==50){selectMainPage(9);showWindow();}
            else if(id==72)writeDiagnostics();else if(id==73)restartAsAdmin();else if(id==74)openPage(feedbackProc,L"BlueAshFeedback",10);else if(id==81)openPage(donateProc,L"BlueAshDonate",12);
            else if(id==83)ShowWindow(h,SW_MINIMIZE);else if(id==84)PostMessageW(h,WM_CLOSE,0,0);
            else if(id==82)showLanguageMenu();
            else if(id==63){toggleFeature(33);InvalidateRect(GetDlgItem(win,63),nullptr,TRUE);}
            else if(id==64){pubgJoin.toggle();InvalidateRect(GetDlgItem(win,64),nullptr,TRUE);}
            else if(id==65)openEmbeddedPage(5);
            else if(id==66){raidWatch.discoverMissing();openPage(raidProc,L"ModeSwitchRaid",6);}
            else if(id==69||id==147){selectedGame=1;openPage(pubgPageProc,L"BlueAshPubg",11);}
            else if(id==75||id==76){showHelp(id);InvalidateRect(GetDlgItem(win,75),nullptr,TRUE);InvalidateRect(GetDlgItem(win,76),nullptr,TRUE);}
            else if(id==67||id==68){selectMainPage(1,id==68?1:0);}
            else if(id==70){modeTab=0;openPage(editorProc,L"ModeSwitchProfiles",2);}
            else if(id==20)toggleStartup();
            else if(id==21){closeToTray=!closeToTray;if(!saveString(configPath,L"CloseToTray",closeToTray?L"1":L"0"))problem(i18n::text(L"설정을 저장하지 못했습니다."));InvalidateRect(GetDlgItem(h,21),nullptr,TRUE);InvalidateRect(h,nullptr,FALSE);}
            else if(id==40||id==41){selectMainPage(id==41?1:0);showWindow();}
            else if(id>=271&&id<=274)SendMessageW(h,WM_COMMAND,id==271?34:id==274?41:32,0); // 현재 상태 tiles: shortcut tab, profile, profile, game tools
            else if(id==42||id==43)selectMainPage(1,id==43?1:0);
            else if(id==30)showWindow();else if(id==31){if(lp&&(HWND)lp==GetDlgItem(win,31))confirmQuit();else quit();}else if(id==32){modeTab=0;openPage(editorProc,L"ModeSwitchProfiles",2);}else if(id==33||id==37||id==39)toggleFeature(id);else if(id==133){raidWatch.discoverMissing();openPage(raidProc,L"ModeSwitchRaid",6);}else if(id==137)openPage(priceProc,L"ModeSwitchPrice",7);else if(id==139)openEmbeddedPage(8);else if(id==49)selectMainPage(1);else if(id>=95&&id<=97)openSite(id-95);else if(id==34){modeTab=1;openPage(shortcutProc,L"ModeSwitchHotkey",2);}else if(id==47){pubgJoin.toggle();InvalidateRect(GetDlgItem(win,47),nullptr,TRUE);}else if(id==48){pubgMap.toggleSetting();InvalidateRect(GetDlgItem(win,48),nullptr,TRUE);}else if(id==38)openPage(updateProc,L"ModeSwitchUpdate",4);else if(id==44)openEmbeddedPage(5);else if(id==46)openCompanionPage(L"hideout");
        }return 0;
    }
    case WM_APP+178:updateWatch.done((UpdateInfo*)lp);updateVersion=updateWatch.info.version;return 0;
    case WM_APP+179:{std::unique_ptr<std::wstring> held((std::wstring*)lp);languageToSettings=wp==0;/* wp 1: picked in the top-right language list */std::wstring previous=*held;const wchar_t* codes[]={L"ko",L"zh-CN",L"ja",L"en",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};
        auto resetChoice=[&](){for(int i=0;i<13;i++)if(previous==codes[i])SendMessageW(languageBox,CB_SETCURSEL,i,0);InvalidateRect(languageBox,nullptr,FALSE);};
        languageRestart=true;quit();if(IsWindow(h)){languageRestart=false;bool restored=saveString(configPath,L"Language",previous);if(restored)resetChoice();problem(i18n::text(restored?L"프로그램을 종료하지 못해 언어 변경을 취소했습니다.":L"이전 언어 설정을 복원하지 못했습니다. 다음 실행에서는 새로 선택한 언어가 적용됩니다."));}return 0;
        return 0;}
    case WM_APP+177:if(editor)DestroyWindow(editor);quit();return 0;
    case WM_HOTKEY:if(wp==2){if(GetTickCount64()>=priceWatch.ignoreUntil)priceWatch.toggle();return 0;}if(wp==1&&!editor){if(!changeMode(!fps))showWindow();}return 0;
    case trayMessage:
        if(lp==WM_LBUTTONDBLCLK)showWindow();
        else if(lp==NIN_BALLOONUSERCLICK&&balloonAdminAt&&GetTickCount64()-balloonAdminAt<60000){balloonAdminAt=0;elevateWithPrompt(true);}
        else if(lp==NIN_BALLOONUSERCLICK&&priceWatch.balloonOcrAt&&GetTickCount64()-priceWatch.balloonOcrAt<60000){priceWatch.balloonOcrAt=0;priceWatch.installOcr();}
        else if(lp==NIN_BALLOONUSERCLICK&&balloonUpdateAt&&GetTickCount64()-balloonUpdateAt<60000){balloonUpdateAt=0;openPage(updateProc,L"ModeSwitchUpdate",4);}
        else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)trayMenu();
        return 0;
    case WM_PARENTNOTIFY:if(LOWORD(wp)==WM_DESTROY&&pageWindow&&(HWND)lp==pageWindow){pageWindow=nullptr;selectMainPage(mainPage==10?9:mainPage==12?0:mainPage>=6?1:0);}return 0;
    case WM_APP+182:raidWatch.deployed((unsigned)wp);return 0;
    case WM_APP+183:donateWatch.remember(wp);return 0;
    case WM_TIMER:if(wp==77){pubgMap.fast();return 0;}if(wp==79){updateWatch.tick();return 0;}if(wp==78){if(mainPage==5||mainPage==8){HWND d=embeddedWindow(mainPage);if(d&&IsWindowVisible(d))embedSeen=true;else if(embedSeen)selectMainPage(mainPage==8?1:0);}return 0;}pollCompanion();updateWatch.poll();tarkovScreen.poll();tarkovKeys.poll();priceWatch.poll();pollAdmin();pubgJoin.poll();boardWatch.poll();phoneGame.poll();donateWatch.poll();pubgMap.poll();raidWatch.poll();refreshStatus();{static unsigned tick=0;if(++tick%3==0)audioUpdate();}if(!trayPresent)trayUpdate(true);return 0;
    case WM_CLOSE:if(closeToTray)hideToTray();else quit();return 0;
    // Minimize goes to the taskbar like any window; only X (WM_CLOSE) hides to the tray.
    case WM_SIZE:return 0;
    // No Windows title bar: the client covers the whole window (the frame style stays for the taskbar, Snap and the shadow).
    case WM_NCCALCSIZE:if(wp){return 0;}break;
    case WM_NCACTIVATE:return DefWindowProcW(h,msg,wp,-1); // no title bar repaint on (de)activation
    // Drag the window by the top strip and the logo (controls there take their own clicks).
    case WM_NCHITTEST:{POINT p={(short)LOWORD(lp),(short)HIWORD(lp)};ScreenToClient(h,&p);if(p.y>=0&&(p.y<50||(p.x<220&&p.y<104)))return HTCAPTION;return HTCLIENT;}
    case WM_QUERYENDSESSION:
        if(fps&&gammaTouched)changeMode(false);
        return TRUE;
    case WM_DESTROY:{
        raidWatch.capture.stop();stopCompanion();priceWatch.stop();UnregisterHotKey(h,2);boardWatch.capture.stop();pubgMap.close();pubgMap.overlay.destroy();KillTimer(h,77);KillTimer(h,78);KillTimer(h,79);PlaySoundW(nullptr,nullptr,0);KillTimer(h,1);UnregisterHotKey(h,1);NOTIFYICONDATAW data={};data.cbSize=sizeof(data);data.hWnd=h;data.uID=1;Shell_NotifyIconW(NIM_DELETE,&data);
        if(updaterProcess){CloseHandle(updaterProcess);updaterProcess=nullptr;}PostQuitMessage(0);return 0;
    }
    }return DefWindowProcW(h,msg,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int show){
    // Release check (support_ui.h selfTest): no window, no mutex, the running copy is left alone.
    if(const wchar_t* t=wcsstr(command,L"--selftest ")){i18n::setLanguage(regString(configPath,L"Language"));return selfTest(t+11);}
    startHidden=wcsstr(command,L"--tray")!=nullptr;
    // Restarted as administrator (support_ui.h): let the old copy close first.
    if(const wchar_t* r=wcsstr(command,L"--replace ")){if(HANDLE old=OpenProcess(SYNCHRONIZE,FALSE,wcstoul(r+10,nullptr,10))){WaitForSingleObject(old,15000);CloseHandle(old);}}
    // Started elevated to follow Tarkov (support_ui.h): close the running copy, then take its place.
    bool takeover=wcsstr(command,L"--takeover")!=nullptr;
    if(takeover){HWND old=FindWindowW(L"ModeSwitch",nullptr);DWORD pid=0;
        if(old&&GetWindowThreadProcessId(old,&pid)&&pid!=GetCurrentProcessId()){PostMessageW(old,WM_APP+177,0,0);if(HANDLE p=OpenProcess(SYNCHRONIZE,FALSE,pid)){WaitForSingleObject(p,15000);CloseHandle(p);}}}
    // No language saved yet: a new install starts in English (the language pill switches it); a PC that already had
    // BlueAsh (settings there, no choice saved) keeps Korean as before. Either way the choice is saved now.
    {std::wstring lang=regString(configPath,L"Language");if(lang.empty()){bool earlier=false;for(const wchar_t* k:{L"InstallId",L"ProfilesV2",L"ProfilesV1",L"Display",L"Browser"})if(!regString(configPath,k).empty())earlier=true;
        lang=earlier?L"ko":L"en";saveString(configPath,L"Language",lang);}i18n::setLanguage(lang);}
    // 2.9.0 renamed ModePulse to BlueAsh. An install a 2.8.x updater refreshed still starts as ModePulse.exe:
    // rename the file to BlueAsh.exe and start that (it rewrites the autostart entry with its own path).
    {std::wstring self=executable();size_t cut=self.find_last_of(L"\\/");std::wstring dir=self.substr(0,cut+1),target=dir+L"BlueAsh.exe";
        if(_wcsicmp(self.substr(cut+1).c_str(),L"ModePulse.exe")==0&&GetFileAttributesW(target.c_str())==INVALID_FILE_ATTRIBUTES&&MoveFileW(self.c_str(),target.c_str())){
            STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};std::wstring line=L"\""+target+L"\" "+command;
            if(CreateProcessW(target.c_str(),&line[0],nullptr,nullptr,FALSE,0,nullptr,dir.c_str(),&si,&pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return 0;}}}
    HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\ModeSwitch-DisplayAudio-v1");
    if(!mutex)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){
        HWND existing=FindWindowW(L"ModeSwitch",nullptr);
        if(existing&&!startHidden){
            wchar_t title[128]={};GetWindowTextW(existing,title,128);
            if(std::wstring(title)!=L"BlueAsh 2.24.0")MessageBoxW(nullptr,i18n::text(L"이전 버전이 트레이에서 실행 중입니다. 이전 프로그램을 완전 종료한 뒤 BlueAsh.exe를 다시 실행하세요."),i18n::text(L"BlueAsh · 이전 버전 실행 중"),MB_OK|MB_ICONINFORMATION);
            else{ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}
        }
        CloseHandle(mutex);return 0;
    }
    HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(co)){CloseHandle(mutex);return 1;}
    // The UI font (resources 401/402, Pretendard): private to this process, so nothing is installed on the PC.
    {HMODULE self=GetModuleHandleW(nullptr);int loaded=0;for(int id:{401,402}){HRSRC res=FindResourceW(self,MAKEINTRESOURCEW(id),RT_RCDATA);if(!res)continue;DWORD size=SizeofResource(self,res);void* data=LockResource(LoadResource(self,res));DWORD count=0;if(data&&size&&AddFontMemResourceEx(data,size,nullptr,&count)&&count)loaded++;}i18n::customFont=loaded==2;}
    painting::initialize();backgroundBrush=carbonBrush();sidebarBrush=CreateSolidBrush(sideFill);panelBrush=CreateSolidBrush(panel);rowBrush=CreateSolidBrush(rowFill);
    WNDCLASSW wc={};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=L"ModeSwitch";
    wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));wc.hbrBackground=backgroundBrush;
    RegisterClassW(&wc);
    DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN;
    RECT bounds={0,0,1160,780}; // the client is the whole window (WM_NCCALCSIZE)
    HWND h=CreateWindowW(wc.lpszClassName,L"BlueAsh 2.24.0",style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
    if(!h){painting::shutdown();CoUninitialize();CloseHandle(mutex);return 1;}
    {MARGINS shadow={0,0,0,1};DwmExtendFrameIntoClientArea(h,&shadow);SetWindowPos(h,nullptr,0,0,0,0,SWP_FRAMECHANGED|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);} // keep the shadow without a frame
    if(wcsstr(command,L"--settings")){startHidden=false;selectMainPage(9);} // back from a language change
    std::thread(removeElevatedTask).detach(); // an administrator task left by 2.23.0 or older
    if(selfElevated()){
        if(takeover)balloon::show(i18n::text(L"관리자 권한으로 전환했습니다"),i18n::text(L"타르코프가 관리자 권한으로 실행 중이라 BlueAsh도 관리자 권한으로 바꿨습니다. 지도와 시세를 그대로 쓰면 됩니다."),L"");}
    if(!startHidden||!trayPresent||!ready)ShowWindow(h,show);
    MSG m;while(GetMessageW(&m,nullptr,0,0)>0){if(capturePriceKey(m))continue;
        // Esc and Enter on the main window itself must not reach IsDialogMessage: it turns them into IDCANCEL (2) and
        // IDOK (1), which are the 게임 모드 / 일반 모드 cards, so Esc switched to game mode. Enter presses the focused button;
        // both keys still reach drop-downs and other controls normally. (Pages handle their own Esc/Enter first.)
        if((m.message==WM_KEYDOWN||m.message==WM_CHAR)&&(m.wParam==VK_ESCAPE||m.wParam==VK_RETURN)&&(m.hwnd==h||(GetParent(m.hwnd)==h&&m.hwnd!=pageWindow))){
            wchar_t cls[16]={};GetClassNameW(m.hwnd,cls,16);
            if(m.message==WM_KEYDOWN&&m.wParam==VK_RETURN&&!_wcsicmp(cls,L"Button")&&IsWindowEnabled(m.hwnd)){SendMessageW(h,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(m.hwnd),BN_CLICKED),(LPARAM)m.hwnd);continue;}
            if(m.hwnd==h||!_wcsicmp(cls,L"Button")){continue;}TranslateMessage(&m);DispatchMessageW(&m);continue;}
        if(!(editor&&IsDialogMessageW(editor,&m))&&!(pageWindow&&IsChild(pageWindow,m.hwnd)&&IsDialogMessageW(pageWindow,&m))&&!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}
    for(HICON icon:icons)if(icon)DestroyIcon(icon);
    for(HICON icon:{logoIcon,appSmallIcon,appLargeIcon})if(icon)DestroyIcon(icon);
    DeleteObject(font);DeleteObject(smallFont);DeleteObject(titleFont);DeleteObject(cardFont);DeleteObject(sectionFont);DeleteObject(tileFont);DeleteObject(footFont);DeleteObject(footSmall);DeleteObject(smallBold);DeleteObject(captionFont);for(auto& item:carbonBrushes)DeleteObject(item.second);DeleteObject(sidebarBrush);DeleteObject(panelBrush);
    gameart::release();painting::shutdown();CoUninitialize();CloseHandle(mutex);
    // The new copy waits for this process to end (--replace) and opens on 설정, where the language was picked.
    if(languageRestart){STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};std::wstring path=executable(),restart=L"\""+path+L"\""+(languageToSettings?L" --settings":L"")+L" --replace "+std::to_wstring(GetCurrentProcessId());if(CreateProcessW(path.c_str(),&restart[0],nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi)){CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}else MessageBoxW(nullptr,i18n::text(L"프로그램을 다시 실행해 주세요."),L"BlueAsh",MB_OK|MB_ICONERROR);}
    return 0;
}
