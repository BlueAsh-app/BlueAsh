#pragma once
// DLSS 5 file changes as one transaction: back up, write a journal, copy, verify, then record. A crash in the middle leaves
// pending.txt, and recover() puts every file back the next time. No game uses this yet: the verified DLSS 5 titles are
// switched on in the game's own menu, and no file route with signed, redistributable NVIDIA files has been verified (see
// AGENTS.md). It is here, tested, so such a route only has to describe its files.
#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
namespace dlssapply {
struct Change {std::wstring target,source;std::string sha256;}; // copy source over target; source must hash to sha256 (hex, lower)
inline int failAfter=-1; // tests: fail after this many copied files
inline std::string sha256(const std::wstring& path){
    HANDLE f=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(f==INVALID_HANDLE_VALUE)return "";
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE h=nullptr;unsigned char out[32]={};bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0&&BCryptCreateHash(alg,&h,nullptr,0,nullptr,0,0)==0;
    std::vector<unsigned char> buf(1<<16);DWORD n=0;while(ok&&ReadFile(f,buf.data(),(DWORD)buf.size(),&n,nullptr)&&n){ok=BCryptHashData(h,buf.data(),n,0)==0;}
    ok=ok&&BCryptFinishHash(h,out,32,0)==0;if(h)BCryptDestroyHash(h);if(alg)BCryptCloseAlgorithmProvider(alg,0);CloseHandle(f);if(!ok)return "";
    std::string s;char x[3];for(unsigned char c:out){sprintf(x,"%02x",c);s+=x;}return s;}
inline bool exists(const std::wstring& p){return GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES;}
// Journal lines: target \t backup ("-" = the target did not exist, so restoring deletes it). UTF-16 file.
inline bool writeJournal(const std::wstring& path,const std::vector<std::pair<std::wstring,std::wstring>>& rows){
    std::wstring tmp=path+L".tmp";FILE* f=_wfopen(tmp.c_str(),L"wb");if(!f)return false;for(auto& r:rows){std::wstring l=r.first+L"\t"+r.second+L"\n";fwrite(l.data(),2,l.size(),f);}
    bool ok=fflush(f)==0;fclose(f);return ok&&MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}
inline std::vector<std::pair<std::wstring,std::wstring>> readJournal(const std::wstring& path){
    std::vector<std::pair<std::wstring,std::wstring>> rows;FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return rows;std::wstring all;wchar_t c;while(fread(&c,2,1,f)==1)all+=c;fclose(f);
    size_t at=0;while(at<all.size()){size_t e=all.find(L'\n',at);if(e==std::wstring::npos)break;std::wstring l=all.substr(at,e-at);size_t t=l.find(L'\t');if(t!=std::wstring::npos)rows.push_back({l.substr(0,t),l.substr(t+1)});at=e+1;}return rows;}
// Puts the journal's files back (backups over targets, new files deleted). Returns false if any file could not be restored.
inline bool undo(const std::vector<std::pair<std::wstring,std::wstring>>& rows){bool ok=true;
    for(auto it=rows.rbegin();it!=rows.rend();++it){if(it->second==L"d"){RemoveDirectoryW(it->first.c_str());continue;}if(it->second==L"-"){if(exists(it->first)&&!DeleteFileW(it->first.c_str()))ok=false;}
        else if(!CopyFileW(it->second.c_str(),it->first.c_str(),FALSE))ok=false;}return ok;}
// dir: this game's record folder (journal, backups). running: is the game running now (checked before anything is touched).
inline std::wstring apply(const std::wstring& dir,const std::vector<Change>& changes,const std::function<bool()>& running,const std::function<void(int)>& progress={}){
    if(running&&running())return L"게임이 실행 중입니다. 게임을 끈 뒤 적용하세요.";
    if(exists(dir+L"\\pending.txt"))return L"이전 적용이 끝나지 않았습니다. 먼저 복구하세요.";
    if(exists(dir+L"\\applied.txt"))return L"이미 적용돼 있습니다. 원본으로 되돌린 뒤 다시 적용하세요.";
    for(size_t i=0;i<changes.size();i++){for(size_t j=0;j<i;j++)if(!_wcsicmp(changes[i].target.c_str(),changes[j].target.c_str()))return L"같은 파일을 두 번 바꾸려 했습니다.";
        if(changes[i].sha256.size()!=64||sha256(changes[i].source)!=changes[i].sha256)return L"적용할 파일의 무결성을 확인하지 못했습니다: "+changes[i].source;}
    CreateDirectoryW(dir.c_str(),nullptr);std::wstring backups=dir+L"\\backup";CreateDirectoryW(backups.c_str(),nullptr);
    std::vector<std::pair<std::wstring,std::wstring>> rows;std::vector<std::wstring> made;
    // Folders the files need that are not there yet (outermost first, journal row "d"): created after the journal is
    // written, removed again by undo when empty.
    for(auto& c:changes){std::vector<std::wstring> chain;std::wstring d=c.target.substr(0,c.target.find_last_of(L"\\/"));while(!d.empty()&&!exists(d)){chain.push_back(d);d=d.substr(0,d.find_last_of(L"\\/"));}
        for(auto it=chain.rbegin();it!=chain.rend();++it){bool seen=false;for(auto& m:made){seen=seen||!_wcsicmp(m.c_str(),it->c_str());}if(!seen){made.push_back(*it);rows.push_back({*it,L"d"});}}}
    for(size_t i=0;i<changes.size();i++){std::wstring b=L"-";if(exists(changes[i].target)){b=backups+L"\\"+std::to_wstring(i)+L".bak";if(!CopyFileW(changes[i].target.c_str(),b.c_str(),FALSE))return L"원본을 백업하지 못했습니다: "+changes[i].target;}rows.push_back({changes[i].target,b});}
    if(!writeJournal(dir+L"\\pending.txt",rows))return L"적용 기록을 쓰지 못했습니다.";
    std::wstring error;for(auto& m:made){if(!CreateDirectoryW(m.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS){error=L"폴더를 만들지 못했습니다: "+m;break;}}
    for(size_t i=0;i<changes.size()&&error.empty();i++){if(failAfter>=0&&(int)i>=failAfter){error=L"test: failure injected";break;}
        std::wstring tmp=changes[i].target+L".blueash-new";
        if(!CopyFileW(changes[i].source.c_str(),tmp.c_str(),FALSE)||!MoveFileExW(tmp.c_str(),changes[i].target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(tmp.c_str());error=L"파일을 바꾸지 못했습니다: "+changes[i].target;}
        else if(sha256(changes[i].target)!=changes[i].sha256)error=L"바꾼 파일이 예상과 다릅니다: "+changes[i].target;else if(progress)progress(55+(int)((i+1)*40/changes.size()));}
    if(!error.empty()){bool back=undo(rows);if(back)DeleteFileW((dir+L"\\pending.txt").c_str());return error+(back?L" · 원본으로 되돌렸습니다.":L" · 일부 파일을 되돌리지 못했습니다. BlueAsh를 다시 켜면 다시 시도합니다.");}
    if(!MoveFileExW((dir+L"\\pending.txt").c_str(),(dir+L"\\applied.txt").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){undo(rows);return L"적용 기록을 저장하지 못해 원본으로 되돌렸습니다.";}
    return L"";}
// The "원본 복구" button: puts back what apply() changed.
inline std::wstring restore(const std::wstring& dir,const std::function<bool()>& running){
    if(running&&running())return L"게임이 실행 중입니다. 게임을 끈 뒤 복구하세요.";
    auto rows=readJournal(dir+L"\\applied.txt");if(rows.empty())return L"되돌릴 변경이 없습니다.";
    if(!undo(rows)){return L"일부 파일을 되돌리지 못했습니다.";}DeleteFileW((dir+L"\\applied.txt").c_str());return L"";}
// At start: an apply that never finished (BlueAsh closed, PC off) is rolled back.
inline bool recover(const std::wstring& dir){auto rows=readJournal(dir+L"\\pending.txt");if(rows.empty())return true;if(!undo(rows))return false;DeleteFileW((dir+L"\\pending.txt").c_str());return true;}
}
