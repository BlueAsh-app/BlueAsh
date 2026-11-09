using System;
using System.IO;
using System.Windows.Forms;
namespace ModePulse {
 // BlueAshHelper.exe: BlueAsh's helper processes, started by BlueAsh.exe (companion_ui.h / price_ui.h) from the unpacked
 // runtime folder:  --companion -ParentId <pid> [-Hidden] [-Page <page>]  |  --price -ParentId <pid>  |  --selftest
 public static class HelperMain {
  [STAThread]
  public static int Main(string[] args){
   string mode="",page="";int parentId=0;bool hidden=false;
   for(int i=0;i<args.Length;i++){var a=args[i].ToLowerInvariant();
    if(a=="--companion"||a=="--price"||a=="--selftest")mode=a.Substring(2);
    else if(a=="-parentid"&&i+1<args.Length)int.TryParse(args[++i],out parentId);
    else if(a=="-hidden")hidden=true;
    else if(a=="-page"&&i+1<args.Length)page=args[++i];}
   // Anything unexpected goes to standard error, which BlueAsh.exe sends to <name>-error.log (companion_ui.h HelperIo).
   AppDomain.CurrentDomain.UnhandledException+=delegate(object s,UnhandledExceptionEventArgs e){try{Console.Error.WriteLine(e.ExceptionObject);Console.Error.Flush();}catch{}};
   Application.ThreadException+=delegate(object s,System.Threading.ThreadExceptionEventArgs e){try{Console.Error.WriteLine(e.Exception);Console.Error.Flush();}catch{}};
   ModeSwitchPrice.Desktop.SetProcessDPIAware(); // system-DPI aware like BlueAsh.exe: pages embedded in its window share one scale
   Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
   var root=Path.GetDirectoryName(typeof(HelperMain).Assembly.Location);
   if(mode=="selftest")return SelfTest(root);
   if(parentId<=0)return 2;
   if(mode=="price")return new ModeSwitchPrice.PriceApp(parentId).Run();
   if(mode!="companion")return 2;
   CompanionApp app;
   try{app=new CompanionApp(parentId,root);}catch(Exception ex){if(ex.Message=="parent-exit")return 0;throw;}
   try{return app.Run(hidden,page);}
   catch(Exception ex){MessageBox.Show(ex.Message,I18n.Text("BlueAsh 도우미 오류"));return 1;}
  }
  // Release check (BlueAsh.exe --selftest): the translations load, every referenced assembly resolves, the map catalog parses.
  static int SelfTest(string root){
   try{
    if(!File.Exists(Path.Combine(root,"Localization.tsv")))throw new Exception("Localization.tsv missing");
    if(I18n.Codes.Length!=13||string.IsNullOrEmpty(I18n.FontName))throw new Exception("I18n");
    foreach(var t in new[]{typeof(Microsoft.Web.WebView2.WinForms.WebView2),typeof(Microsoft.Web.WebView2.Core.CoreWebView2Environment),typeof(ModeSwitchPrice.Catalog),typeof(MapWindow)})if(t.Assembly==null)throw new Exception(t.Name);
    ModeSwitchPrice.Language.Reload();
    Json.Parse(File.ReadAllText(Path.Combine(root,"..","maps","catalog.json")));
    Console.Out.WriteLine("ok");return 0;
   }catch(Exception ex){Console.Out.WriteLine(ex.ToString());return 1;}
  }
 }
}
