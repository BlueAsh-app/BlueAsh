#pragma once
#include <regex>
#include <string>
#include <vector>
#include <algorithm>
namespace raid {
// Scene preset bundle -> Location id. PvE logs have no NetworkGameCreate line, so the preset is the only map marker.
inline std::string presetMap(const std::string& s){
 std::smatch m;if(!std::regex_search(s,m,std::regex("scene preset path:maps/([a-z0-9_]+?)(?:_preset)?\\.bundle")))return "";
 static const char* table[][2]={{"city","tarkovstreets"},{"customs","bigmap"},{"factory_day","factory4_day"},{"factory_night","factory4_night"},{"laboratory_dark","laboratory"},{"rezerv_base","rezervbase"},{"sandbox_start","sandbox"},{"shopping_mall","interchange"},
  {"icebreaker","icebreaker"},{"laboratory","laboratory"},{"labyrinth","labyrinth"},{"lighthouse","lighthouse"},{"sandbox","sandbox"},{"sandbox_high","sandbox_high"},{"shoreline","shoreline"},{"terminal","terminal"},{"woods","woods"}};
 for(auto& row:table){if(m[1]==row[0])return row[1];}
 return "";
}
// EFT gives each account's scav its own profile whose id is the PMC profile id + 1 (verified against
// savage/regenerate after every "+1" raid). The selected-profile lines only ever name the PMC.
inline std::string nextId(std::string s){
 for(size_t i=s.size();i-->0;){char& c=s[i];int v=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(v<0)return "";if(v<15){v++;c=(char)(v<10?'0'+v:'a'+v-10);return s;}c='0';}
 return "";
}
struct Context {
 std::string partial,map,role="UNKNOWN",id,phase="unknown";unsigned serial=0;
 // Transit: the side the player crossed with (PvE "transit hop ... side:Pmc"), or the last raid's role when the new raid
 // says it was reached by transit ("[Transit] ... Count:1"). The new map loads with role UNKNOWN otherwise.
 std::string transitRole,lastRole="UNKNOWN";
 std::vector<std::string> pmc;bool learned=false; // known PMC profile ids (one per game mode/account), persisted by the caller
 bool learn(const std::string& s){
  if(s.find("ProfileId:")==std::string::npos)return false;
  std::smatch m;if(!std::regex_search(s,m,std::regex("(?:CompleteSelectedProfile|PrepareSelectedProfileLocally|SelectProfile|SelectedProfile) ProfileId: ?([0-9a-f]{24})")))return false;
  std::string p=m[1];if(std::find(pmc.begin(),pmc.end(),p)!=pmc.end())return false;
  pmc.push_back(p);if(pmc.size()>8)pmc.erase(pmc.begin());learned=true;return true;
 }
 std::string roleOf(const std::string& profile) const {
  for(auto& p:pmc){if(p==profile)return "PMC";if(nextId(p)==profile)return "SCAV";}
  return "UNKNOWN";
 }
 bool line(const std::string& s){
  bool changed=false;std::smatch m;learn(s);
  if(s.find("transit hop")!=std::string::npos&&std::regex_search(s,m,std::regex("side:(Pmc|Savage|Scav)",std::regex::icase))){transitRole=(m[1]=="Pmc"||m[1]=="pmc")?"PMC":"SCAV";}
  if(s.find("application|scene preset path:")!=std::string::npos){if(role!="UNKNOWN")lastRole=role;map=presetMap(s);role=transitRole.empty()?"UNKNOWN":transitRole;transitRole.clear();id.clear();phase="loading";serial++;changed=true;}
  if(s.find("application|[Transit] Flag:")!=std::string::npos&&role=="UNKNOWN"&&lastRole!="UNKNOWN"&&std::regex_search(s,m,std::regex("Count:([0-9]+)"))&&m[1]!="0"){role=lastRole;changed=true;}
  if(s.find("application|TRACE-NetworkGameCreate profileStatus")!=std::string::npos){
   if(std::regex_search(s,m,std::regex("Location: ([A-Za-z0-9_]+)"))){map=m[1];changed=true;}
   if(std::regex_search(s,m,std::regex("shortId: ([A-Z0-9]{6})"))){id=m[1];changed=true;}
   if(std::regex_search(s,m,std::regex("Profileid: ([0-9a-f]{24})"))){auto r=roleOf(m[1]);if(r!="UNKNOWN"){role=r;changed=true;}}
   if(std::regex_search(s,m,std::regex("Side: (Savage|Usec|Bear)"))){role=m[1]=="Savage"?"SCAV":"PMC";changed=true;}
  }
  if(s.find("application|GameStarted")!=std::string::npos){phase="started";changed=true;}
  if(s.find("Got notification | UserMatchOver")!=std::string::npos || s.find("application|Network game matching aborted")!=std::string::npos || s.find("application|Network game matching cancelled")!=std::string::npos || (phase=="started"&&(s.find("SelectProfile ProfileId:")!=std::string::npos||s.find("SelectedProfile ProfileId:")!=std::string::npos||s.find("PrepareSelectedProfileLocally ProfileId:")!=std::string::npos))){phase="ended";changed=true;}
  return changed;
 }
 template<class F> void feed(const char* p,size_t n,F emit){partial.append(p,n);size_t start=0,end;while((end=partial.find('\n',start))!=std::string::npos){if(line(partial.substr(start,end-start)))emit(*this);start=end+1;}partial.erase(0,start);if(partial.size()>65536)partial.clear();}
};
}
