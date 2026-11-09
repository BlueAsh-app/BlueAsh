#include <cassert>
#include <cstdio>
#include "dlss_approx.h"
int main(){
    // A 64x36 picture: a horizontal ramp with a bright square, alpha 255.
    int w=64,h=36;std::vector<unsigned char> px((size_t)w*h*4);
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){unsigned char* p=&px[((size_t)y*w+x)*4];int v=40+x*2+((x>20&&x<30&&y>10&&y<20)?100:0);p[0]=(unsigned char)v;p[1]=(unsigned char)(v*0.9);p[2]=(unsigned char)(v*0.8);p[3]=255;}
    // Strength 0: the picture is returned unchanged.
    dlssnr::Settings off=dlssnr::defaults();off.v[0]=0;assert(dlssapprox::apply(px,w,h,off)==px);
    // A preset changes the picture, keeps alpha, and never moves a pixel's brightness past MaxRatio (plus rounding).
    for(int which:{1,2,3}){dlssnr::Settings s=dlssnr::builtin(which);s.v[5]=1.5;auto out=dlssapprox::apply(px,w,h,s);assert(out.size()==px.size()&&out!=px);
        for(size_t i=0;i<px.size();i+=4){assert(out[i+3]==255);double a=.0722*px[i]+.7152*px[i+1]+.2126*px[i+2],b=.0722*out[i]+.7152*out[i+1]+.2126*out[i+2];assert(b<=a*1.5*1.25+3&&b>=a/1.5/1.25-3);}}
    // Model A and model B give different pictures with the same knobs.
    dlssnr::Settings a=dlssnr::defaults(),b=a;a.preset=1;b.preset=2;assert(dlssapprox::apply(px,w,h,a)!=dlssapprox::apply(px,w,h,b));
    // Short or empty input is returned as it is, never read past.
    std::vector<unsigned char> tiny(8,7);assert(dlssapprox::apply(tiny,64,36,a)==tiny);
    // Face pass: a skin-coloured patch inside a face box changes; a grey picture with no skin is the same with or without it.
    {int fw=80,fh=80;std::vector<unsigned char> skin((size_t)fw*fh*4);for(size_t i=0;i<skin.size();i+=4){skin[i]=120;skin[i+1]=150;skin[i+2]=210;skin[i+3]=255;}
     std::vector<dlssapprox::Box> boxes={{20,20,40,40}};auto prep=dlssapprox::prepare(skin,fw,fh,boxes);dlssnr::Settings c=dlssnr::builtin(3);
     auto with=dlssapprox::apply(skin,fw,fh,c,&prep),without=dlssapprox::apply(skin,fw,fh,c);size_t mid=((size_t)40*fw+40)*4;assert(with[mid+2]!=without[mid+2]||with[mid]!=without[mid]);
     std::vector<unsigned char> grey((size_t)fw*fh*4,128);auto pg=dlssapprox::prepare(grey,fw,fh,{});assert(dlssapprox::apply(grey,fw,fh,c,&pg)==dlssapprox::apply(grey,fw,fh,c));
     auto other=dlssapprox::prepare(grey,10,10,{});assert(dlssapprox::apply(grey,fw,fh,c,&other)==dlssapprox::apply(grey,fw,fh,c));} // a stale size is ignored
    puts("DLSS 5 approximate preview tests passed");
}
