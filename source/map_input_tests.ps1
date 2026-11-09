$ErrorActionPreference='Stop';function Assert($ok,$why){if(!$ok){throw $why}}
. (Join-Path $PSScriptRoot 'helper_test_load.ps1')
# The map key's search-screen check (flea market / item search take the key): RequestMapOpen / PollMapRead in the helper.
$src=Helper-Source
$poll=[regex]::Match($src,'(?s)void PollMapRead\(IntPtr foreground\)\{.*?\n  \}').Value;$open=[regex]::Match($src,'(?s)void RequestMapOpen\(IntPtr game\)\{.*?\n  \}').Value
Assert ($poll -and $open) 'Map opening has a cancellable search-screen check'
Assert ($open.Contains('if(map.Visible){map.HideMap();return;}')) 'Closing a visible map never waits for screen recognition'
Assert ($open.Contains('job.Task=Task.Run(()=>WinOcr.Read(e,bytes))')) 'The screen is read on a pool thread: pending OCR never blocks the helper'
Assert ($open.Contains('if(raidPhase=="started"&&!ClickTrigger.CursorShown()){LoadMap();map.ShowOver(game);return;}')) 'In a raid without a cursor the map opens at once'
Assert ($poll.Contains('if(!mapsOn||foreground!=job.Game||map.Visible||DateTime.UtcNow>job.Until)job.Discard=true;')) 'Switching away, disabled maps, an open map or an expired result discard the request'
Assert ($poll.Contains('if(!job.Task.IsCompleted)return;')) 'A pending result is only polled'
Assert ($poll.Contains('bool blocked=MapSearch.Blocked(') -and $poll.Contains('if(!blocked&&mapsOn&&!map.Visible&&foreground==job.Game&&foreground!=IntPtr.Zero){LoadMap();map.ShowOver(foreground);}')) 'Flea/item search blocks only this request; ordinary screens open the map'
Assert ($poll.Contains('finally{DisposeMapRead(job);}')) 'The captured screen is released whatever happens'
# Real non-blocking OCR task on a small image.
$engine=[ModePulse.WinOcr]::Engine('en');if(!$engine){$engine=[ModePulse.WinOcr]::Engine('ko')}
$im=New-Object Drawing.Bitmap(320,80);$g=[Drawing.Graphics]::FromImage($im);$g.Clear([Drawing.Color]::White);$g.DrawString('ITEM SEARCH',(New-Object Drawing.Font('Arial',20)),[Drawing.Brushes]::Black,10,20);$ms=New-Object IO.MemoryStream;$im.Save($ms,[Drawing.Imaging.ImageFormat]::Png);$g.Dispose();$im.Dispose()
$w=[Diagnostics.Stopwatch]::StartNew();$t=[ModePulse.WinOcr]::Read($engine,$ms.ToArray());$back=$w.ElapsedMilliseconds
Assert ($back -lt 100) ('Starting the read returns at once: '+$back+' ms');$null=$t.Wait(10000);Assert ($t.Result.Text -match 'SEARCH') ('Read finishes on its own: '+$t.Result.Text)
Write-Output 'PASS: nonblocking OCR, search-only block, normal menu allowance, foreground/disabled/timeout cancellation, immediate map close'
