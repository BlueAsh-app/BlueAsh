if(!(Get-Command T -ErrorAction SilentlyContinue)){. (Join-Path $PSScriptRoot 'I18n.ps1')}
# Read-only parser for EFT's local Control.ini. Never writes game settings.
function Convert-EftKey([string]$Name){
 if($Name -cmatch '^[A-Z]$'){return [int][char]$Name}
 if($Name -match '^F([1-9]|1[0-5])$'){return 111+[int]$Matches[1]}
 if($Name -match '^Alpha([0-9])$'){return 48+[int]$Matches[1]}
 if($Name -match '^Keypad([0-9])$'){return 96+[int]$Matches[1]}
 $table=@{Mouse0=1;Mouse1=2;Mouse2=4;Mouse3=5;Mouse4=6;LeftShift=160;RightShift=161;LeftControl=162;RightControl=163;LeftAlt=164;RightAlt=165;Space=32;Tab=9;Return=13;Escape=27;Backspace=8;Insert=45;Delete=46;Home=36;End=35;PageUp=33;PageDown=34;UpArrow=38;DownArrow=40;LeftArrow=37;RightArrow=39;KeypadPlus=107;KeypadMinus=109;KeypadMultiply=106;KeypadDivide=111;KeypadPeriod=110;CapsLock=20;Numlock=144;ScrollLock=145}
 if($table.ContainsKey($Name)){return [int]$table[$Name]}
 throw ((T '지원하지 않는 키: ')+$Name)
}
function Read-EftExitBinding([string]$Json){
 $doc=ConvertFrom-Json -InputObject $Json
 $rows=@($doc.keyBindings|Where-Object keyName -eq 'WatchTimerAndExits')
 if($rows.Count -ne 1){throw (T '탈출구 확인 항목이 없거나 중복됨')}
 $row=$rows[0];$type=[string]$row.pressType
 if($type -notin @('Press','Release','DoubleClick')){throw ((T '지원하지 않는 입력 방식: ')+$type+(T ' (자동 인식 중지)'))}
 $timeout=0
 if($type -eq 'DoubleClick'){$timeout=[double]$doc.DoubleClickTimeout*1000;if($timeout -lt 10 -or $timeout -gt 2000){throw (T '더블클릭 간격을 확인할 수 없음')}}
 $variants=New-Object 'System.Collections.Generic.List[object]';$labels=New-Object 'System.Collections.Generic.List[string]'
 foreach($v in $row.variants){$names=@($v.keyCode);if($names.Count -eq 0){continue};$codes=@($names|ForEach-Object {Convert-EftKey ([string]$_)})
  $variants.Add([pscustomobject]@{Codes=$codes;Label=($names -join '+')});$labels.Add(($names -join '+'))
 }
 if($variants.Count -eq 0){throw (T '탈출구 확인 키가 지정되지 않음')}
 return [pscustomobject]@{Variants=$variants.ToArray();Type=$type;TimeoutMs=$timeout;Label=(($labels -join ' / ')+' · '+(T $type))}
}
function Update-EftTrigger($State,[bool]$Down,[double]$Now,$Binding){
 if(!$State.ContainsKey('down')){$State.down=$Down;$State.lastTap=-1000000000;return $false}
 $pressed=$Down -and !$State.down;$released=!$Down -and $State.down;$State.down=$Down
 switch($Binding.Type){
  'Press'{return $pressed}
  'Release'{return $released}
  'DoubleClick'{if($pressed){if($Now-$State.lastTap -le $Binding.TimeoutMs){$State.lastTap=-1000000000;return $true};$State.lastTap=$Now};return $false}
 };return $false
}
function Find-EftControlFile {
 $root=[Environment]::GetFolderPath('ApplicationData');$path=Join-Path $root 'Battlestate Games/Escape from Tarkov/Settings/Control.ini'
 if(Test-Path -LiteralPath $path -PathType Leaf){return $path}
 throw (T 'Control.ini를 찾지 못함 · 타르코프 설정을 저장한 뒤 재확인하세요.')
}
