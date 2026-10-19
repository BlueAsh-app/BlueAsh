$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot '../price'
Add-Type -Path @((Join-Path $root 'PriceCore.cs'),(Join-Path $root 'PriceWindow.cs'),(Join-Path $root 'PriceCatalog.cs'),(Join-Path $root 'PriceLanguage.cs')) -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.Net.Http,System.Web.Extensions
function Assert($ok,[string]$name){if(!$ok){throw $name}}
. (Join-Path $root 'PriceCore.ps1')
$workerAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $root 'PriceWorker.ps1'),[ref]$null,[ref]$null)
$bridge=$workerAst.Find({param($a)$a -is [Management.Automation.Language.FunctionDefinitionAst] -and $a.Name -eq 'P'},$true)
Assert ($null -ne $bridge) 'price PowerShell bridge missing'
Invoke-Expression $bridge.Extent.Text
$keys=[ModeSwitchPrice.Language]::Keys
Assert ($keys.Count -ge 40) 'price strings missing'
foreach($code in [ModeSwitchPrice.Language]::Codes){
 [ModeSwitchPrice.Language]::Code=$code
 foreach($key in $keys){
  $text=[ModeSwitchPrice.Language]::T($key)
  Assert (![string]::IsNullOrWhiteSpace($text)) "empty translation $code $key"
  $sourceFormat=([regex]::Matches($key,'\{\d+(?:[^}]*)?\}')|ForEach-Object {$_.Value}|Sort-Object) -join '|'
  $targetFormat=([regex]::Matches($text,'\{\d+(?:[^}]*)?\}')|ForEach-Object {$_.Value}|Sort-Object) -join '|'
  Assert ($sourceFormat -eq $targetFormat) "format changed $code $key"
  $null=[ModeSwitchPrice.Language]::T($key,[object[]]@('A','B','C','D','E'))
 }
 Assert ([ModeSwitchPrice.Language]::ApiLocale -eq $(if($code -eq 'zh-CN' -or $code -eq 'zh-TW'){'zh'}elseif($code -eq 'pt-BR'){'pt'}else{$code})) "API locale $code"
 Assert ((P '현재 시세 : {0}루블' @('8,500')) -eq [ModeSwitchPrice.Language]::T('현재 시세 : {0}루블',[object[]]@('8,500'))) "PowerShell format bridge $code"
 $osError=New-Object InvalidOperationException('한국어 운영체제 오류')
 $detail=[ModeSwitchPrice.Language]::Error($osError)
 Assert ($detail -eq $(if($code -eq 'ko'){'한국어 운영체제 오류'}else{'InvalidOperationException (0x80131509)'})) "OS error presentation $code"
 $authored=New-Object Exception([ModeSwitchPrice.Language]::T('OCR 영역 오류'))
 Assert ([ModeSwitchPrice.Language]::Error($authored) -eq [ModeSwitchPrice.Language]::T('OCR 영역 오류')) "authored error preserved $code"
 Assert ([ModeSwitchPrice.Core]::Mode('1.2.0 | PvE') -eq 'pve' -and (Get-LogMode 'Info|application|Session mode: Regular') -eq 'regular') "game mode classifiers changed $code"
 # All item matching still uses the game's Korean catalog, regardless of UI preference.
 $c=New-Object ModeSwitchPrice.Catalog('pve',$PSScriptRoot)
 $rows=[ModeSwitchPrice.Catalog]::Parse('{"data":{"items":{"5755383e24597772cb798966":{"id":"5755383e24597772cb798966","name":"n","types":[],"lastLowPrice":8500}}}}','{"data":{"n":"군용 케이블"}}','pve')
 $c.Load($rows,[DateTime]::UtcNow)
 $display=[ModeSwitchPrice.Catalog]::Parse('{"data":{"items":{"5755383e24597772cb798966":{"id":"5755383e24597772cb798966","name":"n","types":[],"lastLowPrice":8500}}}}','{"data":{"n":"Military cable"}}','pve')
 $c.LoadDisplayNames($display)
 $hit=$c.Find('군용 케이블');Assert ($null -ne $hit -and $hit.Name -eq '군용 케이블' -and $c.DisplayName($hit) -eq 'Military cable') "OCR catalog changed $code"
 $tj='{"data":{"tasks":{"t1":{"id":"t1","name":"n","trader":"tr","kappaRequired":true,"objectives":[{"type":"giveItem","count":2,"foundInRaid":true,"items":["bolt"]}],"neededKeys":[{"keys":["key"]}]}}}}'
 $ti=New-Object ModeSwitchPrice.TaskIndex('pve',$PSScriptRoot)
 $ti.Load([ModeSwitchPrice.TaskIndex]::Build($tj,'{"data":{"n":"Quest"}}','{"data":{"tr":{"name":"tr name"}}}','{"data":{"tr name":"Trader"}}'))
 $line=$ti.For('bolt') -join ''
 Assert ($line -eq ('Quest (Trader) · '+[ModeSwitchPrice.Language]::T('카파')+' · '+[ModeSwitchPrice.Language]::T('{0}개',[object[]]@(2))+' · '+[ModeSwitchPrice.Language]::T('인레이드'))) "quest terms $code"
 Assert (($ti.For('key') -join '') -eq ('Quest (Trader) · '+[ModeSwitchPrice.Language]::T('카파')+' · '+[ModeSwitchPrice.Language]::T('열쇠 필요'))) "key requirement $code"
 $hm=[ModeSwitchPrice.TaskIndex]::BuildHideout('{"data":{"wb":{"id":"wb","name":"n","levels":[{"level":1,"itemRequirements":[{"item":"bolt","count":2,"attributes":{"foundInRaid":true}}]}]}}}','{"data":{"n":"Workbench"}}')
 $ti.LoadHideout($hm)
 $h=@($ti.Hideout('bolt',$null))
 Assert ($h.Count -eq 1 -and $h[0].Now -and $h[0].Text -eq ('Workbench '+[ModeSwitchPrice.Language]::T('Lv.{0} ({1}개 필요)',[object[]]@(1,2))+' · '+[ModeSwitchPrice.Language]::T('인레이드'))) "hideout terms $code"
 $cache=$ti.GetType().GetField('cachePath',[Reflection.BindingFlags]'NonPublic,Instance').GetValue($ti)
 Assert ($cache.EndsWith('tasks-pve'+$(if($code -eq 'ko'){''}else{'.'+[ModeSwitchPrice.Language]::ApiLocale})+'.tsv')) "display cache locale $code"
 # The layered overlay uses a font for the selected script and leaves transparent outline padding.
 [ModeSwitchPrice.PriceTag]::Korean=[ModeSwitchPrice.Language]::FontName
 $bitmap=[ModeSwitchPrice.PriceTag]::Render([ModeSwitchPrice.Language]::T('현재 시세 : {0}루블',[object[]]@('8,500')),[Drawing.Color]::Gold,22)
 try{Assert ($bitmap.Width -gt 80 -and $bitmap.Height -gt 20) "overlay dimensions $code";Assert ($bitmap.GetPixel(0,0).A -eq 0 -and $bitmap.GetPixel($bitmap.Width-1,$bitmap.Height-1).A -eq 0) "overlay clipped $code"}finally{$bitmap.Dispose()}
}
Assert ([ModeSwitchPrice.Language]::Normalize('ZH-cn') -eq 'zh-CN') 'locale case normalization'
Assert ([ModeSwitchPrice.Language]::Normalize('unsupported') -eq 'ko') 'unknown language fallback'
[ModeSwitchPrice.Language]::Code='ko'
foreach($f in Get-ChildItem $root -Filter '*.ps1'){$t=$null;$e=$null;$null=[Management.Automation.Language.Parser]::ParseFile($f.FullName,[ref]$t,[ref]$e);Assert ($e.Count -eq 0) ($e|Out-String)}
foreach($command in $workerAst.FindAll({param($a)$a -is [Management.Automation.Language.CommandAst] -and $a.GetCommandName() -eq 'P'},$true)){
 $key=$command.CommandElements[1].Value;Assert ([ModeSwitchPrice.Language]::Contains($key)) ('missing price worker translation: '+$key)
}
foreach($file in 'PriceCatalog.cs','PriceWindow.cs'){
 foreach($m in [regex]::Matches([IO.File]::ReadAllText((Join-Path $root $file)),'\bLanguage\.T\("([^"]+)"')){Assert ([ModeSwitchPrice.Language]::Contains($m.Groups[1].Value)) ('missing C# translation: '+$m.Groups[1].Value)}
}
'PASS: price five-language coverage, format placeholders, Korean OCR catalog independence, localized display names, quest/hideout needs, cache isolation, overlay font/render bounds, script parsing.'
