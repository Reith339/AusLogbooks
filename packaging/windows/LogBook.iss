; Inno Setup script for the Windows installer.
; Built by packaging\build-windows.bat, which passes AppVersion and StageDir.

#define AppName "Pilot Logbook"
#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef StageDir
  #error StageDir must be defined (folder containing LogBook.exe and its DLLs)
#endif

[Setup]
; AppId identifies the app for upgrades/uninstall. Never change it once released.
AppId={{61A25479-019D-4AF5-9B90-E6632392B1A6}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Pilot Logbook
VersionInfoVersion={#AppVersion}
DefaultDirName={autopf}\Pilot Logbook
DefaultGroupName=Pilot Logbook
DisableProgramGroupPage=yes
; Installs per-user by default (no admin prompt); users can choose all-users instead.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=PilotLogbook-{#AppVersion}-Windows-Setup
SetupIconFile=..\..\resources\LogBook.ico
UninstallDisplayIcon={app}\LogBook.exe
UninstallDisplayName={#AppName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\LogBook.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\LogBook.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\LogBook.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; Note: uninstalling does NOT delete the user's logbook (kept in %APPDATA%\LogBook).
