// Windows-only integration check; source/raid_deploy_tests.ps1 builds with the required libraries.
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <iostream>
#include <string>
std::wstring hrtext(HRESULT h){return std::to_wstring((unsigned)h);}
#include "raid_deploy_capture.h"
static ULONGLONG start=0;static int alerts=0,remaining=-1;static bool panel=true;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 if(m==WM_APP+182){assert(w==37);alerts++;remaining=4000-int(GetTickCount64()-start);return 0;}
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;GetClientRect(h,&r);HBRUSH bg=CreateSolidBrush(RGB(14,18,23));FillRect(dc,&r,bg);DeleteObject(bg);int W=r.right,H=r.bottom;
  if(panel){RECT ready={W/2-H*11/100,H*53/100,W/2+H*11/100,H*57/100};HBRUSH red=CreateSolidBrush(RGB(210,8,8));FillRect(dc,&ready,red);DeleteObject(red);}
  int ms=std::max(0,4000-int(GetTickCount64()-start));wchar_t text[32];swprintf(text,32,L"00:%02d.%03d",ms/1000,ms%1000);
  HFONT f=CreateFontW(-H*3/100,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Consolas");auto old=SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(220,221,213));RECT digits={W/2-H*7/100,H*72/100,W/2+H*13/100,H*78/100};DrawTextW(dc,text,-1,&digits,DT_LEFT|DT_SINGLELINE);SelectObject(dc,old);DeleteObject(f);EndPaint(h,&ps);return 0;
 }return DefWindowProcW(h,m,w,l);
}
int main(){
 WNDCLASSW c={};c.lpfnWndProc=proc;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"BlueAshDeploymentCaptureQA";assert(RegisterClassW(&c));
 HWND h=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,c.lpszClassName,L"BlueAsh countdown verification",WS_POPUP,40,40,960,600,nullptr,nullptr,c.hInstance,nullptr);assert(h);ShowWindow(h,SW_SHOWNOACTIVATE);
 RaidDeployCapture capture;
 for(int pass=0;pass<3;pass++){
  alerts=0;panel=pass>0;start=GetTickCount64();InvalidateRect(h,nullptr,FALSE);UpdateWindow(h);capture.start(h,h,37);
  while(GetTickCount64()-start<4800){MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}if(pass==2&&GetTickCount64()-start>1800)capture.stop();InvalidateRect(h,nullptr,FALSE);Sleep(40);}
  capture.stop();auto reading=capture.reading();std::cout<<(panel?"deployment":"no red panel")<<": alerts="<<alerts<<" remaining="<<remaining<<" ready="<<reading.ready<<" ms="<<reading.ms<<" text codes:";for(wchar_t ch:reading.text)std::cout<<" "<<std::hex<<int(ch)<<std::dec;auto note=capture.read();char utf8[2048]={};WideCharToMultiByte(CP_UTF8,0,note.c_str(),-1,utf8,2048,nullptr,nullptr);std::cout<<" status="<<utf8<<"\n";
  if(alerts!=(pass==1?1:0)|| (pass==1&&(remaining>1000||remaining<0))){DestroyWindow(h);std::cerr<<"FAIL: real WGC countdown threshold\n";return 1;}
 }
 DestroyWindow(h);std::cout<<"PASS: real WGC / Windows OCR / posted alert / threshold / no-panel negative / stop\n";return 0;
}
