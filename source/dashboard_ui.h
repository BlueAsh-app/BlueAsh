#pragma once
// Dashboard controls edit the same profiles as the profile page; a drag commits once on release.
namespace dashboard {
inline int profileIndex(){return active?appliedProfileIndex:0;}
inline int low(int id){return id==62?50:0;}inline int high(int id){return id==62?300:100;}
inline int value(int id){const auto& p=profiles[profileIndex()];return id==62?(int)p.gamma:(int)p.volume;}
// The track runs from x 40 (after the icon) to 64 px before the right edge, where the value is written.
constexpr int trackLeft=40,valueWidth=64;
inline int fromPoint(int id,int x,int width){int span=std::max(1,width-valueWidth-trackLeft);return std::clamp(low(id)+(x-trackLeft)*(high(id)-low(id))/span,low(id),high(id));}
inline bool commit(int id,int value,int index=-1){
 if(index<0){index=profileIndex();}bool apply=active&&appliedProfileIndex==index;auto before=profiles[index];if(id==62)settings::allGamma(profiles[index],(unsigned)value);else profiles[index].volume=(unsigned)value;
 if(apply&&!changeMode(index!=0,index)){profiles[index]=before;return false;}
 if(!saveString(configPath,L"ProfilesV2",settings::encodeV2(profiles,autoGame))){profiles[index]=before;if(apply&&!changeMode(index!=0,index)){problem(std::wstring(i18n::text(L"설정을 저장하지 못했습니다."))+i18n::text(L" 복원에도 실패했습니다. NVIDIA 앱에서 컬러를 확인하세요."));}else problem(i18n::text(L"설정을 저장하지 못했습니다."));return false;}
 for(int controlId:{1,2,61,62}){InvalidateRect(GetDlgItem(win,controlId),nullptr,FALSE);}refreshStatus();return true;
}
struct Slider {int id=0,pending=0,index=0;bool drag=false;ULONGLONG keyUntil=0;};
inline void paint(HWND h,HDC dc){
 auto s=(Slider*)GetWindowLongPtrW(h,GWLP_USERDATA);if(!s)return;RECT r;GetClientRect(h,&r);FillRect(dc,&r,panelBrush);
 // An icon (brightness for gamma, a speaker for volume), the track, then the value at the right end (2.21 preview).
 int v=s->drag?s->pending:value(s->id),cy=r.bottom/2,left=trackLeft,trackRight=(int)r.right-valueWidth,span=std::max(1,trackRight-left),x=left+(v-low(s->id))*span/(high(s->id)-low(s->id));
 wchar_t icon=s->id==62?0xE706:(v==0?0xE74F:v<34?0xE993:v<67?0xE994:0xE995);glyph(dc,icon,{0,cy-14,30,cy+14},iconFont,s->drag||GetFocus()==h?ink:muted);
 roundBox(dc,{left,cy-2,trackRight,cy+3},well,well,5);if(x>left)roundBox(dc,{left,cy-2,x,cy+3},blue,blue,5);
 if(!painting::circle(dc,{x-7,cy-7,x+7,cy+7},ink,CLR_NONE)){HBRUSH b=CreateSolidBrush(ink);HPEN p=CreatePen(PS_SOLID,1,ink);auto ob=SelectObject(dc,b),op=SelectObject(dc,p);Ellipse(dc,x-7,cy-7,x+7,cy+7);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);}
 textAt(dc,s->id==62?settings::gammaText(v):std::to_wstring(v)+L"%",{trackRight+8,cy-12,(int)r.right-4,cy+12},s->drag||GetTickCount64()<s->keyUntil?smallBold:smallFont,s->drag||GetTickCount64()<s->keyUntil?ink:muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
 // Focus shows as the brighter icon (no box around the slider).
}
inline LRESULT CALLBACK sliderProc(HWND h,UINT m,WPARAM w,LPARAM l){
 auto s=(Slider*)GetWindowLongPtrW(h,GWLP_USERDATA);
 if(m==WM_NCCREATE){auto cs=(CREATESTRUCTW*)l;s=new Slider;s->id=(int)(INT_PTR)cs->hMenu;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);return TRUE;}
 if(!s)return DefWindowProcW(h,m,w,l);
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);paint(h,dc);EndPaint(h,&ps);return 0;}
 if(m==WM_PRINT||m==WM_PRINTCLIENT){paint(h,(HDC)w);return 0;}
 if(m==WM_ERASEBKGND)return 1;
 if(m==WM_GETDLGCODE)return DLGC_WANTARROWS;
 if(m==WM_LBUTTONDOWN){SetFocus(h);SetCapture(h);s->index=profileIndex();s->drag=true;}
 if(m==WM_LBUTTONDOWN||(m==WM_MOUSEMOVE&&s->drag)){RECT r;GetClientRect(h,&r);s->pending=fromPoint(s->id,(int)(short)LOWORD(l),r.right);InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_LBUTTONUP&&s->drag){int v=s->pending;s->drag=false;ReleaseCapture();commit(s->id,v,s->index);InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_CAPTURECHANGED){s->drag=false;InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_KEYDOWN&&(w==VK_LEFT||w==VK_RIGHT||w==VK_HOME||w==VK_END)){int v=value(s->id);v=w==VK_HOME?low(s->id):w==VK_END?high(s->id):std::clamp(v+(w==VK_RIGHT?1:-1),low(s->id),high(s->id));commit(s->id,v);s->keyUntil=GetTickCount64()+1200;SetTimer(h,1,1300,nullptr);InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_TIMER){KillTimer(h,1);InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_SETFOCUS||m==WM_KILLFOCUS){InvalidateRect(h,nullptr,FALSE);return 0;}
 if(m==WM_NCDESTROY){delete s;SetWindowLongPtrW(h,GWLP_USERDATA,0);}
 return DefWindowProcW(h,m,w,l);
}
inline void registerControls(){WNDCLASSW wc={};wc.lpfnWndProc=sliderProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BlueAshSlider";wc.hCursor=LoadCursorW(nullptr,IDC_HAND);RegisterClassW(&wc);}
}
