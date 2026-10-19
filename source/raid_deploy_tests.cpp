#include "raid_deploy.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace raiddeploy;
 assert(milliseconds(L"00:07.247")==7247);
 assert(milliseconds(L" 00 : 01.00 ")==1000);
 assert(milliseconds(L"00:00.912")==912);
 assert(milliseconds(L"00:00+912")==912);
 assert(milliseconds(L"00:61.000")==-1);
 assert(milliseconds(L"07.247")==-1);
 assert(milliseconds(L"12:45")==-1);
 assert(milliseconds(L"00:01.000 extra")==-1);
 Tracker t;
 assert(!t.feed(10,7247,true)); // A log or a single picture never sounds the alarm.
 assert(!t.feed(10.2,7047,true));
 assert(!t.feed(16.1,1147,true)); // Long gaps cannot carry old confirmation forward.
 assert(t.feed(16.3,947,true));
 assert(!t.feed(16.5,747,true)); // Exactly once per armed raid.
 t={};assert(!t.feed(1,900,true));assert(!t.feed(1.2,900,true)); // Frozen timer.
 t={};assert(!t.feed(1,1300,true));assert(!t.feed(1.2,900,false)); // Deployment screen disappeared.
 assert(!t.feed(1.4,700,true));assert(t.feed(1.6,500,true));
 t={};assert(!t.feed(1,7200,true));assert(!t.feed(1.2,900,true)); // Implausible OCR jump.
 assert(t.feed(1.4,700,true));
 t={};assert(!t.feed(1,1300,true));assert(!t.feed(1.2,-1,true));assert(t.feed(1.4,900,true)); // One missed OCR frame.
 t={};assert(!t.feed(1,1300,true));assert(!t.feed(3,700,true));
 Gate gate;gate.line("application|MatchingCompleted:40",1);assert(gate.armed&&!gate.done);unsigned token=gate.token;
 gate.line("application|GameStarting",2);gate.line("application|GameStarted",3);assert(gate.token==token); // Logs only arm; never confirm.
 gate.finish();gate.line("application|GameStarted",4);gate.line("application|MatchingCompleted:40",5);assert(!gate.armed&&gate.done);
 gate.line("2026-10-16 12:00|application|scene preset path:maps/woods.bundle",40);assert(gate.armed&&!gate.done);token=gate.token;
 gate.line("2026-10-16 12:00|application|scene preset path:maps/woods.bundle",41);assert(gate.token==token);
 gate.line("application|Network game matching cancelled",42);assert(!gate.armed&&gate.token!=token);
 gate.line("application|GameStarting",43);assert(!gate.armed);
 gate.line("application|TRACE-NetworkGameCreate profileStatus",50);assert(gate.armed);
 gate.line("Got notification | UserMatchOver",51);assert(!gate.armed);
 gate.line("application|GameStarted",52);assert(!gate.armed);
 gate={};gate.line("application|MatchingCompleted:40",1);gate.line("PrepareSelectedProfileLocally ProfileId: example",2);assert(gate.armed);
 gate.line("application|GameStarted",3);gate.line("SelectProfile ProfileId: example",4);assert(!gate.armed);
 std::cout<<"PASS: deployment milliseconds / countdown evidence / threshold / gaps / cancellation / once\n";
}
