#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>
// Where the PUBG world map sits on the screen, from a screenshot alone (no game data).
// 1) The map's 1 km grid lines are thin dark lines across the whole view: their spacing gives the zoom.
// 2) The offset is the grid phase plus a whole number of cells: each choice is scored by comparing the
//    screen with the official map image (blurred, normalized correlation), which also tells which map it is.
// Result: screen x = ox + u*S, y = oy + v*S for map coordinates u,v in 0..1.
namespace mapfit {
struct Gray {int w=0,h=0;std::vector<float> p;
    float at(int x,int y)const{return p[size_t(std::clamp(y,0,h-1))*w+std::clamp(x,0,w-1)];}
    float sample(double x,double y)const{int x0=(int)std::floor(x),y0=(int)std::floor(y);double fx=x-x0,fy=y-y0;
        return float((at(x0,y0)*(1-fx)+at(x0+1,y0)*fx)*(1-fy)+(at(x0,y0+1)*(1-fx)+at(x0+1,y0+1)*fx)*fy);}};
// The official 8192 px map at 4096 px, 8 bit (16 MB; float would be 64), for deep zoom: downloaded per map on first use.
struct Gray8 {int w=0,h=0;std::vector<unsigned char> p;
    float at(int x,int y)const{return p[size_t(std::clamp(y,0,h-1))*w+std::clamp(x,0,w-1)];}
    float sample(double x,double y)const{int x0=(int)std::floor(x),y0=(int)std::floor(y);double fx=x-x0,fy=y-y0;
        return float((at(x0,y0)*(1-fx)+at(x0+1,y0)*fx)*(1-fy)+(at(x0,y0+1)*(1-fx)+at(x0+1,y0+1)*fx)*fy);}};
// 3 x 3 box blur in place (the screen is compared blurred too).
inline void blur8(Gray8& g){std::vector<unsigned char> t(g.p.size());
    for(int y=0;y<g.h;y++)for(int x=0;x<g.w;x++){t[size_t(y)*g.w+x]=(unsigned char)((g.at(x-1,y)+g.at(x,y)+g.at(x+1,y))/3+0.5f);}
    for(int y=0;y<g.h;y++)for(int x=0;x<g.w;x++){auto v=[&](int yy){return (float)t[size_t(std::clamp(yy,0,g.h-1))*g.w+x];};g.p[size_t(y)*g.w+x]=(unsigned char)((v(y-1)+v(y)+v(y+1))/3+0.5f);}}
inline Gray shrink(const Gray& g,int f){Gray o;o.w=g.w/f;o.h=g.h/f;o.p.assign(size_t(o.w)*o.h,0);
    for(int y=0;y<o.h;y++){for(int x=0;x<o.w;x++){float s=0;for(int dy=0;dy<f;dy++){for(int dx=0;dx<f;dx++)s+=g.p[size_t(y*f+dy)*g.w+x*f+dx];}o.p[size_t(y)*o.w+x]=s/(f*f);}}return o;}
inline Gray blur(const Gray& g,int r){Gray t=g,o=g;
    for(int y=0;y<g.h;y++)for(int x=0;x<g.w;x++){float s=0;for(int k=-r;k<=r;k++)s+=g.at(x+k,y);t.p[size_t(y)*g.w+x]=s/(2*r+1);}
    for(int y=0;y<g.h;y++){for(int x=0;x<g.w;x++){float s=0;for(int k=-r;k<=r;k++)s+=t.at(x,y+k);o.p[size_t(y)*g.w+x]=s/(2*r+1);}}return o;}
// Thin-dark-line response per column (vertical) or row, over the middle of the other axis.
inline std::vector<double> lines(const Gray& g,bool vertical){
    int n=vertical?g.w:g.h,m=vertical?g.h:g.w;std::vector<double> r(n,0);
    for(int i=2;i<n-2;i++){double s=0;int c=0;for(int j=m*15/100;j<m*85/100;j++){auto v=[&](int k){return vertical?g.at(k,j):g.at(j,k);};
        double side=std::min(v(i-2),v(i+2)),mid=std::min(v(i),std::min(v(i-1)+2.0f,v(i+1)+2.0f));s+=std::max(0.0,side-mid);c++;}r[i]=c?s/c:0;}
    return r;
}
// Best spacing/phase of a comb over the response: lines must stand out against the positions between them.
// Weighted by sqrt(teeth), so the true spacing beats both its multiples (fewer teeth) and halves (half miss).
inline bool comb(const std::vector<double>& r,double dmin,double dmax,double& d,double& p,double& strength){
    int n=(int)r.size();double best=0;auto at=[&](double x){int i=(int)std::lround(x);return i>=0&&i<n?r[i]:0.0;};
    for(double dd=dmin;dd<=dmax;dd+=dd<40?0.25:0.5){for(double pp=0;pp<dd;pp+=0.5){double on=0,off=0;int c=0;
        for(double x=pp;x<n;x+=dd){on+=std::max({at(x-0.5),at(x),at(x+0.5)});off+=at(x+dd/2);c++;}
        if(c<3){continue;}double s=(on-off)/c*std::sqrt((double)c);if(s>best){best=s;d=dd;p=pp;}}}
    // Refine around the winner in finer steps (the coarse step alone left the map 0.7% too large).
    if(best>0){double d0=d,p0=p;for(double dd=std::max(dmin,d0-0.6);dd<=d0+0.6;dd+=0.05){for(double pp=p0-1;pp<=p0+1;pp+=0.1){double on=0,off=0;int c=0;
        for(double x=pp;x<n;x+=dd){if(x<0)continue;on+=std::max({at(x-0.5),at(x),at(x+0.5)});off+=at(x+dd/2);c++;}
        if(c<3){continue;}double s=(on-off)/c*std::sqrt((double)c);if(s>best){best=s;d=dd;p=pp;}}}}
    strength=best;return best>0;
}
// The game draws the grid 1-2 px wide and half transparent: at reduced size it drowns in the terrain, so the
// response is taken at full resolution (lines 1 px wide, sides 3 px away) and max-pooled down by f.
inline std::vector<double> responseFull(const Gray& g,bool vertical){
    int n=vertical?g.w:g.h,m=vertical?g.h:g.w;std::vector<double> full(n,0);
    for(int i=3;i<n-3;i++){double s=0;int c=0;for(int j=m*15/100;j<m*85/100;j+=2){auto v=[&](int k){return vertical?g.p[size_t(j)*g.w+k]:g.p[size_t(k)*g.w+j];};
        double side=std::min(v(i-3),v(i+3)),mid=std::min(v(i),v(i+1));s+=std::max(0.0,side-mid);c++;}full[i]=c?s/c:0;}
    return full;
}
inline std::vector<double> pool(const std::vector<double>& full,int f){int n=(int)full.size();std::vector<double> r(n/f,0);for(int i=0;i<n/f*f;i++){r[i/f]=std::max(r[i/f],full[i]);}return r;}
inline std::vector<double> linesFull(const Gray& g,bool vertical,int f){return pool(responseFull(g,vertical),f);}
// Sub-pixel spacing and phase: each line the comb predicts is located on the full-resolution response (parabola through
// the peak) and a straight line fitted through them. In and out in shrunk coordinates (pixel i = full i*f+(f-1)/2).
inline void refine(const std::vector<double>& full,int f,double& d,double& p){
    double D=d*f,P=p*f+(f-1)/2.0;int n=(int)full.size();std::vector<double> ks,xs,peaks;
    for(int k=-1;P+k*D<n-5;k++){double X=P+k*D;if(X<5){continue;}int lo=std::max(1,int(X)-f-2),hi=std::min(n-2,int(X)+f+2),bi=lo;for(int i=lo;i<=hi;i++){if(full[i]>full[bi])bi=i;}
        double l=full[bi-1],c=full[bi],r=full[bi+1],den=l-2*c+r;double off=den<0?std::clamp((l-r)/(2*den),-0.5,0.5):0;ks.push_back(k);xs.push_back(bi+off);peaks.push_back(c);}
    if(ks.size()<2){return;}std::vector<double> sorted=peaks;std::sort(sorted.begin(),sorted.end());double cut=sorted[sorted.size()/2]*0.4;
    double sk=0,sx=0,skk=0,skx=0;int m=0;for(size_t i=0;i<ks.size();i++){if(peaks[i]<cut)continue;sk+=ks[i];sx+=xs[i];skk+=ks[i]*ks[i];skx+=ks[i]*xs[i];m++;}
    double det=m*skk-sk*sk;if(m<2||std::fabs(det)<1e-9){return;}double D2=(m*skx-sk*sx)/det,P2=(sx-D2*sk)/m;if(std::fabs(D2-D)>D*0.03){return;}
    d=D2/f;p=(P2-(f-1)/2.0)/f;p=std::fmod(p,d);if(p<0){p+=d;}
}
// d2: the other axis's spacing when it is a whole multiple of d (0 otherwise). Deep in only two or three lines cross the
// short side, too few for its comb, which then locks onto half the spacing: both are tried.
struct Grid {bool ok=false;double d=0,px=0,py=0,strength=0,d2=0;};
// Phase only, the spacing known from the other axis (deep in, the short side can have just one or two lines).
inline bool phase(const std::vector<double>& r,double d,double& p,double& strength){
    int n=(int)r.size();double best=0;auto at=[&](double x){int i=(int)std::lround(x);return i>=0&&i<n?r[i]:0.0;};
    for(double pp=0;pp<d;pp+=0.25){double on=0,off=0;int c=0;for(double x=pp;x<n;x+=d){on+=std::max({at(x-0.5),at(x),at(x+0.5)});off+=at(x+d/2<n?x+d/2:x-d/2);c++;}
        if(!c){continue;}double s=(on-off)/c*std::sqrt((double)c);if(s>best){best=s;p=pp;}}
    strength=best;return best>0;
}
inline Grid gridFrom(const std::vector<double>& rx,const std::vector<double>& ry,int h){Grid o;double dx=0,px=0,sx=0,dy=0,py=0,sy=0;double lo=std::max(6.0,h/40.0),hi=h/1.5;
    bool okx=comb(rx,lo,hi,dx,px,sx),oky=comb(ry,lo,hi,dy,py,sy);if(!okx&&!oky){return o;}
    // One axis without a clear comb (too few lines) or with a spacing that fits neither way: take the other axis's spacing
    // (the long side has more lines) and only the phase here, if its lines stand out about as well.
    auto borrow=[&](bool toY){if(toY){double s=0;if(!okx||!phase(ry,dx,py,s)||s<sx*0.35)return false;dy=dx;sy=s;}else{double s=0;if(!oky||!phase(rx,dy,px,s)||s<sy*0.35)return false;dx=dy;sx=s;}return true;};
    if(!oky||!okx){if(!borrow(!oky)){return o;}}
    // A square grid: both spacings agree. Otherwise one axis locked onto a multiple; keep the finer one when it fits.
    if(std::fabs(dx-dy)>std::max(dx,dy)*0.06){double f=std::max(dx,dy)/std::min(dx,dy),k=std::round(f);if(k<2||std::fabs(f-k)>0.08){if(!borrow(rx.size()>=ry.size())){return o;}}else{o.d2=k*std::min(dx,dy);if(dx>dy)dx=dy;else dy=dx;}}
    o.ok=true;o.d=(dx+dy)/2;o.px=px;o.py=py;o.strength=std::min(sx,sy);return o;
}
inline Grid grid(const Gray& g){return gridFrom(lines(g,true),lines(g,false),g.h);}
// Full-resolution gray -> grid in the coordinates of the image shrunk by f: as found (coarse) and refined to sub-pixel.
// The refinement can be pulled off by other straight lines on a real screen, so callers keep whichever matches better.
inline void gridFullBoth(const Gray& full,int f,Grid& coarse,Grid& fine){auto fx=responseFull(full,true),fy=responseFull(full,false);coarse=gridFrom(pool(fx,f),pool(fy,f),full.h/f);fine=coarse;if(!coarse.ok){return;}
    double dx=coarse.d,px=coarse.px,dy=coarse.d,py=coarse.py;refine(fx,f,dx,px);refine(fy,f,dy,py);if(std::fabs(dx-dy)<coarse.d*0.01){fine.d=(dx+dy)/2;}fine.px=px;fine.py=py;
    // d2 an exact multiple of d: placements step by d from the measured phase, tens of cells off screen.
    if(coarse.d2>0){fine.d2=std::round(coarse.d2/coarse.d)*fine.d;}}
inline Grid gridFull(const Gray& full,int f){Grid c,g;gridFullBoth(full,f,c,g);return g;}
struct Fit {bool ok=false;double ox=0,oy=0,S=0,score=-1,second=-1;int map=-1;};
// Normalized correlation of the screen with map m placed at (ox,oy,S), on an sx*sy sample grid; -2 when they barely overlap.
template<class R> inline double score(const Gray& screen,const R& ref,double ox,double oy,double S,int sx,int sy){
    double a=0,b=0,aa=0,bb=0,ab=0;int c=0;
    for(int j=0;j<sy;j++)for(int i=0;i<sx;i++){double x=(i+0.5)*screen.w/sx,y=(j+0.5)*screen.h/sy,u=(x-ox)/S,v=(y-oy)/S;
        if(u<0||v<0||u>1||v>1){continue;}double s=screen.sample(x,y),r=ref.sample(u*(ref.w-1),v*(ref.h-1));a+=s;b+=r;aa+=s*s;bb+=r*r;ab+=s*r;c++;}
    if(c<sx*sy/4){return -2;}double va=aa-a*a/c,vb=bb-b*b/c;if(va<=1e-6||vb<=1e-6){return -2;}return (ab-a*b/c)/std::sqrt(va*vb);
}
// Reference detail to the zoom: the 273 px map while it fills a few screen pixels per reference pixel, the 819 px one
// when zoomed in (deep in, the small one is a blur and nothing matches).
inline const Gray& pick(const Gray& lo,const Gray* hi,double S){return hi&&S>lo.w*2.5?*hi:lo;}
// Deeper still the 819 px one is a blur too (1 px = 10 m on an 8 km map, a whole zoomed-in screen is a few dozen of them):
// the 4096 px map once the map is drawn at least 0.8 x its size, when it is there.
struct Refs {const Gray* lo=nullptr;const Gray* hi=nullptr;const Gray8* ultra=nullptr;};
inline bool deep(const Refs& r,double S){return r.ultra&&!r.ultra->p.empty()&&S>r.ultra->w*0.8;}
inline double score(const Gray& screen,const Refs& r,double ox,double oy,double S,int sx,int sy){return deep(r,S)?score(screen,*r.ultra,ox,oy,S,sx,sy):score(screen,pick(*r.lo,r.hi,S),ox,oy,S,sx,sy);}
// Correlation a placement needs: deep in without the 4096 px map the blur tells places apart only roughly, so only a
// clear mismatch is ruled out; with it, a real match.
inline double need(const Refs& r,const Gray& screen,double S,double whole){return S>screen.h*2.5?(deep(r,S)?0.3:0.0):whole;}
// Follow a zoom or drag from the last placement. The game zooms around the cursor and drags the map with it, so the map
// point that was under the cursor at the last placement (lx,ly) is now under the cursor (cx,cy) (same spot when only zooming); the new size comes from the grid spacing (1 km or 100 m lines,
// whichever is nearer the last size) and the offset snaps to the grid phase. Deep in, the blurred official map no longer
// tells places apart, so the correlation only has to rule out a clear mismatch there.
inline Fit track(const Gray& screen,const Grid& g,const Gray& lo,int cells,const Fit& last,double lx,double ly,double cx,double cy,const Gray* hi=nullptr,const Gray8* ultra=nullptr){Refs rs{&lo,hi,ultra};
    Fit f;if(!g.ok||!last.ok||last.S<=0){return f;}
    double best=1e9,S=0;for(double base:{g.d,g.d2})for(int mult:{1,10}){if(base<=0){continue;}double s=base*cells*mult;double r=std::fabs(std::log(s/last.S));if(r<best){best=r;S=s;}}
    if(best>std::log(6.0)){return f;}
    double u=(lx-last.ox)/last.S,v=(ly-last.oy)/last.S,px=cx-u*S,py=cy-v*S;
    f.ox=g.px+std::round((px-g.px)/g.d)*g.d;f.oy=g.py+std::round((py-g.py)/g.d)*g.d;f.S=S;f.map=last.map;
    f.score=score(screen,rs,f.ox,f.oy,f.S,48,std::max(12,48*screen.h/std::max(1,screen.w)));
    // With the 4096 px map the snapped cell can be checked against its neighbours: a fast zoom or drag can land a cell off.
    if(deep(rs,S)){Fit b=f;for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){if(!dx&&!dy){continue;}double ox=f.ox+dx*g.d,oy=f.oy+dy*g.d,s=score(screen,rs,ox,oy,S,48,std::max(12,48*screen.h/std::max(1,screen.w)));if(s>b.score+0.05){b.score=s;b.ox=ox;b.oy=oy;}}f=b;}
    f.ok=f.score>need(rs,screen,S,0.35)&&f.score>-2;return f;
}
// Map reopened as it was closed (the game keeps zoom and position): the last placement is taken back when the grid lines
// up with it exactly (spacing within 1 %, lines within 8 % of a cell).
inline Fit resume(const Gray& screen,const Grid& g,const Gray& lo,int cells,const Fit& prev,const Gray* hi=nullptr,const Gray8* ultra=nullptr){Refs rs{&lo,hi,ultra};
    Fit f;if(!g.ok||!prev.ok||prev.S<=0){return f;}
    for(double base:{g.d,g.d2})for(int mult:{1,10}){double dExp=prev.S/(cells*mult);if(base<=0||std::fabs(base-dExp)>dExp*0.01){continue;}
        double kx=(prev.ox-g.px)/g.d,ky=(prev.oy-g.py)/g.d;if(std::fabs(kx-std::round(kx))>0.08||std::fabs(ky-std::round(ky))>0.08){continue;}
        f=prev;f.score=score(screen,rs,f.ox,f.oy,f.S,48,std::max(12,48*screen.h/std::max(1,screen.w)));f.ok=f.score>need(rs,screen,f.S,0.3);return f;}
    return f;
}
// Deep in, a search over the whole map finds look-alike places (a 250 m view is one of 6400 cells of 100 m): search only
// around where the map was last placed, within km kilometres (mapKm = the map's side). prior: an earlier placement in
// this match (any age); lx,ly: the cursor then, cx,cy: now (the game zooms around the cursor, so that map point is the
// likeliest centre). Every size the grid allows (1 km / 100 m lines, d or d2) is tried; the best must beat the runner-up
// (another cell) clearly.
inline Fit nearby(const Gray& screen,const Grid& g,const Refs& rs,int cells,double mapKm,const Fit& prior,double lx,double ly,double cx,double cy,double km=1.5){
    Fit best;if(!g.ok||!prior.ok||prior.S<=0){return best;}
    double u=(lx-prior.ox)/prior.S,v=(ly-prior.oy)/prior.S;
    struct Cand {double s,ox,oy,S;};std::vector<Cand> all;const int fx=48,fy=std::max(12,fx*screen.h/std::max(1,screen.w)),qx=28,qy=std::max(8,qx*screen.h/std::max(1,screen.w));
    for(double base:{g.d,g.d2})for(int mult:{1,10}){if(base<=0){continue;}double S=base*cells*mult;if(S<screen.h*0.5||S>screen.h*200){continue;}
        double stepKm=g.d/S*mapKm;int R=std::min(40,(int)std::ceil(km/stepKm));
        double px=cx-u*S,py=cy-v*S,bx=g.px+std::round((px-g.px)/g.d)*g.d,by=g.py+std::round((py-g.py)/g.d)*g.d;
        for(int iy=-R;iy<=R;iy++)for(int ix=-R;ix<=R;ix++){double ox=bx+ix*g.d,oy=by+iy*g.d,s=score(screen,rs,ox,oy,S,qx,qy);if(s>-2)all.push_back({s,ox,oy,S});}}
    std::sort(all.begin(),all.end(),[](const Cand& a,const Cand& b){return a.s>b.s;});if(all.size()>32)all.resize(32);
    for(auto& c:all){c.s=score(screen,rs,c.ox,c.oy,c.S,fx,fy);}std::sort(all.begin(),all.end(),[](const Cand& a,const Cand& b){return a.s>b.s;});
    if(all.empty()){return best;}best.ox=all[0].ox;best.oy=all[0].oy;best.S=all[0].S;best.score=all[0].s;best.map=prior.map;
    for(size_t i=1;i<all.size();i++){if(std::fabs(all[i].ox-best.ox)>g.d*0.5||std::fabs(all[i].oy-best.oy)>g.d*0.5||std::fabs(all[i].S-best.S)>best.S*0.01){best.second=all[i].s;break;}}
    best.ok=best.score>std::max(0.3,need(rs,screen,best.S,0.45))&&best.score-best.second>0.05;return best;
}
// ---- Phase-free placement on the 4096 px map (zoomed in). The grid only hints at the size: in real frames the map was
// 1, 2, 3.34, 5 or 10 times what 1 km lines would give (the game adds finer lines as it zooms), so snapping to the grid
// put markers a cell off or picked a 10x size. Instead the screen (a HUD-free rectangle) is resampled onto the map's
// pixel grid at a size and slid over it with normalized correlation: coarse level first (template 24..48 px wide), the
// best peaks refined level by level (size +-3 % each) and to sub-pixel. On 14 saved real frames the true placement
// scored 0.82..0.96 and the best other place at most 0.23.
struct Pyr {std::vector<Gray8> L;}; // L[0] = the blurred 4096 px map, L[k] = L[0] / 2^k
inline Gray8 half8(const Gray8& g){Gray8 o;o.w=g.w/2;o.h=g.h/2;o.p.resize(size_t(o.w)*o.h);
    for(int y=0;y<o.h;y++)for(int x=0;x<o.w;x++){const unsigned char* a=&g.p[size_t(2*y)*g.w+2*x];o.p[size_t(y)*o.w+x]=(unsigned char)((a[0]+a[1]+a[g.w]+a[g.w+1]+2)/4);}
    return o;}
inline void pyramid(Pyr& p,int levels=6){p.L.resize(1);while((int)p.L.size()<levels&&p.L.back().w>=64)p.L.push_back(half8(p.L.back()));}
struct Tmpl {int w=0,h=0;std::vector<float> p;};
// Screen rectangle from (x0,y0), w x h cells of r screen px each (box average).
inline Tmpl tmpl(const Gray& s,double x0,double y0,int w,int h,double r){Tmpl t;t.w=w;t.h=h;t.p.resize(size_t(std::max(0,w))*std::max(0,h));int k=std::clamp((int)std::ceil(r),1,4);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){float v=0;for(int a=0;a<k;a++)for(int b=0;b<k;b++)v+=s.sample(x0+(x+(b+0.5)/k)*r-0.5,y0+(y+(a+0.5)/k)*r-0.5);t.p[size_t(y)*w+x]=v/(k*k);}
    return t;}
// Correlation with the template's top-left at map pixel (ox,oy); the part off the map is left out (at least 60 % on it).
inline double ncc(const Gray8& R,const Tmpl& T,int ox,int oy){int x0=std::max(0,-ox),y0=std::max(0,-oy),x1=std::min(T.w,R.w-ox),y1=std::min(T.h,R.h-oy);
    if(x1<=x0||y1<=y0||double(x1-x0)*(y1-y0)<0.6*T.w*T.h){return -2;}double a=0,aa=0,b=0,bb=0,ab=0;
    for(int y=y0;y<y1;y++){const unsigned char* r=&R.p[size_t(oy+y)*R.w+ox];const float* t=&T.p[size_t(y)*T.w];float ra=0,raa=0,rb=0,rbb=0,rab=0; // per row in float (vectorizes), rows summed in double
        for(int x=x0;x<x1;x++){float u=t[x],v=r[x];ra+=u;raa+=u*u;rb+=v;rbb+=v*v;rab+=u*v;}a+=ra;aa+=raa;b+=rb;bb+=rbb;ab+=rab;}
    double n=double(x1-x0)*(y1-y0),va=aa-a*a/n,vb=bb-b*b/n;return va<1e-6||vb<1e-6?-2:(ab-a*b/n)/std::sqrt(va*vb);}
// A map point (u,v) expected at screen point (x,y): where the last placement had it (nothing moved), under the cursor now
// (the game zooms around the cursor) or under the cursor then (dragged).
struct Anchor {double u,v,x,y;};
// Sizes the grid allows: the map is k x (1 km cells) for the k seen in real frames, each +-3 %.
inline std::vector<double> gridSizes(const Grid& g,int cells,double lo,double hi){std::vector<double> o;if(!g.ok){return o;}
    for(double base:{g.d,g.d2})for(double k:{1.0,2.0,3.34,5.0,10.0}){if(base<=0){continue;}for(double e:{0.97,1.0,1.03}){double S=base*cells*k*e;if(S>=lo&&S<=hi)o.push_back(S);}}
    return o;}
// Map size within 2.5 % of one the grid allows (no grid: anything). The game's own place names match the names printed on
// the official map at one size (about 2 screens high): without this check the search slid there from a true size 4 % away.
inline bool gridAgrees(const Grid* g,int cells,double S){if(!g||!g->ok){return true;}for(double base:{g->d,g->d2})for(double k:{1.0,2.0,3.34,5.0,10.0}){if(base>0&&std::fabs(S/(base*cells*k)-1)<0.025)return true;}return false;}
// Best placement among sizes S (screen px of the whole map) and anchors, searched within win screen px of each anchor's
// prediction (win < 0: the whole map). ok = clear: score >= 0.6 and 0.12 over the best other place.
// stop: polled between sizes and rows (the whole-map pass takes up to a second; closing the map must not wait for it).
inline Fit locate(const Gray& s,const Pyr& P,const std::vector<double>& sizes,const std::vector<Anchor>& anchors,double win,int map,const Grid* grid=nullptr,int cells=8,const std::function<bool()>& stop={}){
    Fit best;if(P.L.empty()||sizes.empty()){return best;}const double gx0=s.w*0.03,gy0=s.h*0.06,rw=s.w*0.94,rh=s.h*0.72; // HUD-free: tabs above, team list / panels below
    auto levelFor=[&](double S,double minW){int lv=0;while(lv+1<(int)P.L.size()&&rw/(S/P.L[lv+1].w)>=minW)lv++;return lv;};
    struct Pk {double s,S;int lv,x,y;};std::vector<Pk> pk;
    for(double S:sizes){if(stop&&stop()){return best;}int lv=levelFor(S,win<0?16:24);const Gray8& M=P.L[lv];double r=S/M.w;Tmpl T=tmpl(s,gx0,gy0,(int)(rw/r),(int)(rh/r),r);if(T.w<10||T.h<4){continue;}
        std::vector<Pk> here;
        auto at=[&](int x,int y){double v=ncc(M,T,x,y);if(v>-2)here.push_back({v,S,lv,x,y});};
        if(win<0){for(int y=-T.h/3;y<M.h-T.h*2/3;y++){if(y%32==0&&stop&&stop()){return best;}for(int x=-T.w/3;x<M.w-T.w*2/3;x++)at(x,y);}}
        else{int R=(int)std::ceil(win/r);std::vector<std::pair<int,int>> seen;
            for(auto& a:anchors){int cx=(int)std::lround((a.u+(gx0-a.x)/S)*M.w),cy=(int)std::lround((a.v+(gy0-a.y)/S)*M.h);
                if(std::any_of(seen.begin(),seen.end(),[&](const std::pair<int,int>& q){return std::abs(q.first-cx)<=R/2&&std::abs(q.second-cy)<=R/2;})){continue;}seen.push_back({cx,cy});
                for(int y=cy-R;y<=cy+R;y++)for(int x=cx-R;x<=cx+R;x++)at(x,y);}}
        std::sort(here.begin(),here.end(),[](const Pk& a,const Pk& b){return a.s>b.s;});
        int sep=std::max(2,T.w/6),n=0;for(auto& p:here){if(n>=4){break;}bool dup=false;for(auto& q:pk)if(q.S==S&&std::abs(q.x-p.x)<=sep&&std::abs(q.y-p.y)<=sep)dup=true;if(!dup){pk.push_back(p);n++;}}}
    std::sort(pk.begin(),pk.end(),[](const Pk& a,const Pk& b){return a.s>b.s;});
    // Peaks are kept apart by where the screen centre lands on the map (15 % of the view) and by size (another size at the
    // same centre is another reading: a 2x size scored lower at the coarse level but 0.89 against 0.76 once refined).
    auto centre=[&](double S,double tu,double tv){return std::pair<double,double>{tu+(s.w/2-gx0)/S,tv+(s.h/2-gy0)/S};};
    std::vector<Pk> top;for(auto& p:pk){auto c=centre(p.S,double(p.x)/P.L[p.lv].w,double(p.y)/P.L[p.lv].h);bool dup=false;
        for(auto& q:top){auto d=centre(q.S,double(q.x)/P.L[q.lv].w,double(q.y)/P.L[q.lv].h);if(std::fabs(std::log(p.S/q.S))<0.1&&std::fabs(c.first-d.first)*p.S<rw*0.15&&std::fabs(c.second-d.second)*p.S<rh*0.15)dup=true;}
        if(!dup){top.push_back(p);}if(top.size()>=6){break;}}
    struct Res {double s,S,u,v;};std::vector<Res> res;
    for(auto& p:top){double S=p.S,tu=double(p.x)/P.L[p.lv].w,tv=double(p.y)/P.L[p.lv].h,sc=p.s;int fine=levelFor(S,192);bool first=true;
        for(int lv=p.lv-1;lv>=fine;lv--){const Gray8& M=P.L[lv];double bs=-2,bu=tu,bv=tv,bS=S;
            for(double k:{0.97,0.985,1.0,1.015,1.03}){if(!first&&(k<0.98||k>1.02)){continue;}double S2=S*k;int D=first?2:1;double r=S2/M.w;Tmpl T=tmpl(s,gx0,gy0,(int)(rw/r),(int)(rh/r),r);
                // size changed about the region's centre
                double cu=tu+rw/2/S-rw/2/S2,cv=tv+rh/2/S-rh/2/S2;int cx=(int)std::lround(cu*M.w),cy=(int)std::lround(cv*M.h);
                for(int y=cy-D;y<=cy+D;y++)for(int x=cx-D;x<=cx+D;x++){double v=ncc(M,T,x,y);if(v>bs){bs=v;bu=double(x)/M.w;bv=double(y)/M.h;bS=S2;}}}
            tu=bu;tv=bv;S=bS;sc=bs;first=false;if(sc<=-2){break;}}
        // Sub-pixel: a parabola through the neighbours at the finest level.
        if(sc>-2){const Gray8& M=P.L[fine];double r=S/M.w;Tmpl T=tmpl(s,gx0,gy0,(int)(rw/r),(int)(rh/r),r);int x=(int)std::lround(tu*M.w),y=(int)std::lround(tv*M.h);
            double l=ncc(M,T,x-1,y),c=ncc(M,T,x,y),rr=ncc(M,T,x+1,y),t=ncc(M,T,x,y-1),b=ncc(M,T,x,y+1),dx=0,dy=0;
            if(l>-2&&rr>-2&&l-2*c+rr<0){dx=std::clamp((l-rr)/(2*(l-2*c+rr)),-0.5,0.5);}if(t>-2&&b>-2&&t-2*c+b<0)dy=std::clamp((t-b)/(2*(t-2*c+b)),-0.5,0.5);
            tu=(x+dx)/M.w;tv=(y+dy)/M.h;sc=c;}
        if(gridAgrees(grid,cells,S))res.push_back({sc,S,tu,tv});}
    std::sort(res.begin(),res.end(),[](const Res& a,const Res& b){return a.s>b.s;});if(res.empty()){return best;}
    best.score=res[0].s;best.S=res[0].S;best.ox=gx0-res[0].u*best.S;best.oy=gy0-res[0].v*best.S;best.map=map;
    auto c0=centre(res[0].S,res[0].u,res[0].v);
    for(size_t i=1;i<res.size();i++){auto c=centre(res[i].S,res[i].u,res[i].v);if(std::fabs(c.first-c0.first)*best.S>=rw*0.15||std::fabs(c.second-c0.second)*best.S>=rh*0.15){best.second=std::max(best.second,res[i].s);}}
    best.ok=best.score>=0.6&&best.score-best.second>=0.12;return best;}
// Zoomed in (map at least the screen height) with the 4096 px map. From an earlier placement of this match (prior):
// first near its size and the grid's sizes within 1.3x (most frames: a drag or one wheel step), then the grid's sizes
// within 6x (a fast scroll went 3.3x in 0.3 s). wholeMap: then every grid size over the whole map (lost; slow when deep).
inline Fit deepFit(const Gray& s,const Grid& g,const Pyr& P,int cells,int map,const Fit* prior,const std::vector<Anchor>& anchors,bool wholeMap,const std::function<bool()>& stop={}){
    Fit f;double win=s.w*0.94*0.25;
    if(prior&&prior->ok&&prior->S>0&&!anchors.empty()){std::vector<double> a={prior->S*0.95,prior->S,prior->S*1.05};for(double S:gridSizes(g,cells,prior->S/1.3,prior->S*1.3))a.push_back(S);
        f=locate(s,P,a,anchors,win,map,&g,cells,stop);if(f.ok&&f.score>=0.8){return f;}
        auto b=gridSizes(g,cells,prior->S/6,prior->S*6);if(!g.ok){for(double S=prior->S/3;S<=prior->S*3;S*=1.07)b.push_back(S);}
        Fit w=locate(s,P,b,anchors,win,map,&g,cells,stop);if(w.score>f.score){f=w;}if(f.ok){return f;}}
    if(wholeMap){Fit w=locate(s,P,gridSizes(g,cells,s.h*1.0,s.h*120),{},-1,map,&g,cells,stop);if(w.score>f.score||w.ok){f=w;}}
    return f;}
// A placement found another way (grid snapping, whole-view search), checked and refined on the 4096 px map: kept only if
// it holds up there (deep in, the 819 px map passed look-alikes at 0.3-0.4).
inline Fit verify(const Gray& s,const Pyr& P,const Fit& f,const Grid& g,int cells){if(!f.ok||f.S<=0){return Fit{};}
    std::vector<Anchor> a={{(s.w/2-f.ox)/f.S,(s.h/2-f.oy)/f.S,s.w/2.0,s.h/2.0}};return locate(s,P,{f.S*0.97,f.S,f.S*1.03},a,s.w*0.94*0.1,f.map,&g,cells);}
// screen: blurred gray of the screen (any size); refs: blurred gray official maps (square). cells = grid cells per
// map side (8 for 8 km maps), tried also at 10x for 100 m lines when zoomed in.
// maxS > 0: only placements where the whole map is at most that big (map identification is only trusted near the whole view).
// ultra: 4096 px maps by index (null or empty = not there).
inline Fit fit(const Gray& screen,const Grid& g,const std::vector<Gray>& refs,const std::vector<int>& cells,double minScore=0.45,const std::vector<Gray>* hi=nullptr,double maxS=0,const std::vector<const Gray8*>* ultra=nullptr){
    Fit best;if(!g.ok){return best;}
    auto ref=[&](int m,double)->Refs{return Refs{&refs[m],hi?&(*hi)[m]:nullptr,ultra&&m<(int)ultra->size()?(*ultra)[m]:nullptr};};
    // Coarse pass (16 x 9 samples) over every placement, then the 12 best again with 48 x 27.
    struct Cand {double s,ox,oy,S;int m;};std::vector<Cand> all;
    const int fx=48,fy=std::max(12,fx*screen.h/std::max(1,screen.w)),cx=16,cy=std::max(6,cx*screen.h/std::max(1,screen.w));
    for(size_t m=0;m<refs.size();m++)for(double base:{g.d,g.d2})for(int mult:{1,10}){
        if(base<=0){continue;}double S=base*cells[m]*mult;if(S<screen.h*0.5||S>screen.h*120||(maxS>0&&S>maxS)){continue;}
        int nx=(int)std::ceil((screen.w+S)/g.d)+1,ny=(int)std::ceil((screen.h+S)/g.d)+1;if(double(nx)*ny>40000){continue;}
        Refs r=ref((int)m,S);
        for(int iy=0;iy<ny;iy++)for(int ix=0;ix<nx;ix++){double ox=g.px-S+ix*g.d,oy=g.py-S+iy*g.d,s=score(screen,r,ox,oy,S,cx,cy);if(s>-2)all.push_back({s,ox,oy,S,(int)m});}
    }
    std::sort(all.begin(),all.end(),[](const Cand& a,const Cand& b){return a.s>b.s;});if(all.size()>12)all.resize(12);
    std::vector<Cand> fine;for(auto& c:all){fine.push_back({score(screen,ref(c.m,c.S),c.ox,c.oy,c.S,fx,fy),c.ox,c.oy,c.S,c.m});}
    for(auto& c:fine){if(c.s>best.score){best.score=c.s;best.ox=c.ox;best.oy=c.oy;best.S=c.S;best.map=c.m;}}
    // The runner-up that makes it ambiguous: another map, or the same map at the same size one or more cells off.
    // (Another size of the same map, e.g. the 10x reading, can correlate by chance on the smooth coast-and-sea layout;
    // a real Erangel match at 87 % was rejected that way.)
    for(auto& c:fine){bool same=c.m==best.map&&std::fabs(c.S-best.S)<1e-6&&std::fabs(c.ox-best.ox)<1e-6&&std::fabs(c.oy-best.oy)<1e-6;
        if(same){continue;}if(c.m!=best.map||std::fabs(c.S-best.S)<best.S*0.01){best.second=std::max(best.second,c.s);}}
    // Final touch: small shifts and scale steps around the best placement.
    if(best.map>=0){for(int round=0;round<2;round++){double step=round?0.25:1.0;Fit start=best;
        for(double k:{-0.004,-0.002,0.0,0.002,0.004})for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++){double S=start.S*(1+k),ox=start.ox+dx*step-(S-start.S)/2,oy=start.oy+dy*step-(S-start.S)/2;
            double s=score(screen,ref(best.map,S),ox,oy,S,fx,fy);if(s>best.score){best.score=s;best.ox=ox;best.oy=oy;best.S=S;}}}}
    // Size and offset snapped to the measured grid, kept only when the match does not get worse.
    if(best.map>=0){double mult=std::round(best.S/(g.d*cells[best.map]));if(mult<1){mult=1;}Fit snap=best;snap.S=g.d*cells[best.map]*mult;
        snap.ox=g.px+std::round((best.ox-g.px)/g.d)*g.d;snap.oy=g.py+std::round((best.oy-g.py)/g.d)*g.d;snap.score=score(screen,ref(best.map,snap.S),snap.ox,snap.oy,snap.S,fx,fy);
        if(snap.score>=best.score-0.01){best.ox=snap.ox;best.oy=snap.oy;best.S=snap.S;best.score=std::max(best.score,snap.score);}}
    best.ok=best.score>minScore&&best.score-best.second>0.05;return best;
}
}
