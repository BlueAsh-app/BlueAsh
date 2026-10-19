// Windows-only integration check; source/raid_deploy_tests.ps1 builds with the required ABI/libraries.
#define NOMINMAX
#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <cassert>
#include "raid_deploy_read.h"
int wmain(int argc,wchar_t** argv){
 if(argc!=2){std::cerr<<"Pass a deployment screenshot path\n";return 2;}
 ULONG_PTR gdip=0;Gdiplus::GdiplusStartupInput startup;Gdiplus::GdiplusStartup(&gdip,&startup,nullptr);
 HRESULT init=RoInitialize(RO_INIT_MULTITHREADED);assert(SUCCEEDED(init));
 {
 Gdiplus::Bitmap input(argv[1]);assert(input.GetLastStatus()==Gdiplus::Ok);winocr::Reader ocr;assert(ocr.open());
 for(int W:{int(input.GetWidth()),2560,1920,1280}){
  int H=int(double(W)*input.GetHeight()/input.GetWidth());Gdiplus::Bitmap frame(W,H,PixelFormat32bppARGB);
  {Gdiplus::Graphics g(&frame);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.DrawImage(&input,0,0,W,H);}
  auto b=raiddeploy::box(W,H);Gdiplus::Rect rect(b.x,b.y,b.w,b.h);Gdiplus::BitmapData data={};assert(frame.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)==Gdiplus::Ok);
  ULONGLONG before=GetTickCount64();auto r=raiddeploy::read(ocr,(const unsigned char*)data.Scan0,data.Stride,b,H);
  std::cout<<W<<"x"<<H<<": ready="<<r.ready<<" ms="<<r.ms<<" cost="<<GetTickCount64()-before<<"ms text codes:";for(wchar_t c:r.text)std::cout<<" "<<std::hex<<int(c)<<std::dec;std::cout<<"\n";
  frame.UnlockBits(&data);if(!r.ready||r.ms!=7247)return 1;
 }
 }
 RoUninitialize();Gdiplus::GdiplusShutdown(gdip);std::cout<<"PASS: real deployment screenshot Windows OCR at four resolutions\n";return 0;
}
