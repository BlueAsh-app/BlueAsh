#include "countdown.h"
#include <cassert>
#include <iostream>
int main(){
 using countdown::values;
 assert((values(L"게임 시작까지 0:45 27")==std::vector<int>{45,27}));
 assert((values(L"255 270 N 285")==std::vector<int>{}));
 assert((values(L"생존 64 1:05")==std::vector<int>{64,65}));
 assert((values(L"12345 7")==std::vector<int>{7}));
 // Countdown 30..1 read once a second next to a fixed compass (15) and a rising player count.
 countdown::Tracker t;int players=40;bool fired=false;double at=0;
 for(int s=0;s<40&&!fired;s++){double now=100+s;std::vector<int> v={15,players++};if(30-s>=1)v.push_back(30-s);
  if(s%7==3)v.clear();// missed reads
  t.feed(now,v);if(t.due(now)){fired=true;at=now;}}
 assert(fired&&at>=130.9&&at<=131.1);
 // Static numbers alone never lock.
 countdown::Tracker q;for(int s=0;s<20;s++)q.feed(s,{15,60,90});assert(!q.locked);
 // A count that rises one per second (player joins) never locks either.
 countdown::Tracker r;for(int s=0;s<20;s++)r.feed(s,{40+s});assert(!r.locked);
 // Strict reading of the digit crop.
 assert(countdown::strictNumber(L"22")==22&&countdown::strictNumber(L" 1 ")==1&&countdown::strictNumber(L"2 8")==28);
 assert(countdown::strictNumber(L"1 45 4")==-1&&countdown::strictNumber(L"4덙1,결")==-1&&countdown::strictNumber(L"")==-1&&countdown::strictNumber(L"0")==-1);
 // Blob filter: a digit-sized bar survives, a big cloud and specks are cleared.
 {int w=60,h=40;std::vector<unsigned char> m(size_t(w)*h,255);auto box=[&](int x0,int y0,int x1,int y1){for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)m[size_t(y)*w+x]=0;};
  box(10,10,14,30);box(30,5,58,39);box(5,2,6,3);int kept=countdown::keepDigitBlobs(m,w,h,15,25);
  assert(kept==1&&m[size_t(20)*w+12]==0&&m[size_t(20)*w+40]==255&&m[size_t(2)*w+5]==255);}
 std::cout<<"Countdown tests passed\n";
}
