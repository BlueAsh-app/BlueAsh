$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../updater/UpdateLanguage.ps1')
function Assert($ok,[string]$name){if(!$ok){throw $name}}
$updaterAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot '../updater/Update.ps1'),[ref]$null,[ref]$null)
$report=$updaterAst.Find({param($a)$a -is [Management.Automation.Language.FunctionDefinitionAst] -and $a.Name -eq 'Report'},$true)
Assert ($null -ne $report) 'updater report function missing'
Invoke-Expression $report.Extent.Text
$reportFolder=Join-Path ([IO.Path]::GetTempPath()) ('BlueAsh-update-language-tests-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($reportFolder)
$script:newVersion='2.9.0'
Assert ($script:UpdateWords.Count -ge 38) 'updater strings missing'
foreach($code in $script:UpdateLanguages){
 $script:UpdateLanguage=$code
 foreach($key in $script:UpdateWords.Keys){
  $row=$script:UpdateWords[$key];Assert ($row.Count -eq 5) "language missing $key"
  $text=U $key
  Assert (![string]::IsNullOrWhiteSpace($text)) "empty translation $code $key"
  $sourceFormat=([regex]::Matches($key,'\{\d+(?:[^}]*)?\}')|ForEach-Object {$_.Value}|Sort-Object) -join '|'
  $targetFormat=([regex]::Matches($text,'\{\d+(?:[^}]*)?\}')|ForEach-Object {$_.Value}|Sort-Object) -join '|'
  Assert ($sourceFormat -eq $targetFormat) "format changed $code $key"
  $null=U $key @('A','B')
 }
 Assert ((U '받지 못했습니다 · 서버 응답 {0}' @(403)).Contains('403')) "server response format $code"
 Assert ((U '연결이 멈춰 다시 받는 중… ({0}/3)' @(2)).Contains('2/3')) "retry format $code"
 $detail=Update-Error (New-Object InvalidOperationException('한국어 운영체제 오류'))
 Assert ($detail -eq $(if($code -eq 'ko'){'한국어 운영체제 오류'}else{'InvalidOperationException (0x80131509)'})) "OS error presentation $code"
 foreach($key in 'ZIP 파일을 찾지 못했습니다.','받지 못했습니다 · 서버 응답 {0}','허용되지 않는 파일: {0}'){
  $text=U $key @('403');Assert ((Update-Error (New-Object Exception($text))) -eq $text) "authored error preserved $code $key"
 }
 $progressFile=Join-Path $reportFolder ($code+'.txt')
 Report 'working' (U '파일을 검사하는 중…') 0.5 (U 'SHA-256 검사')
 $serialized=[IO.File]::ReadAllText($progressFile)
 Assert ($serialized.Contains("state=working`n") -and $serialized.Contains("part=0.500`n") -and $serialized.Contains("version=2.9.0`n") -and $serialized.Contains("language=$code`n")) "progress protocol and language marker $code"
}
$script:UpdateLanguage='ko'
foreach($f in Get-ChildItem (Join-Path $PSScriptRoot '../updater') -Filter '*.ps1'){
 $t=$null;$e=$null;$null=[Management.Automation.Language.Parser]::ParseFile($f.FullName,[ref]$t,[ref]$e);Assert ($e.Count -eq 0) ($e|Out-String)
 if($f.Name -ne 'UpdateLanguage.ps1'){
  $s=[IO.File]::ReadAllText($f.FullName)
  foreach($match in [regex]::Matches($s,"\bU '([^']*)'")){Assert ($script:UpdateWords.ContainsKey($match.Groups[1].Value)) ('missing updater translation: '+$match.Groups[1].Value)}
 }
}
# Run the existing installation/rollback/ZIP security regressions while preserving their unique fixture folder.
$regression=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'update_tests.ps1'))
$cleanup='finally{if(Test-Path $work){Remove-Item $work -Recurse -Force}}'
Assert ($regression.Contains($cleanup)) 'existing regression cleanup changed; inspect before running'
$regression=$regression.Replace($cleanup,'finally{}')
$regression=$regression.Replace('$PSScriptRoot',("'"+$PSScriptRoot.Replace("'","''")+"'"))
& ([scriptblock]::Create($regression))
'PASS: updater five-language coverage, format placeholders, source-key coverage, script parsing and preserved update regressions; fixture files retained.'
