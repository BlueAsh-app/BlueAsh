$ErrorActionPreference='Stop'
$root=Join-Path $PSScriptRoot '../companion'
function Assert($ok,$name){if(!$ok){throw $name}}
$catalog=@{}
foreach($line in [IO.File]::ReadAllLines((Join-Path $root 'Localization.tsv'),[Text.Encoding]::UTF8)){
 if(!$line -or $line.StartsWith('#')){continue};$cells=$line.Split([char]9)
 Assert ($cells.Count -eq 13) ('Catalog columns: '+$cells[0]);Assert (!$catalog.ContainsKey($cells[0])) ('Duplicate key: '+$cells[0])
 $catalog[$cells[0]]=$cells
 $slots=@([regex]::Matches($cells[0],'\{\d+\}')|ForEach-Object Value|Sort-Object)
 foreach($column in 1..4){Assert (![string]::IsNullOrWhiteSpace($cells[$column])) ('Empty translation: '+$cells[0]);$translated=@([regex]::Matches($cells[$column],'\{\d+\}')|ForEach-Object Value|Sort-Object);Assert (($slots -join ',') -eq ($translated -join ',')) ('Format placeholders: '+$cells[0])}
}
# Every Korean UI string the helper shows (T(...), I18n.Text/Format) has a catalog row.
foreach($file in Get-ChildItem $root -Filter 'Companion*.cs'|?{$_.Name -ne 'CompanionUI.cs'}){$code=[IO.File]::ReadAllText($file.FullName)
 foreach($m in [regex]::Matches($code,'(?:(?<![.\w])T|I18n\.Text|I18n\.Format)\("((?:[^"\x5C]|\x5C.)*)"')){$key=$m.Groups[1].Value.Replace('\"','"');if($key -notmatch '[가-힣]'){continue};Assert ($catalog.ContainsKey($key)) ('Missing UI translation: '+$key+' ('+$file.Name+')')}
}
$known=@{ko='저장';en='Save';'zh-CN'='保存';ja='保存';ru='Сохранить';es='Guardar';'pt-BR'='Salvar';de='Speichern';fr='Enregistrer';it='Salva';pl='Zapisz';tr='Kaydet';'zh-TW'='儲存'}
foreach($locale in 'ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'){
 $col=[array]::IndexOf(@('ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW'),$locale)
 function T([string]$text,[object[]]$values=@()){$text=$text.Replace('\r\n',"`r`n").Replace('\n',"`n");$k=$text.Replace("`r`n",'\r\n').Replace("`n",'\n');$t=$text;if($col -gt 0 -and $catalog.ContainsKey($k)){$t=$catalog[$k][$col].Replace('\r\n',"`r`n").Replace('\n',"`n")};if($values.Count){[string]::Format([Globalization.CultureInfo]::GetCultureInfo($locale),$t,$values)}else{$t}}
 Assert ((T '저장') -eq $known[$locale]) ('Locale selection: '+$locale)
 $formatted=T '저장됨 · 지어진 시설 {0} / {1}' @(2,9);Assert ($formatted -match '2 / 9' -and $formatted -notmatch '\{\d+\}') ('Formatted text: '+$locale)
 Assert ((T 'Dorms V-Ex') -eq 'Dorms V-Ex') ('Player/game names preserved: '+$locale)
 Assert ((T '탈출구 {0}곳 · 확인 키 → 사용 가능: 초록 파동 · 나머지: 회색\n초록: 탈출구 · 보라: 트랜짓 · 노랑: PMC · 빨강: 보스 · 검정: 컬티스트 · 주황 !: 퀘스트' @(4)).Contains("`n")) ('Multiline text: '+$locale)
}
. (Join-Path $PSScriptRoot 'helper_test_load.ps1');Assert ([ModePulse.I18n]::Codes.Count -eq 13 -and [ModePulse.I18n]::FontName -and [ModePulse.I18n]::Language) 'CompanionUI I18n starts (static order)'
'PASS: thirteen locales, full UI coverage, format arguments, parsing, and game-name preservation'
