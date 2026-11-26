// g++ -std=c++17 -Wall -Wextra -Werror sound_preview_tests.cpp -o t.exe -lwinmm && ./t.exe
#include <windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "sound_dsp.h"
#include "sound_preview.h"
static int fails=0;
#define CHECK(c,msg) do{if(!(c)){printf("FAIL %s (line %d)\n",msg,__LINE__);fails++;}}while(0)
static double energy(const std::vector<float>& s,int ch){double a=0;for(size_t i=0;i<s.size()/2;i++)a+=s[i*2+ch]*s[i*2+ch];return a;}
// treble share: energy of the first difference (high frequencies) over all energy
static double bright(const std::vector<float>& s,int ch){double hi=0,all=0;for(size_t i=1;i<s.size()/2;i++){double d=s[i*2+ch]-s[(i-1)*2+ch];hi+=d*d;all+=s[i*2+ch]*s[i*2+ch];}return hi/std::max(all,1e-12);}
int main(){
    std::vector<float> click(4800);unsigned r=1;for(auto& v:click){r=r*1103515245+12345;v=((r>>16)%2000-1000)/4000.f;} // 0.1 s of noise
    auto right=sbprev::place(click,sbdsp::Pi/2,5,1),left=sbprev::place(click,-sbdsp::Pi/2,5,1);
    CHECK(energy(right,1)>energy(right,0)*2,"from the right: the right ear is louder");
    CHECK(energy(left,0)>energy(left,1)*2,"from the left: the left ear is louder");
    auto front=sbprev::place(click,0,5,1),back=sbprev::place(click,sbdsp::Pi,5,1);
    CHECK(std::fabs(energy(front,0)-energy(front,1))<energy(front,0)*.05,"ahead: both ears alike");
    CHECK(bright(back,0)<bright(front,0)*.8,"behind: duller than ahead");
    CHECK(sbprev::gainAt(1,150)<sbprev::gainAt(1,20),"a farther shot is quieter");CHECK(sbprev::gainAt(0,8)==1,"a footstep keeps the recording's level");
    // a 16-bit stereo 44.1 kHz WAV reads back as mono at 48 kHz
    wchar_t tmp[MAX_PATH];GetTempPathW(MAX_PATH,tmp);std::wstring path=std::wstring(tmp)+L"blueash-preview-test.wav";
    {FILE* f=_wfopen(path.c_str(),L"wb");uint32_t n=4410,data=n*4,riff=36+data,rate=44100,bps=rate*4,sz=16;uint16_t fmt=1,ch=2,align=4,bits=16;
     fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);fwrite(&sz,4,1,f);fwrite(&fmt,2,1,f);fwrite(&ch,2,1,f);fwrite(&rate,4,1,f);fwrite(&bps,4,1,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&data,4,1,f);
     for(uint32_t i=0;i<n;i++){int16_t a=16384,b=0;fwrite(&a,2,1,f);fwrite(&b,2,1,f);}fclose(f);}
    auto m=sbprev::readWav(path);DeleteFileW(path.c_str());
    CHECK(m.size()>=4790&&m.size()<=4810,"44.1 kHz -> 48 kHz length");CHECK(!m.empty()&&std::fabs(m[100]-.25f)<.01f,"channels averaged");
    auto w=sbprev::render(click,0,1.0,5,0,sbdsp::customPreset(sbdsp::Custom()));CHECK(w.size()>44&&!memcmp(w.data(),"RIFF",4),"render gives a WAV");
    if(!fails)printf("sound_preview ok\n");
    return fails?1:0;}
