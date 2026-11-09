#pragma once
// "DLSS 5 런타임 가져오기": the person picks an nvngx_dlssnr.dll they obtained; BlueAsh never ships or downloads one.
// Accepted only when: the name is nvngx_dlssnr.dll, it is an x64 PE, its Authenticode signature verifies
// (WinVerifyTrust), the signer is "NVIDIA Corporation", the product is "NVIDIA DLSSNR" and the version is 310.8 or
// newer (310.9+ carries models A and B). The same check runs again right before every install, so a file swapped on
// disk afterwards is refused. Whether the person may use that file in another game is not something a signature
// says; the page states this and asks them to confirm (see dlss_ui.h).
#include <windows.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <cstdio>
#include "dlss_nr.h"
namespace dlssrt {
struct Info {std::wstring error,signer,product;std::string version;bool models_ab=false;};
inline std::wstring signerName(const std::wstring& path){
    HCERTSTORE store=nullptr;HCRYPTMSG msg=nullptr;std::wstring name;
    if(!CryptQueryObject(CERT_QUERY_OBJECT_FILE,path.c_str(),CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,CERT_QUERY_FORMAT_FLAG_BINARY,0,nullptr,nullptr,nullptr,&store,&msg,nullptr))return name;
    DWORD size=0;if(CryptMsgGetParam(msg,CMSG_SIGNER_INFO_PARAM,0,nullptr,&size)&&size){std::vector<BYTE> buf(size);auto* si=(CMSG_SIGNER_INFO*)buf.data();
        if(CryptMsgGetParam(msg,CMSG_SIGNER_INFO_PARAM,0,si,&size)){CERT_INFO ci={};ci.Issuer=si->Issuer;ci.SerialNumber=si->SerialNumber;
            PCCERT_CONTEXT cert=CertFindCertificateInStore(store,X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,0,CERT_FIND_SUBJECT_CERT,&ci,nullptr);
            if(cert){wchar_t n[256]={};CertGetNameStringW(cert,CERT_NAME_SIMPLE_DISPLAY_TYPE,0,nullptr,n,256);name=n;CertFreeCertificateContext(cert);}}}
    if(store){CertCloseStore(store,0);}if(msg){CryptMsgClose(msg);}return name;}
inline bool trusted(const std::wstring& path){
    WINTRUST_FILE_INFO file={};file.cbStruct=sizeof(file);file.pcwszFilePath=path.c_str();WINTRUST_DATA d={};d.cbStruct=sizeof(d);d.dwUIChoice=WTD_UI_NONE;d.fdwRevocationChecks=WTD_REVOKE_NONE;
    d.dwUnionChoice=WTD_CHOICE_FILE;d.pFile=&file;d.dwStateAction=WTD_STATEACTION_VERIFY;GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    LONG r=WinVerifyTrust((HWND)INVALID_HANDLE_VALUE,&action,&d);d.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust((HWND)INVALID_HANDLE_VALUE,&action,&d);return r==0;}
inline bool x64(const std::wstring& path){FILE* f=_wfopen(path.c_str(),L"rb");if(!f)return false;IMAGE_DOS_HEADER dos={};IMAGE_NT_HEADERS64 nt={};bool ok=fread(&dos,sizeof(dos),1,f)==1&&dos.e_magic==IMAGE_DOS_SIGNATURE&&dos.e_lfanew>0&&dos.e_lfanew<(1<<20)
    &&fseek(f,dos.e_lfanew,SEEK_SET)==0&&fread(&nt,sizeof(nt),1,f)==1&&nt.Signature==IMAGE_NT_SIGNATURE&&nt.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64;fclose(f);return ok;}
inline std::wstring versionText(const std::wstring& path,const wchar_t* what){DWORD h=0,n=GetFileVersionInfoSizeW(path.c_str(),&h);if(!n||n>(1<<20))return L"";std::vector<BYTE> b(n);if(!GetFileVersionInfoW(path.c_str(),0,n,b.data()))return L"";
    if(!what){VS_FIXEDFILEINFO* v=nullptr;UINT l=0;if(!VerQueryValueW(b.data(),L"\\",(void**)&v,&l)||!v)return L"";wchar_t s[64];swprintf(s,64,L"%u.%u.%u.%u",HIWORD(v->dwFileVersionMS),LOWORD(v->dwFileVersionMS),HIWORD(v->dwFileVersionLS),LOWORD(v->dwFileVersionLS));return s;}
    struct Lang{WORD l,c;}*langs=nullptr;UINT len=0;if(!VerQueryValueW(b.data(),L"\\VarFileInfo\\Translation",(void**)&langs,&len)||!len)return L"";wchar_t q[96];swprintf(q,96,L"\\StringFileInfo\\%04x%04x\\%ls",langs[0].l,langs[0].c,what);
    wchar_t* v=nullptr;UINT vl=0;return VerQueryValueW(b.data(),q,(void**)&v,&vl)&&v?std::wstring(v):L"";}
inline Info check(const std::wstring& path){
    Info i;auto slash=path.find_last_of(L"\\/");std::wstring name=slash==std::wstring::npos?path:path.substr(slash+1);
    if(_wcsicmp(name.c_str(),L"nvngx_dlssnr.dll")){i.error=L"파일 이름이 nvngx_dlssnr.dll이 아닙니다.";return i;}
    if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES){i.error=L"파일을 찾지 못했습니다.";return i;}
    if(!x64(path)){i.error=L"64비트(x64) DLL이 아닙니다.";return i;}
    if(!trusted(path)){i.error=L"디지털 서명이 없거나 올바르지 않습니다 (변조됐을 수 있습니다).";return i;}
    i.signer=signerName(path);if(i.signer!=L"NVIDIA Corporation"){i.error=L"NVIDIA Corporation의 서명이 아닙니다: "+(i.signer.empty()?std::wstring(L"-"):i.signer);return i;}
    i.product=versionText(path,L"ProductName");while(!i.product.empty()&&i.product.back()==L' '){i.product.pop_back();}if(i.product!=L"NVIDIA DLSSNR"){i.error=L"DLSS 5 런타임(NVIDIA DLSSNR)이 아닙니다: "+i.product;return i;}
    std::wstring v=versionText(path,nullptr);i.version.assign(v.begin(),v.end());int a=0,b=0;std::sscanf(i.version.c_str(),"%d.%d",&a,&b);
    if(a<310||(a==310&&b<8)){i.error=L"너무 오래된 버전입니다 (310.8 이상 필요): "+v;return i;}
    i.models_ab=dlssnr::modelsAB(i.version);return i;}
}
