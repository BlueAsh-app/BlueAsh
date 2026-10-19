function Get-Field($Object,[string]$Name) {
 if($null -eq $Object){return $null}
 if($Object -is [System.Collections.IDictionary]){return $Object[$Name]}
 $p=$Object.PSObject.Properties[$Name];if($null -ne $p){return $p.Value};return $null
}
# Last "Session mode:" line in the game's application log. Each mode keeps its own endpoint.
function Get-LogMode([string]$Text) {
 $m=[regex]::Matches($Text,'\|application\|Session mode: (\w+)');if($m.Count -eq 0){return ''}
 switch -CaseSensitive ($m[$m.Count-1].Groups[1].Value){'Pve'{return 'pve'}'PvpSeason'{return 'pvp-season'}'Regular'{return 'regular'}};return ''
}
function Read-LogMode([string]$Root) {
 if(!$Root -or !(Test-Path -LiteralPath $Root -PathType Container)){return ''}
 $dir=Get-ChildItem -LiteralPath $Root -Directory -Filter 'log_*'|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 1;if(!$dir){return ''}
 foreach($f in @(Get-ChildItem -LiteralPath $dir.FullName -File -Filter '*application*.log'|Sort-Object Name -Descending)){
  if($f.Length -gt 16777216){continue}
  # The game keeps the log open for writing; share access instead of locking it.
  $s=New-Object IO.FileStream($f.FullName,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]'ReadWrite,Delete')
  try{$text=(New-Object IO.StreamReader($s,[Text.Encoding]::UTF8)).ReadToEnd()}finally{$s.Dispose()}
  $mode=Get-LogMode $text;if($mode){return $mode}
 };return ''
}
# raid-state.txt from BlueAsh.exe: pid, map, role, raid id, phase, serial, tick. 'started' = inside a raid.
function Read-RaidPhase([string]$Path,[int]$ParentId) {
 try{$s=@([IO.File]::ReadAllLines($Path,[Text.Encoding]::UTF8));if($s.Count -ge 5 -and $s[0] -eq [string]$ParentId){return $s[4]}}catch{};return 'unknown'
}
function Get-PriceState($Item,[string]$Mode) {
 if($null -eq $Item -or (Get-Field $Item 'mode') -ne $Mode){return 'invalid'}
 if((Get-Field $Item 'banned') -eq $true){return 'banned'}
 $price=Get-Field $Item 'price';$offers=Get-Field $Item 'offers'
 if($null -ne $offers -and [double]$offers -eq 0){return 'no-price'}
 if($null -eq $price -or [double]$price -le 0){return 'no-price'}
 return 'price'
}
