#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "dlss_optiscaler.h"
// Conflict rules always; the full fetch-check-install-restore cycle only when BLUEASH_TEST_OPTI_ZIP names the release
// archive (it is never in the repository) and BLUEASH_TEST_NR a signed runtime. Nothing outside a temp folder is touched.
static std::wstring tmp(const std::wstring& n){wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);std::wstring d=std::wstring(t)+L"blueash-opti-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(d.c_str(),nullptr);return d+L"\\"+n;}
static void touch(const std::wstring& p){FILE* f=_wfopen(p.c_str(),L"wb");fputs("x",f);fclose(f);}
static bool gone(const std::wstring& p){return GetFileAttributesW(p.c_str())==INVALID_FILE_ATTRIBUTES;}
int main(){
    std::wstring game=tmp(L"game");CreateDirectoryW(game.c_str(),nullptr);
    assert(dlssopti::conflict(game,{}).empty());
    touch(game+L"\\dxgi.dll");assert(!dlssopti::conflict(game,{}).empty());            // ReShade or any other proxy: refused
    assert(dlssopti::conflict(game,{game+L"\\dxgi.dll"}).empty());                     // ours (in this game's applied record)
    DeleteFileW((game+L"\\dxgi.dll").c_str());touch(game+L"\\renodx-dlss.addon64");assert(!dlssopti::conflict(game,{}).empty());
    DeleteFileW((game+L"\\renodx-dlss.addon64").c_str());touch(game+L"\\nvngx_dlssnr.dll");assert(!dlssopti::conflict(game,{}).empty()); // someone else's runtime
    DeleteFileW((game+L"\\nvngx_dlssnr.dll").c_str());touch(game+L"\\YakuzaParless.asi");touch(game+L"\\dinput8.dll");assert(dlssopti::conflict(game,{}).empty()); // an ASI mod loader: allowed
    touch(game+L"\\OptiScaler.asi");assert(!dlssopti::conflict(game,{}).empty());                // OptiScaler under its ASI name: refused
    for(const wchar_t* n:{L"\\OptiScaler.asi",L"\\YakuzaParless.asi",L"\\dinput8.dll"})DeleteFileW((game+n).c_str());
    const wchar_t* zip=_wgetenv(L"BLUEASH_TEST_OPTI_ZIP");const wchar_t* nr=_wgetenv(L"BLUEASH_TEST_NR");
    if(zip&&nr){std::wstring comp=tmp(L"comp");assert(CopyFileW(zip,(comp+L".zip").c_str(),FALSE));std::wstring note;
        auto e=dlssopti::ensure(comp,&note);if(!e.empty())wprintf(L"ensure: %ls\n",e.c_str());assert(e.empty()&&dlssopti::ready(comp));
        std::wstring rt=tmp(L"nvngx_dlssnr.dll");assert(CopyFileW(nr,rt.c_str(),FALSE)&&dlssrt::check(rt).error.empty());
        // OptiScaler.ini from the build's own file, then install, then restore: the folder is back as it was.
        FILE* f=_wfopen((comp+L"\\OptiScaler.ini").c_str(),L"rb");std::string ini(70000,0);ini.resize(fread(&ini[0],1,ini.size(),f));fclose(f);
        std::string out=dlssnr::configure(ini,dlssnr::builtin(1),"witcher3.exe");std::wstring iniPath=tmp(L"OptiScaler.ini");f=_wfopen(iniPath.c_str(),L"wb");fwrite(out.data(),1,out.size(),f);fclose(f);
        touch(game+L"\\witcher3.exe");std::wstring rec=tmp(L"record");
        auto changes=dlssopti::plan(comp,game,rt,iniPath);assert(dlssapply::apply(rec,changes,nullptr).empty());
        assert(dlssrt::check(game+L"\\nvngx_dlssnr.dll").error.empty()&&dlssapply::sha256(game+L"\\dxgi.dll")==dlssopti::pins()[0].sha);
        assert(!gone(game+L"\\OptiScaler\\licenses\\LICENSE.GPL-3.0.txt"));
        assert(dlssapply::restore(rec,nullptr).empty());
        assert(gone(game+L"\\dxgi.dll")&&gone(game+L"\\OptiScaler")&&gone(game+L"\\nvngx_dlssnr.dll")&&gone(game+L"\\OptiScaler.ini")&&!gone(game+L"\\witcher3.exe"));
        wprintf(L"install + restore on a scratch folder: ok (%ls)\n",note.c_str());}
    else puts("BLUEASH_TEST_OPTI_ZIP / BLUEASH_TEST_NR not set: install cycle skipped");
    puts("DLSS OptiScaler tests passed");
}
