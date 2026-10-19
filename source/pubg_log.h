#pragma once
#include <string>
#include <functional>
#include <cstdio>
// PUBG's TslGame.log is almost all encrypted hex lines; the match join stays plain text:
// "[2026.10.05-13.10.09:610][208][a244]UTslGameInstance::JoinToDedicatedServer() [GameModeAliase=BATTLEROYALE]"
// It is written when a match is found and the starting island loads (before the plane). Log file only, no game memory.
namespace pubglog {
inline std::string joinMode(const std::string& s){
    if(s.find("UTslGameInstance::JoinToDedicatedServer()")==std::string::npos)return "";
    auto at=s.find("[GameModeAliase=");if(at==std::string::npos)return "?";at+=16;
    auto end=s.find(']',at);return end==std::string::npos?"?":s.substr(at,end-at);
}
// UTC time of a log line "[2026.10.05-13.10.09:610]..." as seconds since 1970, -1 when the line has none.
inline long long lineTimeUtc(const std::string& s){
    int Y,M,D,h,m,sec;if(s.size()<21||sscanf(s.c_str(),"[%4d.%2d.%2d-%2d.%2d.%2d",&Y,&M,&D,&h,&m,&sec)!=6){return -1;}
    // days from civil (Howard Hinnant), no time zone involved
    int y=Y-(M<=2);int era=(y>=0?y:y-399)/400;unsigned yoe=unsigned(y-era*400),doy=(153*(M+(M>2?-3:9))+2)/5+D-1,doe=yoe*365+yoe/4-yoe/100+doy;
    long long days=(long long)era*146097+doe-719468;return days*86400+h*3600+m*60+sec;
}
// The last match join in a chunk of log text (e.g. the end of TslGame.log read when BlueAsh starts mid-match).
inline long long lastJoinUtc(const std::string& text){
    size_t at=text.rfind("UTslGameInstance::JoinToDedicatedServer()");if(at==std::string::npos){return -1;}
    size_t start=text.rfind('\n',at);start=start==std::string::npos?0:start+1;return lineTimeUtc(text.substr(start,at-start));
}
struct Parser {
    std::string partial;
    void reset(){partial.clear();}
    void feed(const char* data,size_t size,const std::function<void(const std::string&)>& joined){
        partial.append(data,size);size_t from=0,end;
        while((end=partial.find('\n',from))!=std::string::npos){auto m=joinMode(partial.substr(from,end-from));if(!m.empty())joined(m);from=end+1;}
        partial.erase(0,from);if(partial.size()>65536)partial.clear();
    }
};
}
