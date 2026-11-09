#pragma once
// 근사 미리보기 (NOT DLSS 5): a plain image filter BlueAsh runs on a saved picture to suggest the direction of a preset
// (more local contrast, lifted shadows, richer colour). DLSS 5 is NVIDIA's neural model fed with the game's depth and
// motion, which a filter on one picture cannot reproduce; the page labels this picture as an approximation everywhere.
// The preset's knobs steer it: TransferStrength x Intensity = how much of the filtered picture is mixed in,
// LocalStructure = local contrast, LocalTone = shadow lift / highlight roll-off, ColourStrength = saturation,
// MaxRatio = how far any pixel may move from the original, WorkingScale = the size of the local-contrast blur,
// model A = a warmer, slightly darker-edged grade than model B. Pure: BGRA bytes in, BGRA bytes out (alpha kept).
#include <vector>
#include <algorithm>
#include <cmath>
#include "dlss_nr.h"
namespace dlssapprox {
// Box blur of one float plane, radius r, both directions (three passes ≈ a gaussian).
inline void blur(std::vector<float>& p,int w,int h,int r){if(r<1||w<1||h<1)return;std::vector<float> t(p.size());
    for(int pass=0;pass<3;pass++){
        for(int y=0;y<h;y++){float s=0;int n=0;const float* row=&p[(size_t)y*w];for(int x=-r;x<w;x++){int in=x+r,out=x-r-1;if(in<w){s+=row[in];n++;}if(out>=0){s-=row[out];n--;}if(x>=0)t[(size_t)y*w+x]=s/n;}}
        for(int x=0;x<w;x++){float s=0;int n=0;for(int y=-r;y<h;y++){int in=y+r,out=y-r-1;if(in<h){s+=t[(size_t)in*w+x];n++;}if(out>=0){s-=t[(size_t)out*w+x];n--;}if(y>=0)p[(size_t)y*w+x]=s/n;}}}}
struct Box {int x,y,w,h;}; // a face Windows found (dlss_face.h), in picture pixels
// What the face pass needs that depends only on the picture (made once per picture, off the UI thread): where skin and
// faces are (m), the upper-face band for eyes and brows (eye), and the picture's colours softened (soft, BGR planes).
struct Faces {int w=0,h=0;std::vector<float> m,eye,soft[3];};
inline Faces prepare(const std::vector<unsigned char>& in,int w,int h,const std::vector<Box>& faces){Faces f;size_t n=(size_t)w*h;if(w<1||h<1||in.size()<n*4)return f;f.w=w;f.h=h;
    f.m.resize(n);f.eye.assign(n,0.f);auto band=[](double v,double lo,double hi,double soft){return std::clamp(std::min(v-lo,hi-v)/soft,0.0,1.0);};
    for(size_t i=0;i<n;i++){const unsigned char* p=&in[i*4];double b=p[0],g=p[1],r=p[2],Y=.299*r+.587*g+.114*b,cb=128-.168736*r-.331264*g+.5*b,cr=128+.5*r-.418688*g-.081312*b;
        f.m[i]=(float)(band(cb,77,127,8)*band(cr,133,173,8)*band(Y,35,250,15)*.6);}
    // Skin colour alone also catches wood, leather and warm light: with faces found, skin counts fully only near them
    // (a neck, hands) and fades to 15 % farther away; without faces it counts at half.
    {bool any=false;for(const auto& b:faces){any=any||(b.w>=4&&b.h>=4);}
     for(int y=0;y<h;y++)for(int x=0;x<w;x++){float reach=any?.15f:.5f;for(const auto& b:faces){if(b.w<4||b.h<4)continue;double dx=(x-(b.x+b.w/2.0))/(b.w*1.8),dy=(y-(b.y+b.h*1.2))/(b.h*2.6),d=dx*dx+dy*dy;reach=std::max(reach,(float)std::clamp(1.2-d,.15,1.0));}
         f.m[(size_t)y*w+x]*=reach;}}
    for(const auto& b:faces){if(b.w<4||b.h<4)continue;double cx=b.x+b.w/2.0,cy=b.y+b.h*.45,rx=b.w*.62,ry=b.h*.8;
        for(int y=std::max(0,(int)(cy-ry));y<std::min(h,(int)(cy+ry)+1);y++)for(int x=std::max(0,(int)(cx-rx));x<std::min(w,(int)(cx+rx)+1);x++){
            double d=((x-cx)/rx)*((x-cx)/rx)+((y-cy)/ry)*((y-cy)/ry);if(d>=1)continue;size_t i=(size_t)y*w+x;float e=(float)(1-d*d);f.m[i]=std::max(f.m[i],e*std::max(f.m[i]/.6f,.55f));
            double ey=(y-b.y)/(double)b.h;if(ey>.2&&ey<.55&&x>b.x&&x<b.x+b.w){f.eye[i]=std::max(f.eye[i],(float)(e*std::sin((ey-.2)/.35*3.14159)));}}}
    blur(f.m,w,h,std::max(1,(int)std::lround(std::min(w,h)*.004)));
    for(int ch=0;ch<3;ch++){f.soft[ch].resize(n);for(size_t i=0;i<n;i++)f.soft[ch][i]=in[i*4+ch]/255.f;blur(f.soft[ch],w,h,std::max(1,(int)std::lround(std::min(w,h)*.006)));}
    return f;}
inline std::vector<unsigned char> apply(const std::vector<unsigned char>& in,int w,int h,const dlssnr::Settings& s,const Faces* faces=nullptr){
    std::vector<unsigned char> out(in);if(w<1||h<1||in.size()<(size_t)w*h*4)return out;
    double mix=std::clamp(s.v[0]*s.v[2],0.0,6.0)*.8;if(mix<=0)return out; // above 1 the change is pushed past the model picture (strengths over 1)
    double structure=s.v[3],tone=s.v[4],colour=s.v[1],ratio=std::max(1.0,s.v[5]);bool modelA=s.preset==1;
    size_t n=(size_t)w*h;std::vector<float> L(n),detail(n),wide(n);
    for(size_t i=0;i<n;i++){const unsigned char* p=&in[i*4];L[i]=(.0722f*p[0]+.7152f*p[1]+.2126f*p[2])/255.f;}
    detail=L;wide=L;int dr=std::max(1,(int)std::lround(std::min(w,h)*.006*s.v[6])),wr=std::max(2,(int)std::lround(std::min(w,h)*.04*s.v[6]));blur(detail,w,h,dr);blur(wide,w,h,wr);
    for(size_t i=0;i<n;i++){int x=(int)(i%w),y=(int)(i/w);const unsigned char* p=&in[i*4];unsigned char* q=&out[i*4];
        double l=L[i],nl=l;
        nl+=structure*.9*(l-detail[i])+structure*.35*(l-wide[i]);           // local structure: fine detail and local contrast
        nl+=tone*.22*(.5-wide[i])*(1-std::fabs(2*l-1));                     // local tone: lift shadows, pull highlights in
        nl=nl+.12*structure*(nl-.5)*(1-std::fabs(2*nl-1));                  // gentle S-curve
        if(modelA){double dx=x/(double)w-.5,dy=y/(double)h-.5;nl*=1-.18*(dx*dx+dy*dy);} // model A: darker edges
        nl=std::clamp(nl,0.0,1.0);double k=l>1e-4?nl/l:1.0;k=std::clamp(k,1/ratio,ratio);
        double c[3];for(int ch=0;ch<3;ch++)c[ch]=p[ch]/255.0*k;
        double g=.0722*c[0]+.7152*c[1]+.2126*c[2],sat=1+.35*colour;for(int ch=0;ch<3;ch++)c[ch]=g+(c[ch]-g)*sat;
        if(modelA){c[2]*=1+.05*colour;c[0]*=1-.05*colour;}                    // model A: warmer (BGRA: 0 = blue, 2 = red)
        for(int ch=0;ch<3;ch++){double o=p[ch]/255.0,v=std::clamp(c[ch],0.0,1.0);q[ch]=(unsigned char)std::lround(255*std::clamp(o+(v-o)*mix,0.0,1.0));}}
    // Faces, the part DLSS 5 changes most: skin found by colour (the YCbCr skin range) and the face boxes Windows found.
    // There the picture gets softer, translucent-looking skin (the colour blurred, the fine pores put back), warm light
    // through the shadows, a soft sheen on the lit side, and sharper eyes and brows in the upper face. Still a filter.
    double fs=std::min(mix,1.5)*(.5+.25*(structure+tone));if(fs<.01||!faces||faces->w!=w||faces->h!=h)return out;
    const auto& m=faces->m;const auto& eye=faces->eye;const auto* sc=faces->soft;
    double warm=colour*(modelA?1.3:1.0);
    for(size_t i=0;i<n;i++){double k=m[i]*fs,ke=eye[i]*fs;if(k<.003&&ke<.003)continue;unsigned char* q=&out[i*4];double c[3];for(int ch=0;ch<3;ch++)c[ch]=q[ch]/255.0;
        double pores=L[i]-detail[i];
        for(int ch=0;ch<3;ch++){c[ch]+=(sc[ch][i]-in[i*4+ch]/255.0)*.5*std::min(k,1.0)+pores*(.6*structure*k+1.2*structure*ke);} // the softening the source picture gets, carried onto the graded one
        double l2=.0722*c[0]+.7152*c[1]+.2126*c[2],shade=1-l2;
        c[2]+=.07*warm*k*shade;c[1]+=.02*warm*k*shade;c[0]-=.04*warm*k*shade;      // light through skin in the shadows (BGRA: 2 = red)
        if(l2>.55){double sh=.1*tone*k*(l2-.55)/.45;for(auto& v:c)v+=sh;}          // soft sheen on the lit side
        for(int ch=0;ch<3;ch++)q[ch]=(unsigned char)std::lround(255*std::clamp(c[ch],0.0,1.0));}
    return out;}
}
