// Runs on this PC's real Windows APIs. Changes nothing on the system: the power plan tests only use the plan that is
// already active (applying it is a no-op) and a journal in the temp folder.
#include <cassert>
#include <cstdio>
#include "perf_hw.h"
#include "perf_boost.h"
int main(){
    // Hardware: whatever this PC has, the call returns and the basics are filled.
    auto i=perfhw::collect(L"");assert(!i.cpu.empty()&&i.threads>0&&i.ramGB>0&&!i.os.empty());
    wprintf(L"CPU %ls (%d/%d, %d MHz) | GPU %ls [%ls] %.0f GB drv %ls | RAM %.1f GB, %d modules, rated %d / set %d MT/s | board %ls | %ls | %dx%d@%d | plan %ls | game mode %d | drive %ls | reboot %d\n",
        i.cpu.c_str(),i.cores,i.threads,i.baseMhz,i.gpu.c_str(),perfhw::vendorName(i.vendor),i.vramGB,i.driver.c_str(),i.ramGB,(int)i.modules.size(),perfhw::ratedMts(i),perfhw::configuredMts(i),i.board.c_str(),i.os.c_str(),i.width,i.height,i.refresh,i.powerPlan.c_str(),i.gameMode,i.storage.c_str(),(int)i.reboot);
    // Counters: one sample, no crash; values are either measured or "not measured" (-1).
    {perftrace::Counters c;if(c.open()){Sleep(300);perftrace::Averages a;perftrace::sample(c,nullptr,a);printf("counters: gpu samples %d, core samples %d\n",a.gpuN,a.coreN);}}
    // ETW without the rights: a clear "needs administrator" result, not a crash; with them: no frames from this process.
    {std::atomic<bool> stop{false};auto r=perftrace::measure(GetCurrentProcessId(),1,nullptr,[]{return true;},nullptr,stop);
     assert(r.needAdmin?!r.error.empty():r.ms.empty());printf("trace: %s\n",r.needAdmin?"needs administrator (expected without elevation)":"ran elevated, no presents from the test process");}
    // A process that exits during the measurement ends it early.
    {std::atomic<bool> stop{false};int calls=0;auto r=perftrace::measure(GetCurrentProcessId(),3,nullptr,[&]{return ++calls<2;},nullptr,stop);assert(r.needAdmin||r.gameExited);}
    perftrace::stopLeftover(); // safe when nothing runs
    // Background list: this test process and system programs never show as closable.
    {auto a=perftrace::procTimes();Sleep(200);auto b=perftrace::procTimes();auto v=perfboost::closable(perftrace::busiest(a,b,0,0),0,10);for(auto& x:v){assert(x.pid!=GetCurrentProcessId()&&!perfboost::systemName(x.name));}}
    // Power plan journal: round trip, a second apply keeps the first recorded plan, restore removes the journal.
    {GUID now={};assert(perfboost::active(now));wchar_t tmp[MAX_PATH];GetTempPathW(MAX_PATH,tmp);perfboost::Journal j;j.path=std::wstring(tmp)+L"blueash-boost-test.txt";DeleteFileW(j.path.c_str());
     assert(perfboost::restore(j)); // nothing recorded: nothing to do
     GUID want={};bool plan=perfboost::gamePlan(want);
     if(plan&&perfboost::isGamePlan(now)){assert(perfboost::applyPower(j,GetCurrentProcessId(),L"t")==perfboost::Apply::Already&&!j.exists());}
     // Pretend an earlier apply recorded the current plan; applying again must not overwrite it.
     j.power=now;j.hasPower=true;j.pid=1;j.game=L"g";assert(j.save());perfboost::Journal k;k.path=j.path;assert(k.load()&&k.hasPower&&IsEqualGUID(k.power,now)&&k.pid==1&&k.game==L"g");
     if(plan&&IsEqualGUID(now,want)){assert(perfboost::applyPower(k,2,L"g")==perfboost::Apply::Done);perfboost::Journal m;m.path=j.path;assert(m.load()&&IsEqualGUID(m.power,now)&&m.pid==2);}
     assert(perfboost::restore(k)&&!k.exists());GUID after={};assert(perfboost::active(after)&&IsEqualGUID(after,now));
     // A damaged journal: no plan to restore, the file is removed, nothing else happens.
     {FILE* f=_wfopen(j.path.c_str(),L"wb");fputs("power=garbage\npid=x\n",f);fclose(f);perfboost::Journal d;d.path=j.path;assert(d.load()&&!d.hasPower&&perfboost::restore(d)&&!d.exists());}}
    // 최적화 모드: other DirectX entries survive; apply sets the game-friendly values; a second apply keeps the first
    // originals; restore brings every value back exactly (absent ones removed again) and drops the journal.
    {long gm=optmode::readDword(optmode::gameBar,L"AutoGameModeEnabled"),dv=optmode::readDword(optmode::gameConfig,L"GameDVR_Enabled"),cp=optmode::readDword(optmode::gameDvr,L"AppCaptureEnabled");
     std::wstring dx;bool hadDx=optmode::readText(optmode::directX,L"DirectXUserGlobalSettings",dx);GUID plan={};assert(perfboost::active(plan));
     int anim=optmode::animNow(),m0[3]={};long tr=optmode::readDword(optmode::personalize,L"EnableTransparency");assert(optmode::mouseGet(m0));
     wchar_t tmp[MAX_PATH];GetTempPathW(MAX_PATH,tmp);optmode::Journal j;j.path=std::wstring(tmp)+L"blueash-optmode-test.txt";DeleteFileW(j.path.c_str());
     assert(optmode::restore(j)); // nothing recorded
     // An older version recorded the DirectX entry: apply puts it back and drops it from the journal.
     j.v["windowed"]=hadDx?L"="+dx:L"-";assert(j.save());
     assert(optmode::apply(j)==0&&j.exists()&&!j.v.count("windowed"));auto n=optmode::now();assert(n.gameMode&&n.dvr&&n.visual&&n.mouse);
     assert(optmode::apply(j)==0);optmode::Journal k;k.path=j.path;assert(k.load()&&k.v["gamemode"]==optmode::dwordText(gm)&&k.v["dvr"]==optmode::dwordText(dv));
     assert(optmode::restore(k)&&!k.exists());
     assert(optmode::readDword(optmode::gameBar,L"AutoGameModeEnabled")==gm&&optmode::readDword(optmode::gameConfig,L"GameDVR_Enabled")==dv&&optmode::readDword(optmode::gameDvr,L"AppCaptureEnabled")==cp);
     std::wstring dx2;assert(optmode::readText(optmode::directX,L"DirectXUserGlobalSettings",dx2)==hadDx&&(!hadDx||dx2==dx));GUID after={};assert(perfboost::active(after)&&IsEqualGUID(after,plan));
     int m1[3]={};assert(optmode::animNow()==anim&&optmode::readDword(optmode::personalize,L"EnableTransparency")==tr&&optmode::mouseGet(m1)&&m1[0]==m0[0]&&m1[1]==m0[1]&&m1[2]==m0[2]);}
    puts("perf system tests passed");
}
