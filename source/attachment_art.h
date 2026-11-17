#pragma once
// Original side-view line illustrations for the attachment selection UI.
namespace attachmentart {
inline bool draw(Gdiplus::Graphics& g,int kind,float x,float y,float scale,BYTE alpha,bool selected){
 if(kind<0||kind>5)return false;
 using namespace Gdiplus;auto at=[&](float a,float b){return PointF(x+a*scale,y+b*scale);};
 auto opacity=[&](float a){return (BYTE)std::clamp((int)(alpha*a),0,255);};
 Color primary(opacity(.98f),selected?204:196,selected?224:207,selected?255:222),detail(opacity(.75f),selected?151:139,selected?185:155,selected?237:178);
 Pen edge(primary,std::max(.85f,1.05f*scale));Pen fine(detail,std::max(.65f,.60f*scale));for(Pen* p:{&edge,&fine}){p->SetLineJoin(LineJoinRound);painting::roundCaps(*p);}
 auto line=[&](Pen& p,float x1,float y1,float x2,float y2){g.DrawLine(&p,at(x1,y1),at(x2,y2));};
 auto box=[&](Pen& p,float a,float b,float w,float h){g.DrawRectangle(&p,RectF(x+a*scale,y+b*scale,w*scale,h*scale));};
 auto circle=[&](Pen& p,float a,float b,float d){g.DrawEllipse(&p,RectF(x+a*scale,y+b*scale,d*scale,d*scale));};
 auto path=[&](GraphicsPath& p){g.DrawPath(&edge,&p);};
 auto mount=[&](){GraphicsPath p;p.AddLine(at(28,4),at(52,4));p.AddLine(at(52,4),at(54,6));p.AddLine(at(54,6),at(53,9));p.AddLine(at(53,9),at(27,9));p.AddLine(at(27,9),at(26,6));p.CloseFigure();path(p);for(int i=0;i<4;i++)line(fine,30+i*5,4,30+i*5,6);circle(fine,29,6,1.4f);circle(fine,49,6,1.4f);};
 if(kind==0){ // Long, near-vertical handle with a slightly flared base and molded ribs.
  mount();GraphicsPath p;p.AddLine(at(33,9),at(34,12));p.AddBezier(at(34,12),at(34,14),at(33,15),at(33,17));p.AddLine(at(33,17),at(32,33));p.AddBezier(at(32,33),at(32,36),at(33,37),at(35,38));p.AddLine(at(35,38),at(45,38));p.AddBezier(at(45,38),at(47,37),at(48,36),at(48,33));p.AddLine(at(48,33),at(47,17));p.AddBezier(at(47,17),at(47,15),at(46,14),at(46,12));p.AddLine(at(46,12),at(47,9));p.CloseFigure();path(p);line(fine,34,14,46,14);line(fine,33.5f,34.5f,46.5f,34.5f);for(int i=0;i<6;i++){float v=18+i*2.5f;line(fine,35,v,45,v);}}
 else if(kind==1){ // Half grip: low, curved hand stop with open webbing.
  mount();GraphicsPath p;p.AddLine(at(28,9),at(30,12));p.AddBezier(at(30,12),at(31,14),at(32,15),at(34,16));p.AddLine(at(34,16),at(35,22));p.AddBezier(at(35,22),at(35,24),at(37,25),at(39,25));p.AddLine(at(39,25),at(46,25));p.AddBezier(at(46,25),at(48,25),at(49,23),at(49,21));p.AddLine(at(49,21),at(50,17));p.AddBezier(at(50,17),at(52,15),at(52,13),at(52,9));p.CloseFigure();path(p);line(fine,31,12,50,12);for(int i=0;i<3;i++)line(fine,37,17+i*2,47,17+i*2);line(fine,36,23,48,23);}
 else if(kind==2){ // Tilted grip: swept-back handgrip, clear angled silhouette.
  mount();GraphicsPath p;p.AddLine(at(30,9),at(37,29));p.AddBezier(at(37,29),at(38,32),at(40,34),at(43,34));p.AddLine(at(43,34),at(48,33));p.AddBezier(at(48,33),at(51,32),at(52,30),at(51,27));p.AddLine(at(51,27),at(45,9));p.CloseFigure();path(p);line(fine,32,12,46,12);for(int i=0;i<5;i++){float j=(float)i;line(fine,34+j*1.4f,16+j*2.8f,46+j*1.1f,14+j*2.8f);}line(fine,39,30,50,28.5f);}
 else if(kind==3){ // Thumb grip: compact body and distinctive rearward shelf.
  mount();GraphicsPath p;p.AddLine(at(30,9),at(30,13));p.AddBezier(at(30,13),at(30,15),at(32,16),at(33,17));p.AddLine(at(33,17),at(33,25));p.AddBezier(at(33,25),at(33,27),at(35,28),at(37,28));p.AddLine(at(37,28),at(43,28));p.AddBezier(at(43,28),at(45,28),at(46,27),at(46,25));p.AddLine(at(46,25),at(46,17));p.AddBezier(at(46,17),at(50,16),at(54,18),at(57,20));p.AddBezier(at(57,20),at(59,21),at(61,20),at(60,18));p.AddBezier(at(60,18),at(57,14),at(53,12),at(48,12));p.AddLine(at(48,12),at(48,9));p.CloseFigure();path(p);for(int i=0;i<3;i++)line(fine,35,19+i*2.4f,44,19+i*2.4f);line(fine,34,25.5f,45,25.5f);}
 else if(kind==4){ // Compensator: slim cylindrical body, transverse port slots and collar.
  GraphicsPath p;p.AddLine(at(14,16),at(20,16));p.AddLine(at(20,16),at(22,13));p.AddLine(at(22,13),at(62,13));p.AddBezier(at(62,13),at(66,13),at(68,15),at(68,20));p.AddBezier(at(68,20),at(68,25),at(66,27),at(62,27));p.AddLine(at(62,27),at(22,27));p.AddLine(at(22,27),at(20,24));p.AddLine(at(20,24),at(14,24));p.CloseFigure();path(p);line(fine,18,16,18,24);line(fine,22,13,22,27);line(fine,64,14,64,26);for(int i=0;i<4;i++){float u=27+i*8;box(fine,u,16,4.5f,2.5f);box(fine,u,21.5f,4.5f,2.5f);}line(fine,25,19.8f,61,19.8f);}
 else{ // Muzzle brake: angular body with paired side chambers.
  GraphicsPath p;p.AddLine(at(13,16),at(20,16));p.AddLine(at(20,16),at(23,12));p.AddLine(at(23,12),at(64,12));p.AddLine(at(64,12),at(68,15));p.AddLine(at(68,15),at(68,25));p.AddLine(at(68,25),at(64,28));p.AddLine(at(64,28),at(23,28));p.AddLine(at(23,28),at(20,24));p.AddLine(at(20,24),at(13,24));p.CloseFigure();path(p);line(fine,17,16,17,24);line(fine,22,13,22,27);line(fine,64,13,64,27);for(int i=0;i<3;i++){float u=27+i*11;box(fine,u,15,7,4);box(fine,u,21,7,4);}line(fine,26,20,61,20);}
 return true;
}
}
