#include "pubg_log.h"
#include <cassert>
#include <iostream>
#include <vector>
int main(){
 pubglog::Parser p;std::vector<std::string> modes;auto joined=[&](const std::string& m){modes.push_back(m);};
 std::string log="\xEF\xBB\xBF[2026.10.05-13.05.02:933][  0][0000]Log file open, 10/05/26 22:05:02\r\n"
  "E8A975F02263BF9A05CCE4ADFD80F890BDC360AF48A4CCB0388C722FD1AABA3C0F1FEA8AA73FE7808DB73D6C5F0180082DFCE4\r\n"
  "[2026.10.05-13.10.09:610][208][a244]UTslGameInstance::JoinToDedicatedServer() [GameModeAliase=BATTLEROYALE]\r\n"
  "[2026.10.05-13.10.15:890][240][a244]FAntiCheatPlayerData::Init() Finished.\r\n"
  "[2026.10.05-13.31.39:661][302][a244]Command not recognized: Stat DumpHitches -stop\r\n";
 for(size_t i=0;i<log.size();i+=7)p.feed(log.data()+i,std::min<size_t>(7,log.size()-i),joined);
 assert(modes.size()==1&&modes[0]=="BATTLEROYALE");
 auto feed=[&](const std::string& s){p.feed(s.data(),s.size(),joined);};
 feed("[x]UTslGameInstance::JoinToDedicatedServer()\n");assert(modes.size()==2&&modes[1]=="?");
 feed("[x]UTslGameInstance::SaveReplayPlayLog() match.bro\n");assert(modes.size()==2);
 // Join time for a restart mid-match (UTC seconds; 2026-10-05 13:10:09 = 1791205809).
 assert(pubglog::lineTimeUtc("[2026.10.05-13.10.09:610][208][a244]x")==1791205809);
 assert(pubglog::lastJoinUtc(log)==1791205809&&pubglog::lastJoinUtc("nothing")==-1&&pubglog::lineTimeUtc("E8A975F0")==-1);
 std::cout<<"PUBG log join tests passed\n";
}
