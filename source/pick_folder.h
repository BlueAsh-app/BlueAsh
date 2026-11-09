#pragma once
// Folder picker: the current Windows file dialog in folder mode (IFileOpenDialog + FOS_PICKFOLDERS), the same window
// Explorer uses, instead of the old SHBrowseForFolder tree. Empty result = cancelled or not available.
#include <windows.h>
#include <shobjidl.h>
#include <string>
inline std::wstring pickFolder(HWND owner,const wchar_t* title){std::wstring out;HRESULT co=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* d=nullptr;if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&d)))&&d){DWORD o=0;d->GetOptions(&o);d->SetOptions(o|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
        if(title){d->SetTitle(title);}if(SUCCEEDED(d->Show(owner))){IShellItem* it=nullptr;if(SUCCEEDED(d->GetResult(&it))&&it){PWSTR p=nullptr;if(SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH,&p))&&p){out=p;CoTaskMemFree(p);}it->Release();}}d->Release();}
    if(SUCCEEDED(co)){CoUninitialize();}return out;}
