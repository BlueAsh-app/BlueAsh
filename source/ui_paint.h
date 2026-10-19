#pragma once
// Opaque parent surfaces plus antialiasing keep rounded controls free of black rectangular corners.
namespace painting {
inline ULONG_PTR token=0;inline Gdiplus::Bitmap* logo=nullptr;inline IStream* logoStream=nullptr;
inline Gdiplus::Color color(COLORREF c){return Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c));}
inline void initialize(){
 if(token){return;}Gdiplus::GdiplusStartupInput input;if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok){token=0;return;}
 // Decode the approved 128px PNG already embedded in the desktop icon; both views share the same source.
 HMODULE module=GetModuleHandleW(nullptr);HRSRC group=FindResourceW(module,MAKEINTRESOURCEW(101),RT_GROUP_ICON);if(!group){return;}
 const BYTE* entries=(const BYTE*)LockResource(LoadResource(module,group));DWORD groupSize=SizeofResource(module,group);if(!entries||groupSize<6){return;}
 WORD count=0,imageId=0;memcpy(&count,entries+4,2);for(unsigned i=0;i<count&&6+(i+1)*14<=groupSize;i++){const BYTE* entry=entries+6+i*14;if(entry[0]==128&&entry[1]==128){memcpy(&imageId,entry+12,2);break;}}
 if(!imageId){return;}HRSRC res=FindResourceW(module,MAKEINTRESOURCEW(imageId),RT_ICON);if(!res){return;}
 DWORD size=SizeofResource(module,res);const void* bytes=LockResource(LoadResource(module,res));if(!bytes||!size){return;}
 HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory){return;}void* target=GlobalLock(memory);if(!target){GlobalFree(memory);return;}
 memcpy(target,bytes,size);GlobalUnlock(memory);
 if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&logoStream))){GlobalFree(memory);return;}
 logo=Gdiplus::Bitmap::FromStream(logoStream);if(!logo||logo->GetLastStatus()!=Gdiplus::Ok){delete logo;logo=nullptr;logoStream->Release();logoStream=nullptr;}
}
inline void shutdown(){delete logo;logo=nullptr;if(logoStream){logoStream->Release();logoStream=nullptr;}if(token){Gdiplus::GdiplusShutdown(token);token=0;}}
inline void path(Gdiplus::GraphicsPath& p,RECT r,int diameter){
 float x=(float)r.left+.5f,y=(float)r.top+.5f,w=(float)(r.right-r.left)-1,h=(float)(r.bottom-r.top)-1,d=(float)std::min(diameter,(int)std::min(w,h));
 if(d<=1){p.AddRectangle(Gdiplus::RectF(x,y,w,h));return;}p.AddArc(x,y,d,d,180,90);p.AddArc(x+w-d,y,d,d,270,90);p.AddArc(x+w-d,y+h-d,d,d,0,90);p.AddArc(x,y+h-d,d,d,90,90);p.CloseFigure();
}
inline bool box(HDC dc,RECT r,COLORREF fill,COLORREF edge,int diameter){
 if(!token){return false;}Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
 Gdiplus::GraphicsPath p;path(p,r,diameter);Gdiplus::SolidBrush b(color(fill));g.FillPath(&b,&p);if(edge!=fill){Gdiplus::Pen pen(color(edge),1.f);g.DrawPath(&pen,&p);}return true;
}
inline bool card(HDC dc,RECT r,COLORREF fill,COLORREF edge,bool selected){
 if(!token){return false;}Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
 Gdiplus::GraphicsPath p;path(p,r,24);auto top=Gdiplus::Color(255,(BYTE)std::min(255,(int)GetRValue(fill)+7),(BYTE)std::min(255,(int)GetGValue(fill)+7),(BYTE)std::min(255,(int)GetBValue(fill)+9));
 Gdiplus::LinearGradientBrush b(Gdiplus::Point(r.left,r.top),Gdiplus::Point(r.left,r.bottom),top,color(fill));g.FillPath(&b,&p);Gdiplus::Pen pen(color(edge),selected?1.8f:1.f);g.DrawPath(&pen,&p);return true;
}
inline void outline(HDC dc,RECT r,COLORREF edge,int diameter){
 if(!token){return;}Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::GraphicsPath p;path(p,r,diameter);Gdiplus::Pen pen(color(edge),1.5f);g.DrawPath(&pen,&p);
}
inline bool circle(HDC dc,RECT r,COLORREF fill,COLORREF edge,float width=1.f){
 if(!token){return false;}Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::RectF area((float)r.left+.5f,(float)r.top+.5f,(float)(r.right-r.left)-1,(float)(r.bottom-r.top)-1);
 if(fill!=CLR_NONE){Gdiplus::SolidBrush b(color(fill));g.FillEllipse(&b,area);}if(edge!=CLR_NONE){Gdiplus::Pen p(color(edge),width);g.DrawEllipse(&p,area);}return true;
}
inline void bug(HDC dc,RECT r,COLORREF ink){
 if(token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.TranslateTransform((float)r.left,(float)r.top);g.ScaleTransform((float)(r.right-r.left)/24,(float)(r.bottom-r.top)/24);Gdiplus::Pen p(color(ink),1.7f);p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);
  g.DrawEllipse(&p,7.f,8.f,10.f,13.f);g.DrawArc(&p,8.5f,4.f,7.f,7.f,180.f,180.f);g.DrawLine(&p,10.f,4.5f,8.f,2.f);g.DrawLine(&p,14.f,4.5f,16.f,2.f);g.DrawLine(&p,12.f,12.f,12.f,21.f);
  for(int i=0;i<3;i++){float y=10.f+4.f*i,end=8.f+6.f*i;g.DrawLine(&p,7.f,y,3.f,end);g.DrawLine(&p,17.f,y,21.f,end);}return;
 }
 int saved=SaveDC(dc);HPEN p=CreatePen(PS_SOLID,2,ink);SelectObject(dc,p);SelectObject(dc,GetStockObject(NULL_BRUSH));int x=(r.left+r.right)/2,y=(r.top+r.bottom)/2;Ellipse(dc,x-5,y-4,x+6,y+9);Arc(dc,x-4,y-8,x+5,y-1,x-4,y-4,x+4,y-4);
 for(int i=0;i<3;i++){MoveToEx(dc,x-5,y-2+i*4,nullptr);LineTo(dc,x-9,y-4+i*6);MoveToEx(dc,x+5,y-2+i*4,nullptr);LineTo(dc,x+9,y-4+i*6);}MoveToEx(dc,x,y,nullptr);LineTo(dc,x,y+8);RestoreDC(dc,saved);DeleteObject(p);
}
// Take-away coffee cup (sidebar 커피 한 잔 후원), the bug icon's pen on the same 24 x 24 grid: a domed lid on a wide rim, a cup
// that narrows to the bottom and a solid sleeve band (lines alone read as a bin), with two wisps of steam above.
inline void coffee(HDC dc,RECT r,COLORREF ink){
 if(!token){return;}Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.TranslateTransform((float)r.left,(float)r.top);g.ScaleTransform((float)(r.right-r.left)/24,(float)(r.bottom-r.top)/24);
 Gdiplus::Pen p(color(ink),1.6f);p.SetStartCap(Gdiplus::LineCapRound);p.SetEndCap(Gdiplus::LineCapRound);p.SetLineJoin(Gdiplus::LineJoinRound);Gdiplus::SolidBrush fill(color(ink));
 auto side=[](float y){return 6.6f+(y-9.f)*2.0f/12.f;}; // the cup's left edge at height y (9 = under the rim, 21 = bottom)
 Gdiplus::GraphicsPath lid;lid.AddLine(6.5f,8.f,7.6f,5.6f);lid.AddBezier(7.6f,5.6f,9.f,4.6f,15.f,4.6f,16.4f,5.6f);lid.AddLine(16.4f,5.6f,17.5f,8.f);g.DrawPath(&p,&lid);
 g.DrawLine(&p,5.4f,8.6f,18.6f,8.6f);
 Gdiplus::PointF cup[]={{side(9.f),9.f},{side(21.f),21.f},{24.f-side(21.f),21.f},{24.f-side(9.f),9.f}};g.DrawLines(&p,cup,4);
 Gdiplus::PointF band[]={{side(12.5f),12.5f},{side(16.5f),16.5f},{24.f-side(16.5f),16.5f},{24.f-side(12.5f),12.5f}};g.FillPolygon(&fill,band,4);
 Gdiplus::Pen thin(color(ink),1.2f);thin.SetStartCap(Gdiplus::LineCapRound);thin.SetEndCap(Gdiplus::LineCapRound);
 for(float x:{10.f,14.f})g.DrawBezier(&thin,x,3.4f,x-1.f,2.4f,x+1.f,1.6f,x,0.6f);
}
// A soft light behind an icon: the colour fades from the middle to nothing at the edge of r.
inline void glow(HDC dc,RECT r,COLORREF c,BYTE strength){
 // Drawn in memory and copied (see art(): gradient brushes straight onto a captured window's DC are not safe).
 int w=r.right-r.left,h=r.bottom-r.top;if(!token||w<2||h<2){return;}Gdiplus::Bitmap light(w,h,PixelFormat32bppPARGB);
 {Gdiplus::Graphics o(&light);o.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);o.Clear(Gdiplus::Color(0,0,0,0));Gdiplus::GraphicsPath p;p.AddEllipse(0.f,0.f,(float)w,(float)h);
  Gdiplus::PathGradientBrush b(&p);b.SetCenterColor(Gdiplus::Color(strength,GetRValue(c),GetGValue(c),GetBValue(c)));Gdiplus::Color edge(0,GetRValue(c),GetGValue(c),GetBValue(c));int n=1;b.SetSurroundColors(&edge,&n);
  Gdiplus::REAL pos[]={0.f,.55f,1.f},fall[]={0.f,.7f,1.f};b.SetBlend(fall,pos,3);o.FillPath(&b,&p);}
 Gdiplus::Graphics g(dc);g.DrawImage(&light,r.left,r.top,w,h);
}
// Light that comes off the icon itself: draw() paints the icon in white on black into a small bitmap; that shape,
// blurred, becomes a halo in colour c laid under the icon (the crisp icon is then drawn on top by the caller).
inline void iconGlow(HDC dc,RECT r,COLORREF c,int blur,double gain,const std::function<void(HDC,RECT)>& draw){
 int w=r.right-r.left,h=r.bottom-r.top;if(!token||w<4||h<4){return;}
 BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 void* bits=nullptr;HDC mem=CreateCompatibleDC(dc);HBITMAP dib=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);if(!dib||!bits){if(dib){DeleteObject(dib);}DeleteDC(mem);return;}
 auto old=SelectObject(mem,dib);RECT all={0,0,w,h};FillRect(mem,&all,(HBRUSH)GetStockObject(BLACK_BRUSH));draw(mem,all);GdiFlush();
 std::vector<float> a((size_t)w*h),t((size_t)w*h);const BYTE* px=(const BYTE*)bits;for(size_t i=0;i<a.size();i++)a[i]=std::max({px[i*4],px[i*4+1],px[i*4+2]})/255.f;
 SelectObject(mem,old);DeleteObject(dib);DeleteDC(mem);
 // Three box-blur passes each way ≈ a gaussian.
 for(int pass=0;pass<3;pass++){
  for(int y=0;y<h;y++){float s=0;int n=0;for(int x=-blur;x<w;x++){int in=x+blur,out=x-blur-1;if(in<w){s+=a[(size_t)y*w+in];n++;}if(out>=0){s-=a[(size_t)y*w+out];n--;}if(x>=0)t[(size_t)y*w+x]=n?s/n:0;}}
  for(int x=0;x<w;x++){float s=0;int n=0;for(int y=-blur;y<h;y++){int in=y+blur,out=y-blur-1;if(in<h){s+=t[(size_t)in*w+x];n++;}if(out>=0){s-=t[(size_t)out*w+x];n--;}if(y>=0)a[(size_t)y*w+x]=n?s/n:0;}}}
 Gdiplus::Bitmap halo(w,h,PixelFormat32bppARGB);Gdiplus::BitmapData d;Gdiplus::Rect lock(0,0,w,h);
 if(halo.LockBits(&lock,Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&d)!=Gdiplus::Ok){return;}
 for(int y=0;y<h;y++){BYTE* row=(BYTE*)d.Scan0+(size_t)y*d.Stride;for(int x=0;x<w;x++){int al=(int)std::min(255.0,a[(size_t)y*w+x]*255*gain);row[x*4]=GetBValue(c);row[x*4+1]=GetGValue(c);row[x*4+2]=GetRValue(c);row[x*4+3]=(BYTE)al;}}
 halo.UnlockBits(&d);Gdiplus::Graphics g(dc);g.DrawImage(&halo,r.left,r.top,w,h);
}
// The game-mode controller (as in the BlueAsh preview): rounded body with grips, a d-pad and four face buttons,
// drawn as an outline that fits r keeping a 4:3 shape.
// Mode-card icons, drawn with the same pen so the pair matches: a soft controller (curves only, like an iOS symbol)
// and headphones. Both fit r and use colour c and line width stroke.
inline void roundCaps(Gdiplus::Pen& pen){pen.SetLineJoin(Gdiplus::LineJoinRound);pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);}
inline bool gamepad(HDC dc,RECT r,COLORREF c,float stroke){
 if(!token){return false;}const float k=.76f;float W=(float)(r.right-r.left)-stroke,H=(float)(r.bottom-r.top)-stroke,w=std::min(W,H/k),h=w*k,x0=r.left+((float)(r.right-r.left)-w)/2,y0=r.top+((float)(r.bottom-r.top)-h)/2;
 auto P=[&](float x,float y){return Gdiplus::PointF(x0+x*w,y0+y*h);};Gdiplus::GraphicsPath b;
 // Right half from the top centre, then the mirror image back up the left: every joint is a smooth curve.
 b.AddBezier(P(.50f,.11f),P(.58f,.11f),P(.62f,.02f),P(.74f,.02f));b.AddBezier(P(.74f,.02f),P(.90f,.02f),P(.96f,.16f),P(.985f,.40f));
 b.AddBezier(P(.985f,.40f),P(1.01f,.66f),P(1.f,.93f),P(.85f,.97f));b.AddBezier(P(.85f,.97f),P(.76f,.99f),P(.72f,.86f),P(.66f,.75f));b.AddBezier(P(.66f,.75f),P(.62f,.67f),P(.56f,.65f),P(.50f,.65f));
 b.AddBezier(P(.50f,.65f),P(.44f,.65f),P(.38f,.67f),P(.34f,.75f));b.AddBezier(P(.34f,.75f),P(.28f,.86f),P(.24f,.99f),P(.15f,.97f));b.AddBezier(P(.15f,.97f),P(0.f,.93f),P(-.01f,.66f),P(.015f,.40f));
 b.AddBezier(P(.015f,.40f),P(.04f,.16f),P(.10f,.02f),P(.26f,.02f));b.AddBezier(P(.26f,.02f),P(.38f,.02f),P(.42f,.11f),P(.50f,.11f));b.CloseFigure();
 Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);Gdiplus::Pen pen(color(c),stroke);roundCaps(pen);g.DrawPath(&pen,&b);
 float arm=w*.08f;Gdiplus::PointF d=P(.29f,.38f);g.DrawLine(&pen,d.X-arm,d.Y,d.X+arm,d.Y);g.DrawLine(&pen,d.X,d.Y-arm,d.X,d.Y+arm);
 Gdiplus::PointF q=P(.71f,.38f);float o=w*.066f,dot=std::max(stroke*.85f,w*.034f);Gdiplus::SolidBrush fill(color(c));
 for(auto e:{Gdiplus::PointF(q.X+o,q.Y),Gdiplus::PointF(q.X-o,q.Y),Gdiplus::PointF(q.X,q.Y-o),Gdiplus::PointF(q.X,q.Y+o)})g.FillEllipse(&fill,e.X-dot,e.Y-dot,dot*2,dot*2);
 return true;
}
inline bool headphones(HDC dc,RECT r,COLORREF c,float stroke){
 // Mirror-exact in floating point: the headband is a half circle whose ends run straight into the top of two pill
 // shaped ear cups centred on those ends (no part of the band shows inside a cup).
 if(!token){return false;}float s=std::min((float)(r.right-r.left),(float)(r.bottom-r.top))-stroke,cx=(r.left+r.right)/2.f,cy=(r.top+r.bottom)/2.f;
 Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);Gdiplus::Pen pen(color(c),stroke);roundCaps(pen);
 float R=s*.40f,top=cy-s*.46f,bandY=top+R,cw=s*.22f,ch=s*.40f,cupTop=cy+s*.04f,rad=cw/2;
 Gdiplus::GraphicsPath band;band.AddArc(cx-R,top,R*2,R*2,180.f,180.f);g.DrawPath(&pen,&band);
 for(float side:{-1.f,1.f}){float x=cx+side*R;g.DrawLine(&pen,x,bandY,x,cupTop);
  Gdiplus::GraphicsPath cup;float l=x-cw/2,t=cupTop;cup.AddArc(l,t,cw,cw,180.f,180.f);cup.AddLine(l+cw,t+rad,l+cw,t+ch-rad);cup.AddArc(l,t+ch-cw,cw,cw,0.f,180.f);cup.CloseFigure();g.DrawPath(&pen,&cup);}
 return true;
}
// An exclamation mark drawn as shapes (a rounded bar and a dot) centred in r: the update badge's "!". Not text, because
// GDI+ cannot see the UI font (Pretendard is loaded privately for GDI only), so a text "!" came out empty.
inline bool exclaim(HDC dc,RECT r,COLORREF c){
 if(!token){return false;}float cx=(r.left+r.right)/2.f,cy=(r.top+r.bottom)/2.f,h=(float)(r.bottom-r.top),w=std::max(2.f,h*.13f);
 // Same pixel alignment as circle() (no pixel offset), so the mark sits in the exact middle of the disc drawn there.
 Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
 Gdiplus::Pen bar(color(c),w);bar.SetStartCap(Gdiplus::LineCapRound);bar.SetEndCap(Gdiplus::LineCapRound);g.DrawLine(&bar,cx,cy-h*.25f,cx,cy+h*.07f);
 Gdiplus::SolidBrush dot(color(c));float d=w*1.05f;g.FillEllipse(&dot,cx-d/2,cy+h*.24f-d/2,d,d);return true;
}
// Game artwork (game tiles on the dashboard): a picture filling a rounded square, cropped to its centre (or the
// given vertical anchor 0..1 for tall covers), antialiased edges through a texture brush.
inline bool art(HDC dc,RECT r,Gdiplus::Bitmap* image,int diameter,float anchorY=.5f){
 if(!token||!image){return false;}int w=r.right-r.left,h=r.bottom-r.top;double sw=image->GetWidth(),sh=image->GetHeight();if(sw<1||sh<1){return false;}
 double k=std::max(w/sw,h/sh);int dw=(int)(sw*k+.5),dh=(int)(sh*k+.5);Gdiplus::Bitmap tile(w,h,PixelFormat32bppPARGB);
 {Gdiplus::Graphics t(&tile);t.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);t.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);t.DrawImage(image,(w-dw)/2,(int)((h-dh)*anchorY),dw,dh);}
 // The rounded picture is finished in memory (texture fill, antialiased edge) and then copied with DrawImage: a texture
 // brush filling straight onto the window DC crashed inside GDI+ when the window was captured (PrintWindow).
 Gdiplus::Bitmap round(w,h,PixelFormat32bppPARGB);
 {Gdiplus::Graphics o(&round);o.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);o.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);o.Clear(Gdiplus::Color(0,0,0,0));
  Gdiplus::TextureBrush b(&tile);Gdiplus::GraphicsPath p;path(p,{0,0,w,h},diameter);o.FillPath(&b,&p);Gdiplus::Pen edge(Gdiplus::Color(40,255,255,255),1.f);o.DrawPath(&edge,&p);}
 Gdiplus::Graphics g(dc);return g.DrawImage(&round,r.left,r.top,w,h)==Gdiplus::Ok;
}
// A dashboard mode card (2.21): a rounded navy card inset 6 px in r (the margin holds the glow). The chosen card gets a
// cobalt wash from the left, a brighter edge and a soft outer glow; art (a game cover found on this PC) is blurred and
// faded in from the right. Composed in memory, then copied (see art()).
inline bool modeCard(HDC dc,RECT r,bool selected,bool hover,Gdiplus::Bitmap* image,float anchorY,COLORREF fill,COLORREF edge,COLORREF accent){
 int w=r.right-r.left,h=r.bottom-r.top;if(!token||w<40||h<40){return false;}Gdiplus::Bitmap out(w,h,PixelFormat32bppPARGB);
 {Gdiplus::Graphics g(&out);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);g.Clear(Gdiplus::Color(0,0,0,0));
  RECT c={6,6,w-6,h-6};Gdiplus::GraphicsPath p;path(p,c,22);
  if(selected)for(int i=6;i>=1;i--){RECT e=c;InflateRect(&e,i,i);Gdiplus::GraphicsPath halo;path(halo,e,22+2*i);Gdiplus::Pen pen(Gdiplus::Color((BYTE)(8+(6-i)*7),GetRValue(accent),GetGValue(accent),GetBValue(accent)),1.6f);g.DrawPath(&pen,&halo);}
  if(selected){Gdiplus::LinearGradientBrush b(Gdiplus::Point(c.left,c.top),Gdiplus::Point(c.right,c.bottom),Gdiplus::Color(255,23,52,124),Gdiplus::Color(255,11,26,66));g.FillPath(&b,&p);}
  else{Gdiplus::LinearGradientBrush b(Gdiplus::Point(c.left,c.top),Gdiplus::Point(c.left,c.bottom),Gdiplus::Color(255,(BYTE)std::min(255,GetRValue(fill)+5),(BYTE)std::min(255,GetGValue(fill)+7),(BYTE)std::min(255,GetBValue(fill)+12)),color(fill));g.FillPath(&b,&p);}
  if(image&&image->GetWidth()>15&&image->GetHeight()>15){
   // Blur by drawing small and scaling back up, then fade: nothing on the left, at most 45 % on the right.
   int aw=(int)((c.right-c.left)*.62f),ah=c.bottom-c.top,sw=std::max(8,aw/4),sh=std::max(8,ah/4);double iw=image->GetWidth(),ih=image->GetHeight(),k=std::max(sw/iw,sh/ih);
   Gdiplus::Bitmap tiny(sw,sh,PixelFormat32bppARGB);{Gdiplus::Graphics t(&tiny);t.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);int dw=(int)(iw*k+.5),dh=(int)(ih*k+.5);t.DrawImage(image,(sw-dw)/2,(int)((sh-dh)*anchorY),dw,dh);}
   Gdiplus::Bitmap soft(aw,ah,PixelFormat32bppARGB);{Gdiplus::Graphics t(&soft);t.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBilinear);t.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    Gdiplus::ImageAttributes clamp;clamp.SetWrapMode(Gdiplus::WrapModeTileFlipXY);t.DrawImage(&tiny,Gdiplus::Rect(0,0,aw,ah),0,0,sw,sh,Gdiplus::UnitPixel,&clamp);}
   Gdiplus::BitmapData d;Gdiplus::Rect all(0,0,aw,ah);
   if(soft.LockBits(&all,Gdiplus::ImageLockModeRead|Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&d)==Gdiplus::Ok){
    for(int y=0;y<ah;y++){BYTE* row=(BYTE*)d.Scan0+(size_t)y*d.Stride;for(int x=0;x<aw;x++){double t=std::clamp(x/(aw*.7),0.0,1.0);t=t*t*(3-2*t);row[x*4+3]=(BYTE)(row[x*4+3]*t*.45);}}soft.UnlockBits(&d);}
   g.SetClip(&p);g.DrawImage(&soft,c.right-aw,c.top,aw,ah);g.ResetClip();}
  Gdiplus::Pen pen(color(selected?accent:hover?RGB(64,104,196):edge),selected?1.7f:1.f);g.DrawPath(&pen,&p);}
 Gdiplus::Graphics g(dc);return g.DrawImage(&out,r.left,r.top,w,h)==Gdiplus::Ok;
}
// A heartbeat line (현재 상태 title), fitting r.
inline bool pulse(HDC dc,RECT r,COLORREF c,float stroke){
 if(!token){return false;}float x=(float)r.left,y=(r.top+r.bottom)/2.f,w=(float)(r.right-r.left),h=(float)(r.bottom-r.top)/2.f-stroke;
 Gdiplus::PointF pts[]={{x,y},{x+w*.28f,y},{x+w*.40f,y-h},{x+w*.56f,y+h},{x+w*.68f,y},{x+w,y}};
 Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);Gdiplus::Pen pen(color(c),stroke);roundCaps(pen);g.DrawLines(&pen,pts,6);return true;
}
// An icon (with its alpha) as a GDI+ bitmap, for art(). nullptr when the icon has no 32-bit colour plane.
inline Gdiplus::Bitmap* iconBitmap(HICON icon){
 ICONINFO ii={};if(!icon||!GetIconInfo(icon,&ii)){return nullptr;}BITMAP bm={};Gdiplus::Bitmap* out=nullptr;
 if(ii.hbmColor&&GetObjectW(ii.hbmColor,sizeof(bm),&bm)&&bm.bmBitsPixel==32){BITMAPINFO bi={};bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=bm.bmWidth;bi.bmiHeader.biHeight=-bm.bmHeight;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
  std::vector<BYTE> px((size_t)bm.bmWidth*bm.bmHeight*4);HDC dc=GetDC(nullptr);if(GetDIBits(dc,ii.hbmColor,0,bm.bmHeight,px.data(),&bi,DIB_RGB_COLORS)){// A real copy: Clone() of a bitmap made over px may keep pointing at px, which is gone when this returns.
   out=new Gdiplus::Bitmap(bm.bmWidth,bm.bmHeight,PixelFormat32bppARGB);Gdiplus::BitmapData d;Gdiplus::Rect all(0,0,bm.bmWidth,bm.bmHeight);
   if(out->LockBits(&all,Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&d)==Gdiplus::Ok){for(int y=0;y<bm.bmHeight;y++)memcpy((BYTE*)d.Scan0+(size_t)y*d.Stride,px.data()+(size_t)y*bm.bmWidth*4,(size_t)bm.bmWidth*4);out->UnlockBits(&d);}else{delete out;out=nullptr;}}ReleaseDC(nullptr,dc);}
 if(ii.hbmColor){DeleteObject(ii.hbmColor);}if(ii.hbmMask){DeleteObject(ii.hbmMask);}return out;
}
inline bool drawLogo(HDC dc,RECT r){
 if(!logo){return false;}Gdiplus::Graphics g(dc);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
 Gdiplus::ImageAttributes attributes;attributes.SetWrapMode(Gdiplus::WrapModeTileFlipXY);return g.DrawImage(logo,Gdiplus::Rect(r.left,r.top,r.right-r.left,r.bottom-r.top),0,0,logo->GetWidth(),logo->GetHeight(),Gdiplus::UnitPixel,&attributes)==Gdiplus::Ok;
}
}
