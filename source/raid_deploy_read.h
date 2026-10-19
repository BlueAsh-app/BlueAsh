#pragma once
#include "raid_deploy.h"
#include "winocr.h"
namespace raiddeploy {
struct Reading {bool ready=false;int ms=-1;std::wstring text;};
inline Reading read(winocr::Reader& ocr,const unsigned char* px,size_t pitch,const Box& b,int H){
 Reading r;auto timer=locate(px,pitch,b,H);r.ready=timer.ready;if(!r.ready)return r;
 // Fixed 60px-high glyphs and a generous margin keep the stylized zeroes inside OCR's text bounds.
 const double scales[][2]={{1,1},{0.5,0.4},{1.5,0.8}};
 for(auto& scale:scales){double sx=scale[0]*2160/H,sy=scale[1]*2160/H;int w=int(timer.w*sx),h=int(timer.h*sy),pad=24,iw=w+pad*2,ih=h+pad*2;
 std::vector<unsigned char> gray(size_t(iw)*ih,255);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  auto p=px+size_t(timer.y+int(y/sy))*pitch+(timer.x+int(x/sx))*4;
  int hi=std::max(p[0],std::max(p[1],p[2])),lo=std::min(p[0],std::min(p[1],p[2]));
  gray[size_t(y+pad)*iw+x+pad]=(lo>135&&hi-lo<85)?0:255;
 }
 auto text=ocr.read(gray.data(),iw,ih,400);r.ms=milliseconds(text);r.text+=text+L" | ";if(r.ms>=0)break;
 }return r;
}
}
