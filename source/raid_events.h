#pragma once
#include <string>
#include <functional>
namespace raid {
enum class Event { Countdown, Started };
// Event markers documented by TarkovMonitor's GameWatcher. No game memory access.
class Parser {
    std::string partial;
    bool announced=false,online=false;
public:
    void reset(){partial.clear();announced=false;online=false;}
    void line(const std::string& s,const std::function<void(Event)>& emit){
        bool create=s.find("application|TRACE-NetworkGameCreate profileStatus")!=std::string::npos;
        if(s.find("application|scene preset path:")!=std::string::npos)online=false;
        if(create)online=true;
        if(s.find("application|scene preset path:")!=std::string::npos||create||
           s.find("application|Network game matching aborted")!=std::string::npos ||
           s.find("application|Network game matching cancelled")!=std::string::npos)
            announced=false;
        // Where the deploy countdown starts depends on the raid (1.2.0.0.47965, every raid of 2026-10-12):
        // online raids (PvP, group PvE: a NetworkGameCreate line) go GamePooled -> GameSpawned -> waiting for the other
        // players (7~18 s, longer in a group) -> GameStarting = countdown -> GameStarted ~12 s later;
        // solo PvE has nothing between GamePooled and GameStarted (~23 s later, the rest in the same millisecond),
        // so its countdown starts at GamePooled. A raid with neither line in time still alerts at GameStarted.
        bool starting=s.find("application|GameStarting")!=std::string::npos;
        if(!announced && (starting||(!online&&s.find("application|GamePooled")!=std::string::npos))){announced=true;emit(Event::Countdown);}
        else if(!announced && s.find("application|GameStarted")!=std::string::npos){announced=true;emit(Event::Started);}
    }
    void feed(const char* data,size_t size,const std::function<void(Event)>& emit){
        partial.append(data,size);
        size_t from=0,end;
        while((end=partial.find('\n',from))!=std::string::npos){line(partial.substr(from,end-from),emit);from=end+1;}
        partial.erase(0,from);
        if(partial.size()>65536)partial.clear();
    }
};
}
