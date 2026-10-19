#pragma once
#include "i18n.h"
// "관련 사이트": the Tarkov site links shown as three buttons in their own row on the game page (95~97).
void openSite(int i){
 const wchar_t* urls[]={L"https://escapefromtarkov.fandom.com/wiki/Escape_from_Tarkov_Wiki",L"https://eftboss.com/",L"https://tarkov-market.com/kr"};
 if(i<0||i>2){return;}if((INT_PTR)ShellExecuteW(win,L"open",urls[i],nullptr,nullptr,SW_SHOWNORMAL)<=32)problem(i18n::text(L"사이트를 열지 못했습니다. 기본 브라우저 설정을 확인하세요."));
}
