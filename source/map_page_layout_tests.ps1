# Windows-only: build the real map / notification pages of BlueAshHelper.exe in each of the thirteen languages (one child
# process per language, BLUEASH_TEST_LANGUAGE) and attach them to a stand-in main window, without starting any watcher.
param([string]$Locale='')
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
function Assert($ok,[string]$message){if(!$ok){throw $message}}
$stage=Join-Path $PSScriptRoot '.bundle/map-layout'
if(!$Locale){
 [void][IO.Directory]::CreateDirectory($stage)
 foreach($code in 'ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'){
  $env:BLUEASH_TEST_LANGUAGE=$code;$out=& powershell -NoProfile -STA -ExecutionPolicy Bypass -File $PSCommandPath -Locale $code 2>&1|Out-String
  if($LASTEXITCODE -ne 0 -or $out -notmatch 'OK '+[regex]::Escape($code)){throw ('Layout '+$code+': '+$out)}
 }
 Remove-Item Env:BLUEASH_TEST_LANGUAGE
 'PASS: thirteen locales, real map controls, inset bounds, save/close footer and scrollable content'
 'Preview: '+$stage+'/map-page-ko.png'
 exit 0
}
try{
 . (Join-Path $PSScriptRoot 'helper_test_load.ps1')
 Assert ([ModePulse.I18n]::Language -eq $Locale) ('Language override: '+[ModePulse.I18n]::Language)
 $store=Join-Path $stage ('store-'+$Locale);[void][IO.Directory]::CreateDirectory($store)
 $app=[ModePulse.CompanionApp]::PagesForTest((Join-Path $repo 'companion'),$store);$mapForm=$app.MapPage;$discordForm=$app.NotificationPage
 $k=[ModePulse.Embed]::Scale
 $hostForm=New-Object Windows.Forms.Form;$hostForm.ClientSize=New-Object Drawing.Size([int](1160*$k),[int](780*$k));$hostForm.ShowInTaskbar=$false;$hostForm.StartPosition='Manual';$hostForm.Location=New-Object Drawing.Point(-4000,-3000)
 $null=$hostForm.Handle
 Assert ([ModePulse.Embed]::Attach($mapForm,$hostForm.Handle,260,118,852,600)) 'Map attach failed'
 Assert ($mapForm.Height -le [int](600*$k)+1) ('Map extends below requested inset: '+$mapForm.Height)
 Assert ($mapForm.Bottom -le [int](718*$k)+1) ('Map protrudes past rounded panel inset: '+$mapForm.Bottom)
 $find={param($c,$t)foreach($x in $c.Controls){if($x.Text -eq $t){return $x};$r=& $find $x $t;if($r){return $r}};return $null}
 $save=& $find $mapForm ([ModePulse.I18n]::Text('저장'));$close=& $find $mapForm ([ModePulse.I18n]::Text('닫기'));$quest=& $find $mapForm ([ModePulse.I18n]::Text('진행 중인 퀘스트 목표'))
 Assert ($save -and $close -and $quest) 'Map controls missing'
 $footer=$save.Parent
 Assert ($footer.Parent -eq $mapForm -and $footer.Bottom -le $mapForm.ClientSize.Height) 'Footer outside map page'
 Assert ($save.Bottom -le $footer.ClientSize.Height -and $close.Bottom -le $footer.ClientSize.Height) 'Save/close clipped'
 Assert (@($footer.Controls).Count -eq 2) 'Map footer should contain only save and close'
 $body=$quest.Parent;Assert ($body.AutoScroll -and $body.Bottom -le $footer.Top) 'Body overlaps fixed actions'
 foreach($control in @($body.Controls)){Assert ($control.Right -le $body.ClientSize.Width) ('Control outside body: '+$control.Text)}
 if($Locale -in 'ko','de','ru'){$image=New-Object Drawing.Bitmap($mapForm.Width,$mapForm.Height);try{$mapForm.DrawToBitmap($image,(New-Object Drawing.Rectangle(0,0,$image.Width,$image.Height)));$image.Save((Join-Path $stage ('map-page-'+$Locale+'.png')))}finally{$image.Dispose()}}
 Assert ([ModePulse.Embed]::Attach($discordForm,$hostForm.Handle,260,118,852,622)) 'Notifications attach failed'
 Assert ([Math]::Abs($discordForm.Height-[int](622*$k)) -le 1) ('Notifications page height changed: '+$discordForm.Height)
 $hostForm.Dispose();'OK '+$Locale;exit 0
}catch{$_.Exception.Message;exit 1}
