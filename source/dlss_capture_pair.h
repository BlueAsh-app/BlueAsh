#pragma once
#include <string>
namespace dlsscapture {
inline bool validName(const std::wstring& n){
    const std::wstring pre=L"NR_CAPTURE_",suf=L".meta.txt";
    if(n.size()<=pre.size()+suf.size()||n.compare(0,pre.size(),pre)||n.compare(n.size()-suf.size(),suf.size(),suf))return false;
    for(size_t i=pre.size();i<n.size()-suf.size();i++)if(!((n[i]>=L'0'&&n[i]<=L'9')||n[i]==L'_'))return false;
    return true;
}
inline bool validMetadata(const std::string& s){
    const std::string line="comparison=final-present-same-frame";size_t p=s.find(line);
    return p!=std::string::npos&&(p==0||s[p-1]=='\n')&&(p+line.size()==s.size()||s[p+line.size()]=='\r'||s[p+line.size()]=='\n');
}
}
