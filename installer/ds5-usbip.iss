; DS5-USBIP installer (Inno Setup 6).
;
; Installs the listener into C:\Program Files\DS5-USBIP and installs the
; usbip-win2 driver with it, so the only setup left for a person is the TV app.
;
; Build:  installer\build-ds5-installer.ps1
;   or:   ISCC.exe /DAppVersion=0.1.0 /DAppBuild=12 installer\ds5-usbip.iss
; Output: out\installer\DS5-USBIP-Setup-build-12.exe, named for the build it holds
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
; per-user folder. Since 2026-10-12 the listener keeps them in
; %LOCALAPPDATA%\DS5-USBIP, and copies them there once from beside an older
; copy of the program. build-ds5-installer.ps1 still checks that before it
; builds, so a setup that silently loses settings is never handed to anyone.
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

; THE BUILD NUMBER TOO (2026-10-12): the number every build takes
; (include/ctm/build_number.h), which build-ds5-installer.ps1 reads off the exe
; this setup carries. It names the setup's file, so two setups never share a
; name, and it is the fourth number of the installed version, 0.1.0.N, which
; Windows' list of installed apps shows.
#ifndef AppBuild
  #error Pass the build number in: ISCC.exe /DAppVersion=0.1.0 /DAppBuild=12 -- or use build-ds5-installer.ps1
#endif

; HOW THIS SETUP QUITS A RUNNING DS5-USBIP (2026-10-12): it posts this message
; to the tray icon's window, found by its class, and DS5-USBIP quits exactly
; as its tray menu's Quit does, releasing any controller it has bridged. A
; copy from before that cannot be asked, and the person is asked instead.
; src/app/quit_request.inl names both too; tests/quit_request_test.cpp fails
; when the two disagree.
#define TrayClassName "CtmTrayIcon"
#define QuitMessageName "DS5-USBIP.Quit"

#define AppName "DS5-USBIP"
#define AppPublisher "rhoquinn8217"
#define ExeName "DS5-USBIP.exe"
; Its name until 2026-10-12. A copy still running under it holds the same ports.
#define OldExeName "ctm-usbip.exe"
#ifndef Configuration
  #define Configuration "Release"
#endif
#define IconName "ctm-usbip.ico"
#define UsbipInstaller "USBip-0.9.7.7-x64.exe"
#define SrcRoot ".."

[Setup]
; A NEW AppId, never ciprianmisaila's. Sharing his would make this installer
; upgrade and uninstall HIS product on a machine that has it.
AppId={{5DAD18E8-3795-409F-AEEB-A0CCF8D0EF02}
AppName={#AppName}
AppVersion={#AppVersion}.{#AppBuild}
AppVerName={#AppName} {#AppVersion} ({#AppBuild})
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}.{#AppBuild}
DefaultDirName={commonpf64}\{#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; admin: the bundled usbip-win2 driver cannot be installed without it.
PrivilegesRequired=admin
OutputDir={#SrcRoot}\out\installer
OutputBaseFilename={#AppName}-Setup-build-{#AppBuild}
Compression=lzma2
SolidCompression=yes
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\{#ExeName}
SetupIconFile={#SrcRoot}\app\{#IconName}
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SrcRoot}\out\x64\{#Configuration}\{#ExeName}"; DestDir: "{app}"; Flags: ignoreversion
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
; DS5-USBIP.exe through Windows Firewall on private networks, as Windows' own
; prompt would allow on its first run: a TV reaches it over the network, and a
; prompt missed or answered wrong breaks that without a word. Public networks
; stay closed. Deleted first, so an upgrade leaves one rule, not two.
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""{#AppName}"""; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""{#AppName}"" dir=in action=allow program=""{app}\{#ExeName}"" enable=yes profile=private,domain"; StatusMsg: "Letting {#AppName} through Windows Firewall on private networks..."; Flags: runhidden waituntilterminated
; As the person, not as the administrator who ran setup: their own folder is
; the one it must use. And not before a restart the driver still needs.
Filename: "{app}\{#ExeName}"; Description: "Start {#AppName}"; Flags: nowait postinstall skipifsilent runasoriginaluser; Check: not NeedRestart

[UninstallRun]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""{#AppName}"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveDS5USBIPFirewallRule"

[Code]
{ ======================================================================
  ALREADY INSTALLED: UPGRADE OR UNINSTALL (2026-10-12)
  ======================================================================
  rhoquinn8217: "can we have the installer have an uninstall or upgrade
  path if DS5-USBIP already installed?" Installing over an installed copy
  always worked, the same AppId upgrading it in place, but nothing said so,
  and uninstalling meant finding Windows' list of installed apps. So when a
  copy is installed, the first page says which build it is and which this
  setup holds, and offers two things: upgrade (or reinstall, or replace it
  with an older build), and uninstall, which runs the uninstaller that copy
  was installed with and closes this setup once the copy is gone.

  AND A RUNNING DS5-USBIP IS QUIT FOR THE PERSON (2026-10-12), the way its
  tray menu's Quit does: see AskListenerToQuit. A copy that does not know
  how to be asked is left to them, with a Retry.
  ====================================================================== }

const
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{5DAD18E8-3795-409F-AEEB-A0CCF8D0EF02}_is1';

var
  GInstallUsbip: Boolean;
  { The copy already installed: its version as Windows lists it, 0.1.0.N,
    and the uninstaller it was installed with. Both empty when there is
    none. }
  GInstalledVersion: String;
  GUninstaller: String;
  { The page that asks what to do with it. }
  GActionPage: TWizardPage;
  GUpgradeChoice: TNewRadioButton;
  GUninstallChoice: TNewRadioButton;
  GRunningNote: TNewStaticText;
  { Set once the copy is uninstalled, so closing this setup asks nothing. }
  GDoneAfterUninstall: Boolean;

{ One value of the copy already installed, from the entry Windows lists it
  under: the machine's, or the person's for a copy installed without admin. }
function InstalledValue(const Name: String): String;
begin
  Result := '';
  if not RegQueryStringValue(HKLM, UninstallKey, Name, Result) then
    RegQueryStringValue(HKCU, UninstallKey, Name, Result);
end;

function DotCount(const S: String): Integer;
var
  I: Integer;
begin
  Result := 0;
  for I := 1 to Length(S) do
    if S[I] = '.' then Result := Result + 1;
end;

function LastDot(const S: String): Integer;
var
  I: Integer;
begin
  Result := 0;
  for I := Length(S) downto 1 do
    if S[I] = '.' then
    begin
      Result := I;
      Exit;
    end;
end;

{ A version as Windows lists it, 0.1.0.N, the way DS5-USBIP shows it:
  0.1.0 (N). One from before builds had numbers, 0.1.0, as it is. }
function ShownVersion(const V: String): String;
var
  Dot: Integer;
begin
  Result := V;
  if DotCount(V) = 3 then
  begin
    Dot := LastDot(V);
    Result := Copy(V, 1, Dot - 1) + ' (' + Copy(V, Dot + 1, Length(V) - Dot) + ')';
  end;
end;

{ The build of a version as Windows lists it, or 0 for one from before
  builds had numbers, which is older than any that has one. }
function BuildOf(const V: String): Integer;
begin
  Result := 0;
  if DotCount(V) = 3 then
    Result := StrToIntDef(Copy(V, LastDot(V) + 1, Length(V)), 0);
end;

function ImageIsRunning(const Name: String): Boolean;
var
  ResultCode: Integer;
begin
  { tasklist exits 0 whether or not it matched, so findstr is what answers:
    it exits 1 when nothing matched. }
  Result := Exec(ExpandConstant('{cmd}'),
                 '/C tasklist /FI "IMAGENAME eq ' + Name + '" | findstr /I "' + Name + '"',
                 '', SW_HIDE, ewWaitUntilTerminated, ResultCode) and (ResultCode = 0);
end;

function ListenerIsRunning(): Boolean;
begin
  { The old name as well: a copy from before the rename holds the same ports,
    so the one this installs could not start beside it. }
  Result := ImageIsRunning('{#ExeName}') or ImageIsRunning('{#OldExeName}');
end;

{ Waits up to Ms milliseconds for every running DS5-USBIP to be gone. }
function ListenerGoneWithin(Ms: Integer): Boolean;
var
  Waited: Integer;
begin
  Waited := 0;
  Result := not ListenerIsRunning();
  while (not Result) and (Waited < Ms) do
  begin
    Sleep(500);
    Waited := Waited + 500;
    Result := not ListenerIsRunning();
  end;
end;

{ A RUNNING LISTENER IS ASKED TO QUIT, NEVER KILLED.
  Windows will not overwrite or delete a running exe, so it does have to
  stop. But ending the process strands every controller it has bridged: the
  pad stays attached to a host that is gone, and the person has to unplug
  and replug it. Quit from the tray icon releases the bridges first. So this
  asks for that same Quit, by posting the message the tray window listens
  for, and gives it ten seconds. True when nothing is left running. A copy
  from before 2026-10-12 does not know the message and stays running. }
function AskListenerToQuit(): Boolean;
var
  Wnd: HWND;
begin
  Result := not ListenerIsRunning();
  if Result then Exit;
  Wnd := FindWindowByClassName('{#TrayClassName}');
  if Wnd = 0 then Exit;
  PostMessage(Wnd, RegisterWindowMessage('{#QuitMessageName}'), 0, 0);
  Result := ListenerGoneWithin(10000);
end;

{ Quits a running DS5-USBIP, and asks the person to quit it from its tray
  only when it will not go by itself. False when they give up. }
function EnsureListenerQuit(Silent: Boolean): Boolean;
begin
  Result := AskListenerToQuit();
  while (not Result) and (not Silent) do
  begin
    if MsgBox('{#AppName} is still running, and this copy of it cannot be asked to quit by another program.' + #13#10#13#10
            + 'Quit it from its tray icon, then click Retry. Quitting from the tray releases any controller it has bridged.',
              mbError, MB_RETRYCANCEL) <> IDRETRY then
      Exit;
    Result := ListenerGoneWithin(5000);
  end;
end;

{ The first choice on the page, by how this setup's build compares with the
  installed copy's. }
function UpgradeCaption(): String;
var
  Mine, Theirs: Integer;
begin
  Mine := {#AppBuild};
  Theirs := BuildOf(GInstalledVersion);
  if Mine > Theirs then
    Result := 'Upgrade to {#AppVersion} ({#AppBuild})'
  else if Mine = Theirs then
    Result := 'Reinstall {#AppVersion} ({#AppBuild})'
  else
    Result := 'Replace it with {#AppVersion} ({#AppBuild}), an older build';
end;

procedure ChoiceChanged(Sender: TObject);
begin
  { Never before both choices exist. Ticking the first while the page was
    being built clicked it, this read the second, not made yet, and the
    setup ended with "Could not call proc" (rhoquinn8217, 2026-10-12). }
  if (GUpgradeChoice = nil) or (GUninstallChoice = nil) then Exit;
  if GUninstallChoice.Checked then
    WizardForm.NextButton.Caption := 'Uninstall'
  else
    WizardForm.NextButton.Caption := SetupMessage(msgButtonNext);
end;

function NewNote(const Caption: String; Left, Top, Width, Height: Integer): TNewStaticText;
begin
  Result := TNewStaticText.Create(GActionPage);
  Result.Parent := GActionPage.Surface;
  Result.AutoSize := False;
  Result.WordWrap := True;
  Result.Left := Left;
  Result.Top := Top;
  Result.Width := Width;
  Result.Height := Height;
  Result.Caption := Caption;
end;

function NewChoice(const Caption: String; Top, Width: Integer): TNewRadioButton;
begin
  Result := TNewRadioButton.Create(GActionPage);
  Result.Parent := GActionPage.Surface;
  Result.Top := Top;
  Result.Width := Width;
  Result.Height := ScaleY(18);
  Result.Caption := Caption;
  Result.Font.Style := [fsBold];
end;

procedure InitializeWizard();
var
  Info, UpgradeNote, UninstallNote: TNewStaticText;
  Indent, Width: Integer;
  Installed: String;
begin
  GInstalledVersion := InstalledValue('DisplayVersion');
  GUninstaller := RemoveQuotes(InstalledValue('UninstallString'));
  { Not installed: the plain install, as it always was. }
  if GUninstaller = '' then Exit;

  GActionPage := CreateCustomPage(wpWelcome, '{#AppName} is already installed', 'What should this setup do?');
  Width := GActionPage.SurfaceWidth;
  Indent := ScaleX(18);

  Installed := '{#AppName}';
  if GInstalledVersion <> '' then Installed := Installed + ' ' + ShownVersion(GInstalledVersion);
  Info := NewNote(Installed + ' is installed on this PC. This setup holds {#AppVersion} ({#AppBuild}).',
                  0, 0, Width, ScaleY(30));

  GUpgradeChoice := NewChoice(UpgradeCaption(), Info.Top + Info.Height + ScaleY(8), Width);
  GUpgradeChoice.Checked := True;
  UpgradeNote := NewNote('Your configs, settings and desktop shortcut stay as they are.',
                         Indent, GUpgradeChoice.Top + GUpgradeChoice.Height + ScaleY(2), Width - Indent, ScaleY(30));

  GUninstallChoice := NewChoice('Uninstall {#AppName}', UpgradeNote.Top + UpgradeNote.Height + ScaleY(8), Width);
  UninstallNote := NewNote('Removes the program, its shortcuts and its firewall rule. Your configs and the usbip-win2 driver stay.',
                           Indent, GUninstallChoice.Top + GUninstallChoice.Height + ScaleY(2), Width - Indent, ScaleY(30));
  { The handlers last, once both choices exist: see ChoiceChanged. }
  GUpgradeChoice.OnClick := @ChoiceChanged;
  GUninstallChoice.OnClick := @ChoiceChanged;

  GRunningNote := NewNote('{#AppName} is running. It will be quit before anything changes, the way Quit in its tray menu does: any controller it has bridged is released first.',
                          0, UninstallNote.Top + UninstallNote.Height + ScaleY(14), Width, ScaleY(44));
  GRunningNote.Visible := False;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  { One test at a time, so nothing is read off a page that was never made. }
  if GActionPage = nil then Exit;
  if CurPageID <> GActionPage.ID then Exit;
  GRunningNote.Visible := ListenerIsRunning();
  ChoiceChanged(nil);
end;

{ UNINSTALL, from the page: quit it, run the uninstaller the copy was
  installed with (it asks the person to confirm, as from Windows' list), and
  close this setup once the copy is gone. Cancelled there, the page stays. }
function NextButtonClick(CurPageID: Integer): Boolean;
var
  Code, Waited: Integer;
begin
  Result := True;
  { One test at a time, as in CurPageChanged. }
  if GActionPage = nil then Exit;
  if CurPageID <> GActionPage.ID then Exit;
  if not GUninstallChoice.Checked then Exit;

  Result := False;
  if not EnsureListenerQuit(WizardSilent()) then Exit;
  if not Exec(GUninstaller, '', '', SW_SHOWNORMAL, ewWaitUntilTerminated, Code) then
  begin
    MsgBox('The uninstaller did not start: ' + SysErrorMessage(Code), mbError, MB_OK);
    Exit;
  end;
  { Its entry goes as the uninstall ends; a moment's grace for that. }
  Waited := 0;
  while (InstalledValue('UninstallString') <> '') and (Code = 0) and (Waited < 10000) do
  begin
    Sleep(500);
    Waited := Waited + 500;
  end;
  if InstalledValue('UninstallString') <> '' then Exit;
  GDoneAfterUninstall := True;
  WizardForm.Close;
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  if GDoneAfterUninstall then Confirm := False;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';

  { Quit, not killed: see AskListenerToQuit. }
  if not EnsureListenerQuit(WizardSilent()) then
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

{ UNINSTALLING FROM WINDOWS' LIST, while DS5-USBIP runs: asked, then quit the
  same way, since Windows cannot delete a running exe. A copy that will not
  go is left to the person, and nothing is removed. }
function InitializeUninstall(): Boolean;
begin
  Result := True;
  if not ListenerIsRunning() then Exit;
  if not UninstallSilent() then
    if MsgBox('{#AppName} is running. Quit it and uninstall?' + #13#10#13#10
            + 'It quits the way Quit in its tray menu does: any controller it has bridged is released first.',
              mbConfirmation, MB_YESNO) <> IDYES then
    begin
      Result := False;
      Exit;
    end;
  if not AskListenerToQuit() then
  begin
    if not UninstallSilent() then
      MsgBox('{#AppName} did not quit. Quit it from its tray icon, then uninstall it again.', mbError, MB_OK);
    Result := False;
  end;
end;
