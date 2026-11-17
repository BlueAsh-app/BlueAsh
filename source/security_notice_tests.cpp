#include <atomic>
#include <string>
#include <iostream>
namespace i18n { std::atomic<int> language{0}; }
#include "security_notice.h"
int main(){for(int i=0;i<13;i++){i18n::language=i;for(int n:{-1,0,2,3}){auto s=securitynotice::result(n);if(s.empty()||s.find(L"{n}")!=std::wstring::npos)return 1;if(n>0&&s.find(std::to_wstring(n))==std::wstring::npos)return 2;}}std::cout<<"13 languages: fallback, zero and count OK\n";}
