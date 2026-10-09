; Installer for TranslucentTB. Build it with build.ps1, which prepares the folders below.
; It installs per user (no administrator rights needed), so the settings file next to the
; executable stays writable, and it can register the app to start with Windows.

#define AppName "TranslucentTB"
#define AppExe "TranslucentTB.exe"
#define RepoUrl "https://github.com/IvannPerezz/TranslucentTB-Revamped"

#ifndef AppVersion
  #define AppVersion "1.0"
#endif
#ifndef StageDir
  #define StageDir "stage\app"
#endif
#ifndef FrameworkDir
  #define FrameworkDir "stage\frameworks"
#endif

[Setup]
AppId={{4F535AE2-9062-454B-BCA8-E3653541196E}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=IvannPerezz
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
AppUpdatesURL={#RepoUrl}/releases
VersionInfoVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\{#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Windows 11, like the upstream builds that run without the Microsoft Store package
MinVersion=10.0.22000
; the app must be closed before it can be updated or removed
AppMutex=344635E9-9AE4-4E60-B128-D53E25AB70A7
SetupIconFile=..\branding\TranslucentTB.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
WizardStyle=modern
ShowLanguageDialog=no
Compression=lzma2/max
SolidCompression=yes
OutputBaseFilename={#AppName}-{#AppVersion}-Setup

[Languages]
; the same languages as the app; the wizard follows the Windows display language and falls back to English.
; Indonesian and both Chinese files come from the Inno Setup translations, newer than the ones it installs.
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "hebrew"; MessagesFile: "compiler:Languages\Hebrew.isl"
Name: "indonesian"; MessagesFile: "Languages\Indonesian.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "korean"; MessagesFile: "compiler:Languages\Korean.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "chinesesimplified"; MessagesFile: "Languages\ChineseSimplified.isl"
Name: "chinesetraditional"; MessagesFile: "Languages\ChineseTraditional.isl"

[CustomMessages]
english.StartupGroup=Startup:
german.StartupGroup=Autostart:
spanish.StartupGroup=Inicio:
hebrew.StartupGroup=הפעלה:
indonesian.StartupGroup=Mulai otomatis:
japanese.StartupGroup=スタートアップ:
korean.StartupGroup=시작 프로그램:
russian.StartupGroup=Автозапуск:
turkish.StartupGroup=Başlangıç:
ukrainian.StartupGroup=Автозапуск:
chinesesimplified.StartupGroup=启动:
chinesetraditional.StartupGroup=啟動:

english.StartupTask=Start TranslucentTB minimized to the system tray when Windows starts
german.StartupTask=TranslucentTB beim Start von Windows minimiert im Infobereich starten
spanish.StartupTask=Iniciar TranslucentTB minimizado en la bandeja del sistema al encender el equipo
hebrew.StartupTask=הפעל את TranslucentTB ממוזער במגש המערכת עם הפעלת Windows
indonesian.StartupTask=Jalankan TranslucentTB dalam keadaan diminimalkan di baki sistem saat Windows dimulai
japanese.StartupTask=Windows の起動時に TranslucentTB をシステム トレイに最小化して起動する
korean.StartupTask=Windows 시작 시 TranslucentTB를 시스템 트레이에 최소화된 상태로 실행
russian.StartupTask=Запускать TranslucentTB свёрнутым в области уведомлений при старте Windows
turkish.StartupTask=Windows başladığında TranslucentTB'yi sistem tepsisinde simge durumunda başlat
ukrainian.StartupTask=Запускати TranslucentTB згорнутим в області сповіщень під час запуску Windows
chinesesimplified.StartupTask=Windows 启动时将 TranslucentTB 最小化运行在系统托盘中
chinesetraditional.StartupTask=Windows 啟動時將 TranslucentTB 最小化執行於系統匣

english.InstallingFrameworks=Installing the Windows components TranslucentTB needs...
german.InstallingFrameworks=Die von TranslucentTB benötigten Windows-Komponenten werden installiert...
spanish.InstallingFrameworks=Instalando los componentes de Windows que necesita TranslucentTB...
hebrew.InstallingFrameworks=מתקין את רכיבי Windows ש-TranslucentTB צריך...
indonesian.InstallingFrameworks=Memasang komponen Windows yang dibutuhkan TranslucentTB...
japanese.InstallingFrameworks=TranslucentTB に必要な Windows コンポーネントをインストールしています...
korean.InstallingFrameworks=TranslucentTB에 필요한 Windows 구성 요소를 설치하는 중...
russian.InstallingFrameworks=Установка компонентов Windows, необходимых TranslucentTB...
turkish.InstallingFrameworks=TranslucentTB'nin ihtiyaç duyduğu Windows bileşenleri yükleniyor...
ukrainian.InstallingFrameworks=Встановлення компонентів Windows, потрібних TranslucentTB...
chinesesimplified.InstallingFrameworks=正在安装 TranslucentTB 所需的 Windows 组件...
chinesetraditional.InstallingFrameworks=正在安裝 TranslucentTB 所需的 Windows 元件...

[Tasks]
Name: "startup"; Description: "{cm:StartupTask}"; GroupDescription: "{cm:StartupGroup}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; the app, its assets and the Visual C++ runtime it is linked against
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; WinUI 2.8 and the UWP C++ runtime, shared Windows frameworks the settings UI is built on
Source: "{#FrameworkDir}\*.appx"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Registry]
; same value the app's own "Open at boot" switch reads and writes
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#AppName}"; ValueData: """{app}\{#AppExe}"""; Flags: uninsdeletevalue; Tasks: startup
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "{#AppName}"; Flags: deletevalue uninsdeletevalue; Tasks: not startup
; forget an earlier "disabled" choice from Task Manager, so the option picked here is the one that applies
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run"; ValueType: none; ValueName: "{#AppName}"; Flags: deletevalue uninsdeletevalue

[Run]
; the C++ runtime first, WinUI depends on it; a framework that is already installed is left as is
Filename: "powershell.exe"; Parameters: "-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command ""foreach ($name in 'Microsoft.VCLibs.x64.14.00.appx', 'Microsoft.UI.Xaml.2.8.appx') {{ try {{ Add-AppxPackage -Path (Join-Path '{tmp}' $name) -ErrorAction Stop } catch {{ } }"""; StatusMsg: "{cm:InstallingFrameworks}"; Flags: runhidden waituntilterminated
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: "{app}\settings.json"
Type: dirifempty; Name: "{app}"
