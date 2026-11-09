#pragma once
// 배그 · 지도 표시 settings (page 13, the gear beside the row on 게임 도우미): which labels go on the map (registry PubgMapCars /
// PubgMapBoats / PubgMapSecrets) and the key that shows / hides them while the map is open (PubgMapToggleKey, a virtual key,
// default L). A change is saved right away and read by pubgMap.fast, so the page only has 닫기.
std::vector<unsigned> pubgMapKeys;
LRESULT CALLBACK pubgMapPageProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{
        pageLabel(h,i18n::text(L"표시할 것"),36,34,780,28,2);
        pageSwitch(h,i18n::text(L"고정 차 (대회용 고정 스폰)"),36,72,780,32,70);checkBox(h,70,(pubgMapKinds&1)!=0);
        pageSwitch(h,i18n::text(L"고정 보트"),36,112,780,32,71);checkBox(h,71,(pubgMapKinds&2)!=0);
        pageSwitch(h,i18n::text(L"비밀의 방 (데스턴 보안실·카라킨 벙커 포함)"),36,152,780,32,72);checkBox(h,72,(pubgMapKinds&4)!=0);
        pageLabel(h,i18n::text(L"표시 켜기·끄기 키"),36,216,780,28,2);
        pageLabel(h,i18n::text(L"지도를 연 채로 이 키를 누르면 표시를 숨기거나 다시 보여 줍니다. 배그에서 쓰지 않는 키를 고르세요."),36,248,780,40,1);
        HWND box=addSetting(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,36,294,300,240,60);
        pubgMapKeys.clear();for(unsigned k='A';k<='Z';k++)pubgMapKeys.push_back(k);
        for(unsigned k=VK_F1;k<=VK_F11;k++)pubgMapKeys.push_back(k);
        for(unsigned k=VK_NUMPAD0;k<=VK_NUMPAD9;k++)pubgMapKeys.push_back(k);
        for(unsigned k:{VK_INSERT,VK_HOME,VK_END,VK_PRIOR,VK_NEXT,VK_PAUSE,VK_SCROLL})pubgMapKeys.push_back(k);
        if(std::find(pubgMapKeys.begin(),pubgMapKeys.end(),pubgMapKey)==pubgMapKeys.end())pubgMapKeys.push_back(pubgMapKey);
        for(size_t i=0;i<pubgMapKeys.size();i++){SendMessageW(box,CB_ADDSTRING,0,(LPARAM)keyName(pubgMapKeys[i]).c_str());if(pubgMapKeys[i]==pubgMapKey)SendMessageW(box,CB_SETCURSEL,i,0);}
        pageLabel(h,i18n::text(L"기본값: L"),352,300,300,24,1);
        pageLabel(h,L"",36,346,780,30,1,176);
        pageButton(h,i18n::text(L"닫기"),696,556,120,180);
        return 0;}
    case WM_COMMAND:{int id=LOWORD(wp);bool ok=true;
        if(id>=70&&id<=72&&HIWORD(wp)==BN_CLICKED){checkBox(h,id,!checked(h,id));const wchar_t* names[]={L"PubgMapCars",L"PubgMapBoats",L"PubgMapSecrets"};
            unsigned bit=1u<<(id-70);bool on=checked(h,id);pubgMapKinds=on?(pubgMapKinds|bit):(pubgMapKinds&~bit);ok=saveString(configPath,names[id-70],on?L"1":L"0");}
        else if(id==60&&HIWORD(wp)==CBN_SELCHANGE){int i=(int)SendDlgItemMessageW(h,60,CB_GETCURSEL,0,0);if(i>=0&&i<(int)pubgMapKeys.size()){pubgMapKey=pubgMapKeys[i];ok=saveString(configPath,L"PubgMapToggleKey",std::to_wstring(pubgMapKey));
            if(ok)SetDlgItemTextW(h,176,(i18n::text(L"저장했습니다: ")+keyName(pubgMapKey)).c_str());}}
        else if(id==180&&HIWORD(wp)==BN_CLICKED)DestroyWindow(h);
        if(!ok)SetDlgItemTextW(h,176,i18n::text(L"설정을 저장하지 못했습니다."));
        return 0;}
    case WM_ERASEBKGND:return eraseCarbon(h,(HDC)wp);
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_CTLCOLORLISTBOX:case WM_CTLCOLOREDIT:{HDC dc=(HDC)wp;SetTextColor(dc,ink);SetBkColor(dc,panel);return (LRESULT)panelBrush;}
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
