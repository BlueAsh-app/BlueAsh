#pragma once
// PUBG 에임랩 (page 18, opened from 게임 도우미 > 배그, button 79). Flow: step 1 PUBG nickname -> step 2 mouse DPI -> 감도 분석 시작 (BlueAsh's own practice screen,
// aim_practice.h) -> the recommendation -> 이 감도 적용 완료 (the moment is kept) -> play as usual -> matches before and
// after it are compared from PUBG's official API (relay/pubg.js keeps the key) and the match telemetry (aim_core.h).
// Views: see setView. Everything is drawn off screen in one picture per paint.
// Files under %LOCALAPPDATA%\BlueAsh\aimlab: account.txt, matches-<platform>-<name>.tsv, profile.txt, history.tsv, result.txt, applied.txt, consent.txt.
// PUBG's own settings file is only read (the sensitivity), never written. Live measuring inside PUBG is off (livePubg).
#include <fstream>
#include <sstream>
#include "aim_practice.h"
#include "attachment_art.h"
namespace aimui {
const bool livePubg=false; // measuring while PUBG runs: off until PUBG's policy on it is confirmed
HWND page=nullptr;int view=0,settingsFrom=0; /* 설정 goes back to where it was opened from (home or the results) */std::wstring note;COLORREF noteColor=0;bool busy=false,refreshing=false;int refreshDone=0,refreshTotal=0;
using KV=std::map<std::string,std::string>;
std::string u8(const std::wstring& w){int n=WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),nullptr,0,nullptr,nullptr);std::string u(n,'\0');if(n)WideCharToMultiByte(CP_UTF8,0,w.c_str(),(int)w.size(),&u[0],n,nullptr,nullptr);return u;}
std::wstring w16(const std::string& u){int n=MultiByteToWideChar(CP_UTF8,0,u.data(),(int)u.size(),nullptr,0);std::wstring w(n,0);if(n)MultiByteToWideChar(CP_UTF8,0,u.data(),(int)u.size(),&w[0],n);return w;}
std::wstring dir(){wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);std::wstring r=std::wstring(d)+dataSub();CreateDirectoryW(r.c_str(),nullptr);
#ifdef BLUEASH_TT
    r+=L"\\aimlab-test";CreateDirectoryW(r.c_str(),nullptr);return r; // test builds never touch the real records
#endif
    r+=L"\\aimlab";CreateDirectoryW(r.c_str(),nullptr);return r;}
std::string readAll(const std::wstring& p){std::string s;FILE* f=_wfopen(p.c_str(),L"rb");if(!f){return s;}char b[65536];size_t n;while((n=fread(b,1,sizeof(b),f))>0)s.append(b,n);fclose(f);return s;}
KV load(const wchar_t* name){KV kv;std::istringstream in(readAll(dir()+L"\\"+name));std::string l;while(std::getline(in,l)){if(!l.empty()&&l.back()=='\r')l.pop_back();auto e=l.find('=');if(e!=std::string::npos)kv[l.substr(0,e)]=l.substr(e+1);}return kv;}
// Written whole to a temporary file, then swapped in, so a crash never leaves half a file.
bool save(const wchar_t* name,const KV& kv){std::wstring p=dir()+L"\\"+name,t=p+L".tmp";std::string body;for(auto& [k,v]:kv){body+=k+"="+v+"\n";}FILE* f=_wfopen(t.c_str(),L"wb");if(!f){return false;}bool ok=fwrite(body.data(),1,body.size(),f)==body.size();ok=fclose(f)==0&&ok;return ok&&MoveFileExW(t.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING)!=0;}
void appendLine(const wchar_t* name,const std::string& line){FILE* f=_wfopen((dir()+L"\\"+name).c_str(),L"ab");if(!f){return;}fputs((line+"\n").c_str(),f);fclose(f);}
double num(const KV& kv,const char* k,double d=0){auto f=kv.find(k);return f==kv.end()||f->second.empty()?d:atof(f->second.c_str());}
std::string str(const KV& kv,const char* k){auto f=kv.find(k);return f==kv.end()?"":f->second;}
std::string fixed(double v,int digits){char b[64];snprintf(b,64,"%.*f",digits,v);return b;}
std::wstring wfixed(double v,int digits){return w16(fixed(v,digits));}
std::wstring fmt(const wchar_t* key,...){wchar_t s[512];va_list a;va_start(a,key);vswprintf(s,512,i18n::text(key),a);va_end(a);return s;}
std::wstring dateOf(double t){if(t<=0)return L"";time_t tt=(time_t)t;tm lt;localtime_s(&lt,&tt);wchar_t b[64];swprintf(b,64,L"%04d-%02d-%02d %02d:%02d",lt.tm_year+1900,lt.tm_mon+1,lt.tm_mday,lt.tm_hour,lt.tm_min);return b;}
// ---- the sensitivity: PUBG's settings file first, the profile's typed values when it is not there --------------------
struct Profile {aim::Sens ini;double normal=0,conv=0,vertical=1,targeting=0,adsConv=0;int dpi=0,type=-1;bool fromIni=false;
    int distance=25,minShots=60;unsigned key=VK_INSERT;bool ads=false,debug=false; // 고급 설정
    double yaw=aim::defaultYaw,calCm=0;bool calibrated=false;}; // 감도 보정: PUBG's real turn rate from the player's own 360 measurement
Profile profile(){Profile p;KV kv=load(L"profile.txt");p.dpi=(int)num(kv,"dpi");p.type=(int)num(kv,"type",-1);p.distance=(int)num(kv,"dist",25);p.minShots=(int)num(kv,"shots",60);p.key=(unsigned)num(kv,"key",VK_INSERT);p.ads=num(kv,"ads")==1;p.debug=num(kv,"debug")==1;
    wchar_t d[MAX_PATH]={};SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,d);p.ini=aim::readIni(readAll(std::wstring(d)+L"\\TslGame\\Saved\\Config\\WindowsNoEditor\\GameUserSettings.ini"));
    if(p.ini.found){p.fromIni=true;p.normal=p.ini.normal;p.conv=p.ini.normalConv;p.vertical=p.ini.vertical;p.targeting=p.ini.targeting;}
    else{p.normal=num(kv,"normal");p.vertical=num(kv,"vertical",1);p.conv=p.normal>0?aim::converted(p.normal):0;p.targeting=num(kv,"targeting");}
    p.adsConv=p.targeting>0?aim::converted(p.targeting):p.conv;p.calCm=num(kv,"cal");double y=num(kv,"yaw");if(y>0){p.yaw=y;p.calibrated=true;}return p;}
// The target is a person-sized circle (0.6 m from the middle to the edge) at the chosen distance (고급 설정; 25 m by default).
double radiusFor(int metres){return std::atan(0.6/std::max(5,metres))*180/3.14159265358979;}
double cmOf(const Profile& p){return aim::cm360c(p.conv,p.dpi,p.yaw);}
int typeOf(const Profile& p){return p.type>=0?p.type:aim::typeOf(cmOf(p));}
const wchar_t* typeName(int t){return t==aim::High?i18n::text(L"고감도"):t==aim::Mid?i18n::text(L"중감도"):t==aim::Low?i18n::text(L"저감도"):L"";}
// A history row when the sensitivity differs from the last one (or on 적용 완료).
void remember(const Profile& p,const char* why){std::istringstream in(readAll(dir()+L"\\history.tsv"));std::string l,last;while(std::getline(in,l))if(!l.empty())last=l;
    std::string row=fixed(p.normal,2)+"\t"+fixed(p.vertical,2)+"\t"+std::to_string(p.dpi);if(strcmp(why,"apply")&&last.size()>row.size()&&last.find(row)!=std::string::npos)return;
    appendLine(L"history.tsv",std::to_string((long long)time(nullptr))+"\t"+row+"\t"+why);}
// ---- PUBG account and matches (threads) ------------------------------------------------------------------------------
KV account(){return load(L"account.txt");}
// A nickname is enough to be signed in: the account id is filled in by the first lookup that reaches PUBG (the server may not be ready yet).
bool linked(){return !str(account(),"name").empty();}
// One match file per nickname, so signing out and back in (or switching accounts) never mixes two players' matches.
std::wstring mfile(){KV a=account();std::string k=str(a,"shard")+"-"+str(a,"name");for(auto& c:k)if(!isalnum((unsigned char)c)&&c!='-'&&c!='_')c='_';return L"matches-"+w16(k)+L".tsv";}
// The match file (mfile), one line per match: id at end fired damage knocks kills valid rider vehicle falling, then (since the hit
// count) hits hitsOk and per class AR SMG DMR LMG: fired hits damage. Kept on this PC past PUBG's 14 days; a match id
// seen twice keeps its last line, so nothing is counted twice.
std::string matchLine(const aim::Match& r){std::string s=r.id+"\t"+fixed(r.at,0)+"\t"+fixed(r.end,0)+"\t"+std::to_string(r.fired)+"\t"+fixed(r.damage,1)+"\t"+std::to_string(r.knocks)+"\t"+std::to_string(r.kills)+"\t"+(r.valid?"1":"0")+"\t"+fixed(r.riderDamage,1)+"\t"+fixed(r.vehicleDamage,1)+"\t"+fixed(r.fallingDamage,1)+"\t"+std::to_string(r.hits)+"\t"+(r.hitsOk?"1":"0");
    for(int c=0;c<4;c++){s+="\t"+std::to_string(r.classFired[c])+"\t"+std::to_string(r.classHits[c])+"\t"+fixed(r.classDamage[c],1);}return s;}
std::vector<aim::Match> matches(){std::vector<aim::Match> v;std::map<std::string,size_t> at;int need=profile().minShots; /* the threshold of 고급 설정, applied when read */std::istringstream in(readAll(dir()+L"\\"+mfile()));std::string l;
    while(std::getline(in,l)){std::vector<std::string> c;std::stringstream s(l);std::string x;while(std::getline(s,x,'\t'))c.push_back(x);if(c.size()<11)continue;
        aim::Match m;m.id=c[0];m.at=atof(c[1].c_str());m.end=atof(c[2].c_str());m.fired=atoi(c[3].c_str());m.damage=atof(c[4].c_str());m.knocks=atoi(c[5].c_str());m.kills=atoi(c[6].c_str());m.valid=m.fired>=need;
        m.riderDamage=atof(c[8].c_str());m.vehicleDamage=atof(c[9].c_str());m.fallingDamage=atof(c[10].c_str());
        if(c.size()>=25){m.hits=atoi(c[11].c_str());m.hitsOk=c[12]=="1";for(int k=0;k<4;k++){m.classFired[k]=atoi(c[13+k*3].c_str());m.classHits[k]=atoi(c[14+k*3].c_str());m.classDamage[k]=atof(c[15+k*3].c_str());}}
        auto f=at.find(m.id);if(f!=at.end()){v[f->second]=m;}else{at[m.id]=v.size();v.push_back(m);}}return v;}
// A telemetry file from PUBG's CDN only (gzip is undone by WinHTTP), at most 96 MB.
bool fetchTelemetry(const std::wstring& url,std::string& out){URL_COMPONENTS u={};u.dwStructSize=sizeof(u);wchar_t host[256]={},rest[2048]={};u.lpszHostName=host;u.dwHostNameLength=256;u.lpszUrlPath=rest;u.dwUrlPathLength=2048;
    if(!WinHttpCrackUrl(url.c_str(),0,0,&u)||u.nScheme!=INTERNET_SCHEME_HTTPS||_wcsicmp(host,L"telemetry-cdn.pubg.com"))return false;
    HINTERNET s=WinHttpOpen(L"BlueAsh",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,nullptr,nullptr,0),c=nullptr,r=nullptr;DWORD status=0,size=sizeof(status);bool ok=false;
    if(s){WinHttpSetTimeouts(s,10000,10000,30000,60000);DWORD dec=WINHTTP_DECOMPRESSION_FLAG_ALL;WinHttpSetOption(s,WINHTTP_OPTION_DECOMPRESSION,&dec,sizeof(dec));c=WinHttpConnect(s,host,u.nPort,0);}
    if(c)r=WinHttpOpenRequest(c,L"GET",rest,nullptr,nullptr,nullptr,WINHTTP_FLAG_SECURE);
    if(r&&WinHttpSendRequest(r,nullptr,0,nullptr,0,0,0)&&WinHttpReceiveResponse(r,nullptr)&&WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr)&&status==200){
        std::vector<char> b(1<<16);DWORD n=0;ok=true;while(WinHttpReadData(r,b.data(),(DWORD)b.size(),&n)&&n){out.append(b.data(),n);if(out.size()>(96u<<20)){ok=false;break;}}}
    for(HINTERNET h:{r,c,s}){if(h)WinHttpCloseHandle(h);}return ok&&!out.empty();}
KV lines(const std::string& body,std::vector<std::string>* many=nullptr){KV kv;std::istringstream in(body);std::string l;while(std::getline(in,l)){auto e=l.find('=');if(e==std::string::npos)continue;std::string k=l.substr(0,e),v=l.substr(e+1);if(many&&k=="match")many->push_back(v);else kv[k]=v;}return kv;}
std::wstring enc(const std::string& s){std::wstring o;for(unsigned char c:s){if(isalnum(c)||c=='-'||c=='_'||c=='.')o+=(wchar_t)c;else{wchar_t b[4];swprintf(b,4,L"%%%02X",c);o+=b;}}return o;}
// 계정 연결: the in-game name on a platform -> the account id (WM_APP+242: wp = HTTP status, lp = the answer).
void connect(const std::string& shard,const std::wstring& name){if(busy)return;busy=true;auto* job=new std::pair<std::string,std::wstring>(shard,name);
    HANDLE t=CreateThread(nullptr,0,[](LPVOID p)->DWORD{std::unique_ptr<std::pair<std::string,std::wstring>> j((std::pair<std::string,std::wstring>*)p);std::string body;
        DWORD st=relayRequest(L"GET",L"/pubg/player?shard="+w16(j->first)+L"&name="+enc(u8(j->second)),std::string(),&body);
        if(st==404&&body.rfind("none",0)!=0)st=503; /* a relay without the PUBG routes yet */
        if(st==200){KV a=lines(body);if(!str(a,"id").empty()){KV kv;kv["shard"]=j->first;kv["id"]=str(a,"id");kv["name"]=str(a,"name");kv["linked"]=std::to_string((long long)time(nullptr));save(L"account.txt",kv);}else st=404;}
        else if(st==503){KV kv;kv["shard"]=j->first;kv["name"]=u8(j->second);kv["linked"]=std::to_string((long long)time(nullptr));save(L"account.txt",kv);} /* server not ready: the nickname is kept, matches come once it is */
        if(!page||!PostMessageW(page,WM_APP+242,st,0)){busy=false;}return 0;},job,0,nullptr);if(t)CloseHandle(t);else{delete job;busy=false;}}
// 전적 새로고침: recent matches not seen yet (newest first, at most 20 kept for the comparison); only the player lookup
// counts against the key's 10 a minute (PUBG: /matches and telemetry are not rate limited). WM_APP+243 after each match, WM_APP+244 at the end (wp = last status).
void refresh(bool force){if(refreshing||!linked())return;KV a=account();if(!force&&time(nullptr)-(long long)num(a,"checked")<600)return;refreshing=true;refreshDone=refreshTotal=0;
    HANDLE t=CreateThread(nullptr,0,[](LPVOID)->DWORD{SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);KV a=account();std::string shard=str(a,"shard"),id=str(a,"id"),body;
        DWORD st=relayRequest(L"GET",L"/pubg/player?shard="+w16(shard)+L"&name="+enc(str(a,"name")),std::string(),&body);if(st==404&&body.rfind("none",0)!=0)st=503;std::vector<std::string> ids;
        if(st==200){KV pa=lines(body,&ids);if(id.empty()){id=str(pa,"id");KV now=account();if(str(now,"name")==str(a,"name")){now["id"]=id;save(L"account.txt",now);}a["id"]=id;}std::set<std::string> known;for(auto& m:matches())known.insert(m.id);std::vector<std::string> todo;for(auto& m:ids)if(!known.count(m)&&todo.size()<20)todo.push_back(m);refreshTotal=(int)todo.size();
            for(auto& m:todo){if(!page)break;std::string mb;DWORD ms=relayRequest(L"GET",L"/pubg/match?shard="+w16(shard)+L"&id="+w16(m),std::string(),&mb);if(ms==429){st=429;break;}
                aim::Match r;r.id=m;bool got=false;if(ms==200){KV mk=lines(mb);std::string tel;if(fetchTelemetry(w16(str(mk,"telemetry")),tel)){r=aim::summarize(tel,id,1);r.id=m;got=true;}if(r.at<=0){r.at=aim::isoTime(str(mk,"at"));r.end=r.at+num(mk,"duration");}}
                if((ms==200&&got)||ms==404){appendLine(mfile().c_str(),matchLine(r));} /* a failed download is tried again next time */
                refreshDone++;if(page)PostMessageW(page,WM_APP+243,0,0);Sleep(300);} /* /matches and telemetry are not counted against the key's rate limit */
            a=account();a["checked"]=std::to_string((long long)time(nullptr));if(!str(a,"id").empty())save(L"account.txt",a);}
        if(!page||!PostMessageW(page,WM_APP+244,st,0)){refreshing=false;}return 0;},nullptr,0,nullptr);if(t)CloseHandle(t);else refreshing=false;}
// ---- what the last analysis found ----------------------------------------------------------------------------------
std::wstring verdictLine(const KV& r,int kind,const Profile& p){const char* s=kind==aim::Flick?"flick":kind==aim::Recoil?"recoil":"ads";int st=(int)num(r,(std::string(s)+".state").c_str(),aim::Short);double m=num(r,(std::string(s)+".mult").c_str(),1),g=num(r,(std::string(s)+".gain").c_str());
    if(st==aim::Mixed){return i18n::text(kind==aim::Flick?L"표적 전환: 지난 측정과 결과가 달라 추천을 보류합니다 · 한 번 더 측정해 주세요":kind==aim::Recoil?L"수직 반동: 지난 측정과 결과가 달라 추천을 보류합니다 · 한 번 더 측정해 주세요":L"조준(ADS): 지난 측정과 결과가 달라 추천을 보류합니다 · 한 번 더 측정해 주세요");}
    if(st==aim::Short){return i18n::text(L"유효한 측정이 부족합니다 · 다시 측정해 주세요");}
    if(st==aim::Same){return i18n::text(kind==aim::Flick?L"표적 전환: 지금 감도와 후보 사이에 뚜렷한 차이가 없었습니다":kind==aim::Recoil?L"수직 반동: 지금 감도와 후보 사이에 뚜렷한 차이가 없었습니다":L"조준(ADS): 지금 감도와 후보 사이에 뚜렷한 차이가 없었습니다");}
    if(kind==aim::Flick)return fmt(L"일반 감도 %ls → %ls에서 표적 전환이 %ls%% 더 빠르게 측정됐습니다",wfixed(p.normal,0).c_str(),wfixed(aim::sliderFor(p.normal,m),0).c_str(),wfixed(g,0).c_str());
    if(kind==aim::Ads)return fmt(L"조준 감도 %ls → %ls에서 조준(ADS) 표적 전환이 %ls%% 더 빠르게 측정됐습니다",wfixed(p.targeting,0).c_str(),wfixed(aim::sliderFor(p.targeting,m),0).c_str(),wfixed(g,0).c_str());
    return fmt(L"수직 감도 배율 %ls → %ls에서 조준 안정성이 %ls%% 더 높게 측정됐습니다",wfixed(p.vertical,2).c_str(),wfixed(p.vertical*m,2).c_str(),wfixed(g,0).c_str());}
bool hasAds(const KV& r){return r.count("ads.state")>0;}
bool hasAdvice(const KV& r){return num(r,"flick.state",aim::Short)==aim::Better||num(r,"recoil.state",aim::Short)==aim::Better||num(r,"ads.state",aim::Short)==aim::Better;}
void storeResult(const aimprac::Result& res){KV r;r["at"]=std::to_string((long long)time(nullptr));r["conv"]=fixed(res.conv,6);r["vertical"]=fixed(res.vertical,3);r["stopped"]=res.stopped?"1":"0";
    auto put=[&](const char* s,const aim::Verdict& v,int done){std::string k=s;r[k+".state"]=std::to_string(v.state);r[k+".mult"]=fixed(v.mult,2);r[k+".gain"]=fixed(v.gain,1);r[k+".base"]=fixed(v.base.mean,3);r[k+".n"]=std::to_string(v.base.n);r[k+".done"]=std::to_string(done);};
    put("flick",res.flick,res.flickDone);put("recoil",res.recoil,res.recoilDone);if(res.withAds){put("ads",res.ads,res.adsDone);}
    // the same sensitivity measured again: two "better" answers for different candidates are not trusted (Mixed)
    KV prev=load(L"result.txt");if(!prev.empty()&&std::fabs(num(prev,"conv")-res.conv)<1e-6&&std::fabs(num(prev,"vertical")-res.vertical)<1e-3){for(const char* k:{"flick","recoil","ads"}){std::string s=k;if(!r.count(s+".state")){continue;}int st=aim::consistent((int)num(prev,(s+".state").c_str(),aim::Short),num(prev,(s+".mult").c_str(),1),(int)num(r,(s+".state").c_str()),num(r,(s+".mult").c_str(),1));if(st==aim::Mixed){r[s+".state"]=std::to_string(st);}}}
    r["yaw"]=fixed(res.yaw,5);r["calibrated"]=res.calibrated?"1":"0";
    // at 1.00: the hit rate, the overshoot and the time to reach the target, for 자세히 보기
    double hit=0,over=0,reach=0;int n=0;for(auto& t:res.trials)if(t.kind==aim::Flick&&t.valid&&std::fabs(t.mult-1)<.001){n++;hit+=t.hit;over+=t.overshoot;reach+=t.reach;}
    if(n){r["flick.hit"]=fixed(hit/n*100,0);r["flick.over"]=fixed(over/n,1);r["flick.reach"]=fixed(reach/n,3);}save(L"result.txt",r);}
// ---- 배그 반동 분석 records (recoil_capture.h writes them; the page names the gun and the scope) -----------------------
struct Rec {long long at=0;std::string weapon,icon,scope;double vm=1,ratio=1,recoil=0,pull=0,drift=0,fire=0,kv=0;int bursts=0;};
std::vector<Rec> recs(){std::vector<Rec> v;std::istringstream in(readAll(dir()+L"\\recoil.tsv"));std::string l;
    while(std::getline(in,l)){std::vector<std::string> c;std::stringstream ss(l);std::string x;while(std::getline(ss,x,'\t'))c.push_back(x);if(c.size()<12)continue;
        Rec r;r.at=atoll(c[0].c_str());r.weapon=c[1];r.icon=c[2];r.scope=c[3];r.vm=atof(c[4].c_str());r.ratio=atof(c[5].c_str());r.recoil=atof(c[6].c_str());r.pull=atof(c[7].c_str());r.drift=atof(c[8].c_str());r.fire=atof(c[9].c_str());r.kv=atof(c[10].c_str());r.bursts=atoi(c[11].c_str());v.insert(v.begin(),r);}return v;} // newest first
void saveRecs(const std::vector<Rec>& v){std::string body;char b[512];for(auto it=v.rbegin();it!=v.rend();++it){snprintf(b,512,"%lld\t%s\t%s\t%s\t%.3f\t%.4f\t%.1f\t%.1f\t%.1f\t%.2f\t%.4f\t%d\n",it->at,it->weapon.c_str(),it->icon.c_str(),it->scope.c_str(),it->vm,it->ratio,it->recoil,it->pull,it->drift,it->fire,it->kv,it->bursts);body+=b;}
    std::wstring path=dir()+L"\\recoil.tsv",tmp=path+L".tmp";FILE* f=_wfopen(tmp.c_str(),L"wb");if(!f)return;bool ok=fwrite(body.data(),1,body.size(),f)==body.size();ok=fclose(f)==0&&ok;if(ok)MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING);}
// the scopes the player picks from (PUBG's sensitivity names: Targeting = aiming, Scoping = 1x, ScopeNX)
// The scopes a rifle or SMG can carry (6x at most; 8x and up are DMR/SR scopes, single shots, not spray analysis).
const int scopeCount=6;const char* scopeCodes[]={"t","1","2","3","4","6"};const wchar_t* scopeNames[]={L"조준",L"1배",L"2배",L"3배",L"4배",L"6배"};
int scopeIndex(const std::string& c){for(int i=0;i<scopeCount;i++)if(c==scopeCodes[i])return i;return -1;}
// the steady suggestion for a spray: the sprays with the same gun and scope up to it (an unknown gun stands alone)
recoil::Steady steadyFor(const std::vector<Rec>& all,const Rec& q){std::vector<double> t;for(auto& r:all){if(r.at>q.at)continue;if(r.at!=q.at&&(q.weapon.empty()||r.weapon!=q.weapon||r.scope!=q.scope))continue;for(int k=0;k<std::clamp(r.bursts,1,5);k++)t.push_back(r.vm*r.ratio);}return recoil::steady(t);}
double scopeSlider(const aim::Sens& s,const std::string& c);
void recoilBadge(long long at,const std::string& weapon,double vm,double ratio,std::wstring& title,std::wstring& line){
    // the scope this gun was last fired with (the player picks it on the results once; the badge reuses it)
    auto all=recs();Rec me;me.at=at;me.weapon=weapon;me.vm=vm;me.ratio=ratio;
    for(auto& q:all)if(q.at==at&&!q.scope.empty())me.scope=q.scope; /* found by its picture */
    if(me.scope.empty()&&!weapon.empty())for(auto& q:all)if(q.at!=at&&q.weapon==weapon&&!q.scope.empty()){me.scope=q.scope;break;}
    recoil::Steady sd=steadyFor(all,me);if(sd.n==0){sd.value=vm*ratio;sd.n=1;}
    wchar_t a[16],b2[16];swprintf(a,16,L"%.2f",vm);swprintf(b2,16,L"%.2f",sd.value);std::wstring w16name;for(char c:weapon)w16name+=(wchar_t)(unsigned char)c;
    std::wstring gun=weapon.empty()?std::wstring(i18n::text(L"총을 알아보지 못했습니다")):fmt(L"%ls 인식",w16name.c_str());
    std::wstring l2=sd.n>=3?fmt(L"최근 %d탄창 중간값 · 차이 ±%d%%",sd.n,(int)std::lround(sd.spread*100))+(sd.n<5?fmt(L"%d/5탄창 측정",sd.n).insert(0,L" · "):std::wstring()):fmt(L"%d/5탄창 측정 · 같은 총·부착물로 5탄창을 쏘면 추천이 정확해집니다",sd.n);
    int si=scopeIndex(me.scope);double cur=si>=0?scopeSlider(recoilcap::pubgSens(),me.scope):0;
    if(cur>0){wchar_t c1[16],c2[16];swprintf(c1,16,L"%.0f",cur);swprintf(c2,16,L"%.0f",aim::sliderFor(cur,sd.value/std::max(.01,vm)));l2+=L"  ·  "+fmt(L"또는 %ls 감도 %ls → %ls",i18n::text(scopeNames[si]),c1,c2);}
    else if(weapon.empty())l2+=L"  ·  "+std::wstring(i18n::text(L"에임랩에서 총을 한 번 고르면 다음부터 알아봅니다"));
    title=gun+L"  ·  "+fmt(L"수직 감도 %ls → %ls 추천",a,b2);line=l2;
    // the card: 수직 감도 and the scope's sensitivity side by side (the scope found by its picture, or the gun's last one)
    {auto& c=recoilcap::card;c=recoilcap::Card();c.on=true;c.gun=weapon.empty()?std::wstring(i18n::text(L"총을 알아보지 못했습니다")):w16name;c.count=fmt(L"%d/5탄창 측정",std::min(sd.n,5));
     c.vFrom=a;c.vTo=b2;int dv=(int)std::lround((sd.value/std::max(.01,vm)-1)*100);c.vDelta=dv?fmt(dv>0?L"+%d%%":L"%d%%",dv):std::wstring(i18n::text(L"그대로"));
     if(cur>0){c.sLabel=fmt(L"%ls 감도",i18n::text(scopeNames[si]));wchar_t c1[16],c2[16];swprintf(c1,16,L"%.0f",cur);double to=aim::sliderFor(cur,sd.value/std::max(.01,vm));swprintf(c2,16,L"%.0f",to);c.sFrom=c1;c.sTo=c2;int ds=(int)std::lround(to-cur);c.sDelta=ds?fmt(ds>0?L"+%d":L"%d",ds):std::wstring(i18n::text(L"그대로"));}
     else{c.sLabel=i18n::text(L"스코프 감도");c.sFrom=i18n::text(L"스코프를 고르면 표시");}
     c.hint=i18n::text(L"둘 중 하나만 바꾸세요 · 스코프 감도는 가로도 함께 바뀝니다");}
}
const bool recoilBadgeHook=(recoilcap::describe=recoilBadge,true);
double scopeSlider(const aim::Sens& s,const std::string& c){if(c=="t")return s.targeting;if(c=="1")return s.scoping;std::string want="Scope"+c+"X";for(auto& sc:s.scopes)if(sc.first==want)return sc.second;return 0;}
// 3x and 4x are for spraying by default, 6x and 8x for single shots (the player changes it in 반동 분석 설정)
bool autoFire(const std::string& c){std::string v=str(load(L"profile.txt"),("auto."+c).c_str());if(!v.empty())return v=="1";return c!="6"&&c!="8";}
const char* weaponNames[]={"Beryl M762","AKM","M416","SCAR-L","AUG","ACE32","G36C","QBZ","K2","Groza","M16A4","Mk47 Mutant","FAMAS","Vector","UMP45","Tommy Gun","MP5K","Micro UZI","P90","JS9","PP-19 Bizon","MP9","Mini14","SKS","SLR","Mk12","QBU","Mk14","VSS","Dragunov","DP-28","M249","MG3"};
struct Trace {std::vector<recoil::Pt> aim,hand,gun,laser;std::vector<recoil::Burst> each;double hComp=-9;};
Trace trace(long long at){Trace t;std::istringstream in(readAll(dir()+L"\\recoil\\"+std::to_wstring(at)+L".txt"));std::string l;
    while(std::getline(in,l)){auto e=l.find('=');if(e==std::string::npos)continue;std::string k=l.substr(0,e);if(k=="hcomp"){t.hComp=atof(l.c_str()+e+1);continue;}if(k=="b"){recoil::Burst b;double f[10]={0,0,0,0,0,0,0,0,0,-1};int i=0;std::stringstream bs(l.substr(e+1));std::string x;while(i<10&&std::getline(bs,x,','))f[i++]=atof(x.c_str());if(i>=8){b.shots=(int)f[8];b.breakAt=(int)f[9];b.ratio=f[0];b.vErr=f[1];b.hErr=f[2];b.wrong=f[3];b.react=f[4];for(int q=0;q<3;q++)b.phase[q]=f[5+q];t.each.push_back(b);}continue;}
        std::vector<recoil::Pt>* v=k=="aim"?&t.aim:k=="hand"?&t.hand:k=="gun"?&t.gun:k=="laser"?&t.laser:k=="ba"&&!t.each.empty()?&t.each.back().aim:k=="bs"&&!t.each.empty()?&t.each.back().shot:nullptr;if(!v)continue;
        std::stringstream ss(l.substr(e+1));std::string pt;while(std::getline(ss,pt,';')){auto c=pt.find(',');if(c!=std::string::npos)v->push_back({(float)atof(pt.c_str()),(float)atof(pt.c_str()+c+1)});}}return t;}
long long pickedAt=0;int homeTab=0; // the record shown on 반동 분석 결과; the home card's tab (0 PUBG recoil, 1 practice screen)
const unsigned recoilKeys[]={VK_INSERT,VK_HOME,VK_END,VK_F8,VK_F9,VK_F10};
// ---- the page ------------------------------------------------------------------------------------------------------
// Views: 0 home, 1 step 1 (PUBG nickname), 2 step 2 (mouse DPI), 3 details, 4 pro settings, 6 a saved account: keep it or sign out.
// The first visit walks 1 -> 2 -> home (onboarding); later visits start at 6 when an account is saved.
std::string pickShard="steam";bool onboarding=false;
const int dpiPresets[]={400,800,1600,3200};
// A new label on an owner-drawn button, then a full repaint (the button's own redraw on WM_SETTEXT left a band of the old picture).
void label(int id,const wchar_t* t){HWND c=GetDlgItem(page,id);wchar_t was[256]={};GetWindowTextW(c,was,256);if(!wcscmp(was,t)){return;}SetWindowTextW(c,t);InvalidateRect(c,nullptr,FALSE);}
void show(int id,bool on){if(HWND c=GetDlgItem(page,id))ShowWindow(c,on?SW_SHOW:SW_HIDE);}
// The two inputs sit inside a box the page draws (fieldBox), centred, so the text is in the middle of the box.
RECT fieldBox(int id){return id==513?RECT{224,308,628,360}:RECT{224,220,628,272};}
void layout(){for(HWND c=GetWindow(page,GW_CHILD);c;c=GetWindow(c,GW_HWNDNEXT)){RECT b;GetWindowRect(c,&b);MapWindowPoints(nullptr,page,(POINT*)&b,2);if(b.top<540){SetPropW(c,L"BlueAsh.OnRow",(HANDLE)1);}else{RemovePropW(c,L"BlueAsh.OnRow");}} /* buttons on a card: corners in the card's colour */
    bool measuring=aimprac::active();KV r=load(L"result.txt"),ap=load(L"applied.txt");bool advice=hasAdvice(r)&&num(ap,"at")<num(r,"at");
    
    label((501),i18n::text(measuring?L"분석 종료":L"감도 분석 시작"));SetWindowLongPtrW(GetDlgItem(page,501),GWLP_USERDATA,advice&&!measuring?0:1);InvalidateRect(GetDlgItem(page,501),nullptr,FALSE);
    for(int id:{501,505,506}){show(id,view==0);}show(502,view==0&&advice&&!measuring);
    for(int id:{511,512,513,514}){show(id,view==1);}show(518,view==1&&!linked());for(int id:{521,522,523,524,525,526}){show(id,view==2);}for(int id:{516,517}){show(id,view==6);}
    for(int id:{532,533,534,535,571}){show(id,view==0);}show(572,view==11);label(572,i18n::text(livecap::enabled()?L"자동 기록 끄기":L"자동 기록 켜기"));show(549,view==7&&!recs().empty());show(570,view==7&&!recs().empty());show(501,false);show(502,false); /* the old sensitivity run is no longer offered */
    {bool any=!recs().empty();for(int id=540;id<=548;id++)show(id,view==7&&any);}for(int id=550;id<=555;id++){show(id,view==8);}for(int id=560;id<560+scopeCount;id++){show(id,view==8);}
    Profile p=profile();for(int id:{527,528})show(id,view==2&&!p.fromIni);
    label(526,i18n::text(onboarding?L"다음":L"저장"));label(505,i18n::text(linked()?L"계정 변경":L"계정 연결"));
    // the bottom row: 프로게이머 설정 / 자세히 보기 at home, ‹ 뒤로 elsewhere; nothing during the first steps but a way back to step 1
    label((504),i18n::text(view==0?L"프로게이머 설정":L"‹  뒤로"));show(504,view==0||view==3||view==4||(view==1&&!onboarding)||view==2||view==7||view==8||view==9||view==10||view==11);
    label((503),i18n::text(view==0?L"자세히 보기":view==7?L"설정":L"전적 새로고침"));show(503,view==0||(view==3&&linked())||view==7);InvalidateRect(GetDlgItem(page,503),nullptr,FALSE);InvalidateRect(page,nullptr,FALSE);}
void setView(int v){view=v;note.clear();if(v==1){KV a=account();SetWindowTextW(GetDlgItem(page,513),w16(str(a,"name")).c_str());pickShard=str(a,"shard").empty()?"steam":str(a,"shard");SetFocus(GetDlgItem(page,513));}
    if(v==2){Profile p=profile();SetWindowTextW(GetDlgItem(page,521),p.dpi?std::to_wstring(p.dpi).c_str():L"");label((527),p.normal>0?wfixed(p.normal,2).c_str():L"");label((528),wfixed(p.vertical,2).c_str());SetFocus(GetDlgItem(page,521));}
    if(v==7){auto all=recs();const Rec* q=nullptr;for(auto& r:all)if(r.at==pickedAt)q=&r;if(!q&&!all.empty()){pickedAt=all[0].at;q=&all[0];}
        if(q){int wi=0;for(int i=0;i<(int)(sizeof(weaponNames)/sizeof(*weaponNames));i++)if(q->weapon==weaponNames[i])wi=i+1;SendMessageW(GetDlgItem(page,547),CB_SETCURSEL,wi,0);}}
    if(v==8){for(int i=0;i<scopeCount;i++)checkBox(page,560+i,autoFire(scopeCodes[i]));}
    layout();}
// where the page goes once the account is settled: the DPI step when it is missing, else home
void afterAccount(){if(profile().dpi<=0){onboarding=true;setView(2);}else{onboarding=false;setView(0);}}
void setNote(const std::wstring& s,COLORREF c);
void startAnalysis(){if(aimprac::active()){aimprac::stop();return;}
    KV c=load(L"consent.txt");if(c["ok"]!="1"){if(MessageBoxW(win,i18n::text(L"감도 분석은 BlueAsh의 연습 화면에서 마우스 이동량(Raw Input)만 측정합니다.\n\n· 키보드 입력은 읽지 않습니다.\n· 화면을 캡처하지 않고, 측정한 값은 이 PC에만 저장합니다. 어디에도 업로드하지 않습니다.\n· 배그 게임 안에서는 측정하지 않습니다.\n\n시작할까요?"),i18n::text(L"에임랩"),MB_YESNO|MB_ICONINFORMATION)!=IDYES)return;c["ok"]="1";c["at"]=std::to_string((long long)time(nullptr));save(L"consent.txt",c);}
    Profile p=profile();remember(p,"seen");aimprac::Options o;o.conv=p.conv;o.vertical=p.vertical;o.adsConv=p.adsConv;o.radius=radiusFor(p.distance);o.yaw=p.yaw;o.key=p.key;o.ads=p.ads;o.debug=p.debug;
#ifdef BLUEASH_TT
    o.debug=o.debug||getenv("BLUEASH_AIM_DEBUG");o.ads=o.ads||getenv("BLUEASH_AIM_ADS");
#endif
    aimprac::onDone=storeResult;if(!aimprac::open(page,o))setNote(i18n::text(L"연습 화면을 열지 못했습니다."),danger);layout();}
void setNote(const std::wstring& s,COLORREF c){note=s;noteColor=c;if(page)InvalidateRect(page,nullptr,FALSE);}
void card(HDC dc,RECT r){roundBox(dc,r,rowFill,line,18);}
// The two changes on the home page: 명중률 (matches whose hits could be trusted), and damage per 100 shots.
std::wstring trendWord(int t){return i18n::text(t==aim::Up?L"상승":t==aim::Down?L"하락":L"불명확");}
aim::Change accChange(const std::vector<aim::Match>& b,const std::vector<aim::Match>& a,int cls=-1){std::vector<double> x,y;
    auto rate=[&](const aim::Match& m)->double{if(!m.hitsOk)return -1;if(cls<0)return aim::accuracy(m);return m.classFired[cls]>=10?100.0*m.classHits[cls]/m.classFired[cls]:-1;};
    for(auto& m:b){double r=rate(m);if(r>=0)x.push_back(r);}for(auto& m:a){double r=rate(m);if(r>=0)y.push_back(r);}return aim::compare(x,y);}
aim::Change dmgChange(const std::vector<aim::Match>& b,const std::vector<aim::Match>& a,int cls=-1){std::vector<double> x,y;
    auto per=[&](const aim::Match& m)->double{if(cls<0)return aim::per100(m);return m.classFired[cls]>=10?m.classDamage[cls]/m.classFired[cls]*100:-1;};
    for(auto& m:b){double r=per(m);if(r>=0)x.push_back(r);}for(auto& m:a){double r=per(m);if(r>=0)y.push_back(r);}return aim::compare(x,y);}
std::wstring accText(const aim::Change& c){return fmt(L"명중률 %ls%% → %ls%% (%ls%ls%%p · %ls%ls%%) · %ls",wfixed(c.before.mean,1).c_str(),wfixed(c.after.mean,1).c_str(),c.diff>=0?L"+":L"",wfixed(c.diff,1).c_str(),c.rel>=0?L"+":L"",wfixed(c.rel,0).c_str(),trendWord(c.trend).c_str());}
std::wstring dmgText(const aim::Change& c){return fmt(L"100발당 피해 %ls → %ls (%ls%ls · %ls%ls%%) · %ls",wfixed(c.before.mean,1).c_str(),wfixed(c.after.mean,1).c_str(),c.diff>=0?L"+":L"",wfixed(c.diff,1).c_str(),c.rel>=0?L"+":L"",wfixed(c.rel,0).c_str(),trendWord(c.trend).c_str());}
std::vector<recoil::Pt> homeSpray; /* the latest capture's gun pattern, set while the still picture is drawn */
void paintHome(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;Profile p=profile();KV a=account(),r=load(L"result.txt"),ap=load(L"applied.txt");
    // account
    card(dc,{36,20,420,132});{std::wstring nm=w16(str(a,"name"));RECT av={52,48,100,96};roundBox(dc,av,linked()?RGB(52,92,255):rowFill,linked()?RGB(52,92,255):line,24);textAt(dc,linked()?nm.substr(0,1):L"?",av,boldFont,linked()?RGB(255,255,255):muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}textAt(dc,i18n::text(L"PUBG 계정"),{116,30,400,52},smallFont,muted,one);
    if(linked()){textAt(dc,w16(str(a,"name")),{116,54,250,82},boldFont,ink,one);bool ready=!str(a,"id").empty();textAt(dc,(ready?L"\x2713 "+std::wstring(i18n::text(L"전적 연결 완료")):std::wstring(i18n::text(L"전적 서버 준비 중")))+L" · "+(str(a,"shard")=="kakao"?L"Kakao":L"Steam"),{116,86,256,110},smallFont,ready?RGB(96,210,150):RGB(240,180,90),one);}
    else{textAt(dc,i18n::text(L"연결 안 됨"),{116,54,250,82},boldFont,ink,one);textAt(dc,i18n::text(L"연결은 선택 사항입니다"),{116,86,256,110},smallFont,muted,one);}
    // sensitivity
    card(dc,{432,20,816,132});textAt(dc,i18n::text(L"감도 프로필"),{452,30,800,52},smallFont,muted,one);
    if(p.conv>0){textAt(dc,fmt(L"일반 %ls · 수직 %ls",wfixed(p.normal,1).c_str(),wfixed(p.vertical,2).c_str()),{452,54,648,82},boldFont,ink,one);double cm=cmOf(p);int t=typeOf(p);
        textAt(dc,cm>0?fmt(L"%d DPI · %ls cm/360 · %ls",p.dpi,wfixed(cm,1).c_str(),typeName(t)):std::wstring(i18n::text(L"DPI를 넣으면 cm/360을 계산합니다")),{452,86,648,110},smallFont,muted,one);}
    else{textAt(dc,i18n::text(L"배그 감도를 찾지 못했습니다"),{452,54,648,82},boldFont,ink,one);textAt(dc,i18n::text(L"감도 프로필에서 직접 넣어 주세요"),{452,86,648,110},smallFont,muted,one);}
    // the analysis: the reticle (its moving parts are paintMotion) on the left, the words and the actions on the right
    card(dc,{36,146,816,392});painting::glow(dc,{56,170,256,366},RGB(52,92,255),34);roundBox(dc,{72,182,240,354},RGB(12,20,42),RGB(30,48,92),20);
    {static HBRUSH ax=CreateSolidBrush(RGB(26,40,76));painting::hairline(dc,{84,268,228,269},ax);painting::hairline(dc,{155,194,156,342},ax);}
    const int x0=280,x1=796;
    // 배그 반동 분석: the latest capture's spray pattern draws itself inside the reticle (paintMotion); words and actions on the right
    textAt(dc,i18n::text(L"배그 반동 분석"),{x0,166,552,198},sectionFont,ink,one);
    textAt(dc,fmt(L"배그 훈련장에서 %ls를 누르고 5탄창을 쏘세요 · 탄창마다 따로 계산합니다",keyName(recoilcap::key()).c_str()),{x0,204,x1,232},smallFont,muted,one);
    {auto all=recs();homeSpray.clear();if(!all.empty()){const Rec& q=all[0];int si=scopeIndex(q.scope);std::wstring nm=q.weapon.empty()?std::wstring(i18n::text(L"모르는 총")):w16(q.weapon);homeSpray=trace(q.at).gun;
        recoil::Steady sd=steadyFor(all,q);double to=sd.n?sd.value:q.vm*q.ratio;
        textAt(dc,nm+(si>=0?L" · "+std::wstring(i18n::text(scopeNames[si])):L"")+L" · "+i18n::text(L"수직 감도 추천"),{x0,240,x1,260},captionFont,muted,one);
        std::wstring a=wfixed(q.vm,2),b=wfixed(to,2);SIZE za={},zr={};HGDIOBJ of=SelectObject(dc,sectionFont);GetTextExtentPoint32W(dc,a.c_str(),(int)a.size(),&za);GetTextExtentPoint32W(dc,L"\x2192",1,&zr);SelectObject(dc,of);
        textAt(dc,a,{x0,262,x0+4+za.cx,296},sectionFont,RGB(200,210,232),one);textAt(dc,L"\x2192",{x0+10+za.cx,262,x0+14+za.cx+zr.cx,296},sectionFont,RGB(100,120,165),one);textAt(dc,b,{x0+20+za.cx+zr.cx,262,x1,296},sectionFont,RGB(130,175,255),one);}
     else textAt(dc,i18n::text(L"아직 측정이 없습니다"),{x0,250,x1,290},boldFont,RGB(140,176,255),one);}
    // matches
    card(dc,{36,406,816,540});{static HBRUSH sep=CreateSolidBrush(RGB(30,46,84));painting::hairline(dc,{424,420,425,526},sep);}
    // left: 반응 속도 테스트 - the bests, one per line under one label
    painting::glow(dc,{44,420,156,532},RGB(52,92,255),40);painting::circle(dc,{52,428,148,524},CLR_NONE,RGB(36,58,110),1.3f);painting::circle(dc,{76,452,124,500},CLR_NONE,RGB(30,48,92),1.f);
    const int rx=170;textAt(dc,i18n::text(L"반응 속도 테스트"),{rx,414,410,438},boldFont,ink,one);
    {KV best=load(L"reaction.txt");double r1=num(best,"best1"),r2=num(best,"best2");std::wstring lab=i18n::text(L"최고 기록");SIZE z={};HGDIOBJ of=SelectObject(dc,captionFont);GetTextExtentPoint32W(dc,lab.c_str(),(int)lab.size(),&z);SelectObject(dc,of);int vx=rx+z.cx+12;
     if(r1>0||r2>0){textAt(dc,lab,{rx,440,vx,458},captionFont,muted,one);textAt(dc,r1>0?fmt(L"반응 속도 %d ms",(int)std::lround(r1)):std::wstring(i18n::text(L"반응 속도"))+L"  —",{vx,438,410,458},smallBold,RGB(140,176,255),one);
        textAt(dc,r2>0?fmt(L"표적 반응 %d ms",(int)std::lround(r2)):std::wstring(i18n::text(L"표적 반응"))+L"  —",{vx,458,410,478},smallBold,RGB(140,176,255),one);}
     else textAt(dc,i18n::text(L"화면이 바뀌는 순간 클릭하는 반응 속도와, 나타나는 표적을 맞히는 표적 반응을 재미로 재 보세요."),{rx,440,410,478},captionFont,muted,DT_LEFT|DT_WORDBREAK);}
    // right: the matches before and after the applied change
    textAt(dc,i18n::text(L"최근 경기 분석"),{440,416,796,440},boldFont,ink,one);
    if(!linked())textAt(dc,i18n::text(L"계정을 연결하면 감도를 바꾸기 전후 경기를 자동으로 비교합니다."),{440,442,796,486},smallFont,muted,DT_LEFT|DT_WORDBREAK);
    else{auto all=matches();int valid=0;for(auto& m:all)valid+=m.valid;double at=num(ap,"at");std::vector<aim::Match> b,af;if(at>0){aim::split(all,at,10,b,af);}
        std::wstring status=refreshing?fmt(L"경기를 불러오는 중 · %d / %d",refreshDone,refreshTotal):at>0?fmt(L"감도 적용 후 %d / 10경기 완료 · 적용 전 %d경기",(int)af.size(),(int)b.size()):fmt(L"유효 경기 %d개 (대상 무기 %d발 이상)",valid,p.minShots);
        textAt(dc,status,{440,442,796,464},smallFont,refreshing?RGB(140,176,255):muted,one);
        if(at<=0){textAt(dc,i18n::text(L"반동 분석 결과에서 감도를 적용하면 그 시점 전후 경기를 비교합니다."),{440,466,796,512},smallFont,muted,DT_LEFT|DT_WORDBREAK);}
        else{auto acc=accChange(b,af),dmg=dmgChange(b,af);
            textAt(dc,acc.trend==aim::Few?std::wstring(i18n::text(L"명중률 · 비교하려면 양쪽 3경기 이상이 필요합니다")):accText(acc),{440,468,796,490},smallFont,acc.trend==aim::Few?muted:ink,one);
            textAt(dc,dmg.trend==aim::Few?std::wstring(i18n::text(L"피해량 · 비교하려면 양쪽 3경기 이상이 필요합니다")):dmgText(dmg),{440,494,796,516},smallFont,dmg.trend==aim::Few?muted:ink,one);}}
    if(!note.empty()){textAt(dc,note,{232,550,624,600},smallFont,noteColor?noteColor:muted,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}}
// The steps at the top of the first visit: 1 PUBG 계정 · 2 마우스 DPI · 3 감도 분석 (done ones cobalt with a check).
void stepper(HDC dc,int at){const wchar_t* names[]={L"PUBG 계정",L"마우스 DPI",L"감도 분석"};static HBRUSH done=CreateSolidBrush(RGB(52,92,255)),todo=CreateSolidBrush(RGB(36,54,96));
    for(int i=0;i<3;i++){int cx=286+i*140;if(i<2){painting::hairline(dc,{cx+20,39,cx+120,41},i<at?done:todo);}
        RECT c={cx-15,25,cx+15,55};bool past=i<at,now=i==at;roundBox(dc,c,past||now?RGB(52,92,255):rowFill,past||now?RGB(52,92,255):line,15);
        textAt(dc,past?L"\x2713":std::to_wstring(i+1),c,boldFont,past||now?RGB(255,255,255):muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        textAt(dc,i18n::text(names[i]),{cx-70,60,cx+70,82},smallFont,now?ink:muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}}
void field(HDC dc,int id){RECT b=fieldBox(id);bool focus=GetFocus()==GetDlgItem(page,id);roundBox(dc,b,well,focus?RGB(52,92,255):line,12);}
void heading(HDC dc,const wchar_t* title,const wchar_t* sub){textAt(dc,i18n::text(title),{36,100,816,134},sectionFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE);textAt(dc,i18n::text(sub),{36,136,816,160},smallFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
void noteLine(HDC dc){if(!note.empty())textAt(dc,note,{136,508,716,548},smallFont,noteColor?noteColor:muted,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}
// The page's moving parts, drawn over the still picture (WM_PAINT keeps that in a cache, so a frame only redraws motionRect):
// home - a target pops up in the reticle, the aim slides to it and a ring bursts; the steps - the current one sends out a
// soft ring; a saved account - the same ring around the initial.
RECT motionRect(){if(view==0)return {44,166,256,532};if(view==1||view==2){int cx=286+(view-1)*140;return {cx-32,8,cx+32,72};}if(view==6)return {370,176,482,288};return {0,0,0,0};}
double ease3(double u){u=std::clamp(u,0.0,1.0);return 1-(1-u)*(1-u)*(1-u);}
void ringPulse(Gdiplus::Graphics& g,float x,float y,float r0,float grow,double t,double period){double u=std::fmod(t,period)/period;float rr=r0+grow*(float)ease3(u);Gdiplus::Pen pen(Gdiplus::Color((BYTE)(150*(1-u)),82,124,255),2.f);g.DrawEllipse(&pen,x-rr,y-rr,rr*2,rr*2);}
void paintMotion(HDC dc){if(!painting::token)return;double t=GetTickCount64()/1000.0;Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    if(view==1||view==2){ringPulse(g,286.f+(view-1)*140,40.f,15.f,12.f,t,1.6);return;}
    if(view==6){ringPulse(g,426.f,232.f,34.f,16.f,t,2.0);return;}
    if(view!=0)return;
    {const RECT b={84,194,228,342};std::vector<recoil::Pt> v=homeSpray;if(v.size()<3){for(int i=0;i<30;i++)v.push_back({(float)(6*std::sin(i*.5)+i*.4),(float)(-i*3.2)});}
        float x0=0,x1=0,y0=0,y1=0;for(auto& q:v){x0=std::min(x0,q.x);x1=std::max(x1,q.x);y0=std::min(y0,q.y);y1=std::max(y1,q.y);}float w=std::max(8.f,x1-x0),hh=std::max(8.f,y1-y0),sc=std::min((b.right-b.left-24)/w,(b.bottom-b.top-16)/hh);
        float ox=(b.left+b.right)/2.f-(x0+x1)/2*sc,oy=(b.top+b.bottom)/2.f-(y0+y1)/2*sc;double cyc=3.0,u=std::fmod(t,cyc);double grow=std::min(1.0,u/2.0),fade=u>2.4?1-(u-2.4)/.6:1;size_t n=std::max<size_t>(2,(size_t)(v.size()*ease3(grow)));
        std::vector<Gdiplus::PointF> q;for(size_t i=0;i<n&&i<v.size();i++)q.push_back({ox+v[i].x*sc,oy+v[i].y*sc});
        Gdiplus::Pen glow(Gdiplus::Color((BYTE)(55*fade),96,210,150),6.f),line(Gdiplus::Color((BYTE)(235*fade),110,220,160),2.f);for(auto* pp:{&glow,&line}){painting::roundCaps(*pp);pp->SetLineJoin(Gdiplus::LineJoinRound);}
        if(q.size()>=2){g.DrawLines(&glow,q.data(),(INT)q.size());g.DrawLines(&line,q.data(),(INT)q.size());}
        if(!q.empty()){auto h=q.back();Gdiplus::SolidBrush halo(Gdiplus::Color((BYTE)(90*fade),110,220,160)),dot(Gdiplus::Color((BYTE)(255*fade),235,255,244));g.FillEllipse(&halo,h.X-6,h.Y-6,12.f,12.f);g.FillEllipse(&dot,h.X-2.5f,h.Y-2.5f,5.f,5.f);}}
    // 반응 속도 테스트's reticle (bottom left): two arcs turning, a target pops up, the aim slides to it, a ring bursts
    const float cx=100,cy=476,sc=.62f;bool fast=aimprac::active();double spin=t*(fast?120:40);
    Gdiplus::Pen arc(Gdiplus::Color(200,72,118,255),2.f);painting::roundCaps(arc);for(int i=0;i<2;i++){g.DrawArc(&arc,cx-37,cy-37,74.f,74.f,(float)(spin+i*180),70.f);}
    const float pts[5][2]={{-36,-22},{30,-34},{40,18},{-18,36},{-44,6}};const double cycle=2.2;int i=(int)std::floor(t/cycle)%5,prev=(i+4)%5;double s=std::fmod(t,cycle);
    float tx=cx+pts[i][0]*sc,ty=cy+pts[i][1]*sc,px=cx+pts[prev][0]*sc,py=cy+pts[prev][1]*sc;
    if(s<1.05){float k=s<.22?(float)(1+2.7*std::pow(s/.22-1,3)+1.7*std::pow(s/.22-1,2)):1.f;float a=s>.85?(float)(1-(s-.85)/.2):1.f;float rr=5.f*std::max(.05f,k);
        Gdiplus::SolidBrush tb(Gdiplus::Color((BYTE)(255*a),72,118,255));g.FillEllipse(&tb,tx-rr,ty-rr,rr*2,rr*2);Gdiplus::Pen te(Gdiplus::Color((BYTE)(220*a),190,212,255),1.3f);g.DrawEllipse(&te,tx-rr,ty-rr,rr*2,rr*2);}
    if(s>=.8&&s<1.3){double u=(s-.8)/.5;float rr=4+13*(float)ease3(u);Gdiplus::Pen burst(Gdiplus::Color((BYTE)(220*(1-u)*(1-u)),140,176,255),1.6f);g.DrawEllipse(&burst,tx-rr,ty-rr,rr*2,rr*2);}
    double m=ease3((s-.4)/.38);float ax=px+(tx-px)*(float)m,ay=py+(ty-py)*(float)m,kk=6.5f;
    Gdiplus::Pen aim(Gdiplus::Color(255,247,249,255),1.5f);painting::roundCaps(aim);g.DrawLine(&aim,ax-kk,ay,ax-kk/3,ay);g.DrawLine(&aim,ax+kk/3,ay,ax+kk,ay);g.DrawLine(&aim,ax,ay-kk,ax,ay-kk/3);g.DrawLine(&aim,ax,ay+kk/3,ax,ay+kk);
}
HDC stillDC=nullptr;HBITMAP stillBmp=nullptr;int stillW=0,stillH=0,stillView=-1;bool stillOk=false;
// 손잡이 추천, from the player's own spray: what the hand left uncontrolled (the crosshair's sideways spread and the
// vertical drift, against the gun's climb). Effects as PUBG's patch notes state them: Vertical Foregrip vertical recoil
// control +15% (Update 20.1); Half Grip horizontal +16% (41.1, was 8), vertical +8%, recoil recovery +10% (20.1); Tilted
// Grip vertical +12%, horizontal +6%, camera shake while firing +25% (41.1, April 2026; the Angled Foregrip was removed
// then). Muzzles and stocks are not suggested: no current official numbers were found. Shown as a hint, not an answer.
struct Grip {const wchar_t* name;const wchar_t* effect;const wchar_t* why;int tile=-1;double side=0,drift=0,fit[4]={0,0,0,0};}; // tile: 0 vertical, 1 half, 2 tilted, -1 keep
// How well each grip answers what the hand left (0..1): Vertical Foregrip the vertical drift, Half Grip the sideways
// spread, Tilted Grip both together. The pick is the best one when it reaches 0.4; single shots take the Half Grip.
Grip gripFor(const Trace& t,bool single){float gy0=0,gy1=0,ax0=0,ax1=0;for(auto& p:t.gun){gy0=std::min(gy0,p.y);gy1=std::max(gy1,p.y);}for(auto& p:t.aim){ax0=std::min(ax0,p.x);ax1=std::max(ax1,p.x);}
    double climb=std::max(1.f,gy1-gy0),side=(ax1-ax0)/climb,drift=t.aim.empty()?0:std::fabs(t.aim.back().y)/climb,v=std::min(1.0,drift/.3),hz=std::min(1.0,side/.3);
    Grip g{L"지금 손잡이 유지",L"남은 흔들림이 작습니다",L"지금도 잘 제어하고 있습니다",-1,side,drift};g.fit[0]=v*(1-.6*hz);g.fit[1]=hz*(1-.3*v);g.fit[2]=std::sqrt(v*hz);
    int best=0;for(int i=1;i<3;i++)if(g.fit[i]>g.fit[best])best=i;
    if(single){g.fit[1]=std::max(g.fit[1],.8);best=1;}else if(g.fit[best]<.4)return g;
    const wchar_t* names[]={L"수직 손잡이",L"하프 그립",L"틸티드 그립"};const wchar_t* effects[]={L"세로 반동 +15%",L"가로 반동 +16% · 세로 +8% · 반동 회복 +10%",L"세로 반동 +12% · 가로 +6% · 화면 흔들림 +25%"};
    const wchar_t* whys[]={L"위아래로 밀린 폭이 큼",single?L"단발은 다음 탄 전 회복이 중요":L"좌우로 흔들린 폭이 큼",L"위아래·좌우가 함께 남음"};
    g.name=names[best];g.effect=effects[best];g.why=whys[best];g.tile=best;return g;}
// 부착물 추천 (view 9): four grips and two muzzle options, drawn as matching transparent outlines.
// Only the three grips with checked figures are scored; the other options never imply a recommendation.
struct GripTile {const wchar_t* name;double vert,horiz,recover,shake;};
const GripTile gripTiles[]={{L"수직 손잡이",15,0,0,0},{L"하프 그립",8,16,10,0},{L"틸티드 그립",12,6,0,25}};
// Every tile's figures (namu.wiki "PUBG: BATTLEGROUNDS/총기 부착물", edit of 2026-10-18, assault rifle figures): good =
// helps (a "-" on the pattern or the spread is a reduction, so good too); mag = the size for the bar (0 = words only).
struct AttStat {const wchar_t* label;const wchar_t* text;double mag;bool good;};
struct Att {const wchar_t* name;const wchar_t* about;int n;AttStat st[4];double vert,horiz;};
const Att atts[]={
    {L"수직 손잡이",L"세로 반동을 가장 크게 줄입니다. 쏠 때 위로 크게 튀는 총에 맞습니다.",1,{{L"수직 반동 제어",L"+15%",15,true}},15,0},
    {L"하프 그립",L"가로 반동을 크게 줄이고 단발 반동 회복이 빨라집니다. 좌우로 흔들릴 때 맞습니다.",4,{{L"수평 반동 제어",L"+16%",16,true},{L"수직 반동 제어",L"+8%",8,true},{L"단발 반동 회복",L"+10%",10,true},{L"호흡 흔들림 제어",L"-10%",10,false}},8,16},
    {L"틸티드 그립",L"세로·가로 반동을 고르게 줄이고 사격 중 화면 흔들림을 잡아 줍니다.",3,{{L"수직 반동 제어",L"+12%",12,true},{L"수평 반동 제어",L"+6%",6,true},{L"사격 시 화면 흔들림 제어",L"+25%",25,true}},12,6},
    {L"엄지 손잡이",L"조준이 빨라지고 호흡 흔들림이 줄어듭니다. 반동은 세로만 조금 줄입니다.",4,{{L"조준 속도",L"+40%",40,true},{L"호흡 흔들림 제어",L"+20%",20,true},{L"수직 반동 제어",L"+10%",10,true},{L"단발 반동 회복",L"+10%",10,true}},10,0},
    {L"보정기",L"세로·가로 반동과 반동 패턴을 함께 줄입니다. 반동만 보면 총구 중 가장 강합니다.",4,{{L"반동 패턴",L"-25%",25,true},{L"수직 반동 제어",L"+15%",15,true},{L"수평 반동 제어",L"+10%",10,true},{L"탄 퍼짐",L"-10%",10,true}},15,10},
    {L"제동기",L"사격 중 화면 흔들림을 크게 줄입니다. 반동 감소는 보정기보다 작습니다.",3,{{L"사격 시 화면 흔들림 제어",L"+35%",35,true},{L"수직 반동 제어",L"+10%",10,true},{L"수평 반동 제어",L"+8%",8,true}},10,8},
    {L"소염기",L"총구 화염을 가리고 반동도 조금 줄입니다.",4,{{L"반동 패턴",L"-10%",10,true},{L"수직 반동 제어",L"+10%",10,true},{L"수평 반동 제어",L"+10%",10,true},{L"총구 화염",L"감소",0,true}},10,10}};
int attPick=-1; // the tile shown in the detail card (-1: the recommended grip)
// Grips: four tiles of 180 with 20 between; muzzles: three wider tiles filling the same width.
RECT attRect(int i){if(i<4){int x=36+i*200;return {x,96,x+180,208};}int x=36+(i-4)*267;return {x,250,x+246,362};}
// how well each muzzle answers the pattern: the vertical and sideways help weighted by what the hand left (best = 1)
void muzzleFit(double v,double hz,double out[3],int& best){double raw[3],top=0;for(int i=0;i<3;i++){raw[i]=v*atts[4+i].vert+hz*atts[4+i].horiz;top=std::max(top,raw[i]);}best=0;for(int i=0;i<3;i++){out[i]=top>0?raw[i]/top:0;if(raw[i]>raw[best])best=i;}if(v+hz<.1)best=-1;}
// Embedded PNGs are optional: keep the original vector drawings as a fallback.
bool attachmentImage(Gdiplus::Graphics& g,int kind,float x,float y,float k,BYTE alpha){
    if(kind<0||kind>=5){return false;}static Gdiplus::Bitmap* images[5]={};static IStream* streams[5]={};static bool tried[5]={};
    if(!tried[kind]){tried[kind]=true;HMODULE m=GetModuleHandleW(nullptr);HRSRC r=FindResourceW(m,MAKEINTRESOURCEW(451+kind),RT_RCDATA);if(r){DWORD size=SizeofResource(m,r);const void* data=LockResource(LoadResource(m,r));IStream* s=nullptr;if(data&&size&&SUCCEEDED(CreateStreamOnHGlobal(nullptr,TRUE,&s))){ULONG written=0;if(SUCCEEDED(s->Write(data,size,&written))&&written==size){LARGE_INTEGER z={};s->Seek(z,STREAM_SEEK_SET,nullptr);auto* b=new Gdiplus::Bitmap(s);if(b->GetLastStatus()==Gdiplus::Ok){images[kind]=b;streams[kind]=s;}else delete b;}if(!images[kind])s->Release();}}}
    if(!images[kind]){return false;}(void)streams;Gdiplus::ImageAttributes attrs;Gdiplus::ColorMatrix cm={{{1,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0},{0,0,0,alpha/255.f,0},{0,0,0,0,1}}};attrs.SetColorMatrix(&cm);g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.DrawImage(images[kind],Gdiplus::RectF(x,y,80*k,40*k),0,0,(float)images[kind]->GetWidth(),(float)images[kind]->GetHeight(),Gdiplus::UnitPixel,&attrs);return true;
}
// Each attachment drawn in an 80 x 40 box, scaled into place: smooth outlines with a faint fill, details as thin lines.
// The player's own drawings (source/assets/attachment-user-20261116, resources 461..467 in tile order: vertical, half,
// tilted, thumb grips, compensator, muzzle brake, flash hider), fitted into the 80 x 44 box; the pick tinted cobalt.
bool userArt(Gdiplus::Graphics& g,int kind,float x,float y,float k,BYTE alpha,bool lit){
    if(kind<0||kind>6){return false;}static Gdiplus::Bitmap* img[7]={};static bool tried[7]={};
    if(!tried[kind]){tried[kind]=true;HMODULE m=GetModuleHandleW(nullptr);HRSRC r=FindResourceW(m,MAKEINTRESOURCEW(461+kind),RT_RCDATA);DWORD size=r?SizeofResource(m,r):0;const void* data=r?LockResource(LoadResource(m,r)):nullptr;
        if(data&&size){HGLOBAL mem=GlobalAlloc(GMEM_MOVEABLE,size);void* p=mem?GlobalLock(mem):nullptr;if(p){memcpy(p,data,size);GlobalUnlock(mem);IStream* st=nullptr;if(SUCCEEDED(CreateStreamOnHGlobal(mem,TRUE,&st))){Gdiplus::Bitmap tmp(st);if(tmp.GetLastStatus()==Gdiplus::Ok){img[kind]=new Gdiplus::Bitmap(tmp.GetWidth(),tmp.GetHeight(),PixelFormat32bppPARGB);Gdiplus::Graphics c(img[kind]);c.DrawImage(&tmp,0,0,tmp.GetWidth(),tmp.GetHeight());}st->Release();}else GlobalFree(mem);}else if(mem)GlobalFree(mem);}} /* copied out of the stream: a GDI+ image keeps reading its source */
    Gdiplus::Bitmap* b=img[kind];if(!b){return false;}float bw=80*k,bh=44*k,sc=std::min(bw/b->GetWidth(),bh/b->GetHeight()),w=b->GetWidth()*sc,h=b->GetHeight()*sc;
    float a=alpha/255.f;Gdiplus::ColorMatrix cm={{{lit?.55f:1,0,0,0,0},{0,1,0,0,0},{0,0,lit?.72f:1,0,0},{0,0,0,a,0},{0,0,0,0,1}}};Gdiplus::ImageAttributes at;at.SetColorMatrix(&cm);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);g.DrawImage(b,Gdiplus::RectF(x+(bw-w)/2,y+(bh-h)/2,w,h),0,0,(Gdiplus::REAL)b->GetWidth(),(Gdiplus::REAL)b->GetHeight(),Gdiplus::UnitPixel,&at);return true;}
void attachment(Gdiplus::Graphics& g,int kind,float x,float y,float k,BYTE alpha,bool lit){
    if(userArt(g,kind,x,y,k,alpha,lit))return;
    if(attachmentart::draw(g,kind,x,y,k,alpha,lit))return;
    // Blueprint-style outlines: transparent interiors and consistent light strokes across all attachments.
    auto A=[&](double f){return (BYTE)std::clamp(alpha*f,0.0,255.0);};
    Gdiplus::Color rim(A(.95),lit?196:176,lit?214:192,lit?255:216),groove(A(.70),lit?166:145,lit?192:164,lit?244:188);
    Gdiplus::Pen pen(rim,std::max(1.f,1.1f*k/1.6f)),thin(groove,std::max(1.f,1.2f*k/1.6f));for(auto* pp:{&pen,&thin}){pp->SetLineJoin(Gdiplus::LineJoinRound);painting::roundCaps(*pp);}
    auto P=[&](float px,float py){return Gdiplus::PointF(x+px*k,y+py*k);};
    auto shape=[&](Gdiplus::GraphicsPath& gp){g.DrawPath(&pen,&gp);};
    auto rail=[&](float x0,float x1){Gdiplus::GraphicsPath r;r.AddLine(P(x0,6),P(x1,6));r.AddLine(P(x1,6),P(x1+2,8));r.AddLine(P(x1+2,8),P(x1+2,11));r.AddLine(P(x1+2,11),P(x0-2,11));r.AddLine(P(x0-2,11),P(x0-2,8));r.CloseFigure();shape(r);
        for(float u=x0+4;u<x1-1;u+=6){g.DrawLine(&thin,P(u,6),P(u,8.5f));g.DrawLine(&thin,P(u+3,6),P(u+3,8.5f));}};
    if(kind==0){rail(16,64);Gdiplus::GraphicsPath b;b.AddLine(P(33,11),P(33,14));b.AddBezier(P(33,14),P(33,16),P(34,17),P(34,19));b.AddLine(P(34,19),P(32.5f,33));b.AddBezier(P(32.5f,33),P(32,37),P(34.5f,38.5f),P(37,38.5f));b.AddLine(P(37,38.5f),P(45,38.5f));
        b.AddBezier(P(45,38.5f),P(47.5f,38.5f),P(50,37),P(49.5f,33));b.AddLine(P(49.5f,33),P(48,19));b.AddBezier(P(48,19),P(48,17),P(49,16),P(49,14));b.AddLine(P(49,14),P(49,11));b.CloseFigure();shape(b);
        for(int i=0;i<4;i++){g.DrawLine(&thin,P(34.5f,21+i*3.5f),P(47.5f,21+i*3.5f));}g.DrawLine(&thin,P(33,35),P(49,35));}
    else if(kind==1){rail(10,70);Gdiplus::GraphicsPath b;b.AddLine(P(16,11),P(19,16));b.AddBezier(P(19,16),P(21,19),P(24,20),P(28,20));b.AddLine(P(28,20),P(30,22));b.AddLine(P(30,22),P(36,22));b.AddLine(P(36,22),P(38,20));b.AddLine(P(38,20),P(42,20));b.AddLine(P(42,20),P(44,22));b.AddLine(P(44,22),P(50,22));b.AddLine(P(50,22),P(52,20));
        b.AddBezier(P(52,20),P(57,20),P(60,18),P(62,15));b.AddLine(P(62,15),P(64,11));b.CloseFigure();shape(b);g.DrawLine(&thin,P(22,15),P(60,15));}
    else if(kind==2){rail(14,66);Gdiplus::GraphicsPath b;b.AddLine(P(28,11),P(37,32));b.AddBezier(P(37,32),P(38.5f,35.5f),P(42,37),P(45.5f,36));b.AddLine(P(45.5f,36),P(50,34.5f));b.AddBezier(P(50,34.5f),P(53,33.5f),P(54,30.5f),P(52.5f,27.5f));b.AddLine(P(52.5f,27.5f),P(46,11));b.CloseFigure();shape(b);
        for(int i=0;i<4;i++)g.DrawLine(&thin,P(33.5f+i*2.6f,17+i*4.2f),P(45.5f+i*1.2f,15.5f+i*4.2f));}
    else if(kind==3){rail(14,66);Gdiplus::GraphicsPath b;b.AddLine(P(30,11),P(30,16));b.AddBezier(P(30,16),P(30,18.5f),P(32,20),P(34.5f,20));b.AddLine(P(34.5f,20),P(46,20));b.AddBezier(P(46,20),P(48.5f,20),P(50,18.5f),P(50,16));b.AddLine(P(50,16),P(50,11));b.CloseFigure();shape(b);
        Gdiplus::GraphicsPath th;th.AddBezier(P(50,13),P(56,13),P(61,15.5f),P(63,20));th.AddBezier(P(63,20),P(62,22),P(59,22),P(57,20));th.AddBezier(P(57,20),P(55,17.5f),P(53,16.5f),P(50,16.5f));th.CloseFigure();shape(th);g.DrawLine(&thin,P(33,16),P(47,16));}
    else if(kind==4){Gdiplus::GraphicsPath b;b.AddLine(P(12,13),P(60,13));b.AddBezier(P(60,13),P(66,13),P(70,16),P(70,20));b.AddBezier(P(70,20),P(70,24),P(66,27),P(60,27));b.AddLine(P(60,27),P(12,27));b.AddLine(P(12,27),P(10,25));b.AddLine(P(10,25),P(10,15));b.CloseFigure();shape(b);
        for(int i=0;i<4;i++){Gdiplus::GraphicsPath port;port.AddEllipse(P(26+i*9,14.5f).X,P(0,14.5f).Y,5*k,4*k);g.DrawPath(&thin,&port);}g.DrawLine(&thin,P(18,13),P(18,27));g.DrawLine(&thin,P(14,20),P(66,20));}
    else if(kind==5){Gdiplus::GraphicsPath b;b.AddLine(P(10,14),P(38,14));b.AddLine(P(38,14),P(40,12));b.AddLine(P(40,12),P(70,12));b.AddLine(P(70,12),P(70,28));b.AddLine(P(70,28),P(40,28));b.AddLine(P(40,28),P(38,26));b.AddLine(P(38,26),P(10,26));b.CloseFigure();shape(b);
        for(int i=0;i<3;i++){g.DrawLine(&thin,P(46+i*8,12),P(46+i*8,20));g.DrawLine(&thin,P(50+i*8,28),P(50+i*8,20));}g.DrawLine(&thin,P(18,14),P(18,26));}
    else if(kind==6){Gdiplus::GraphicsPath b;b.AddLine(P(14,12),P(68,12));b.AddBezier(P(68,12),P(72,12),P(74,15),P(74,20));b.AddBezier(P(74,20),P(74,25),P(72,28),P(68,28));b.AddLine(P(68,28),P(14,28));b.AddBezier(P(14,28),P(10,28),P(8,25),P(8,20));b.AddBezier(P(8,20),P(8,15),P(10,12),P(14,12));shape(b);
        for(float u:{16.f,20.f,62.f,66.f}){g.DrawLine(&thin,P(u,12),P(u,28));}g.DrawLine(&thin,P(24,16),P(58,16));}
    else{Gdiplus::GraphicsPath b;b.AddLine(P(8,10),P(38,10));b.AddLine(P(38,10),P(46,16));b.AddLine(P(46,16),P(72,16));b.AddBezier(P(72,16),P(74,16),P(75,17),P(75,19));b.AddLine(P(75,19),P(75,28));b.AddBezier(P(75,28),P(75,31),P(73,33),P(70,33));b.AddLine(P(70,33),P(52,33));b.AddLine(P(52,33),P(44,27));b.AddLine(P(44,27),P(8,27));b.CloseFigure();shape(b);
        Gdiplus::GraphicsPath hole;hole.AddRectangle(Gdiplus::RectF(P(52,20).X,P(0,20).Y,16*k,7*k));g.DrawPath(&thin,&hole);g.DrawLine(&thin,P(12,14),P(36,14));g.DrawLine(&thin,P(12,23),P(40,23));}}
void paintGrips(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS,mid=DT_CENTER|DT_VCENTER|DT_SINGLELINE;auto all=recs();const Rec* q=nullptr;for(auto& r:all)if(r.at==pickedAt)q=&r;if(!q){return;}
    Trace t=trace(q->at);int si=scopeIndex(q->scope);Grip gp=gripFor(t,si>=0&&!autoFire(q->scope));
    double v=std::min(1.0,gp.drift/.3),hz=std::min(1.0,gp.side/.3);gp.fit[3]=v*(10/15.0)*(1-.6*hz);double mf[3];int mb=-1;muzzleFit(v,hz,mf,mb);
    double fit[7]={gp.fit[0],gp.fit[1],gp.fit[2],gp.fit[3],mf[0],mf[1],mf[2]};auto recd=[&](int i){return i<4?i==gp.tile:mb>=0&&i==4+mb;};
    int pick=attPick>=0?attPick:gp.tile>=0?gp.tile:mb>=0?4+mb:0;
    const COLORREF green=RGB(70,208,146);
    textAt(dc,i18n::text(L"부착물 추천"),{36,20,400,52},sectionFont,ink,one);textAt(dc,(q->weapon.empty()?std::wstring(i18n::text(L"모르는 총")):w16(q->weapon))+(si>=0?L" · "+std::wstring(i18n::text(scopeNames[si])):L"")+L" · "+dateOf((double)q->at),{360,24,816,52},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,i18n::text(L"손잡이"),{36,64,400,88},boldFont,ink,one);textAt(dc,gp.tile>=0?std::wstring(i18n::text(L"녹색 칸이 내 패턴에 맞는 추천입니다 · 누르면 설명")):std::wstring(i18n::text(L"남은 흔들림이 작아 지금 손잡이를 유지해도 됩니다 · 누르면 설명")),{300,64,816,88},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,i18n::text(L"총구"),{36,218,400,242},boldFont,ink,one);
    auto gfx=[&](auto&& draw){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);draw(g);}; // one short-lived Graphics per drawing: a long one on the same DC fights the other helpers' own
    for(int i=0;i<7;i++){RECT r=attRect(i);bool rec=recd(i),sel=i==pick;
        COLORREF fill=rec?painting::mixColor(rowFill,green,.16):RGB(14,22,46),edge=rec?green:sel?RGB(96,142,255):RGB(30,46,86);
        roundBox(dc,r,fill,edge,16);if(sel){RECT in=r;InflateRect(&in,-1,-1);painting::outline(dc,in,rec?green:RGB(96,142,255),16);}
        // the picture centred above, the name, then the fit as a slim bar with its % at the end
        gfx([&](Gdiplus::Graphics& g){attachment(g,i,(r.left+r.right)/2.f-44.f,(float)r.top+12,1.1f,255,rec);});
        int pct=(int)std::lround(std::clamp(fit[i],0.0,1.0)*100);
        textAt(dc,i18n::text(atts[i].name),{r.left+16,r.top+66,r.right-16,r.top+88},boldFont,rec?RGB(220,255,236):ink,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        RECT bar={r.left+16,r.bottom-17,r.right-56,r.bottom-12};roundBox(dc,bar,RGB(24,36,70),RGB(24,36,70),5);int fw=(int)std::lround((bar.right-bar.left)*std::clamp(fit[i],0.0,1.0));if(fw>5){COLORREF c=rec?green:RGB(84,120,200);roundBox(dc,{bar.left,bar.top,bar.left+fw,bar.bottom},c,c,5);}
        textAt(dc,fmt(L"%d%%",pct),{r.right-54,r.bottom-26,r.right-14,r.bottom-4},smallBold,rec?green:RGB(150,166,198),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        if(rec){RECT pill={r.right-58,r.top+12,r.right-12,r.top+32};roundBox(dc,pill,green,green,20);textAt(dc,i18n::text(L"추천"),pill,captionFont,RGB(6,30,20),mid);}}
    // the detail card: the tile picked (or the recommendation): what it does, its figures, how it fits
    const Att& at=atts[pick];bool rec=recd(pick);RECT d={36,378,816,540};card(dc,d);
    {RECT ib={54,394,206,470};roundBox(dc,ib,rec?painting::mixColor(rowFill,green,.16):RGB(14,22,46),rec?green:RGB(30,46,86),12);float k=std::min((ib.right-ib.left-20)/80.f,(ib.bottom-ib.top-12)/44.f);gfx([&](Gdiplus::Graphics& g){attachment(g,pick,(ib.left+ib.right)/2.f-40*k,(ib.top+ib.bottom)/2.f-22*k,k,255,rec);});}
    textAt(dc,i18n::text(at.name),{224,392,470,418},sectionFont,rec?RGB(150,240,190):ink,one);
    if(rec){RECT pill={224,422,280,442};roundBox(dc,pill,green,green,20);textAt(dc,i18n::text(L"추천"),pill,smallBold,RGB(6,30,20),mid);}
    textAt(dc,fmt(L"내 패턴과 맞는 정도 %d%%",(int)std::lround(std::clamp(fit[pick],0.0,1.0)*100)),{rec?288:224,422,500,442},smallFont,rec?green:RGB(140,176,255),one);
    textAt(dc,i18n::text(at.about),{224,446,500,486},smallFont,muted,DT_LEFT|DT_WORDBREAK);
    for(int k=0;k<at.n;k++){const AttStat& st=at.st[k];int y=394+k*22;textAt(dc,i18n::text(st.label),{516,y,660,y+20},smallFont,muted,one);int bx=664,bw=82;roundBox(dc,{bx,y+7,bx+bw,y+13},RGB(22,34,64),RGB(22,34,64),6);
        if(st.mag>0){int fw=std::max(6,(int)(bw*std::min(1.0,st.mag/40)));COLORREF c=st.good?(rec?green:RGB(96,142,255)):RGB(236,96,110);roundBox(dc,{bx,y+7,bx+fw,y+13},c,c,6);}
        textAt(dc,st.mag>0?std::wstring(st.text):std::wstring(i18n::text(st.text)),{bx+bw+6,y,800,y+20},smallBold,st.good?ink:RGB(236,120,130),one);}
    textAt(dc,i18n::text(L"내 패턴")+std::wstring(L" · ")+fmt(L"좌우 흔들림 %d%% · 위아래 밀림 %d%%",(int)std::lround(gp.side*100),(int)std::lround(gp.drift*100)),{54,494,504,514},boldFont,ink,one);
    textAt(dc,i18n::text(L"정답은 아닙니다 · 바꾼 뒤 다시 측정해 보세요 · 수치: 나무위키 총기 부착물 (2026-10-18 판, 돌격소총 기준)"),{54,516,800,534},captionFont,RGB(110,124,154),one);}
// 반동 분석 결과: the captures on the left, the picked one on the right with the four traces and what to change.
RECT listRow(int i){return {48,104+i*54,288,104+i*54+48};}
void paintRecoil(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;auto all=recs();textAt(dc,i18n::text(L"반동 분석 결과"),{36,20,600,52},sectionFont,ink,one);if(!note.empty())textAt(dc,note,{360,24,816,52},smallFont,noteColor?noteColor:muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    card(dc,{36,64,300,540});textAt(dc,i18n::text(L"측정 기록"),{52,72,284,96},smallFont,muted,one);
    if(all.empty()){textAt(dc,fmt(L"아직 측정이 없습니다. 배그 훈련장에서 %ls를 눌러 시작하세요.",keyName(recoilcap::key()).c_str()),{52,110,284,200},smallFont,muted,DT_LEFT|DT_WORDBREAK);card(dc,{312,64,816,540});return;}
    if(!pickedAt){pickedAt=all[0].at;}const Rec* q=&all[0];
    for(int i=0;i<(int)all.size()&&i<8;i++){const Rec& r=all[i];RECT rr=listRow(i);bool on=r.at==pickedAt;if(on){q=&all[i];roundBox(dc,rr,RGB(26,42,82),RGB(52,92,255),12);}
        int si=scopeIndex(r.scope);textAt(dc,r.weapon.empty()?std::wstring(i18n::text(L"모르는 총")):w16(r.weapon),{rr.left+14,rr.top+4,rr.right-80,rr.top+26},boldFont,ink,one);
        textAt(dc,(si>=0?std::wstring(i18n::text(scopeNames[si]))+L" · ":L"")+dateOf((double)r.at),{rr.left+14,rr.top+26,rr.right-80,rr.bottom-4},smallFont,muted,one);
        textAt(dc,wfixed(r.vm*r.ratio,2),{rr.right-78,rr.top,rr.right-12,rr.bottom},boldFont,r.ratio>1.03?RGB(140,176,255):r.ratio<.97?RGB(240,170,120):RGB(96,210,150),DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
    card(dc,{312,64,816,540});textAt(dc,i18n::text(L"스코프"),{332,72,600,94},smallFont,muted,one);textAt(dc,i18n::text(L"총"),{332,142,520,162},smallFont,muted,one);
    // the graph: where the crosshair went, the hand, the gun alone, and the gun at the suggested sensitivity (the laser)
    // two plots: left the gun against the hand turned over (they overlap when the pull matches), right where the crosshair
    // went and where it would have gone at the suggested sensitivity (the laser), each scaled to fit its own lines
    Trace t=trace(q->at);RECT left={332,206,560,354},right={568,206,796,354};roundBox(dc,left,well,line,12);roundBox(dc,right,well,line,12);
    textAt(dc,i18n::text(L"총 반동 · 내 손 (뒤집음)"),{left.left+10,left.top+6,left.right-8,left.top+26},smallFont,muted,one);textAt(dc,i18n::text(L"조준점 · 추천 감도"),{right.left+10,right.top+6,right.right-8,right.top+26},smallFont,muted,one);
    if(!t.aim.empty()&&painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);std::vector<recoil::Pt> flip;for(auto& pt:t.hand)flip.push_back({pt.x,-pt.y});
        auto plot=[&](RECT box,std::vector<const std::vector<recoil::Pt>*> fitTo,float minSpan,auto&& draw){float x0=0,x1=0,y0=0,y1=0;for(auto* v:fitTo)for(auto& pt:*v){x0=std::min(x0,pt.x);x1=std::max(x1,pt.x);y0=std::min(y0,pt.y);y1=std::max(y1,pt.y);}
            float w=std::max(minSpan,x1-x0),hh=std::max(minSpan,y1-y0),sc=std::min((box.right-box.left-36)/w,(box.bottom-box.top-56)/hh),ox=(box.left+box.right)/2.f-(x0+x1)/2*sc,oy=(box.top+30+box.bottom)/2.f-(y0+y1)/2*sc;
            Gdiplus::Region clip(Gdiplus::Rect(box.left+2,box.top+28,box.right-box.left-4,box.bottom-box.top-30));g.SetClip(&clip);
            Gdiplus::Pen axis(Gdiplus::Color(255,36,56,104),1.f);axis.SetDashStyle(Gdiplus::DashStyleDash);g.DrawLine(&axis,(float)box.left+8,oy,(float)box.right-8,oy);g.DrawLine(&axis,ox,(float)box.top+30,ox,(float)box.bottom-8);
            draw(ox,oy,sc);Gdiplus::SolidBrush o(Gdiplus::Color(255,255,255,255));g.FillEllipse(&o,ox-3.5f,oy-3.5f,7.f,7.f);g.ResetClip();};
        auto poly=[&](const std::vector<recoil::Pt>& v,float ox,float oy,float sc,Gdiplus::Color c,float wd,bool dash){if(v.size()<2)return;std::vector<Gdiplus::PointF> q2;for(auto& pt:v)q2.push_back({ox+pt.x*sc,oy+pt.y*sc});Gdiplus::Pen pen(c,wd);painting::roundCaps(pen);pen.SetLineJoin(Gdiplus::LineJoinRound);if(dash)pen.SetDashStyle(Gdiplus::DashStyleDash);g.DrawLines(&pen,q2.data(),(INT)q2.size());};
        auto dots=[&](const std::vector<recoil::Pt>& v,float ox,float oy,float sc,Gdiplus::Color c){Gdiplus::SolidBrush br(c);size_t st=std::max<size_t>(1,v.size()/24);for(size_t i=0;i<v.size();i+=st){float x=ox+v[i].x*sc,y=oy+v[i].y*sc;g.FillEllipse(&br,x-2.6f,y-2.6f,5.2f,5.2f);}};
        plot(left,{&t.gun,&flip},40.f,[&](float ox,float oy,float sc){poly(t.gun,ox,oy,sc,Gdiplus::Color(220,150,160,184),2.f,true);poly(flip,ox,oy,sc,Gdiplus::Color(240,96,140,255),2.2f,false);});
        plot(right,{&t.aim,&t.laser},24.f,[&](float ox,float oy,float sc){poly(t.laser,ox,oy,sc,Gdiplus::Color(60,96,210,150),7.f,false);poly(t.laser,ox,oy,sc,Gdiplus::Color(255,96,210,150),2.4f,false);poly(t.aim,ox,oy,sc,Gdiplus::Color(255,236,240,250),2.f,false);dots(t.aim,ox,oy,sc,Gdiplus::Color(255,236,240,250));dots(t.laser,ox,oy,sc,Gdiplus::Color(255,96,210,150));});}
    struct Key{COLORREF c;const wchar_t* name;};Key keys[]={{RGB(236,240,250),L"조준점"},{RGB(96,140,255),L"마우스(손)"},{RGB(150,160,184),L"총 반동"},{RGB(96,210,150),L"추천 감도"}};int lx=336;
    for(auto& k:keys){std::wstring nm=i18n::text(k.name);painting::circle(dc,{lx,366,lx+8,374},k.c,CLR_NONE);SIZE ts={};HGDIOBJ of=SelectObject(dc,captionFont);GetTextExtentPoint32W(dc,nm.c_str(),(int)nm.size(),&ts);SelectObject(dc,of);textAt(dc,nm,{lx+14,360,lx+14+ts.cx+4,380},captionFont,muted,one);lx+=ts.cx+34;}
    // the two numbers to type into PUBG, side by side: 수직 감도 and the scope's sensitivity (from -> to, the change)
    recoil::Steady sd=steadyFor(all,*q);double mult=sd.value/std::max(.01,q->vm);int si=scopeIndex(q->scope);double cur=si>=0?scopeSlider(recoilcap::pubgSens(),q->scope):0;
    auto width=[&](HFONT f,const std::wstring& s){SIZE z={};HGDIOBJ o=SelectObject(dc,f);GetTextExtentPoint32W(dc,s.c_str(),(int)s.size(),&z);SelectObject(dc,o);return (int)z.cx;};
    auto tile=[&](RECT t,bool lead,const std::wstring& label,const std::wstring& from,const std::wstring& to,const std::wstring& delta,const std::wstring& sub){
        roundBox(dc,t,lead?RGB(24,40,84):RGB(18,30,62),lead?RGB(64,104,220):line,14);textAt(dc,label,{t.left+16,t.top+9,t.right-80,t.top+29},smallFont,muted,one);
        if(!delta.empty()){int dw=width(captionFont,delta)+18;RECT p={t.right-14-dw,t.top+9,t.right-14,t.top+29};roundBox(dc,p,RGB(18,58,48),RGB(18,58,48),10);textAt(dc,delta,p,captionFont,RGB(96,210,150),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
        if(to.empty()){textAt(dc,from,{t.left+16,t.top+32,t.right-12,t.top+60},smallFont,muted,one);}
        else{int x=t.left+16;textAt(dc,from,{x,t.top+27,x+width(sectionFont,from)+4,t.top+57},sectionFont,RGB(200,210,232),one);x+=width(sectionFont,from)+10;
            textAt(dc,L"\x2192",{x,t.top+30,x+28,t.top+62},sectionFont,RGB(100,120,165),one);x+=width(sectionFont,L"\x2192")+10;textAt(dc,to,{x,t.top+27,t.right-12,t.top+57},sectionFont,RGB(130,175,255),one);}
        if(!sub.empty())textAt(dc,sub,{t.left+16,t.bottom-22,t.right-12,t.bottom-4},captionFont,muted,one);};
    int dv=(int)std::lround((mult-1)*100);
    tile({332,388,560,466},true,i18n::text(L"수직 감도"),wfixed(q->vm,2),wfixed(sd.value,2),dv?fmt(dv>0?L"+%d%%":L"%d%%",dv):std::wstring(i18n::text(L"그대로")),
        sd.n>=3?fmt(L"최근 %d탄창 중간값 · 차이 ±%d%%",sd.n,(int)std::lround(sd.spread*100)):fmt(L"%d/5탄창 측정",sd.n));
    if(cur>0){double to=aim::sliderFor(cur,mult);int ds=(int)std::lround(to-cur);tile({568,388,796,466},false,fmt(L"%ls 감도",i18n::text(scopeNames[si])),wfixed(cur,0),wfixed(to,0),ds?fmt(ds>0?L"+%d":L"%d",ds):std::wstring(i18n::text(L"그대로")),
        std::wstring(i18n::text(L"둘 중 하나만 바꾸세요"))+L" · "+i18n::text(autoFire(q->scope)?L"연사":L"단발"));}
    else tile({568,388,796,466},false,i18n::text(L"스코프 감도"),i18n::text(L"위에서 스코프를 고르면 표시"),L"",L"",L"");
    // the grip that fits this spray (values from PUBG's own patch notes)
    Grip gp=gripFor(t,si>=0&&!autoFire(q->scope));roundBox(dc,{332,476,796,534},rowFill,line,14);
    {RECT ib={344,484,398,526};roundBox(dc,ib,RGB(14,22,46),RGB(30,46,86),10);int kind=gp.tile>=0?gp.tile:0;if(painting::token){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);attachment(g,kind,(ib.left+ib.right)/2.f-24.f,(ib.top+ib.bottom)/2.f-13.f,.6f,255,gp.tile>=0);}}
    textAt(dc,i18n::text(L"추천 손잡이")+std::wstring(L" · ")+i18n::text(gp.name),{412,482,652,504},boldFont,ink,one);
    textAt(dc,i18n::text(gp.why),{412,505,652,527},smallFont,muted,one);}
// 자세한 분석: the settings PUBG saved, each magazine's error on a crosshair chart, the spray paths, the numbers, and what to change.
// The magazines: the picked record's, then earlier ones of the same gun and scope, up to five (like the suggestion).
void paintAnalysis(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;auto all=recs();if(all.empty())return;const Rec* q=&all[0];for(auto& r:all)if(r.at==pickedAt)q=&r;
    int si=scopeIndex(q->scope);textAt(dc,i18n::text(L"자세한 분석"),{36,20,400,52},sectionFont,ink,one);
    textAt(dc,(q->weapon.empty()?std::wstring(i18n::text(L"모르는 총")):w16(q->weapon))+(si>=0?L" · "+std::wstring(i18n::text(scopeNames[si])):L"")+L" · "+dateOf((double)q->at),{400,24,816,52},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    std::vector<recoil::Burst> mags;std::vector<int> owner;double hc=0;int hcn=0,ri=0;
    for(auto& r:all){if(r.at>q->at)continue;if(r.at!=q->at&&(q->weapon.empty()||r.weapon!=q->weapon||r.scope!=q->scope))continue;Trace t=trace(r.at);if(t.hComp>-9){hc+=t.hComp;hcn++;}
        for(auto& b:t.each){if(mags.size()<5){mags.push_back(b);owner.push_back(ri);}}ri++;if(mags.size()>=5)break;}
    recoil::Summary sm=recoil::summarize(mags);if(hcn)hc/=hcn;
    const COLORREF pal[]={RGB(96,140,255),RGB(96,210,150),RGB(255,186,82),RGB(255,104,134),RGB(172,138,255)};
    // the settings PUBG saved (GameUserSettings.ini, read only)
    aim::Sens sens=recoilcap::pubgSens();Profile pf=profile();card(dc,{36,64,300,540});textAt(dc,i18n::text(L"내 배그 설정"),{52,72,284,96},boldFont,ink,one);textAt(dc,i18n::text(L"배그 설정 파일에서 불러옴"),{52,96,284,116},smallFont,muted,one);
    recoil::Steady sd=steadyFor(all,*q);if(sd.n==0){sd.value=q->vm*q->ratio;}double mult=sd.value/std::max(.01,q->vm),dv=(mult-1)*100;bool vAct=std::fabs(dv)>=3;
    if(!sens.found){textAt(dc,i18n::text(L"배그 설정 파일을 찾지 못했습니다 · 배그를 한 번 켠 뒤 다시 열어 주세요"),{52,128,284,200},smallFont,muted,DT_LEFT|DT_WORDBREAK);}
    else{struct Row{std::wstring k,v;bool hot;};std::vector<Row> rows;double cm=aim::cm360c(sens.normalConv,pf.dpi,pf.yaw);
        rows.push_back({L"DPI",pf.dpi?std::to_wstring(pf.dpi):L"—",false});rows.push_back({i18n::text(L"일반"),wfixed(sens.normal,0),false});rows.push_back({L"cm/360",cm>0?wfixed(cm,1)+L" cm":L"—",false});
        rows.push_back({i18n::text(L"수직"),wfixed(sens.vertical,2),vAct});rows.push_back({i18n::text(L"조준"),wfixed(sens.targeting,0),q->scope=="t"});rows.push_back({i18n::text(L"조준경"),wfixed(sens.scoping,0),q->scope=="1"});
        const char* sc[]={"2","3","4","6"};for(auto c:sc){double v=0;bool have=false;for(auto& x:sens.scopes)if(x.first==std::string("Scope")+c+"X"){v=x.second;have=true;}if(have)rows.push_back({std::string(c)=="15"?std::wstring(i18n::text(L"15배")):std::wstring(i18n::text(scopeNames[scopeIndex(c)])),wfixed(v,0),q->scope==c});}
        int y=124;for(auto& r:rows){RECT rr={48,y,288,y+28};if(r.hot)roundBox(dc,rr,RGB(26,42,82),RGB(52,92,255),8);textAt(dc,r.k,{rr.left+12,rr.top,rr.left+150,rr.bottom},smallFont,r.hot?ink:muted,one);textAt(dc,r.v,{rr.left+120,rr.top,rr.right-12,rr.bottom},boldFont,r.hot?RGB(140,176,255):ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);y+=31;}
        if(std::fabs(q->vm-sens.vertical)>.005)textAt(dc,fmt(L"측정할 때 수직 %ls",wfixed(q->vm,2).c_str()),{52,y+6,284,y+28},smallFont,muted,one);}
    // the error chart and the spray paths
    RECT ch={312,64,556,236},tr={568,64,816,236};card(dc,ch);card(dc,tr);textAt(dc,i18n::text(L"탄착 오차"),{ch.left+16,72,ch.right-12,96},boldFont,ink,one);textAt(dc,i18n::text(L"스프레이 궤적"),{tr.left+16,72,tr.right-12,96},boldFont,ink,one);
    auto gfx=[&](auto&& draw){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);draw(g);};
    if(mags.empty()){textAt(dc,i18n::text(L"이 기록에는 탄창별 자료가 없습니다 · 새로 측정하면 보입니다"),{ch.left+16,140,tr.right-16,220},smallFont,muted,DT_CENTER|DT_WORDBREAK);}
    else{double R=10;for(auto& b:mags)R=std::max(R,std::max(std::fabs(b.vErr),std::fabs(b.hErr))*1.25);
        float cx=(ch.left+ch.right)/2.f,cy=(ch.top+30+ch.bottom-22)/2.f,half=std::min((ch.right-ch.left-40)/2.f,(ch.bottom-ch.top-62)/2.f),k=(float)(half/R);
        gfx([&](Gdiplus::Graphics& g){Gdiplus::Pen ax(Gdiplus::Color(255,36,56,104),1.f);g.DrawLine(&ax,cx-half,cy,cx+half,cy);g.DrawLine(&ax,cx,cy-half,cx,cy+half);
            Gdiplus::Pen zone(Gdiplus::Color(200,96,210,150),1.4f);zone.SetDashStyle(Gdiplus::DashStyleDash);float z=3*k;g.DrawEllipse(&zone,cx-z,cy-z,2*z,2*z);
            for(size_t i=0;i<mags.size();i++){COLORREF c=pal[owner[i]%5];float x=cx+(float)mags[i].hErr*k,y=cy-(float)mags[i].vErr*k;Gdiplus::SolidBrush glow(Gdiplus::Color(60,GetRValue(c),GetGValue(c),GetBValue(c)));g.FillEllipse(&glow,x-8.f,y-8.f,16.f,16.f);Gdiplus::SolidBrush br(Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c)));g.FillEllipse(&br,x-4.5f,y-4.5f,9.f,9.f);}
            float mx=cx+(float)sm.hErr*k,my=cy-(float)sm.vErr*k;Gdiplus::Pen avg(Gdiplus::Color(255,240,244,252),2.f);g.DrawEllipse(&avg,mx-7.f,my-7.f,14.f,14.f);});
        textAt(dc,fmt(L"±%d%%",(int)std::lround(R)),{ch.right-90,72,ch.right-16,96},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
        textAt(dc,i18n::text(L"위 = 덜 당김 · 원 = 오차 3%"),{ch.left+16,ch.bottom-24,ch.right-12,ch.bottom-6},smallFont,muted,one);
        // paths: every magazine from one origin, the same scale
        float x0=0,x1=0,y0=0,y1=0;for(auto& b:mags)for(auto& pt:b.aim){x0=std::min(x0,pt.x);x1=std::max(x1,pt.x);y0=std::min(y0,pt.y);y1=std::max(y1,pt.y);}
        float w=std::max(30.f,x1-x0),hh=std::max(30.f,y1-y0),sc=std::min((tr.right-tr.left-40)/w,(tr.bottom-tr.top-56)/hh),ox=(tr.left+tr.right)/2.f-(x0+x1)/2*sc,oy=(tr.top+32+tr.bottom-12)/2.f-(y0+y1)/2*sc;
        gfx([&](Gdiplus::Graphics& g){Gdiplus::Pen ax(Gdiplus::Color(255,36,56,104),1.f);ax.SetDashStyle(Gdiplus::DashStyleDash);g.DrawLine(&ax,(float)tr.left+12,oy,(float)tr.right-12,oy);g.DrawLine(&ax,ox,(float)tr.top+34,ox,(float)tr.bottom-10);
            for(size_t i=0;i<mags.size();i++){auto& v=mags[i].aim;if(v.size()<2)continue;COLORREF c=pal[owner[i]%5];std::vector<Gdiplus::PointF> pts;for(auto& pt:v)pts.push_back({ox+pt.x*sc,oy+pt.y*sc});Gdiplus::Pen pen(Gdiplus::Color(230,GetRValue(c),GetGValue(c),GetBValue(c)),2.f);painting::roundCaps(pen);pen.SetLineJoin(Gdiplus::LineJoinRound);g.DrawLines(&pen,pts.data(),(INT)pts.size());
                Gdiplus::SolidBrush end(Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c)));g.FillEllipse(&end,pts.back().X-3.5f,pts.back().Y-3.5f,7.f,7.f);
                // each shot: a small ring, the shot where it broke away filled white
                Gdiplus::Pen ring(Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c)),1.2f);Gdiplus::SolidBrush hole(Gdiplus::Color(255,13,22,44)),brk(Gdiplus::Color(255,255,255,255));
                for(size_t k2=0;k2<mags[i].shot.size();k2++){auto& sp=mags[i].shot[k2];float x=ox+sp.x*sc,y=oy+sp.y*sc;bool b=(int)k2+1==mags[i].breakAt;g.FillEllipse(b?&brk:&hole,x-2.6f,y-2.6f,5.2f,5.2f);g.DrawEllipse(&ring,x-2.6f,y-2.6f,5.2f,5.2f);}}
            Gdiplus::SolidBrush o(Gdiplus::Color(255,255,255,255));g.FillEllipse(&o,ox-4.f,oy-4.f,8.f,8.f);});}
    COLORREF ok=RGB(96,210,150),warn=RGB(255,186,82);
    // 개선할 점: at most three cards, the biggest problem first (each: what, how much, what to do); none = a green card
    struct Issue {int level;std::wstring title,value,todo;};std::vector<Issue> issues;
    if(dv>=3)issues.push_back({2,i18n::text(L"수직 당김 부족"),fmt(L"%d%%",(int)std::lround(dv)),fmt(L"수직 감도 %ls → %ls",wfixed(q->vm,2).c_str(),wfixed(sd.value,2).c_str())});
    else if(dv<=-3)issues.push_back({2,i18n::text(L"수직 당김 과다"),fmt(L"%d%%",(int)std::lround(-dv)),fmt(L"수직 감도 %ls → %ls",wfixed(q->vm,2).c_str(),wfixed(sd.value,2).c_str())});
    if(sm.n){if(hcn&&hc>1.15)issues.push_back({1,i18n::text(L"좌우 과잉 보정"),fmt(L"+%d%%",(int)std::lround((hc-1)*100)),i18n::text(L"좌우로 덜 움직이기")});
        else if(std::fabs(sm.hErr)>3)issues.push_back({1,i18n::text(sm.hErr>0?L"오른쪽으로 흐름":L"왼쪽으로 흐름"),fmt(L"%d%%",(int)std::lround(std::fabs(sm.hErr))),i18n::text(sm.hErr>0?L"왼쪽으로 조금 더 당기기":L"오른쪽으로 조금 더 당기기")});
        if(sm.wrong>40)issues.push_back({1,i18n::text(L"좌우 반응 실수"),fmt(L"%d%%",(int)std::lround(sm.wrong)),i18n::text(L"튀는 반대쪽으로 잡기")});
        if(sm.react>.15)issues.push_back({1,i18n::text(L"첫 당김 늦음"),fmt(L"%ls초",wfixed(sm.react,2).c_str()),i18n::text(L"쏘자마자 당기기")});
        if(sm.breakAt>0&&sm.shots>0&&sm.breakAt<sm.shots*.6)issues.push_back({1,i18n::text(L"탄이 일찍 흐트러짐"),fmt(L"%d번째 탄",sm.breakAt),i18n::text(L"그때부터 조금 더 당기기")});
        if(sm.phase[2]<85)issues.push_back({1,i18n::text(L"후반 힘 빠짐"),fmt(L"%d%%",(int)std::lround(sm.phase[2])),i18n::text(L"끝까지 당기기")});else if(sm.phase[2]>115)issues.push_back({1,i18n::text(L"후반 과하게 당김"),fmt(L"%d%%",(int)std::lround(sm.phase[2])),i18n::text(L"끝에서 힘 빼기")});
        else if(sm.phase[0]<80)issues.push_back({1,i18n::text(L"초반 덜 당김"),fmt(L"%d%%",(int)std::lround(sm.phase[0])),i18n::text(L"처음부터 당기기")});
        if(sm.vSd>8)issues.push_back({0,i18n::text(L"탄창마다 차이 큼"),fmt(L"±%ls%%",wfixed(sm.vSd,1).c_str()),i18n::text(L"몇 번 더 측정하기")});}
    std::stable_sort(issues.begin(),issues.end(),[](const Issue& a,const Issue& b){return a.level>b.level;});
    RECT ic={312,248,816,404};card(dc,ic);textAt(dc,i18n::text(L"개선할 점"),{ic.left+16,ic.top+8,ic.right-16,ic.top+30},boldFont,ink,one);
    if(issues.empty()){RECT g={ic.left+16,ic.top+40,ic.right-16,ic.bottom-14};roundBox(dc,g,RGB(14,46,40),RGB(40,140,100),14);painting::circle(dc,{g.left+18,g.top+40,g.left+44,g.top+66},ok,CLR_NONE);textAt(dc,L"\x2713",{g.left+18,g.top+40,g.left+44,g.top+66},boldFont,RGB(6,30,20),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        textAt(dc,i18n::text(sm.n?L"잘 잡고 있습니다":L"새로 측정하면 개선할 점을 알려 드립니다"),{g.left+58,g.top+28,g.right-16,g.top+56},sectionFont,ink,one);textAt(dc,i18n::text(sm.n?L"눈에 띄는 문제가 없습니다 · 감도는 지금 그대로 두세요":L"배그 훈련장에서 Insert를 누르고 쏘세요"),{g.left+58,g.top+56,g.right-16,g.top+80},smallFont,muted,one);}
    else{int n=(int)std::min<size_t>(3,issues.size()),cw=(ic.right-ic.left-32-(n-1)*12)/n;
        for(int i=0;i<n;i++){const Issue& is=issues[i];RECT c={ic.left+16+i*(cw+12),ic.top+40,ic.left+16+i*(cw+12)+cw,ic.bottom-14};COLORREF tone=is.level==2?RGB(110,160,255):is.level==1?warn:RGB(150,166,198);
            roundBox(dc,c,painting::mixColor(tone,RGB(12,20,42),.86),painting::mixColor(tone,RGB(12,20,42),.55),14);roundBox(dc,{c.left+14,c.top+14,c.left+18,c.bottom-14},tone,tone,2);
            textAt(dc,is.title,{c.left+28,c.top+10,c.right-10,c.top+32},smallBold,ink,one);textAt(dc,is.value,{c.left+28,c.top+32,c.right-10,c.top+66},sectionFont,tone,one);textAt(dc,is.todo,{c.left+28,c.bottom-30,c.right-10,c.bottom-10},captionFont,RGB(190,202,228),one);}}
    // the numbers, quieter: a 3 x 3 grid of label and value, a dot for the ones that need work
    RECT mc={312,416,816,540};card(dc,mc);
    auto cell=[&](int i,const wchar_t* k,const std::wstring& v,int state){int col=i%3,row=i/3,cw=(mc.right-mc.left-32)/3,x=mc.left+16+col*cw,y=mc.top+10+row*36;
        textAt(dc,i18n::text(k),{x,y,x+cw-80,y+32},captionFont,muted,one);textAt(dc,v,{x+cw-80,y,x+cw-10,y+32},smallBold,state==1?warn:state==2?ok:ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);};
    std::wstring na=L"—";auto pc=[](double v){return wfixed(std::fabs(v),1)+L"%";};double dt=recoil::fireInterval(q->weapon);
    cell(0,L"평균 오차",sm.n?pc(std::max(std::fabs(sm.hErr),std::fabs(sm.vErr))):na,sm.n?(std::max(std::fabs(sm.hErr),std::fabs(sm.vErr))<=3?2:1):0);
    cell(1,L"탄창별 차이",sm.n?pc(std::max(sm.hSd,sm.vSd)):na,sm.n&&sm.vSd>8?1:0);
    cell(2,L"좌우 보정",hcn?wfixed(hc*100,0)+L"%":na,hcn&&(hc>1.15||hc<.6)?1:0);
    cell(3,L"좌우 반응 실수",sm.wrong>=0?wfixed(sm.wrong,0)+L"%":na,sm.wrong>40?1:0);
    cell(4,L"첫 당김",sm.n?fmt(L"%ls초",wfixed(sm.react,2).c_str()):na,sm.n&&sm.react>.15?1:0);
    cell(5,L"구간별 당김",sm.n?wfixed(sm.phase[0],0)+L"·"+wfixed(sm.phase[1],0)+L"·"+wfixed(sm.phase[2],0):na,0);
    cell(6,L"탄창당 탄 수",sm.shots>0?fmt(L"%d발",(int)std::lround(sm.shots)):na,0);
    cell(7,L"흐트러지는 탄",sm.breakAt>0?fmt(L"%d번째 탄",sm.breakAt):sm.shots>0?std::wstring(i18n::text(L"끝까지 유지")):na,sm.breakAt>0&&sm.breakAt<sm.shots*.6?1:sm.breakAt<0&&sm.shots>0?2:0);
    cell(8,L"연사 속도",dt>0?fmt(L"%d RPM",(int)std::lround(60/dt)):na,0);}
// 실전 기록: what the live recorder logged (live.tsv), one entry per match (the PUBG join time; outside a match, per day).
struct LiveRow {long long at=0,match=0;std::string weapon,scope;int shots=0,breakAt=-1;double hold=0,ratio=0,vErr=0,hErr=0,wrong=-1,react=0;bool ok=false;};
struct LiveMatch {long long key=0,first=0,last=0;bool match=false;std::vector<LiveRow> rows;};
long long livePick=0;
std::vector<LiveMatch> liveMatches(){std::vector<LiveRow> rows;std::istringstream in(readAll(dir()+L"\\live.tsv"));std::string l;
    while(std::getline(in,l)){if(!l.empty()&&l.back()=='\r')l.pop_back();std::vector<std::string> c;std::stringstream ss(l);std::string x;while(std::getline(ss,x,'\t'))c.push_back(x);if(c.size()<13)continue;
        LiveRow r;r.at=atoll(c[0].c_str());r.match=atoll(c[1].c_str());r.weapon=c[2];r.scope=c[3];r.shots=atoi(c[4].c_str());r.hold=atof(c[5].c_str());r.ok=c[6]=="1";r.ratio=atof(c[7].c_str());r.vErr=atof(c[8].c_str());r.hErr=atof(c[9].c_str());r.wrong=atof(c[10].c_str());r.react=atof(c[11].c_str());r.breakAt=atoi(c[12].c_str());if(r.at>0)rows.push_back(r);}
    std::vector<LiveMatch> v;for(auto& r:rows){long long key=r.match>0?r.match:-(r.at-(r.at%86400));LiveMatch* m=nullptr;for(auto& x:v)if(x.key==key)m=&x;if(!m){v.push_back({});m=&v.back();m->key=key;m->match=r.match>0;m->first=r.at;}m->last=std::max(m->last,r.at);m->first=std::min(m->first,r.at);m->rows.push_back(r);}
    std::sort(v.begin(),v.end(),[](const LiveMatch& a,const LiveMatch& b){return a.last>b.last;});return v;}
void paintLive(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;textAt(dc,i18n::text(L"실전 기록"),{36,20,400,52},sectionFont,ink,one);
    bool on=livecap::enabled();textAt(dc,i18n::text(on?L"배그에서 쏘면 자동으로 기록합니다":L"자동 기록이 꺼져 있습니다"),{400,24,816,52},smallFont,on?RGB(96,210,150):muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    auto all=liveMatches();card(dc,{36,64,300,540});textAt(dc,i18n::text(L"경기"),{52,72,284,96},smallFont,muted,one);
    if(all.empty()){textAt(dc,i18n::text(L"아직 기록이 없습니다. 배그에서 총을 쏘면 총·탄 수·긴 연사의 반동이 경기별로 쌓입니다."),{52,110,284,240},smallFont,muted,DT_LEFT|DT_WORDBREAK);card(dc,{312,64,816,540});}
    else{if(!livePick)livePick=all[0].key;const LiveMatch* q=&all[0];
        for(int i=0;i<(int)all.size()&&i<8;i++){const LiveMatch& m=all[i];RECT rr=listRow(i);bool sel=m.key==livePick;if(sel){q=&all[i];roundBox(dc,rr,RGB(26,42,82),RGB(52,92,255),12);}int shots=0;for(auto& r:m.rows)shots+=r.shots;
            textAt(dc,std::wstring(i18n::text(m.match?L"경기":L"연습 · 기타"))+L" · "+dateOf((double)m.first),{rr.left+14,rr.top+4,rr.right-12,rr.top+26},boldFont,ink,one);
            textAt(dc,fmt(L"%d발 · 사격 %d번",shots,(int)m.rows.size()),{rr.left+14,rr.top+26,rr.right-12,rr.bottom-4},smallFont,muted,one);}
        // the chosen match: one line per gun
        card(dc,{312,64,816,540});int shots=0,longs=0;for(auto& r:q->rows){shots+=r.shots;longs+=r.ok;}
        textAt(dc,fmt(L"총 %d발 · 사격 %d번 · 연사 분석 %d번",shots,(int)q->rows.size(),longs),{332,72,796,98},boldFont,ink,one);
        const int cx[]={332,470,540,610,690};const wchar_t* head[]={L"총",L"탄 수",L"사격",L"연사 분석",L"평균 오차 · 좌우 실수"};for(int i=0;i<5;i++)textAt(dc,i18n::text(head[i]),{cx[i],110,i<4?cx[i+1]-6:796,130},smallFont,muted,one);
        std::vector<std::string> guns;for(auto& r:q->rows)if(std::find(guns.begin(),guns.end(),r.weapon)==guns.end())guns.push_back(r.weapon);
        std::sort(guns.begin(),guns.end(),[&](const std::string& a,const std::string& b){int sa=0,sb=0;for(auto& r:q->rows){if(r.weapon==a)sa+=r.shots;if(r.weapon==b)sb+=r.shots;}return sa>sb;});
        int y=136;for(size_t gi=0;gi<guns.size()&&gi<8;gi++){int sh=0,n=0,ln=0;double ve=0,wr=0;int wn=0;for(auto& r:q->rows){if(r.weapon!=guns[gi])continue;sh+=r.shots;n++;if(r.ok){ln++;ve+=std::fabs(r.vErr);if(r.wrong>=0){wr+=r.wrong;wn++;}}}
            RECT rr={324,y,804,y+38};roundBox(dc,rr,rowFill,line,10);textAt(dc,guns[gi].empty()?std::wstring(i18n::text(L"모르는 총")):w16(guns[gi]),{cx[0]+4,y,cx[1]-6,y+38},boldFont,ink,one);
            textAt(dc,std::to_wstring(sh),{cx[1],y,cx[2]-6,y+38},boldFont,ink,one);textAt(dc,std::to_wstring(n),{cx[2],y,cx[3]-6,y+38},smallFont,ink,one);textAt(dc,std::to_wstring(ln),{cx[3],y,cx[4]-6,y+38},smallFont,ink,one);
            textAt(dc,ln?wfixed(ve/ln,1)+L"% · "+(wn?wfixed(wr/wn,0)+L"%":std::wstring(L"—")):std::wstring(L"—"),{cx[4],y,796,y+38},smallFont,ln&&ve/ln>8?RGB(255,186,82):ink,one);y+=44;}
        textAt(dc,i18n::text(L"탄 수는 누른 시간과 연사 속도로 계산한 추정입니다 · 연사 분석은 0.7초 이상 누른 사격만"),{332,488,796,508},smallFont,muted,one);
        textAt(dc,i18n::text(L"실전은 적을 따라가며 쏜 것도 섞여 참고용입니다 · 감도 추천은 훈련장 측정을 기준으로 하세요"),{332,508,796,530},smallFont,muted,one);}}
void paintRecoilSettings(HDC dc){textAt(dc,i18n::text(L"반동 분석 설정"),{36,20,816,52},sectionFont,ink);
    card(dc,{36,64,816,236});textAt(dc,i18n::text(L"측정 키 · 배그가 앞에 있을 때만 씁니다"),{56,78,796,100},smallFont,muted);
    textAt(dc,i18n::text(L"모드 전환 단축키와 같은 키면 배그 안에서는 측정 키로 씁니다."),{56,168,796,220},smallFont,RGB(110,124,154),DT_LEFT|DT_WORDBREAK);
    card(dc,{36,250,816,540});textAt(dc,i18n::text(L"스코프별 측정 방식 · 켜면 연사, 끄면 단발"),{56,264,796,286},smallFont,muted);}
void paintAccount(HDC dc){stepper(dc,0);heading(dc,L"PUBG 계정 연결",L"게임 닉네임만 입력하면 됩니다. 비밀번호는 묻지 않습니다.");
    card(dc,{196,176,656,500});textAt(dc,i18n::text(L"플랫폼"),{224,194,628,214},smallFont,muted);textAt(dc,i18n::text(L"닉네임"),{224,282,628,302},smallFont,muted);field(dc,513);noteLine(dc);}
void paintDpi(HDC dc){Profile p=profile();stepper(dc,1);heading(dc,L"마우스 DPI",L"마우스 소프트웨어에 설정한 DPI를 입력하세요");
    card(dc,{196,176,656,476});textAt(dc,L"DPI",{224,194,628,214},smallFont,muted);field(dc,521);
    if(p.fromIni)textAt(dc,fmt(L"배그 감도 · 일반 %ls · 수직 %ls (설정 파일에서 읽음)",wfixed(p.normal,0).c_str(),wfixed(p.vertical,2).c_str()),{224,346,628,370},smallFont,RGB(110,124,154),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    else{textAt(dc,i18n::text(L"일반 감도"),{224,336,420,354},smallFont,muted);textAt(dc,i18n::text(L"수직 배율"),{432,336,628,354},smallFont,muted);}
    noteLine(dc);}
void paintChoose(HDC dc){KV a=account();std::wstring n=w16(str(a,"name"));heading(dc,L"다시 오신 걸 환영합니다",L"저장된 계정으로 계속하거나 다른 계정으로 로그인하세요.");
    card(dc,{226,180,626,336});RECT av={392,198,460,266};roundBox(dc,av,RGB(52,92,255),RGB(52,92,255),34);textAt(dc,n.substr(0,1),av,sectionFont,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    textAt(dc,n,{246,274,606,302},sectionFont,ink,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);textAt(dc,str(a,"shard")=="kakao"?L"Kakao":L"Steam",{246,304,606,324},smallFont,muted,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
void paintDetail(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS;KV r=load(L"result.txt"),ap=load(L"applied.txt");Profile p=profile();
    card(dc,{36,20,420,540});textAt(dc,i18n::text(L"감도 분석 결과"),{56,32,400,58},boldFont,ink,one);
    if(r.empty())textAt(dc,i18n::text(L"아직 분석하지 않았습니다."),{56,66,400,92},smallFont,muted,one);
    else{int y=66;auto row=[&](const std::wstring& k,const std::wstring& v){textAt(dc,k,{56,y,250,y+26},smallFont,muted,one);textAt(dc,v,{250,y,400,y+26},smallFont,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);y+=28;};
        row(i18n::text(L"분석한 때"),dateOf(num(r,"at")));row(i18n::text(L"배그 감도 보정"),i18n::text(r["calibrated"]=="1"?L"보정됨":L"안 됨 (추정 속도)"));row(i18n::text(L"표적 전환 측정"),fmt(L"%d회",(int)num(r,"flick.done")));
        if(r.count("flick.hit")){row(i18n::text(L"명중 (현재 감도)"),wfixed(num(r,"flick.hit"),0)+L"%");row(i18n::text(L"표적까지 걸린 시간"),wfixed(num(r,"flick.reach")*1000,0)+L" ms");row(i18n::text(L"지나친 정도 (오버슈트)"),wfixed(num(r,"flick.over"),1)+L"%");}
        row(i18n::text(L"수직 반동 측정"),fmt(L"%d회",(int)num(r,"recoil.done")));if(num(r,"recoil.n"))row(i18n::text(L"반동 중 평균 오차 (현재)"),wfixed(num(r,"recoil.base"),2)+L"°");
        if(hasAds(r)){row(i18n::text(L"조준(ADS) 측정"),fmt(L"%d회",(int)num(r,"ads.done")));}
        y+=8;for(int k:{(int)aim::Flick,(int)aim::Recoil,(int)aim::Ads}){if(k==aim::Ads&&!hasAds(r)){continue;}textAt(dc,verdictLine(r,k,p),{56,y,400,y+44},smallFont,ink,DT_LEFT|DT_WORDBREAK);y+=46;}
        textAt(dc,i18n::text(L"후보(0.95 / 1.00 / 1.05배)를 섞은 순서로 같은 조건에서 반복해 비교했습니다. 차이가 측정 오차보다 클 때만 추천합니다."),{56,456,400,530},smallFont,RGB(110,124,154),DT_LEFT|DT_WORDBREAK);}
    card(dc,{432,20,816,540});textAt(dc,i18n::text(L"경기 비교 (참고 통계)"),{452,32,800,58},boldFont,ink,one);
    double at=num(ap,"at");auto all=matches();
    if(!linked())textAt(dc,i18n::text(L"계정을 연결하면 볼 수 있습니다."),{452,66,800,92},smallFont,muted,one);
    else if(at<=0)textAt(dc,i18n::text(L"감도를 적용하면 그 시점 전후 경기를 비교합니다."),{452,66,800,92},smallFont,muted,one);
    else{std::vector<aim::Match> b,af;aim::split(all,at,10,b,af);int y=62;
        textAt(dc,fmt(L"적용 %ls · 전 %d경기 / 후 %d경기",dateOf(at).c_str(),(int)b.size(),(int)af.size()),{452,y,800,y+22},smallFont,muted,one);y+=22;
        if(ap.count("from.normal")){std::wstring was=fmt(L"일반 %ls · 수직 %ls",w16(str(ap,"from.normal")).c_str(),wfixed(num(ap,"from.vertical",1),2).c_str()),now=fmt(L"일반 %ls · 수직 %ls",w16(str(ap,ap.count("normal")?"normal":"from.normal")).c_str(),wfixed(num(ap,ap.count("vertical")?"vertical":"from.vertical",1),2).c_str());
            textAt(dc,fmt(L"감도 %ls → %ls",was.c_str(),now.c_str()),{452,y,800,y+22},smallFont,muted,one);}y+=30;
        // one row per weapon class: hit rate and damage per 100 shots, before -> after (only classes with data on both sides)
        textAt(dc,i18n::text(L"명중률"),{560,y,676,y+20},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);textAt(dc,i18n::text(L"100발당 피해"),{680,y,796,y+20},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);y+=24;
        static HBRUSH sep=CreateSolidBrush(RGB(30,46,84));const wchar_t* names[]={L"AR",L"SMG",L"DMR",L"LMG"};
        auto cell=[&](const aim::Change& c,RECT r,int digits,bool pct){std::wstring v=c.trend==aim::Few?L"–":wfixed(c.before.mean,digits)+(pct?L"%":L"")+L" → "+wfixed(c.after.mean,digits)+(pct?L"%":L"");
            COLORREF col=c.trend==aim::Up?RGB(96,210,150):c.trend==aim::Down?RGB(240,150,120):c.trend==aim::Few?muted:ink;textAt(dc,v,r,smallFont,col,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);};
        for(int k=-1;k<4;k++){painting::hairline(dc,{452,y,796,y+1},sep);textAt(dc,k<0?std::wstring(i18n::text(L"전체")):std::wstring(names[k]),{452,y+4,556,y+28},k<0?boldFont:smallFont,ink,one);
            cell(accChange(b,af,k),{556,y+4,676,y+28},1,true);cell(dmgChange(b,af,k),{680,y+4,796,y+28},0,false);y+=32;}
        y+=6;auto line=[&](const wchar_t* name,auto get){std::vector<double> x,z;for(auto& m:b)x.push_back(get(m));for(auto& m:af)z.push_back(get(m));auto c=aim::compare(x,z);
            textAt(dc,i18n::text(name),{452,y,620,y+22},smallFont,ink,one);textAt(dc,c.trend==aim::Few?std::wstring(i18n::text(L"데이터 부족")):wfixed(c.before.mean,2)+L" → "+wfixed(c.after.mean,2)+L" · "+trendWord(c.trend),{600,y,796,y+22},smallFont,ink,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);y+=24;};
        line(L"경기당 기절",[](const aim::Match& m){return (double)m.knocks;});line(L"경기당 처치",[](const aim::Match& m){return (double)m.kills;});
        double ride=0,veh=0,fall=0;for(auto& m:af){ride+=m.riderDamage;veh+=m.vehicleDamage;fall+=m.fallingDamage;}
        textAt(dc,fmt(L"적용 후 따로 센 피해 (추정): 탑승자 %ls · 차량 %ls · 낙하 중 %ls",wfixed(ride,0).c_str(),wfixed(veh,0).c_str(),wfixed(fall,0).c_str()),{452,y+4,800,y+42},smallFont,muted,DT_LEFT|DT_WORDBREAK);}
    textAt(dc,i18n::text(L"AR·SMG·DMR·LMG만 셉니다. 명중률은 발사 기록이 빠짐없는 경기만 씁니다. 경기마다 상대·모드·교전 거리가 달라 감도와 상관없이 달라질 수 있습니다."),{452,462,800,532},smallFont,RGB(110,124,154),DT_LEFT|DT_WORDBREAK);
    if(!note.empty()){textAt(dc,note,{232,550,624,600},smallFont,noteColor?noteColor:muted,DT_CENTER|DT_VCENTER|DT_WORDBREAK);}}
// Pro settings: what prosettings.net's PUBG list shows (read on 2026-11-16), page order. General = 일반, Vertical = 수직 배율,
// Aim = 조준 (Targeting), ADS = 조준경 (Scoping). The page's 2x-6x columns could not be read reliably, so they are left out.
// Empty strings: not on the page. No photos.
struct Pro {const wchar_t* name;const wchar_t* team;int dpi;double normal,vertical,aim,ads;const wchar_t* mouse;const wchar_t* monitor;};
const Pro pros[]={
    {L"TGLTN",L"Falcons Esports",1600,20,1.11,20,10,L"Logitech G Pro X3 Superstrike",L"ZOWIE XL2546"},{L"Gustav",L"Falcons Esports",800,32,1.30,32,32,L"SteelSeries Prime Mini Wireless",L"Acer Predator XB272"},
    {L"hwinn",L"Falcons Esports",950,30,1.0,27,27,L"ZOWIE EC3-DW",L"ZOWIE XL2546K"},{L"Shrimzy",L"Falcons Esports",800,33,1.11,33,28,L"Logitech G Pro X Superlight 2",L"ASUS ROG Swift PG258Q"},
    {L"Kickstart",L"Falcons Esports",800,30,1.11,30,25,L"Logitech G Pro Wireless",L"ZOWIE XL2546"},{L"Jeemzz",L"FaZe Clan",800,30,1.20,30,30,L"Logitech G Pro X Superlight",L"Alienware AW2518H"},
    {L"Fexx",L"FaZe Clan",400,50,1.00,50,50,L"Logitech G Pro X Superlight",L""},{L"mxey",L"FaZe Clan",800,35,1,32,32,L"Razer Viper V3 Pro",L"ZOWIE XL2540"},
    {L"Insight",L"Aurora",1600,15,1,15,15,L"ZOWIE EC2",L"Alienware AW2521H"},{L"ibiza",L"Virtus.pro",800,29,1.00,29,29,L"Logitech G Pro X Superlight",L"ZOWIE XL2546"},
    {L"luke12",L"Team Liquid",1600,15,.93,15,15,L"Logitech G Pro X Superlight",L"ZOWIE XL2546"},{L"aLOW",L"Team Liquid",400,56,1,51,50,L"ZOWIE EC1-B",L""},
    {L"ADOUZ1E",L"BetBoom Team",400,45,.85,38,38,L"Logitech G Pro X Superlight 2c",L"ZOWIE XL2546K"},{L"Bestoloch",L"BetBoom Team",800,35,1,35,30,L"Logitech G Pro X Superlight",L"ZOWIE XL2546"},
    {L"Fludd",L"BESTIA",1600,8,1,8,8,L"Razer Viper V4 Pro",L"ZOWIE XL2546"},{L"Himass",L"Anyone's Legend",400,33,.98,34,33,L"Logitech G Pro X2 Superstrike",L"ZOWIE XL2546K"},
    {L"Taikonn",L"Anyone's Legend",400,30,.98,30,30,L"Corsair M75 Wireless",L"ZOWIE XL2546K"},{L"Marcelek",L"PolishPower",800,43,.8,43,43,L"Razer Viper V3 Pro",L"ZOWIE XL2546"},
    {L"Uncivil",L"Nootropic",800,40,.85,36,36,L"Logitech G Pro X Superlight",L""},{L"lionkk",L"Petrichor Road",400,50,.7,50,50,L"ZOWIE FK2",L"ZOWIE XL2546"},
    {L"Aixleft",L"Petrichor Road",800,50,1.15,41,38,L"Logitech G Pro X Superlight",L"ZOWIE XL2586X+"},{L"Inonix",L"Crazy Raccoon",1600,16,.85,16,11,L"ROCCAT Kone Pure Ultra",L"ZOWIE XL2546"},
    {L"Braexco",L"Acend",800,35,1.25,32,32,L"Logitech G Pro X Superlight",L"AOC AG251FZ"},{L"Aitzy",L"Free Agent",800,25,1.15,25,25,L"ZOWIE EC2-A",L"ZOWIE XL2540"},
    {L"SKUIJKE",L"Free Agent",400,44,1.1,44,37,L"Razer Viper V3 Pro",L"ZOWIE XL2540"},{L"Under",L"Free Agent",800,32,1.1,32,32,L"ZOWIE FK1-B",L"ZOWIE XL2546K"},
    {L"Menteul",L"Free Agent",800,33,1,33,33,L"ZOWIE EC2-A",L"Alienware AW2720HF"},{L"LongSkr",L"Edward Gaming",400,48,.85,45,33,L"Logitech G Pro X Superlight",L"ZOWIE XL2546X+"},
    {L"CRAZY112",L"Four Angry Men",400,45,.85,43,41,L"Logitech G Pro X Superlight",L"ZOWIE XL2546"},{L"xmpl",L"Twisted Minds",400,46,1.00,45,33,L"Ninjutso Sora V2",L"ZOWIE XL2566X+"}};
const int proCount=(int)(sizeof(pros)/sizeof(*pros)),proRow=58,proTop=176,proBottom=496;
// pixel scroll: proY is drawn, proGoal is where the wheel / the bar wants it (eased toward on timer 8)
const bool proHelpHook=(helpClosed=[](int id){if(id==77&&page&&view==4)InvalidateRect(page,nullptr,FALSE);},true);
int proMode=0;const wchar_t* proModes[]={L"일반",L"수직",L"조준",L"조준경"};
const wchar_t* proHead(){static const wchar_t* h[]={L"cm/360 (추정)",L"수직 배율",L"조준 cm/360",L"조준경 cm/360"};return h[proMode];}
RECT proSeg(int k){const int w=78;return {816-4*w+k*w,20,816-4*w+(k+1)*w,52};}
double proVal(const Pro& r,int m){return m==1?r.vertical:aim::cm360(m==0?r.normal:m==2?r.aim:r.ads,r.dpi);}
double proRange(int m,double mine){if(m==1)return 1.5;double hi=mine;for(const Pro& r:pros)hi=std::max(hi,proVal(r,m));return std::ceil(hi*1.1/10)*10;}
// Sorting by a column (the header pressed): DPI and the settings highest first, then lowest; cm/360 closest to mine
// first, then farthest; the name A to Z, then Z to A. -1 = the source order.
int proSort=-1;bool proDesc=false;
const int proCx[]={56,316,374,432,490,548,612};
std::vector<int> proOrder(int m,double mine){std::vector<int> o;for(int i=0;i<proCount;i++)o.push_back(i);if(proSort<0)return o;
    auto key=[&](int i)->double{const Pro& r=pros[i];switch(proSort){case 1:return r.dpi;case 2:return r.normal;case 3:return r.vertical;case 4:return r.aim;case 5:return r.ads;case 6:return mine>0?std::fabs(proVal(r,m)-mine):proVal(r,m);default:return 0;}};
    std::stable_sort(o.begin(),o.end(),[&](int a,int b){if(proSort==0){int c=_wcsicmp(pros[a].name,pros[b].name);return proDesc?c>0:c<0;}double ka=key(a),kb=key(b);return proDesc?ka>kb:ka<kb;});return o;}
double proY=0,proGoal=0;int proHover=-1;bool proDrag=false,proBarHot=false,proHelpHot=false;int proGrab=0;
RECT proHelp(){SIZE z{};HDC d=GetDC(nullptr);auto o=SelectObject(d,smallFont);std::wstring t=i18n::text(proHead());if(proSort==6)t+=L" ▾";GetTextExtentPoint32W(d,t.c_str(),(int)t.size(),&z);SelectObject(d,o);ReleaseDC(nullptr,d);int x=std::min(612+(int)z.cx+6,770);return {x,146,x+24,170};} // the "?" after the cm/360 header
int proMax(){return std::max(0,proCount*proRow-(proBottom-proTop));}
RECT proBar(){int track=proBottom-proTop,th=std::max(36,track*track/std::max(1,proCount*proRow)),ty=proTop+(int)((track-th)*(proMax()?proY/proMax():0));return {800,ty,808,ty+th};}
COLORREF teamColor(const wchar_t* team){unsigned hsh=2166136261u;for(const wchar_t* c=team;*c;c++)hsh=(hsh^*c)*16777619u;
    static const COLORREF hues[]={RGB(72,118,255),RGB(46,170,200),RGB(130,96,240),RGB(220,120,60),RGB(60,180,130),RGB(210,80,120),RGB(200,170,60),RGB(110,140,190)};return hues[hsh%8];}
COLORREF mixc(COLORREF x,COLORREF y,double t){return RGB((int)(GetRValue(x)*(1-t)+GetRValue(y)*t),(int)(GetGValue(x)*(1-t)+GetGValue(y)*t),(int)(GetBValue(x)*(1-t)+GetBValue(y)*t));}
// text centred on its ink box: glyph metrics measured on a plain screen DC (design units, no world transform), then
// drawn from the baseline so the letters themselves sit in the middle
void inkCenter(HDC dc,const std::wstring& t,RECT r,HFONT f,COLORREF c){XFORM was{};bool sub=GetWorldTransform(dc,&was)!=0;double k=sub&&was.eM11>0?was.eM11:1;
    /* measured with the font at its real pixel size (hinting differs from the design size), then back to design units */
    static std::map<std::pair<HFONT,int>,HFONT> big;auto key=std::make_pair(f,(int)std::lround(k*100));HFONT mf=big[key];if(!mf){LOGFONTW lf{};GetObjectW(f,sizeof lf,&lf);lf.lfHeight=(LONG)std::lround(lf.lfHeight*k);lf.lfWidth=0;mf=big[key]=CreateFontIndirectW(&lf);}
    HDC sd=GetDC(nullptr);auto o=SelectObject(sd,mf);MAT2 m={{0,1},{0,0},{0,0},{0,1}};
    double pen=0,l=1e9,rr=-1e9,top=-1e9,bot=1e9;for(wchar_t ch:t){GLYPHMETRICS gm{};if(GetGlyphOutlineW(sd,ch,GGO_METRICS,&gm,0,nullptr,&m)==GDI_ERROR)continue;
        l=std::min(l,pen+gm.gmptGlyphOrigin.x);rr=std::max(rr,pen+gm.gmptGlyphOrigin.x+(double)gm.gmBlackBoxX);top=std::max(top,(double)gm.gmptGlyphOrigin.y);bot=std::min(bot,(double)gm.gmptGlyphOrigin.y-(double)gm.gmBlackBoxY);pen+=gm.gmCellIncX;}
    SelectObject(sd,o);ReleaseDC(nullptr,sd);l/=k;rr/=k;top/=k;bot/=k;if(l>rr){textAt(dc,t,r,f,c,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return;}
    double fx=(r.left+r.right)/2.0-(l+rr)/2,fy=(r.top+r.bottom)/2.0+(top+bot)/2;int x=(int)std::floor(fx),y=(int)std::floor(fy);
    /* the fraction left after the design unit goes into the scale transform dpi.h set, so the ink lands on the physical pixel */
    XFORM xf{};if(sub){xf=was;xf.eDx+=(FLOAT)((fx-x)*was.eM11);xf.eDy+=(FLOAT)((fy-y)*was.eM22);SetWorldTransform(dc,&xf);}
    auto of=SelectObject(dc,f);int bm=SetBkMode(dc,TRANSPARENT);COLORREF tc=SetTextColor(dc,c);UINT al=SetTextAlign(dc,TA_BASELINE|TA_LEFT);TextOutW(dc,x,y,t.c_str(),(int)t.size());SetTextAlign(dc,al);if(sub)SetWorldTransform(dc,&was);SetTextColor(dc,tc);SetBkMode(dc,bm);SelectObject(dc,of);}
// a player's mark: the team colour circle with the first two letters
void proAvatar(HDC dc,int x,int y,int d,const Pro& r,bool hot){COLORREF c=teamColor(r.team);painting::circle(dc,{x,y,x+d,y+d},mixc(c,RGB(10,16,34),.55),hot?RGB(150,180,255):mixc(c,RGB(240,244,255),.25),hot?2.f:1.2f);
    std::wstring in;for(const wchar_t* q=r.name;*q&&in.size()<2;q++)if(iswalnum(*q))in+=(wchar_t)towupper(*q);inkCenter(dc,in,{x,y,x+d,y+d},boldFont,RGB(240,244,255));}
// "45.5" big and " cm" small, right-aligned at right
void cmValue(HDC dc,double c,int right,int top,int bottom,COLORREF col,bool cm=true){SIZE sz{};if(cm){auto o=SelectObject(dc,smallFont);GetTextExtentPoint32W(dc,L"cm",2,&sz);SelectObject(dc,o);
    textAt(dc,L"cm",{right-sz.cx-2,top+3,right,bottom},smallFont,muted,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);sz.cx+=5;}textAt(dc,wfixed(c,cm?1:2),{right-sz.cx-90,top,right-sz.cx,bottom},boldFont,col,DT_RIGHT|DT_VCENTER|DT_SINGLELINE);}
// the gauge: 0..80 cm track, the fill, and my own place as a ringed dot
void cmGauge(HDC dc,int x,int w,int cy,double c,double mine,bool hot,double lo=0,double hi=80){auto at=[&](double v){return std::clamp((v-lo)/(hi-lo)*w,0.0,(double)w);};roundBox(dc,{x,cy-3,x+w,cy+3},RGB(22,34,66),RGB(22,34,66),6);int fw=(int)std::max(6.0,at(c));
    roundBox(dc,{x,cy-3,x+fw,cy+3},hot?RGB(120,160,255):RGB(52,92,255),hot?RGB(120,160,255):RGB(52,92,255),6);
    if(mine>0){int mx=x+(int)at(mine);painting::circle(dc,{mx-5,cy-5,mx+5,cy+5},RGB(240,244,255),RGB(13,22,44),2.f);}}
void paintPro(HDC dc){UINT one=DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS,mid=DT_CENTER|DT_VCENTER|DT_SINGLELINE;Profile p=profile();const int m=proMode;
    // mine on the pros' scale (the default turn rate, even after a calibration)
    double mine=m==0?aim::cm360c(p.conv,p.dpi):m==1?p.vertical:m==2?(p.targeting>0?aim::cm360(p.targeting,p.dpi):0):(p.ini.scoping>0?aim::cm360(p.ini.scoping,p.dpi):0);
    const bool cm=m!=1;const double lo=m==1?.5:0,hi=proRange(m,mine);
    textAt(dc,i18n::text(L"프로게이머 설정"),{36,20,400,52},sectionFont,ink,one);
    // which setting is compared: a segmented switch
    roundBox(dc,{proSeg(0).left-3,17,proSeg(3).right+3,55},RGB(14,24,48),line,19);
    for(int k=0;k<4;k++){RECT q=proSeg(k);bool on=k==m;if(on)roundBox(dc,q,RGB(52,92,255),RGB(52,92,255),16);textAt(dc,i18n::text(proModes[k]),q,on?boldFont:smallFont,on?RGB(255,255,255):muted,mid);}
    int closest=-1;double gap=1e9;if(mine>0)for(int i=0;i<proCount;i++){double c=proVal(pros[i],m);if(std::fabs(c-mine)<gap){gap=std::fabs(c-mine);closest=i;}}
    card(dc,{36,62,816,128});textAt(dc,i18n::text(L"내 감도"),{56,72,300,92},smallFont,muted,one);
    textAt(dc,fmt(L"DPI %d · 일반 %ls · 수직 %ls · 조준 %ls · 조준경 %ls",p.dpi,wfixed(p.normal,0).c_str(),wfixed(p.vertical,2).c_str(),p.targeting>0?wfixed(p.targeting,0).c_str():L"–",p.ini.scoping>0?wfixed(p.ini.scoping,0).c_str():L"–"),{56,94,440,118},boldFont,ink,one);
    if(mine>0){roundBox(dc,{440,84,590,118},RGB(20,32,64),RGB(52,92,255),17);textAt(dc,cm?std::wstring(L"cm/360"):std::wstring(i18n::text(L"수직")),{454,84,520,118},captionFont,muted,DT_LEFT|DT_VCENTER|DT_SINGLELINE);cmValue(dc,mine,578,84,118,RGB(240,244,255),cm);}
    if(closest>=0){const Pro& r=pros[closest];proAvatar(dc,606,76,40,r,true);textAt(dc,i18n::text(L"가장 비슷한 선수"),{654,72,796,92},smallFont,muted,one);
        textAt(dc,r.name,{654,94,746,118},boldFont,RGB(170,196,255),one);cmValue(dc,proVal(r,m),796,94,118,RGB(170,196,255),cm);}
    // the table: the compared setting's column in cobalt
    card(dc,{36,136,816,540});const int* cx=proCx;const wchar_t* head[]={L"선수",L"DPI",L"일반",L"수직",L"조준",L"조준경",proHead()};const int pick=2+m;auto ord=proOrder(m,mine);
    for(int i=0;i<7;i++){std::wstring t=i18n::text(head[i]);if(i==proSort)t+=proDesc?L" ▾":L" ▴";textAt(dc,t,{cx[i],146,i<6?cx[i+1]-4:796,170},i==proSort?smallBold:smallFont,i==proSort?ink:i==pick?RGB(140,176,255):muted,i==0||i==6?one:mid);}
    {RECT q=proHelp();bool on=proHelpHot||(helpCard&&helpFor==77);if(on)painting::iconGlow(dc,q,RGB(120,160,255),3,1.6,[&](HDC d,RECT){glyph(d,0xE9CE,q,footFont,RGB(255,255,255));});glyph(dc,0xE9CE,q,footFont,on?ink:muted);}
    static HBRUSH sep=CreateSolidBrush(RGB(30,46,84));painting::hairline(dc,{48,174,806,175},sep);
    int sv=SaveDC(dc);IntersectClipRect(dc,40,proTop,798,proBottom);
    int first=std::max(0,(int)(proY/proRow)),y0=proTop-(int)proY;
    for(int pos=first;pos<proCount;pos++){int y=y0+pos*proRow;if(y>=proBottom)break;int i=ord[pos];const Pro& r=pros[i];bool hot=i==closest,hov=pos==proHover;
        if(hot)roundBox(dc,{46,y+3,796,y+proRow-3},RGB(22,38,82),RGB(52,92,255),12);else if(hov)roundBox(dc,{46,y+3,796,y+proRow-3},RGB(26,42,82),RGB(42,64,116),12);else if(pos)painting::hairline(dc,{60,y,790,y+1},sep);
        proAvatar(dc,58,y+(proRow-36)/2,36,r,hot);
        textAt(dc,r.name,{104,y+8,cx[1]-6,y+30},boldFont,hot?RGB(170,196,255):ink,one);
        COLORREF tc=teamColor(r.team);SIZE tz{};auto o=SelectObject(dc,captionFont);GetTextExtentPoint32W(dc,r.team,(int)wcslen(r.team),&tz);SelectObject(dc,o);int tw=std::min((int)tz.cx+18,cx[1]-110);
        roundBox(dc,{104,y+33,104+tw,y+51},mixc(tc,RGB(10,16,34),.78),mixc(tc,RGB(10,16,34),.5),9);textAt(dc,r.team,{104,y+33,104+tw,y+51},captionFont,mixc(tc,RGB(240,244,255),.55),mid|DT_END_ELLIPSIS);
        const std::wstring cell[]={std::to_wstring(r.dpi),wfixed(r.normal,0),wfixed(r.vertical,2),wfixed(r.aim,0),wfixed(r.ads,0)};
        for(int k=0;k<5;k++)textAt(dc,cell[k],{cx[k+1],y,cx[k+2]-4,y+proRow},k+1==pick?boldFont:font,k+1==pick?RGB(170,196,255):ink,mid);
        double c=proVal(r,m);cmGauge(dc,cx[6],96,y+proRow/2,c,mine,hot,lo,hi);cmValue(dc,c,786,y,y+proRow,hot?RGB(170,196,255):ink,cm);}
    RestoreDC(dc,sv);
    if(painting::token){Gdiplus::Graphics g(dc);auto fade=[&](int top,int bottom,bool down){Gdiplus::LinearGradientBrush b(Gdiplus::Point(0,down?top:bottom),Gdiplus::Point(0,down?bottom:top),Gdiplus::Color(0,GetRValue(rowFill),GetGValue(rowFill),GetBValue(rowFill)),Gdiplus::Color(255,GetRValue(rowFill),GetGValue(rowFill),GetBValue(rowFill)));g.FillRectangle(&b,40,top,758,bottom-top);};
        if(proY>.5){fade(proTop,proTop+16,false);}if(proY<proMax()-.5){fade(proBottom-22,proBottom,true);}}
    if(proMax()>0){RECT t=proBar();bool on=proDrag||proBarHot;roundBox(dc,{802,proTop,806,proBottom},RGB(18,28,54),RGB(18,28,54),4);
        roundBox(dc,on?RECT{t.left,t.top,t.right,t.bottom}:RECT{t.left+2,t.top,t.right-2,t.bottom},on?RGB(110,150,255):RGB(60,90,170),on?RGB(110,150,255):RGB(60,90,170),6);}
    textAt(dc,fmt(L"%d명 · 출처 prosettings.net (2026-11-16 확인)",proCount),{56,506,796,530},smallFont,RGB(110,124,154),one);}
void proScrollTo(HWND h,double y,bool now){proGoal=std::clamp(y,0.0,(double)proMax());if(now){proY=proGoal;InvalidateRect(h,nullptr,FALSE);}else SetTimer(h,8,10,nullptr);}
int proRowAt(POINT pt){if(pt.x<46||pt.x>796||pt.y<proTop||pt.y>=proBottom)return -1;int i=(int)((pt.y-proTop+proY)/proRow);return i<proCount?i:-1;}
WNDPROC inputBase=nullptr;
// Enter in an input presses the step's button (연결 / 다음).
LRESULT CALLBACK inputProc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_KEYDOWN&&w==VK_RETURN){int id=GetDlgCtrlID(h)==513?514:526;PostMessageW(GetParent(h),WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),0);return 0;}if(m==WM_CHAR&&(w==L'\r'||w==L'\n'))return 0;return CallWindowProcW(inputBase,h,m,w,l);}
// Borderless and as tall as its font, placed in the middle of the box the page draws around it (fieldBox).
void input(HWND h,int id,int limit,bool digits){RECT b=fieldBox(id);int lh=28;HWND c=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_CENTER|ES_AUTOHSCROLL|(digits?ES_NUMBER:0),b.left+16,(b.top+b.bottom-lh)/2,b.right-b.left-32,lh,h,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    SendMessageW(c,WM_SETFONT,(WPARAM)sectionFont,TRUE);SendMessageW(c,EM_SETLIMITTEXT,limit,0);WNDPROC old=(WNDPROC)SetWindowLongPtrW(c,GWLP_WNDPROC,(LONG_PTR)inputProc);if(!inputBase)inputBase=old;
    if(digits)SendMessageW(c,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:{page=h;view=0;note.clear();
        HWND big=addSetting(h,L"BUTTON",L"감도 분석 시작",BS_OWNERDRAW|WS_TABSTOP,280,312,240,56,501);SetWindowLongPtrW(big,GWLP_USERDATA,1);
        pageButton(h,i18n::text(L"이 감도 적용 완료"),536,320,260,502,true);pageButton(h,i18n::text(L"계정 변경"),260,82,148,505);pageButton(h,i18n::text(L"DPI 변경"),656,82,148,506);
        pageButton(h,i18n::text(L"프로게이머 설정"),36,556,180,504);pageButton(h,i18n::text(L"자세히 보기"),636,556,180,503);
        pageButton(h,L"Steam",224,220,196,511);pageButton(h,L"Kakao",432,220,196,512);input(h,513,24,false);pageButton(h,i18n::text(L"연결"),224,394,404,514,true);pageButton(h,i18n::text(L"연결 없이 진행"),224,444,404,518);
        input(h,521,6,true);for(int i=0;i<4;i++)pageButton(h,std::to_wstring(dpiPresets[i]).c_str(),224+i*102,290,96,522+i);
        addSetting(h,L"EDIT",L"",WS_TABSTOP,224,358,196,32,527);addSetting(h,L"EDIT",L"",WS_TABSTOP,432,358,196,32,528);pageButton(h,i18n::text(L"다음"),224,416,404,526,true);
        pageButton(h,i18n::text(L"이 계정으로 계속"),226,356,400,516,true);
        
        {HWND rb=addSetting(h,L"BUTTON",i18n::text(L"결과 보기"),BS_OWNERDRAW|WS_TABSTOP,280,312,240,56,532);SetWindowLongPtrW(rb,GWLP_USERDATA,1);}pageButton(h,i18n::text(L"실전 기록"),536,320,140,571);pageButton(h,L"",596,556,220,572);pageButton(h,i18n::text(L"설정"),688,320,108,533);pageButton(h,i18n::text(L"이 감도 적용"),236,556,180,549,true);pageButton(h,i18n::text(L"자세한 분석"),436,556,180,570);
        pageButton(h,i18n::text(L"반응 속도"),170,482,116,534,true);pageButton(h,i18n::text(L"표적 반응"),294,482,116,535);
        for(int i=0;i<scopeCount;i++)pageButton(h,i18n::text(scopeNames[i]),332+i*77,96,69,540+i);
        pageButton(h,i18n::text(L"부착물 보기"),664,485,120,548);{HWND cb=addSetting(h,L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,332,164,240,320,547);SendMessageW(cb,CB_ADDSTRING,0,(LPARAM)i18n::text(L"모르는 총"));for(auto n:weaponNames)SendMessageW(cb,CB_ADDSTRING,0,(LPARAM)w16(n).c_str());}
        for(int i=0;i<6;i++)pageButton(h,keyName(recoilKeys[i]).c_str(),56+i*124,108,116,550+i);
        for(int i=0;i<scopeCount;i++){pageSwitch(h,i18n::text(scopeNames[i]),i<3?56:436,296+(i%3)*56,340,44,560+i);}pageButton(h,i18n::text(L"다른 계정으로 로그인"),226,406,400,517);
        if(!linked()&&num(load(L"profile.txt"),"noaccount")!=1){onboarding=true;setView(1);}else if(!linked()){afterAccount();}else{afterAccount();} /* a saved account is used until 계정 변경 */
#ifdef BLUEASH_TT
        recoilcap::demo(win);
#endif
        SetTimer(h,7,33,nullptr);recoilcap::onSaved=[](){pickedAt=0;if(page){if(view==7)setView(7);else InvalidateRect(page,nullptr,FALSE);}};refresh(false);return 0;}
    case WM_APP+240:setView(0);ShowWindow(win,SW_SHOW);SetForegroundWindow(win);return 0;
    case WM_APP+241:layout();return 0;
    case WM_APP+242:busy=false;if(wp==200||wp==503){{KV kv=load(L"profile.txt");if(kv.erase("noaccount"))save(L"profile.txt",kv);}refresh(true);afterAccount();if(wp==503)setNote(i18n::text(L"계정을 저장했습니다. 전적은 서버가 준비되면 자동으로 불러옵니다."),RGB(240,180,90));}
        else{setNote(i18n::text(wp==404?L"그 닉네임의 PUBG 계정을 찾지 못했습니다. 플랫폼과 대소문자를 확인해 주세요.":wp==429?L"요청이 많아 잠시 뒤에 다시 시도해 주세요.":wp==503?L"전적 서버가 아직 준비되지 않았습니다 (PUBG API 키 등록 대기).":L"서버에 연결하지 못했습니다."),danger);}return 0;
    case WM_APP+243:InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_APP+244:refreshing=false;if(wp==429)setNote(i18n::text(L"요청이 많아 잠시 뒤에 다시 시도해 주세요."),RGB(240,180,90));else if(wp!=200&&wp!=503&&view==3)setNote(i18n::text(L"서버에 연결하지 못했습니다."),danger); /* not ready yet: the account card already says so */InvalidateRect(h,nullptr,FALSE);return 0;
    case WM_COMMAND:{if(HIWORD(wp)==EN_CHANGE&&LOWORD(wp)==521){for(int i=522;i<=525;i++)InvalidateRect(GetDlgItem(h,i),nullptr,FALSE);return 0;}if(HIWORD(wp)==EN_SETFOCUS||HIWORD(wp)==EN_KILLFOCUS){RECT b=fieldBox(LOWORD(wp));InflateRect(&b,2,2);InvalidateRect(h,&b,FALSE);return 0;}
        if(LOWORD(wp)==547&&HIWORD(wp)==CBN_SELCHANGE){int sel=(int)SendMessageW(GetDlgItem(h,547),CB_GETCURSEL,0,0);auto all=recs();
            for(auto& r:all)if(r.at==pickedAt){r.weapon=sel>0?weaponNames[sel-1]:"";if(sel>0&&!r.icon.empty()){if(FILE* f=_wfopen((dir()+L"\\weapons.tsv").c_str(),L"ab")){fputs((r.weapon+"\t"+r.icon+"\n").c_str(),f);fclose(f);}}} /* the icon is remembered: next time this gun is told by it */
            saveRecs(all);InvalidateRect(h,nullptr,FALSE);return 0;}
        if(HIWORD(wp)!=BN_CLICKED){return 0;}int id=LOWORD(wp);if(id==503&&view==7)id=533;if(id==504&&view==2&&onboarding)id=505;else if(id==504&&view==8){setView(settingsFrom);return 0;}else if(id==504&&view==9)id=532;else if(id==504&&view==10){setView(7);return 0;}else if(id==504&&view==11){setView(0);return 0;}else if(id==504&&view!=0)id=510;else if(id==503&&view==3)id=507;switch(id){
        case 501:startAnalysis();break;
        case 502:{KV ap;Profile p=profile();KV r=load(L"result.txt");ap["at"]=std::to_string((long long)time(nullptr));ap["from.normal"]=fixed(p.normal,0);ap["from.vertical"]=fixed(p.vertical,2);ap["from.targeting"]=fixed(p.targeting,0);
            if(num(r,"flick.state",2)==aim::Better){ap["normal"]=fixed(aim::sliderFor(p.normal,num(r,"flick.mult",1)),0);}if(num(r,"recoil.state",2)==aim::Better){ap["vertical"]=fixed(p.vertical*num(r,"recoil.mult",1),2);}if(num(r,"ads.state",2)==aim::Better){ap["targeting"]=fixed(aim::sliderFor(p.targeting,num(r,"ads.mult",1)),0);}
            save(L"applied.txt",ap);Profile q=p;if(ap.count("normal"))q.normal=num(ap,"normal");if(ap.count("vertical"))q.vertical=num(ap,"vertical");remember(q,"apply");
            setNote(i18n::text(L"이 시점부터의 경기를 감도 적용 후로 비교합니다. 배그 설정에서 직접 바꿔 주세요."),RGB(96,210,150));layout();break;}
        case 503:setView(3);break;case 504:setView(4);break;case 505:setView(linked()&&view==0?6:1);break;case 506:setView(2);break;case 510:setView(0);break;
        case 516:afterAccount();break;
        case 530:case 531:homeTab=id-530;for(int i:{530,531})InvalidateRect(GetDlgItem(h,i),nullptr,FALSE);layout();break;
        case 532:setView(7);break;
        case 549:{auto all=recs();for(auto& r:all)if(r.at==pickedAt){KV ap;ap["at"]=std::to_string((long long)time(nullptr));ap["from.vertical"]=fixed(r.vm,2);double sv=steadyFor(all,r).value;ap["vertical"]=fixed(sv,2);Profile pf=profile();ap["from.normal"]=fixed(pf.normal,0);save(L"applied.txt",ap);Profile q2=pf;q2.vertical=sv;remember(q2,"apply");
            setNote(fmt(L"감도 적용 완료 · %ls",dateOf((double)time(nullptr)).c_str()),RGB(96,210,150));InvalidateRect(h,nullptr,FALSE);}break;} /* the moment the change starts: matches before and after are compared */case 548:attPick=-1;setView(9);break;case 570:setView(10);break;case 571:setView(11);break;case 572:{KV kv=load(L"profile.txt");kv["live"]=livecap::enabled()?"0":"1";save(L"profile.txt",kv);setView(11);break;}case 533:settingsFrom=view;setView(8);break;
        case 534:case 535:{Profile p=profile();aimprac::Options o;o.conv=p.conv;o.vertical=p.vertical;o.yaw=p.yaw;o.radius=radiusFor(id==535?12:p.distance);o.key=p.key;o.test=id==534?1:2;o.speed=num(load(L"profile.txt"),"rspeed",1);aimprac::onSpeed=[](double v){KV kv=load(L"profile.txt");kv["rspeed"]=fixed(v,2);save(L"profile.txt",kv);}; /* a bigger target on the still screen */
            aimprac::onReact=[](int test,double avg,double best,int n,int misses){KV kv=load(L"reaction.txt");std::string k=std::to_string(test);double was=num(kv,("best"+k).c_str());if(!was||avg<was)kv["best"+k]=fixed(avg,1);kv["last"+k]=fixed(avg,1);save(L"reaction.txt",kv);
                appendLine(L"reaction.tsv",std::to_string((long long)time(nullptr))+"\t"+k+"\t"+fixed(avg,1)+"\t"+fixed(best,1)+"\t"+std::to_string(n)+"\t"+std::to_string(misses));if(page)InvalidateRect(page,nullptr,FALSE);};
            if(!aimprac::open(page,o)){setNote(i18n::text(L"연습 화면을 열지 못했습니다."),danger);}break;}
        case 540:case 541:case 542:case 543:case 544:case 545:case 546:{auto all=recs();for(auto& r:all)if(r.at==pickedAt)r.scope=r.scope==scopeCodes[id-540]?"":scopeCodes[id-540];saveRecs(all);for(auto& r:all)if(r.at==pickedAt)recoilcap::learnScope(r.at,r.scope);for(int i=540;i<=546;i++)InvalidateRect(GetDlgItem(h,i),nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);break;}
        case 550:case 551:case 552:case 553:case 554:case 555:{KV kv=load(L"profile.txt");kv["rkey"]=std::to_string(recoilKeys[id-550]);save(L"profile.txt",kv);recoilcap::rekey(win);for(int i=550;i<=555;i++)InvalidateRect(GetDlgItem(h,i),nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);break;}
        case 560:case 561:case 562:case 563:case 564:case 565:case 566:{bool on=!checked(h,id);checkBox(h,id,on);KV kv=load(L"profile.txt");kv[std::string("auto.")+scopeCodes[id-560]]=on?"1":"0";save(L"profile.txt",kv);break;}
        case 518:{KV kv=load(L"profile.txt");kv["noaccount"]="1";save(L"profile.txt",kv);afterAccount();break;} /* the analysis needs no account; 계정 연결 stays on the home card */
        case 517:{KV a=account();DeleteFileW((dir()+L"\\account.txt").c_str());setView(1);SetWindowTextW(GetDlgItem(h,513),L"");pickShard=str(a,"shard").empty()?"steam":str(a,"shard");onboarding=true;layout();break;} /* this account's matches stay in their own file */case 507:refresh(true);InvalidateRect(h,nullptr,FALSE);break;
        case 511:pickShard="steam";InvalidateRect(GetDlgItem(h,511),nullptr,FALSE);InvalidateRect(GetDlgItem(h,512),nullptr,FALSE);break;
        case 512:pickShard="kakao";InvalidateRect(GetDlgItem(h,511),nullptr,FALSE);InvalidateRect(GetDlgItem(h,512),nullptr,FALSE);break;
        case 514:{wchar_t b[64]={};GetWindowTextW(GetDlgItem(h,513),b,64);std::wstring n=b;while(!n.empty()&&n.back()==L' ')n.pop_back();while(!n.empty()&&n[0]==L' ')n.erase(0,1);
            bool ok=n.size()>=2&&n.size()<=24;for(wchar_t c:n)if(!(iswalnum(c)&&c<128)&&c!=L'_'&&c!=L'-')ok=false;
            if(!ok){setNote(i18n::text(L"닉네임은 영문·숫자·_·- 2~24자입니다."),danger);break;}setNote(i18n::text(L"찾는 중…"),muted);connect(pickShard,n);break;}
        case 522:case 523:case 524:case 525:SetWindowTextW(GetDlgItem(h,521),std::to_wstring(dpiPresets[id-522]).c_str());break;
        case 526:{wchar_t b[32]={};GetWindowTextW(GetDlgItem(h,521),b,32);int d=_wtoi(b);if(d<100||d>32000){setNote(i18n::text(L"DPI는 100~32000 사이로 넣어 주세요."),danger);break;}
            KV kv=load(L"profile.txt");kv["dpi"]=std::to_string(d);Profile p=profile();
            if(!p.fromIni){GetWindowTextW(GetDlgItem(h,527),b,32);double nm=_wtof(b);GetWindowTextW(GetDlgItem(h,528),b,32);double vt=_wtof(b);if(nm<1||nm>100||vt<.5||vt>2){setNote(i18n::text(L"일반 감도는 1~100, 수직 배율은 0.5~2.0입니다."),danger);break;}kv["normal"]=fixed(nm,2);kv["vertical"]=fixed(vt,2);}
            if(!save(L"profile.txt",kv)){setNote(i18n::text(L"저장하지 못했습니다."),danger);break;}remember(profile(),"edit");onboarding=false;setView(0);break;}
        }return 0;}
    case WM_DRAWITEM:{auto* ds=(DRAWITEMSTRUCT*)lp;int id=(int)ds->CtlID;
        // the platform and type choices: the picked one drawn as the primary (cobalt) button
        if(id==530||id==531){SetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA,homeTab==id-530?1:0);}
        if(id>=540&&id<=546){std::string sc;for(auto& r:recs())if(r.at==pickedAt)sc=r.scope;SetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA,sc==scopeCodes[id-540]?1:0);}
        if(id>=550&&id<=555){SetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA,recoilcap::key()==recoilKeys[id-550]?1:0);}
        if(id==511||id==512||(id>=522&&id<=525)){wchar_t b[16]={};GetWindowTextW(GetDlgItem(h,521),b,16);bool on=id==511?pickShard=="steam":id==512?pickShard=="kakao":_wtoi(b)==dpiPresets[id-522];SetWindowLongPtrW(ds->hwndItem,GWLP_USERDATA,on?1:0);}
        drawButton(ds);return TRUE;}
    case WM_CTLCOLOREDIT:{int cid=GetDlgCtrlID((HWND)lp);if(cid==513||cid==521){static HBRUSH fb=CreateSolidBrush(well);SetTextColor((HDC)wp,ink);SetBkColor((HDC)wp,well);return (LRESULT)fb;}return pageStatic(wp,lp);}
    case WM_CTLCOLORSTATIC:return pageStatic(wp,lp);
    case WM_MOUSEWHEEL:if(view==4){proScrollTo(h,proGoal-GET_WHEEL_DELTA_WPARAM(wp)/120.0*proRow*2,false);return 0;}break;
    case WM_MOUSEMOVE:if(view==4){POINT pt={(short)LOWORD(lp),(short)HIWORD(lp)};TRACKMOUSEEVENT te{sizeof te,TME_LEAVE,h,0};TrackMouseEvent(&te);
        if(proDrag){int track=proBottom-proTop;RECT t=proBar();int th=t.bottom-t.top;if(track>th)proScrollTo(h,(double)(pt.y-proGrab-proTop)/(track-th)*proMax(),true);return 0;}
        RECT t=proBar(),q=proHelp();bool hot=pt.x>=796&&pt.x<=812&&pt.y>=t.top&&pt.y<=t.bottom,hq=PtInRect(&q,pt);RECT sw={proSeg(0).left,proSeg(0).top,proSeg(3).right,proSeg(3).bottom};if(PtInRect(&sw,pt)||(pt.y>=142&&pt.y<=174&&pt.x>=proCx[0]&&pt.x<proHelp().left-2))SetCursor(LoadCursorW(nullptr,IDC_HAND));int hv=proRowAt(pt);if(hot!=proBarHot||hv!=proHover||hq!=proHelpHot){proBarHot=hot;proHover=hv;proHelpHot=hq;SetCursor(LoadCursorW(nullptr,hq?IDC_HAND:IDC_ARROW));InvalidateRect(h,nullptr,FALSE);}return 0;}break;
    case WM_MOUSELEAVE:if(view==4&&(proHover>=0||proBarHot||proHelpHot)){proHover=-1;proBarHot=false;proHelpHot=false;InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:if(proDrag){proDrag=false;ReleaseCapture();InvalidateRect(h,nullptr,FALSE);}return 0;
    case WM_CAPTURECHANGED:if(proDrag&&(HWND)lp!=h){proDrag=false;InvalidateRect(h,nullptr,FALSE);}break;
    case WM_SETCURSOR:if(view==9){POINT c;GetCursorPos(&c);ScreenToClient(h,&c);for(int i=0;i<7;i++){RECT r=attRect(i);if(PtInRect(&r,c)){SetCursor(LoadCursorW(nullptr,IDC_HAND));return TRUE;}}}break;
    case WM_LBUTTONDOWN:{POINT pt={(short)LOWORD(lp),(short)HIWORD(lp)};
        if(view==9){for(int i=0;i<7;i++){RECT r=attRect(i);if(PtInRect(&r,pt)){attPick=i;InvalidateRect(h,nullptr,FALSE);return 0;}}}
        if(view==4){for(int k=0;k<4;k++){RECT sg=proSeg(k);if(PtInRect(&sg,pt)){if(proMode!=k){proMode=k;InvalidateRect(h,nullptr,FALSE);}return 0;}}}
        if(view==4&&pt.y>=142&&pt.y<=174){RECT q=proHelp();for(int k=0;k<7;k++){int x1=k<6?proCx[k+1]-4:q.left-2;if(pt.x>=proCx[k]&&pt.x<x1){if(proSort==k)proDesc=!proDesc;else{proSort=k;proDesc=k>=1&&k<=5;}proY=proGoal=0;InvalidateRect(h,nullptr,FALSE);return 0;}}}
        if(view==4){RECT q=proHelp();if(PtInRect(&q,pt)){POINT a={q.left,q.top},z={q.right,q.bottom};ClientToScreen(h,&a);ClientToScreen(h,&z);showHelpAt(77,{a.x,a.y,z.x,z.y});InvalidateRect(h,nullptr,FALSE);return 0;}}
        if(view==4&&proMax()>0&&pt.x>=796&&pt.x<=812&&pt.y>=proTop&&pt.y<proBottom){RECT t=proBar();int th=t.bottom-t.top;
            if(pt.y<t.top||pt.y>t.bottom){int track=proBottom-proTop;proScrollTo(h,(double)(pt.y-th/2-proTop)/std::max(1,track-th)*proMax(),true);t=proBar();}
            KillTimer(h,8);proGrab=pt.y-t.top;proDrag=true;SetCapture(h);InvalidateRect(h,nullptr,FALSE);return 0;}
        if(view==11){auto lm=liveMatches();for(int i=0;i<(int)lm.size()&&i<8;i++){RECT rr=listRow(i);if(PtInRect(&rr,pt)){livePick=lm[i].key;InvalidateRect(h,nullptr,FALSE);return 0;}}}
        if(view==7){auto all=recs();for(int i=0;i<(int)all.size()&&i<8;i++){RECT rr=listRow(i);if(PtInRect(&rr,pt)){pickedAt=all[i].at;setView(7);return 0;}}}for(int id:{513,521}){RECT b=fieldBox(id);if(IsWindowVisible(GetDlgItem(h,id))&&PtInRect(&b,pt))SetFocus(GetDlgItem(h,id));}return 0;}
    case WM_TIMER:if(wp==8){double d=proGoal-proY;if(std::fabs(d)<.6){proY=proGoal;KillTimer(h,8);}else proY+=d*.28;InvalidateRect(h,nullptr,FALSE);return 0;}
        if(wp==7&&IsWindowVisible(h)&&stillOk&&stillView==view){RECT a=motionRect();int w=a.right-a.left,hh=a.bottom-a.top;if(w>0){ /* a frame of motion: the still picture under motionRect plus the moving parts, straight to the screen (no WM_PAINT of the whole page) */
        HDC dc=dpi::getDC(h);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,w,hh);auto o=SelectObject(mem,bmp);SetViewportOrgEx(mem,-a.left,-a.top,nullptr);BitBlt(mem,a.left,a.top,w,hh,stillDC,a.left,a.top,SRCCOPY);paintMotion(mem);SetViewportOrgEx(mem,0,0,nullptr);BitBlt(dc,a.left,a.top,w,hh,mem,0,0,SRCCOPY);SelectObject(mem,o);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(h,dc);}}return 0;
    case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT c;GetClientRect(h,&c);
        // the still picture is kept; a frame of motion (only motionRect invalid) copies it and draws the moving parts on top
        if(!stillDC||stillW!=c.right||stillH!=c.bottom){if(stillDC){DeleteDC(stillDC);DeleteObject(stillBmp);}stillDC=CreateCompatibleDC(dc);stillBmp=CreateCompatibleBitmap(dc,std::max(1L,c.right),std::max(1L,c.bottom));SelectObject(stillDC,stillBmp);stillW=c.right;stillH=c.bottom;stillOk=false;}
        bool motionOnly=false;
        HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,std::max(1L,c.right),std::max(1L,c.bottom));auto old=SelectObject(mem,bmp);
        if(!motionOnly){HDC still=stillDC;eraseCarbon(h,still);if(view==0)paintHome(still);else if(view==1)paintAccount(still);else if(view==2)paintDpi(still);else if(view==3)paintDetail(still);else if(view==6)paintChoose(still);else if(view==7)paintRecoil(still);else if(view==8)paintRecoilSettings(still);else if(view==9)paintGrips(still);else if(view==10)paintAnalysis(still);else if(view==11)paintLive(still);else paintPro(still);stillOk=true;stillView=view;}
        BitBlt(mem,0,0,c.right,c.bottom,stillDC,0,0,SRCCOPY);paintMotion(mem);
        BitBlt(dc,0,0,c.right,c.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&ps);return 0;}
    case WM_ERASEBKGND:return 1;
    case WM_DESTROY:page=nullptr;busy=false;KillTimer(h,7);KillTimer(h,8);proDrag=false;proHover=-1;if(stillDC){DeleteDC(stillDC);DeleteObject(stillBmp);stillDC=nullptr;}return 0; // a refresh in flight keeps writing its file and stops posting
    case WM_CLOSE:DestroyWindow(h);return 0;
    }return DefWindowProcW(h,msg,wp,lp);}
// Insert from the mode hotkey (RegisterHotKey takes the key before the practice window sees it): while the practice
// screen is open it starts / stops the measurement instead of switching the mode.
inline bool takeHotkey(){if(!aimprac::active()||hotkeyVk!=aimprac::key())return false;aimprac::toggle();return true;}
}
