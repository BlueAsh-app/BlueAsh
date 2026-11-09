using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;
using System.Web.Script.Serialization;
namespace ModePulse {
 // Small JSON reader over JavaScriptSerializer output (Dictionary<string,object> / object[]), for files written by the
 // game and by PowerShell before: a key is matched exactly first, then without case (ConvertFrom-Json did that).
 public static class Json {
  public static object Parse(string text){return new JavaScriptSerializer{MaxJsonLength=int.MaxValue,RecursionLimit=256}.DeserializeObject(text);}
  public static string Write(object value){return new JavaScriptSerializer{MaxJsonLength=int.MaxValue,RecursionLimit=256}.Serialize(value);}
  public static object Get(object o,string key){var d=o as IDictionary<string,object>;if(d==null)return null;object v;if(d.TryGetValue(key,out v))return v;
   foreach(var p in d)if(string.Equals(p.Key,key,StringComparison.OrdinalIgnoreCase))return p.Value;return null;}
  // A JSON array, a single value (PowerShell often wrote one-element arrays as the value itself), or nothing.
  public static List<object> List(object o){var r=new List<object>();if(o==null)return r;var a=o as object[];if(a!=null){r.AddRange(a);return r;}var l=o as System.Collections.ArrayList;if(l!=null){foreach(var x in l)r.Add(x);return r;}r.Add(o);return r;}
  public static string Str(object o){return o==null?"":Convert.ToString(o,CultureInfo.InvariantCulture);}
  public static bool Bool(object o,bool fallback){if(o is bool)return (bool)o;if(o==null)return fallback;bool b;if(bool.TryParse(Str(o),out b))return b;double d;return double.TryParse(Str(o),NumberStyles.Float,CultureInfo.InvariantCulture,out d)?d!=0:fallback;}
  public static double Num(object o,double fallback){if(o==null)return fallback;double d;return double.TryParse(Str(o),NumberStyles.Float,CultureInfo.InvariantCulture,out d)?d:fallback;}
 }
 // Exit names read from the in-game exit list matched against the map's exits (was CompanionCore.ps1).
 public static class ExitMatch {
  // Exit names are English in the game's Korean locale, read by the Korean OCR engine: fold look-alike characters.
  public static string Normalize(string text){var t=Regex.Replace((text??"").ToLowerInvariant(),@"[\s\p{P}\p{S}]","");return t.Replace('0','o').Replace('1','l').Replace('i','l').Replace('|','l');}
  // Fewest edits to turn name into some substring of line (approximate substring match).
  public static int Distance(string line,string name){
   int n=name.Length;var prev=new int[n+1];for(int j=0;j<=n;j++)prev[j]=j;int best=n;
   foreach(char ch in line){var cur=new int[n+1];cur[0]=0;
    for(int j=1;j<=n;j++){int cost=name[j-1]==ch?0:1;cur[j]=Math.Min(Math.Min(prev[j]+1,cur[j-1]+1),prev[j-1]+cost);}
    if(cur[n]<best)best=cur[n];prev=cur;}
   return best;
  }
  // In-game transit lines are Korean ("리저브로 이동"); Fandom names them "Transit to Reserve".
  static readonly string[,] transitMaps={{"리저브","reserve"},{"공장","factory"},{"팩토리","factory"},{"인터체인지","interchange"},{"해안선","shoreline"},{"쇼어라인","shoreline"},{"세관","customs"},{"커스텀","customs"},{"삼림","woods"},{"숲","woods"},{"등대","lighthouse"},{"라이트하우스","lighthouse"},{"연구소","lab"},{"렙","lab"},{"시내","streets"},{"스트리트","streets"},{"그라운드","ground zero"},{"터미널","terminal"},{"미궁","labyrinth"},{"아이스브레이커","icebreaker"}};
  public static List<string> Match(IEnumerable<string> lines,IEnumerable<Pin> pins){
   var result=new List<string>();var seen=new HashSet<string>();var list=new List<Pin>(pins);
   foreach(var line in lines){string text=line??"";string key=Normalize(text);string digits=Regex.Replace(text,"[^0-9]","");
    // Digits misread as look-alikes within one word ("ZB-1011" read as "7+1이1": 이 = 0 1, o/ㅇ = 0, l/I/| = 1).
    var folded=new List<string>();foreach(var w in Regex.Split(text,@"\s+")){var f=Regex.Replace(Regex.Replace(Regex.Replace(w.Replace("이","01"),"[oOㅇ]","0"),"[lI|]","1"),"[^0-9]","");if(f.Length>0)folded.Add(f);}
    foreach(var pin in list){string raw=pin.Name??"";
     // Transit: the Korean destination in the line names the map in "Transit to <map>".
     if(raw.StartsWith("Transit to",StringComparison.OrdinalIgnoreCase)){
      if(key.Contains(Normalize(raw))){if(seen.Add(raw))result.Add(raw);continue;}
      if(text.Contains("이동")){for(int i=0;i<transitMaps.GetLength(0);i++){if(text.Contains(transitMaps[i,0])&&raw.ToLowerInvariant().Contains(transitMaps[i,1])){if(seen.Add(raw))result.Add(raw);break;}}}
      continue;}
     // Numbered exits ("ZB-1013", read as "7+1013"): the digit run decides.
     var runs=new List<string>();foreach(System.Text.RegularExpressions.Match m in Regex.Matches(raw,@"\d{3,}"))runs.Add(m.Value);
     if(runs.Count>0){int ok=0;foreach(var r in runs){bool hit=digits.Contains(r);if(!hit)foreach(var f in folded)if(f.Contains(r)){hit=true;break;}if(hit)ok++;}if(ok==runs.Count){if(seen.Add(raw))result.Add(raw);continue;}}
     // Words: notes in brackets differ between the game and the wiki ("(조명탄 탈출구)" / "(Flare)"), so they are ignored.
     string name=Normalize(Regex.Replace(raw,@"\s*\([^)]*\)",""));if(name.Length<3)continue;
     // One misread character allowed per 4 (none for short names like "Gate 3", which differ by one digit).
     if(key.Contains(name)||(name.Length>=7&&Distance(key,name)<=name.Length/4)){if(seen.Add(raw))result.Add(raw);}
    }
   }
   return result;
  }
  // An exit is shown once its name is read in two frames of the same check (any two: a misread frame in between does not
  // reset it); seen keeps every name read in this check.
  public static void Confirm(ISet<string> seen,IEnumerable<string> hits,ICollection<string> available){var list=new List<string>(hits);foreach(var n in list)if(seen.Contains(n)&&!available.Contains(n))available.Add(n);foreach(var n in list)seen.Add(n);}
  public static string ClassifyNotice(string text){
   var t=(text??"").ToLowerInvariant();
   if(!Regex.IsMatch(t,"maintenance|installation|update|patch"))return "";
   if(Regex.IsMatch(t,"website|arena"))return "";
   if(Regex.IsMatch(t,"complete|completed|available again"))return "maintenance.completed";
   if(Regex.IsMatch(t,"extended|extension"))return "maintenance.extended";
   if(Regex.IsMatch(t,"have started|has started|we are installing|installation has begun"))return "maintenance.started";
   return "";
  }
  public static bool ValidWebhook(string url){return url!=null&&Regex.IsMatch(url,@"^https://discord\.com/api(?:/v[0-9]+)?/webhooks/[0-9]+/[A-Za-z0-9_-]+$");}
 }
 // Read-only parser for EFT's local Control.ini (was ControlBinding.ps1). Never writes game settings.
 public sealed class ExitVariant{public int[] Codes;public string Label;}
 public sealed class ExitBinding{public ExitVariant[] Variants;public string Type;public double TimeoutMs;public string Label;}
 public sealed class TriggerState{public bool Seen,Down;public double LastTap;}
 public static class EftControl {
  static readonly Dictionary<string,int> table=new Dictionary<string,int>{{"Mouse0",1},{"Mouse1",2},{"Mouse2",4},{"Mouse3",5},{"Mouse4",6},{"LeftShift",160},{"RightShift",161},{"LeftControl",162},{"RightControl",163},{"LeftAlt",164},{"RightAlt",165},{"Space",32},{"Tab",9},{"Return",13},{"Escape",27},{"Backspace",8},{"Insert",45},{"Delete",46},{"Home",36},{"End",35},{"PageUp",33},{"PageDown",34},{"UpArrow",38},{"DownArrow",40},{"LeftArrow",37},{"RightArrow",39},{"KeypadPlus",107},{"KeypadMinus",109},{"KeypadMultiply",106},{"KeypadDivide",111},{"KeypadPeriod",110},{"CapsLock",20},{"Numlock",144},{"ScrollLock",145}};
  public static int ConvertKey(string name){
   if(Regex.IsMatch(name??"","^[A-Z]$"))return name[0];
   var m=Regex.Match(name??"","^F([1-9]|1[0-5])$",RegexOptions.IgnoreCase);if(m.Success)return 111+int.Parse(m.Groups[1].Value);
   m=Regex.Match(name??"","^Alpha([0-9])$",RegexOptions.IgnoreCase);if(m.Success)return 48+int.Parse(m.Groups[1].Value);
   m=Regex.Match(name??"","^Keypad([0-9])$",RegexOptions.IgnoreCase);if(m.Success)return 96+int.Parse(m.Groups[1].Value);
   foreach(var p in table)if(string.Equals(p.Key,name,StringComparison.OrdinalIgnoreCase))return p.Value;
   throw new Exception(I18n.Text("지원하지 않는 키: ")+name);
  }
  public static ExitBinding ReadExitBinding(string json){
   var doc=Json.Parse(json);var rows=new List<object>();foreach(var b in Json.List(Json.Get(doc,"keyBindings")))if(string.Equals(Json.Str(Json.Get(b,"keyName")),"WatchTimerAndExits",StringComparison.OrdinalIgnoreCase))rows.Add(b);
   if(rows.Count!=1)throw new Exception(I18n.Text("탈출구 확인 항목이 없거나 중복됨"));
   var row=rows[0];string type=Json.Str(Json.Get(row,"pressType"));
   string canon=null;foreach(var t in new[]{"Press","Release","DoubleClick"})if(string.Equals(t,type,StringComparison.OrdinalIgnoreCase))canon=t;
   if(canon==null)throw new Exception(I18n.Text("지원하지 않는 입력 방식: ")+type+I18n.Text(" (자동 인식 중지)"));
   double timeout=0;
   if(canon=="DoubleClick"){timeout=Json.Num(Json.Get(doc,"DoubleClickTimeout"),0)*1000;if(timeout<10||timeout>2000)throw new Exception(I18n.Text("더블클릭 간격을 확인할 수 없음"));}
   var variants=new List<ExitVariant>();var labels=new List<string>();
   foreach(var v in Json.List(Json.Get(row,"variants"))){var names=new List<string>();foreach(var n in Json.List(Json.Get(v,"keyCode")))names.Add(Json.Str(n));if(names.Count==0)continue;
    var codes=new List<int>();foreach(var n in names)codes.Add(ConvertKey(n));var label=string.Join("+",names);variants.Add(new ExitVariant{Codes=codes.ToArray(),Label=label});labels.Add(label);}
   if(variants.Count==0)throw new Exception(I18n.Text("탈출구 확인 키가 지정되지 않음"));
   return new ExitBinding{Variants=variants.ToArray(),Type=canon,TimeoutMs=timeout,Label=string.Join(" / ",labels)+" · "+I18n.Text(canon)};
  }
  public static bool UpdateTrigger(TriggerState s,bool down,double now,ExitBinding binding){
   if(!s.Seen){s.Seen=true;s.Down=down;s.LastTap=-1000000000;return false;}
   bool pressed=down&&!s.Down,released=!down&&s.Down;s.Down=down;
   switch(binding.Type){
    case "Press":return pressed;
    case "Release":return released;
    case "DoubleClick":if(pressed){if(now-s.LastTap<=binding.TimeoutMs){s.LastTap=-1000000000;return true;}s.LastTap=now;}return false;
   }
   return false;
  }
  public static string FindControlFile(){
   var path=System.IO.Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),"Battlestate Games","Escape from Tarkov","Settings","Control.ini");
   if(System.IO.File.Exists(path))return path;
   throw new Exception(I18n.Text("Control.ini를 찾지 못함 · 타르코프 설정을 저장한 뒤 재확인하세요."));
  }
 }
 // The automatic exit-list read right after a raid starts (was ExitCapture.ps1 New-RaidExitSchedule).
 public sealed class ExitSchedule{public string Key;public long Until,Next;}
 public static class RaidExits {
  public static ExitSchedule Schedule(string[] state,long tick,string previous){
   if(state==null||state.Length<8||state[4]!="started")return null;
   long start;if(!long.TryParse(state[7],out start)||start<=0)return null;
   var key=state[0]+":"+state[5]+":"+start;
   if(key==previous||tick<start||tick-start>2500)return null;
   return new ExitSchedule{Key=key,Until=tick+3800,Next=tick+150};
  }
 }
}
