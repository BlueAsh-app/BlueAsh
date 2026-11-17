// PUBG 에임랩 rules: the saved sensitivity, cm/360 and type, trial scoring, the candidate order, the verdict,
// one match's telemetry and the before/after comparison.
#include "aim_core.h"
#include <cassert>
#include <cstdio>
int main(){
    // the empty map's vertical value (1.0) is not the mouse one; the populated map's (0.9) is
    std::string ini="GamePadSensitiveList=,SensitiveMap=(),MouseVerticalSensitivityMultiplierAdjusted=1.000000,X=1,SensitiveMap=((Mouse, (Array=((SensitiveName=\"Normal\",Sensitivity=32.490974,LastConvertedSensitivity=0.008938),(SensitiveName=\"Targeting\",Sensitivity=21.000000,LastConvertedSensitivity=0.005266),(SensitiveName=\"Scope4X\",Sensitivity=25.000000,LastConvertedSensitivity=0.006318))))),MouseVerticalSensitivityMultiplierAdjusted=0.900000,bInvertMouse=False";
    auto s=aim::readIni(ini);assert(s.found&&std::fabs(s.normal-32.490974)<1e-6&&s.targeting==21&&std::fabs(s.vertical-.9)<1e-9&&s.scopes.size()==1);
    assert(std::fabs(aim::converted(32.490974)/0.008938-1)<.003&&std::fabs(aim::converted(21)/0.005266-1)<.003&&std::fabs(s.normalConv-0.008938)<1e-9&&std::fabs(aim::converted(50)-0.02)<1e-12);
    assert(std::fabs(aim::cm360(50,800)-22.86)<0.01);
    assert(aim::typeOf(20)==aim::High&&aim::typeOf(25)==aim::Mid&&aim::typeOf(45)==aim::Mid&&aim::typeOf(46)==aim::Low&&aim::typeOf(0)==aim::Unknown);
    assert(std::fabs(aim::converted(aim::sliderFor(32,1.05))/aim::converted(32)-1.05)<1e-9);
    // flick: from 10 deg right, overshoot to -1, settle back inside, click at 0.5 s
    std::vector<aim::Sample> f;for(int i=0;i<=50;i++){double t=i*.01,x=t<=.3?10-t/.3*11:t<=.4?-1+(t-.3)/.1:0;f.push_back({t,x,0});}
    auto tr=aim::flick(f,1.5,0.5,1.0);assert(tr.valid&&tr.hit&&tr.overshoot>9&&tr.overshoot<11&&tr.reach>0.2&&tr.reach<0.3&&std::fabs(tr.score-.5)<1e-9);
    auto miss=aim::flick({{0,10,0},{.2,5,0},{.4,3,0}},1,0.4,1);assert(miss.valid&&!miss.hit&&std::fabs(miss.score-.9)<1e-9);
    assert(!aim::flick({{0,10,0},{.2,9.9,0},{.4,9.8,0}},1,0.4,1).valid); // barely moved
    std::vector<aim::Sample> rc;for(int i=0;i<100;i++)rc.push_back({i*.02,0,(i%2?.5:-.5)});assert(std::fabs(aim::recoil(rc,1).score-.5)<1e-9);
    // schedule: every block holds every candidate the same number of times
    auto sch=aim::schedule({.95,1,1.05},4,2,7);assert(sch.size()==24);for(int b=0;b<4;b++){int c=0;for(int i=b*6;i<b*6+6;i++)if(sch[i]==1)c++;assert(c==2);}
    // verdict: 1.05 clearly faster -> Better; no difference -> Same; too few -> Short
    std::vector<aim::Trial> ts;std::mt19937 rng(3);std::normal_distribution<double> n(0,.03);
    for(int i=0;i<12;i++)for(double m:{.95,1.0,1.05}){aim::Trial t;t.kind=aim::Flick;t.mult=m;t.valid=true;t.score=(m==1.05?.60:.75)+n(rng);ts.push_back(t);}
    auto v=aim::decide(ts,aim::Flick,6);assert(v.state==aim::Better&&v.mult==1.05&&v.gain>10);
    for(auto& t:ts){t.score=.7+n(rng);}assert(aim::decide(ts,aim::Flick,6).state==aim::Same);assert(aim::decide(ts,aim::Flick,20).state==aim::Short);
    aim::prune(ts,3);int out=0;for(auto& t:ts)if(!t.valid)out++;assert(out>=3);
    {std::vector<aim::Trial> two;for(int i=0;i<10;i++)for(double m:{1.0,.95}){aim::Trial t;t.mult=m;t.valid=true;t.score=(m==1?.6:.9)+i*.001;two.push_back(t);}aim::prune(two,0);for(auto& t:two)assert(t.valid);} // a worse candidate is not pruned as wild
    // telemetry: shots in tens, damage split by riding / falling / on foot, knocks and kills with counted weapons only
    std::string tel=R"([{"_T":"LogMatchStart","_D":"2026-11-15T10:00:00.000Z"},
 {"_T":"LogParachuteLanding","_D":"2026-11-15T10:02:00Z","character":{"accountId":"v1"}},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:00Z","character":{"accountId":"me"},"weaponId":"WeapHK416_C","fireCount":10},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:00Z","character":{"accountId":"me"},"weaponId":"WeapHK416_C","fireCount":20},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:00Z","character":{"accountId":"me"},"weaponId":"WeapHK416_C","fireCount":30},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:00Z","character":{"accountId":"me"},"weaponId":"WeapHK416_C","fireCount":10},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:00Z","character":{"accountId":"me"},"weaponId":"WeapHK416_C","fireCount":20},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:01Z","character":{"accountId":"me"},"weaponId":"WeapKar98k_C","fireCount":10},
 {"_T":"LogWeaponFireCount","_D":"2026-11-15T10:03:02Z","character":{"accountId":"me"},"weaponId":"WeapUMP_C","fireCount":20},
 {"_T":"LogPlayerTakeDamage","_D":"2026-11-15T10:03:03Z","attacker":{"accountId":"me","location":{"x":1}},"victim":{"accountId":"v1"},"damageTypeCategory":"Damage_Gun","damage":40.5,"damageCauserName":"WeapHK416_C","extra":[1,{"a":2}]},
 {"_T":"LogPlayerTakeDamage","_D":"2026-11-15T10:03:04Z","attacker":{"accountId":"me"},"victim":{"accountId":"v2"},"damageTypeCategory":"Damage_Gun","damage":20,"damageCauserName":"WeapHK416_C"},
 {"_T":"LogVehicleRide","_D":"2026-11-15T10:04:00Z","character":{"accountId":"v1"}},
 {"_T":"LogPlayerTakeDamage","_D":"2026-11-15T10:04:01Z","attacker":{"accountId":"me"},"victim":{"accountId":"v1"},"damageTypeCategory":"Damage_Gun","damage":10,"damageCauserName":"WeapUMP_C"},
 {"_T":"LogVehicleDamage","_D":"2026-11-15T10:04:02Z","attacker":{"accountId":"me"},"damage":30,"damageCauserName":"WeapUMP_C"},
 {"_T":"LogPlayerMakeGroggy","_D":"2026-11-15T10:04:03Z","attacker":{"accountId":"me"},"damageCauserName":"WeapHK416_C"},
 {"_T":"LogPlayerKillV2","_D":"2026-11-15T10:04:04Z","killer":{"accountId":"me"},"killerDamageInfo":{"damageCauserName":"WeapKar98k_C"}},
 {"_T":"LogMatchEnd","_D":"2026-11-15T10:30:00Z"}])";
    auto m=aim::summarize(tel,"me");assert(m.fired==70&&m.valid&&std::fabs(m.damage-40.5)<1e-9&&m.fallingDamage==20&&m.riderDamage==10&&m.vehicleDamage==30&&m.knocks==1&&m.kills==0);
    {std::string d=R"([{"_T":"LogWeaponFireCount","character":{"accountId":"me"},"weaponId":"WeapUMP_C","fireCount":10},{"_T":"LogWeaponFireCount","character":{"accountId":"me"},"weaponId":"WeapUMP_C","fireCount":10},{"_T":"LogWeaponFireCount","character":{"accountId":"me"},"weaponId":"WeapUMP_C","fireCount":10}])";assert(aim::summarize(d,"me").fired==30);} // steps of 10
    assert(m.end-m.at==1800);assert(!aim::summarize(tel,"me",80).valid);
    // real-format shots and hits (field names as in 2025 telemetry): two weapons, a weapon picked up again (its running
    // total restarts), a shot recorded twice on the victim and a pellet-like second record (one hit), a teammate hit
    // (left out), a downed enemy hit in a vehicle (rider damage), a weapon outside the classes (left out)
    {auto atk=[](int id,const char* w){return std::string("{\"_T\":\"LogPlayerAttack\",\"attackId\":")+std::to_string(id)+",\"attacker\":{\"accountId\":\"me\",\"teamId\":1},\"attackType\":\"Weapon\",\"weapon\":{\"itemId\":\""+w+"\"}},";};
     auto dmg=[](int id,const char* v,int team,double d,const char* w,bool car=false){return std::string("{\"_T\":\"LogPlayerTakeDamage\",\"attackId\":")+std::to_string(id)+",\"attacker\":{\"accountId\":\"me\",\"teamId\":1},\"victim\":{\"accountId\":\""+v+"\",\"teamId\":"+std::to_string(team)+",\"isInVehicle\":"+(car?"true":"false")+"},\"damageTypeCategory\":\"Damage_Gun\",\"damage\":"+std::to_string(d)+",\"damageCauserName\":\""+w+"\"},";};
     auto fc=[](const char* w,int n){return std::string("{\"_T\":\"LogWeaponFireCount\",\"character\":{\"accountId\":\"me\"},\"weaponId\":\"")+w+"\",\"fireCount\":"+std::to_string(n)+"},";};
     std::string t="[";for(int i=1;i<=25;i++){t+=atk(i,"Item_Weapon_HK416_C");if(i==10)t+=fc("Item_Weapon_HK416_C",10);if(i==20)t+=fc("Item_Weapon_HK416_C",20);}
     for(int i=26;i<=37;i++){t+=atk(i,"Item_Weapon_HK416_C");if(i==35)t+=fc("Item_Weapon_HK416_C",10);} // picked up again: 10 after 20
     for(int i=38;i<=45;i++){t+=atk(i,"Item_Weapon_UMP_C");}t+=atk(46,"Item_Weapon_Kar98k_C");
     t+=dmg(3,"e1",2,30,"WeapHK416_C")+dmg(3,"e1",2,30,"WeapHK416_C")+dmg(7,"e2",2,25,"WeapHK416_C")+dmg(8,"t1",1,40,"WeapHK416_C")+dmg(40,"e3",3,20,"WeapUMP_C")+dmg(41,"e3",3,15,"WeapUMP_C",true)+dmg(46,"e4",4,90,"WeapKar98k_C");
     t+="{\"_T\":\"LogMatchEnd\"}]";auto r=aim::summarize(t,"me",30);
     assert(r.hitsOk&&r.fired==45&&r.classFired[0]==37&&r.classFired[1]==8);      // every attack counted, Kar98k left out
     assert(r.classHits[0]==2&&r.classHits[1]==2&&r.hits==4);                    // shot 3 once, teammate shot 8 left out
     assert(std::fabs(r.damage-(30+30+25+20))<1e-6&&std::fabs(r.riderDamage-15)<1e-6&&std::fabs(aim::accuracy(r)-100.0*4/45)<1e-9&&r.valid);
     // a log missing shots (fewer attacks than the running total): shots from the totals, no hit rate
     std::string g="["+fc("Item_Weapon_HK416_C",10)+fc("Item_Weapon_HK416_C",20)+fc("Item_Weapon_HK416_C",30)+fc("Item_Weapon_HK416_C",40)+atk(1,"Item_Weapon_HK416_C")+dmg(1,"e1",2,30,"WeapHK416_C")+"{\"_T\":\"LogMatchEnd\"}]";
     auto q=aim::summarize(g,"me",30);assert(!q.hitsOk&&q.fired==40&&q.hits==0&&aim::accuracy(q)<0&&q.valid);
     assert(aim::weaponCore("Item_Weapon_SCAR-L_C")=="SCAR-L"&&aim::weaponClass("WeapDuncansHK416_C")==0&&aim::weaponClass("Item_Weapon_Mk14_C")==2&&aim::weaponClass("WeapM1911_C")<0);}
    // split: the match running at the change counts on neither side
    std::vector<aim::Match> all;for(int i=0;i<8;i++){aim::Match x;x.id=std::to_string(i);x.valid=true;x.at=1000+i*100;x.end=x.at+50;all.push_back(x);}
    std::vector<aim::Match> b,a;aim::split(all,1420,10,b,a);assert(b.size()==4&&a.size()==3);aim::split(all,1440,10,b,a);assert(b.size()==4&&a.size()==3);
    aim::split(all,1460,2,b,a);assert(b.size()==2&&b[0].id=="4"&&a.size()==2&&a[0].id=="5");
    auto c=aim::compare({10,11,9,10},{15,16,14,15});assert(c.trend==aim::Up&&std::fabs(c.diff-5)<1e-9&&std::fabs(c.rel-50)<1e-9);
    assert(aim::compare({10,11},{15,16,17}).trend==aim::Few);assert(aim::compare({10,14,9,13},{11,13,10,12}).trend==aim::Unclear);
    // calibration: 22.86 cm at 800 DPI with sensitivity 50 is the 2.5 yaw; a measured 30 cm gives a slower real yaw
    assert(std::fabs(aim::calibrate(22.86,800,aim::converted(50))-2.5)<1e-3&&std::fabs(aim::cm360c(aim::converted(50),800,aim::calibrate(30,800,aim::converted(50)))-30)<1e-9);
    assert(aim::calibrate(2,800,.02)==0&&aim::calibrate(30,0,.02)==0);assert(std::fabs(aim::dpiFrom(-3150,10)-800.1)<.1);
    assert(aim::consistent(aim::Better,1.05,aim::Better,.95)==aim::Mixed&&aim::consistent(aim::Better,1.05,aim::Better,1.05)==aim::Better&&aim::consistent(aim::Same,1,aim::Better,.95)==aim::Better);
    puts("aim_core ok");}
