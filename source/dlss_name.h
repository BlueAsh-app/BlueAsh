#pragma once
#include <string>
// Take one stable value before forming its iterator range.
inline std::string dlssExecutableName(const std::wstring& value){return std::string(value.begin(),value.end());}
