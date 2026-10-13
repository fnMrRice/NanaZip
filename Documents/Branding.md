# Nanazip-PM branding

Nanazip-PM is a fork of NanaZip. User-facing application names, translations,
version-resource product names, executables, release archives, and CI artifacts
use the `Nanazip-PM` brand. Preview builds retain the `Preview` suffix.

## Application and package identities

| Item | Stable | Preview |
| --- | --- | --- |
| Display name | Nanazip-PM | Nanazip-PM Preview |
| MSIX identity name | Nanazip-PM.Stable | Nanazip-PM.Preview |
| Shell extension CLSID | B53C7912-02D2-441C-89A1-67923B6A8A04 | 9BA7FC21-285D-47ED-9425-3644A5954774 |

The MSIX publisher/signing identity is unchanged; signing still requires a
certificate matching the manifest publisher. The package names and shell
extension CLSIDs differ from upstream NanaZip.

The packaged GUI alias is `Nanazip-PM.exe`. Its executable is
`Nanazip-PM.Modern.FileManager.exe`. The console and Windows command executables
and aliases are `Nanazip-PM.Console.exe` and `Nanazip-PM.Windows.exe`.
The classic GUI executable is `Nanazip-PM.exe`; the universal file-manager
target produces `Nanazip-PM.FileManager.exe`.

The package no longer registers upstream `NanaZip`, `K7`, or `7z` command aliases.
Settings use `HKEY_CURRENT_USER\Software\Nanazip-PM`; policy keys use
`Software\Policies\M2Team\Nanazip-PM`. Classic file-association ProgIDs use
`Nanazip-PM.`. Existing upstream preferences are not migrated automatically.
The extension installer also has its own AppId and installation directory.

## Building and upstream attribution

Source directories, Visual Studio project names, C++/WinRT namespaces, resource
identifiers, DLL/SFX filenames, and existing artwork retain their upstream names.
Copyright, license text, historical release notes, and upstream sponsorship
destinations remain attributed to NanaZip and its authors.

Release/preview switching is implemented in
`NanaZip.Build.Tasks/RefreshProjectResources.cs`. Its checked-in compiled helper
`NanaZip.Project/NanaZip.Build.Tasks.dll` must be rebuilt after changing that task:

```powershell
dotnet build --configuration Release NanaZip.Build.Tasks/NanaZip.Build.Tasks.csproj
```

Use the existing build scripts and project filenames. Build archives are named
`Nanazip-PM_<version>` or `Nanazip-PM-Preview_<version>`; the extension installer is
`Nanazip-PM.ExtensionPackage_<version>.exe`.
MSIX package filenames use `Nanazip-PM_<version>_<architecture>`.
