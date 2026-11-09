#pragma once
#include <string>
namespace dlssstatus {
enum class State {Absent,Installed,Restart,Active,Waiting};
enum class Log {Unknown,Active,Off,Waiting};
inline State classify(bool files,bool running,bool restart,bool active,bool waiting){
 if(!files)return State::Absent;
 if(running&&restart)return State::Restart;
 if(running&&active)return State::Active;
 if(running&&waiting)return State::Waiting;
 return State::Installed;
}
inline Log logState(const std::string& s){
 Log out=Log::Unknown;size_t a=0;
 while(a<s.size()){size_t b=s.find('\n',a);if(b==std::string::npos)b=s.size();std::string line=s.substr(a,b-a);
  if(line.find("NR-VERDICT")!=std::string::npos){if(line.find("state=ENGAGED")!=std::string::npos)out=Log::Active;
   else if(line.find("state=OFF")!=std::string::npos||line.find("state=UNAVAILABLE")!=std::string::npos||line.find("state=NOT_ENGAGED")!=std::string::npos)out=Log::Off;}
  else if(line.find("NR toggled OFF")!=std::string::npos)out=Log::Off;
  else if(line.find("DlssNr_Dx12::Dispatch DLSS-NR cost:")!=std::string::npos||line.find("inline feature 18 evaluation succeeded")!=std::string::npos)out=Log::Active;
  else if(line.find("waiting for upscaler input")!=std::string::npos)out=Log::Waiting;
  a=b+1;
 }return out;
}
}
