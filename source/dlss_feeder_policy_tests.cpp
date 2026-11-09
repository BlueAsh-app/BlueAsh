#include "dlss_feeder_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using namespace feeder;
    auto x=route(Api::D3D11,Arch::X64);
    assert(x.supported && !x.host64 && !x.wrapper && x.addon=="dlss5-feed.addon64");
    x=route(Api::D3D9,Arch::X86);
    assert(x.supported && x.host64 && x.wrapper && x.addon=="dlss5-feed.addon32");
    x=route(Api::D3D10,Arch::X64);
    assert(x.supported && x.host64 && !x.wrapper && x.addon=="dlss5-feed-helper.addon64");
    assert(!route(Api::Unknown,Arch::X64).supported);
    assert(!route(Api::D3D12,Arch::Unknown).supported);
    assert(!route(Api::Vulkan,Arch::X64).supported);
    std::vector<unsigned char> pe(256,0);pe[0]='M';pe[1]='Z';pe[0x3c]=0x80;
    pe[0x80]='P';pe[0x81]='E';pe[0x84]=0x4c;pe[0x85]=0x01;
    assert(architecture(pe)==Arch::X86);
    pe[0x84]=0x64;pe[0x85]=0x86;assert(architecture(pe)==Arch::X64);
    pe[0x80]=0;assert(architecture(pe)==Arch::Unknown);
    assert(architecture(std::vector<unsigned char>(3,0))==Arch::Unknown);
    auto p=layout(route(Api::D3D11,Arch::X64));
    assert(containsTarget(p,"dxgi.dll") && containsTarget(p,"dlss5-feed.addon64"));
    assert(containsTarget(p,"reshade-shaders/Shaders/DLSS5_Feed.fx"));
    assert(!containsTarget(p,"host64/dlss5-feed-host64.exe"));
    p=layout(route(Api::D3D9,Arch::X86));
    assert(containsTarget(p,"d3d9.dll") && containsTarget(p,"dlss5-feed.addon32"));
    assert(containsTarget(p,"host64/dlss5-feed-host64.exe"));
    assert(containsTarget(p,"host64/nvngx_dlssnr.dll"));
    assert(layout(route(Api::Unknown,Arch::X86)).empty());
    assert(validLayout(p));
    p.push_back({"unknown","../outside.dll"});assert(!validLayout(p));p.pop_back();
    p.push_back({"unknown","D3D9.DLL"});assert(!validLayout(p));
    puts("Feeder route policy tests passed");
}
