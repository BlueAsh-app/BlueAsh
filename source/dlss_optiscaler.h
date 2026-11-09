#pragma once
// The community route BlueAsh can install: Dagherbou's OptiScaler DLSS-NR build (GPL-3.0, a fork of OptiScaler;
// https://github.com/Dagherbou/OptiScaler_DLSSNR, release v0.2.0-patch1). It is fetched from the author's own GitHub
// release, never bundled, checked against the archive digest and a digest per file, unpacked with Windows' tar.exe and
// scanned with Microsoft Defender when present. A matching digest proves the bytes are the published ones, not that
// they are harmless: OptiScaler.dll and the forwarder carry no signature (the AMD / Intel / Microsoft libraries in the
// archive do), and the page says so. The NVIDIA runtime is never part of it — the person imports their own
// (dlss_runtime.h). One neural pass (this build has no multipass), frame generation off, WorkingScale at most 1.0.
#include <windows.h>
#include <winhttp.h>
#include "dlss_nr.h"
#include "dlss_apply.h"
#include "dlss_runtime.h"
namespace dlssopti {
const wchar_t* version=L"0.2.0-patch1";
const wchar_t* zipUrl=L"https://github.com/Dagherbou/OptiScaler_DLSSNR/releases/download/v0.2.0-patch1/OptiScaler-DLSSNR-v0.2.0-onimusha-fix.zip";
const char* zipSha="5db547216fa8a7dbd8ab0a193da1e3bce0ea4bd71f91189afa4ed2ede8bb9561";
const wchar_t* licenseUrl=L"https://raw.githubusercontent.com/Dagherbou/OptiScaler_DLSSNR/393e070/LICENSE";
const char* licenseSha="3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986";
const char* iniSha="27af5289887b90403eef25b15d8295b35c72e1a26d13e83acd0daa28129bfff6";
// Archive path -> where it goes beside the game's executable, and its digest.
struct Pin {const wchar_t* from;const wchar_t* to;const char* sha;};
inline const std::vector<Pin>& pins(){static const std::vector<Pin> p={
    {L"OptiScaler.dll",L"dxgi.dll","114a1d18e79d90a011406b9467b4b6dc4d4948487c4b30289311def311174bce"},
    {L"nvngx.dll_dlssnr.dll",L"nvngx.dll_dlssnr.dll","17cf51d25d142d0be15fcac945cc928c37849a1cb61011cb9c742d72df86647c"},
    {L"OptiScaler\\D3D12_OptiScaler\\D3D12Core.dll",L"OptiScaler\\D3D12_OptiScaler\\D3D12Core.dll","07d286c306f8117321422affd9e6388c12d0fb4be1c7fc689d9e899324feeb24"},
    {L"OptiScaler\\amd_fidelityfx_framegeneration_dx12.dll",L"OptiScaler\\amd_fidelityfx_framegeneration_dx12.dll","02297beedd285e822d3a64f314cf00faf378dcec0edc47ff0c4dd71b3a8c2f18"},
    {L"OptiScaler\\amd_fidelityfx_loader_dx12.dll",L"OptiScaler\\amd_fidelityfx_loader_dx12.dll","e2d85aa05a9bd9ed8b38935fdf5199372cca6f74c12015143bb6f945ee1608aa"},
    {L"OptiScaler\\amd_fidelityfx_upscaler_dx12.dll",L"OptiScaler\\amd_fidelityfx_upscaler_dx12.dll","d0dcccc74a43c44ba435b7a369b456e0970d8a4464e4bd683119b374f2c9fb46"},
    {L"OptiScaler\\amd_fidelityfx_vk.dll",L"OptiScaler\\amd_fidelityfx_vk.dll","a1624cc4238fef046f30c4d80ce3f47be63fc5f5373f49e3ee9edb9960f54c78"},
    {L"OptiScaler\\libxell.dll",L"OptiScaler\\libxell.dll","d2030dcd694fda8f2ec7e044b13e6db8f0b56d4ba9113a5efad334e3f3ded8c7"},
    {L"OptiScaler\\libxess.dll",L"OptiScaler\\libxess.dll","251659dd84a3e84de67c886a4186e01f3eca49b00641906fe38bb6b807e5d5b7"},
    {L"OptiScaler\\libxess_dx11.dll",L"OptiScaler\\libxess_dx11.dll","c7cfe86f0c9d94e4fb3696d3cd5035e2bbb6a8b1b0572f8b7395a4cdfd0c625e"},
    {L"OptiScaler\\libxess_fg.dll",L"OptiScaler\\libxess_fg.dll","ec5e0c65e075570c6ede72618bb666d0be0c2e10b2ea9762c0fe8cb8e375ab27"},
    {L"Licenses\\DirectX_LICENSE.txt",L"OptiScaler\\licenses\\DirectX_LICENSE.txt","5239850894610071566f7ecee0b751fde43c862032d92b99d7d0f596b3433ebd"},
    {L"Licenses\\FidelityFX_v2_LICENSE.md",L"OptiScaler\\licenses\\FidelityFX_v2_LICENSE.md","f0da09d71ad5c82759a179e774535d4a829e5c96c49294167c1152402b2cb400"},
    {L"Licenses\\RenoDX_ATTRIBUTION.txt",L"OptiScaler\\licenses\\RenoDX_ATTRIBUTION.txt","1d2f2b2f8cf5c9acfe2eaaa2d2c22face87bd19f7df85453079a28e7123ae727"},
    {L"Licenses\\XeSS_LICENSE.txt",L"OptiScaler\\licenses\\XeSS_LICENSE.txt","784be42c39a4a4d03cf85d88e15e59d75828fca773ce45b8dbfffbd7eaf208df"},
    {L"READ ME - DLSS Neural Rendering.txt",L"OptiScaler\\README-DLSSNR.txt","f535e30eea5bc251c1a61e1eab2191a578512b7f2174158014da964195ace451"},
    {L"LICENSE.GPL-3.0.txt",L"OptiScaler\\licenses\\LICENSE.GPL-3.0.txt","3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986"}};return p;}
// GET a public https URL into a file (redirects followed, at most maxBytes). Worker thread.
inline std::wstring download(const std::wstring& url,const std::wstring& path,ULONGLONG maxBytes,const std::function<void(int)>& progress={}){
    URL_COMPONENTS u={};u.dwStructSize=sizeof(u);wchar_t host[256]={},rest[2048]={},extra[1024]={};u.lpszHostName=host;u.dwHostNameLength=256;u.lpszUrlPath=rest;u.dwUrlPathLength=2048;u.lpszExtraInfo=extra;u.dwExtraInfoLength=1024;
    if(!WinHttpCrackUrl(url.c_str(),0,0,&u)||u.nScheme!=INTERNET_SCHEME_HTTPS)return L"주소가 올바르지 않습니다.";
    HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;std::wstring error;DWORD status=0,size=sizeof(status);
    if(s){WinHttpSetTimeouts(s,10000,10000,30000,60000);c=WinHttpConnect(s,host,u.nPort,0);}
    if(c)r=WinHttpOpenRequest(c,L"GET",(std::wstring(rest)+extra).c_str(),nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE); // path + ?query
    if(r){DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(r,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));}
    std::wstring tmp=path+L".part";FILE* f=nullptr;
    if(r&&WinHttpSendRequest(r,nullptr,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)&&status==200&&(f=_wfopen(tmp.c_str(),L"wb"))){
        wchar_t length[32]={};DWORD len=sizeof(length);ULONGLONG expected=WinHttpQueryHeaders(r,WINHTTP_QUERY_CONTENT_LENGTH,WINHTTP_HEADER_NAME_BY_INDEX,length,&len,WINHTTP_NO_HEADER_INDEX)?_wcstoui64(length,nullptr,10):0;int last=-1;
        std::vector<char> buf(1<<16);DWORD n=0;ULONGLONG total=0;while(WinHttpReadData(r,buf.data(),(DWORD)buf.size(),&n)&&n){total+=n;if(total>maxBytes){error=L"파일이 예상보다 큽니다.";break;}fwrite(buf.data(),1,n,f);if(progress&&expected){int p=std::min(34,5+(int)(std::min(total,expected)*29/expected));if(p!=last){last=p;progress(p);}}}
        if(fclose(f)!=0&&error.empty())error=L"파일을 저장하지 못했습니다.";}
    else error=status?L"내려받기 실패 (HTTP "+std::to_wstring(status)+L")":L"인터넷에 연결하지 못했습니다.";
    for(HINTERNET h:{r,c,s}){if(h)WinHttpCloseHandle(h);}
    if(error.empty()&&!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)){error=L"파일을 저장하지 못했습니다.";}if(!error.empty()){DeleteFileW(tmp.c_str());}return error;}
inline DWORD run(const std::wstring& exe,std::wstring args,DWORD timeoutMs){STARTUPINFOW si={};si.cb=sizeof(si);PROCESS_INFORMATION pi={};std::wstring line=L"\""+exe+L"\" "+args;
    if(!CreateProcessW(exe.c_str(),&line[0],nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){return 0xFFFFFFFF;}DWORD code=0xFFFFFFFE;
    if(WaitForSingleObject(pi.hProcess,timeoutMs)==WAIT_OBJECT_0){GetExitCodeProcess(pi.hProcess,&code);}else{TerminateProcess(pi.hProcess,1);}CloseHandle(pi.hThread);CloseHandle(pi.hProcess);return code;}
inline bool ready(const std::wstring& dir){for(auto& p:pins()){if(dlssapply::sha256(dir+L"\\"+p.from)!=p.sha)return false;}return dlssapply::sha256(dir+L"\\OptiScaler.ini")==iniSha;}
// Fetches and checks the build into dir (BlueAsh's cache). Returns "" when ready, else the reason. Worker thread.
inline std::wstring ensure(const std::wstring& dir,std::wstring* note=nullptr,const std::function<void(int)>& progress={}){
    if(ready(dir)){if(progress)progress(45);return L"";}CreateDirectoryW(dir.c_str(),nullptr);std::wstring zip=dir+L".zip";
    if(dlssapply::sha256(zip)!=zipSha){auto e=download(zipUrl,zip,400ull<<20,progress);if(!e.empty())return e;}if(progress)progress(35);
    if(dlssapply::sha256(zip)!=zipSha){DeleteFileW(zip.c_str());return L"받은 OptiScaler 파일이 공개된 파일과 다릅니다 (SHA-256 불일치).";}
    wchar_t sys[MAX_PATH]={};GetSystemDirectoryW(sys,MAX_PATH);DWORD x=run(std::wstring(sys)+L"\\tar.exe",L"-xf \""+zip+L"\" -C \""+dir+L"\"",120000);if(x!=0)return L"압축을 풀지 못했습니다 (tar "+std::to_wstring(x)+L").";if(progress)progress(40);
    std::wstring gpl=dir+L"\\LICENSE.GPL-3.0.txt";if(dlssapply::sha256(gpl)!=licenseSha){auto e=download(licenseUrl,gpl,1<<20);if(!e.empty())return e;}
    if(!ready(dir))return L"압축 안 파일이 공개된 파일과 다릅니다 (SHA-256 불일치).";
    // Defender: a second opinion, not proof. Exit 0 = nothing found, 2 = something found.
    std::wstring mp=[]{wchar_t pf[MAX_PATH]={};return GetEnvironmentVariableW(L"ProgramFiles",pf,MAX_PATH)?std::wstring(pf)+L"\\Windows Defender\\MpCmdRun.exe":std::wstring();}();
    if(!mp.empty()&&GetFileAttributesW(mp.c_str())!=INVALID_FILE_ATTRIBUTES){DWORD code=run(mp,L"-Scan -ScanType 3 -File \""+dir+L"\" -DisableRemediation",600000);
        if(code==2){return L"Microsoft Defender가 OptiScaler 파일에서 위협을 찾았습니다. 설치하지 않습니다.";}if(note)*note=code==0?L"Defender 검사 통과":L"Defender 검사 결과를 확인하지 못함";}
    else if(note){*note=L"Defender를 찾지 못해 검사하지 못함";}
    if(progress)progress(45);
    return L"";}
// Another graphics mod already in the folder (ReShade, another OptiScaler under any of its proxy names, a RenoDX add-on, a
// DLSS 5 runtime someone else placed): refused, never overwritten. Files listed in this game's own applied record are ours.
// A plain ASI loader (dinput8.dll + *.asi, e.g. Shin Ryu Mod Manager) is left alone: it never hooks the swap chain, and
// OptiScaler.ini keeps LoadAsiPlugins off, so the two load side by side.
inline std::wstring conflict(const std::wstring& exeDir,const std::vector<std::wstring>& ours){
    auto mine=[&](const std::wstring& n){for(auto& o:ours)if(!_wcsicmp(o.c_str(),(exeDir+L"\\"+n).c_str()))return true;return false;};
    for(const wchar_t* n:{L"dxgi.dll",L"winmm.dll",L"version.dll",L"dbghelp.dll",L"wininet.dll",L"winhttp.dll",L"d3d11.dll",L"d3d12.dll",L"OptiScaler.ini",L"OptiScaler.dll",L"OptiScaler.asi",L"nvngx.dll_dlssnr.dll",L"nvngx_dlssnr.dll",L"ReShade.ini"}){
        if(GetFileAttributesW((exeDir+L"\\"+n).c_str())!=INVALID_FILE_ATTRIBUTES&&!mine(n))return std::wstring(L"다른 도구가 넣은 파일이 있습니다: ")+n;}
    WIN32_FIND_DATAW f;for(const wchar_t* pat:{L"\\*.addon64",L"\\*.addon"}){HANDLE h=FindFirstFileW((exeDir+pat).c_str(),&f);if(h!=INVALID_HANDLE_VALUE){FindClose(h);return std::wstring(L"다른 모드 파일이 있습니다: ")+f.cFileName;}}
    return L"";}
// Every file of one install: the build's files, the person's runtime, and the OptiScaler.ini BlueAsh wrote (iniPath).
inline std::vector<dlssapply::Change> plan(const std::wstring& comp,const std::wstring& exeDir,const std::wstring& runtime,const std::wstring& iniPath){
    std::vector<dlssapply::Change> c;for(auto& p:pins())c.push_back({exeDir+L"\\"+p.to,comp+L"\\"+p.from,p.sha});
    c.push_back({exeDir+L"\\nvngx_dlssnr.dll",runtime,dlssapply::sha256(runtime)});c.push_back({exeDir+L"\\OptiScaler.ini",iniPath,dlssapply::sha256(iniPath)});return c;}
// The build's capture of real before/after frames, requested with its own trigger file while the game runs.
inline std::wstring readCapture(const std::wstring& exeDir,std::vector<unsigned char>& before,std::vector<unsigned char>& after,int& w,int& h){
    std::wstring dir=exeDir+L"\\dlssnr-capture";FILE* f=_wfopen((dir+L"\\manifest.txt").c_str(),L"rb");if(!f)return L"캡처 기록이 없습니다.";std::string m(4096,0);m.resize(fread(&m[0],1,m.size(),f));fclose(f);
    dlssnr::Frame fb,fa;if(!dlssnr::parseManifest(m,fb,fa)||fb.w!=fa.w||fb.h!=fa.h)return L"캡처 기록을 읽지 못했습니다.";
    auto load=[&](const wchar_t* name,std::vector<unsigned char>& raw){FILE* r=_wfopen((dir+L"\\"+name).c_str(),L"rb");if(!r)return false;fseek(r,0,SEEK_END);long n=ftell(r);fseek(r,0,SEEK_SET);if(n<=0||n>(512l<<20)){fclose(r);return false;}raw.resize(n);bool ok=fread(raw.data(),1,n,r)==(size_t)n;fclose(r);return ok;};
    std::vector<unsigned char> rb,ra;if(!load(L"before_00.raw",rb)||!load(L"after_00.raw",ra))return L"캡처한 프레임을 읽지 못했습니다.";
    double mean=dlssnr::linearHdr(fb.format)?dlssnr::meanLuma(fb,rb):0,scale=mean>0?0.18/mean:1;
    if(!dlssnr::toDisplay(fb,rb,scale,before)||!dlssnr::toDisplay(fa,ra,scale,after))return L"지원하지 않는 화면 형식입니다 (format "+std::to_wstring(fb.format)+L").";
    w=fb.w;h=fb.h;return L"";}
}
