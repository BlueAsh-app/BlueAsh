param([int]$ParentId,[long]$Window,[string]$Install,[string]$CurrentVersion,[ValidateSet('online','zip')][string]$Mode='online',[string]$Feed='',[string]$Zip='')
# Runs without a window: BlueAsh's 업데이트 page shows the progress, read from update-progress.txt (key=value lines:
# state working|done|error|latest, step, part 0..1 or -1, detail, version). BlueAsh is closed only to swap the files;
# whatever happens after that, BlueAsh is started again (the new version, or the restored old one).
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Net.Http,System.IO.Compression.FileSystem
Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices;public static class ModeSwitchUpdateNative{[DllImport("user32.dll")]public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);[DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string t);[DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);}'
. (Join-Path $PSScriptRoot 'UpdateCore.ps1')
$store=Join-Path $env:LOCALAPPDATA 'ModeSwitch';[void][IO.Directory]::CreateDirectory($store)
$progressFile=Join-Path $store 'update-progress.txt'
$guard=New-Object Threading.Mutex($false,'Local\ModeSwitch-Updater')
$locked=$false;$temp=$null;$http=$null;$closed=$false;$newVersion=''
function Report([string]$State,[string]$Step,[double]$Part=-1,[string]$Detail=''){
 $text="state=$State`nstep=$Step`npart=$([string]::Format([Globalization.CultureInfo]::InvariantCulture,'{0:0.000}',$Part))`ndetail=$Detail`nversion=$script:newVersion`nlanguage=$script:UpdateLanguage`n"
 $tmp=$progressFile+'.tmp';[IO.File]::WriteAllText($tmp,$text,(New-Object Text.UTF8Encoding($false)))
 if(Test-Path -LiteralPath $progressFile){[IO.File]::Replace($tmp,$progressFile,[NullString]::Value)}else{[IO.File]::Move($tmp,$progressFile)}
}
function Mb([double]$b){[string]::Format([Globalization.CultureInfo]::GetCultureInfo($script:UpdateLanguage),'{0:0.0} MB',($b/1MB))}
function Wait-Network($task,[int]$Limit=0){$w=[Diagnostics.Stopwatch]::StartNew();while([Threading.Tasks.Task]::WaitAny([Threading.Tasks.Task[]]@($task),50) -lt 0){if($Limit -and $w.ElapsedMilliseconds -gt $Limit){throw (New-Object TimeoutException (U '응답이 멈췄습니다'))}};return $task.GetAwaiter().GetResult()}
# Streams each request into the file. HTTPS only, no redirect to plain HTTP. A read that brings nothing for 10 s is a
# stall (seen on real connections): that piece starts over, up to 3 tries.
function Fetch-Into($out,[string]$Url,[long]$Base,[long]$Total,[string]$Label){
 $uri=[Uri]$Url;if(!$uri.IsAbsoluteUri -or $uri.Scheme -ne 'https' -or $uri.UserInfo){throw (U '업데이트 주소는 HTTPS여야 합니다.')}
 for($try=1;;$try++){
  $out.SetLength($Base);$out.Position=$Base;[long]$got=0
  try{
   $r=Wait-Network ($http.GetAsync($uri,[Net.Http.HttpCompletionOption]::ResponseHeadersRead)) 15000;try{
    if(!$r.IsSuccessStatusCode){throw (U '받지 못했습니다 · 서버 응답 {0}' @([int]$r.StatusCode))}
    if($r.RequestMessage.RequestUri.Scheme -ne 'https'){throw (U 'HTTPS가 아닌 주소로 이동했습니다.')}
    $in=Wait-Network ($r.Content.ReadAsStreamAsync()) 15000;try{
     while($true){$n=Wait-Network ($in.ReadAsync($script:buf,0,$script:buf.Length)) 10000;if($n -le 0){break};$out.Write($script:buf,0,$n);$got+=$n
      if($Base+$got -gt 128000000){throw (U '업데이트 파일이 너무 큽니다.')}
      if($Label -and ($script:clock.ElapsedMilliseconds-$script:shown -ge 150)){$script:shown=$script:clock.ElapsedMilliseconds;$now=$Base+$got
       $t=$(if($script:dlClock){$script:dlClock}else{$script:clock});$speed=$(if($t.Elapsed.TotalSeconds -gt 0.5){'  ·  '+(Mb ($now/$t.Elapsed.TotalSeconds))+'/s'}else{''})
       if($Total){Report 'working' $Label ($now/$Total) ((Mb $now)+' / '+(Mb $Total)+$speed)}else{Report 'working' $Label -1 ((Mb $now)+$speed)}}}
    }finally{$in.Dispose()}
   }finally{$r.Dispose()}
   return $got
  }catch [TimeoutException],[Net.Http.HttpRequestException],[IO.IOException]{
   if($try -ge 3){throw (U '서버 연결이 자꾸 끊겨 받지 못했습니다. 잠시 뒤 다시 시도해 주세요.')}
   if($Label){Report 'working' (U '연결이 멈춰 다시 받는 중… ({0}/3)' @($try+1)) $(if($Total){$Base/$Total}else{-1})}
  }
 }
}
function Download([string]$Url,[string]$File){$out=[IO.File]::Create($File);try{[void](Fetch-Into $out $Url 0 0 '')}finally{$out.Dispose()}}
try {
 try{$locked=$guard.WaitOne(0)}catch [Threading.AbandonedMutexException]{$locked=$true}
 if(!$locked){throw (U '이미 업데이트를 진행하고 있습니다.')}
 $Install=[IO.Path]::GetFullPath($Install)
 $temp=Join-Path ([IO.Path]::GetTempPath()) ('ModeSwitch-update-'+[Guid]::NewGuid().ToString('N'))
 [void][IO.Directory]::CreateDirectory($temp)
 $http=New-Object Net.Http.HttpClient;$http.Timeout=[TimeSpan]::FromSeconds(120)
 $script:buf=New-Object byte[] 262144;$script:shown=-1000;$script:clock=[Diagnostics.Stopwatch]::StartNew()
 Report 'working' (U '업데이트 정보를 확인하는 중…')
 $expectedVersion=$null
 if($Mode -eq 'online'){
  $url=$Feed;if(!$url){$url=[string](Get-ItemPropertyValue -Path 'HKCU:\Software\ModeSwitch' -Name 'UpdateFeed' -ErrorAction SilentlyContinue)}
  if(!$url){throw (U '업데이트 서버 주소가 없습니다.')}
  $manifest=Join-Path $temp 'manifest.json';Download $url $manifest
  if((Get-Item $manifest).Length -gt 65536){throw (U '업데이트 정보가 너무 큽니다.')}
  $m=Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
  if($m.version -notmatch '^\d+\.\d+\.\d+(\.\d+)?$'){throw (U '업데이트 정보 형식 오류')}
  if([version]$m.version -le [version]$CurrentVersion){Report 'latest' (U '이미 최신 버전입니다.');return}
  if($m.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw (U '업데이트 정보 형식 오류')}
  $expectedVersion=[version]$m.version;$script:newVersion=[string]$m.version
  # The ZIP is published in parts (relay/public/update/<version>/partN, next to manifest.json), fetched straight from
  # the static files into one file. The single ZIP URL is the fallback for a manifest without parts.
  $zipFile=Join-Path $temp 'update.zip';$out=[IO.File]::Create($zipFile);$script:clock.Restart();try{
   # Speed counts from the first request of the download (not from the updater's start).
   $script:dlClock=[Diagnostics.Stopwatch]::StartNew()
   if(@($m.parts).Count){$base=$url.Substring(0,$url.LastIndexOf('/')+1);[long]$at=0
    foreach($p in @($m.parts)){if([string]$p -notmatch '^[0-9.]+/part\d+$'){throw (U '업데이트 정보 형식 오류')};$at+=Fetch-Into $out ($base+$p) $at ([long]$m.size) (U '새 버전을 받는 중…')}}
   else{[void](Fetch-Into $out ([string]$m.url) 0 ([long]$m.size) (U '새 버전을 받는 중…'))}
  }finally{$out.Dispose()}
  Report 'working' (U '받은 파일을 확인하는 중…') 1 ((Mb $m.size)+' · '+(U 'SHA-256 검사'))
  # .NET, not Get-FileHash: that one is a script function of the Utility module and was missing in a user's PowerShell.
  $fs=[IO.File]::OpenRead($zipFile);try{$hash=[BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash($fs)) -replace '-',''}finally{$fs.Dispose()}
  if($hash -ne $m.sha256){throw (U '받은 파일이 배포 정보와 다릅니다 (SHA-256 불일치). 다시 시도해 주세요.')}
 }else{
  # BlueAsh picked the ZIP (its own file dialog) and passes the path.
  if(!$Zip -or !(Test-Path -LiteralPath $Zip)){throw (U 'ZIP 파일을 찾지 못했습니다.')}
  $zipFile=Join-Path $temp 'update.zip';[IO.File]::Copy($Zip,$zipFile)
 }
 Report 'working' (U '파일을 검사하는 중…') 1
 $plan=@(Get-UpdatePlan $zipFile (Join-Path $temp 'stage'))
 # Compare major.minor.build: the exe's version string is "2.8.1" while FILEVERSION carries a 4th 0.
 $vi=[Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $temp 'stage/BlueAsh.exe'));$version=New-Object Version($vi.FileMajorPart,$vi.FileMinorPart,$vi.FileBuildPart)
 if($null -ne $expectedVersion -and $version -ne (New-Object Version($expectedVersion.Major,$expectedVersion.Minor,$expectedVersion.Build))){throw (U '실행 파일 버전과 배포 정보가 다릅니다.')}
 if($version -lt [version]$CurrentVersion){throw (U '이전 버전으로는 바꾸지 않습니다.')}
 $script:newVersion=$version.ToString()
 # Check write permission before asking BlueAsh to close.
 $probe=Join-Path $Install ('.update-test-'+[Guid]::NewGuid().ToString('N'));[IO.File]::WriteAllText($probe,'');[IO.File]::Delete($probe)
 Report 'working' (U 'BlueAsh를 다시 시작하는 중…') 1 (U '잠시 뒤 새 버전으로 다시 켜집니다')
 if(![ModeSwitchUpdateNative]::PostMessage([IntPtr]$Window,32945,[IntPtr]::Zero,[IntPtr]::Zero)){throw (U '프로그램 종료 요청 실패')}
 $parent=Get-Process -Id $ParentId -ErrorAction SilentlyContinue
 if($parent -and !$parent.WaitForExit(20000)){throw (U 'BlueAsh가 닫히지 않아 업데이트를 멈췄습니다. 화면 오류 문구를 확인하세요.')}
 $closed=$true
 $backup=Join-Path $env:LOCALAPPDATA ('ModeSwitch\update-backups\'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
 # The BlueAsh window must belong to the exe just installed (not, say, a copy run from elsewhere).
 function Test-NewWindow{$h=[ModeSwitchUpdateNative]::FindWindow('ModeSwitch',[NullString]::Value);if($h -eq [IntPtr]::Zero){return $false};$id=[uint32]0;[void][ModeSwitchUpdateNative]::GetWindowThreadProcessId($h,[ref]$id)
  try{return (Get-Process -Id $id -ErrorAction Stop).Path -ieq (Join-Path $Install 'BlueAsh.exe')}catch{return $false}}
 $changed=Apply-UpdatePlan $plan $Install $backup
 # The new BlueAsh must open its window (class ModeSwitch) within 30 s and still have it 5 s later; if it never does,
 # the replaced files go back and the previous version starts again (backup kept under update-backups).
 Report 'working' (U '새 버전이 잘 켜지는지 확인하는 중…') 1
 $new=Start-Process -FilePath (Join-Path $Install 'BlueAsh.exe') -WorkingDirectory $Install -PassThru
 $alive=$false;$until=[DateTime]::UtcNow.AddSeconds(30)
 while([DateTime]::UtcNow -lt $until){Start-Sleep -Milliseconds 500
  if(Test-NewWindow){Start-Sleep -Seconds 5;$alive=Test-NewWindow;break}}
 if(!$alive){
  try{if(!$new.HasExited){$new.Kill();[void]$new.WaitForExit(5000)}}catch{}
  foreach($p in Get-Process BlueAsh -ErrorAction SilentlyContinue){try{if($p.Path -and $p.Path -ieq (Join-Path $Install 'BlueAsh.exe')){$p.Kill();[void]$p.WaitForExit(5000)}}catch{}}
  $restored=Restore-UpdatePlan $changed
  throw (U $(if($restored){'새 버전이 정상적으로 켜지지 않아 이전 버전으로 되돌렸습니다.'}else{'새 버전이 켜지지 않았고 이전 버전 복구도 일부 실패했습니다. 백업 위치: {0}'}) @($backup))
 }
 # Installed with the setup program (installer/BlueAsh.iss, same AppId): Control Panel reads the version from the setup's
 # uninstall entry, so it follows the update. Only an entry that points at this install folder is touched.
 try{$entry='HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{6B0F3E2A-8C1D-4E5B-9A7F-B1A5E0A5C001}_is1';$loc=[string](Get-ItemPropertyValue -LiteralPath $entry -Name 'InstallLocation' -ErrorAction Stop)
  if($loc.TrimEnd('\') -ieq $Install.TrimEnd('\')){Set-ItemProperty -LiteralPath $entry -Name 'DisplayVersion' -Value $version.ToString()}}catch{}
 # Only the backup of the version just replaced is worth keeping; older ones only filled the disk (170 MB here).
 Get-ChildItem -LiteralPath (Split-Path $backup) -Directory -ErrorAction SilentlyContinue|Where-Object{$_.FullName -ne $backup}|ForEach-Object{Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction SilentlyContinue}
 Report 'done' (U '업데이트 완료 · {0}' @($version)) 1
}catch{
 try{Report 'error' (Update-Error $_.Exception) -1 (U '기존 프로그램과 설정은 그대로입니다.')}catch{}
 # Files are rolled back on a failed swap; bring the (old) BlueAsh back so the reason shows there.
 if($closed){try{Start-Process -FilePath (Join-Path $Install 'BlueAsh.exe') -WorkingDirectory $Install}catch{}}
}finally{
 if($http){$http.Dispose()}
 if($temp -and (Test-Path $temp)){Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue}
 if($locked){$guard.ReleaseMutex()};$guard.Dispose()
}
