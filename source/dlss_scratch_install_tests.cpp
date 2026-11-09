#include "dlss_optiscaler.h"
#include <cassert>
#include <cstdio>
int main(){wchar_t local[MAX_PATH]={},temp[MAX_PATH]={};assert(GetEnvironmentVariableW(L"LOCALAPPDATA",local,MAX_PATH));GetTempPathW(MAX_PATH,temp);
 std::wstring base=std::wstring(local)+L"\\BlueAsh\\dlss",comp=base+L"\\components\\OptiScaler-DLSSNR-0.2.0-patch1",runtime=base+L"\\runtime\\nvngx_dlssnr.dll";
 assert(dlssopti::ready(comp));assert(dlssrt::check(runtime).error.empty());std::wstring root=std::wstring(temp)+L"blueash-install-progress-"+std::to_wstring(GetCurrentProcessId());assert(CreateDirectoryW(root.c_str(),nullptr));
 std::wstring game=root+L"\\game",record=root+L"\\record",ini=root+L"\\OptiScaler.ini";assert(CreateDirectoryW(game.c_str(),nullptr));assert(CopyFileW((comp+L"\\OptiScaler.ini").c_str(),ini.c_str(),TRUE));
 auto plan=dlssopti::plan(comp,game,runtime,ini);std::vector<int> values;auto error=dlssapply::apply(record,plan,nullptr,[&](int n){values.push_back(n);});if(!error.empty())wprintf(L"%ls\n",error.c_str());assert(error.empty());assert(!values.empty()&&values.back()==95);assert(dlssapply::exists(record+L"\\applied.txt"));
 for(auto& c:plan){assert(dlssapply::sha256(c.target)==c.sha256);}puts("Verified OptiScaler scratch installation and progress; files preserved");}
