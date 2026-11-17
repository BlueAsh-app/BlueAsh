#pragma once
// PUBG 반동 분석 – the pure part (no Windows calls, tested by recoil_core_tests.cpp).
// While the player sprays at a wall, the capture keeps only how far the picture around the crosshair moved from one
// frame to the next (match(): image registration, coarse to fine), and the recorder keeps the raw mouse counts. Then:
//   camera motion = the gun's recoil - the player's pull     (pull = mouse counts x pixels per count)
// so recoil = camera motion + pull. If the hand pulled 80 % of the recoil, the same hand movement pulls 100 % at a
// sensitivity 1.25x as high ("the laser": the spray would stay on one point). Pixels per count are measured from the
// same clip (frames where the camera only follows the mouse), so neither the FOV nor the scope's zoom is needed.
// Also: the held gun's HUD icon as a small mask (named once by the player, matched later). The scope cannot be told
// from pixels per count: PUBG scales each scope's sensitivity by its zoom, so the player picks it on the result.
#include <vector>
#include <string>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <cstdio>
namespace recoil {
struct Gray {int w=0,h=0;std::vector<uint8_t> p;uint8_t at(int x,int y)const{return p[size_t(y)*w+x];}};
// A BGRA region shrunk by an integer factor into gray (green weighted: haze and blue sky matter less).
inline Gray shrink(const uint8_t* bgra,int pitch,int w,int h,int f){Gray g;f=std::max(1,f);g.w=w/f;g.h=h/f;g.p.assign(size_t(g.w)*g.h,0);
    for(int y=0;y<g.h;y++){for(int x=0;x<g.w;x++){unsigned s=0;for(int j=0;j<f;j++){const uint8_t* r=bgra+size_t(y*f+j)*pitch+size_t(x*f)*4;for(int i=0;i<f;i++,r+=4)s+=(r[0]+2u*r[1]+r[2]);}g.p[size_t(y)*g.w+x]=(uint8_t)(s/(4u*f*f));}}return g;}
inline Gray half(const Gray& a){Gray g;g.w=a.w/2;g.h=a.h/2;g.p.resize(size_t(g.w)*g.h);for(int y=0;y<g.h;y++)for(int x=0;x<g.w;x++)g.p[size_t(y)*g.w+x]=(uint8_t)((a.at(2*x,2*y)+a.at(2*x+1,2*y)+a.at(2*x,2*y+1)+a.at(2*x+1,2*y+1)+2)/4);return g;}
// Mean absolute difference between b and a moved by (dx,dy): b(x,y) ~ a(x-dx,y-dy), over the overlap, every step-th pixel,
// leaving out the columns [skipL,skipR) of b (the crosshair or a scope's reticle stays put on screen and pulls toward 0).
inline double sad(const Gray& a,const Gray& b,int dx,int dy,int step,int skipL,int skipR){long long s=0,n=0;
    int y0=std::max(0,dy),y1=std::min(b.h,a.h+dy),x0=std::max(0,dx),x1=std::min(b.w,a.w+dx);
    for(int y=y0;y<y1;y+=step){const uint8_t* rb=&b.p[size_t(y)*b.w];const uint8_t* ra=&a.p[size_t(y-dy)*a.w];for(int x=x0;x<x1;x+=step){if(x>=skipL&&x<skipR)continue;s+=std::abs(int(rb[x])-int(ra[x-dx]));n++;}}
    long long need=(long long)b.w*b.h/(step*step)/3;return n<need?1e9:double(s)/n;}
struct Match {double dx=0,dy=0,score=1e9;};
struct Pyramid {Gray l0,l1,l2;};
inline Pyramid pyramid(Gray g){Pyramid p;p.l1=half(g);p.l2=half(p.l1);p.l0=std::move(g);return p;}
// How far the picture moved from a to b (pixels of l0), searched around a guess (the last motion): +-r*4 at a quarter
// size, then +-1 at half and at full size, then a parabola through the neighbours for the fraction.
inline Match match(const Pyramid& a,const Pyramid& b,double gx,double gy,int r,int skipL,int skipR){
    int bx=(int)std::lround(gx/4),by=(int)std::lround(gy/4);double best=1e18;int cx=bx,cy=by;
    for(int dy=by-r;dy<=by+r;dy++)for(int dx=bx-r;dx<=bx+r;dx++){double s=sad(a.l2,b.l2,dx,dy,1,skipL/4,skipR/4);if(s<best){best=s;cx=dx;cy=dy;}}
    auto refine=[&](const Gray& ga,const Gray& gb,int k,int& x,int& y){x*=2;y*=2;double m=1e18;int ox=x,oy=y;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){double s=sad(ga,gb,ox+dx,oy+dy,1,skipL/k,skipR/k);if(s<m){m=s;x=ox+dx;y=oy+dy;}}return m;};
    refine(a.l1,b.l1,2,cx,cy);double s0=refine(a.l0,b.l0,1,cx,cy);
    auto frac=[&](double l,double c,double r2){double d=l-2*c+r2;return d>1e-9?std::clamp(.5*(l-r2)/d,-.5,.5):0.0;};
    Match m;m.score=s0;m.dx=cx+frac(sad(a.l0,b.l0,cx-1,cy,1,skipL,skipR),s0,sad(a.l0,b.l0,cx+1,cy,1,skipL,skipR));m.dy=cy+frac(sad(a.l0,b.l0,cx,cy-1,1,skipL,skipR),s0,sad(a.l0,b.l0,cx,cy+1,1,skipL,skipR));return m;}

// ---- fire intervals --------------------------------------------------------------------------------------------------
// Seconds between shots in full auto, from the Namu Wiki page of each PUBG weapon (checked 2026-11-17). 0 = no full auto
// (the Mk47 fires bursts only; DMRs and rifles are single shot). MG3 has two rates; the 660 RPM one is used.
inline double fireInterval(const std::string& w){struct R{const char* n;double s;};static const R t[]={{"Beryl M762",.086},{"AKM",.1},{"M416",.0857},{"SCAR-L",.092},{"AUG",.083},{"ACE32",.088},
    {"G36C",.0857},{"QBZ",.0923},{"K2",.0857},{"Groza",.08},{"M16A4",.0909},{"FAMAS",.067},{"Vector",.0545},{"UMP45",.089},{"Tommy Gun",.08},{"MP5K",.0666},{"Micro UZI",.048},{"P90",.06},
    {"JS9",.067},{"PP-19 Bizon",.0857},{"MP9",.06},{"DP-28",.109},{"M249",.075},{"MG3",.0909}};for(auto& x:t)if(w==x.n)return x.s;return 0;}
// ---- the scope, by what stays still -------------------------------------------------------------------------------
// While the world moves (recoil, the pull), a scope's reticle and its dark housing stay on the same screen pixels. Over a
// capture, a square at the centre of the screen is averaged: pixels that hardly changed AND are an edge (or dark housing)
// are the scope's own picture. A flat wall also barely changes, which is why plain still pixels without an edge do not count.
struct Mask {int w=0,h=0;std::vector<uint8_t> bits;bool ok()const{return w>0&&bits.size()==size_t(w*h);}};
struct StillMap {int w=0,h=0,n=0;std::vector<double> s,ss;
    void add(const Gray& g){if(!w){w=g.w;h=g.h;s.assign(size_t(w)*h,0);ss.assign(size_t(w)*h,0);}if(g.w!=w||g.h!=h)return;for(size_t i=0;i<s.size();i++){double v=g.p[i];s[i]+=v;ss[i]+=v*v;}n++;}
    Mask sig()const{Mask m;if(n<12||w<8||h<8)return m;m.w=w;m.h=h;m.bits.assign(size_t(w)*h,0);std::vector<double> mean(s.size());for(size_t i=0;i<s.size();i++)mean[i]=s[i]/n;
        for(int y=1;y<h-1;y++)for(int x=1;x<w-1;x++){size_t i=size_t(y)*w+x;double var=ss[i]/n-mean[i]*mean[i],sd=std::sqrt(std::max(0.0,var));
            double grad=std::max({std::fabs(mean[i]-mean[i-1]),std::fabs(mean[i]-mean[i+1]),std::fabs(mean[i]-mean[i-w]),std::fabs(mean[i]-mean[i+w])}); /* a 1-pixel line has no central difference on itself */m.bits[i]=sd<7&&(grad>16||mean[i]<28);}
        int on=0;for(auto b:m.bits)on+=b;if(on<6)m.bits.assign(m.bits.size(),0);return m;}};
inline double same(const Mask& a,const Mask& b){if(!a.ok()||!b.ok()||a.w!=b.w||a.h!=b.h)return 0;int i=0,u=0;for(size_t k=0;k<a.bits.size();k++){i+=a.bits[k]&&b.bits[k];u+=a.bits[k]||b.bits[k];}return u>=6?double(i)/u:0;}
inline std::string hex(const Mask& a){std::string s=std::to_string(a.w)+"x"+std::to_string(a.h)+":";const char* d="0123456789abcdef";for(size_t k=0;k+3<a.bits.size();k+=4){s+=d[a.bits[k]|a.bits[k+1]<<1|a.bits[k+2]<<2|a.bits[k+3]<<3];}return s;}
inline Mask unhexMask(const std::string& s){Mask a;int w=0,h=0;size_t c=s.find(':');if(c==std::string::npos||sscanf(s.c_str(),"%dx%d",&w,&h)!=2||w<=0||h<=0||(w*h)%4)return a;std::string b=s.substr(c+1);if(b.size()*4!=size_t(w*h))return a;
    a.w=w;a.h=h;a.bits.resize(b.size()*4);for(size_t i=0;i<b.size();i++){int v=b[i]<='9'?b[i]-'0':b[i]-'a'+10;for(int k=0;k<4;k++)a.bits[i*4+k]=(v>>k)&1;}return a;}
// ---- a steady suggestion --------------------------------------------------------------------------------------
// Each spray says "the vertical setting that would have pulled 100 %" (vm x ratio). One hand never pulls the same way
// twice, so one spray's number wanders a few %. The median of the latest five (same gun, same scope) is what to set; the
// spread says how far they disagreed. targets: newest first.
struct Steady {double value=0,spread=0;int n=0;};
inline Steady steady(const std::vector<double>& targets){Steady s;std::vector<double> v;for(double t:targets){if(t>0)v.push_back(t);if(v.size()==5)break;}if(v.empty())return s;
    s.n=(int)v.size();std::vector<double> o=v;std::sort(o.begin(),o.end());s.value=o.size()%2?o[o.size()/2]:(o[o.size()/2-1]+o[o.size()/2])/2;s.spread=s.value>0?(o.back()-o.front())/2/s.value:0;return s;}
// ---- the analysis -----------------------------------------------------------------------------------------------
struct Shift {double t,dx,dy;};          // picture motion from the previous frame to this one (screen pixels, right / down +)
struct Mouse {double t;long dx,dy;int button;}; // raw counts (right / toward the player +); button +1 left down, -1 left up
struct Pt {float x,y;};
// ---- one magazine in detail ------------------------------------------------------------------------------------
// Errors are % of the magazine's vertical recoil travel (so neither the resolution nor the scope matters):
//   vErr: the crosshair's average height over the spray (+ above the start = under-pulled, - below = over-pulled)
//   hErr: its average sideways place (+ right); wrong: % of the frames where the hand moved the same way the gun kicked
//   sideways (adding to the kick instead of countering it); react: seconds until the hand had pulled 10 % of its pull;
//   phase: % of the recoil pulled in the first, middle and last third of the spray.
// shots: bullets fired (hold time / interval + 1, 0 = gun unknown); shot: the crosshair at each shot; breakAt: the first shot (1-based) farther
// than 6 % of the travel from the start (-1 = it stayed close).
struct Burst {double ratio=0,vErr=0,hErr=0,wrong=-1,react=0,phase[3]={0,0,0};int shots=0,breakAt=-1;std::vector<Pt> aim,shot;};
inline Burst burst(const std::vector<Shift>& s,const std::vector<double>& mx,const std::vector<double>& my,size_t s0,size_t s1,double kH,double kV,int lag,double dt=0,double release=0){Burst b;
    double pl=0,rc=0,ax=0,ay=0,sax=0,say=0,t0=s[s0-1].t,dur=std::max(1e-6,s[s1].t-t0),ph[3]={0,0,0},pr[3]={0,0,0};int nf=0,wr=0,cnt=0;std::vector<double> cum;b.aim.push_back({0,0});
    for(size_t j=s0;j<=s1;j++){size_t k=j>=(size_t)lag?j-lag:0;double camX=-s[j].dx,camY=-s[j].dy,hX=kH*mx[k],hY=kV*my[k],gX=camX-hX,gY=camY-hY;
        pl+=hY;rc+=gY;ax+=camX;ay+=camY;sax+=ax;say+=ay;nf++;b.aim.push_back({(float)ax,(float)ay});cum.push_back(pl);
        int p=std::min(2,(int)((s[j].t-t0)/dur*3));ph[p]+=hY;pr[p]-=gY;if(std::fabs(gX)>.3&&std::fabs(hX)>.3){cnt++;if(gX*hX>0)wr++;}}
    double travel=-rc;if(travel<=0||pl<=travel*.05||!nf)return b;
    b.ratio=std::clamp(travel/pl,.4,2.5);b.vErr=-say/nf/travel*100;b.hErr=sax/nf/travel*100;b.wrong=cnt>=5?100.0*wr/cnt:-1;
    for(size_t q=0;q<cum.size();q++)if(cum[q]>=pl*.1){b.react=s[s0+q].t-t0;break;}
    for(int p=0;p<3;p++){b.phase[p]=pr[p]>0?ph[p]/pr[p]*100:0;}
    if(dt>0&&release>t0){b.shots=(int)std::floor((release-t0)/dt+1e-9)+1;size_t j=0;for(int k=0;k<b.shots;k++){double tk=t0+k*dt;while(j+1<b.aim.size()-1&&s[s0+j].t<tk)j++;Pt p=b.aim[std::min(j+1,b.aim.size()-1)];b.shot.push_back(p);
        if(b.breakAt<0&&std::hypot(p.x,p.y)>travel*.06)b.breakAt=k+1;}}
    return b;}
// Several magazines summed up: the averages, how far the magazines disagreed (standard deviation), the median reaction.
struct Summary {int n=0;double vErr=0,hErr=0,vSd=0,hSd=0,wrong=-1,react=0,phase[3]={0,0,0},shots=0;int breakAt=-1;};
inline Summary summarize(const std::vector<Burst>& v){Summary m;std::vector<double> re;double w=0;int wn=0;for(auto& b:v){if(b.ratio<=0)continue;m.n++;m.vErr+=b.vErr;m.hErr+=b.hErr;for(int p=0;p<3;p++)m.phase[p]+=b.phase[p];re.push_back(b.react);if(b.wrong>=0){w+=b.wrong;wn++;}}
    if(!m.n){return m;}m.vErr/=m.n;m.hErr/=m.n;for(int p=0;p<3;p++)m.phase[p]/=m.n;for(auto& b:v){if(b.ratio<=0)continue;m.vSd+=(b.vErr-m.vErr)*(b.vErr-m.vErr);m.hSd+=(b.hErr-m.hErr)*(b.hErr-m.hErr);}
    {std::vector<int> br;int sn=0;for(auto& b:v){if(b.ratio<=0)continue;if(b.shots>0){m.shots+=b.shots;sn++;}if(b.breakAt>0)br.push_back(b.breakAt);}if(sn)m.shots/=sn;if(!br.empty()){std::sort(br.begin(),br.end());m.breakAt=br[br.size()/2];}}
    m.vSd=std::sqrt(m.vSd/m.n);m.hSd=std::sqrt(m.hSd/m.n);if(wn)m.wrong=w/wn;std::sort(re.begin(),re.end());m.react=re[re.size()/2];return m;}
struct Result {bool ok=false;std::string why;     // why: "short" "still" "nopull" "nofire" when not ok
    double kH=0,kV=0;bool kMeasured=false,reused=false; // reused: too little mouse movement, the last measurement with the same PUBG settings was used        // screen pixels per count, horizontal and vertical
    std::vector<Burst> each;double hComp=-9;      // each magazine in detail; hComp: how much of the sideways kick the hand countered (1 = all, >1 too much, -9 = too little sideways kick to tell)
    std::vector<double> burstRatios;              // one answer per burst (trigger held) long enough to read; the ratio is their median when there are 2+
    double fitH=0,fitV=0,movedH=0,movedV=0;       // how well the camera followed the mouse (r2) and how many counts moved (for the diagnosis file)
    double recoil=0,pull=0,ratio=1,drift=0,fireSec=0;int frames=0,lag=0,bursts=0;
    std::vector<Pt> aim,hand,gun,laser;};       // from the first shot: where the crosshair went, the hand (as screen motion), the gun alone, the gun at the suggested sensitivity
// vm: the current vertical multiplier (PUBG scales y by it), for the vertical pixels per count when only x could be measured.
// fbH/fbV/fbLag: pixels per count from an earlier capture with the same PUBG sensitivity settings (0 = none), used when
// this clip has too little movement of its own to measure them.
inline Result analyse(const std::vector<Shift>& s,const std::vector<Mouse>& m,double vm,double dt=0,double fbH=0,double fbV=0,int fbLag=1){Result r;r.frames=(int)s.size();if(s.size()<12){r.why="short";return r;}
    size_t n=s.size();std::vector<double> mx(n,0),my(n,0);std::vector<char> fire(n,0);
    // mouse counts and the trigger per frame interval (t[i-1], t[i]]
    {size_t j=0;bool down=false;while(j<m.size()&&m[j].t<=s[0].t){if(m[j].button>0)down=true;if(m[j].button<0)down=false;j++;}
     for(size_t i=1;i<n;i++){bool any=down;while(j<m.size()&&m[j].t<=s[i].t){mx[i]+=m[j].dx;my[i]+=m[j].dy;if(m[j].button>0){down=true;any=true;}if(m[j].button<0)down=false;j++;}fire[i]=any;}}
    for(size_t i=1;i<n;i++){if(fire[i]&&!fire[i-1])r.bursts++;}
    // the screen shows the mouse a frame or two late: the lag (0..4 frames) that best explains the camera by the mouse
    auto fit=[&](int lag,bool vertical,double& k,double& r2){double sxy=0,sxx=0,syy=0,moved=0;
        for(size_t i=1+lag;i<n;i++){if(vertical&&(fire[i]||fire[i-lag]))continue;double c=vertical?s[i].dy:-s[i].dx,u=vertical?-my[i-lag]:mx[i-lag];sxy+=c*u;sxx+=u*u;syy+=c*c;moved+=std::fabs(u);}
        k=sxx>0?sxy/sxx:0;r2=(sxx>0&&syy>0)?sxy*sxy/(sxx*syy):0;return moved;};
    double kH=0,kV=0,rH=0,rV=0,eH=0,eV=0;int lagH=0,lagV=0;
    for(int lag=0;lag<=4;lag++){double k,q,e=fit(lag,false,k,q);if(q>rH){rH=q;kH=k;lagH=lag;eH=e;}e=fit(lag,true,k,q);if(q>rV){rV=q;kV=k;lagV=lag;eV=e;}}
    r.fitH=rH;r.fitV=rV;r.movedH=eH;r.movedV=eV;
    bool hOk=eH>=150&&rH>=.5&&kH>0,vOk=eV>=150&&rV>=.6&&kV>0; // enough counts moved and the camera clearly followed
    if(vOk){r.kV=kV;r.kH=hOk?kH:kV/std::max(.1,vm);r.lag=lagV;}else if(hOk){r.kH=kH;r.kV=kH*vm;r.lag=lagH;}else if(fbH>0&&fbV>0){r.kH=fbH;r.kV=fbV;r.lag=std::clamp(fbLag,0,4);r.reused=true;}else{r.why="still";return r;}
    r.kMeasured=!r.reused;
    // the spray: from the first frame with the trigger held to a quarter second after the last (the last pull counts)
    size_t a=0,b=0;for(size_t i=1;i<n;i++)if(fire[i]){if(!a)a=i;b=i;}
    if(!a){r.why="nofire";return r;}{double te=s[b].t+.25;while(b+1<n&&s[b+1].t<=te)b++;}
    r.fireSec=s[b].t-s[a-1].t;if(r.fireSec<.35){r.why="short";return r;}
    double ax=0,ay=0,hx=0,hy=0,gx=0,gy=0,pull=0,rec=0;r.aim.push_back({0,0});r.hand.push_back({0,0});r.gun.push_back({0,0});
    for(size_t i=a;i<=b;i++){size_t k=i>=(size_t)r.lag?i-r.lag:0;double camX=-s[i].dx,camY=-s[i].dy; // the aim point in the world, screen axes (up = -y): the picture moving down means the aim went up
        double handX=r.kH*mx[k],handY=r.kV*my[k];ax+=camX;ay+=camY;hx+=handX;hy+=handY;gx+=camX-handX;gy+=camY-handY;pull+=handY;rec+=camY-handY;
        r.aim.push_back({(float)ax,(float)ay});r.hand.push_back({(float)hx,(float)hy});r.gun.push_back({(float)gx,(float)gy});}
    // gun y is negative going up (screen y); the recoil climbs: rec < 0, the pull is down: pull > 0 when compensating
    r.recoil=-rec;r.pull=pull;r.drift=-ay;
    {double hg=0,gg=0,ag=0,av=0;for(size_t i=a;i<=b;i++){size_t k=i>=(size_t)r.lag?i-r.lag:0;double hX=r.kH*mx[k],gX=-s[i].dx-hX,gY=-s[i].dy-r.kV*my[k];hg+=hX*gX;gg+=gX*gX;ag+=std::fabs(gX);av+=std::fabs(gY);}
     if(gg>0&&ag>.15*av)r.hComp=-hg/gg;}
    if(r.recoil<=0){r.why="norecoil";return r;}if(r.pull<=r.recoil*.05){r.why="nopull";return r;}
    r.ratio=std::clamp(r.recoil/r.pull,.4,2.5);
    {std::vector<double> each;size_t i=1;while(i<n){if(!fire[i]){i++;continue;}size_t s0=i;while(i<n&&fire[i])i++;size_t s1=i-1;double te=s[s1].t+.25;while(s1+1<n&&s[s1+1].t<=te&&!fire[s1+1])s1++;
        if(s[s1].t-s[s0-1].t<.35){continue;}Burst bd=burst(s,mx,my,s0,s1,r.kH,r.kV,r.lag,dt,s[i-1].t);if(bd.ratio>0){each.push_back(bd.ratio);r.each.push_back(std::move(bd));}}
     r.burstRatios=each;if(each.size()>=2){std::sort(each.begin(),each.end());r.ratio=each.size()%2?each[each.size()/2]:(each[each.size()/2-1]+each[each.size()/2])/2;}}
    // the same hand at ratio x the sensitivity: the vertical pull scaled, the horizontal as it was
    double py=0;r.laser.push_back({0,0});for(size_t i=1;i<r.gun.size();i++){double handYi=r.hand[i].y-r.hand[i-1].y;py+=(r.gun[i].y-r.gun[i-1].y)+handYi*r.ratio;r.laser.push_back({r.aim[i].x,(float)py});}
    r.ok=true;return r;}

// ---- the held gun's HUD icon --------------------------------------------------------------------------------------
// The selected weapon slot's icon is drawn bright white, the other slot grey; the minimap's white labels are small letters.
struct Icon {static const int W=48,H=16;std::vector<uint8_t> bits;bool ok()const{return bits.size()==size_t(W*H);}};
// The icon corner: 68..100 % across, 82..100 % down. iconFromCorner reads a copy of just that corner (reg = its top left).
inline void iconCorner(int w,int h,int& x0,int& y0){x0=w*68/100;y0=h*82/100;}
inline Icon iconFromCorner(const uint8_t* reg,int pitch,int w,int h);
inline Icon iconFrom(const uint8_t* bgra,int pitch,int w,int h){int x0,y0;iconCorner(w,h,x0,y0);return iconFromCorner(bgra+size_t(y0)*pitch+size_t(x0)*4,pitch,w,h);}
inline Icon iconFromCorner(const uint8_t* reg,int pitch,int w,int h){Icon ic;int x0,y0;iconCorner(w,h,x0,y0);int cw=w-x0,ch=h-y0,g=2,gw=cw/g,gh=ch/g;if(gw<8||gh<4)return ic;
    std::vector<uint8_t> on(size_t(gw)*gh,0);std::vector<uint16_t> lum(size_t(gw)*gh,0);
    for(int y=0;y<gh;y++)for(int x=0;x<gw;x++){int hit=0,sum=0;for(int j=0;j<g;j++)for(int i=0;i<g;i++){const uint8_t* p=reg+size_t(y*g+j)*pitch+size_t(x*g+i)*4;int mn=std::min({p[0],p[1],p[2]}),mx=std::max({p[0],p[1],p[2]});if(mn>=205&&mx-mn<=45)hit++;sum+=mn;}on[size_t(y)*gw+x]=hit>=2;lum[size_t(y)*gw+x]=(uint16_t)(sum/(g*g));}
    // blobs (8-connected); a gun is wide: at least 4 % of the screen across and 2.5x wider than tall
    std::vector<int> id(on.size(),0);int bestId=0;double bestScore=0;int bx0=0,by0=0,bx1=0,by1=0;std::vector<int> stack;
    for(int sy=0;sy<gh;sy++)for(int sx=0;sx<gw;sx++){size_t s0=size_t(sy)*gw+sx;if(!on[s0]||id[s0])continue;int lab=(int)(s0+1);stack.assign(1,(int)s0);id[s0]=lab;int x0b=sx,x1b=sx,y0b=sy,y1b=sy,count=0;double bright=0;
        while(!stack.empty()){int q=stack.back();stack.pop_back();int qx=q%gw,qy=q/gw;count++;bright+=lum[q];x0b=std::min(x0b,qx);x1b=std::max(x1b,qx);y0b=std::min(y0b,qy);y1b=std::max(y1b,qy);
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){int nx=qx+dx,ny=qy+dy;if(nx<0||ny<0||nx>=gw||ny>=gh)continue;size_t nq=size_t(ny)*gw+nx;if(on[nq]&&!id[nq]){id[nq]=lab;stack.push_back((int)nq);}}}
        int bw=(x1b-x0b+1)*g,bh=(y1b-y0b+1)*g;if(bw<w*4/100||bw<bh*5/2||bh<h/90)continue;double score=count*(bright/count);if(score>bestScore){bestScore=score;bestId=lab;bx0=x0b;by0=y0b;bx1=x1b;by1=y1b;}}
    if(!bestId){return ic;}ic.bits.assign(size_t(Icon::W)*Icon::H,0);int bw=bx1-bx0+1,bh=by1-by0+1;
    for(int y=0;y<Icon::H;y++)for(int x=0;x<Icon::W;x++){int sx0=bx0+x*bw/Icon::W,sx1=std::max(sx0+1,bx0+(x+1)*bw/Icon::W),sy0=by0+y*bh/Icon::H,sy1=std::max(sy0+1,by0+(y+1)*bh/Icon::H);int c=0,t=0;
        for(int yy=sy0;yy<sy1;yy++){for(int xx=sx0;xx<sx1;xx++){t++;c+=id[size_t(yy)*gw+xx]==bestId;}}ic.bits[size_t(y)*Icon::W+x]=c*2>=t;}return ic;}
inline double same(const Icon& a,const Icon& b){if(!a.ok()||!b.ok())return 0;int i=0,u=0;for(size_t k=0;k<a.bits.size();k++){i+=a.bits[k]&&b.bits[k];u+=a.bits[k]||b.bits[k];}return u?double(i)/u:0;}
inline std::string hex(const Icon& a){std::string s;const char* d="0123456789abcdef";for(size_t k=0;k+3<a.bits.size();k+=4){int v=a.bits[k]|a.bits[k+1]<<1|a.bits[k+2]<<2|a.bits[k+3]<<3;s+=d[v];}return s;}
inline Icon unhex(const std::string& s){Icon a;if(s.size()*4!=size_t(Icon::W*Icon::H))return a;a.bits.resize(s.size()*4);for(size_t i=0;i<s.size();i++){int v=s[i]<='9'?s[i]-'0':s[i]-'a'+10;for(int b=0;b<4;b++)a.bits[i*4+b]=(v>>b)&1;}return a;}
}
