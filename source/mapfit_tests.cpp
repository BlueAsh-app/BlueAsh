#include <windows.h>
#include <cassert>
#include <cstdio>
#include <string>
#define PUBGMAP_NO_CAPTURE
#include "mapfit.h"
#include "pubg_map.h"
// Synthetic terrain (no image files): a map is found at full view and zoomed in, and a different terrain is told apart.
mapfit::Gray terrain(unsigned seed){mapfit::Gray g;g.w=g.h=273;g.p.resize(273*273);
    for(int y=0;y<273;y++)for(int x=0;x<273;x++){double v=0;unsigned s=seed;for(int k=1;k<=6;k++){s=s*1103515245+12345;double fx=(s%97)/97.0*0.09*k,fy=(s/97%89)/89.0*0.09*k,ph=(s/9000%628)/100.0;v+=std::sin(x*fx+y*fy+ph)*60/k;}
        g.p[size_t(y)*273+x]=float(110+v);}
    return g;}
mapfit::Gray screen(const mapfit::Gray& m,double ox,double oy,double S){mapfit::Gray s;s.w=960;s.h=540;s.p.assign(960*540,10);
    for(int y=0;y<540;y++)for(int x=0;x<960;x++){double u=(x-ox)/S,v=(y-oy)/S;if(u<0||v<0||u>1||v>1)continue;float val=m.sample(u*272,v*272);
        if(std::fabs(u*8-std::round(u*8))*S/8<0.6||std::fabs(v*8-std::round(v*8))*S/8<0.6){val*=0.35f;}s.p[size_t(y)*960+x]=val;}
    return s;}
int main(){setvbuf(stdout,nullptr,_IONBF,0);
    std::vector<mapfit::Gray> refs={mapfit::blur(terrain(1),2),mapfit::blur(terrain(2),2)};std::vector<int> cells={8,8};
    struct Case{int map;double ox,oy,S;};Case cases[]={{0,210,0,540},{0,-400,-300,1500},{1,150,40,470}};
    for(auto& c:cases){auto s=screen(terrain(c.map+1),c.ox,c.oy,c.S);auto g=mapfit::grid(s);assert(g.ok&&std::fabs(g.d-c.S/8)<1);
        auto f=mapfit::fit(mapfit::blur(s,2),g,refs,cells);printf("map %d ox %.1f oy %.1f S %.1f score %.2f\n",f.map,f.ox,f.oy,f.S,f.score);
        assert(f.ok&&f.map==c.map&&std::fabs(f.ox-c.ox)<c.S*0.01&&std::fabs(f.oy-c.oy)<c.S*0.01&&std::fabs(f.S-c.S)<c.S*0.01);}
    // Zoom 3x around the cursor, then drag: followed from the last placement.
    {double cx=500,cy=200;mapfit::Fit last;last.ok=true;last.ox=210;last.oy=0;last.S=540;last.map=0;
     double u=(cx-210)/540,v=(cy-0)/540,S=1620,ox=cx-u*S,oy=cy-v*S;auto s=screen(terrain(1),ox,oy,S);auto g=mapfit::grid(s);
     auto t=mapfit::track(mapfit::blur(s,2),g,refs[0],8,last,cx,cy,cx,cy);printf("track ox %.1f oy %.1f S %.1f (true %.1f %.1f %.1f)\n",t.ox,t.oy,t.S,ox,oy,S);
     assert(t.ok&&std::fabs(t.ox-ox)<3&&std::fabs(t.oy-oy)<3&&std::fabs(t.S-S)<S*0.01);
     auto s2=screen(terrain(1),ox-60,oy+40,S);auto g2=mapfit::grid(s2);auto t2=mapfit::track(mapfit::blur(s2,2),g2,refs[0],8,t,cx,cy,cx-60,cy+40);
     assert(t2.ok&&std::fabs(t2.ox-(ox-60))<3&&std::fabs(t2.oy-(oy+40))<3);}
    // Deep in, the short side crosses only two lines: its comb locks onto half the spacing or fails; both spacings are kept.
    {std::vector<double> rx(960,0),ry(540,0);for(int x=50;x<960;x+=216)rx[x]=10;for(int y=100;y<540;y+=216)ry[y]=10;auto g=mapfit::gridFrom(rx,ry,540);
     printf("few lines d %.1f d2 %.1f\n",g.d,g.d2);assert(g.ok&&(std::fabs(g.d-216)<1||std::fabs(g.d2-216)<1));}
    // Lost deep in (a 100 m grid, a tenth of the map on screen): found again around the last placement with the detailed map,
    // 300 m off where the cursor was.
    {mapfit::Gray t=terrain(1);mapfit::Gray8 u8;u8.w=u8.h=1092;u8.p.resize(1092*1092);for(int y=0;y<1092;y++)for(int x=0;x<1092;x++){unsigned h=unsigned(x*73856093u)^unsigned(y*19349663u);h^=h>>13;h*=0x5bd1e995u;h^=h>>15;u8.p[size_t(y)*1092+x]=(unsigned char)std::clamp(t.sample(x/4.0,y/4.0)+float(int(h%61)-30),0.0f,255.0f);}mapfit::blur8(u8); // texture: real maps have it, the sine terrain alone is the same one cell over
     double S=5400,ox=-2300,oy=-1900;mapfit::Gray s;s.w=960;s.h=540;s.p.assign(960*540,10);
     for(int y=0;y<540;y++)for(int x=0;x<960;x++){double u=(x-ox)/S,v=(y-oy)/S;float val=u8.sample(u*1091,v*1091);if(std::fabs(u*80-std::round(u*80))*S/80<0.6||std::fabs(v*80-std::round(v*80))*S/80<0.6){val*=0.5f;}s.p[size_t(y)*960+x]=val;}
     auto g=mapfit::grid(s);printf("deep grid ok %d d %.2f d2 %.2f px %.2f py %.2f (true cell %.2f)\n",g.ok,g.d,g.d2,g.px,g.py,S/80);mapfit::Refs rs{&refs[0],nullptr,&u8};mapfit::Fit prior;prior.ok=true;prior.ox=210;prior.oy=0;prior.S=540;prior.map=0;
     double cu=(480-ox)/S+0.3/8,cv=(270-oy)/S-0.2/8; // cursor then: 300 m east, 200 m north of today's centre
     auto n=mapfit::nearby(mapfit::blur(s,2),g,rs,8,8,prior,210+cu*540,cv*540,480,270);printf("nearby ox %.1f oy %.1f S %.1f score %.2f (true %.1f %.1f %.1f)\n",n.ox,n.oy,n.S,n.score,ox,oy,S);
     assert(g.ok&&n.ok&&std::fabs(n.S-S)<S*0.01&&std::fabs((480-n.ox)/n.S-(480-ox)/S)*8000<15&&std::fabs((270-n.oy)/n.S-(270-oy)/S)*8000<15);}
    // An empty screen (map closed) has no grid.
    mapfit::Gray blank;blank.w=960;blank.h=540;blank.p.assign(960*540,40);assert(!mapfit::grid(blank).ok);
    // Map key from GameUserSettings.ini text.
    auto k=pubgmap::mapKeys("(ActionName=\"ToggleMap\",Keys=((Key=M))),(ActionName=\"SetMapMarkerAtCurrentPosition\",Keys=((Key=Insert)))");assert(k.size()==1&&k[0].vk=='M'&&!k[0].ctrl);
    k=pubgmap::mapKeys("(ActionName=\"ToggleMap\",Keys=((Key=Tab,bCtrl=True),(Key=F2)))");assert(k.size()==2&&k[0].vk==VK_TAB&&k[0].ctrl&&k[1].vk==VK_F2);
    assert(pubgmap::mapKeys("nothing").at(0).vk=='M');
    printf("Map fit / map key tests passed\n");
}
