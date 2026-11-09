#include <cassert>
#include <cstdio>
#include "dlss_nr.h"
using namespace dlssnr;
int main(){
    // Round trip, and the ranges hold.
    Settings s=builtin(1);std::string t=serialize(s);Settings back;assert(parse(t,back));assert(back.preset==2&&std::fabs(back.v[0]-0.6)<1e-9&&std::fabs(back.v[1]-0.5)<1e-9&&back.autoMask);
    assert(builtin(3).preset==1&&builtin(2).preset==2&&builtin(3).v[0]>1.0);
    std::string e;
    // Imports carry only known numeric keys in range: no paths, commands, supersampling or other sections.
    assert(!parse("BlueAshDlssNr=1\nWorkingScale=2.0\n",back,&e));
    assert(parse("BlueAshDlssNr=1\nTransferStrength=2.5\nLocalTone=2\n",back,&e)&&std::fabs(back.v[0]-2.5)<1e-9&&!parse("BlueAshDlssNr=1\nMaxRatio=9\n",back,&e)); // strengths go past 1
    assert(!parse("BlueAshDlssNr=1\nTargetProcessName=C:\\\\evil.exe\n",back,&e));
    assert(!parse("BlueAshDlssNr=1\nTransferStrength=1;calc.exe\n",back,&e));
    assert(!parse("BlueAshDlssNr=1\nPreset=7\n",back,&e));
    assert(!parse("Preset=1\n",back,&e));               // not ours
    assert(!parse(std::string(5000,'x'),back,&e));
    assert(parse("BlueAshDlssNr=1\r\nPreset=1\r\n; note\r\nWorkingScale=0.75\r\n",back,&e)&&back.preset==1&&std::fabs(back.v[6]-0.75)<1e-9);
    // INI writing keeps the rest of the file, replaces in the right section only, adds what is missing.
    std::string ini="[Upscalers]\r\nDx12Upscaler=auto\r\n[DlssNr]\r\n; comment\r\nEnabled=auto\r\nPreset=auto\r\n[Log]\r\nLogToFile=true\r\n";
    std::string out=configure(ini,builtin(3),"witcher3.exe");
    assert(getIni(out,"DlssNr","Enabled")=="true"&&getIni(out,"DlssNr","Preset")=="1"&&getIni(out,"Upscalers","Dx12Upscaler")=="dlss");
    assert(getIni(out,"FrameGen","Enabled")=="false"&&getIni(out,"FrameGen","FGOutput")=="nofg"&&getIni(out,"ProcessFilter","TargetProcessName")=="witcher3.exe");
    assert(getIni(out,"DlssNr","WorkingScale")=="1.00"&&getIni(out,"DlssNr","DebugView")=="0"&&out.find("; comment")!=std::string::npos&&out.find("\r\n")!=std::string::npos);
    assert(getIni(configure(ini,builtin(2),"a.exe",false),"DlssNr","Enabled")=="false");
    assert(setIni("[A]\nx=1\n[B]\nx=2\n","B","x","9")=="[A]\nx=1\n[B]\nx=9\n");
    assert(setIni("[A]\nx=1\n","A","y","2")=="[A]\nx=1\ny=2\n");
    // Frame limit: off = auto, values outside 30..500 fall back to off, nothing else in the file changes.
    assert(getIni(frameLimit(out,120),"Framerate","FramerateLimit")=="120.0"&&getIni(frameLimit(out,0),"Framerate","FramerateLimit")=="auto");
    assert(getIni(frameLimit(out,5),"Framerate","FramerateLimit")=="auto"&&getIni(frameLimit(out,9999),"Framerate","FramerateLimit")=="auto"&&getIni(frameLimit(out,144),"DlssNr","Preset")=="1");
    // Runtime versions: 310.9 and newer carry models A and B.
    assert(modelsAB("310.9.16")&&modelsAB("311.0.1")&&!modelsAB("310.8.2"));
    // The build's capture manifest and pixel formats.
    Frame b,a;assert(parseManifest("frames 8\nbefore width 2560 height 1440 format 10 rowPitch 20480\nafter width 2560 height 1440 format 10 rowPitch 20480\n",b,a));
    assert(b.w==2560&&b.h==1440&&b.format==10&&b.pitch==20480&&a.format==10);assert(!parseManifest("frames 0\n",b,a));
    assert(std::fabs(half(0x3C00)-1.0f)<1e-6&&std::fabs(half(0x3800)-0.5f)<1e-6&&half(0)==0);
    // 2x1 RGBA16F frame: a mid-grey and a bright pixel; the same exposure for both frames keeps their order.
    Frame f;f.w=2;f.h=1;f.format=10;f.pitch=16;std::vector<unsigned char> px(16,0);unsigned short g[8]={0x3000,0x3000,0x3000,0x3C00,0x4400,0x4400,0x4400,0x3C00};std::memcpy(px.data(),g,16);
    double m=meanLuma(f,px);assert(m>0);std::vector<unsigned char> pic;assert(toDisplay(f,px,0.18/m,pic)&&pic.size()==8&&pic[0]<pic[4]&&pic[3]==255);
    Frame u;u.w=1;u.h=1;u.format=28;u.pitch=4;std::vector<unsigned char> rgba={10,20,30,255};assert(toDisplay(u,rgba,1,pic)&&pic[0]==30&&pic[2]==10); // RGBA -> BGRA
    Frame bad;bad.w=4;bad.h=4;bad.format=10;bad.pitch=32;assert(!toDisplay(bad,px,1,pic));   // truncated file refused
    puts("DLSS NR settings tests passed");
}
