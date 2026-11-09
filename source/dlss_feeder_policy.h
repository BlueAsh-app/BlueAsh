#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cctype>
// Planning only: support here means the required Feeder layout is known, not that the game has been tested.
namespace feeder {
enum class Api {Unknown,D3D9,D3D10,D3D11,D3D12,Vulkan,OpenGL};
enum class Arch {Unknown,X86,X64};
struct Route {bool supported=false,host64=false,wrapper=false;std::string addon;};
inline Arch architecture(const std::vector<unsigned char>& pe){
    if(pe.size()<64||pe[0]!='M'||pe[1]!='Z')return Arch::Unknown;
    uint32_t o=(uint32_t)pe[60]|((uint32_t)pe[61]<<8)|((uint32_t)pe[62]<<16)|((uint32_t)pe[63]<<24);
    if(o>pe.size()||pe.size()-o<6||pe[o]!='P'||pe[o+1]!='E'||pe[o+2]||pe[o+3])return Arch::Unknown;
    unsigned machine=pe[o+4]|(unsigned(pe[o+5])<<8);
    return machine==0x14c?Arch::X86:machine==0x8664?Arch::X64:Arch::Unknown;
}
inline Route route(Api api,Arch arch){
    if(api==Api::Unknown||api==Api::Vulkan||api==Api::OpenGL||arch==Arch::Unknown)return {};
    if(api==Api::D3D12&&arch==Arch::X86)return {};
    if(arch==Arch::X86)return {true,true,api==Api::D3D9,"dlss5-feed.addon32"};
    if(api==Api::D3D10)return {true,true,false,"dlss5-feed-helper.addon64"};
    return {true,false,api==Api::D3D9,"dlss5-feed.addon64"};
}
// Core files only; shader headers, motion-vector shaders, textures, presets and any API
// wrappers must also be resolved before a complete installation may be offered.
struct File {std::string component,target;};
inline std::vector<File> layout(const Route& r){
    if(!r.supported)return {};
    std::vector<File> out={{r.host64&&r.addon=="dlss5-feed.addon32"?"reshade32":"reshade64","dxgi.dll"},
        {r.addon,r.addon},{"feeder-shader","reshade-shaders/Shaders/DLSS5_Feed.fx"}};
    if(r.wrapper)out.push_back({r.host64?"dgvoodoo32":"dgvoodoo64","d3d9.dll"});
    std::string prefix=r.host64?"host64/":"";
    if(r.host64){out.push_back({"feeder-host","host64/dlss5-feed-host64.exe"});out.push_back({"reshade64","host64/dxgi.dll"});}
    out.push_back({"neural-consumer",prefix+"renodx-dlss5.addon64"});
    out.push_back({"dlss-runtime",prefix+"nvngx_dlss.dll"});
    out.push_back({"neural-runtime",prefix+"nvngx_dlssnr.dll"});
    return out;
}
inline bool containsTarget(const std::vector<File>& files,const std::string& target){
    for(const auto& f:files){if(f.target==target)return true;}return false;
}
inline bool validLayout(const std::vector<File>& files){
    std::vector<std::string> seen;
    for(const auto& f:files){
        if(f.component.empty()||f.target.empty()||f.target[0]=='/'||f.target[0]=='\\')return false;
        std::string normalized,part;
        for(char c:f.target){
            if(c=='/'||c=='\\'){if(part.empty()||part=="."||part=="..")return false;normalized+=part+'/';part.clear();}
            else {if(c==':'||c=='\0')return false;part+=(char)std::tolower((unsigned char)c);}
        }
        if(part.empty()||part=="."||part==".."){return false;}normalized+=part;
        for(const auto& old:seen){if(old==normalized)return false;}seen.push_back(normalized);
    }
    return !files.empty();
}
}
