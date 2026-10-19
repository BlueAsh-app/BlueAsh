. (Join-Path $PSScriptRoot 'UpdateLanguage.ps1')
function Get-UpdatePlan([string]$Zip,[string]$Stage) {
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $archive=[IO.Compression.ZipFile]::OpenRead($Zip)
 try {
  if($archive.Entries.Count -gt 500){throw (U '업데이트 파일 수 제한 초과')}
  $seen=New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
  $plan=New-Object 'System.Collections.Generic.List[object]';[long]$total=0
  foreach($e in $archive.Entries){
   $n=$e.FullName.Replace('\','/')
   if($n.EndsWith('/')){continue}
   # BlueAsh/ (2.9.0+, the ZIP given to users) or ModePulse/ (online updates, so 2.8.x updaters keep working);
   # either exe name is installed as BlueAsh.exe.
   $prefix=$(if($n.StartsWith('BlueAsh/')){8}elseif($n.StartsWith('ModePulse/')){10}else{0})
   if(!$prefix -or $n -match '(^|/)\.\.?(/|$)|:|\x00'){throw (U '잘못된 ZIP 경로')}
   $relative=$n.Substring($prefix);if($relative -eq 'ModePulse.exe'){$relative='BlueAsh.exe'}
   if($relative -notmatch '^(BlueAsh\.exe|README\.txt|FEATURES\.txt|NOTICE\.txt|LICENSE\.txt|BUILD-INFO\.txt|price/[A-Za-z0-9_.-]+|updater/[A-Za-z0-9_.-]+|source/[A-Za-z0-9_./-]+|companion/[A-Za-z0-9_.-]+|maps/[A-Za-z0-9_.-]+)$'){throw (U '허용되지 않는 파일: {0}' @($n))}
   if(!$seen.Add($relative)){throw (U '중복 ZIP 경로')}
   if((($e.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000){throw (U 'ZIP 링크는 허용되지 않습니다.')}
   $total+=$e.Length;if($e.Length -gt 64000000 -or $total -gt 128000000){throw (U 'ZIP 압축 해제 크기 제한 초과')}
   $dest=Join-Path $Stage ($relative.Replace('/',[IO.Path]::DirectorySeparatorChar))
   $plan.Add([pscustomobject]@{Entry=$e;Relative=$relative;Staged=$dest})
  }
  if(!$seen.Contains('BlueAsh.exe')){throw (U 'BlueAsh 업데이트 구성 파일 누락')}
  foreach($p in $plan){[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($p.Staged));[IO.Compression.ZipFileExtensions]::ExtractToFile($p.Entry,$p.Staged,$false)}
  $exe=Join-Path $Stage 'BlueAsh.exe';$f=[IO.File]::OpenRead($exe);try{if($f.ReadByte() -ne 77 -or $f.ReadByte() -ne 90){throw (U 'Windows 실행 파일 형식 오류')}}finally{$f.Dispose()}
  return @($plan | ForEach-Object {[pscustomobject]@{Relative=$_.Relative;Staged=$_.Staged}})
 }finally{$archive.Dispose()}
}
function Apply-UpdatePlan($Plan,[string]$Install,[string]$Backup) {
 $changed=New-Object 'System.Collections.Generic.List[object]'
 try {
  foreach($p in $Plan){
   $relative=$p.Relative.Replace('/',[IO.Path]::DirectorySeparatorChar)
   $dest=Join-Path $Install $relative;$old=Join-Path $Backup $relative
   # Never write through reparse points (junctions/symlinks) inside the install tree.
   $check=[IO.Path]::GetDirectoryName($dest)
   while($check.Length -ge $Install.TrimEnd('\','/').Length){
    if((Test-Path -LiteralPath $check) -and ((Get-Item -LiteralPath $check -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw (U '설치 경로에 링크가 있어 업데이트를 중단했습니다.')}
    $next=[IO.Path]::GetDirectoryName($check);if(!$next -or $next -eq $check){break};$check=$next
   }
   if((Test-Path -LiteralPath $dest) -and ((Get-Item -LiteralPath $dest -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)){throw (U '대상 파일이 링크입니다.')}
   $existed=Test-Path -LiteralPath $dest
   if($existed){[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($old));[IO.File]::Copy($dest,$old,$true)}
   [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest))
   $changed.Add([pscustomobject]@{Path=$dest;Old=$old;Existed=$existed})
   [IO.File]::Copy($p.Staged,$dest,$true)
  }
 }catch{
  $failure=$_;$rollbackFailed=$false
  for($i=$changed.Count-1;$i -ge 0;$i--){$p=$changed[$i];try{if($p.Existed){[IO.File]::Copy($p.Old,$p.Path,$true)}elseif(Test-Path -LiteralPath $p.Path){[IO.File]::Delete($p.Path)}}catch{$rollbackFailed=$true}}
  if($rollbackFailed){throw (U '복구 일부 실패. 백업 위치: {0} / {1}' @($Backup,(Update-Error $failure.Exception)))}
  throw $failure
 }
 return ,$changed
}
# Put the files an applied plan replaced back (the new version did not start); new files it added are removed.
function Restore-UpdatePlan($Changed){
 $failed=$false
 for($i=$Changed.Count-1;$i -ge 0;$i--){$p=$Changed[$i];try{if($p.Existed){[IO.File]::Copy($p.Old,$p.Path,$true)}elseif(Test-Path -LiteralPath $p.Path){[IO.File]::Delete($p.Path)}}catch{$failed=$true}}
 return !$failed
}
