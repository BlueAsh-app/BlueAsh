#pragma once
#include <string>
#include <cwctype>
#include <cstdint>
namespace raid {
inline bool logName(std::wstring name){
    for(auto& c:name)c=(wchar_t)std::towlower(c);
    return name.size()>=4 && name.substr(name.size()-4)==L".log" &&
        (name.find(L"application")!=std::wstring::npos||name.find(L"output")!=std::wstring::npos||name.find(L"notifications")!=std::wstring::npos);
}
inline std::uint64_t initialOffset(bool initial,std::uint64_t created,std::uint64_t started,std::uint64_t size){
    return initial||created<started?size:0;
}
}
