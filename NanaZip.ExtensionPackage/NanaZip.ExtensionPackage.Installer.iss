#define AppName "Nanazip-PM Extension Package"
#define AppPublisher "Nanazip-PM"
#define AppCopyright "© M2-Team and Contributors. All rights reserved."
#define AppURL "https://github.com/fnMrRice/NanaZip"

#ifndef AppVersion
#define AppVersion "5.1.0.0"
#endif

[Setup]
AppId={{ED246F44-F793-4F89-AF7D-4661DB11E5C8}
AppName={#AppName}
AppCopyright={#AppCopyright}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\{#AppName}
DisableDirPage=yes
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
VersionInfoVersion={#AppVersion}

PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputBaseFilename=Nanazip-PM.ExtensionPackage_{#AppVersion}
SolidCompression=yes
WizardStyle=modern

ArchitecturesAllowed=x64compatible or arm64
ArchitecturesInstallIn64BitMode=x64compatible or arm64

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
