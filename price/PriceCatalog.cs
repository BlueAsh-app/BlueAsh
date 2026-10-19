using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
namespace ModeSwitchPrice {
 // PriceMax is set only on a merged entry (several items sharing one display name) whose prices differ.
 public class Item { public string Id{get;set;} public string Name{get;set;} public bool Banned{get;set;} public double? Price{get;set;} public double? PriceMax{get;set;} public double? Offers{get;set;} public string Updated{get;set;} public string Mode{get;set;} public string Category{get;set;} public string Ids{get;set;} public bool Grid{get;set;} public double? TraderPrice{get;set;} public string TraderId{get;set;} }
 // Per-mode price table. Lookups never wait for the network: a disk cache is served
 // immediately and refreshed on a below-normal background thread.
 public class Catalog {
  public static readonly string[] Modes={"pve","pvp-season","regular"};
  static readonly HttpClient http=MakeClient();
  internal static HttpClient Http{get{return http;}}
  static HttpClient MakeClient(){var h=new HttpClientHandler();h.AutomaticDecompression=DecompressionMethods.GZip|DecompressionMethods.Deflate;var c=new HttpClient(h);c.Timeout=TimeSpan.FromSeconds(30);c.MaxResponseContentBufferSize=60000000;c.DefaultRequestHeaders.UserAgent.ParseAdd("BlueAsh/2.8.1");return c;}
  class Snapshot { public Dictionary<string,Item> Exact; public Item[] Rows; public string[] Folded; public Dictionary<string,List<int>> ByCategory; public string[] Categories; public DateTime Time; }
  public readonly string Mode,Locale;readonly string cachePath,displayPath;volatile Snapshot snap;volatile Dictionary<string,string> displayNames;Task refresh;DateTime retryAt=DateTime.MinValue;
  public string Error{get;private set;}public string ErrorDetail{get;private set;}
  // locale = the game's text language (GameLanguage.Api): names in that language are what the titles are matched to.
  public Catalog(string mode,string cacheDir):this(mode,cacheDir,"ko"){}
  public Catalog(string mode,string cacheDir,string locale){if(Array.IndexOf(Modes,mode)<0)throw new ArgumentException("mode");Mode=mode;Locale=string.IsNullOrEmpty(locale)?"ko":locale;
   cachePath=Path.Combine(cacheDir,"price-"+mode+(Locale=="ko"?"":"."+Locale)+".tsv");displayPath=Path.Combine(cacheDir,"price-names-"+mode+"."+Language.ApiLocale+".json");}
  bool OwnDisplay{get{return Language.ApiLocale!=Locale;}}
  public bool Ready{get{return snap!=null;}}
  public bool Refreshing{get{return refresh!=null&&!refresh.IsCompleted;}}
  public int Count{get{var s=snap;return s==null?0:s.Rows.Length;}}
  public DateTime LoadedUtc{get{var s=snap;return s==null?DateTime.MinValue:s.Time;}}
  // Display names (BlueAsh's UI language) are separate from the game-language OCR catalog and never affect a match or price cache.
  volatile Dictionary<string,string> traderNames;
  string TraderPath{get{return Path.Combine(Path.GetDirectoryName(cachePath),"trader-names."+Language.ApiLocale+".json");}}
  public string TraderName(string id){var n=traderNames;string name;if(n==null&&File.Exists(TraderPath))try{traderNames=n=new JavaScriptSerializer().Deserialize<Dictionary<string,string>>(File.ReadAllText(TraderPath,Encoding.UTF8));}catch{}
   return n!=null&&id!=null&&n.TryGetValue(id,out name)?name:"";}
  public string DisplayName(Item item){if(item==null)return "";var names=displayNames;string name;return names!=null&&names.TryGetValue(item.Id,out name)?name:item.Name;}
  public void LoadDisplayNames(IList<Item> rows){var names=new Dictionary<string,string>();foreach(var r in rows)names[r.Id]=r.Name;displayNames=names;}

  public static List<Item> Parse(string itemsJson,string localeJson,string mode){
   var json=new JavaScriptSerializer();json.MaxJsonLength=int.MaxValue;json.RecursionLimit=64;
   // Case-sensitive dictionaries: the payload has keys differing only by case.
   var payload=json.DeserializeObject(itemsJson) as IDictionary;var locale=json.DeserializeObject(localeJson) as IDictionary;
   var data=payload==null?null:payload["data"] as IDictionary;var items=data==null?null:data["items"] as IDictionary;var names=locale==null?null:locale["data"] as IDictionary;
   if(items==null||names==null)throw new Exception(Language.T("API 응답 형식 불일치 (items/번역)"));
   // Handbook leaf category per item (the game shows its English path, e.g. "Weapons > Throwables").
   var handbook=data["handbookCategories"] as IDictionary;
   var result=new List<Item>();var hex=new Regex("^[a-fA-F0-9]{24}$");var hexName=new Regex("^[a-fA-F0-9]{24}\\s");
   foreach(DictionaryEntry e in items){
    var v=e.Value as IDictionary;if(v==null)continue;
    var id=v.Contains("id")&&v["id"]!=null?Convert.ToString(v["id"]):Convert.ToString(e.Key);if(!hex.IsMatch(id))continue;
    var key=v.Contains("name")?Convert.ToString(v["name"]):null;var name=key!=null&&names.Contains(key)?Convert.ToString(names[key]):null;
    if(string.IsNullOrEmpty(name)&&key!=null&&!hexName.IsMatch(key))name=key;if(string.IsNullOrEmpty(name))continue;
    var types=v.Contains("types")?v["types"] as IEnumerable:null;if(types==null)continue;
    bool banned=false;foreach(var t in types)if("noFlea".Equals(t))banned=true;string tid;
    result.Add(new Item{Id=id,Name=name,Banned=banned,Price=Number(v,"lastLowPrice"),Offers=Number(v,"lastOfferCount"),Updated=v.Contains("updated")?Convert.ToString(v["updated"]):"",Mode=mode,Category=Leaf(v,handbook),Grid=v.Contains("hasGrid")&&true.Equals(v["hasGrid"]),TraderPrice=BestTrader(v,out tid),TraderId=tid});
   }
   if(result.Count<1)throw new Exception(Language.T("API에서 유효한 아이템을 읽지 못함"));
   return result;
  }
  // The trader paying the most for the item (tarkov.dev sellToTrader, in roubles): shown when the flea market has no number.
  static double? BestTrader(IDictionary v,out string id){id="";double? best=null;var list=v.Contains("sellToTrader")?v["sellToTrader"] as IEnumerable:null;if(list==null)return null;
   foreach(var o in list){var t=o as IDictionary;if(t==null||!t.Contains("priceRUB")||t["priceRUB"]==null)continue;double p=Convert.ToDouble(t["priceRUB"],System.Globalization.CultureInfo.InvariantCulture);if(p>0&&(best==null||p>best)){best=p;id=Convert.ToString(t["trader"]);}}return best;}
  static string Leaf(IDictionary v,IDictionary handbook){
   var list=v.Contains("handbookCategories")?v["handbookCategories"] as IList:null;if(list==null||list.Count==0||handbook==null)return "";
   var c=handbook[Convert.ToString(list[0])] as IDictionary;return c==null?"":CategoryKey(Convert.ToString(c["normalizedName"]));
  }
  // "throwables" (API) and OCR "ThrowabIes" both become letters-only, OCR-folded keys so they compare alike.
  public static string CategoryKey(string s){return Core.Fold(Regex.Replace(s??"","[^\\p{L}]",""));}
  static double? Number(IDictionary v,string k){if(!v.Contains(k)||v[k]==null)return null;return Convert.ToDouble(v[k],System.Globalization.CultureInfo.InvariantCulture);}

  // Items sharing a display name (423 of ~5,500: event cases, dog tags, ammo pack variants) cannot be told
  // apart from the title, so they merge into one entry: banned only if all are, lowest live price, and
  // PriceMax when the prices differ by more than 15% so the overlay shows a range instead of guessing.
  static string JoinIds(IList<Item> same){var sb=new StringBuilder();foreach(var r in same){if(sb.Length>0)sb.Append(',');sb.Append(r.Id);}return sb.ToString();}
  public static Item Merge(IList<Item> same){
   if(same.Count==1)return same[0];
   double? min=null,max=null,offers=null;bool banned=true;string updated="";
   foreach(var r in same){
    if(!r.Banned)banned=false;if(r.Offers.HasValue)offers=(offers??0)+r.Offers.Value;if(string.Compare(r.Updated,updated,StringComparison.Ordinal)>0)updated=r.Updated;
    bool live=!r.Banned&&r.Price.HasValue&&r.Price.Value>0&&(!r.Offers.HasValue||r.Offers.Value>0);
    if(live){if(min==null||r.Price<min)min=r.Price;if(max==null||r.Price>max)max=r.Price;}
   }
   return new Item{Id=same[0].Id,Name=same[0].Name,Banned=banned,Price=min,PriceMax=max.HasValue&&min.HasValue&&max.Value>min.Value*1.15?max:null,
    Offers=min.HasValue?offers:0,Updated=updated,Mode=same[0].Mode,Category=same[0].Category,Ids=JoinIds(same),Grid=same[0].Grid,TraderPrice=same[0].TraderPrice,TraderId=same[0].TraderId};
  }
  static Snapshot Build(IList<Item> rows,DateTime time){
   var groups=new Dictionary<string,List<Item>>();
   foreach(var r in rows){var k=Core.Normalize(r.Name);List<Item> g;if(!groups.TryGetValue(k,out g))groups[k]=g=new List<Item>();g.Add(r);}
   var exact=new Dictionary<string,Item>();var unique=new List<Item>();
   foreach(var g in groups){var m=Merge(g.Value);exact[g.Key]=m;unique.Add(m);}
   var folded=new string[unique.Count];for(int i=0;i<folded.Length;i++)folded[i]=Core.Fold(unique[i].Name);
   var byCat=new Dictionary<string,List<int>>();
   for(int i=0;i<unique.Count;i++){var c=unique[i].Category??"";if(c.Length==0)continue;List<int> l;if(!byCat.TryGetValue(c,out l))byCat[c]=l=new List<int>();l.Add(i);}
   var cats=new string[byCat.Count];byCat.Keys.CopyTo(cats,0);
   return new Snapshot{Exact=exact,Rows=unique.ToArray(),Folded=folded,ByCategory=byCat,Categories=cats,Time=time};
  }
  public void Load(IList<Item> rows,DateTime time){snap=Build(rows,time);}

  // Exact normalized title first; otherwise one unambiguous OCR-tolerant near match. Duplicate names never match.
  public Item Find(string text){
   var s=snap;if(s==null||string.IsNullOrEmpty(text))return null;Item hit;
   if(s.Exact.TryGetValue(Core.Normalize(text),out hit))return hit;
   int i=Core.Closest(text,s.Folded);if(i>=0)return s.Rows[i];
   // Long titles can be cut with an ellipsis: accept the single name that starts with the visible part.
   var t=text.TrimEnd();
   if(t.EndsWith("...")||t.EndsWith("…")){var prefix=Core.Fold(t.TrimEnd('.','…'));if(prefix.Length<6)return null;
    int found=-1;for(int j=0;j<s.Folded.Length;j++)if(s.Folded[j].StartsWith(prefix,StringComparison.Ordinal)){if(found>=0)return null;found=j;}
    return found<0?null:s.Rows[found];}
   // Only part of the title was read (its start cut off on screen): accept the single item whose name contains
   // it. At least 6 characters with 3 letters, so a lone word shared by several items matches nothing.
   var part=Core.Fold(t);int letters=0;foreach(var ch in part)if(char.IsLetter(ch))letters++;if(part.Length<6||letters<3)return null;
   Item one=null;for(int j=0;j<s.Folded.Length;j++)if(s.Folded[j].Contains(part)){if(one!=null&&!ReferenceEquals(one,s.Rows[j]))return null;one=s.Rows[j];}
   return one;
  }

  // Second chance when the title read badly ("F-I 丁五" for "F-1 수류탄"): restrict to the item's handbook
  // category read from the line under the title ("Weapons > Throwables" -> last segment), then pick the one
  // name whose characters appear, in order, the most in the reading. Only an unambiguous winner counts.
  public Item FindInCategory(string text,string categoryLine){
   var s=snap;if(s==null||string.IsNullOrEmpty(text)||string.IsNullOrEmpty(categoryLine))return null;
   var parts=categoryLine.Split('>','〉','›','»');var cat=CategoryKey(parts[parts.Length-1]);if(cat.Length<3)return null;
   List<int> pool;
   if(!s.ByCategory.TryGetValue(cat,out pool)){int c=Core.Closest(cat,s.Categories);if(c<0||!s.ByCategory.TryGetValue(s.Categories[c],out pool))return null;}
   // Han characters never occur in Korean item names: they are misreads of Hangul, drop them.
   var read=Locale!="ko"?Core.Fold(text):Core.Fold(Regex.Replace(text,"[\\p{IsCJKUnifiedIdeographs}\\p{IsCJKSymbolsandPunctuation}]",""));if(read.Length<2)return null;
   int best=-1;double bestScore=0,second=0;
   foreach(int i in pool){var name=s.Folded[i];if(name.Length==0)continue;int common=Core.CommonInOrder(read,name);if(common<2)continue;
    double score=(double)common/name.Length;if(score>bestScore){second=bestScore;bestScore=score;best=i;}else if(score>second)second=score;}
   return best>=0&&bestScore>=0.34&&bestScore-second>=0.2?s.Rows[best]:null;
  }
  public bool LoadCache(){
   try{
    if(OwnDisplay&&File.Exists(displayPath))try{displayNames=new JavaScriptSerializer{MaxJsonLength=int.MaxValue}.Deserialize<Dictionary<string,string>>(File.ReadAllText(displayPath,Encoding.UTF8));}catch{}
    if(!File.Exists(cachePath))return false;var lines=File.ReadAllLines(cachePath,Encoding.UTF8);
    var head=lines.Length>0?lines[0].Split('\t'):new string[0];if(head.Length!=3||(head[0]!="v2"&&head[0]!="v3"&&head[0]!="v4")||head[1]!=Mode)return false;
    var rows=new List<Item>();
    for(int i=1;i<lines.Length;i++){var f=lines[i].Split('\t');if(f.Length<7)continue;rows.Add(new Item{Id=f[0],Name=f[1],Banned=f[2]=="1",Price=Opt(f[3]),Offers=Opt(f[4]),Updated=f[5],Mode=Mode,Category=f[6],Grid=f.Length>7&&f[7]=="1",TraderPrice=f.Length>8?Opt(f[8]):null,TraderId=f.Length>9?f[9]:""});}
    if(rows.Count<1)return false;Load(rows,head[0]!="v4"?DateTime.MinValue:new DateTime(long.Parse(head[2]),DateTimeKind.Utc));return true;
   }catch{return false;}
  }
  static double? Opt(string s){double d;return double.TryParse(s,System.Globalization.NumberStyles.Float,System.Globalization.CultureInfo.InvariantCulture,out d)?d:(double?)null;}
  static string Cell(string s){return (s??"").Replace('\t',' ').Replace('\r',' ').Replace('\n',' ');}
  static string Num(double? d){return d.HasValue?d.Value.ToString("R",System.Globalization.CultureInfo.InvariantCulture):"";}
  void SaveCache(IList<Item> rows,DateTime time){
   var sb=new StringBuilder();sb.Append("v4\t").Append(Mode).Append('\t').Append(time.Ticks).Append('\n');
   foreach(var r in rows)sb.Append(Cell(r.Id)).Append('\t').Append(Cell(r.Name)).Append('\t').Append(r.Banned?"1":"0").Append('\t').Append(Num(r.Price)).Append('\t').Append(Num(r.Offers)).Append('\t').Append(Cell(r.Updated)).Append('\t').Append(Cell(r.Category)).Append('\t').Append(r.Grid?"1":"0").Append('\t').Append(Num(r.TraderPrice)).Append('\t').Append(Cell(r.TraderId)).Append('\n');
   var temp=cachePath+".tmp";File.WriteAllText(temp,sb.ToString(),new UTF8Encoding(false));if(File.Exists(cachePath))File.Replace(temp,cachePath,null);else File.Move(temp,cachePath);
  }

  // Starts at most one background download when the table is missing or older than maxAge. Never blocks.
  public void RefreshIfStale(TimeSpan maxAge){
   var now=DateTime.UtcNow;if(Refreshing||now<retryAt)return;if(snap!=null&&now-snap.Time<maxAge)return;
   refresh=Task.Run(()=>{
    var thread=Thread.CurrentThread;var priority=thread.Priority;thread.Priority=ThreadPriority.BelowNormal;
    try{
     var root="https://json.tarkov.dev/"+Mode+"/";
     var items=http.GetStringAsync(root+"items").Result;var rows=Parse(items,http.GetStringAsync(root+"items_"+Locale).Result,Mode);
     var time=DateTime.UtcNow;snap=Build(rows,time);Error=null;ErrorDetail=null;
     try{SaveCache(rows,time);}catch{}
     try{var js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};var tr=(js.DeserializeObject(http.GetStringAsync(root+"traders").Result) as IDictionary)["data"] as IDictionary;var loc=(js.DeserializeObject(http.GetStringAsync(root+"traders_"+Language.ApiLocale).Result) as IDictionary)["data"] as IDictionary;
      var names=new Dictionary<string,string>();foreach(DictionaryEntry e in tr){var t=e.Value as IDictionary;if(t==null)continue;var key=Convert.ToString(t["name"]);names[Convert.ToString(t.Contains("id")?t["id"]:e.Key)]=loc!=null&&loc.Contains(key)?Convert.ToString(loc[key]):key;}
      traderNames=names;File.WriteAllText(TraderPath,js.Serialize(names),new UTF8Encoding(false));}catch{}
     if(OwnDisplay)try{LoadDisplayNames(Parse(items,http.GetStringAsync(root+"items_"+Language.ApiLocale).Result,Mode));File.WriteAllText(displayPath,new JavaScriptSerializer{MaxJsonLength=int.MaxValue}.Serialize(displayNames),new UTF8Encoding(false));}catch{}
    }catch(Exception e){var inner=e is AggregateException&&e.InnerException!=null?e.InnerException:e;ErrorDetail=inner.Message;Error=Language.T("시세 데이터 갱신 실패 · {0}",Language.Error(inner));retryAt=DateTime.UtcNow.AddMinutes(1);}
    finally{thread.Priority=priority;GC.Collect();}
   });
  }
 }
 // Which quests need an item: tarkov.dev tasks (hand over / find / plant / use a specific item, or a key the
 // quest needs), with Korean quest and trader names. "Any of 50 medical items" objectives are skipped: they
 // would tag half the stash. Cached per mode, refreshed in the background every 6 h like prices.
 public class TaskIndex {
  public readonly string Mode;readonly string cachePath;volatile Dictionary<string,List<string>> byItem;Task refresh;DateTime loaded=DateTime.MinValue,retryAt=DateTime.MinValue;
  public string Error{get;private set;}
  public TaskIndex(string mode,string cacheDir){Mode=mode;cachePath=Path.Combine(cacheDir,"tasks-"+mode+(Language.Code=="ko"?"":"."+Language.ApiLocale)+".tsv");}
  public bool Ready{get{return byItem!=null;}}
  public bool Refreshing{get{return refresh!=null&&!refresh.IsCompleted;}}
  const int MaxItemsPerObjective=5;
  static string Str(object o){return o==null?"":Convert.ToString(o,System.Globalization.CultureInfo.InvariantCulture);}
  public static Dictionary<string,List<string>> Build(string tasksJson,string tasksKoJson,string tradersJson,string tradersKoJson){
   var json=new JavaScriptSerializer();json.MaxJsonLength=int.MaxValue;json.RecursionLimit=100;
   var data=(json.DeserializeObject(tasksJson) as IDictionary)["data"] as IDictionary;var tasks=data==null?null:data["tasks"] as IDictionary;
   var ko=(json.DeserializeObject(tasksKoJson) as IDictionary)["data"] as IDictionary;
   var traders=(json.DeserializeObject(tradersJson) as IDictionary)["data"] as IDictionary;var traderKo=(json.DeserializeObject(tradersKoJson) as IDictionary)["data"] as IDictionary;
   if(tasks==null||ko==null)throw new Exception(Language.T("퀘스트 데이터 형식 불일치"));
   // item id -> task id -> (label, what): one line per quest per item; the larger count wins.
   var need=new Dictionary<string,Dictionary<string,KeyValuePair<string,string>>>();
   Action<string,string,string,string> add=(item,task,label,what)=>{Dictionary<string,KeyValuePair<string,string>> m;if(!need.TryGetValue(item,out m))need[item]=m=new Dictionary<string,KeyValuePair<string,string>>();
    KeyValuePair<string,string> old;if(!m.TryGetValue(task,out old)||what.Length>old.Value.Length)m[task]=new KeyValuePair<string,string>(label,what);};
   foreach(DictionaryEntry e in tasks){
    var t=e.Value as IDictionary;if(t==null)continue;string tid=Str(t["id"]),nameKey=Str(t["name"]),name=ko.Contains(nameKey)?Str(ko[nameKey]):"";if(name.Length==0)continue;
    string trader="";var tr=traders!=null?traders[Str(t["trader"])] as IDictionary:null;if(tr!=null&&traderKo!=null&&traderKo.Contains(Str(tr["name"])))trader=Str(traderKo[Str(tr["name"])]);
    string label=name+(trader.Length>0?" ("+trader+")":"")+(true.Equals(t["kappaRequired"])?" · "+Language.T("카파"):"");
    foreach(var o in (t["objectives"] as IEnumerable)??new object[0]){
     var ob=o as IDictionary;if(ob==null)continue;string type=Str(ob["type"]);var items=ob.Contains("items")?ob["items"] as IList:null;if(items==null||items.Count==0||items.Count>MaxItemsPerObjective)continue;
     string count=Str(ob["count"]);bool fir=true.Equals(ob["foundInRaid"]);string what;
     if(type=="giveItem"||type=="findItem")what=(count.Length>0&&count!="0"?Language.T("{0}개",count):"")+(fir?" · "+Language.T("인레이드"):"");
     else if(type=="plantItem")what=Language.T("설치")+(count.Length>0&&count!="1"?" "+Language.T("{0}개",count):"");
     else if(type=="useItem")what=Language.T("사용");else continue;
     foreach(var id in items)add(Str(id),tid,label,what.Trim());
    }
    foreach(var k in (t["neededKeys"] as IEnumerable)??new object[0]){var kd=k as IDictionary;var keys=kd!=null?kd["keys"] as IEnumerable:null;if(keys==null)continue;foreach(var id in keys)add(Str(id),tid,label,Language.T("열쇠 필요"));}
   }
   var result=new Dictionary<string,List<string>>();
   // Each line carries its quest id ("<id>\x1f<text>") so completed quests can be left out (For).
   foreach(var p in need){var lines=new List<string>();foreach(var q in p.Value)lines.Add(q.Key+"\u001f"+(q.Value.Value.Length>0?q.Value.Key+" · "+q.Value.Value:q.Value.Key));
    lines.Sort((a,b)=>string.CompareOrdinal(a.Substring(a.IndexOf('\u001f')+1),b.Substring(b.IndexOf('\u001f')+1)));result[p.Key]=lines;}
   return result;
  }
  // Hideout: per item, which station levels need it (raw, so lines can follow the player's own levels).
  public class HideNeed{public string Station,Name;public int Level,Count;public bool Fir;}
  public class Station{public string Id{get;set;}public string Name{get;set;}public int MaxLevel{get;set;}}
  public class HideoutData{public Dictionary<string,List<HideNeed>> Needs=new Dictionary<string,List<HideNeed>>();public List<Station> Stations=new List<Station>();}
  // Bright = needed for the next upgrade (or levels unknown); dim = only for later upgrades.
  public class HideLine{public string Text{get;set;}public bool Now{get;set;}}
  volatile HideoutData hideout;
  public Station[] Stations{get{var h=hideout;return h==null?new Station[0]:h.Stations.ToArray();}}
  public static HideoutData BuildHideout(string hideoutJson,string hideoutKoJson){
   var json=new JavaScriptSerializer();json.MaxJsonLength=int.MaxValue;json.RecursionLimit=100;
   var stations=(json.DeserializeObject(hideoutJson) as IDictionary)["data"] as IDictionary;var ko=(json.DeserializeObject(hideoutKoJson) as IDictionary)["data"] as IDictionary;
   if(stations==null||ko==null)throw new Exception(Language.T("은신처 데이터 형식 불일치"));
   var result=new HideoutData();
   foreach(DictionaryEntry e in stations){
    var s=e.Value as IDictionary;if(s==null)continue;string id=Str(s["id"]),key=Str(s["name"]),name=ko.Contains(key)?Str(ko[key]):"";if(name.Length==0||id.Length==0)continue;int max=0;
    foreach(var lv in (s["levels"] as IEnumerable)??new object[0]){var l=lv as IDictionary;if(l==null)continue;int level;int.TryParse(Str(l["level"]),out level);max=Math.Max(max,level);
     foreach(var r in (l["itemRequirements"] as IEnumerable)??new object[0]){var req=r as IDictionary;if(req==null)continue;string item=Str(req["item"]);int count;int.TryParse(Str(req["count"]),out count);if(item.Length==0)continue;
      // attributes is {"foundInRaid": true|false} (older payloads: a list of {name,value}).
      bool fir=false;var attrMap=req["attributes"] as IDictionary;
      if(attrMap!=null){if(attrMap.Contains("foundInRaid"))fir=Str(attrMap["foundInRaid"]).Equals("true",StringComparison.OrdinalIgnoreCase);}
      else{var attrs=req["attributes"] as IEnumerable;if(attrs!=null)foreach(var a in attrs){var ad=a as IDictionary;if(ad!=null&&Str(ad["name"])=="foundInRaid"&&Str(ad["value"]).Equals("true",StringComparison.OrdinalIgnoreCase))fir=true;}}
      List<HideNeed> list;if(!result.Needs.TryGetValue(item,out list))result.Needs[item]=list=new List<HideNeed>();list.Add(new HideNeed{Station=id,Name=name,Level=level,Count=count,Fir=fir});}}
    result.Stations.Add(new Station{Id=id,Name=name,MaxLevel=max});
   }
   result.Stations.Sort((a,b)=>string.CompareOrdinal(a.Name,b.Name));
   return result;
  }
  // "Lv.1,2,3 (2·3·5개)" / "(각 3개)" / "Lv.2 (3개 필요)" for one level.
  static string Levels(List<HideNeed> l,bool needWord){
   var lv=new StringBuilder();var counts=new StringBuilder();bool same=true;foreach(var n in l){if(lv.Length>0){lv.Append(',');counts.Append('·');}lv.Append(n.Level);counts.Append(n.Count);if(n.Count!=l[0].Count)same=false;}
   if(l.Count==1)return Language.T(needWord?"Lv.{0} ({1}개 필요)":"Lv.{0} ({1}개)",lv,l[0].Count);
   return same?Language.T("Lv.{0} (각 {1}개)",lv,l[0].Count):Language.T("Lv.{0} ({1}개)",lv,counts);
  }
  // levels: the player's station levels for this mode (station id -> built level), or null when not entered.
  public HideLine[] Hideout(string ids,IDictionary<string,int> levels){return Hideout(ids,levels,null);}
  // skip: stations already finished regardless of the entered levels (the stash of the top game editions).
  public HideLine[] Hideout(string ids,IDictionary<string,int> levels,ICollection<string> skip){
   var h=hideout;if(h==null||string.IsNullOrEmpty(ids))return new HideLine[0];
   var byStation=new Dictionary<string,List<HideNeed>>();var order=new List<string>();
   foreach(var id in ids.Split(',')){List<HideNeed> l;if(!h.Needs.TryGetValue(id,out l))continue;
    foreach(var n in l){if(skip!=null&&skip.Contains(n.Station))continue;List<HideNeed> s;if(!byStation.TryGetValue(n.Station,out s)){byStation[n.Station]=s=new List<HideNeed>();order.Add(n.Station);}if(!s.Exists(x=>x.Level==n.Level))s.Add(n);}}
   var lines=new List<HideLine>();
   foreach(var st in order){var all=byStation[st];all.Sort((a,b)=>a.Level-b.Level);string name=all[0].Name;bool fir=all.Exists(x=>x.Fir);string tag=fir?" · "+Language.T("인레이드"):"";
    if(levels==null){lines.Add(new HideLine{Text=name+" "+Levels(all,true)+tag,Now=true});continue;}
    int built;levels.TryGetValue(st,out built);var left=all.FindAll(x=>x.Level>built);if(left.Count==0)continue;
    var next=left.FindAll(x=>x.Level==built+1);var later=left.FindAll(x=>x.Level>built+1);
    if(next.Count>0)lines.Add(new HideLine{Text=name+" "+Levels(next,true)+(later.Count>0?" · "+Language.T("이후 {0}",Levels(later,false)):"")+tag,Now=true});
    else lines.Add(new HideLine{Text=name+" "+Levels(later,false)+" · "+Language.T("나중에")+tag,Now=false});
   }
   lines.Sort((a,b)=>a.Now!=b.Now?(a.Now?-1:1):string.CompareOrdinal(a.Text,b.Text));
   return lines.ToArray();
  }
  // The stash comes fully upgraded with the Edge of Darkness and Unheard editions. The game's settings name the
  // member category the player shows ("UniqueId" = EOD, "Unheard"), which only owners can pick; others say "Default".
  public const string StashId="5d484fc0654e76006657e0ab";
  // The Cultist Circle comes already built with the Unheard edition only (other editions build it).
  public const string CultistId="667298e75ea6b4493c08f266";
  public static bool CultistByEdition(string gameIniPath){
   try{if(!File.Exists(gameIniPath))return false;var m=System.Text.RegularExpressions.Regex.Match(File.ReadAllText(gameIniPath),"\"SelectedMemberCategory\"\\s*:\\s*\"(\\w+)\"");return m.Success&&m.Groups[1].Value=="Unheard";}catch{return false;}
  }
  public static bool StashMaxedByEdition(string gameIniPath){
   try{if(!File.Exists(gameIniPath))return false;var m=System.Text.RegularExpressions.Regex.Match(File.ReadAllText(gameIniPath),"\"SelectedMemberCategory\"\\s*:\\s*\"(\\w+)\"");return m.Success&&(m.Groups[1].Value=="Unheard"||m.Groups[1].Value=="UniqueId");}catch{return false;}
  }
  public void Load(Dictionary<string,List<string>> index){byItem=index;}
  public void LoadHideout(HideoutData data){hideout=data;}
  // Player's hideout levels per mode, kept in hideout-levels.json: {"pve":{"<station id>":2,...},...}. Null = not entered.
  public static Dictionary<string,int> ReadLevels(string path,string mode){
   try{if(!File.Exists(path))return null;var all=new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path,Encoding.UTF8)) as IDictionary;var m=all==null?null:all[mode] as IDictionary;if(m==null)return null;
    var r=new Dictionary<string,int>();foreach(DictionaryEntry e in m){int v;if(int.TryParse(Str(e.Value),out v))r[Str(e.Key)]=v;}return r;}catch{return null;}
  }
  public static void WriteLevels(string path,string mode,IDictionary<string,int> levels){
   var all=new Dictionary<string,object>();
   try{if(File.Exists(path)){var old=new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path,Encoding.UTF8)) as IDictionary;if(old!=null)foreach(DictionaryEntry e in old)all[Str(e.Key)]=e.Value;}}catch{}
   // "saved:<mode>" = when the levels were last saved (unix seconds), to compare with a prestige or wipe (ClearIfOlder).
   if(levels==null){all.Remove(mode);all.Remove("saved:"+mode);}else{all[mode]=new Dictionary<string,int>(levels);all["saved:"+mode]=(long)(DateTime.UtcNow-new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc)).TotalSeconds;}
   var temp=path+".tmp";File.WriteAllText(temp,new JavaScriptSerializer().Serialize(all),new UTF8Encoding(false));if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
  }
  // Prestige and season wipes start the hideout over: levels saved before resetAt (unix seconds; 0 = none) are removed.
  // Files from before the save time was kept use the file's own write time (never earlier than the real save).
  public static bool ClearIfOlder(string path,string mode,long resetAt){
   try{if(resetAt<=0||!File.Exists(path))return false;var all=new JavaScriptSerializer().DeserializeObject(File.ReadAllText(path,Encoding.UTF8)) as IDictionary;if(all==null||!(all[mode] is IDictionary))return false;
    long saved;if(!all.Contains("saved:"+mode)||!long.TryParse(Str(all["saved:"+mode]),out saved))saved=(long)(File.GetLastWriteTimeUtc(path)-new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc)).TotalSeconds;
    if(saved>=resetAt)return false;WriteLevels(path,mode,null);return true;}catch{return false;}
  }
  // Lines for an item (merged entries pass several ids): at most four, then "외 N개". done = quests already
  // completed (from the game's logs): they are not listed, the item is no longer needed for them.
  public string[] For(string ids){return For(ids,null);}
  public string[] For(string ids,ICollection<string> done){
   var m=byItem;if(m==null||string.IsNullOrEmpty(ids))return new string[0];var all=new List<string>();
   foreach(var id in ids.Split(',')){List<string> l;if(m.TryGetValue(id,out l))foreach(var s in l){int cut=s.IndexOf('\u001f');string task=cut>0?s.Substring(0,cut):"",text=cut>=0?s.Substring(cut+1):s;
    if(done!=null&&task.Length>0&&done.Contains(task))continue;if(!all.Contains(text))all.Add(text);}}
   if(all.Count>4){int more=all.Count-3;all=all.GetRange(0,3);all.Add(Language.T("외 퀘스트 {0}개",more));}
   return all.ToArray();
  }
  public bool LoadCache(){
   // t4 rows: q \t item \t quest id \x1f quest line | h \t item \t station \t name \t level \t count \t fir | s \t station \t name \t max level
   try{if(!File.Exists(cachePath))return false;var lines=File.ReadAllLines(cachePath,Encoding.UTF8);if(lines.Length<1)return false;var head=lines[0].Split('\t');if(head.Length!=3||head[0]!="t4"||head[1]!=Mode)return false;
    var q=new Dictionary<string,List<string>>();var h=new HideoutData();
    for(int i=1;i<lines.Length;i++){var f=lines[i].Split('\t');
     if(f[0]=="q"&&f.Length==3){List<string> l;if(!q.TryGetValue(f[1],out l))q[f[1]]=l=new List<string>();l.Add(f[2]);}
     else if(f[0]=="h"&&f.Length==7){List<HideNeed> l;if(!h.Needs.TryGetValue(f[1],out l))h.Needs[f[1]]=l=new List<HideNeed>();l.Add(new HideNeed{Station=f[2],Name=f[3],Level=int.Parse(f[4]),Count=int.Parse(f[5]),Fir=f[6]=="1"});}
     else if(f[0]=="s"&&f.Length==4)h.Stations.Add(new Station{Id=f[1],Name=f[2],MaxLevel=int.Parse(f[3])});}
    byItem=q;hideout=h;loaded=new DateTime(long.Parse(head[2]),DateTimeKind.Utc);return true;}catch{return false;}
  }
  void SaveCache(Dictionary<string,List<string>> quests,HideoutData hide,DateTime time){
   Func<string,string> c=s=>(s??"").Replace('\t',' ').Replace('\n',' ');
   var sb=new StringBuilder();sb.Append("t4\t").Append(Mode).Append('\t').Append(time.Ticks).Append('\n');
   foreach(var p in quests)foreach(var s in p.Value)sb.Append("q\t").Append(p.Key).Append('\t').Append(c(s)).Append('\n');
   foreach(var p in hide.Needs)foreach(var n in p.Value)sb.Append("h\t").Append(p.Key).Append('\t').Append(n.Station).Append('\t').Append(c(n.Name)).Append('\t').Append(n.Level).Append('\t').Append(n.Count).Append('\t').Append(n.Fir?"1":"0").Append('\n');
   foreach(var s in hide.Stations)sb.Append("s\t").Append(s.Id).Append('\t').Append(c(s.Name)).Append('\t').Append(s.MaxLevel).Append('\n');
   var temp=cachePath+".tmp";File.WriteAllText(temp,sb.ToString(),new UTF8Encoding(false));if(File.Exists(cachePath))File.Replace(temp,cachePath,null);else File.Move(temp,cachePath);
  }
  public void RefreshIfStale(TimeSpan maxAge){
   var now=DateTime.UtcNow;if(Refreshing||now<retryAt)return;if(byItem!=null&&now-loaded<maxAge)return;
   refresh=Task.Run(()=>{
    var thread=Thread.CurrentThread;var priority=thread.Priority;thread.Priority=ThreadPriority.BelowNormal;
    try{
     var root="https://json.tarkov.dev/"+Mode+"/";var http=Catalog.Http;
     var m=Build(http.GetStringAsync(root+"tasks").Result,http.GetStringAsync(root+"tasks_"+Language.ApiLocale).Result,http.GetStringAsync(root+"traders").Result,http.GetStringAsync(root+"traders_"+Language.ApiLocale).Result);
     var hide=BuildHideout(http.GetStringAsync(root+"hideout").Result,http.GetStringAsync(root+"hideout_"+Language.ApiLocale).Result);
     var time=DateTime.UtcNow;byItem=m;hideout=hide;loaded=time;Error=null;try{SaveCache(m,hide,time);}catch{}
    }catch(Exception e){var inner=e is AggregateException&&e.InnerException!=null?e.InnerException:e;Error=Language.T("퀘스트 데이터 갱신 실패 · {0}",Language.Error(inner));retryAt=DateTime.UtcNow.AddMinutes(5);}
    finally{thread.Priority=priority;}
   });
  }
 }
 // Keys the player inspected in the price window, so the map can show where they open (my-keys.json: mode -> key id -> name).
 // Nothing is read from the game: a key is known only once its name was read from its inspect window.
 public static class MyKeys {
  static readonly System.Web.Script.Serialization.JavaScriptSerializer js=new System.Web.Script.Serialization.JavaScriptSerializer();
  static readonly object gate=new object();
  public static bool IsKey(Item item){return item!=null&&item.Category!=null&&item.Category.IndexOf("key",StringComparison.OrdinalIgnoreCase)>=0;}
  // my-keys.json (mode -> key id -> name) is shared with the map's key list (CompanionUI KeyPanel), which reloads
  // when the file changes: the inspect window's "지도에 열쇠 등록" button and the list stay the same list.
  static Dictionary<string,object> Read(string path){try{var root=File.Exists(path)?js.Deserialize<Dictionary<string,object>>(File.ReadAllText(path)):null;return root??new Dictionary<string,object>();}catch{return new Dictionary<string,object>();}}
  static Dictionary<string,object> Of(Dictionary<string,object> root,string mode,bool create){var keys=root.ContainsKey(mode)?root[mode] as Dictionary<string,object>:null;if(keys==null&&create){keys=new Dictionary<string,object>();root[mode]=keys;}return keys;}
  static void Write(string path,Dictionary<string,object> root){var tmp=path+"."+System.Diagnostics.Process.GetCurrentProcess().Id+".tmp";File.WriteAllText(tmp,js.Serialize(root));if(File.Exists(path))File.Replace(tmp,path,null);else File.Move(tmp,path);}
  public static bool Has(string path,string mode,string id){lock(gate){var keys=Of(Read(path),mode,false);return keys!=null&&keys.ContainsKey(id);}}
  // True when the key is new to the list.
  public static bool Add(string path,string mode,string id,string name){
   lock(gate){try{var root=Read(path);var keys=Of(root,mode,true);if(keys.ContainsKey(id))return false;keys[id]=name;Write(path,root);return true;}catch{return false;}}
  }
  // True when the key was in the list.
  public static bool Remove(string path,string mode,string id){
   lock(gate){try{var root=Read(path);var keys=Of(root,mode,false);if(keys==null||!keys.Remove(id))return false;Write(path,root);return true;}catch{return false;}}
  }
 }
}
