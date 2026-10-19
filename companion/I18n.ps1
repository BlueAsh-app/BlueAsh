# Shared companion UI catalog. Game identifiers, OCR text and saved user data are never translated.
param([string]$UiLocale='')
$script:uiLanguage=$(if($UiLocale){$UiLocale}else{[string][Microsoft.Win32.Registry]::GetValue('HKEY_CURRENT_USER\Software\ModeSwitch','Language','ko')})
$script:uiLanguages=@('ko','en','zh-CN','ja','ru','es','pt-BR','de','fr','it','pl','tr','zh-TW')
if($script:uiLanguage -notin $script:uiLanguages){$script:uiLanguage='ko'}
$script:uiTranslations=@{}
$column=[array]::IndexOf($script:uiLanguages,$script:uiLanguage)
if($column){foreach($line in [IO.File]::ReadAllLines((Join-Path $PSScriptRoot 'Localization.tsv'),[Text.Encoding]::UTF8)){
 if(!$line -or $line.StartsWith('#')){continue};$cells=$line.Split([char]9);if($cells.Length -le $column){continue}
 $key=$cells[0].Replace('\r\n',"`r`n").Replace('\n',"`n").Replace('\u0020',' ');$script:uiTranslations[$key]=$cells[$column].Replace('\r\n',"`r`n").Replace('\n',"`n").Replace('\u0020',' ')
}}
function T([string]$text,[object[]]$values=@()){
 $text=$text.Replace('\r\n',"`r`n").Replace('\n',"`n")
 $translated=$text;if($script:uiTranslations.ContainsKey($text)){$translated=[string]$script:uiTranslations[$text]}
 if($values.Count){return [string]::Format([Globalization.CultureInfo]::GetCultureInfo($script:uiLanguage),$translated,$values)};return $translated
}
