$ErrorActionPreference='Stop'
function Assert($ok,$name){if(!$ok){throw $name}}
Add-Type -AssemblyName System.Web.Extensions,System.Drawing,System.Windows.Forms
$dll=Join-Path $PSScriptRoot '../companion/CompanionUI.dll';Add-Type -Path $dll
Add-Type -TypeDefinition 'using System;using System.Collections;using System.Reflection;public static class KeyDescriptionTest{public static string Read(Hashtable local,Hashtable english){return (string)typeof(ModePulse.Quests).GetMethod("KeyDescription",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,new object[]{local,english,"test"});}}' -ReferencedAssemblies System.Web.Extensions,System.Windows.Forms,System.Drawing,$dll
$english=@{'test Description'='English key description.'}
foreach($locale in 'ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'){
 $local=@{'test Description'=('Localized description '+$locale)}
 Assert ([KeyDescriptionTest]::Read($local,$english) -eq $local['test Description']) ('Localized description has priority: '+$locale)
}
Assert ([KeyDescriptionTest]::Read(@{},$english) -eq 'English key description.') 'Missing localized field falls back to English'
Assert ([KeyDescriptionTest]::Read(@{'test Description'='  '},$english) -eq 'English key description.') 'Empty localized field falls back to English'
Assert ([KeyDescriptionTest]::Read($null,$english) -eq 'English key description.') 'Missing localization dictionary falls back to English'
Assert ([KeyDescriptionTest]::Read($english,$english) -eq 'English key description.') 'API English fallback is kept instead of hidden'
Assert ([KeyDescriptionTest]::Read(@{},@{}) -eq '') 'No description in either API stays empty without invented text'
'PASS: localized priority across 13 locales, missing/blank localization English fallback, existing English preserved'
