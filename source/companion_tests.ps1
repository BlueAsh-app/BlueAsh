$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../companion/CompanionCore.ps1')
function Assert($ok,$name){if(!$ok){throw $name}}
foreach($f in Get-ChildItem (Join-Path $PSScriptRoot '../companion') -Filter '*.ps1'){$t=$null;$e=$null;$null=[Management.Automation.Language.Parser]::ParseFile($f.FullName,[ref]$t,[ref]$e);Assert ($e.Count -eq 0) ($e|Out-String)}
Assert (Valid-Webhook 'https://discord.com/api/webhooks/123/abc_DEF-123') 'valid webhook'
Assert (!(Valid-Webhook 'https://discord.com.evil.test/api/webhooks/123/abc')) 'host confusion'
Assert (!(Valid-Webhook 'http://discord.com/api/webhooks/123/abc')) 'HTTPS required'
Assert ((Classify-Notice 'We have started technical maintenance on the Escape from Tarkov servers') -eq 'maintenance.started') 'start'
Assert ((Classify-Notice 'The technical maintenance has been extended') -eq 'maintenance.extended') 'extension'
Assert ((Classify-Notice 'The technical maintenance is complete') -eq 'maintenance.completed') 'complete'
Assert ((Classify-Notice 'The website maintenance is complete') -eq '') 'website excluded'
Assert ((Classify-Notice 'Tomorrow we plan technical maintenance') -eq '') 'planned is not started'
Assert ((Classify-Notice 'HTTP 403') -eq '') 'network failure is unknown'
$rows=@([pscustomobject]@{Name='오래된 주유소'},[pscustomobject]@{Name='기숙사 차량 탈출구'})
$hits=@(Match-Exits @('EXFIL 03 오래된 주유소','엉뚱한 이름') $rows)
Assert ($hits.Count -eq 1 -and $hits[0] -eq '오래된 주유소') 'exact aliases only'
$rows=@([pscustomobject]@{Name='Crossroads'},[pscustomobject]@{Name='Gate 3'},[pscustomobject]@{Name='Gate 0'},[pscustomobject]@{Name='ZB-1011'},[pscustomobject]@{Name='Old Gas Station Gate'})
$hits=@(Match-Exits @('EXFIL01 Cr0ssroad5 12:30','EXFIL02 Gate 3','ZB-1O11','0ld Gas Statlon Gat') $rows)
Assert ($hits.Count -eq 4 -and $hits -contains 'Crossroads' -and $hits -contains 'Gate 3' -and $hits -contains 'ZB-1011' -and $hits -contains 'Old Gas Station Gate' -and $hits -notcontains 'Gate 0') 'OCR-tolerant English exit names'
$hits=@(Match-Exits @('EXFIL03 Railroad Passage') @([pscustomobject]@{Name='Railroad Passage (조명탄 탈출구)'}))
Assert ($hits.Count -eq 1) 'Korean bracket note ignored'
$rows=@('Trailer Park','Dorms V-Ex','ZB-1011','Smugglers'' Bunker (ZB-1012)','Old Gas Station','ZB-1013','Crossroads','Transit to Factory','Transit to Reserve','Transit to Interchange','Boiler Room Basement (Co-op)','Railroad Passage (Flare)','Transit to Shoreline')|ForEach-Object {[pscustomobject]@{Name=$_}}
$ocr=@('탈출지점01 00「ms V-EX','탈출지점02 7+1013','탈출지점03 7+1011','탈출지점04 Old Gas station','탈출지점05 Railroad Passage (조명탄 탈출구)','탈출지점06 Boile「 Room Basement (CO-叩)','지역이동01 리저브로 이동','월 지역이동02 공장으로 이동','지역이동03 인터체인지로 이동','지역이동03 해안선으로 이동')
$hits=@(Match-Exits $ocr $rows)
Assert ($hits.Count -eq 10 -and $hits -notcontains 'Trailer Park' -and $hits -notcontains 'Crossroads' -and $hits -notcontains 'Smugglers'' Bunker (ZB-1012)') ('real Customs exit list: '+($hits -join ', '))
$hits=@(Match-Exits @('탈출지점 03 7+1이1','탈출지점02 z바101로~') @([pscustomobject]@{Name='ZB-1011'},[pscustomobject]@{Name='ZB-1013'}))
Assert ($hits.Count -eq 1 -and $hits[0] -eq 'ZB-1011') ('look-alike digits: '+($hits -join ', '))
$hits=@(Match-Exits @('탈출지점04 Old Gas Station','탈출지점01 Railroad') @([pscustomobject]@{Name='ZB-1011'},[pscustomobject]@{Name='ZB-1013'}))
Assert ($hits.Count -eq 0) ('words are not digits: '+($hits -join ', '))
'PASS: script parsing, webhook restriction, notice state classification, extraction-name matching (OCR-tolerant)'
