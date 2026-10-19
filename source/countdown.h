#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
// Starting-island countdown from OCR text of the top/middle of the PUBG screen. The layout is not assumed:
// any number that falls one per second (t+value stays constant) is the countdown, which leaves out the
// compass bearings and player counts (they stay or rise). "m:ss" counts as seconds.
namespace countdown {
inline std::vector<int> values(const std::wstring& t){
    std::vector<int> out;size_t i=0,n=t.size();
    auto digit=[&](size_t k){return k<n&&t[k]>=L'0'&&t[k]<=L'9';};
    while(i<n){
        if(!digit(i)){i++;continue;}
        size_t a=i;while(digit(i))i++;int first=std::stoi(t.substr(a,std::min<size_t>(i-a,6)));
        if(i<n&&(t[i]==L':'||t[i]==L'.')&&digit(i+1)&&digit(i+2)&&!digit(i+3)&&i-a<=2){int sec=(t[i+1]-L'0')*10+(t[i+2]-L'0');i+=3;if(sec<60&&first<=5)out.push_back(first*60+sec);continue;}
        if(i-a<=3&&first>=1&&first<=180)out.push_back(first);
    }
    return out;
}
// OCR text of the digit crop counts only when it is nothing but a 1-2 digit number (garbage such as "1 45 4" from
// clouds and terrain locked the tracker onto a false end in a real match). -1 otherwise.
inline int strictNumber(const std::wstring& t){
    std::wstring d;for(wchar_t c:t){if(c==L' '||c==L'\n'||c==L'\r')continue;if(c<L'0'||c>L'9')return -1;d+=c;}
    if(d.empty()||d.size()>2){return -1;}int v=std::stoi(d);return v>=1&&v<=99?v:-1;
}
// In a black-on-white mask (0 = text), keep only blobs shaped like the countdown digits: height minH..maxH, width at
// most maxH. Sky, clouds and map text become white. Returns the number of blobs kept.
// boxes (optional) receives x0,y0,x1,y1 of each kept blob, left to right.
inline int keepDigitBlobs(std::vector<unsigned char>& m,int w,int h,int minH,int maxH,std::vector<std::vector<int>>* boxes=nullptr){
    std::vector<int> lab(m.size(),0);int kept=0,id=0;std::vector<int> stack;
    for(int i=0;i<w*h;i++){if(m[i]||lab[i])continue;id++;stack.assign(1,i);lab[i]=id;std::vector<int> px;int x0=w,x1=-1,y0=h,y1=-1;
        while(!stack.empty()){int p=stack.back();stack.pop_back();px.push_back(p);int x=p%w,y=p/w;x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);
            const int nb[4]={p-1,p+1,p-w,p+w};for(int k=0;k<4;k++){int q=nb[k];if(q<0||q>=w*h)continue;if((k==0&&x==0)||(k==1&&x==w-1))continue;if(!m[q]&&!lab[q]){lab[q]=id;stack.push_back(q);}}}
        int bw=x1-x0+1,bh=y1-y0+1;bool digit=bh>=minH&&bh<=maxH&&bw<=maxH&&bw>=std::max(2,minH/10)&&y0>0&&y1<h-1;
        if(digit){kept++;if(boxes)boxes->push_back({x0,y0,x1,y1});}else for(int p:px)m[p]=255;}
    if(boxes){std::sort(boxes->begin(),boxes->end());}return kept;
}
struct Tracker {
    struct Seen {double t;int v;};
    std::vector<Seen> seen;double end=0;bool locked=false;
    void reset(){seen.clear();end=0;locked=false;}
    // t in seconds. Locks when 3+ readings within 1.2 s of one end time (t+v) and 2+ different values.
    void feed(double t,const std::vector<int>& vals){
        for(int v:vals)seen.push_back({t,v});
        seen.erase(std::remove_if(seen.begin(),seen.end(),[&](const Seen& s){return t-s.t>25;}),seen.end());
        double best=0;int bestCount=0;
        for(auto& c:seen){double k=c.t+c.v;int count=0,lo=1000,hi=-1;for(auto& s:seen)if(std::fabs(s.t+s.v-k)<=1.2){count++;lo=std::min(lo,s.v);hi=std::max(hi,s.v);}
            if(hi>lo&&count>bestCount){bestCount=count;best=k;}}
        if(bestCount>=3){double sum=0;int m=0;for(auto& s:seen)if(std::fabs(s.t+s.v-best)<=1.2){sum+=s.t+s.v;m++;}end=sum/m;locked=true;}
    }
    // The screen shows 1 during the last second before end; the plane boarding follows ~2 s after that.
    bool due(double now)const{return locked&&now>=end+1;}
};
}
