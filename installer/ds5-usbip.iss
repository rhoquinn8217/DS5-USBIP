; DS5-USBIP installer (Inno Setup 6).
;
; Installs the listener into C:\Program Files\DS5-USBIP and installs the
; usbip-win2 driver with it, so the only setup left for a person is the TV app.
;
; Build:  installer\build-ds5-installer.ps1
;   or:   ISCC.exe /DAppVersion=0.1.0 installer\ds5-usbip.iss
; Output: out\installer\DS5-USBIP-Setup.exe
;
; ============================================================================
; WHAT THIS IS NOT, AND WHY
; ============================================================================
;
; IT DOES NOT INSTALL A WINDOWS SERVICE, AND THAT IS DELIBERATE. The listener
; is meant to run only while someone is streaming: started from its shortcut,
; closed from its tray icon. An always-on LocalSystem service holds the USB/IP
; host controller open when nobody is playing.
;
; IT WRITES NOTHING A PERSON OWNS INTO {app}. Program Files is not writable by
; a normal user, so configs, the log and the window state have to live in a
; per-user folder. Until the listener puts them there, an install made by this
; script cannot save them. build-ds5-installer.ps1 refuses to build until it
; can, so a setup that silently loses settings is never handed to anyone.
;
; ============================================================================
; DERIVED FROM ciprianmisaila's installer\ctm-usbip.iss
; ============================================================================
;
; His file is left where it is rather than edited, so what is his stays legible.
;
; TAKEN, because it was good and proven before this existed:
;   - the usbip-win2 bundling, its component list and its licence handling
;   - deciding once, before the run steps, whether the driver is needed, and
;     raising the restart prompt only when we were the ones who installed it
;   - the uninstall-the-previous-version path
;
; DROPPED:
;   - the Windows service: registering it, stopping it, removing it
;   - CipriansBridge.exe, its Start Menu entry, and the .NET Desktop Runtime
;     check that app needed. This installs one program.
;   - the "CTM Bridge" name throughout
;
; ============================================================================
; THE VERSION IS PASSED IN, NEVER TYPED HERE
; ============================================================================
;
; include/ctm/product.h is the one place the version is written, and the window
; title, --version and the exe's own fields all read from it. A number typed
; here would drift from the binary and then name the wrong build with complete
; confidence, which is worse than carrying no number at all. So this refuses to
; compile rather than guess.

#ifndef AppVersion
  #error Pass the version in: ISCC.exe /DAppVersion=0.1.0 -- or use build-ds5-installer.ps1
#endif

#define AppName "DS5-USBIP"
#define AppPublisher "rhoquinn8217"
#define ExeName "ctm-usbip.exe"
#define IconName "ctm-usbip.ico"
#define UsbipInstaller "USBip-0.9.7.7-x64.exe"
#define SrcRoot ".."

[Setup]
; A NEW AppId, never ciprianmisaila's. Sharing his would make this installer
; upgrade and uninstall HIS product on a machine that has it.
AppId={{5DAD18E8-3795-409F-AEEB-A0CCF8D0EF02}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
DefaultDirName={commonpf64}\{#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; admin: the bundled usbip-win2 driver cannot be installed without it.
PrivilegesRequired=admin
OutputDir={#SrcRoot}\out\installer
OutputBaseFilename={#AppName}-Setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#ExeName}
SetupIconFile={#SrcRoot}\app\{#IconName}
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SrcRoot}\out\x64\Release\{#ExeName}"; DestDir: "{app}"; Flags: ignoreversion
; The ffmpeg runtime comes from the REPO, not from the build output: the build
; output holds whatever was last compiled there, which may be stale or absent.
Source: "{#SrcRoot}\third_party\ffmpeg\x64\release\bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcRoot}\profiles\descriptors\*"; DestDir: "{app}\profiles\descriptors"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SrcRoot}\maps\*"; DestDir: "{app}\maps"; Flags: ignoreversion recursesubdirs createallsubdirs
; usbip-win2 (BSD-2-Clause, (c) Vadym Hrynchyshyn): its signed installer ships
; unmodified, and its licence is installed beside the app.
Source: "{#SrcRoot}\third_party\usbip-win2\{#UsbipInstaller}"; DestDir: "{tmp}"; Flags: deleteafterinstall
Source: "{#SrcRoot}\third_party\usbip-win2\LICENSE.txt"; DestDir: "{app}"; DestName: "usbip-win2-LICENSE.txt"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#ExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#ExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; Only the main and client components: not the pdb, sdk or gui, none of which
; this uses. /NORESTART so the restart prompt a person sees is ours.
Filename: "{tmp}\{#UsbipInstaller}"; Parameters: "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /COMPONENTS=main,client"; StatusMsg: "Installing the usbip-win2 driver..."; Flags: waituntilterminated; Check: NeedsUsbip
Filename: "{app}\{#ExeName}"; Description: "Start {#AppName}"; Flags: nowait postinstall skipifsilent

[Code]
var
  GInstallUsbip: Boolean;

function GetUninstallString(): String;
var
  Key, Value: String;
begin
  Key := 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{5DAD18E8-3795-409F-AEEB-A0CCF8D0EF02}_is1';
  Value := '';
  if not RegQueryStringValue(HKLM, Key, 'UninstallString', Value) then
    RegQueryStringValue(HKCU, Key, 'UninstallString', Value);
  Result := Value;
end;

function ListenerIsRunning(): Boolean;
var
  ResultCode: Integer;
begin
  { tasklist exits 0 whether or not it matched, so findstr is what answers:
    it exits 1 when nothing matched. }
  Result := Exec(ExpandConstant('{cmd}'),
                 '/C tasklist /FI "IMAGENAME eq {#ExeName}" | findstr /I "{#ExeName}"',
                 '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';

  { A RUNNING LISTENER IS ASKED ABOUT, NEVER KILLED.
    Windows will not overwrite a running exe, so it does have to stop. But
    ending the process strands every controller it has bridged: the pad stays
    attached to a host that is gone, and the person has to unplug and replug
    it. Quit from the tray icon releases the bridges first. So this stops and
    says so, instead of doing it for them. }
  if ListenerIsRunning() then
  begin
    Result := '{#AppName} is running. Quit it from its tray icon, then run this'
            + ' installer again.' + #13#10#13#10
            + 'Quitting from the tray releases any controller it has bridged.'
            + ' Ending the process does not.';
    Exit;
  end;

  { Decided once, before the run steps, so the answer cannot change halfway. }
  GInstallUsbip := not FileExists(ExpandConstant('{commonpf64}\USBip\usbip.exe'));
end;

function NeedsUsbip(): Boolean;
begin
  { True only when usbip-win2 was absent, so a copy someone manages themselves
    is never downgraded or clobbered. }
  Result := GInstallUsbip;
end;

function NeedRestart(): Boolean;
begin
  { usbip-win2's own setup is AlwaysRestart=yes, because it installs a driver.
    We run it /NORESTART, so the reboot prompt has to come from here instead,
    and only when we were the ones who installed it. }
  Result := GInstallUsbip;
end;
