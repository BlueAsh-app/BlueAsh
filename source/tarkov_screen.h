#pragma once
#include "i18n.h"
// Price tags and the map are windows over the game, which cannot appear over exclusive fullscreen.
// So while Tarkov is closed, its saved screen mode is switched from fullscreen to borderless: the same value its
// own settings menu writes (Graphics.ini "FullScreenMode": 0 fullscreen / 1 borderless / 2 windowed; checked on
// this PC: mode 1 stays unminimised behind other windows). Never while the game runs (it rewrites the file
// itself), only that one number changes, and the first original is kept as Graphics.ini.modepulse-backup.
// Registry TarkovBorderless (default on) turns it off for players who want fullscreen.
// Changing the game's own settings files is asked once, right before the first change would be written (a Windows
// message box that says what changes and that the original is kept). The answer is kept in GameConfigConsent: "0" stops
// both automatic changes here and in tarkov_keys.h; turning 전체창모드 자동 맞춤 back on (setEnabled) counts as yes.
inline bool gameConfigAllowed(){
    auto v=regString(configPath,L"GameConfigConsent");if(v==L"1"){return true;}if(v==L"0"){return false;}
    static bool asking=false;if(asking){return false;}asking=true;
    int r=MessageBoxW(win,i18n::text(L"BlueAsh가 타르코프 설정 파일을 바꿔도 될까요?\n\n• 화면 모드: 전체화면이면 전체창모드로 (시세·지도가 게임 위에 보이도록)\n• 키 설정: 지도 키를 게임 스크린샷 키에도 추가 (지도에 내 위치 표시)\n\n게임이 꺼져 있을 때만 바꾸고, 원본은 .modepulse-backup 파일로 남깁니다. 아이템 시세 설정에서 언제든 끌 수 있습니다."),i18n::text(L"타르코프 설정 변경"),MB_YESNO|MB_ICONQUESTION|MB_SETFOREGROUND);
    asking=false;bool yes=r==IDYES;saveString(configPath,L"GameConfigConsent",yes?L"1":L"0");if(!yes){saveString(configPath,L"TarkovBorderless",L"0");}return yes;
}
struct TarkovScreen{
    bool warned=false;ULONGLONG next=0;
    bool enabled(){return regString(configPath,L"TarkovBorderless")!=L"0";}
    void setEnabled(bool on){saveString(configPath,L"TarkovBorderless",on?L"1":L"0");if(on){saveString(configPath,L"GameConfigConsent",L"1");}warned=false;next=0;}
    static std::wstring path(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);return std::wstring(d)+L"\\Battlestate Games\\Escape from Tarkov\\Settings\\Graphics.ini";}
    static bool read(const std::wstring& file,std::string& text){
        HANDLE f=CreateFileW(file.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return false;
        LARGE_INTEGER size={};bool ok=GetFileSizeEx(f,&size)&&size.QuadPart>0&&size.QuadPart<1048576;
        if(ok){text.resize((size_t)size.QuadPart);DWORD got=0;ok=ReadFile(f,&text[0],(DWORD)text.size(),&got,nullptr)&&got==text.size();}
        CloseHandle(f);return ok;
    }
    // Offset of the mode digit, or npos.
    static size_t modeAt(const std::string& text){
        size_t at=text.find("\"FullScreenMode\"");if(at==std::string::npos)return at;at=text.find(':',at);if(at==std::string::npos)return at;
        do at++;while(at<text.size()&&(text[at]==' '||text[at]=='\t'));
        return at<text.size()&&text[at]>='0'&&text[at]<='9'&&(at+1>=text.size()||text[at+1]<'0'||text[at+1]>'9')?at:std::string::npos;
    }
    static void balloon(const wchar_t* text){
        NOTIFYICONDATAW n={};n.cbSize=sizeof(n);n.hWnd=win;n.uID=1;n.uFlags=NIF_INFO;n.dwInfoFlags=NIIF_INFO|NIIF_NOSOUND;
        wcscpy(n.szInfoTitle,i18n::text(L"타르코프 화면 모드"));wcsncpy(n.szInfo,text,255);Shell_NotifyIconW(NIM_MODIFY,&n);
    }
    void poll(){
        ULONGLONG now=GetTickCount64();if(now<next||!enabled())return;next=now+5000;
        auto file=path();std::string text;if(!read(file,text))return;size_t at=modeAt(text);if(at==std::string::npos||text[at]!='0'){warned=false;return;}
        if(tarkovProcess()){if(!warned){warned=true;reportProblem(L"fullscreen");balloon(i18n::text(L"전체화면에서는 시세·지도가 게임 위에 보이지 않습니다. 게임을 끄면 전체창모드로 바꿔 둡니다."));}return;}
        if(!gameConfigAllowed()){return;}
        auto backup=file+L".modepulse-backup";CopyFileW(file.c_str(),backup.c_str(),TRUE);
        text[at]='1';auto temp=file+L".modepulse-tmp";
        HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(f==INVALID_HANDLE_VALUE)return;
        DWORD wrote=0;bool ok=WriteFile(f,text.data(),(DWORD)text.size(),&wrote,nullptr)&&wrote==text.size();CloseHandle(f);
        if(!ok||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temp.c_str());return;}
        warned=false;balloon(i18n::text(L"다음 실행부터 전체창모드로 바꿨습니다. 시세·지도 표시가 게임 위에 보입니다."));
    }
} tarkovScreen;
