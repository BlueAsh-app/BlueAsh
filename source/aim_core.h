#pragma once
// PUBG 에임랩: the pure rules (no Windows calls), so aim_core_tests.cpp can include it alone.
//  - the mouse sensitivity PUBG saved (GameUserSettings.ini, read only), cm/360 and the BlueAsh sensitivity type
//  - scoring one practice trial (flick / vertical recoil) from crosshair offsets, the shuffled candidate order, the verdict
//  - summing one match's telemetry events for one account and comparing matches before/after a sensitivity change
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <random>
#include <map>
#include <set>
#include <ctime>
namespace aim {
// ---- sensitivity --------------------------------------------------------------------------------------------------
inline double converted(double slider){return 0.002*std::pow(10.0,slider/50.0);}
struct Sens {bool found=false;double normal=0,normalConv=0,targeting=0,scoping=0,vertical=1;std::vector<std::pair<std::string,double>> scopes;};
// The mouse block is the last non-empty "SensitiveMap=((Mouse"; its vertical multiplier follows it (an empty map before it
// carries another, unused one).
inline Sens readIni(const std::string& t){Sens s;size_t at=t.rfind("SensitiveMap=((Mouse");if(at==std::string::npos)return s;
    size_t end=t.find("MouseVerticalSensitivityMultiplierAdjusted=",at);std::string block=t.substr(at,end==std::string::npos?std::string::npos:end-at);
    size_t p=0;while((p=block.find("SensitiveName=\"",p))!=std::string::npos){p+=15;size_t q=block.find('"',p);if(q==std::string::npos)break;std::string name=block.substr(p,q-p);
        size_t v=block.find("Sensitivity=",q);if(v==std::string::npos)break;double val=atof(block.c_str()+v+12);
        if(name=="Normal"){s.normal=val;size_t lc=block.find("LastConvertedSensitivity=",v);if(lc!=std::string::npos&&lc<block.find("SensitiveName",v)){s.normalConv=atof(block.c_str()+lc+25);}}else if(name=="Targeting")s.targeting=val;else if(name=="Scoping")s.scoping=val;else s.scopes.push_back({name,val});p=v;}
    if(end!=std::string::npos){s.vertical=atof(t.c_str()+end+43);}if(s.normalConv<=0&&s.normal>0){s.normalConv=converted(s.normal);}s.found=s.normal>0;return s;}
// PUBG turns the slider into LastConvertedSensitivity, close to 0.002 * 10^(S/50) (within 0.3 % of the saved pairs; the
// saved value is used when there is one).
// shortcut: degrees per mouse count = converted * 2.5 (Unreal's default yaw scale); PUBG publishes no yaw value, so
// without the player's own measurement (calibrate) cm/360 and the practice speed are an estimate.
const double defaultYaw=2.5;
inline double degPerCount(double slider,double yaw=defaultYaw){return converted(slider)*yaw;}
inline double cm360c(double conv,int dpi,double yaw=defaultYaw){return dpi>0&&conv>0&&yaw>0?360.0*2.54/(dpi*conv*yaw):0;}
inline double cm360(double slider,int dpi){return cm360c(converted(slider),dpi);}
// 감도 보정: the player turns once (360 degrees) in PUBG with the mouse along a ruler and types the distance. With the
// DPI that gives PUBG's real degrees per count, and the yaw scale is that divided by the converted value (0 = not usable).
inline double calibrate(double cmPer360,int dpi,double conv){if(cmPer360<5||cmPer360>300||dpi<100||conv<=0)return 0;double y=360.0*2.54/(cmPer360*dpi)/conv;return y>0.2&&y<20?y:0;}
// DPI from a straight move of a known length: counts / inches.
inline double dpiFrom(long counts,double cm){return cm>0?std::abs(counts)/(cm/2.54):0;}
// The slider giving a multiplier of the current speed.
inline double sliderFor(double slider,double mult){return slider+50.0*std::log10(mult);}
enum Type {High=0,Mid=1,Low=2,Unknown=3};
// BlueAsh's own split (not a PUBG standard): under 25 cm high, 25..45 middle, over 45 low.
inline Type typeOf(double cm){return cm<=0?Unknown:cm<25?High:cm<=45?Mid:Low;}
// ---- practice ------------------------------------------------------------------------------------------------------
enum Kind {Flick=0,Recoil=1,Ads=2};
struct Sample {double t,x,y;}; // seconds since the trial began, crosshair -> target offset in degrees
struct Trial {int kind=Flick;double mult=1;bool valid=false,hit=false;double score=0,reach=0,settle=0,overshoot=0,error=0;};
// Flick: the target appears off centre, the player turns and clicks. reach = first time inside the target, settle = when it
// last came inside before the click, overshoot = how far it went past the target along the first direction (in % of the
// starting distance), error = distance at the click in target radii. Score (lower is better): the click time, +0.5 s for a miss.
inline Trial flick(const std::vector<Sample>& s,double radius,double clickT,double mult){Trial r;r.kind=Flick;r.mult=mult;
    if(s.size()<3||radius<=0||clickT<=0){return r;}double d0=std::hypot(s[0].x,s[0].y);if(d0<=radius){return r;}double ux=s[0].x/d0,uy=s[0].y/d0;
    double minAlong=1e9,path=0,lastIn=-1;bool in=false;r.reach=-1;Sample at=s.back();
    for(size_t i=0;i<s.size()&&s[i].t<=clickT;i++){double d=std::hypot(s[i].x,s[i].y);bool now=d<=radius;if(now&&r.reach<0)r.reach=s[i].t;if(now&&!in)lastIn=s[i].t;in=now;
        minAlong=std::min(minAlong,s[i].x*ux+s[i].y*uy);if(i)path+=std::hypot(s[i].x-s[i-1].x,s[i].y-s[i-1].y);at=s[i];}
    r.error=std::hypot(at.x,at.y)/radius;r.hit=r.error<=1;r.settle=r.hit?lastIn:clickT;r.overshoot=std::max(0.0,-minAlong)/d0*100;
    r.valid=path>=0.3*d0&&clickT<=3.0;r.score=clickT+(r.hit?0:0.5);return r;}
// Recoil: the view climbs while the button is held; the player pulls down to stay on the target. Score = RMS vertical
// offset (degrees) over the hold, skipping the first 0.15 s (reaction).
inline Trial recoil(const std::vector<Sample>& s,double mult){Trial r;r.kind=Recoil;r.mult=mult;double sum=0,path=0;int n=0;
    for(size_t i=0;i<s.size();i++){if(i)path+=std::fabs(s[i].y-s[i-1].y);if(s[i].t<.15)continue;sum+=s[i].y*s[i].y;n++;}
    if(n<10){return r;}r.score=std::sqrt(sum/n);r.error=r.score;r.valid=path>0.2;return r;}
// The order: each block holds every candidate `per` times, shuffled, so fatigue and practice spread evenly over them.
inline std::vector<double> schedule(const std::vector<double>& cands,int blocks,int per,unsigned seed){std::vector<double> out;std::mt19937 rng(seed);
    for(int b=0;b<blocks;b++){std::vector<double> blk;for(double c:cands)for(int i=0;i<per;i++)blk.push_back(c);std::shuffle(blk.begin(),blk.end(),rng);out.insert(out.end(),blk.begin(),blk.end());}return out;}
inline double median(std::vector<double> v){if(v.empty())return 0;std::sort(v.begin(),v.end());size_t n=v.size();return n%2?v[n/2]:(v[n/2-1]+v[n/2])/2;}
struct Mean {double mean=0,se=0;int n=0;};
inline Mean meanOf(const std::vector<double>& v){Mean m;m.n=(int)v.size();if(!m.n)return m;m.mean=std::accumulate(v.begin(),v.end(),0.0)/m.n;
    if(m.n>1){double s=0;for(double x:v)s+=(x-m.mean)*(x-m.mean);m.se=std::sqrt(s/(m.n-1)/m.n);}return m;}
// Warm-up and wild trials out: the first `warm` trials of a kind, then, within each candidate (a worse candidate's normal
// trials must not look wild next to a better one's), scores beyond median + 3 * MAD (MAD at least 5 % of the median).
inline void prune(std::vector<Trial>& ts,int warm){std::map<int,int> seen;for(auto& t:ts){if(seen[t.kind]++<warm){t.valid=false;}}
    std::map<std::pair<int,int>,std::vector<double>> by;for(auto& t:ts){if(t.valid){by[{t.kind,(int)std::lround(t.mult*100)}].push_back(t.score);}}
    for(auto& [key,sc]:by){if(sc.size()<5){continue;}double m=median(sc);std::vector<double> dev;for(double x:sc){dev.push_back(std::fabs(x-m));}double lim=m+3*std::max(median(dev),.05*std::fabs(m));
        for(auto& t:ts){if(t.valid&&t.kind==key.first&&(int)std::lround(t.mult*100)==key.second&&t.score>lim){t.valid=false;}}}}
enum State {Better=0,Same=1,Short=2,Mixed=3};
// Against the last analysis (same sensitivity): two "better" answers pointing to different candidates mean the measurement
// is not repeatable yet, so neither is recommended (Mixed) until another run agrees.
inline int consistent(int prevState,double prevMult,int state,double mult){return prevState==Better&&state==Better&&std::fabs(prevMult-mult)>.001?Mixed:state;}
struct Verdict {int state=Short;double mult=1,gain=0;Mean base,best;};
// A candidate wins only when its mean score is lower than the current one (1.00) by more than twice the combined standard
// error and by at least 3 %; otherwise the current sensitivity stays.
inline Verdict decide(const std::vector<Trial>& ts,int kind,int need){Verdict v;std::map<double,std::vector<double>> by;for(auto& t:ts)if(t.kind==kind&&t.valid)by[std::round(t.mult*100)/100].push_back(t.score);
    auto b=by.find(1.0);if(b==by.end()||(int)b->second.size()<need)return v;v.base=meanOf(b->second);v.best=v.base;v.state=Same;
    for(auto& [m,sc]:by){if(m==1.0)continue;if((int)sc.size()<need){v.state=Short;return v;}Mean c=meanOf(sc);double diff=v.base.mean-c.mean,se=std::sqrt(v.base.se*v.base.se+c.se*c.se);
        if(diff>2*se&&diff>=0.03*v.base.mean&&c.mean<v.best.mean){v.best=c;v.mult=m;v.state=Better;v.gain=diff/v.base.mean*100;}}return v;}
// ---- telemetry -----------------------------------------------------------------------------------------------------
// Weapon classes counted (0 AR, 1 SMG, 2 DMR, 3 LMG); anything else is left out. Telemetry names a weapon two ways:
// "Item_Weapon_HK416_C" (LogPlayerAttack, LogWeaponFireCount) and "WeapHK416_C" (damage, knocks, kills) — checked on real
// 2025 match files — so both are reduced to the core name. Names from PUBG's api-assets damageCauserName dictionary,
// including the skin variants it lists (DuncansHK416, LunchmeatsAK47, MadsQBU88).
inline std::string weaponCore(std::string id){if(id.rfind("Item_Weapon_",0)==0)id=id.substr(12);else if(id.rfind("Weap",0)==0)id=id.substr(4);if(id.size()>2&&id.compare(id.size()-2,2,"_C")==0)id.resize(id.size()-2);return id;}
inline int weaponClass(const std::string& id){static const std::map<std::string,int> w={
    {"HK416",0},{"DuncansHK416",0},{"AK47",0},{"LunchmeatsAK47",0},{"SCAR-L",0},{"M16A4",0},{"BerylM762",0},{"G36C",0},{"QBZ95",0},{"AUG",0},{"Groza",0},{"Mk47Mutant",0},{"ACE32",0},{"K2",0},{"FamasG2",0},
    {"UMP",1},{"Vector",1},{"UZI",1},{"UziPro",1},{"Thompson",1},{"BizonPP19",1},{"MP5K",1},{"P90",1},{"JS9",1},{"MP9",1},
    {"Mini14",2},{"SKS",2},{"FNFal",2},{"QBU88",2},{"MadsQBU88",2},{"Mk14",2},{"VSS",2},{"Mk12",2},{"Dragunov",2},
    {"M249",3},{"DP28",3},{"MG3",3}};auto f=w.find(weaponCore(id));
#ifdef AIM_COUNT_ALL_GUNS
    if(f==w.end()){static const std::set<std::string> gun={"M1911","Kar98k","M24","M9","G18","Win94","MosinNagant","AWM","Berreta686","Saiga12","Winchester","DP12","DesertEagle","Rhino","NagantM1895","vz61Skorpion","L6","OriginS12","Sawnoff","Crossbow_1"};return gun.count(weaponCore(id))?0:-1;} // verification on real files that have no AR/SMG/DMR/LMG
#endif
    return f==w.end()?-1:f->second;}
// One event, its fields flattened to "a.b.c" -> text (numbers as written). Telemetry events are small objects, so a flat
// map is enough; arrays are skipped.
using Flat=std::map<std::string,std::string>;
inline void skipWs(const std::string& s,size_t& i){while(i<s.size()&&(s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t'))i++;}
inline bool str(const std::string& s,size_t& i,std::string& out){if(i>=s.size()||s[i]!='"')return false;i++;out.clear();
    while(i<s.size()&&s[i]!='"'){if(s[i]=='\\'&&i+1<s.size()){i++;out+=s[i]=='n'?'\n':s[i];}else out+=s[i];i++;}if(i>=s.size())return false;i++;return true;}
inline bool value(const std::string& s,size_t& i,const std::string& key,Flat& f,int depth){skipWs(s,i);if(i>=s.size()||depth>12)return false;char c=s[i];
    if(c=='{'){i++;skipWs(s,i);if(i<s.size()&&s[i]=='}'){i++;return true;}
        while(i<s.size()){skipWs(s,i);std::string k;if(!str(s,i,k))return false;skipWs(s,i);if(i>=s.size()||s[i]!=':')return false;i++;
            if(!value(s,i,key.empty()?k:key+"."+k,f,depth+1)){return false;}skipWs(s,i);if(i<s.size()&&s[i]==','){i++;continue;}if(i<s.size()&&s[i]=='}'){i++;return true;}return false;}return false;}
    if(c=='['){int d=0;bool q=false;for(;i<s.size();i++){char x=s[i];if(q){if(x=='\\')i++;else if(x=='"')q=false;}else if(x=='"')q=true;else if(x=='['||x=='{')d++;else if(x==']'||x=='}'){if(--d==0){i++;return true;}}}return false;}
    if(c=='"'){std::string v;if(!str(s,i,v))return false;f[key]=v;return true;}
    size_t b=i;while(i<s.size()&&s[i]!=','&&s[i]!='}'&&s[i]!=']'&&s[i]!=' '&&s[i]!='\n'&&s[i]!='\r')i++;f[key]=s.substr(b,i-b);return true;}
// "2026-11-15T12:34:56.789Z" -> Unix seconds (UTC).
inline double isoTime(const std::string& z){if(z.size()<19)return 0;std::tm t={};t.tm_year=atoi(z.c_str())-1900;t.tm_mon=atoi(z.c_str()+5)-1;t.tm_mday=atoi(z.c_str()+8);t.tm_hour=atoi(z.c_str()+11);t.tm_min=atoi(z.c_str()+14);t.tm_sec=atoi(z.c_str()+17);
    double frac=z.size()>20&&z[19]=='.'?atof(z.c_str()+19):0;
#ifdef _WIN32
    return (double)_mkgmtime(&t)+frac;
#else
    return (double)timegm(&t)+frac;
#endif
}
struct Match {std::string id;double at=0,end=0;int fired=0,hits=0,knocks=0,kills=0;double damage=0,riderDamage=0,vehicleDamage=0,fallingDamage=0;bool valid=false,hitsOk=false;
    int classFired[4]={0,0,0,0},classHits[4]={0,0,0,0};double classDamage[4]={0,0,0,0};};
// One match for one account, checked against real 2025 telemetry:
//  - shots: every LogPlayerAttack (attackType "Weapon") with a counted weapon; its fireWeaponStackCount rises 1,2,3.. per
//    shot. LogWeaponFireCount is the running total per weapon written at each 10th shot (10,20,..; a player with 69
//    attacks ended at 60), kept as a floor: a fall in it starts a new run (a picked-up weapon counts from zero again).
//  - hits: shots whose attackId appears on a gun LogPlayerTakeDamage to an enemy (one per attackId: several pellets or a
//    repeated record of the same shot count once). Hits are trusted (hitsOk) only when the attacks are at least the
//    running totals, i.e. no shots are missing from the log; otherwise shots come from the totals and no hit rate is shown.
//  - damage to enemies on foot and landed, by class; to riders (isInVehicle, or between LogVehicleRide and Leave), to
//    those still in the air (before their LogParachuteLanding) and to vehicles is kept apart. Knocks, kills by class.
inline Match summarize(const std::string& json,const std::string& account,int minShots=60){Match m;std::set<std::string> riding,landed;bool anyLanding=false;
    std::map<std::string,std::pair<int,int>> fire; /* weapon -> (finished runs, current run) of LogWeaponFireCount */
    std::map<std::string,int> attackClass;std::set<std::string> hitAttacks;int attacks[4]={0,0,0,0};std::string team;
    size_t i=json.find('[');if(i==std::string::npos)return m;i++;
    while(i<json.size()){skipWs(json,i);if(i>=json.size()||json[i]==']')break;if(json[i]==','){i++;continue;}Flat f;size_t start=i;if(!value(json,i,"",f,0)){if(i==start)i++;continue;}
        const std::string& T=f["_T"];double t=isoTime(f["_D"]);
        if(T=="LogMatchStart")m.at=t;else if(T=="LogMatchEnd")m.end=t;
        else if(T=="LogVehicleRide")riding.insert(f["character.accountId"]);else if(T=="LogVehicleLeave")riding.erase(f["character.accountId"]);
        else if(T=="LogParachuteLanding"){landed.insert(f["character.accountId"]);anyLanding=true;}
        else if(T=="LogPlayerAttack"){if(f["attacker.accountId"]!=account||f["attackType"]!="Weapon")continue;if(team.empty())team=f["attacker.teamId"];int c=weaponClass(f["weapon.itemId"]);if(c<0)continue;attacks[c]++;attackClass[f["attackId"]]=c;}
        else if(T=="LogWeaponFireCount"){if(f["character.accountId"]==account&&weaponClass(f["weaponId"])>=0){int v=atoi(f["fireCount"].c_str());auto& w=fire[weaponCore(f["weaponId"])];if(v>w.second){w.second=v;}else{w.first+=w.second;w.second=v;}}}
        else if(T=="LogPlayerTakeDamage"){if(f["attacker.accountId"]!=account||f["victim.accountId"]==account||f["damageTypeCategory"]!="Damage_Gun")continue;int c=weaponClass(f["damageCauserName"]);if(c<0)continue;
            if(team.empty()){team=f["attacker.teamId"];}if(!team.empty()&&f["victim.teamId"]==team)continue; /* teammates */
            double d=atof(f["damage"].c_str());const std::string& v=f["victim.accountId"];
            if(f["victim.isInVehicle"]=="true"||riding.count(v))m.riderDamage+=d;else if(anyLanding&&!landed.count(v))m.fallingDamage+=d;else{m.damage+=d;m.classDamage[c]+=d;}
            auto a=attackClass.find(f["attackId"]);if(a!=attackClass.end()&&hitAttacks.insert(a->first).second)m.classHits[a->second]++;}
        else if(T=="LogVehicleDamage"){if(f["attacker.accountId"]==account&&weaponClass(f["damageCauserName"])>=0)m.vehicleDamage+=atof(f["damage"].c_str());}
        else if(T=="LogPlayerMakeGroggy"){if(f["attacker.accountId"]==account&&weaponClass(f["damageCauserName"])>=0)m.knocks++;}
        else if(T=="LogPlayerKillV2"){if(f["killer.accountId"]==account&&weaponClass(f["killerDamageInfo.damageCauserName"])>=0)m.kills++;}}
    int totals[4]={0,0,0,0};for(auto& [id,w]:fire){int c=weaponClass(id);if(c>=0)totals[c]+=w.first+w.second;}
    m.hitsOk=true;for(int c=0;c<4;c++){if(attacks[c]<totals[c])m.hitsOk=false;}
    for(int c=0;c<4;c++){m.classFired[c]=m.hitsOk?attacks[c]:std::max(attacks[c],totals[c]);m.fired+=m.classFired[c];m.hits+=m.classHits[c];}
    if(!m.hitsOk){m.hits=0;for(int& h:m.classHits)h=0;}m.hitsOk=m.hitsOk&&m.fired>0;m.valid=m.fired>=minShots;return m;}
// Per-match numbers compared before/after the change: 명중률 (hits / shots, only when hitsOk), 100발당 피해, knocks, kills.
inline double per100(const Match& m){return m.fired>0?m.damage/m.fired*100:0;}
inline double accuracy(const Match& m){return m.hitsOk&&m.fired>0?100.0*m.hits/m.fired:-1;}
enum Trend {Up=0,Down=1,Unclear=2,Few=3};
struct Change {Mean before,after;double diff=0,rel=0;int trend=Few;};
// Up/Down only when the means differ by more than twice the combined standard error; fewer than 3 matches on a side = Few.
inline Change compare(const std::vector<double>& b,const std::vector<double>& a){Change c;c.before=meanOf(b);c.after=meanOf(a);if(c.before.n<3||c.after.n<3)return c;
    c.diff=c.after.mean-c.before.mean;c.rel=c.before.mean!=0?c.diff/std::fabs(c.before.mean)*100:0;double se=std::sqrt(c.before.se*c.before.se+c.after.se*c.after.se);
    c.trend=std::fabs(c.diff)>2*se&&std::fabs(c.diff)>0?(c.diff>0?Up:Down):Unclear;return c;}
// The matches either side of the change: up to `n` valid ones that ended before it, and up to `n` that began after it (a
// match running when the change was made counts on neither side).
inline void split(const std::vector<Match>& all,double changedAt,int n,std::vector<Match>& before,std::vector<Match>& after){before.clear();after.clear();
    std::vector<Match> v;for(auto& m:all)if(m.valid&&m.at>0)v.push_back(m);std::sort(v.begin(),v.end(),[](const Match& x,const Match& y){return x.at<y.at;});
    for(auto it=v.rbegin();it!=v.rend()&&(int)before.size()<n;++it)if((it->end>0?it->end:it->at)<changedAt)before.push_back(*it);
    for(auto& m:v)if(m.at>changedAt&&(int)after.size()<n)after.push_back(m);}
}
