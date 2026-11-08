; Redzone installer (Inno Setup 6.3+).
; Build:  ISCC.exe /DAppVersion=0.1.0 /DBuildDir=..\build installer\redzone.iss

#define AppName "Redzone"
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef BuildDir
  #define BuildDir "..\build"
#endif
#define ArtefactDir BuildDir + "\Redzone_artefacts\Release"

[Setup]
AppId={{6F2C8B3E-7A41-4D58-9B0C-3E5D1A9C2F47}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Redzone
DefaultDirName={autopf}\Redzone
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
; Windows 8 (6.2) and later
MinVersion=6.2
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=Redzone-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName} (VST3)

[Tasks]
Name: "standalone"; Description: "Also install the Standalone application"; Flags: unchecked

[Files]
; VST3 bundle -> C:\Program Files\Common Files\VST3\Redzone.vst3
Source: "{#ArtefactDir}\VST3\Redzone.vst3\*"; DestDir: "{commoncf64}\VST3\Redzone.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Optional standalone app
Source: "{#ArtefactDir}\Standalone\Redzone.exe"; DestDir: "{app}"; Flags: ignoreversion; Tasks: standalone

[Icons]
Name: "{autoprograms}\Redzone"; Filename: "{app}\Redzone.exe"; Tasks: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Redzone.vst3"
