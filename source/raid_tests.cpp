#include "raid_events.h"
#include "raid_files.h"
#include <fstream>
#include <iterator>
#include <cassert>
#include <iostream>
int main(int argc,char** argv){
 raid::Parser p;int countdown=0,started=0;
 auto emit=[&](raid::Event e){if(e==raid::Event::Countdown)countdown++;else started++;};
 auto feed=[&](const std::string&s){p.feed(s.data(),s.size(),emit);};
 feed("application|MatchingCompleted:40\n");assert(!countdown&&!started);
 feed("application|GameStar");assert(!countdown);
 feed("ting\r\napplication|GameStarting\napplication|GameStarted\n");assert(countdown==1&&started==0);
 feed("application|TRACE-NetworkGameCreate profileStatus\napplication|GameStarted\napplication|GameStarted\n");assert(started==1);
 feed("application|Network game matching cancelled\napplication|MatchingCompleted\n");assert(started==1);
 feed("application|GameStarting\n");assert(countdown==2);
 p.reset();feed("application|GameStarted\n");assert(started==2);
 p.reset();feed(std::string(70000,'x'));feed("application|GameStarting\n");assert(countdown==3);
 assert(raid::logName(L"2026.10.01_application.log"));
 assert(raid::logName(L"2026.10.01_1.1.5_output.log"));
 assert(raid::logName(L"OUTPUT.LOG"));
 assert(raid::logName(L"notifications_000.log"));
 assert(!raid::logName(L"output.log.bak")&&!raid::logName(L"errors.log"));
 assert(raid::initialOffset(true,200,100,500)==500);
 assert(raid::initialOffset(false,200,100,500)==0);
 assert(raid::initialOffset(false,50,100,500)==500);
 p.reset();int events=0;
 std::string line="2026-10-01 12:41:45.793|1.1.5.1.47510|Info|output|application|GameStarted:89.62(89.62) real:107.17(107.17) diff:17.54\n";
 line+="EFT.<GameStartingAsync>d__113:MoveNext()\nEFT.NetworkGame`1:GameStarted(Single, Int32)\n";
 for(char c:line)p.feed(&c,1,[&](raid::Event e){assert(e==raid::Event::Started);events++;});
 assert(events==1);
 // Real 1.2 raid order: the alert comes at GamePooled (deploy timer), not again at GameStarting/GameStarted.
 p.reset();countdown=started=0;
 feed("2026-10-08 23:00:54.077|1.2.0.0.47888|Info|application|scene preset path:maps/customs_preset.bundle rcid:bigmap.scenespreset.asset\n"
      "2026-10-08 23:02:09.792|1.2.0.0.47888|Info|application|GamePooled:65.45(30.23) real:78.38(36.8) diff:12.93\n"
      "2026-10-08 23:02:30.823|1.2.0.0.47888|Info|application|GameStarting:77.96(0) real:99.41(0) diff:21.45\n"
      "2026-10-08 23:02:30.823|1.2.0.0.47888|Info|application|GameStarted:77.96(0) real:99.41(0) diff:21.45\n");
 assert(countdown==1&&started==0);
 // 1.2.0.0.47965 online raid (NetworkGameCreate): spawned, then waiting for the other players; the countdown is GameStarting.
 p.reset();countdown=started=0;
 feed("2026-10-12 02:16:30.858|1.2.0.0.47965|Info|application|scene preset path:maps/city_preset.bundle rcid:city.scenespreset.asset\n"
      "2026-10-12 02:17:49.489|1.2.0.0.47965|Debug|application|TRACE-NetworkGameCreate profileStatus: 'Profileid: 69192727a4d169575c01eccf, Status: Busy, RaidMode: Online'\n"
      "2026-10-12 02:18:42.098|1.2.0.0.47965|Info|application|GamePooled:126.65(52.12) real:134.31(52.6) diff:7.65\n"
      "2026-10-12 02:18:52.125|1.2.0.0.47965|Info|application|GameSpawn:129.13(0.27) real:144.34(0.26) diff:15.2\n"
      "2026-10-12 02:19:04.082|1.2.0.0.47965|Info|application|GameSpawned:140.17(10.09) real:156.3(11.01) diff:16.13\n");
 assert(countdown==0&&started==0);
 feed("2026-10-12 02:19:06.948|1.2.0.0.47965|Info|application|GameStarting:143.03(2.86) real:159.17(2.86) diff:16.14\n"
      "2026-10-12 02:19:18.996|1.2.0.0.47965|Info|application|GameStarted:154.27(11.24) real:171.21(12.04) diff:16.94\n");
 assert(countdown==1&&started==0);
 // 1.2.0.0.47965 solo PvE (no NetworkGameCreate): the countdown runs from GamePooled; the rest comes at the start itself.
 p.reset();countdown=started=0;
 feed("2026-10-12 18:38:01.178|1.2.0.0.47965|Info|application|scene preset path:maps/sandbox_preset.bundle rcid:sandbox.scenespreset.asset\n"
      "2026-10-12 18:38:40.536|1.2.0.0.47965|Info|application|GameCreated:33.62(2.82) real:43.14(2.86) diff:9.52\n"
      "2026-10-12 18:39:05.715|1.2.0.0.47965|Info|application|GamePooled:44.75(11.13) real:68.32(25.18) diff:23.57\n");
 assert(countdown==1&&started==0);
 feed("2026-10-12 18:39:28.753|1.2.0.0.47965|Info|application|GameSpawned:57.1(21.03) real:91.36(32.49) diff:34.26\n"
      "2026-10-12 18:39:28.753|1.2.0.0.47965|Info|application|GameStarting:57.1(0) real:91.36(0) diff:34.26\n"
      "2026-10-12 18:39:28.753|1.2.0.0.47965|Info|application|GameStarted:57.1(0) real:91.36(0) diff:34.26\n");
 assert(countdown==1&&started==0);
 // An online raid joined late (no GameStarting line): the alert still comes, at GameStarted.
 p.reset();countdown=started=0;
 feed("2026-10-12 15:10:46.547|1.2.0.0.47965|Info|application|scene preset path:maps/sandbox_high_preset.bundle rcid:sandbox_high.scenespreset.asset\n"
      "2026-10-12 15:12:12.163|1.2.0.0.47965|Debug|application|TRACE-NetworkGameCreate profileStatus: 'Profileid: x, RaidMode: Online'\n"
      "2026-10-12 15:13:09.044|1.2.0.0.47965|Info|application|GamePooled:136.84(55.75) real:145.78(56.87) diff:8.94\n"
      "2026-10-12 15:13:20.037|1.2.0.0.47965|Info|application|GameSpawned:140.32(0.38) real:156.77(0.87) diff:16.44\n"
      "2026-10-12 15:13:20.423|1.2.0.0.47965|Info|application|GameStarted:140.7(140.7) real:157.16(157.16) diff:16.46\n");
 assert(countdown==0&&started==1);
 if(argc==2){
  std::ifstream input(argv[1],std::ios::binary);assert(input.is_open());
  std::string sample((std::istreambuf_iterator<char>(input)),{});p.reset();events=0;
  for(size_t i=0;i<sample.size();i+=37)p.feed(sample.data()+i,std::min(size_t(37),sample.size()-i),[&](raid::Event e){assert(e==raid::Event::Started);events++;});
  assert(events==1);std::cout<<"User sample replay: exactly one Started event\n";
 }
 std::cout<<"Raid parser / filename / baseline tests passed\n";
}
