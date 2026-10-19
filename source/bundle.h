#pragma once
#include "i18n.h"
#include <shlobj.h>
// price/, companion/, maps/ and updater/ are embedded as RCDATA by build.sh (999 = "<build id>\n<path>..." list,
// 1000+ = files in list order) and unpacked once per build to %LOCALAPPDATA%\ModeSwitch\runtime\<build id>,
// so BlueAsh.exe works without its folders. Returns that directory, or empty with problem() on failure.
inline bool bundleResource(int id,const char*& data,DWORD& size){
 HMODULE m=GetModuleHandleW(nullptr);HRSRC r=FindResourceW(m,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));if(!r)return false;
 HGLOBAL g=LoadResource(m,r);if(!g)return false;data=(const char*)LockResource(g);size=SizeofResource(m,r);return data!=nullptr;
}
inline void removeTree(const std::wstring& path){std::wstring from=path+L'\0';SHFILEOPSTRUCTW op={};op.wFunc=FO_DELETE;op.pFrom=from.c_str();op.fFlags=FOF_NO_UI;SHFileOperationW(&op);}
inline std::wstring widen(const std::string& s){int n=MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),nullptr,0);std::wstring w(n,0);if(n)MultiByteToWideChar(CP_UTF8,0,s.data(),(int)s.size(),&w[0],n);return w;}
// Same bytes as the copy inside the exe? (read in blocks; a missing or locked file counts as different)
inline bool sameAsResource(const std::wstring& path,const char* data,DWORD n){
 HANDLE h=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size={};bool same=GetFileSizeEx(h,&size)&&size.QuadPart==n;char block[65536];DWORD done=0;
 while(same&&done<n){DWORD want=std::min<DWORD>(sizeof(block),n-done),got=0;if(!ReadFile(h,block,want,&got,nullptr)||got!=want||memcmp(block,data+done,got)!=0)same=false;done+=got;}
 CloseHandle(h);return same;
}
// Tamper check before every helper start: each unpacked file is compared with the exe's own copy and rewritten
// when it differs, so an edited or swapped script/DLL in the runtime folder is never run. A file a running
// helper keeps open cannot be rewritten: then nothing is started.
inline bool verifyBundle(const std::wstring& root){
 const char* data=nullptr;DWORD size=0;if(!bundleResource(999,data,size))return false;std::string list(data,size);size_t at=0;int i=0;
 while(at<list.size()){size_t end=list.find('\n',at);if(end==std::string::npos)end=list.size();auto line=list.substr(at,end-at);at=end+1;if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
  if(i++==0){continue;}
  std::wstring rel=widen(line);for(auto& c:rel)if(c==L'/')c=L'\\';
  const char* file=nullptr;DWORD n=0;if(!bundleResource(999+i-1,file,n))return false;auto path=root+L"\\"+rel;
  if(sameAsResource(path,file,n))continue;
  auto temp=path+L".restore";HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
  DWORD wrote=0;bool ok=WriteFile(h,file,n,&wrote,nullptr)&&wrote==n;CloseHandle(h);
  if(!ok||!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)){DeleteFileW(temp.c_str());return false;}
 }
 return true;
}
std::wstring unpackBundle();
void datapackUse(const std::wstring& root); // datapack.h: the downloaded data files go into the same folder
// Where runtime\<build id> goes: %LOCALAPPDATA%\ModeSwitch normally, a scratch folder for --selftest (so a release check
// never touches, or prunes, the folder the running BlueAsh uses).
std::wstring bundleBase;
std::wstring bundleRoot(){
 auto root=unpackBundle();if(root.empty())return root;
 if(!verifyBundle(root)){problem(i18n::text(L"내장 파일이 바뀌어 있어 원본으로 되돌리지 못했습니다. BlueAsh를 다시 실행하세요."));return L"";}
 datapackUse(root);return root;
}
std::wstring unpackBundle(){
 static std::wstring cached;if(!cached.empty())return cached;
 const char* data=nullptr;DWORD size=0;if(!bundleResource(999,data,size)){problem(i18n::text(L"내장 파일이 없는 빌드입니다. build.sh로 다시 빌드하세요."));return L"";}
 std::vector<std::string> lines;std::string list(data,size);size_t at=0;
 while(at<list.size()){size_t end=list.find('\n',at);if(end==std::string::npos)end=list.size();auto line=list.substr(at,end-at);if(!line.empty()&&line.back()=='\r')line.pop_back();if(!line.empty())lines.push_back(line);at=end+1;}
 if(lines.size()<2){problem(i18n::text(L"내장 파일 목록 오류"));return L"";}
 wchar_t local[MAX_PATH]={};if(FAILED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,local))){problem(i18n::text(L"AppData 폴더를 찾지 못했습니다."));return L"";}
 std::wstring base=bundleBase.empty()?std::wstring(local)+L"\\ModeSwitch":bundleBase,runtime=base+L"\\runtime",root=runtime+L"\\"+widen(lines[0]);
 if(GetFileAttributesW((root+L"\\.complete").c_str())!=INVALID_FILE_ATTRIBUTES)return cached=root;
 CreateDirectoryW(base.c_str(),nullptr);CreateDirectoryW(runtime.c_str(),nullptr);
 // Unpack into a staging folder and rename it, so a crash never leaves a half-written version in use.
 std::wstring stage=root+L".tmp";removeTree(stage);if(!CreateDirectoryW(stage.c_str(),nullptr)){problem(i18n::text(L"내장 파일 폴더를 만들지 못했습니다."));return L"";}
 for(size_t i=1;i<lines.size();i++){
  if(lines[i].find("..")!=std::string::npos||lines[i].find(':')!=std::string::npos){removeTree(stage);problem(i18n::text(L"내장 파일 경로 오류"));return L"";}
  std::wstring rel=widen(lines[i]);for(auto& c:rel)if(c==L'/')c=L'\\';
  for(size_t s=rel.find(L'\\');s!=std::wstring::npos;s=rel.find(L'\\',s+1))CreateDirectoryW((stage+L"\\"+rel.substr(0,s)).c_str(),nullptr);
  const char* file=nullptr;DWORD n=0;bool ok=bundleResource(999+(int)i,file,n);
  HANDLE h=ok?CreateFileW((stage+L"\\"+rel).c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr):INVALID_HANDLE_VALUE;
  DWORD wrote=0;ok=h!=INVALID_HANDLE_VALUE&&WriteFile(h,file,n,&wrote,nullptr)&&wrote==n;if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);
  if(!ok){removeTree(stage);problem(i18n::text(L"내장 파일을 풀지 못했습니다: ")+rel);return L"";}
 }
 HANDLE done=CreateFileW((stage+L"\\.complete").c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(done!=INVALID_HANDLE_VALUE)CloseHandle(done);
 if(!MoveFileExW(stage.c_str(),root.c_str(),0)){removeTree(stage);if(GetFileAttributesW((root+L"\\.complete").c_str())==INVALID_FILE_ATTRIBUTES){problem(i18n::text(L"내장 파일 설치 실패"));return L"";}}
 // Drop other builds' folders; one still in use simply survives until next start.
 WIN32_FIND_DATAW f;HANDLE find=FindFirstFileW((runtime+L"\\*").c_str(),&f);
 if(find!=INVALID_HANDLE_VALUE){do{std::wstring n=f.cFileName;if((f.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&n!=L"."&&n!=L".."&&runtime+L"\\"+n!=root)removeTree(runtime+L"\\"+n);}while(FindNextFileW(find,&f));FindClose(find);}
 return cached=root;
}
