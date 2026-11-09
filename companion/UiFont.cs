using System;
using System.Drawing;
using System.Drawing.Text;
using System.Runtime.InteropServices;
namespace BlueAshUi {
 // BlueAsh's own text face (Pretendard, OFL) for the helper windows, like the main window (i18n::fontFamily): Japanese and
 // Chinese keep their system UI fonts. The font is read from BlueAsh.exe's resources (RCDATA 401 regular, 402 bold), not
 // installed: GDI+ draws it from a private collection, and AddFontMemResourceEx lets GDI (TextRenderer, native controls)
 // find it by name. Not loaded (tests, an old BlueAsh.exe): every caller falls back to the language's system font.
 public static class UiFont {
  static PrivateFontCollection fonts;static FontFamily family;
  public static FontFamily Family{get{return family;}}
  [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr LoadLibraryEx(string path,IntPtr file,uint flags);
  [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
  [DllImport("kernel32.dll")] static extern IntPtr FindResource(IntPtr module,IntPtr name,IntPtr type);
  [DllImport("kernel32.dll")] static extern IntPtr LoadResource(IntPtr module,IntPtr res);
  [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr data);
  [DllImport("kernel32.dll")] static extern uint SizeofResource(IntPtr module,IntPtr res);
  [DllImport("gdi32.dll")] static extern IntPtr AddFontMemResourceEx(IntPtr data,uint size,IntPtr reserved,ref uint count);
  public static bool Load(string exe,string language){
   if(family!=null)return true;if(language=="ja"||language=="zh-CN"||language=="zh-TW"||string.IsNullOrEmpty(exe))return false;
   IntPtr m=LoadLibraryEx(exe,IntPtr.Zero,0x2|0x20);if(m==IntPtr.Zero)return false; // LOAD_LIBRARY_AS_DATAFILE | AS_IMAGE_RESOURCE
   try{var set=new PrivateFontCollection();int added=0;
    foreach(int id in new[]{401,402}){IntPtr r=FindResource(m,new IntPtr(id),new IntPtr(10));if(r==IntPtr.Zero)continue;IntPtr g=LoadResource(m,r);uint n=SizeofResource(m,r);IntPtr src=LockResource(g);if(src==IntPtr.Zero||n==0)continue;
     // GDI+ reads the font from this memory while the collection lives: a copy kept for the life of the process.
     var bytes=new byte[n];Marshal.Copy(src,bytes,0,(int)n);IntPtr p=Marshal.AllocCoTaskMem((int)n);Marshal.Copy(bytes,0,p,(int)n);
     uint count=0;AddFontMemResourceEx(p,n,IntPtr.Zero,ref count);set.AddMemoryFont(p,(int)n);added++;}
    if(added==0||set.Families.Length==0){set.Dispose();return false;}
    fonts=set;family=set.Families[0];return true;
   }catch{return false;}finally{FreeLibrary(m);} // GDI+ and GDI keep their own copies of the font data
  }
  public static Font New(string fallback,float size){return New(fallback,size,FontStyle.Regular,GraphicsUnit.Point);}
  public static Font New(string fallback,float size,FontStyle style){return New(fallback,size,style,GraphicsUnit.Point);}
  public static Font New(string fallback,float size,FontStyle style,GraphicsUnit unit){
   if(family!=null&&family.IsStyleAvailable(style))return new Font(family,size,style,unit);
   return new Font(fallback,size,style,unit);
  }
 }
}
