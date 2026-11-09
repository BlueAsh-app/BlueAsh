#include <cassert>
#include <cstdio>
#include "dlss_apply.h"
using namespace dlssapply;
static std::wstring root;
static void put(const std::wstring& p,const char* s){FILE* f=_wfopen(p.c_str(),L"wb");fputs(s,f);fclose(f);}
static std::string get(const std::wstring& p){FILE* f=_wfopen(p.c_str(),L"rb");if(!f)return "<none>";char b[256]={};size_t n=fread(b,1,255,f);fclose(f);return std::string(b,n);}
int main(){
    wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);root=std::wstring(t)+L"blueash-dlss-test-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(root.c_str(),nullptr);
    std::wstring game=root+L"\\game",rec=root+L"\\record",src=root+L"\\src";for(auto& d:{game,src})CreateDirectoryW(d.c_str(),nullptr);
    put(game+L"\\a.dll","original A");put(src+L"\\a.dll","new A");put(src+L"\\b.dll","new B");
    std::vector<Change> ch={{game+L"\\a.dll",src+L"\\a.dll",sha256(src+L"\\a.dll")},{game+L"\\b.dll",src+L"\\b.dll",sha256(src+L"\\b.dll")}};
    // Game running: nothing is touched.
    assert(!apply(rec,ch,[]{return true;}).empty()&&get(game+L"\\a.dll")=="original A"&&!exists(rec+L"\\pending.txt"));
    // Wrong hash: refused before any change.
    auto bad=ch;bad[1].sha256=std::string(64,'0');assert(!apply(rec,bad,nullptr).empty()&&get(game+L"\\a.dll")=="original A");
    // Same target twice: refused.
    auto twice=ch;twice[1].target=game+L"\\A.DLL";assert(!apply(rec,twice,nullptr).empty());
    // TEST 7: failure in the middle -> originals back, the new file removed.
    failAfter=1;assert(!apply(rec,ch,nullptr).empty());failAfter=-1;
    assert(get(game+L"\\a.dll")=="original A"&&!exists(game+L"\\b.dll")&&!exists(rec+L"\\pending.txt")&&!exists(rec+L"\\applied.txt"));
    // Normal apply, no second apply on top, then restore.
    assert(apply(rec,ch,nullptr).empty()&&get(game+L"\\a.dll")=="new A"&&get(game+L"\\b.dll")=="new B");
    assert(!apply(rec,ch,nullptr).empty());
    assert(!restore(rec,[]{return true;}).empty()&&get(game+L"\\a.dll")=="new A"); // not while the game runs
    assert(restore(rec,nullptr).empty()&&get(game+L"\\a.dll")=="original A"&&!exists(game+L"\\b.dll"));
    // BlueAsh killed mid-apply: the journal is there, a file already changed -> recover() rolls back.
    CreateDirectoryW((rec+L"\\backup").c_str(),nullptr);CopyFileW((game+L"\\a.dll").c_str(),(rec+L"\\backup\\0.bak").c_str(),FALSE);
    assert(writeJournal(rec+L"\\pending.txt",{{game+L"\\a.dll",rec+L"\\backup\\0.bak"},{game+L"\\b.dll",L"-"}}));
    put(game+L"\\a.dll","half written");put(game+L"\\b.dll","new B");
    assert(recover(rec)&&get(game+L"\\a.dll")=="original A"&&!exists(game+L"\\b.dll")&&!exists(rec+L"\\pending.txt"));
    // Files in folders that do not exist yet: the folders are created, and removed again on restore or failure.
    put(src+L"\\c.dll","new C");std::vector<Change> deep={{game+L"\\Sub\\Inner\\c.dll",src+L"\\c.dll",sha256(src+L"\\c.dll")}};
    assert(apply(rec,deep,nullptr).empty()&&get(game+L"\\Sub\\Inner\\c.dll")=="new C");
    assert(restore(rec,nullptr).empty()&&!exists(game+L"\\Sub\\Inner\\c.dll")&&!exists(game+L"\\Sub"));
    failAfter=0;assert(!apply(rec,deep,nullptr).empty());failAfter=-1;assert(!exists(game+L"\\Sub"));
    puts("DLSS apply tests passed");
}
