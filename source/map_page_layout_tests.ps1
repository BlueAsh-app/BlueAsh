# Windows-only: create the real map controls without starting game watchers or deleting screenshots.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$repo=Split-Path $PSScriptRoot -Parent
Add-Type -Path (Join-Path $repo 'companion/CompanionUI.dll')
$source=[IO.File]::ReadAllText((Join-Path $repo 'companion/Companion.ps1'))
$start=$source.IndexOf('$fontName=');$end=$source.IndexOf('# ---- 디스코드 알림 (모든 게임 공통) ----')
if($start -lt 0 -or $end -le $start){throw 'Map setup boundary missing'}
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors)
$fit=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'Fit-Page'},$true)
if(!$fit){throw 'Fit-Page missing'}
Invoke-Expression $fit.Extent.Text
$words=@{};foreach($line in [IO.File]::ReadAllLines((Join-Path $repo 'companion/Localization.tsv'))){if($line -and !$line.StartsWith('#')){$cells=$line.Split("`t");$words[$cells[0]]=$cells}}
function T([string]$text){if($words.ContainsKey($text) -and $words[$text].Count -gt $script:column){return $words[$text][$script:column]};return $text}
function Assert($ok,[string]$message){if(!$ok){throw $message}}
$stage=Join-Path $PSScriptRoot ('.bundle/map-layout-'+[Guid]::NewGuid().ToString('N'));[void][IO.Directory]::CreateDirectory($stage)
foreach($script:column in 0..12){
 $config=@{mapKey=77;showQuests=$true;showKeys=$true;shotPos=$true;shotDelete=$true};$appExe=''
 Invoke-Expression $source.Substring($start,$end-$start)
 [ModePulse.Theme]::Apply($mapForm);$mapForm.BackColor=[ModePulse.Theme]::Panel
 foreach($c in $script:sections){$c.Font=$sectionFont};foreach($c in $script:hints){$c.Font=$hintFont}
 Fit-Page $mapForm;$mapForm.ResumeLayout($true)
 $hostForm=New-Object Windows.Forms.Form;$hostForm.ClientSize=New-Object Drawing.Size(1160,780);$hostForm.ShowInTaskbar=$false;$hostForm.StartPosition='Manual';$hostForm.Location=New-Object Drawing.Point(-1400,-1000)
 try{
  $null=$hostForm.Handle
  Assert ([ModePulse.Embed]::Attach($mapForm,$hostForm.Handle,260,118,852,600)) 'Map attach failed'
  Assert ($mapForm.Height -le 600) ('Map extends below requested inset: '+$mapForm.Height)
  Assert ($mapForm.Bottom -le 718) ('Map protrudes past rounded panel inset: '+$mapForm.Bottom)
  $footer=$saveMapBtn.Parent
  Assert ($footer.Parent -eq $mapForm -and $footer.Bottom -le $mapForm.ClientSize.Height) 'Footer outside map page'
  Assert ($saveMapBtn.Bottom -le $footer.ClientSize.Height -and $closeMapBtn.Bottom -le $footer.ClientSize.Height) 'Save/close clipped'
  Assert (@($footer.Controls).Count -eq 2) 'Map footer should contain only save and close'
  Assert ($questCheck.Parent -eq $keyCheck.Parent -and $shotCheck.Parent -eq $questCheck.Parent) 'Map options missing'
  $body=$questCheck.Parent;Assert ($body.AutoScroll -and $body.Bottom -le $footer.Top) 'Body overlaps fixed actions'
  foreach($control in @($body.Controls)){Assert ($control.Right -le $body.ClientSize.Width) ('Control outside body: '+$control.Text)}
  if($script:column -eq 0){$image=New-Object Drawing.Bitmap($mapForm.Width,$mapForm.Height);try{$mapForm.DrawToBitmap($image,(New-Object Drawing.Rectangle(0,0,$image.Width,$image.Height)));$image.Save((Join-Path $stage 'map-page.png'))}finally{$image.Dispose()}}
  Assert ([ModePulse.Embed]::Attach($discordForm,$hostForm.Handle,260,118,852,622)) 'Notifications attach failed'
  Assert ($discordForm.Height -eq 622) 'Existing notifications page height changed'
 }finally{$mapForm.Dispose();$discordForm.Dispose();$hostForm.Dispose()}
}
'PASS: thirteen locales, real map controls, inset bounds, save/close footer and scrollable content'
'Preview: '+$stage+'/map-page.png'
