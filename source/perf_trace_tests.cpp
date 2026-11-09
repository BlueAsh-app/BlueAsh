// End-to-end check of the FPS measurement: this process presents a D3D11 swap chain at a steady pace (about 100 per
// second, no V-Sync) on another thread while perftrace::measure() records it through ETW. Needs administrator rights
// (or Performance Log Users); without them it checks the "needs administrator" result instead.
#include <cassert>
#include <cstdio>
#include <thread>
#include <d3d11.h>
#include "perf_trace.h"
int main(){
    WNDCLASSW wc={};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"PerfTraceTest";RegisterClassW(&wc);
    HWND w=CreateWindowW(L"PerfTraceTest",L"t",WS_OVERLAPPEDWINDOW,-3000,0,320,240,nullptr,nullptr,wc.hInstance,nullptr);ShowWindow(w,SW_SHOWNOACTIVATE);
    DXGI_SWAP_CHAIN_DESC d={};d.BufferCount=2;d.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;d.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;d.OutputWindow=w;d.SampleDesc.Count=1;d.Windowed=TRUE;d.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain* sc=nullptr;ID3D11Device* dev=nullptr;ID3D11DeviceContext* ctx=nullptr;
    if(FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&d,&sc,&dev,nullptr,&ctx))){puts("no D3D11 device: skipped");return 0;}
    std::atomic<bool> run{true};timeBeginPeriod(1);
    std::thread t([&]{LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);LONGLONG next=a.QuadPart;
        while(run){next+=f.QuadPart/100;do{QueryPerformanceCounter(&b);}while(b.QuadPart<next);sc->Present(0,0);sc->Present(0,DXGI_PRESENT_TEST);}}); // TEST presents must not count
    std::atomic<bool> stop{false};int ticks=0;
    auto r=perftrace::measure(GetCurrentProcessId(),12,nullptr,[]{return true;},[&](int){ticks++;},stop);run=false;t.join();
    if(r.needAdmin){assert(!r.error.empty());puts("trace needs administrator: end-to-end part skipped");return 0;}
    assert(r.error.empty());auto s=perf::stats(r.ms);printf("presents %d, %.1f s, avg %.1f FPS, 1%% low %.1f, %.2f +/- %.2f ms, vsync %d, ticks %d\n",s.frames,s.seconds,s.fps,s.low1,s.avgMs,s.sdMs,r.vsync,ticks);
    assert(s.frames>800&&s.fps>90&&s.fps<110&&r.vsync==0&&perf::enough(s)&&ticks>0);
    sc->Release();ctx->Release();dev->Release();DestroyWindow(w);puts("perf trace end-to-end test passed");}
