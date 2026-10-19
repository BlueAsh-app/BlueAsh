#pragma once
#include "i18n.h"
// Tarkov key bindings the player asked BlueAsh to set, written to Control.ini only while the game is closed
// (it rewrites the file on exit). Only the named keyCode lists change; the first original is kept as
// Control.ini.modepulse-backup.
//  - TarkovKeyMoves  "Binding:From>To;..."  one-time moves of a single-key binding (e.g. WeaponMounting:M>Comma),
//                    dropped once applied or once the binding no longer is From.
//  - MapScreenshotKey (map window option "지도 열 때 내 위치 갱신"): the map key also becomes the second
//                    MakeScreenshot binding, so one key press makes the game take its own screenshot (position in
//                    the file name) while BlueAsh opens the map. Nothing is sent to the game. MapScreenshotSet
//                    remembers what was added so turning the option off removes only that.
namespace gamekeys{
    const size_t none=std::string::npos;
    // The keyCode array of one variant: offsets of '[' and ']'.
    inline bool locate(const std::string& t,const std::string& name,int variant,size_t& open,size_t& close){
        size_t at=t.find("\"keyName\": \""+name+"\"");if(at==none)return false;size_t next=t.find("\"keyName\"",at+1),p=at;
        for(int i=0;i<=variant;i++){p=t.find("\"keyCode\"",p+1);if(p==none||(next!=none&&p>next))return false;}
        open=t.find('[',p);close=open==none?none:t.find(']',open);return close!=none&&(next==none||close<next);
    }
    inline bool get(const std::string& t,const std::string& name,int variant,std::vector<std::string>& keys){
        size_t open,close;if(!locate(t,name,variant,open,close))return false;keys.clear();
        for(size_t q=t.find('"',open);q!=none&&q<close;){size_t e=t.find('"',q+1);if(e==none||e>close)return false;keys.push_back(t.substr(q+1,e-q-1));q=t.find('"',e+1);}
        return true;
    }
    // Same layout the game writes: elements two spaces deeper than the "keyCode" line, "[]" when empty.
    inline bool set(std::string& t,const std::string& name,int variant,const std::vector<std::string>& keys){
        size_t open,close;if(!locate(t,name,variant,open,close))return false;
        std::string nl=t.find("\r\n")!=none?"\r\n":"\n",indent;size_t line=t.rfind('\n',open);for(size_t i=line==none?0:line+1;i<t.size()&&t[i]==' ';i++)indent+=' ';
        std::string body="[";for(size_t i=0;i<keys.size();i++)body+=nl+indent+"  \""+keys[i]+"\""+(i+1<keys.size()?",":"");body+=keys.empty()?"]":nl+indent+"]";
        t.replace(open,close-open+1,body);return true;
    }
    // Pure edit for tests: applies moves (returns the ones still pending) and the screenshot key.
    inline std::string apply(std::string& t,const std::string& moves,const std::string& want,std::string& added){
        std::string pending;size_t start=0;
        while(start<moves.size()){
            size_t end=moves.find(';',start);if(end==none)end=moves.size();std::string item=moves.substr(start,end-start);start=end+1;
            size_t colon=item.find(':'),arrow=item.find('>');if(colon==none||arrow==none||arrow<colon)continue;
            std::string name=item.substr(0,colon),from=item.substr(colon+1,arrow-colon-1),to=item.substr(arrow+1);std::vector<std::string> keys;
            if(!get(t,name,0,keys)){pending+=item+";";continue;}
            if(keys.size()==1&&keys[0]==from)set(t,name,0,{to});
        }
        std::vector<std::string> shot;
        if(get(t,"MakeScreenshot",1,shot)){
            if(!want.empty()){if(shot.empty()||(shot.size()==1&&shot[0]==added)){set(t,"MakeScreenshot",1,{want});added=want;}}
            else if(!added.empty()){if(shot.size()==1&&shot[0]==added)set(t,"MakeScreenshot",1,{});added="";}
        }
        return pending;
    }
}
struct TarkovKeys{
    ULONGLONG next=0;
    static std::wstring path(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);return std::wstring(d)+L"\\Battlestate Games\\Escape from Tarkov\\Settings\\Control.ini";}
    static std::string narrow(const std::wstring& w){return std::string(w.begin(),w.end());}
    void poll(){
        ULONGLONG now=GetTickCount64();if(now<next)return;next=now+5000;
        std::string moves=narrow(regString(configPath,L"TarkovKeyMoves")),want=narrow(regString(configPath,L"MapScreenshotKey")),added=narrow(regString(configPath,L"MapScreenshotSet")),was=added;
        if(moves.empty()&&(want==added||regString(configPath,L"GameConfigConsent")==L"0"))return;
        if(tarkovProcess())return;
        auto file=path();std::string text;if(!TarkovScreen::read(file,text))return;std::string before=text;
        std::string pending=gamekeys::apply(text,moves,want,added);
        // A move the player picked in the key-conflict dialog was asked for; the screenshot key needs the one-time consent.
        if(text!=before&&want!=was&&!gameConfigAllowed()){text=before;added=was;std::string keep=was;pending=gamekeys::apply(text,moves,keep,added);}
        if(text!=before){
            auto backup=file+L".modepulse-backup";CopyFileW(file.c_str(),backup.c_str(),TRUE);auto temp=file+L".modepulse-tmp";
            HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return;
            DWORD wrote=0;bool ok=WriteFile(f,text.data(),(DWORD)text.size(),&wrote,nullptr)&&wrote==text.size();CloseHandle(f);
            if(!ok||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temp.c_str());return;}
            NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;
            wcscpy(n.szInfoTitle,i18n::text(L"타르코프 키 설정"));wcscpy(n.szInfo,i18n::text(L"요청한 게임 키 설정을 적용했습니다. 다음 실행부터 반영됩니다."));Shell_NotifyIconW(NIM_MODIFY,&n);
        }
        saveString(configPath,L"TarkovKeyMoves",std::wstring(pending.begin(),pending.end()));
        if(added!=was)saveString(configPath,L"MapScreenshotSet",std::wstring(added.begin(),added.end()));
    }
} tarkovKeys;
