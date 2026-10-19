#include "i18n.h"
#include <iostream>
#include <stdexcept>
#include <thread>

void expect(bool ok,const char* note){if(!ok)throw std::runtime_error(note);}
bool hasKorean(const std::wstring& s){return std::any_of(s.begin(),s.end(),i18n::hangul);}
int main(){try{
 expect(i18n::languageCode()==L"ko","default language");
 expect(std::wstring(i18n::text(L"저장"))==L"저장","Korean source unchanged");
 const wchar_t* codes[]={L"ko",L"en",L"zh-CN",L"ja",L"ru",L"es",L"pt-BR",L"de",L"fr",L"it",L"pl",L"tr",L"zh-TW"};
 for(int l=0;l<i18n::languageCount;l++){
  i18n::setLanguage(codes[l]);expect(i18n::languageCode()==codes[l],"language selection");
  for(const auto& e:i18n::catalog){auto value=i18n::text(e.values[0]);expect(*value!=0,"empty catalog translation");expect(std::wstring(value)==e.values[l],"exact catalog lookup");if(l)expect(!hasKorean(value),"Korean fallback in translated catalog");}
  auto format=std::wstring(i18n::text(L"마지막 확인  %ls %d:%02d"));expect(format.find(L"%ls")!=std::wstring::npos&&format.find(L"%d:%02d")!=std::wstring::npos,"printf placeholders preserved");
  expect(i18n::text(std::wstring(L"C:\\사용자\\타르코프\\Logs"))==L"C:\\사용자\\타르코프\\Logs","user paths preserved");
  expect(i18n::text(std::wstring(L"사용자 타르코프 메모"))==L"사용자 타르코프 메모","unknown user data preserved");
  expect(std::wstring(i18n::text(nullptr)).empty(),"null input");
 }
 i18n::setLanguage(L"en");
 expect(i18n::text(std::wstring(L"감마 1.55 / 볼륨 35%"))==L"Gamma 1.55 / Volume 35%","composed numbers preserved");
 expect(i18n::text(std::wstring(L"새 버전 2.10.1이 나왔습니다"))==L"New version 2.10.1 is available","version and suffix preserved");
 expect(i18n::text(std::wstring(L"내장 파일을 풀지 못했습니다: C:\\사용자\\타르코프\\test.ps1")).find(L"C:\\사용자\\타르코프\\test.ps1")!=std::wstring::npos,"error path preserved");
 expect(i18n::text(std::wstring(L"BlueAsh · 에란겔 · L: 표시 끄기"))==L"BlueAsh · Erangel · L: hide markers","map title and key hint");
 const wchar_t* saved=i18n::text(L"저장");const wchar_t* dynamic=i18n::text(L"새 버전 2.10.1이 나왔습니다");
 for(int i=0;i<2000;i++)i18n::text((L"Unknown "+std::to_wstring(i)).c_str());
 i18n::setLanguage(L"ja");expect(std::wstring(saved)==L"Save"&&std::wstring(dynamic)==L"New version 2.10.1 is available","stable returned pointers");
 i18n::setLanguage(L"ZH-cn");expect(i18n::languageCode()==L"zh-CN","case normalization");i18n::setLanguage(L"zh-Hans");expect(i18n::languageCode()==L"zh-CN","simplified Chinese alias");
 i18n::setLanguage(L"PT-br");expect(i18n::languageCode()==L"pt-BR","Brazilian Portuguese");i18n::setLanguage(L"zh-Hant");expect(i18n::languageCode()==L"zh-TW","traditional Chinese alias");i18n::setLanguage(L"unsupported");expect(i18n::languageCode()==L"ko","unsupported language defaults to Korean");
 std::atomic<bool> valid{true};std::thread reader([&]{for(int i=0;i<1000;i++){auto t=std::wstring(i18n::text(L"저장"));if(t!=L"저장"&&t!=L"Save"&&t!=L"保存"&&t!=L"Сохранить")valid=false;}});
 for(int i=0;i<1000;i++){i18n::setLanguage(codes[i%5]);}reader.join();expect(valid,"concurrent language lookup");
 std::cout<<"PASS: native catalog ("<<(sizeof(i18n::catalog)/sizeof(i18n::catalog[0]))<<" entries), thirteen languages, placeholders, paths, stable pointers and concurrent lookup\n";
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
