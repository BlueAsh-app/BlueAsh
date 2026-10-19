#pragma once
#include <algorithm>
#include <atomic>
#include <cwchar>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "i18n_native.h"
// Presentation only: do not apply these helpers to registry names, parser tokens or IPC identifiers.
namespace i18n {
inline std::atomic<int> language{0};
inline const wchar_t* const codes[]={L"ko",L"en",L"zh-CN",L"ja",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};
inline constexpr int languageCount=13;
inline void setLanguage(std::wstring code){
 for(auto& c:code)if(c>=L'A'&&c<=L'Z')c+=L'a'-L'A';
 int value=0;for(int i=0;i<languageCount;i++){std::wstring c=codes[i];for(auto& ch:c)if(ch>=L'A'&&ch<=L'Z')ch+=L'a'-L'A';if(code==c)value=i;}
 if(code==L"en-us")value=1;else if(code==L"zh"||code==L"zh-hans")value=2;else if(code==L"ja-jp")value=3;else if(code==L"ru-ru")value=4;else if(code==L"pt"||code==L"pt-pt")value=6;else if(code==L"zh-hant"||code==L"zh-hk")value=12;
 language.store(value);
}
inline std::wstring languageCode(){return codes[language.load()];}
// UI font per language. Pretendard (built into BlueAsh.exe, loaded at start: customFont) covers Korean, Latin with the Polish
// and Turkish letters, and Cyrillic; Japanese and Chinese keep their Windows fonts. Without it: Malgun Gothic / Segoe UI.
inline bool customFont=false;
inline const wchar_t* fontFamily(){auto c=languageCode();return c==L"ja"?L"Yu Gothic UI":c==L"zh-CN"?L"Microsoft YaHei UI":c==L"zh-TW"?L"Microsoft JhengHei UI":customFont?L"Pretendard":c==L"ko"?L"Malgun Gothic":L"Segoe UI";}
inline bool hangul(wchar_t c){return (c>=0xac00&&c<=0xd7a3)||(c>=0x1100&&c<=0x11ff)||(c>=0x3130&&c<=0x318f);}
inline const std::map<std::wstring,const Entry*>& index(){static const auto data=[](){std::map<std::wstring,const Entry*> r;for(const auto& e:catalog)r.emplace(e.values[0],&e);return r;}();return data;}
inline const std::vector<const Entry*>& fragments(){static const auto data=[](){std::vector<const Entry*> r;for(const auto& e:catalog)r.push_back(&e);std::stable_sort(r.begin(),r.end(),[](const Entry* a,const Entry* b){return std::wcslen(a->values[0])>std::wcslen(b->values[0]);});return r;}();return data;}
inline bool boundary(const std::wstring& s,size_t at){return !at||s[at-1]==L'\n'||s[at-1]==L'\r'||(at>=3&&(s.compare(at-3,3,L" · ")==0||s.compare(at-3,3,L" / ")==0||s.compare(at-3,3,L" | ")==0));}
inline std::wstring translate(const std::wstring& source,int lang){
 if(!lang||source.empty())return source;
 auto found=index().find(source);if(found!=index().end())return found->second->values[lang];
 if(std::none_of(source.begin(),source.end(),hangul))return source;
 // Greedy catalog fragments handle composed status lines while leaving numbers, paths and names intact.
 std::wstring out;size_t lastEnd=0;
 for(size_t at=0;at<source.size();){const Entry* match=nullptr;size_t length=0;
  for(auto e:fragments()){const wchar_t* key=e->values[0];size_t n=std::wcslen(key);if(n>source.size()-at||source.compare(at,n,key)!=0)continue;
   bool numericTail=at&&source[at-1]>=L'0'&&source[at-1]<=L'9'&&(!std::wcscmp(key,L"이 나왔습니다")||!std::wcscmp(key,L"로 업데이트했습니다")||!std::wcscmp(key,L"초 전)")||!std::wcscmp(key,L"개에 적용"));
   bool start=at==lastEnd||boundary(source,at)||!hangul(key[0])||numericTail;bool end=!hangul(key[n-1])||at+n==source.size()||(!hangul(source[at+n])&&source[at+n]!=L'\\'&&source[at+n]!=L'/');
   if(start&&end){match=e;length=n;break;}}
  if(match){out+=match->values[lang];at+=length;lastEnd=at;}else out+=source[at++];
 }return out;
}
inline std::wstring text(const std::wstring& source){return translate(source,language.load());}
inline const wchar_t* text(const wchar_t* source){
 if(!source){return L"";}int lang=language.load();auto found=index().find(source);if(found!=index().end())return found->second->values[lang];
 // Map nodes never move or get erased, so Win32 controls may retain the returned pointer safely.
 static std::mutex lock;static std::map<std::pair<int,std::wstring>,std::wstring> pool;
 std::lock_guard<std::mutex> guard(lock);auto key=std::make_pair(lang,std::wstring(source));auto foundPool=pool.find(key);if(foundPool!=pool.end())return foundPool->second.c_str();return pool.emplace(std::move(key),translate(source,lang)).first->second.c_str();
}
}
