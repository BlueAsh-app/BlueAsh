param([string]$FleaImage,[string]$ItemSearchImage)
$ErrorActionPreference='Stop';function Assert($ok,$why){if(!$ok){throw $why}}
$repo=Split-Path $PSScriptRoot -Parent
Add-Type -AssemblyName System.Drawing,System.Windows.Forms,System.Runtime.WindowsRuntime,System.Web.Extensions
Add-Type -Path ($repo+'/price/PriceHelper.dll'),($repo+'/companion/CompanionUI.dll')
Assert ([bool]('ModePulse.MapSearch' -as [type])) 'Map hotkey needs a screen classifier for flea market and ITEM SEARCH'
Assert ([ModePulse.MapSearch]::SearchTitle('ITEM SEARCH')) 'Item-search title'
Assert (![ModePulse.MapSearch]::SearchTitle('Search') -and ![ModePulse.MapSearch]::SearchTitle('ITEM DESCRIPTION')) 'Generic search text and inspect title never block normal inventory'
foreach($text in '플리마켓','FLEA MARKET','Барахолка','フリーマーケット','跳蚤市场','跳蚤市場','FLOHMARKT','MARCHÉ AUX PUCES','PCHLI TARG','BIT PAZARI','MERCADILLO','MERCADO','MERCATINO'){Assert ([ModePulse.MapSearch]::FleaTitle($text)) ('Localized flea title: '+$text)}
$tile=New-Object Drawing.Bitmap(400,100);$g=[Drawing.Graphics]::FromImage($tile);$r=[Drawing.RectangleF]::new(30,30,140,22)
try{$g.Clear([Drawing.Color]::FromArgb(195,200,185));Assert ([ModePulse.MapSearch]::Selected($tile,$r)) 'Light selected navigation tile';$g.Clear([Drawing.Color]::FromArgb(30,34,36));Assert (![ModePulse.MapSearch]::Selected($tile,$r)) 'Inactive flea navigation remains allowed'}finally{$g.Dispose();$tile.Dispose()}
if($FleaImage -or $ItemSearchImage){
 $t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile(($repo+'/companion/Companion.ps1'),[ref]$t,[ref]$e)
 Invoke-Expression ($ast.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Init-Ocr'},$true).Extent.Text);function T($s){return $s};Init-Ocr
 function Await($op,$type){return $script:asTask.MakeGenericMethod($type).Invoke($null,@($op)).GetAwaiter().GetResult()}
 function Check($source){
  $w=0;$h=0;$px=[ModeSwitchPrice.Desktop]::Sample($source,1600,[ref]$w,[ref]$h);$bars=New-Object 'System.Collections.Generic.List[Drawing.Rectangle]';foreach($b in [ModeSwitchPrice.Core]::TitleBars($px,$w,$h)){$bars.Add([Drawing.Rectangle]::new($b.X,$b.Y,$b.W,$b.H))}
  $nav=0;$sheet=[ModePulse.MapSearch]::Sheet($source,$bars.ToArray(),[ref]$nav);$ms=New-Object IO.MemoryStream;$sheet.Save($ms,[Drawing.Imaging.ImageFormat]::Png);$stream=[Windows.Storage.Streams.InMemoryRandomAccessStream]::new();$writer=[Windows.Storage.Streams.DataWriter]::new($stream);$writer.WriteBytes($ms.ToArray());$null=Await ($writer.StoreAsync()) ([uint32]);$null=$writer.DetachStream();$stream.Seek(0);$decoder=Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder]);$bitmap=Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
  try{$result=Await ($script:engine.RecognizeAsync($bitmap)) ([Windows.Media.Ocr.OcrResult]);$text=New-Object 'System.Collections.Generic.List[string]';$rects=New-Object 'System.Collections.Generic.List[Drawing.RectangleF]';foreach($line in $result.Lines){foreach($word in $line.Words){$text.Add([string]$word.Text);$r=$word.BoundingRect;$rects.Add([Drawing.RectangleF]::new($r.X,$r.Y,$r.Width,$r.Height))}};return [ModePulse.MapSearch]::Blocked($sheet,$nav,$text.ToArray(),$rects.ToArray())}finally{$bitmap.Dispose();$writer.Dispose();$stream.Dispose();$sheet.Dispose();$ms.Dispose()}
 }
 if($FleaImage){$source=[Drawing.Bitmap]::FromFile($FleaImage);try{Assert (Check $source) 'Actual flea-market screenshot blocks map';foreach($width in 1920,1280){$scaled=New-Object Drawing.Bitmap($source,([Drawing.Size]::new($width,[int]($source.Height*$width/$source.Width))));try{Assert (Check $scaled) ('Flea market at '+$width)}finally{$scaled.Dispose()}}}finally{$source.Dispose()}}
 if($ItemSearchImage){$source=[Drawing.Bitmap]::FromFile($ItemSearchImage);try{Assert (Check $source) 'Actual ITEM SEARCH screenshot blocks map';$full=New-Object Drawing.Bitmap(3840,2160);$g=[Drawing.Graphics]::FromImage($full);try{$g.Clear([Drawing.Color]::FromArgb(20,24,27));$g.DrawImageUnscaled($source,1000,240);Assert (Check $full) 'Moved item-search window inside full game frame';$g.FillRectangle([Drawing.Brushes]::Black,1000,240,$source.Width,60);Assert (!(Check $full)) 'Normal inventory with no ITEM SEARCH title remains allowed'}finally{$g.Dispose();$full.Dispose()}}finally{$source.Dispose()}}
}
Write-Output 'PASS: search titles, selected versus inactive flea tab, localized labels and supplied screenshot/scale/moved-window cases'
