$ErrorActionPreference='Stop'
function Assert($ok,$why){if(!$ok){throw $why}}
. (Join-Path $PSScriptRoot 'helper_test_load.ps1')
# PriceApp.ShowPrice (the price helper): the price and the key button come first; quest / hideout tags only from a ready
# background history snapshot (QuestHistory never reads the logs on this thread: price_history_tests).
$src=Price-Source
$show=[regex]::Match($src,'(?s)void ShowPrice\(Hit hit,Slot slot\)\{.*?\n  \}').Value
Assert ($show) 'ShowPrice exists'
$price=$show.IndexOf('slot.Price.ShowAt(');$key=$show.IndexOf('slot.Key.ShowFor(');$ready=$show.IndexOf('if(history.Ready){');$done=$show.IndexOf('history.Completed')
Assert ($price -gt 0 -and $key -gt $price -and $ready -gt $key) 'Price, then key button, before any quest history use'
Assert ($done -gt $ready) 'Completed quests are read only from a ready snapshot'
Assert ($show.Contains('history.RefreshIfStale();') -and $show.Contains('}else{slot.Quest.Hide();slot.Hide.Hide();}')) 'A pending snapshot starts in the background and hides the optional tags meanwhile'
# The history object itself: a cold one is not ready and starting it returns at once.
$h=New-Object ModePulse.QuestHistory((Join-Path $PSScriptRoot '.bundle/no-logs-here'),'pve');$w=[Diagnostics.Stopwatch]::StartNew();$h.RefreshIfStale();$t=$w.ElapsedMilliseconds
Assert ($t -lt 200) ('Starting the history read does not block: '+$t+' ms')
Write-Output 'PASS: pending history never delays price/key overlay; ready metadata retained; key shown before optional tags'
