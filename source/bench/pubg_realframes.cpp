// Build: g++ -std=c++17 -O2 -I source source/bench/pubg_realframes.cpp -o realframes.exe
// Run from the repo: realframes.exe <dir with fail-NN.bmp/.txt> . prior|whole fail-00 fail-01 ...
// Real PUBG map frames saved by BlueAsh (%LOCALAPPDATA%\ModeSwitch\pubg-map: 4K green channel + the state then) matched
// with mapfit::deepFit as the worker does: 'prior' follows from the frame's last good placement and cursors, 'whole'
// searches the whole map. Needs relay/public/pubg/Erangel.u8 (source/build-pubg-hires.ps1). 2026-10-18 frames: all 11
// with a right prior found at 0.76..0.96 (runner-up <= 0.26), 3 with a wrong prior rejected; whole map: 12/12, 0.5..1.3 s.
#include <cstring>
#include <string>
#include <chrono>
#include "mapfit.h"
using namespace mapfit;
static std::string readAll(const std::string& p){std::string s;FILE* f=fopen(p.c_str(),"rb");if(!f)return s;char b[1<<16];size_t n;while((n=fread(b,1,sizeof b,f))>0)s.append(b,n);fclose(f);return s;}
int main(int argc,char** argv){std::string dir=argv[1],repo=argv[2];bool whole=std::string(argv[3])=="whole";Pyr P;P.L.resize(1);{std::string b=readAll(repo+"/relay/public/pubg/Erangel.u8");P.L[0].w=P.L[0].h=4096;P.L[0].p.assign(b.begin(),b.end());blur8(P.L[0]);}pyramid(P);
 for(int a=4;a<argc;a++){std::string n=argv[a],b=readAll(dir+"/"+n+".bmp"),t=readAll(dir+"/"+n+".txt");Gray full;full.w=3840;full.h=2160;full.p.resize(size_t(3840)*2160);for(size_t i=0;i<full.p.size();i++)full.p[i]=(unsigned char)b[1078+i];
  int ok,m;Fit pr;unsigned long long age;long gcx,gcy,cx,cy;sscanf(strstr(t.c_str(),"good "),"good %d %d %lf %lf %lf %llu",&ok,&m,&pr.ox,&pr.oy,&pr.S,&age);pr.ok=ok;sscanf(strstr(t.c_str(),"goodCursor "),"goodCursor %ld %ld",&gcx,&gcy);sscanf(strstr(t.c_str(),"cursor "),"cursor %ld %ld",&cx,&cy);
  Gray small=shrink(full,4),bl=blur(small,2);Grid c,g;gridFullBoth(full,4,c,g);double X=(cx-1.5)/4,Y=(cy-1.5)/4,GX=(gcx-1.5)/4,GY=(gcy-1.5)/4;
  std::vector<Anchor> an={{(small.w/2-pr.ox)/pr.S,(small.h/2-pr.oy)/pr.S,small.w/2.0,small.h/2.0},{(X-pr.ox)/pr.S,(Y-pr.oy)/pr.S,X,Y},{(GX-pr.ox)/pr.S,(GY-pr.oy)/pr.S,X,Y}};
  auto t0=std::chrono::steady_clock::now();Fit f=deepFit(bl,g,P,8,0,whole?nullptr:&pr,an,whole);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
  printf("%s ok%d S=%8.1f score %.3f 2nd %.3f centre u=%.4f v=%.4f %6.0f ms\n",n.c_str(),f.ok,f.S,f.score,f.second,(small.w/2-f.ox)/f.S,(small.h/2-f.oy)/f.S,ms);fflush(stdout);}}
