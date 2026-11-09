#pragma once
// AI 번역기 (모드 번역): a small language model run on this PC's graphics card, so translating costs nothing and no text
// leaves the PC. llama.cpp's Vulkan build (MIT; NVIDIA, AMD and Intel cards, the processor when there is none) and
// Qwen3-4B-Instruct-2507 (Apache-2.0, 4-bit, 2.5 GB) are fetched once from their publishers, checked against pinned
// SHA-256, kept in %LOCALAPPDATA%\ModeSwitch\ai, and run hidden on 127.0.0.1 only while a translation runs (in a job
// object, so it ends with BlueAsh; an elevated BlueAsh starts it as the signed-in user, like the updater).
// Measured here (RTX 5080, Vulkan): load ~15 s, ~300 characters of mod text a second.
#include <functional>
#include <mutex>
#include <atomic>
namespace localai {
const wchar_t* llamaUrl=L"https://github.com/ggml-org/llama.cpp/releases/download/b12065/llama-b12065-bin-win-vulkan-x64.zip";
const char* llamaSha="80adeed888c397b252ce9677d237c426fd4f7317ee1f16a45ebcca5ce842c323";const ULONGLONG llamaSize=37554508;
const wchar_t* modelUrl=L"https://huggingface.co/unsloth/Qwen3-4B-Instruct-2507-GGUF/resolve/main/Qwen3-4B-Instruct-2507-Q4_K_M.gguf";
const char* modelSha="3605803b982cb64aead44f6c1b2ae36e3acdb41d8e46c8a94c6533bc4c67e597";const ULONGLONG modelSize=2497281120ull;
enum Error {Ok=0,Network=-10,Disk=-11,Start=-12,Mismatch=-13};
inline std::wstring dir(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+L"\\ModeSwitch";CreateDirectoryW(r.c_str(),nullptr);r+=L"\\ai";CreateDirectoryW(r.c_str(),nullptr);return r;}
inline std::wstring modelPath(){return dir()+L"\\Qwen3-4B-Instruct-2507-Q4_K_M.gguf";}
inline std::wstring serverPath(){return dir()+L"\\llama-b12065\\llama-server.exe";}
inline ULONGLONG fileSize(const std::wstring& p){WIN32_FILE_ATTRIBUTE_DATA a;if(!GetFileAttributesExW(p.c_str(),GetFileExInfoStandard,&a)){return 0;}return ((ULONGLONG)a.nFileSizeHigh<<32)|a.nFileSizeLow;}
// The 2.5 GB model is hashed once: <file>.ok keeps the hash with the size and time it was taken at.
inline std::wstring stamp(const std::wstring& p){WIN32_FILE_ATTRIBUTE_DATA a;if(!GetFileAttributesExW(p.c_str(),GetFileExInfoStandard,&a)){return L"";}wchar_t b[96];
    swprintf(b,96,L"%llu-%lu-%lu",((ULONGLONG)a.nFileSizeHigh<<32)|a.nFileSizeLow,a.ftLastWriteTime.dwHighDateTime,a.ftLastWriteTime.dwLowDateTime);return b;}
inline bool verified(const std::wstring& p,const char* sha,ULONGLONG size){if(fileSize(p)!=size){return false;}std::wstring want=std::wstring(sha,sha+strlen(sha))+L" "+stamp(p);
    std::string ok;std::wstring have;int enc;if(modtr::readBytes(p+L".ok",ok)&&modtr::decodeText(ok,have,enc)&&have==want){return true;}
    if(dlssapply::sha256(p)!=sha){return false;}modtr::writeBytes(p+L".ok",modtr::utf8(want));return true;}
// GET into path, going on from a partial file (Range) so a broken 2.5 GB download is not started over. progress(bytes so far).
inline int fetch(const std::wstring& url,const std::wstring& path,ULONGLONG size,const std::function<void(ULONGLONG)>& progress){
    std::wstring part=path+L".part";ULONGLONG have=fileSize(part);if(have>size){DeleteFileW(part.c_str());have=0;}
    URL_COMPONENTS u={};u.dwStructSize=sizeof(u);wchar_t host[256]={},rest[2048]={};u.lpszHostName=host;u.dwHostNameLength=256;u.lpszUrlPath=rest;u.dwUrlPathLength=2048;
    if(!WinHttpCrackUrl(url.c_str(),0,0,&u)||u.nScheme!=INTERNET_SCHEME_HTTPS){return Network;}
    HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;int result=Network;DWORD status=0,len=sizeof(status);
    if(s){WinHttpSetTimeouts(s,15000,15000,30000,60000);c=WinHttpConnect(s,host,u.nPort,0);}
    if(c){r=WinHttpOpenRequest(c,L"GET",rest,nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE);}
    if(r){DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(r,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));}
    std::wstring range=have?L"Range: bytes="+std::to_wstring(have)+L"-":L"";
    if(r&&WinHttpSendRequest(r,range.empty()?nullptr:range.c_str(),range.empty()?0:(DWORD)-1L,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&len,nullptr)&&(status==200||status==206)){
        if(status==200)have=0; // the server sent it whole
        HANDLE f=CreateFileW(part.c_str(),GENERIC_WRITE,0,nullptr,have?OPEN_EXISTING:CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(f!=INVALID_HANDLE_VALUE){LARGE_INTEGER at;at.QuadPart=(LONGLONG)have;SetFilePointerEx(f,at,nullptr,FILE_BEGIN);SetEndOfFile(f);std::vector<char> buf(1<<20);DWORD n=0,w=0;ULONGLONG done=have,shown=0;result=Ok;
            while(WinHttpReadData(r,buf.data(),(DWORD)buf.size(),&n)&&n){if(!WriteFile(f,buf.data(),n,&w,nullptr)||w!=n){result=Disk;break;}done+=n;if(done>size){result=Mismatch;break;}if(done-shown>=(4u<<20)){shown=done;progress(done);}}
            if(result==Ok&&done!=size){result=Network;}CloseHandle(f);}else{result=Disk;}}
    for(HINTERNET h:{r,c,s}){if(h){WinHttpCloseHandle(h);}}
    if(result==Ok&&!MoveFileExW(part.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)){result=Disk;}return result;}
// Both pieces present and checked; fetched first when missing. progress(bytes, of total) while downloading.
inline int ensure(const std::function<void(ULONGLONG,ULONGLONG)>& progress){
    static std::mutex one;std::lock_guard<std::mutex> hold(one); // mod translation and the map helper may both ask: one download at a time
    std::wstring d=dir(),model=modelPath(),zip=d+L"\\llama-b12065.zip",server=serverPath();ULONGLONG total=llamaSize+modelSize;
    bool haveServer=fileSize(server)>0&&verified(zip,llamaSha,llamaSize),haveModel=verified(model,modelSha,modelSize);if(haveServer&&haveModel){return Ok;}
    ULARGE_INTEGER free={};ULONGLONG need=(haveModel?0:modelSize-fileSize(model+L".part"))+(haveServer?0:llamaSize*5);if(GetDiskFreeSpaceExW(d.c_str(),&free,nullptr,nullptr)&&free.QuadPart<need+(200ull<<20)){return Disk;}
    if(!haveServer){if(!verified(zip,llamaSha,llamaSize)){int e=fetch(llamaUrl,zip,llamaSize,[&](ULONGLONG b){progress(b,total);});if(e){return e;}if(!verified(zip,llamaSha,llamaSize)){DeleteFileW(zip.c_str());return Mismatch;}}
        std::wstring out=d+L"\\llama-b12065";CreateDirectoryW(out.c_str(),nullptr);wchar_t sys[MAX_PATH]={};GetSystemDirectoryW(sys,MAX_PATH);
        if(dlssopti::run(std::wstring(sys)+L"\\tar.exe",L"-xf \""+zip+L"\" -C \""+out+L"\"",120000)!=0||fileSize(server)==0){return Start;}}
    if(!haveModel){int e=fetch(modelUrl,model,modelSize,[&](ULONGLONG b){progress(llamaSize+b,total);});if(e){return e;}if(!verified(model,modelSha,modelSize)){DeleteFileW(model.c_str());return Mismatch;}}
    progress(total,total);return Ok;}
// --- the running server -----------------------------------------------------------------------------------------------
struct Server {HANDLE proc=nullptr,job=nullptr;int port=0;};
inline std::string request(int port,const wchar_t* verb,const wchar_t* path,const std::string& body,DWORD& status,int wait){
    std::string out;status=0;HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;DWORD len=sizeof(status);
    if(s){WinHttpSetTimeouts(s,3000,3000,wait,wait);c=WinHttpConnect(s,L"127.0.0.1",(INTERNET_PORT)port,0);}
    if(c){r=WinHttpOpenRequest(c,verb,path,nullptr,nullptr,nullptr,0);}
    const wchar_t* type=body.empty()?nullptr:L"Content-Type: application/json; charset=utf-8";
    if(r&&WinHttpSendRequest(r,type,type?(DWORD)-1L:0,body.empty()?nullptr:(LPVOID)body.data(),(DWORD)body.size(),(DWORD)body.size(),0)&&WinHttpReceiveResponse(r,nullptr)){
        WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&len,nullptr);char buf[16384];DWORD n=0;while(WinHttpReadData(r,buf,sizeof(buf),&n)&&n&&out.size()<(8u<<20)){out.append(buf,n);}}
    for(HINTERNET h:{r,c,s}){if(h){WinHttpCloseHandle(h);}}return out;}
inline void stop(Server& s){if(s.proc){TerminateProcess(s.proc,0);WaitForSingleObject(s.proc,5000);CloseHandle(s.proc);s.proc=nullptr;}if(s.job){CloseHandle(s.job);s.job=nullptr;}s.port=0;}
// Started on a free port of a small range (one that already answers is skipped) and ready once /health answers. The model
// goes on the graphics card first; when that fails (too little video memory: a small card, or a game holding it) the
// processor runs it instead: slower, still translating. Any card with a Vulkan driver works; without one llama.cpp
// stays on the processor by itself.
inline int start(Server& s,const std::function<bool()>& stopped={}){
    std::wstring exe=serverPath(),wd=exe.substr(0,exe.find_last_of(L'\\'));int port=0;
    for(int p=51831;p<51851&&!port;p++){DWORD st;request(p,L"GET",L"/health",std::string(),st,500);if(!st){port=p;}}
    if(!port){return Start;}
    for(int layers:{99,0}){
        std::wstring args=L"\""+exe+L"\" -m \""+modelPath()+L"\" -ngl "+std::to_wstring(layers)+L" -c 8192 -np 1 --host 127.0.0.1 --port "+std::to_wstring(port)+L" --no-webui";
        PROCESS_INFORMATION pi={};bool ok;
        if(selfElevated()){ok=startAsShellUser(exe,args,wd,pi);if(ok){ResumeThread(pi.hThread);}} // never a user-folder program with administrator rights
        else{STARTUPINFOW si={};si.cb=sizeof(si);ok=CreateProcessW(exe.c_str(),&args[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,wd.c_str(),&si,&pi)!=FALSE;
            if(ok){s.job=CreateJobObjectW(nullptr,nullptr);JOBOBJECT_EXTENDED_LIMIT_INFORMATION li={};li.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                if(s.job){SetInformationJobObject(s.job,JobObjectExtendedLimitInformation,&li,sizeof(li));AssignProcessToJobObject(s.job,pi.hProcess);}ResumeThread(pi.hThread);}}
        if(!ok){return Start;}CloseHandle(pi.hThread);s.proc=pi.hProcess;s.port=port;
        for(int i=0;i<360;i++){if(WaitForSingleObject(s.proc,500)==WAIT_OBJECT_0){break;}if(stopped&&stopped()){stop(s);return Start;}
            DWORD st;request(port,L"GET",L"/health",std::string(),st,2000);if(st==200){return Ok;}}
        stop(s);} // did not come up on the card: the processor next
    return Start;}
// One chat turn; the model's text, or "" when the server did not answer.
inline std::wstring chat(const Server& s,const std::wstring& system,const std::wstring& user){
    std::string body=modtr::utf8(L"{\"messages\":[{\"role\":\"system\",\"content\":\""+modtr::jsonEscape(system)+L"\"},{\"role\":\"user\",\"content\":\""+modtr::jsonEscape(user)+L"\"}],\"temperature\":0.2,\"max_tokens\":3072}");
    DWORD st;std::string r=request(s.port,L"POST",L"/v1/chat/completions",body,st,180000);std::wstring t;if(st!=200||!modtr::fromUtf8(r,t)){return L"";}
    size_t k=t.find(L"\"content\"");if(k==std::wstring::npos){return L"";}size_t q=t.find(L'"',t.find(L':',k)+1);if(q==std::wstring::npos){return L"";}
    size_t e=q+1;while(e<t.size()&&t[e]!=L'"'){if(t[e]==L'\\')e++;e++;}return modtr::jsonUnescape(t.substr(q+1,e-q-1));}
// The map helper (Companion.ps1, quest and key cards) translates wiki text with the same model and server files, but the
// download and its checks live here: it posts WM_APP+207 and follows ai-progress.txt (state=working part=0..1 | done |
// error=<code>) in %LOCALAPPDATA%\ModeSwitch.
inline void fetchForHelper(){static std::atomic<bool> busy{false};if(busy.exchange(true)){return;}
    std::thread([]{std::wstring file=dir();file=file.substr(0,file.find_last_of(L'\\'))+L"\\ai-progress.txt";
        auto write=[&](const std::string& t){HANDLE h=CreateFileW(file.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,CREATE_ALWAYS,0,nullptr);if(h!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(h,t.data(),(DWORD)t.size(),&n,nullptr);CloseHandle(h);}};
        ULONGLONG last=0;write("state=working\npart=0\n");
        int e=ensure([&](ULONGLONG b,ULONGLONG t){ULONGLONG now=GetTickCount64();if(now-last<250){return;}last=now;char x[64];snprintf(x,64,"state=working\npart=%.4f\n",t?(double)b/t:0.0);write(x);});
        write(e?"state=error\nerror="+std::to_string(e)+"\n":std::string("state=done\n"));busy=false;}).detach();}
}
