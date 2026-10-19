#pragma once
#include "datapack_id.h"
#include <atomic>
#include <mutex>
// Data pack (source/build-datapack.ps1): the Fandom map pictures, markers and text translations and the PUBG reference
// maps and labels are not open source, so they are neither in git nor inside the exe. BlueAsh downloads the pack from
// the relay once (in the background after start), accepts it only with the SHA-256 this build pins (datapack_id.h),
// keeps it in %LOCALAPPDATA%\ModeSwitch\data\<sha12>\ and copies its files into the runtime folder at the paths the
// helpers have always read (maps/..., companion/FandomText.tsv). Until it is there those features wait: the map says it
// is still receiving its picture, the PUBG overlay tries again later.
namespace datapack {
inline std::mutex guard;inline std::wstring liveRoot;inline std::atomic<bool> busy{false};inline std::atomic<ULONGLONG> lastTry{0};
inline std::wstring cacheDir(){
 wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){return L"";}
 std::wstring f=DATAPACK_FILE;return std::wstring(local)+L"\\ModeSwitch\\data\\"+f.substr(9,12); // datapack-<sha12>.bin
}
inline bool ready(){auto d=cacheDir();return !d.empty()&&GetFileAttributesW((d+L"\\.complete").c_str())!=INVALID_FILE_ATTRIBUTES;}
inline std::string sha256(const std::string& b){
 BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;unsigned char d[32]={};std::string hex;
 if(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)!=0){return hex;}
 if(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)==0){if(BCryptHashData(h,(PUCHAR)b.data(),(ULONG)b.size(),0)==0&&BCryptFinishHash(h,d,32,0)==0){char x[3];for(unsigned char c:d){snprintf(x,3,"%02x",c);hex+=x;}}BCryptDestroyHash(h);}
 BCryptCloseAlgorithmProvider(a,0);return hex;
}
// "BAPK1\n", "<path>\t<size>\n" per file, an empty line, then the bytes. Paths stay inside dir.
inline bool unpack(const std::string& b,const std::wstring& dir){
 if(b.compare(0,6,"BAPK1\n")!=0){return false;}size_t at=6;std::vector<std::pair<std::string,size_t>> list;
 for(;;){size_t e=b.find('\n',at);if(e==std::string::npos){return false;}std::string line=b.substr(at,e-at);at=e+1;if(line.empty()){break;}
  size_t t=line.find('\t');if(t==std::string::npos){return false;}list.push_back({line.substr(0,t),(size_t)strtoull(line.c_str()+t+1,nullptr,10)});}
 for(auto& f:list){
  if(f.first.empty()||f.first.find("..")!=std::string::npos||f.first.find(':')!=std::string::npos||f.first[0]=='/'||at+f.second>b.size()){return false;}
  std::wstring rel=widen(f.first);for(auto& c:rel){if(c==L'/'){c=L'\\';}}
  for(size_t s=rel.find(L'\\');s!=std::wstring::npos;s=rel.find(L'\\',s+1)){CreateDirectoryW((dir+L"\\"+rel.substr(0,s)).c_str(),nullptr);}
  HANDLE h=CreateFileW((dir+L"\\"+rel).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE){return false;}
  DWORD wrote=0;bool ok=WriteFile(h,b.data()+at,(DWORD)f.second,&wrote,nullptr)&&wrote==f.second;CloseHandle(h);if(!ok){return false;}at+=f.second;
 }
 return at==b.size();
}
// Copies the cached pack into a runtime folder once (a marker file says it is done).
inline void install(const std::wstring& root){
 if(root.empty()||!ready()){return;}std::lock_guard<std::mutex> hold(guard);auto src=cacheDir();std::wstring marker=root+L"\\.datapack-"+src.substr(src.size()-12);
 if(GetFileAttributesW(marker.c_str())!=INVALID_FILE_ATTRIBUTES){return;}
 bool ok=true;std::function<void(const std::wstring&)> walk=[&](const std::wstring& rel){
  WIN32_FIND_DATAW f;HANDLE find=FindFirstFileW((src+rel+L"\\*").c_str(),&f);if(find==INVALID_HANDLE_VALUE){return;}
  do{std::wstring n=f.cFileName;if(n==L"."||n==L".."||n==L".complete"){continue;}std::wstring r=rel+L"\\"+n;
   if(f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){CreateDirectoryW((root+r).c_str(),nullptr);walk(r);}
   else if(!CopyFileW((src+r).c_str(),(root+r).c_str(),FALSE)){ok=false;}}while(FindNextFileW(find,&f));FindClose(find);};
 walk(L"");if(ok){HANDLE h=CreateFileW(marker.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);}}
}
// Downloads, checks and caches the pack, then installs it into the runtime folder in use. Runs on its own thread.
inline void fetch(){
 if(ready()){std::wstring root;{std::lock_guard<std::mutex> hold(guard);root=liveRoot;}install(root);return;}
 bool expected=false;if(!busy.compare_exchange_strong(expected,true)){return;}lastTry=GetTickCount64();
 std::string body;HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;DWORD status=0,size=sizeof(status);
 if(s){WinHttpSetTimeouts(s,10000,10000,30000,30000);c=WinHttpConnect(s,updateHost,INTERNET_DEFAULT_HTTPS_PORT,0);}
 std::wstring path=std::wstring(L"/data/")+DATAPACK_FILE;if(c){r=WinHttpOpenRequest(c,L"GET",path.c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE);}
 if(r&&WinHttpSendRequest(r,nullptr,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)&&status==200){
  std::vector<char> buf(1<<16);DWORD n=0;while(WinHttpReadData(r,buf.data(),(DWORD)buf.size(),&n)&&n&&body.size()<40u*1024*1024){body.append(buf.data(),n);}}
 for(HINTERNET h:{r,c,s}){if(h){WinHttpCloseHandle(h);}}
 auto dir=cacheDir();
 if(!dir.empty()&&!body.empty()&&sha256(body)==DATAPACK_SHA256){
  std::wstring stage=dir+L".tmp";removeTree(stage);SHCreateDirectoryExW(nullptr,stage.c_str(),nullptr);
  if(unpack(body,stage)){HANDLE h=CreateFileW((stage+L"\\.complete").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h!=INVALID_HANDLE_VALUE){CloseHandle(h);}
   removeTree(dir);if(!MoveFileExW(stage.c_str(),dir.c_str(),0)){removeTree(stage);}}else{removeTree(stage);}
 }
 busy=false;std::wstring root;{std::lock_guard<std::mutex> hold(guard);root=liveRoot;}install(root);
}
// Called with the runtime folder before helpers start (bundle.h): installs the pack, or starts fetching it (again after
// 10 minutes when an earlier try failed).
inline void use(const std::wstring& root){
 {std::lock_guard<std::mutex> hold(guard);liveRoot=root;}
 if(ready()){install(root);return;}
 if(!busy&&(!lastTry||GetTickCount64()-lastTry>600000)){std::thread(fetch).detach();}
}
}
void datapackUse(const std::wstring& root){if(bundleBase.empty())datapack::use(root);} // not for a --selftest scratch folder
