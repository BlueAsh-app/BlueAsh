; BlueAsh setup (Inno Setup 6). Build with: powershell -ExecutionPolicy Bypass -File source/build-installer.ps1
; Per-user install (no administrator prompt) into %LOCALAPPDATA%\Programs\BlueAsh: the in-app updater replaces
; BlueAsh.exe there, so the folder must stay writable for the user. Settings (HKCU\Software\ModeSwitch,
; %LOCALAPPDATA%\ModeSwitch) are kept on uninstall unless the user says to remove them.
#ifndef AppVersion
  #error AppVersion is passed by build-installer.ps1
#endif
#ifndef Root
  #define Root ".."
#endif
#ifndef ExeSource
  #define ExeSource Root + "\BlueAsh.exe"
#endif
; BlueAsh's main window class (closed before install and uninstall). Tests pass /DWindowClass=ModeSwitchTest with a renamed
; test build, so a test run never closes the BlueAsh the user is running.
#ifndef WindowClass
  #define WindowClass "ModeSwitch"
#endif
; The same for BlueAsh's data folder and its administrator task (the defaults are the real ones).
#ifndef DataDir
  #define DataDir "{localappdata}\ModeSwitch"
#endif
#ifndef TaskName
  #define TaskName "\BlueAsh\Elevated"
#endif

[Setup]
AppId={{6B0F3E2A-8C1D-4E5B-9A7F-B1A5E0A5C001}
AppName=BlueAsh
AppVersion={#AppVersion}
AppVerName=BlueAsh {#AppVersion}
AppPublisher=BlueAsh
VersionInfoVersion={#AppVersion}.0
VersionInfoProductName=BlueAsh
VersionInfoDescription=BlueAsh Setup
DefaultDirName={localappdata}\Programs\BlueAsh
DefaultGroupName=BlueAsh
DisableProgramGroupPage=yes
; Always ask where to install (any drive or folder the user can write to; the updater replaces files there later).
DisableDirPage=no
AppPublisherURL=https://blueash.pages.dev
AppSupportURL=https://github.com/BlueAsh-app/BlueAsh/issues
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=BlueAsh-Setup-{#AppVersion}
SetupIconFile={#Root}\source\assets\app.ico
UninstallDisplayIcon={app}\BlueAsh.exe
UninstallDisplayName=BlueAsh
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=auto
LicenseFile={#Root}\LICENSE.txt
; What BlueAsh sends and downloads, shown before installing (SignPath Foundation terms: a privacy policy during installation).
InfoBeforeFile={#Root}\installer\privacy.txt
CloseApplications=no

[Languages]
Name: "ko"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "ja"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "ru"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "es"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"
Name: "fr"; MessagesFile: "compiler:Languages\French.isl"
Name: "it"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "pl"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"

[CustomMessages]
ko.RemoveSettings=BlueAsh 설정과 저장된 데이터(지도 핀, 은신처 레벨, 시세 캐시 등)도 지울까요?%n%n아니요를 누르면 다시 설치했을 때 그대로 이어서 쓸 수 있습니다.
en.RemoveSettings=Also remove BlueAsh settings and saved data (map pins, hideout levels, price cache...)?%n%nChoose No to keep them for a later reinstall.
ja.RemoveSettings=BlueAsh の設定と保存データ（マップのピン、隠れ家レベル、相場キャッシュなど）も削除しますか？%n%n「いいえ」を選ぶと、再インストール時にそのまま使えます。
ru.RemoveSettings=Удалить также настройки и сохранённые данные BlueAsh (метки на карте, уровни убежища, кэш цен)?%n%nНажмите «Нет», чтобы сохранить их для повторной установки.
es.RemoveSettings=¿Eliminar también la configuración y los datos guardados de BlueAsh (marcadores del mapa, niveles del refugio, caché de precios...)?%n%nElige No para conservarlos si lo reinstalas.
ptbr.RemoveSettings=Remover também as configurações e os dados salvos do BlueAsh (marcadores do mapa, níveis do esconderijo, cache de preços...)?%n%nEscolha Não para mantê-los em uma futura reinstalação.
de.RemoveSettings=Auch BlueAsh-Einstellungen und gespeicherte Daten (Kartenmarkierungen, Versteck-Stufen, Preis-Cache ...) entfernen?%n%nWähle Nein, um sie für eine spätere Neuinstallation zu behalten.
fr.RemoveSettings=Supprimer aussi les paramètres et données enregistrées de BlueAsh (repères de carte, niveaux de planque, cache des prix...) ?%n%nChoisissez Non pour les conserver en vue d'une réinstallation.
it.RemoveSettings=Rimuovere anche le impostazioni e i dati salvati di BlueAsh (indicatori mappa, livelli del rifugio, cache dei prezzi...)?%n%nScegli No per conservarli per una futura reinstallazione.
pl.RemoveSettings=Usunąć także ustawienia i zapisane dane BlueAsh (znaczniki mapy, poziomy kryjówki, pamięć podręczną cen...)?%n%nWybierz Nie, aby zachować je na wypadek ponownej instalacji.
tr.RemoveSettings=BlueAsh ayarları ve kayıtlı veriler (harita işaretleri, sığınak seviyeleri, fiyat önbelleği...) de silinsin mi?%n%nYeniden kurulumda kullanmak üzere saklamak için Hayır'ı seçin.
ko.StartWithWindows=Windows를 켤 때 BlueAsh 자동 실행
en.StartWithWindows=Start BlueAsh with Windows
ja.StartWithWindows=Windows 起動時に BlueAsh を自動実行
ru.StartWithWindows=Запускать BlueAsh вместе с Windows
es.StartWithWindows=Iniciar BlueAsh con Windows
ptbr.StartWithWindows=Iniciar o BlueAsh com o Windows
de.StartWithWindows=BlueAsh mit Windows starten
fr.StartWithWindows=Lancer BlueAsh avec Windows
it.StartWithWindows=Avvia BlueAsh con Windows
pl.StartWithWindows=Uruchamiaj BlueAsh razem z Windows
tr.StartWithWindows=BlueAsh'i Windows ile başlat

ko.CloseFailed=BlueAsh를 닫지 못했습니다. 관리자 권한으로 실행 중일 수 있습니다.%n%n트레이 아이콘에서 BlueAsh를 완전 종료한 뒤 다시 시도해 주세요.
en.CloseFailed=BlueAsh could not be closed. It may be running as administrator.%n%nQuit BlueAsh from its tray icon, then try again.
ja.CloseFailed=BlueAsh を終了できませんでした。管理者として実行中の可能性があります。%n%nトレイアイコンから BlueAsh を完全に終了してから、もう一度お試しください。
ru.CloseFailed=Не удалось закрыть BlueAsh. Возможно, он запущен от имени администратора.%n%nЗакройте BlueAsh через значок в трее и повторите попытку.
es.CloseFailed=No se pudo cerrar BlueAsh. Puede que se esté ejecutando como administrador.%n%nCierra BlueAsh desde su icono de la bandeja e inténtalo de nuevo.
ptbr.CloseFailed=Não foi possível fechar o BlueAsh. Ele pode estar sendo executado como administrador.%n%nFeche o BlueAsh pelo ícone da bandeja e tente novamente.
de.CloseFailed=BlueAsh konnte nicht beendet werden. Möglicherweise läuft es als Administrator.%n%nBeende BlueAsh über das Tray-Symbol und versuche es erneut.
fr.CloseFailed=Impossible de fermer BlueAsh. Il est peut-être exécuté en tant qu'administrateur.%n%nQuittez BlueAsh depuis son icône de la barre d'état, puis réessayez.
it.CloseFailed=Impossibile chiudere BlueAsh. Potrebbe essere in esecuzione come amministratore.%n%nChiudi BlueAsh dall'icona nell'area di notifica e riprova.
pl.CloseFailed=Nie udało się zamknąć BlueAsh. Może działać jako administrator.%n%nZamknij BlueAsh z ikony w zasobniku i spróbuj ponownie.
tr.CloseFailed=BlueAsh kapatılamadı. Yönetici olarak çalışıyor olabilir.%n%nBlueAsh'i tepsi simgesinden tamamen kapatıp yeniden deneyin.
ko.RemoveTask=BlueAsh가 만든 관리자 권한 예약 작업을 지우려면 관리자 확인이 필요합니다. 다음 Windows 창에서 '예'를 누르세요.
en.RemoveTask=Removing the administrator task BlueAsh created needs administrator approval. Choose Yes in the next Windows prompt.
ja.RemoveTask=BlueAsh が作成した管理者タスクを削除するには、管理者の承認が必要です。次の Windows の画面で「はい」を選んでください。
ru.RemoveTask=Чтобы удалить задачу администратора, созданную BlueAsh, нужно подтверждение администратора. Нажмите «Да» в следующем окне Windows.
es.RemoveTask=Para quitar la tarea de administrador que creó BlueAsh se necesita aprobación de administrador. Elige Sí en la siguiente ventana de Windows.
ptbr.RemoveTask=Para remover a tarefa de administrador criada pelo BlueAsh é preciso aprovação de administrador. Escolha Sim na próxima janela do Windows.
de.RemoveTask=Zum Entfernen der von BlueAsh erstellten Administratoraufgabe ist eine Administratorbestätigung nötig. Wähle im nächsten Windows-Fenster Ja.
fr.RemoveTask=La suppression de la tâche administrateur créée par BlueAsh nécessite une autorisation administrateur. Choisissez Oui dans la fenêtre Windows suivante.
it.RemoveTask=Per rimuovere l'attività di amministratore creata da BlueAsh serve l'approvazione dell'amministratore. Scegli Sì nella prossima finestra di Windows.
pl.RemoveTask=Usunięcie zadania administratora utworzonego przez BlueAsh wymaga zgody administratora. Wybierz Tak w następnym oknie Windows.
tr.RemoveTask=BlueAsh'in oluşturduğu yönetici görevini kaldırmak için yönetici onayı gerekir. Sonraki Windows penceresinde Evet'i seçin.

ko.InstallingWebView2=지도에서 퀘스트 사진을 보기 위한 Microsoft WebView2 설치 중…
en.InstallingWebView2=Installing Microsoft WebView2 (quest pictures on the map)…
ja.InstallingWebView2=マップでクエスト写真を見るための Microsoft WebView2 をインストール中…
ru.InstallingWebView2=Установка Microsoft WebView2 (фото заданий на карте)…
es.InstallingWebView2=Instalando Microsoft WebView2 (fotos de misiones en el mapa)…
ptbr.InstallingWebView2=Instalando o Microsoft WebView2 (fotos de missões no mapa)…
de.InstallingWebView2=Microsoft WebView2 wird installiert (Auftragsbilder auf der Karte)…
fr.InstallingWebView2=Installation de Microsoft WebView2 (images de quêtes sur la carte)…
it.InstallingWebView2=Installazione di Microsoft WebView2 (foto delle missioni sulla mappa)…
pl.InstallingWebView2=Instalowanie Microsoft WebView2 (zdjęcia zadań na mapie)…
tr.InstallingWebView2=Microsoft WebView2 kuruluyor (haritada görev resimleri)…

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "autostart"; Description: "{cm:StartWithWindows}"; Flags: unchecked

[Files]
Source: "{#ExeSource}"; DestDir: "{app}"; DestName: "BlueAsh.exe"; Flags: ignoreversion
; The install folder shows only BlueAsh.exe: the text files (and the uninstaller, CurStepChanged) are hidden.
Source: "{#Root}\README.txt"; DestDir: "{app}"; Attribs: hidden; Flags: ignoreversion
Source: "{#Root}\LICENSE.txt"; DestDir: "{app}"; Attribs: hidden; Flags: ignoreversion
Source: "{#Root}\NOTICE.txt"; DestDir: "{app}"; Attribs: hidden; Flags: ignoreversion
Source: "{#Root}\installer\MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedsWebView2

[Icons]
Name: "{autoprograms}\BlueAsh"; Filename: "{app}\BlueAsh.exe"
Name: "{autodesktop}\BlueAsh"; Filename: "{app}\BlueAsh.exe"; Tasks: desktopicon

[Registry]
; First install: BlueAsh starts in the language picked for setup (an existing choice is kept).
Root: HKCU; Subkey: "Software\ModeSwitch"; ValueType: string; ValueName: "Language"; ValueData: "{code:UiLanguage}"; Flags: createvalueifdoesntexist
; Same value name BlueAsh itself uses for 설정 > Windows 시작 시 자동 실행.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "ModeSwitch"; ValueData: """{app}\BlueAsh.exe"" --tray"; Tasks: autostart

[Run]
; Only without the WebView2 Runtime (Windows 11 and most Windows 10 have it): Microsoft's bootstrapper, per-user when not administrator.
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "{cm:InstallingWebView2}"; Check: NeedsWebView2; Flags: waituntilterminated
Filename: "{app}\BlueAsh.exe"; Description: "{cm:LaunchProgram,BlueAsh}"; Flags: nowait postinstall skipifsilent

[Code]
// WebView2 Runtime installed (machine-wide or for this user): its EdgeUpdate client has a version.
function HasWebView2(Root: Integer; Key: String): Boolean;
var v: String;
begin
  Result := RegQueryStringValue(Root, Key, 'pv', v) and (v <> '') and (v <> '0.0.0.0');
end;
function NeedsWebView2(): Boolean;
begin
  Result := not (HasWebView2(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    HasWebView2(HKLM, 'SOFTWARE\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}') or
    HasWebView2(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'));
end;
// BlueAsh language code for the setup language (Inno names cannot contain "-").
function UiLanguage(Param: String): String;
begin
  if ActiveLanguage = 'ptbr' then Result := 'pt-BR' else Result := ActiveLanguage;
end;

const WM_COMMAND = $0111;
function FindWindowW(ClassName: string; WindowName: Integer): HWND; external 'FindWindowW@user32.dll stdcall';
// BlueAsh's own "완전 종료" (command 31): puts the display back to normal mode, then exits (its helper processes are in a
// job object and end with it). Waits up to 10 s. False when it is still running: a BlueAsh running as administrator does
// not accept messages from this (non-administrator) setup, and its files would stay locked.
function CloseBlueAsh(): Boolean;
var h: HWND; i: Integer;
begin
  Result := True;
  h := FindWindowW('{#WindowClass}', 0);
  if h = 0 then exit;
  PostMessage(h, WM_COMMAND, 31, 0);
  for i := 1 to 50 do begin
    Sleep(200);
    if FindWindowW('{#WindowClass}', 0) = 0 then exit;
  end;
  Result := False;
end;
function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  if CloseBlueAsh() then Result := '' else Result := CustomMessage('CloseFailed');
end;
function InitializeUninstall(): Boolean;
begin
  Result := CloseBlueAsh();
  if not Result then MsgBox(CustomMessage('CloseFailed'), mbError, MB_OK);
end;
// The administrator task (\BlueAsh\Elevated) BlueAsh registers when it switches to administrator: removed with this
// install. It was created by an elevated BlueAsh, so deleting it may need administrator rights: then Windows asks once.
function TaskExists(): Boolean;
var code: Integer;
begin
  Result := Exec(ExpandConstant('{sys}\schtasks.exe'), '/Query /TN "{#TaskName}"', '', SW_HIDE, ewWaitUntilTerminated, code) and (code = 0);
end;
function RemoveElevatedTask(): Boolean;
var code: Integer;
begin
  Result := not TaskExists();
  if Result then exit;
  Exec(ExpandConstant('{sys}\schtasks.exe'), '/Delete /TN "{#TaskName}" /F', '', SW_HIDE, ewWaitUntilTerminated, code);
  if (not TaskExists()) then begin Result := True; exit; end;
  if UninstallSilent then exit;
  MsgBox(CustomMessage('RemoveTask'), mbInformation, MB_OK);
  ShellExec('runas', ExpandConstant('{sys}\schtasks.exe'), '/Delete /TN "{#TaskName}" /F', '', SW_HIDE, ewWaitUntilTerminated, code);
  Result := not TaskExists();
end;
// The uninstaller (unins000.exe/.dat) is hidden too, so the install folder shows only BlueAsh.exe; Windows Settings and
// the Control Panel still list BlueAsh and remove it.
procedure CurStepChanged(CurStep: TSetupStep);
var code: Integer;
begin
  if CurStep = ssDone then
    Exec(ExpandConstant('{sys}\attrib.exe'), '+h "' + ExpandConstant('{app}') + '\unins000.*"', '', SW_HIDE, ewWaitUntilTerminated, code);
end;
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var exe, v: String;
begin
  if CurUninstallStep = usPostUninstall then begin
    // Only what points at this install: a BlueAsh kept elsewhere keeps its autostart and administrator task.
    exe := Lowercase(ExpandConstant('{app}\BlueAsh.exe'));
    if RegQueryStringValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'ModeSwitch', v) and (Pos(exe, Lowercase(v)) > 0) then
      RegDeleteValue(HKCU, 'Software\Microsoft\Windows\CurrentVersion\Run', 'ModeSwitch');
    if RegQueryStringValue(HKCU, 'Software\ModeSwitch', 'ElevatedTaskPath', v) and (Lowercase(v) = exe) then
      if RemoveElevatedTask() then RegDeleteValue(HKCU, 'Software\ModeSwitch', 'ElevatedTaskPath');
    // Program copies, not user data: the files BlueAsh unpacks from its exe and the backup of the exe an update replaced.
    DelTree(ExpandConstant('{#DataDir}\runtime'), True, True, True);
    DelTree(ExpandConstant('{#DataDir}\update-backups'), True, True, True);
    if (not UninstallSilent) and (MsgBox(CustomMessage('RemoveSettings'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES) then begin
      RegDeleteKeyIncludingSubkeys(HKCU, 'Software\ModeSwitch');
      DelTree(ExpandConstant('{#DataDir}'), True, True, True);
    end;
  end;
end;
