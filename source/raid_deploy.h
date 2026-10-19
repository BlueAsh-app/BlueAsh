#pragma once
#include <string>
#include <algorithm>
#include <cmath>
#include <cwctype>
namespace raiddeploy {
// EFT deployment timer is mm:ss.mmm (not the in-raid mm:ss clock).
inline int milliseconds(std::wstring text){
 text.erase(std::remove_if(text.begin(),text.end(),[](wchar_t c){return std::iswspace(c)!=0;}),text.end());
 // Korean Windows OCR reads EFT's slashed zero as these enclosed/boxed glyphs (real deployment screenshot).
 for(auto& c:text){if(c==L'O'||c==L'o'||c==0x3268||c==0x52ff||c==0x56eb||c==0x3208)c=L'0';}
 if((text.size()!=8&&text.size()!=9)||text.substr(0,3)!=L"00:"||(text[5]!=L'.'&&text[5]!=L','&&text[5]!=L'+'))return -1;
 for(size_t i=3;i<text.size();i++){if(i!=5&&(text[i]<L'0'||text[i]>L'9'))return -1;}
 int seconds=(text[3]-L'0')*10+text[4]-L'0';if(seconds>59)return -1;
 int fraction=0;for(size_t i=6;i<text.size();i++)fraction=fraction*10+text[i]-L'0';
 return seconds*1000+fraction*(text.size()==8?10:1);
}
struct Tracker {
 double previousAt=0;int previous=-1;bool fired=false;
 bool feed(double at,int ms,bool ready){
  if(fired)return false;
  if(!ready){previous=-1;return false;}
  if(ms<0)return false;
  double dt=at-previousAt;bool descending=previous>=0&&ms<previous&&dt>=0.04&&dt<=1.5&&std::abs((previous-ms)/1000.0-dt)<=0.35;
  previous=ms;previousAt=at;
  if(descending&&ms<=1000){fired=true;return true;}return false;
 }
};
struct Gate {
 bool armed=false,done=false,cancelled=false,entered=false;unsigned token=0;double since=0;std::string scene;
 void cancel(){armed=false;cancelled=true;entered=false;token++;}
 void finish(){armed=false;done=true;token++;}
 void line(const std::string& s,double at){
  bool begin=s.find("application|scene preset path:")!=std::string::npos;
  if(s.find("application|Network game matching aborted")!=std::string::npos||s.find("application|Network game matching cancelled")!=std::string::npos||s.find("Got notification | UserMatchOver")!=std::string::npos||
     (entered&&(s.find("SelectProfile ProfileId:")!=std::string::npos||s.find("SelectedProfile ProfileId:")!=std::string::npos||s.find("PrepareSelectedProfileLocally ProfileId:")!=std::string::npos))){cancel();return;}
  if(begin&&s!=scene){scene=s;armed=false;done=false;cancelled=false;entered=false;token++;}
  bool matched=s.find("application|TRACE-NetworkGameCreate profileStatus")!=std::string::npos||s.find("application|MatchingCompleted")!=std::string::npos;
  if(matched&&cancelled){cancelled=false;done=false;}
  bool signal=begin||matched||s.find("application|GamePooled")!=std::string::npos||s.find("application|GameStarting")!=std::string::npos||s.find("application|GameStarted")!=std::string::npos;
  if(signal&&!armed&&!done&&!cancelled){armed=true;since=at;token++;}
  if(s.find("application|GameStarted")!=std::string::npos)entered=true;
 }
};
// The red GET READY panel anchors the timer independently of display language and window borders.
struct Box {int x,y,w,h;};
inline Box box(int W,int H){int x=std::max(0,W/2-H*35/100);return {x,H*48/100,std::min(W-x,H*70/100),std::min(H-H*48/100,H*33/100)};}
struct TimerBox {bool ready=false;int x=0,y=0,w=0,h=0;};
inline TimerBox locate(const unsigned char* px,size_t pitch,const Box& b,int H){
 int top=-1,bottom=-1,left=b.w,right=-1;
 for(int y=0;y<b.h&&y<H*18/100;y++){
  int lo=b.w,hi=-1,count=0;auto p=px+size_t(y)*pitch;
  for(int x=0;x<b.w;x++,p+=4)if(p[2]>130&&p[2]>p[1]*2&&p[2]>p[0]*2&&p[1]<100){lo=std::min(lo,x);hi=x;count++;}
  if(count>H*12/100&&hi-lo>H*15/100&&hi-lo<H*35/100){if(top<0)top=y;bottom=y;left=std::min(left,lo);right=std::max(right,hi);}
  else if(top>=0&&y-bottom>H/200)break;
 }
 if(top<0||bottom-top<H/70||bottom-top>H*7/100)return {};
 int cx=(left+right)/2,cy=(top+bottom)/2;
 TimerBox r;r.x=cx-H*7/100;r.y=cy+H*17/100;r.w=H*20/100;r.h=H*5/100;
 r.ready=r.x>=0&&r.y>=0&&r.x+r.w<=b.w&&r.y+r.h<=b.h;return r;
}
}
