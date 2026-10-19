#pragma once
// 배그 · 비행기 탑승 알림 settings (page 11, the gear beside the row on 게임 도우미 and on the dashboard): how loud the
// alert is. A change is saved right away (registry PubgAudioVolume, read by pubgJoin.load), so the page only has 닫기.
LRESULT CALLBACK pubgPageProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:
        pageLabel(h,i18n::text(L"알림 볼륨"),36,34,780,28,2);
        pageLabel(h,i18n::text(L"탑승 카운트다운이 끝나면 이 크기로 알려 줍니다. 배그 창을 보고 있을 때는 울리지 않습니다."),36,66,780,40,1);
        addSetting(h,L"BlueAshLevel",L"",WS_TABSTOP,36,110,560,46,73);levelSet(h,73,(int)pubgLoudness);
        pageButton(h,i18n::text(L"소리 테스트"),666,120,150,174);
        pageLabel(h,L"",36,176,780,30,1,176);
        pageButton(h,i18n::text(L"닫기"),696,556,120,180);
        return 0;
    case WM_COMMAND:{int id=LOWORD(wp);
        if(id==73&&HIWORD(wp)==level::LEVEL_CHANGED){pubgLoudness=(unsigned)levelValue(h,73);if(!saveString(configPath,L"PubgAudioVolume",std::to_wstring(pubgLoudness)))SetDlgItemTextW(h,176,i18n::text(L"설정을 저장하지 못했습니다."));}
        else if(id==174&&HIWORD(wp)==BN_CLICKED)SetDlgItemTextW(h,176,pubgSound()?i18n::text(L"알림음을 울렸습니다. 안 들리면 Windows 볼륨을 확인하세요."):i18n::text(L"알림음을 울리지 못했습니다."));
        else if(id==180&&HIWORD(wp)==BN_CLICKED)DestroyWindow(h);
        return 0;}
    case WM_ERASEBKGND:return eraseCarbon(h,(HDC)wp);
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_DRAWITEM:drawButton((DRAWITEMSTRUCT*)lp);return TRUE;
    case WM_CLOSE:DestroyWindow(h);return 0;
    }return DefWindowProcW(h,msg,wp,lp);
}
