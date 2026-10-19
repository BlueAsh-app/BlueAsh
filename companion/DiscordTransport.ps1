# Network transport is asynchronous; no game memory, injection or keyboard hook.
# Two routes: the BlueAsh relay (one-click Discord connect, alerts arrive as bot DMs) or an advanced channel webhook.
$handler=New-Object Net.Http.HttpClientHandler;$handler.AllowAutoRedirect=$false
$http=New-Object Net.Http.HttpClient($handler);$http.Timeout=[TimeSpan]::FromSeconds(20);$http.MaxResponseContentBufferSize=2000000
$queue=New-Object 'System.Collections.Generic.Queue[object]';$script:sendTask=$null;$script:sendItem=$null;$script:sendAfter=[DateTime]::MinValue;$script:disabledTransport=$false
$script:claimState=$null;$script:claimTask=$null;$script:claimUntil=[DateTime]::MinValue;$script:nextClaim=[DateTime]::MinValue
function Enqueue($game,$type,$id,$text){if($queue.Count -lt 32){$queue.Enqueue(@{game=$game;type=$type;id=$id;text=$text;expires=[DateTime]::UtcNow.AddMinutes(3);tries=0;target=$script:activeUrl;relay=$script:relayKey})}}
function Update-Link {
 if($script:relayKey){$linkLabel.Text=(T '연결됨 · 알림은 BlueAsh 봇이 DM으로 보냅니다.');$connectBtn.Text=(T 'Discord 연결')}
 elseif($script:claimState){$linkLabel.Text=(T '브라우저에서 Discord 승인 대기 중…');$connectBtn.Text=(T 'Discord 연결')}
 elseif($hook.Text.Trim()){$linkLabel.Text=$(if($relay){(T '고급 웹후크로 전송합니다.')}else{(T '웹후크 채널로 알림을 보냅니다.')});$connectBtn.Text=(T 'Discord 연결')}
 elseif(!$relay){$linkLabel.Text=(T '아래 웹후크의 채널로 알림이 옵니다.');$connectBtn.Text=(T 'Discord 연결')}
 else{$linkLabel.Text=(T '아직 연결 안 됨 · 오른쪽 버튼을 눌러 디스코드에서 승인하세요.');$connectBtn.Text=(T 'Discord 연결')}
}
function Start-Connect {
 if(!$relay){DStatus (T '간편 연결이 아직 설정되지 않은 빌드입니다. 고급 웹후크를 사용하세요.');return}
 $b=New-Object byte[] 16;$rng=[Security.Cryptography.RandomNumberGenerator]::Create();$rng.GetBytes($b);$rng.Dispose()
 $script:claimState=-join($b|ForEach-Object {$_.ToString('x2')});$script:claimUntil=[DateTime]::UtcNow.AddMinutes(5);$script:nextClaim=[DateTime]::UtcNow.AddSeconds(3)
 # Default browser: one Discord approval click (log in first if needed). Joining the server and DMs follow automatically.
 Start-Process ($relay+'/connect?state='+$script:claimState+'&lang='+$script:uiLanguage)
 DStatus (T 'Discord 승인 창을 열었습니다. 승인하면 자동으로 연결됩니다. (5분 대기)');Update-Link
}
$connectBtn.add_Click({Start-Connect})
# ---- 휴대폰 알림 (web push through the relay: relay/phone.js) ----
# A random pairing code made here (DPAPI-saved like the Discord key). The PC opens /phone/pair (a QR); the phone allows
# notifications there. Alerts are POST /phone/notify {c, kind}; the relay owns the wording.
$script:phoneCode=Unprotect $config.phoneCode;$script:phonePaired=$false;$script:phoneChecked=$false;$script:phoneWatchUntil=[DateTime]::MinValue;$script:phoneNext=[DateTime]::MinValue;$script:phoneStatusTask=$null;$script:phoneSendTask=$null;$script:phoneSendContent=$null
function Update-Phone {
 $phoneLabel.Text=$(if(!$relay){(T '이 빌드에는 휴대폰 알림 서버가 없습니다.')}elseif($script:phonePaired){(T '연결됨 · 이 휴대폰으로 바로 알림이 옵니다.')}elseif([DateTime]::UtcNow -lt $script:phoneWatchUntil){(T '휴대폰에서 알림 허용을 기다리는 중…')}else{(T '아직 연결 안 됨 · 오른쪽 버튼을 눌러 주세요.')})
}
function Start-Phone {
 if(!$relay){return}
 if(!$script:phoneCode){$b=New-Object byte[] 16;$rng=[Security.Cryptography.RandomNumberGenerator]::Create();$rng.GetBytes($b);$rng.Dispose();$script:phoneCode=-join($b|ForEach-Object {$_.ToString('x2')})
  $config.phoneCode=Protect $script:phoneCode;$config|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8}
 $script:phoneWatchUntil=[DateTime]::UtcNow.AddMinutes(10);$script:phoneNext=[DateTime]::UtcNow.AddSeconds(3)
 Update-Phone;Show-PhonePair
}
# The QR in BlueAsh's own window (the relay draws it: /phone/qr.gif). "Connected" means a phone subscribed after the
# window opened (/phone/status "at" changed): a PC that is already paired used to show "connected" a moment later.
function Show-PhonePair {
 $pairUrl=$relay+'/phone/pair?c='+$script:phoneCode+'&lang='+$script:uiLanguage
 $f=New-Object Windows.Forms.Form;$f.Text=(T 'BlueAsh · 휴대폰 알림 연결');[ModePulse.Brand]::Apply($f,$appExe);$f.StartPosition='CenterScreen';$f.FormBorderStyle='FixedDialog';$f.MaximizeBox=$false;$f.MinimizeBox=$false;$f.Font=$uiFont;$f.TopMost=$true
 $f.ClientSize=New-Object Drawing.Size(420,560)
 $title=New-Object Windows.Forms.Label;$title.Text=(T '휴대폰 알림 연결');$title.Font=New-Object Drawing.Font($fontName,15,[Drawing.FontStyle]::Bold);$title.SetBounds(24,20,372,34);$f.Controls.Add($title)
 $how=New-Object Windows.Forms.Label;$how.Text=(T '휴대폰 카메라로 QR을 찍고, 열린 페이지에서 알림 허용을 누르세요.');$how.SetBounds(24,58,372,44);$f.Controls.Add($how)
 # White plate behind the code: phone cameras read a dark-on-light QR best.
 $plate=New-Object ModePulse.ThemeCard;$plate.SetBounds(70,112,280,280);$f.Controls.Add($plate)
 $pic=New-Object Windows.Forms.PictureBox;$pic.SetBounds(14,14,252,252);$pic.SizeMode='Zoom';$plate.Controls.Add($pic)
 $state=New-Object Windows.Forms.Label;$state.Text=(T 'QR을 불러오는 중…');$state.TextAlign='MiddleCenter';$state.SetBounds(24,404,372,48);$f.Controls.Add($state)
 $open=New-Object ModePulse.ThemeButton;$open.Text=(T '브라우저로 열기');$open.SetBounds(24,496,180,40);$f.Controls.Add($open);$open.add_Click({Start-Process $pairUrl})
 $close=New-Object ModePulse.ThemeButton;$close.Text=(T '닫기');$close.SetBounds(276,496,120,40);$f.Controls.Add($close);$close.add_Click({$f.Close()});$f.CancelButton=$close
 Apply-Theme $f;$plate.BackColor=[Drawing.Color]::White;$pic.BackColor=[Drawing.Color]::White;$how.ForeColor=[ModePulse.Theme]::Muted
 # Buttons in the same size as the text around them (the form font is larger than the rest of the app reads).
 $small=New-Object Drawing.Font($fontName,9.5);foreach($c in $open,$close,$how,$state){$c.Font=$small}
 $script:pairQr=$http.GetByteArrayAsync($relay+'/phone/qr.gif?c='+$script:phoneCode+'&lang='+$script:uiLanguage);$script:pairStatus=$null;$script:pairBase=$null;$script:pairDone=$false;$script:pairAt=[DateTime]::MinValue;$script:pairNext=[DateTime]::MinValue
 $timer=New-Object Windows.Forms.Timer;$timer.Interval=250
 $timer.add_Tick({
  if($script:pairQr -and $script:pairQr.IsCompleted){try{$ms=New-Object IO.MemoryStream(,$script:pairQr.GetAwaiter().GetResult());$pic.Image=[Drawing.Image]::FromStream($ms)}catch{$state.Text=(T 'QR을 불러오지 못했습니다. 브라우저로 열기를 눌러 주세요.')};$script:pairQr=$null}
  if($script:pairStatus -and $script:pairStatus.IsCompleted){
   try{$r=$script:pairStatus.GetAwaiter().GetResult();try{if($r.IsSuccessStatusCode){$j=$r.Content.ReadAsStringAsync().GetAwaiter().GetResult()|ConvertFrom-Json;$at=[double]$j.at
    if($null -eq $script:pairBase){$script:pairBase=$at;$state.Text=$(if($j.paired){(T '이미 연결된 휴대폰이 있습니다. 다른 휴대폰으로 바꾸려면 QR을 찍으세요.')}else{(T '휴대폰에서 알림 허용을 기다리는 중…')})}
    elseif($j.paired -and $at -ne $script:pairBase -and !$script:pairDone){$script:pairDone=$true;$script:phonePaired=$true;$script:phoneWatchUntil=[DateTime]::MinValue;Update-Phone
     $state.ForeColor=[Drawing.Color]::FromArgb(95,211,141);$state.Text=(T '연결 완료! 휴대폰에 확인 알림이 왔는지 보세요.');DStatus (T '휴대폰 알림 연결 완료 · 휴대폰에 확인 알림이 왔는지 보세요.');$script:pairAt=[DateTime]::UtcNow}}}finally{$r.Dispose()}}catch{}
   $script:pairStatus=$null}
  if(!$script:pairStatus -and !$script:pairDone -and [DateTime]::UtcNow -ge $script:pairNext){$script:pairNext=[DateTime]::UtcNow.AddSeconds(2);$script:pairStatus=$http.GetAsync($relay+'/phone/status?c='+$script:phoneCode)}
  if($script:pairDone -and ([DateTime]::UtcNow-$script:pairAt).TotalSeconds -gt 3){$f.Close()}
 })
 $f.add_FormClosed({$timer.Stop();$timer.Dispose();if($pic.Image){$pic.Image.Dispose()}})
 $timer.Start();[void]$f.ShowDialog()
}
$phoneBtn.add_Click({Start-Phone})
function Phone-Send([string]$kind,$values=$null){
 if(!$relay -or !$script:phoneCode -or !$script:phonePaired -or $script:phoneSendTask){return $false}
 $body=@{c=$script:phoneCode;kind=$kind;language=$script:uiLanguage};if($values){$body.values=@($values|ForEach-Object {[string]$_})}
 $script:phoneSendContent=New-Object Net.Http.StringContent(($body|ConvertTo-Json -Compress),[Text.Encoding]::UTF8,'application/json')
 $script:phoneSendTask=$http.PostAsync($relay+'/phone/notify',$script:phoneSendContent);return $true
}
function Phone-Pump {
 if($script:phoneStatusTask -and $script:phoneStatusTask.IsCompleted){
  try{$r=$script:phoneStatusTask.GetAwaiter().GetResult();try{if($r.IsSuccessStatusCode){$was=$script:phonePaired;$script:phonePaired=[bool](($r.Content.ReadAsStringAsync().GetAwaiter().GetResult()|ConvertFrom-Json).paired)
   if($script:phonePaired -and !$was -and [DateTime]::UtcNow -lt $script:phoneWatchUntil){$script:phoneWatchUntil=[DateTime]::MinValue;DStatus (T '휴대폰 알림 연결 완료 · 휴대폰에 확인 알림이 왔는지 보세요.')}}}finally{$r.Dispose()}}catch{}
  $script:phoneStatusTask=$null;$script:phoneChecked=$true;Update-Phone
 }
 # Ask once at start (already paired?), then every 3 s while waiting for the phone.
 if(!$script:phoneStatusTask -and $relay -and $script:phoneCode -and [DateTime]::UtcNow -ge $script:phoneNext -and (!$script:phoneChecked -or [DateTime]::UtcNow -lt $script:phoneWatchUntil)){
  $script:phoneNext=[DateTime]::UtcNow.AddSeconds(3);$script:phoneStatusTask=$http.GetAsync($relay+'/phone/status?c='+$script:phoneCode)}
 if($script:phoneSendTask -and $script:phoneSendTask.IsCompleted){
  try{$r=$script:phoneSendTask.GetAwaiter().GetResult();try{$code=[int]$r.StatusCode
   if($r.IsSuccessStatusCode){DStatus (T '휴대폰으로 알림을 보냈습니다.')}
   elseif($code -eq 410){$script:phonePaired=$false;Update-Phone;DStatus (T '휴대폰 연결이 끊겼습니다 (알림을 껐거나 브라우저 데이터를 지움). 휴대폰 연결을 다시 눌러 주세요.')}
   elseif($code -eq 429){DStatus (T '휴대폰 알림은 1분에 1번까지 보냅니다.')}
   else{DStatus ((T '휴대폰 알림 전송 실패 · 서버 응답 ')+$code)}
  }finally{$r.Dispose()}}catch{DStatus (T '휴대폰 알림 전송 실패 · 인터넷 연결을 확인하세요.')}
  $script:phoneSendTask=$null;if($script:phoneSendContent){$script:phoneSendContent.Dispose();$script:phoneSendContent=$null}
 }
}
Update-Phone
# Turning the alert on with nothing connected starts the phone pairing (the main route).
$discordCheck.add_CheckedChanged({if($discordCheck.Checked -and !$script:phonePaired -and !$script:relayKey -and !$hook.Text.Trim() -and $relay){Start-Phone}})
$testBtn.add_Click({
 # With the relay: the phone only. A build without the relay tests the channel webhook.
 if($relay){if(Phone-Send 'test'){DStatus (T '휴대폰으로 테스트 알림을 보내는 중…')}else{DStatus (T '먼저 휴대폰 연결을 눌러 휴대폰을 연결해 주세요.')};return}
 if(!(Valid-Webhook $hook.Text.Trim())){DStatus (T '먼저 웹후크 주소를 넣어 주세요. (웹후크 만드는 법 참고)');return}
 $script:activeUrl=$hook.Text.Trim();$script:disabledTransport=$false;Enqueue 'system' 'test' ([Guid]::NewGuid().ToString()) (T 'BlueAsh 테스트 알림입니다.')
})
function Claim-Pump {
 if($script:claimTask -and $script:claimTask.IsCompleted){
  try{$r=$script:claimTask.GetAwaiter().GetResult();try{
   if([int]$r.StatusCode -eq 200){$key=($r.Content.ReadAsStringAsync().GetAwaiter().GetResult()|ConvertFrom-Json).key
    if($key){$script:relayKey=[string]$key;$script:claimState=$null;$script:disabledTransport=$false
     if(!$discordCheck.Checked){$discordCheck.Checked=$true}
     Update-Link;if(Save-Config){DStatus (T 'Discord 연결 완료 · BlueAsh 서버에 들어갔고 DM으로 알림을 받습니다. 테스트 전송으로 확인해 보세요.')}}}
  }finally{$r.Dispose()}}catch{}
  $script:claimTask=$null
 }
 if($script:claimState -and !$script:claimTask -and [DateTime]::UtcNow -ge $script:nextClaim){
  if([DateTime]::UtcNow -gt $script:claimUntil){$script:claimState=$null;DStatus (T 'Discord 승인 대기 시간이 지났습니다. 다시 연결을 눌러 주세요.');Update-Link;return}
  $script:nextClaim=[DateTime]::UtcNow.AddSeconds(2);$script:claimTask=$http.GetAsync($relay+'/claim?state='+$script:claimState)
 }
}
function Network-Pump {
 Claim-Pump;Phone-Pump
 if($script:sendTask -and $script:sendTask.IsCompleted){
  $viaRelay=[bool]$script:sendItem.relay
  try{$r=$script:sendTask.GetAwaiter().GetResult();try{$code=[int]$r.StatusCode
   if($r.IsSuccessStatusCode){DStatus (T 'Discord 전송 완료')}
   elseif($viaRelay -and $code -eq 429){DStatus (T 'Discord 알림 간격 제한 (1분에 1회) · 이번 알림은 건너뜁니다.')}
   elseif($viaRelay -and $code -eq 401){$script:relayKey='';$config.relayKey='';$config|ConvertTo-Json|Set-Content $settingsPath -Encoding UTF8;Update-Link;DStatus (T 'Discord 연결이 만료되었습니다. 다시 연결을 눌러 주세요.')}
   elseif($viaRelay){DStatus (T 'Discord DM 전송 실패 · BlueAsh 서버를 나갔거나 서버 멤버 DM이 꺼져 있습니다. 다시 연결하거나 Discord 개인정보 설정을 확인하세요.')}
   elseif($code -eq 429){$body=$r.Content.ReadAsStringAsync().GetAwaiter().GetResult()|ConvertFrom-Json;$wait=[Math]::Max(1,[double]$body.retry_after);$script:sendAfter=[DateTime]::UtcNow.AddSeconds($wait);if($script:sendItem.tries -lt 3){$queue.Enqueue($script:sendItem)};DStatus (T 'Discord 요청 제한 · 지정된 시간 이후 재시도')}
   elseif($code -in @(401,403,404)){$script:disabledTransport=$true;DStatus (T 'Discord 권한/주소 오류 · 주소 수정 후 테스트 버튼을 눌러 주세요.')}
   else{DStatus ((T 'Discord 실패 HTTP ')+$code)}
  }finally{$r.Dispose()}}catch{DStatus (T 'Discord 전송 실패 · 자동 중복 전송하지 않습니다.')}
  $script:sendTask=$null;$script:sendItem=$null;if($script:sendContent){$script:sendContent.Dispose();$script:sendContent=$null}
 }
 if(!$script:sendTask -and $queue.Count -gt 0 -and [DateTime]::UtcNow -ge $script:sendAfter){
  $item=$queue.Dequeue();if($item.type -eq 'raid.started.away' -and !$config.discord){return};if($item.type -like 'maintenance.*' -and !$config.maintenance){return};if($item.expires -lt [DateTime]::UtcNow){return}
  $item.tries++
  if($item.relay -and $relay){
   # The relay owns the wording; only official notice text is passed along as detail.
   $detail=if($item.type -like 'maintenance.*'){[string]$item.text}else{''}
   $payload=@{key=$item.relay;kind=$item.type;detail=$detail;language=$script:uiLanguage}|ConvertTo-Json -Compress;$url=$relay+'/notify'
  }else{
   if($script:disabledTransport){return};$url=$item.target;if(!(Valid-Webhook $url)){DStatus (T 'Discord 연결 또는 웹후크가 설정되지 않았습니다.');return}
   $payload=@{content=$item.text;allowed_mentions=@{parse=@()}}|ConvertTo-Json -Depth 4 -Compress;$url=$url+'?wait=true'
  }
  $content=New-Object Net.Http.StringContent($payload,[Text.Encoding]::UTF8,'application/json')
  $script:sendContent=$content;$script:sendItem=$item;$script:sendTask=$http.PostAsync($url,$content);$script:sendAfter=[DateTime]::UtcNow.AddSeconds(2)
 }
}
Update-Link
