// Build: g++ -std=c++17 -O2 -I source source/bench/pubg_deepzoom.cpp -o bench.exe   Run from the repo: bench.exe <dir with <Map>.8192> [v]
// (<Map>.8192 = 8192x8192 green bytes of the official map: powershell -File source/bench/dump8192.ps1 <png dir> <out dir> Erangel,Miramar,Taego,Sanhok;
// the PNGs are downloaded by source/build-pubg-hires.ps1 into %TEMP%\blueash-pubg-src). Needs relay/public/pubg (build-pubg-hires.ps1).
// Offline benchmark for PUBG map matching at deep zoom: frames rendered from the official 8192 px maps (green), with the
// game's grid lines, contrast change, noise, a tinted area and white markers; matched as the BlueAsh worker does.
#include <windows.h>
#include <cstdio>
#include <string>
#include <random>
#include <chrono>
#include "mapfit.h"
using namespace mapfit;
static std::string readAll(const std::string& p){std::string s;FILE* f=fopen(p.c_str(),"rb");if(!f)return s;char b[1<<16];size_t n;while((n=fread(b,1,sizeof b,f))>0)s.append(b,n);fclose(f);return s;}
struct Src {int w=8192;std::string p;float at(int x,int y)const{x=std::clamp(x,0,w-1);y=std::clamp(y,0,w-1);return (unsigned char)p[size_t(y)*w+x];}
  float sample(double x,double y)const{int x0=(int)std::floor(x),y0=(int)std::floor(y);double fx=x-x0,fy=y-y0;return float((at(x0,y0)*(1-fx)+at(x0+1,y0)*fx)*(1-fy)+(at(x0,y0+1)*(1-fx)+at(x0+1,y0+1)*fx)*fy);}};
const int W=3840,H=2160,F=4;const double h0=(F-1)/2.0;
// Frame: map drawn at size Sf (full px) with top-left (ox,oy) in full px.
Gray render(const Src& s,int cells,double ox,double oy,double Sf,std::mt19937& rng,bool tint){Gray g;g.w=W;g.h=H;g.p.resize(size_t(W)*H);
  std::uniform_real_distribution<double> u(0,1);double gam=0.85+u(rng)*0.3,con=0.85+u(rng)*0.3,bri=-10+u(rng)*20;std::normal_distribution<double> noise(0,6);
  double c1=Sf/cells,c100=c1/10;double spp=Sf/s.w; // screen px per source px
  for(int y=0;y<H;y++)for(int x=0;x<W;x++){double mu=(x+0.5-ox)/Sf*s.w-0.5,mv=(y+0.5-oy)/Sf*s.w-0.5;double v;
    if(mu<0||mv<0||mu>s.w-1||mv>s.w-1)v=40;else{ if(spp<1){double r=0.5/spp;v=(s.sample(mu-r,mv-r)+s.sample(mu+r,mv-r)+s.sample(mu-r,mv+r)+s.sample(mu+r,mv+r))/4;}else v=s.sample(mu,mv);}
    v=255*std::pow(std::max(0.0,v)/255,gam)*con+bri;
    if(tint&&x>W/2)v=v*0.8+25;
    auto online=[&](double pos,double org,double cell,double wpx){double k=(pos-org)/cell;double d=std::fabs(k-std::round(k))*cell;return d<wpx;};
    if(online(x+0.5,ox,c1,1.0)||online(y+0.5,oy,c1,1.0))v*=0.55;else if(c100>30&&(online(x+0.5,ox,c100,0.6)||online(y+0.5,oy,c100,0.6)))v*=0.72;
    v+=noise(rng);g.p[size_t(y)*W+x]=(float)std::clamp(v,0.0,255.0);}
  // white markers / pings
  for(int k=0;k<5;k++){int cx=int(u(rng)*W),cy=int(u(rng)*H),r=10+int(u(rng)*25);for(int y=std::max(0,cy-r);y<std::min(H,cy+r);y++)for(int x=std::max(0,cx-r);x<std::min(W,cx+r);x++)g.p[size_t(y)*W+x]=240;}
  return g;}
Gray loadRef(const std::string& p,int n,int r){std::string b=readAll(p);Gray g;g.w=g.h=n;g.p.resize(size_t(n)*n);for(size_t i=0;i<g.p.size();i++)g.p[i]=(unsigned char)b[i];return blur(g,r);}
int main(int argc,char** argv){
  std::string repo="./",bench=argc>1?argv[1]:".";
  struct M {const char* id;int cells;double km;};M maps[]={{"Erangel",8,8},{"Miramar",8,8},{"Taego",8,8},{"Sanhok",4,4}};
  std::mt19937 rng(7);
  for(auto& m:maps){Src s;s.p=readAll(bench+"/"+m.id+".8192");if(s.p.size()!=8192u*8192){printf("missing %s\n",m.id);continue;}
    Gray lo=loadRef(repo+"maps/pubg/refs/"+m.id+".gray",273,2),hi=loadRef(repo+"maps/pubg/refs/"+m.id+".hi.gray",819,1);
    Gray8 ul;{std::string b=readAll(repo+"relay/public/pubg/"+m.id+".u8");ul.w=ul.h=4096;ul.p.assign(b.begin(),b.end());blur8(ul);}
    std::vector<Gray> R={lo},Hh={hi};std::vector<int> C={m.cells};std::vector<const Gray8*> U={&ul};
    printf("== %s\n",m.id);
    for(double screenM:{1000.0,600.0,400.0,250.0,180.0}){
      int okOld=0,okNew=0,badOld=0,badNew=0,n=0;double msOld=0,msNew=0;int nOk[2]={0,0},nBad[2]={0,0};double nMs[2]={0,0};
      for(int t=0;t<20;t++){double Sf=H*m.km*1000/screenM;std::uniform_real_distribution<double> u(0.1,0.9);
        double cu=u(rng),cv=u(rng);double ox=W/2-cu*Sf,oy=H/2-cv*Sf;if(Sf<=H){ox=(W-Sf)/2+(u(rng)-0.5)*200;oy=(H-Sf)/2;}
        Gray full=render(s,m.cells,ox,oy,Sf,rng,t%3==0);Gray small=shrink(full,F);Grid coarse,g;gridFullBoth(full,F,coarse,g);Gray bl=blur(small,2);
        double tS=Sf/F,tox=(ox-h0)/F,toy=(oy-h0)/F;n++;
        for(int mode=0;mode<2;mode++){auto t0=std::chrono::steady_clock::now();
          auto both=[&](const Grid& gg){return fit(bl,gg,R,C,0.45,&Hh,0,mode?&U:nullptr);};Fit a=both(g),b=both(coarse);Fit r=(b.ok&&(!a.ok||b.score>a.score))?b:a;
          double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t0).count();
          // error at screen centre, metres
          double px=bl.w/2.0,py=bl.h/2.0;double eu=(px-r.ox)/std::max(1e-9,r.S)-(px-tox)/tS,ev=(py-r.oy)/std::max(1e-9,r.S)-(py-toy)/tS;double err=std::hypot(eu,ev)*m.km*1000;
          bool good=r.ok&&err<20,bad=r.ok&&err>=20;
          if(mode){okNew+=good;badNew+=bad;msNew+=ms;}else{okOld+=good;badOld+=bad;msOld+=ms;}
          // nearby: prior = whole view placement (known), zoom centre drifted up to 1 km
          {std::uniform_real_distribution<double> e(-1000,1000);double du=e(rng)/(m.km*1000),dv=e(rng)/(m.km*1000);Fit prior;prior.ok=true;prior.S=540;prior.ox=(960-540)/2.0;prior.oy=0;prior.map=0;
           double lx=prior.ox+(cu+du)*prior.S,ly=prior.oy+(cv+dv)*prior.S;Refs rs{&lo,&hi,mode?&ul:nullptr};auto t1=std::chrono::steady_clock::now();
           Fit q=nearby(bl,g,rs,m.cells,m.km,prior,lx,ly,bl.w/2.0,bl.h/2.0);Fit q2=nearby(bl,coarse,rs,m.cells,m.km,prior,lx,ly,bl.w/2.0,bl.h/2.0);if(q2.ok&&(!q.ok||q2.score>q.score))q=q2;
           nMs[mode]+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t1).count();
           double eu2=(px-q.ox)/std::max(1e-9,q.S)-(px-tox)/tS,ev2=(py-q.oy)/std::max(1e-9,q.S)-(py-toy)/tS;double err2=std::hypot(eu2,ev2)*m.km*1000;nOk[mode]+=q.ok&&err2<20;nBad[mode]+=q.ok&&err2>=20;
           if(argc>2&&mode&&!(q.ok&&err2<20))printf("   nearby miss %.0fm ok%d score %.2f second %.2f err %.0fm truth %.2f truthHi %.2f grid %d d %.2f d2 %.1f trueCell %.2f | lattice dx %.1f dy %.1f score %.2f | px %.1f py %.1f\n",screenM,q.ok,q.score,q.second,err2,score(bl,rs,tox,toy,tS,48,27),score(bl,hi,tox,toy,tS,48,27),(int)g.ok,g.d,g.d2,tS/(m.cells*10),tox-(g.px+std::round((tox-g.px)/g.d)*g.d),toy-(g.py+std::round((toy-g.py)/g.d)*g.d),score(bl,rs,g.px+std::round((tox-g.px)/g.d)*g.d,g.py+std::round((toy-g.py)/g.d)*g.d,(g.d2>0?g.d2:g.d)*m.cells*10,48,27),g.px,g.py);}
          if(mode&&!good&&argc>2)printf("   miss %.0fm u%.2f v%.2f ok%d score %.2f second %.2f err %.0fm grid %d d%.1f\n",screenM,cu,cv,r.ok,r.score,r.second,err,g.ok,g.d);}
      }
      printf("screen %5.0f m: global old %2d/%d wrong %d | global new %2d/%d wrong %d | nearby old %2d wrong %d | nearby new %2d wrong %d (%.0f ms)\n",screenM,okOld,n,badOld,okNew,n,badNew,nOk[0],nBad[0],nOk[1],nBad[1],nMs[1]/n);fflush(stdout);}
  }
}
