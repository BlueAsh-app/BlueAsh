#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "dlss_runtime.h"
// Real-file checks run only when BLUEASH_TEST_NR names a signed nvngx_dlssnr.dll (it is never in the repository);
// the file is copied to a temp folder first, the original is only read.
static std::wstring temp(const wchar_t* name){wchar_t t[MAX_PATH];GetTempPathW(MAX_PATH,t);std::wstring d=std::wstring(t)+L"blueash-nr-"+std::to_wstring(GetCurrentProcessId());CreateDirectoryW(d.c_str(),nullptr);return d+L"\\"+name;}
int main(){
    std::wstring fake=temp(L"nvngx_dlssnr.dll");FILE* f=_wfopen(fake.c_str(),L"wb");fputs("MZ not a dll",f);fclose(f);
    assert(!dlssrt::check(fake).error.empty());                         // not a PE, unsigned
    assert(!dlssrt::check(temp(L"other.dll")).error.empty());           // wrong name
    assert(!dlssrt::check(temp(L"missing\\nvngx_dlssnr.dll")).error.empty());
    const char* real=getenv("BLUEASH_TEST_NR");
    if(real){std::string r=real;std::wstring src(r.begin(),r.end()),copy=temp(L"good\\nvngx_dlssnr.dll");CreateDirectoryW(copy.substr(0,copy.find_last_of(L'\\')).c_str(),nullptr);assert(CopyFileW(src.c_str(),copy.c_str(),FALSE));
        auto ok=dlssrt::check(copy);assert(ok.error.empty()&&ok.signer==L"NVIDIA Corporation"&&ok.product==L"NVIDIA DLSSNR");printf("real runtime %s, models %s\n",ok.version.c_str(),ok.models_ab?"A+B":"A");
        // One byte changed in the middle: the signature no longer verifies.
        std::wstring bad=temp(L"bad\\nvngx_dlssnr.dll");CreateDirectoryW(bad.substr(0,bad.find_last_of(L'\\')).c_str(),nullptr);assert(CopyFileW(src.c_str(),bad.c_str(),FALSE));
        FILE* b=_wfopen(bad.c_str(),L"r+b");fseek(b,0,SEEK_END);long n=ftell(b);fseek(b,n/2,SEEK_SET);int c=fgetc(b);fseek(b,n/2,SEEK_SET);fputc(c^0x5A,b);fclose(b);
        assert(!dlssrt::check(bad).error.empty());puts("tampered copy refused");}
    else puts("BLUEASH_TEST_NR not set: signed-runtime cases skipped");
    puts("DLSS runtime tests passed");
}
