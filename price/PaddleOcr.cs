using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
namespace ModeSwitchPrice {
 // PaddleOCR PP-OCRv5 text-line recognition (one model per game text language), run with ONNX Runtime through its
 // C API (no managed wrapper or extra .NET assemblies). The runtime and the model are not in BlueAsh.exe: the first
 // time a language needs them they are downloaded from BlueAsh's own server in the background, checked against
 // manifest.json (SHA-256) and kept in %LOCALAPPDATA%\ModeSwitch\ocr. Until then, and whenever anything fails,
 // callers keep using Windows OCR. Sources: relay/public/ocr, built by source/ocr-models.mjs.
 public static class OcrModels {
  public const string BaseUrl="https://modepulse-relay.modepulse.workers.dev/ocr/";
  // Tarkov text language (GameLanguage.Api) -> model.
  public static string ModelFor(string api){
   switch(api){
    case "ko":return "korean_PP-OCRv5_mobile_rec";
    case "ja":case "zh":return "PP-OCRv5_mobile_rec";
    case "en":return "en_PP-OCRv5_mobile_rec";
    case "ru":return "eslav_PP-OCRv5_mobile_rec";
    case "de":case "fr":case "es":case "it":case "pl":case "pt":case "hu":case "sk":case "ro":case "tr":return "latin_PP-OCRv5_mobile_rec";
   }return null;}
  static readonly object gate=new object();static Task job;static DateTime retryAt=DateTime.MinValue;
  public static string Status{get;private set;}
  static string Sha(string f){using(var s=SHA256.Create())using(var r=File.OpenRead(f)){var h=s.ComputeHash(r);var b=new StringBuilder();foreach(var x in h)b.Append(x.ToString("x2"));return b.ToString();}}
  static bool Good(string f,IDictionary<string,object> want){if(!File.Exists(f))return false;return new FileInfo(f).Length==Convert.ToInt64(want["size"])&&Sha(f)==Convert.ToString(want["sha256"]);}
  static void Fetch(string url,string file,IDictionary<string,object> want){
   if(Good(file,want))return;Directory.CreateDirectory(Path.GetDirectoryName(file));var tmp=file+".part";
   using(var r=Catalog.Http.GetAsync(url,System.Net.Http.HttpCompletionOption.ResponseHeadersRead).Result){r.EnsureSuccessStatusCode();using(var s=r.Content.ReadAsStreamAsync().Result)using(var o=File.Create(tmp))s.CopyTo(o);}
   if(!Good(tmp,want)){File.Delete(tmp);throw new Exception("checksum "+Path.GetFileName(file));}
   if(File.Exists(file))File.Delete(file);File.Move(tmp,file);}
  // Folder with runtime\ and <model>\ once everything is present and verified; null while downloading (started once,
  // retried after 10 minutes on failure). Never blocks.
  public static string Ready(string store,string model){
   if(model==null)return null;var root=Path.Combine(store,"ocr");var ok=Path.Combine(root,model,".ok");
   if(File.Exists(ok)&&File.Exists(Path.Combine(root,"runtime","onnxruntime.dll")))return root;
   lock(gate){if((job==null||job.IsCompleted)&&DateTime.UtcNow>=retryAt)job=Task.Run(()=>{
    var t=Thread.CurrentThread;t.Priority=ThreadPriority.BelowNormal;
    try{Status="downloading";var js=new JavaScriptSerializer{MaxJsonLength=int.MaxValue};
     var m=js.DeserializeObject(Catalog.Http.GetStringAsync(BaseUrl+"manifest.json").Result) as IDictionary<string,object>;
     foreach(var e in (IDictionary<string,object>)m["runtime"])Fetch(BaseUrl+"runtime/"+e.Key,Path.Combine(root,"runtime",e.Key),(IDictionary<string,object>)e.Value);
     var files=(IDictionary<string,object>)((IDictionary<string,object>)((IDictionary<string,object>)m["models"])[model])["files"];
     foreach(var e in files)Fetch(BaseUrl+model+"/"+e.Key,Path.Combine(root,model,e.Key),(IDictionary<string,object>)e.Value);
     File.WriteAllText(ok,DateTime.UtcNow.ToString("o"));Status="ready";}
    catch(Exception e){Status="failed: "+(e.GetBaseException().Message);retryAt=DateTime.UtcNow.AddMinutes(10);}});}
   return null;}
 }
 public sealed class PaddleRec : IDisposable {
  [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryExW(string path,IntPtr file,uint flags);
  [DllImport("kernel32",CharSet=CharSet.Ansi)] static extern IntPtr GetProcAddress(IntPtr module,string name);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr GetApiBaseFn();
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr GetApiFn(uint version);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr ErrorMessageFn(IntPtr status);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr CreateEnvFn(int level,[MarshalAs(UnmanagedType.LPStr)] string id,out IntPtr env);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr OutFn(out IntPtr result);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr SetIntFn(IntPtr options,int value);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr CreateSessionFn(IntPtr env,[MarshalAs(UnmanagedType.LPWStr)] string path,IntPtr options,out IntPtr session);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr CpuInfoFn(int allocator,int memType,out IntPtr info);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr TensorFn(IntPtr info,IntPtr data,UIntPtr bytes,long[] shape,UIntPtr dims,int type,out IntPtr value);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr RunFn(IntPtr session,IntPtr runOptions,IntPtr[] inputNames,IntPtr[] inputs,UIntPtr inputCount,IntPtr[] outputNames,UIntPtr outputCount,IntPtr[] outputs);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr DataFn(IntPtr value,out IntPtr data);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr CountFn(IntPtr info,out UIntPtr count);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr DimsFn(IntPtr info,long[] dims,UIntPtr length);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr NameFn(IntPtr session,UIntPtr index,IntPtr allocator,out IntPtr name);
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate void ReleaseFn(IntPtr item);
  // OrtApi slots for ORT_API_VERSION 22 (ONNX Runtime 1.22, append-only ABI; read from onnxruntime_c_api.h).
  const uint ApiVersion=22;
  const int S_ErrorMessage=2,S_CreateEnv=3,S_CreateSession=7,S_Run=9,S_CreateSessionOptions=10,S_SetGraphOptimization=23,S_SetIntraOp=24,S_SetInterOp=25,
   S_InputName=36,S_OutputName=37,S_CreateTensor=49,S_TensorData=51,S_DimsCount=61,S_Dims=62,S_TypeAndShape=65,S_CpuInfo=69,S_DefaultAllocator=78,
   S_ReleaseEnv=92,S_ReleaseStatus=93,S_ReleaseMemoryInfo=94,S_ReleaseSession=95,S_ReleaseValue=96,S_ReleaseShape=99,S_ReleaseOptions=100;
  static IntPtr api;static IntPtr env;static readonly object loadGate=new object();
  static T Fn<T>(int slot){return (T)(object)Marshal.GetDelegateForFunctionPointer(Marshal.ReadIntPtr(api,slot*IntPtr.Size),typeof(T));}
  static void Check(IntPtr status){if(status==IntPtr.Zero)return;string m=Marshal.PtrToStringAnsi(Fn<ErrorMessageFn>(S_ErrorMessage)(status));Fn<ReleaseFn>(S_ReleaseStatus)(status);throw new Exception("ONNX Runtime: "+m);}
  // The Visual C++ runtime next to onnxruntime.dll is loaded from that folder first, so no system install is needed.
  static void LoadRuntime(string folder){lock(loadGate){if(api!=IntPtr.Zero)return;
   foreach(var dll in new[]{"vcruntime140.dll","vcruntime140_1.dll","msvcp140.dll","msvcp140_1.dll"}){var p=Path.Combine(folder,dll);if(File.Exists(p))LoadLibraryExW(p,IntPtr.Zero,8);}
   var module=LoadLibraryExW(Path.Combine(folder,"onnxruntime.dll"),IntPtr.Zero,8/*LOAD_WITH_ALTERED_SEARCH_PATH*/);if(module==IntPtr.Zero)throw new Exception("onnxruntime.dll load failed "+Marshal.GetLastWin32Error());
   var baseFn=GetProcAddress(module,"OrtGetApiBase");if(baseFn==IntPtr.Zero)throw new Exception("OrtGetApiBase missing");
   var apiBase=((GetApiBaseFn)Marshal.GetDelegateForFunctionPointer(baseFn,typeof(GetApiBaseFn)))();
   var a=((GetApiFn)Marshal.GetDelegateForFunctionPointer(Marshal.ReadIntPtr(apiBase),typeof(GetApiFn)))(ApiVersion);if(a==IntPtr.Zero)throw new Exception("ONNX Runtime API "+ApiVersion+" unavailable");
   api=a;IntPtr e;Check(Fn<CreateEnvFn>(S_CreateEnv)(3/*ORT_LOGGING_LEVEL_ERROR*/,"BlueAsh",out e));env=e;}}
  // Loading a session takes about 0.4 s and the first run is slower still: done on a background thread as soon as the
  // model is on disk (not on the first inspect click), with one warm-up line.
  public static Task<PaddleRec> LoadAsync(string root,string model){return Task.Run(()=>{
   var t=Thread.CurrentThread;t.Priority=ThreadPriority.BelowNormal;var rec=new PaddleRec(root,model);
   using(var b=new Bitmap(240,40,PixelFormat.Format24bppRgb)){using(var g=Graphics.FromImage(b)){g.Clear(Color.FromArgb(58,60,58));using(var f=new Font("Segoe UI",18,GraphicsUnit.Pixel))g.DrawString("Warm up 123",f,Brushes.Gainsboro,8,8);}double c;rec.Recognize(b,out c);}
   return rec;});}
  IntPtr session,memory,inputName,outputName;readonly string[] chars;
  public string Model{get;private set;}
  public PaddleRec(string root,string model){
   LoadRuntime(Path.Combine(root,"runtime"));Model=model;var dir=Path.Combine(root,model);
   var lines=File.ReadAllLines(Path.Combine(dir,"dict.txt"),Encoding.UTF8);chars=new string[lines.Length];
   for(int i=0;i<lines.Length;i++)chars[i]=lines[i].Replace("\\n","\n").Replace("\\\\","\\");
   IntPtr options;Check(Fn<OutFn>(S_CreateSessionOptions)(out options));
   try{Check(Fn<SetIntFn>(S_SetIntraOp)(options,2));Check(Fn<SetIntFn>(S_SetInterOp)(options,1));Check(Fn<SetIntFn>(S_SetGraphOptimization)(options,99/*ORT_ENABLE_ALL*/));
    Check(Fn<CreateSessionFn>(S_CreateSession)(env,Path.Combine(dir,"inference.onnx"),options,out session));}
   finally{Fn<ReleaseFn>(S_ReleaseOptions)(options);}
   Check(Fn<CpuInfoFn>(S_CpuInfo)(0/*OrtDeviceAllocator*/,0/*OrtMemTypeDefault*/,out memory));
   IntPtr allocator;Check(Fn<OutFn>(S_DefaultAllocator)(out allocator));
   Check(Fn<NameFn>(S_InputName)(session,UIntPtr.Zero,allocator,out inputName));Check(Fn<NameFn>(S_OutputName)(session,UIntPtr.Zero,allocator,out outputName));
  }
  // One text line (any size; dark-on-light or light-on-dark) -> text and mean confidence (0..1).
  public string RecognizePng(byte[] png,out double confidence){using(var ms=new MemoryStream(png))using(var b=new Bitmap(ms))return Recognize(b,out confidence);}
  // The model expects the text to fill the line's height: crop to the pixels that differ from the background
  // (the border's typical colour), with a small margin, so short title strips with wide margins read like its training lines.
  static Bitmap TightCrop(Bitmap s){
   int w=s.Width,h=s.Height;var d=s.LockBits(new Rectangle(0,0,w,h),ImageLockMode.ReadOnly,PixelFormat.Format24bppRgb);var px=new byte[d.Stride*h];Marshal.Copy(d.Scan0,px,0,px.Length);s.UnlockBits(d);
   Func<int,int,int> lum=(x,y)=>{int o=y*d.Stride+x*3;return (px[o]+px[o+1]*2+px[o+2])>>2;};
   var border=new List<int>();for(int x=0;x<w;x++){border.Add(lum(x,0));border.Add(lum(x,h-1));}for(int y=0;y<h;y++){border.Add(lum(0,y));border.Add(lum(w-1,y));}border.Sort();int bg=border[border.Count/2];
   int top=h,bottom=-1,left=w,right=-1;for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(Math.Abs(lum(x,y)-bg)>48){if(y<top)top=y;if(y>bottom)bottom=y;if(x<left)left=x;if(x>right)right=x;}
   if(bottom<0||bottom-top<4)return null;int th=bottom-top+1,m=Math.Max(2,th/6);
   var r=Rectangle.FromLTRB(Math.Max(0,left-m),Math.Max(0,top-m),Math.Min(w,right+m+1),Math.Min(h,bottom+m+1));if(r.Width>=w&&r.Height>=h)return null;return s.Clone(r,PixelFormat.Format24bppRgb);}
  // A region of a captured frame (the title strip), clamped to the frame.
  public string Recognize(Bitmap frame,Rectangle region,out double confidence){
   confidence=0;region.Intersect(new Rectangle(0,0,frame.Width,frame.Height));if(region.Width<2||region.Height<2)return "";
   using(var part=frame.Clone(region,PixelFormat.Format24bppRgb))return Recognize(part,out confidence);}
  public string Recognize(Bitmap source,out double confidence){
   confidence=0;if(source.Width<2||source.Height<2)return "";
   var tight=TightCrop(source);if(tight!=null){try{return RecognizeLine(tight,out confidence);}finally{tight.Dispose();}}
   return RecognizeLine(source,out confidence);}
  string RecognizeLine(Bitmap source,out double confidence){
   confidence=0;
   // PaddleOCR rec preprocessing: height 48, width by aspect (at least 320, padded with 0 after normalising), BGR, (v/255-0.5)/0.5.
   const int H=48;int w=Math.Max(1,Math.Min(3200,(int)Math.Ceiling(H*(double)source.Width/source.Height))),W=Math.Max(320,w);
   var input=new float[3*H*W];
   using(var r=new Bitmap(w,H,PixelFormat.Format24bppRgb)){using(var g=Graphics.FromImage(r)){g.InterpolationMode=System.Drawing.Drawing2D.InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=System.Drawing.Drawing2D.PixelOffsetMode.HighQuality;g.DrawImage(source,new Rectangle(0,0,w,H));}
    var d=r.LockBits(new Rectangle(0,0,w,H),ImageLockMode.ReadOnly,PixelFormat.Format24bppRgb);var px=new byte[d.Stride*H];Marshal.Copy(d.Scan0,px,0,px.Length);r.UnlockBits(d);
    for(int y=0;y<H;y++)for(int x=0;x<w;x++){int o=y*d.Stride+x*3;for(int c=0;c<3;c++)input[(c*H+y)*W+x]=px[o+c]/127.5f-1f;}} // memory order B,G,R = channel 0,1,2
   var handle=GCHandle.Alloc(input,GCHandleType.Pinned);IntPtr tensor=IntPtr.Zero,result=IntPtr.Zero,shapeInfo=IntPtr.Zero;
   try{Check(Fn<TensorFn>(S_CreateTensor)(memory,handle.AddrOfPinnedObject(),(UIntPtr)(input.Length*4),new long[]{1,3,H,W},(UIntPtr)4,1/*FLOAT*/,out tensor));
    var outs=new IntPtr[1];Check(Fn<RunFn>(S_Run)(session,IntPtr.Zero,new[]{inputName},new[]{tensor},(UIntPtr)1,new[]{outputName},(UIntPtr)1,outs));result=outs[0];
    Check(Fn<OutFn2>(S_TypeAndShape)(result,out shapeInfo));UIntPtr n;Check(Fn<CountFn>(S_DimsCount)(shapeInfo,out n));var dims=new long[(int)n];Check(Fn<DimsFn>(S_Dims)(shapeInfo,dims,n));
    IntPtr data;Check(Fn<DataFn>(S_TensorData)(result,out data));int steps=(int)dims[1],classes=(int)dims[2];var probs=new float[steps*classes];Marshal.Copy(data,probs,0,probs.Length);
    // CTC greedy decode: class 0 is blank, class i is dictionary entry i-1, one class past the dictionary is a space.
    var sb=new StringBuilder();int last=0;double sum=0;int count=0;
    for(int t=0;t<steps;t++){int best=0;float p=probs[t*classes];for(int c=1;c<classes;c++)if(probs[t*classes+c]>p){p=probs[t*classes+c];best=c;}
     if(best!=0&&best!=last){sb.Append(best-1<chars.Length?chars[best-1]:" ");sum+=p;count++;}last=best;}
    confidence=count>0?sum/count:0;return sb.ToString().Trim();}
   finally{handle.Free();if(shapeInfo!=IntPtr.Zero)Fn<ReleaseFn>(S_ReleaseShape)(shapeInfo);if(result!=IntPtr.Zero)Fn<ReleaseFn>(S_ReleaseValue)(result);if(tensor!=IntPtr.Zero)Fn<ReleaseFn>(S_ReleaseValue)(tensor);}
  }
  [UnmanagedFunctionPointer(CallingConvention.Winapi)] delegate IntPtr OutFn2(IntPtr value,out IntPtr result);
  public void Dispose(){if(session!=IntPtr.Zero){Fn<ReleaseFn>(S_ReleaseSession)(session);session=IntPtr.Zero;}if(memory!=IntPtr.Zero){Fn<ReleaseFn>(S_ReleaseMemoryInfo)(memory);memory=IntPtr.Zero;}}
 }
}
