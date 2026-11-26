// g++ -std=c++17 -Wall -Wextra -Werror recoil_core_tests.cpp -o t.exe && ./t.exe
#include "recoil_core.h"
#include <cstdio>
#include <cstdlib>
#include <random>
using namespace recoil;
static int fails=0;
#define CHECK(c,msg) do{if(!(c)){printf("FAIL %s (line %d)\n",msg,__LINE__);fails++;}}while(0)
// A wall-like texture sampled at (x+ox, y+oy), bilinear, plus a fixed crosshair strip that never moves.
static Gray wall(int w,int h,double ox,double oy,bool cross){Gray g;g.w=w;g.h=h;g.p.resize(size_t(w)*h);
    auto f=[](double x,double y){return 128+40*std::sin(x*.21)*std::cos(y*.17)+30*std::sin(x*.053+y*.091)+20*std::cos(x*.37-y*.23);};
    for(int y=0;y<h;y++){for(int x=0;x<w;x++){double v=f(x+ox,y+oy);if(cross&&std::abs(x-w/2)<2)v=250;g.p[size_t(y)*w+x]=(uint8_t)std::clamp(v,0.0,255.0);}}return g;}
int main(){
    // 1. registration: integer and fractional moves, a crosshair strip that stays put, a guess far from the answer
    {struct C{double dx,dy,gx,gy;};C cs[]={{0,0,0,0},{3,-2,0,0},{7.4,5.6,0,0},{-12.3,9.25,-10,8},{24,-18,0,0},{-30,2,-28,0}};
     for(auto& c:cs){Gray a=wall(320,200,0,0,true),b=wall(320,200,-c.dx,-c.dy,true);auto pa=pyramid(a),pb=pyramid(b);Match m=match(pa,pb,c.gx,c.gy,8,160-6,160+6);
        char msg[96];snprintf(msg,96,"registration %.2f,%.2f -> %.2f,%.2f",c.dx,c.dy,m.dx,m.dy);CHECK(std::fabs(m.dx-c.dx)<.35&&std::fabs(m.dy-c.dy)<.35,msg);}}
    // 2. a spray: a short sideways look first (the camera follows the mouse at 0.8 px per count), then 2.5 s of fire where
    //    the gun climbs 300 px/s and the hand pulls only 80 % of it, the screen one frame behind the mouse
    {std::vector<Shift> s;std::vector<Mouse> m;double k=.8,t=0,dt=1/144.0;std::mt19937 rng(3);std::normal_distribution<double> noise(0,.15);std::vector<double> hx,hy;double carry=0;
     int pre=90,fire=360,post=40;for(int i=0;i<pre+fire+post;i++){t+=dt;long mx=0,my=0;bool f=i>=pre&&i<pre+fire;
        if(i<pre){mx=std::lround(40*std::sin(i*.12));my=std::lround(6*std::sin(i*.07));}
        else if(f){carry+=300*dt/k*.8;my=(long)carry;carry-=my;mx=std::lround(3*std::sin(i*.3));}
        if(i==pre){m.push_back({t-dt/2,0,0,1});}if(i==pre+fire){m.push_back({t-dt/2,0,0,-1});}m.push_back({t-dt/3,mx,my,0});hx.push_back(mx);hy.push_back(my);
        double camX=0,camY=0;size_t j=hx.size()>=2?hx.size()-2:0; // one frame late
        camX=k*hx[j];camY=k*hy[j]; // down + from the pull
        bool kick=i-1>=pre&&i-1<pre+fire;if(kick)camY-=300*dt;
        s.push_back({t,-camX+noise(rng),-camY+noise(rng)});}
     Result r=analyse(s,m,1.0);char msg[160];snprintf(msg,160,"spray ok=%d why=%s ratio=%.3f kV=%.3f kH=%.3f lag=%d",r.ok,r.why.c_str(),r.ratio,r.kV,r.kH,r.lag);
     CHECK(r.ok&&std::fabs(r.ratio-1.25)<.04,msg);CHECK(std::fabs(r.kV-.8)<.03&&r.lag==1,msg);CHECK(r.bursts==1,msg);
     CHECK(!r.laser.empty()&&std::fabs(r.laser.back().y)<std::fabs(r.aim.back().y)*.2,"the laser ends near where it started");
     CHECK(r.drift>0,"under-pulled: the crosshair ended above the start");}
    // 2b. a gentle sideways wobble at 165 fps (about 1 cm at 800 DPI split over many frames) is enough to measure
    {std::vector<Shift> s;std::vector<Mouse> m;double k=.8,t=0,dt=1/165.0;std::vector<double> hx,hy;double carry=0;int pre=90,fire=300;
     for(int i=0;i<pre+fire+20;i++){t+=dt;long mx=0,my=0;bool f=i>=pre&&i<pre+fire;if(i<pre)mx=std::lround(6*std::sin(i*.12));else if(f){carry+=300*dt/k*.8;my=(long)carry;carry-=my;}
        if(i==pre){m.push_back({t-dt/2,0,0,1});}if(i==pre+fire){m.push_back({t-dt/2,0,0,-1});}m.push_back({t-dt/3,mx,my,0});hx.push_back(mx);hy.push_back(my);
        size_t j=hx.size()>=2?hx.size()-2:0;double camX=k*hx[j],camY=k*hy[j];if(i-1>=pre&&i-1<pre+fire)camY-=300*dt;s.push_back({t,-camX,-camY});}
     Result r=analyse(s,m,1.0);char msg[160];snprintf(msg,160,"gentle wobble ok=%d why=%s kH=%.3f moved=%.0f",r.ok,r.why.c_str(),r.kH,r.movedH);CHECK(r.ok&&std::fabs(r.kH-.8)<.05,msg);}
    // 2c. the steady suggestion: the median of the newest five, one wild spray does not move it
    {Steady a=steady({.97,.93,.95,1.20,.94,.50});char msg[96];snprintf(msg,96,"steady %.3f spread %.3f n %d",a.value,a.spread,a.n);CHECK(a.n==5&&std::fabs(a.value-.95)<1e-9,msg);
     Steady b=steady({.97});CHECK(b.n==1&&b.value==.97&&b.spread==0,"one spray is its own value");CHECK(steady({}).n==0,"no sprays");}
    // 2d. three magazines in one capture: each burst pulls a different share; the capture's answer is their median
    {std::vector<Shift> s;std::vector<Mouse> m;double k=.8,t=0,dt=1/144.0;std::vector<double> hx,hy;double carry=0;const double share[3]={.9,.8,.7};int pre=90,burst=300,gap=150;
     int total=pre+3*(burst+gap);for(int i=0;i<total;i++){t+=dt;long mx=0,my=0;int b=i<pre?-1:(i-pre)/(burst+gap),in=i<pre?0:(i-pre)%(burst+gap);bool f=b>=0&&in<burst;
        if(i<pre)mx=std::lround(40*std::sin(i*.12));else if(f){carry+=300*dt/k*share[b];my=(long)carry;carry-=my;}
        if(b>=0&&in==0){m.push_back({t-dt/2,0,0,1});}if(b>=0&&in==burst){m.push_back({t-dt/2,0,0,-1});}m.push_back({t-dt/3,mx,my,0});hx.push_back(mx);hy.push_back(my);
        size_t j=hx.size()>=2?hx.size()-2:0;double camX=k*hx[j],camY=k*hy[j];int pb=i-1<pre?-1:(i-1-pre)/(burst+gap),pin=i-1<pre?0:(i-1-pre)%(burst+gap);if(pb>=0&&pin<burst)camY-=300*dt;s.push_back({t,-camX,-camY});}
     Result r=analyse(s,m,1.0);char msg[160];snprintf(msg,160,"bursts ok=%d n=%zu ratio=%.3f",r.ok,r.burstRatios.size(),r.ratio);CHECK(r.ok&&r.burstRatios.size()==3&&std::fabs(r.ratio-1/.8)<.04,msg);
     // each magazine in detail: under-pulled, so the crosshair sat above the start (vErr > 0), more so with less pull
     {CHECK(r.each.size()==3,"each n");bool rising=r.each.size()==3&&r.each[0].vErr>0&&r.each[2].vErr>r.each[0].vErr;snprintf(msg,160,"vErr %.1f %.1f",r.each.empty()?0:r.each[0].vErr,r.each.size()<3?0:r.each[2].vErr);CHECK(rising,msg);
      Summary sm=summarize(r.each);CHECK(sm.n==3&&sm.vSd>0&&sm.phase[1]>50&&sm.phase[1]<110,"summary");}}
    // 2e. shots: 3 bursts of 300 frames at 1/144 s ~ 2.08 s each; at 0.0857 s per shot that is 25 shots each
    {std::vector<Shift> s;std::vector<Mouse> m;double k=.8,t=0,dt=1/144.0;std::vector<double> hx,hy;double carry=0;int pre=90,burst=300,gap=150;
     int total=pre+3*(burst+gap);for(int i=0;i<total;i++){t+=dt;long mx=0,my=0;int b=i<pre?-1:(i-pre)/(burst+gap),in=i<pre?0:(i-pre)%(burst+gap);bool f=b>=0&&in<burst;
        if(i<pre)mx=std::lround(40*std::sin(i*.12));else if(f){carry+=300*dt/k*.8;my=(long)carry;carry-=my;}
        if(b>=0&&in==0){m.push_back({t-dt/2,0,0,1});}if(b>=0&&in==burst){m.push_back({t-dt/2,0,0,-1});}m.push_back({t-dt/3,mx,my,0});hx.push_back(mx);hy.push_back(my);
        size_t j=hx.size()>=2?hx.size()-2:0;double camX=k*hx[j],camY=k*hy[j];int pb=i-1<pre?-1:(i-1-pre)/(burst+gap),pin=i-1<pre?0:(i-1-pre)%(burst+gap);if(pb>=0&&pin<burst)camY-=300*dt;s.push_back({t,-camX,-camY});}
     Result r=analyse(s,m,1.0,.0857);char msg[160];int sh=r.each.empty()?0:r.each[0].shots;snprintf(msg,160,"shots %d break %d",sh,r.each.empty()?0:r.each[0].breakAt);
     CHECK(r.each.size()==3&&sh>=23&&sh<=26&&r.each[0].shot.size()==(size_t)sh,msg);CHECK(r.each[0].breakAt>1,msg);CHECK(fireInterval("M416")>.08&&fireInterval("Mk47 Mutant")==0,"fire interval table");}
    // 2f. the scope by what stays still: a cross that never moves over a moving texture is found, the texture is not;
    // a second run with the same cross matches, a different reticle does not
    {auto run=[&](int kind){StillMap sm;unsigned seed=1;std::vector<uint8_t> tex(256*256);for(auto& v:tex){seed=seed*1103515245+12345;v=(uint8_t)(60+(seed>>16)%120);}
        for(int f=0;f<40;f++){Gray g;g.w=48;g.h=48;g.p.resize(48*48);int ox=f*3,oy=f*2;for(int y=0;y<48;y++)for(int x=0;x<48;x++)g.p[y*48+x]=tex[((y+oy)%256)*256+(x+ox)%256];
            for(int i=8;i<40;i++){if(kind==0){g.p[24*48+i]=250;g.p[i*48+24]=250;}else{g.p[i*48+i]=250;g.p[i*48+(47-i)]=250;}}sm.add(g);}return sm.sig();};
     Mask a=run(0),b=run(0),c=run(1);int on=0;for(auto v:a.bits)on+=v;char msg[96];snprintf(msg,96,"still on=%d same=%.2f other=%.2f",on,same(a,b),same(a,c));
     CHECK(on>20&&on<400&&same(a,b)>.9&&same(a,c)<.3,msg);Mask rt=unhexMask(hex(a));CHECK(same(a,rt)>.99,"mask hex round trip");}
    // 2g. a hip-fire look (1.2 px per count) before an aimed-in sweep and spray (0.8): with the sweep window kV is the
    //     aimed one; the hand starts pulling 0.25 s late (the reaction), then pulls 80 % — the answer stays 1.25
    {std::vector<Shift> s;std::vector<Mouse> m;double t=0,dt=1/144.0;std::vector<double> hx,hy,kk;double carry=0,calA=0,calB=0;int hip=120,sweep=300,fire=360;
     for(int i=0;i<hip+sweep+fire+60;i++){t+=dt;long mx=0,my=0;double k=i<hip?1.2:.8;bool f=i>=hip+sweep&&i<hip+sweep+fire;
        if(i<hip){mx=std::lround(40*std::sin(i*.12));my=std::lround(30*std::sin(i*.09));}
        else if(i<hip+sweep){if(i==hip)calA=t;my=std::lround(14*std::sin((i-hip)*.06));calB=t;}
        else if(f&&i-(hip+sweep)>=36){carry+=300*dt/k*.8;my=(long)carry;carry-=my;}
        if(i==hip+sweep){m.push_back({t-dt/2,0,0,1});}if(i==hip+sweep+fire){m.push_back({t-dt/2,0,0,-1});}m.push_back({t-dt/3,mx,my,0});hx.push_back(mx);hy.push_back(my);kk.push_back(k);
        size_t j=hx.size()>=2?hx.size()-2:0;double camX=kk[j]*hx[j],camY=kk[j]*hy[j];if(i-1>=hip+sweep&&i-1<hip+sweep+fire)camY-=300*dt;s.push_back({t,-camX,-camY});}
     Result r=analyse(s,m,1.0,0,0,0,1,calA,calB);char msg[160];snprintf(msg,160,"sweep+reaction ok=%d cal=%d kV=%.3f ratio=%.3f",r.ok,r.calibrated,r.kV,r.ratio);
     CHECK(r.ok&&r.calibrated&&std::fabs(r.kV-.8)<.03&&std::fabs(r.ratio-1.25)<.06,msg);
     Result old=analyse(s,m,1.0);snprintf(msg,160,"without the sweep window cal=%d kV=%.3f ratio=%.3f",old.calibrated,old.kV,old.ratio);printf("%s\n",msg);CHECK(!old.calibrated,msg);}
    // 3. no sideways look and no move before the shots: pixels per count cannot be measured
    {std::vector<Shift> s;std::vector<Mouse> m;double t=0;for(int i=0;i<200;i++){t+=1/144.0;s.push_back({t,0,i>50?2.0:0.0});}m.push_back({.4,0,0,1});m.push_back({1.2,0,0,-1});
     Result r=analyse(s,m,1);CHECK(!r.ok&&r.why=="still","still mouse is reported");
     Result r2=analyse(s,m,1,0,.8,.8,1);CHECK(r2.why!="still","the last measurement stands in for a still mouse");}
    // 4. the held gun's icon: a wide white silhouette among small white letters, matched against itself and another shape
    {int W=1920,H=1080;std::vector<uint8_t> img(size_t(W)*H*4,30);auto box=[&](int x0,int y0,int w,int h,uint8_t v){for(int y=y0;y<y0+h;y++)for(int x=x0;x<x0+w;x++){uint8_t* p=&img[(size_t(y)*W+x)*4];p[0]=p[1]=p[2]=v;}};
     box(1500,960,180,22,240);box(1520,982,40,26,240);box(1650,940,8,10,240);box(1700,1000,6,8,240);box(1500,1030,180,20,170); // gun, grip, two letters, the other (grey) slot
     Icon a=iconFrom(img.data(),W*4,W,H);CHECK(a.ok(),"icon found");CHECK(same(a,a)>.99,"icon matches itself");
     Icon back=unhex(hex(a));CHECK(same(a,back)>.99,"icon hex round trip");
     std::fill(img.begin(),img.end(),30);box(1500,960,180,22,240);box(1640,982,40,26,240);Icon b=iconFrom(img.data(),W*4,W,H);CHECK(b.ok()&&same(a,b)<.85,"a different gun differs");}
    if(fails){printf("%d failed\n",fails);return 1;}printf("recoil_core ok\n");return 0;}
