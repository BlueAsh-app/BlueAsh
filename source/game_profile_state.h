#pragma once
#include <string>
#include <vector>
#include <cwctype>
#include <algorithm>
#include <map>
#include "profiles.h"
namespace gameprofiles {
inline std::wstring encodeProfiles(const std::map<std::wstring,settings::Profile>& saved){
    std::wstring out;for(const auto& entry:saved){settings::Profiles p={entry.second,entry.second,entry.second,entry.second};out+=entry.first+L"|"+settings::encodeV2(p,false)+L"\n";}return out;
}
inline bool decodeProfiles(const std::wstring& value,std::map<std::wstring,settings::Profile>& out){
    std::map<std::wstring,settings::Profile> found;size_t pos=0;
    while(pos<value.size()){auto end=value.find(L'\n',pos);if(end==std::wstring::npos)return false;auto line=value.substr(pos,end-pos);auto sep=line.find(L'|');
        if(sep==std::wstring::npos||sep==0||sep>200)return false;
        auto id=line.substr(0,sep);if(id.find_first_of(L"\r\n|")!=std::wstring::npos||found.count(id))return false;
        settings::Profiles p;bool automatic=false;if(!settings::decodeV2(line.substr(sep+1),p,automatic)||automatic)return false;found[id]=p[0];pos=end+1;
    }out=std::move(found);return true;
}
inline std::wstring normalize(std::wstring s){for(auto& c:s){if(c==L'/')c=L'\\';else c=(wchar_t)std::towlower(c);}while(!s.empty()&&s.back()==L'\\')s.pop_back();return s;}
inline bool within(const std::wstring& executable,const std::wstring& directory){
    auto p=normalize(executable),d=normalize(directory);if(d.empty()||p.size()<=d.size()||p.compare(0,d.size(),d)||p[d.size()]!=L'\\')return false;
    auto name=p.substr(p.find_last_of(L'\\')+1);for(const wchar_t* s:{L"launcher",L"crash",L"unins",L"setup",L"redist",L"report",L"helper",L"updater"})if(name.find(s)!=std::wstring::npos)return false;
    return true;
}
inline std::wstring choose(const std::vector<std::wstring>& running,const std::wstring& foreground,const std::wstring& previous){
    auto contains=[&](const std::wstring& id){return !id.empty()&&std::find(running.begin(),running.end(),id)!=running.end();};
    if(contains(foreground))return foreground;
    if(contains(previous))return previous;
    if(running.size()==1)return running[0];
    return L"";
}
}
