#include "dlss_apply.h"
#include <cassert>
#include <cstdio>
int main(){wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);std::wstring root=std::wstring(t)+L"blueash-progress-"+std::to_wstring(GetCurrentProcessId());assert(CreateDirectoryW(root.c_str(),nullptr));
 std::wstring src=root+L"\\source",dest=root+L"\\game",record=root+L"\\record";FILE* f=_wfopen(src.c_str(),L"wb");assert(f);fputs("progress-test",f);fclose(f);
 std::vector<dlssapply::Change> plan={{dest,src,dlssapply::sha256(src)}};std::vector<int> updates;
 assert(!dlssapply::apply(record,plan,[]{return true;},[&](int n){updates.push_back(n);}).empty());assert(updates.empty());
 assert(dlssapply::apply(record,plan,nullptr,[&](int n){updates.push_back(n);}).empty());assert(updates.size()==1&&updates[0]==95);assert(dlssapply::exists(record+L"\\applied.txt"));assert(dlssapply::sha256(dest)==plan[0].sha256);
 puts("DLSS verified file progress passed; scratch files preserved");}
