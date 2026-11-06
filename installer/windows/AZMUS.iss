; AZ MUS — Windows o'rnatuvchisi (Inno Setup 6)
#ifndef AppVer
  #define AppVer "0.1.0"
#endif
#define Root "..\..\build\AZMUS_artefacts\Release"

[Setup]
AppId={{D5E7E11B-D4F1-498B-8418-A058A47D65B1}
AppName=AZ MUS
AppVersion={#AppVer}
AppPublisher=Azimxodjayev Music
DefaultDirName={autopf}\Azimxodjayev Music\AZ MUS
DefaultGroupName=AZ MUS
OutputBaseFilename=AZ-MUS-{#AppVer}-Setup
SetupIconFile=..\..\Resources\icon.ico
UninstallDisplayIcon={app}\AZ MUS.exe
Compression=lzma2/ultra
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
ChangesAssociations=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Ish stolida yorliq yaratish"; GroupDescription: "Qo'shimcha:"

[Files]
Source: "{#Root}\AZ MUS.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\AZ MUS"; Filename: "{app}\AZ MUS.exe"
Name: "{autodesktop}\AZ MUS"; Filename: "{app}\AZ MUS.exe"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.azmus"; ValueType: string; ValueName: ""; ValueData: "AZMUS.Project"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\AZMUS.Project"; ValueType: string; ValueName: ""; ValueData: "AZ MUS loyihasi"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\AZMUS.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\AZ MUS.exe,0"
Root: HKA; Subkey: "Software\Classes\AZMUS.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\AZ MUS.exe"" ""%1"""

[Run]
Filename: "{app}\AZ MUS.exe"; Description: "AZ MUS ni ishga tushirish"; Flags: nowait postinstall skipifsilent
