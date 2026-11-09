#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "dlss_gpu.h"
// NVML is only on PCs with the NVIDIA driver: the reading is printed when present, and a missing nvml.dll must not crash.
int main(){
    assert(dlssgpu::blackwell("NVIDIA GeForce RTX 5080")&&dlssgpu::blackwell("NVIDIA GeForce RTX 5090 D")&&!dlssgpu::blackwell("NVIDIA GeForce RTX 4090")&&!dlssgpu::blackwell("NVIDIA GeForce RTX 3080"));
    assert(dlssgpu::driverMajor("617.42")==617);
    dlssgpu::Nvml n;auto r=n.read();
    if(r.ok)printf("%s · driver %s · %u%% · %u C · %u W · %.1f / %.1f GB\n",r.name.c_str(),r.driver.c_str(),r.util,r.temp,r.watts,r.usedGB,r.totalGB);else puts("NVML not available here");
    puts("DLSS GPU tests passed");
}
