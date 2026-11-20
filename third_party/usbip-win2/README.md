# usbip-win2, as the setup carries it

The setup (`installer/ds5-usbip.iss`) carries usbip-win2 for the USB/IP host
controller DS5-USBIP attaches a TV's controller to. Every file here is
someone else's, unchanged; this says where each came from, so it can be
checked and replaced.

## Which version, and why

**0.9.8.1**, the same installer Vibepollo pins (`cmake/packaging/windows_usbip_contract.cmake`
in Nonary/Vibepollo, unchanged from 2.0.4 to 2.18.9 and master as read on
2026-11-19), and usbip-win2's newest release. rhoquinn8217, 2026-11-19:
*"instead of the newest USB/IP lets use the same one vibepollo is using"*.
Vibepollo installs the same driver for its own use, and two builds of it on
one PC broke every attach (a PC where Vibepollo had installed it first).

## The files

| File | From | SHA-256 |
|---|---|---|
| `USBip-0.9.8.1-x64.exe` | usbip-win2's release v.0.9.8.1, https://github.com/vadimgrn/usbip-win2/releases/tag/v.0.9.8.1, signed (valid on 2026-11-19); the setup runs it, unchanged, where no usbip-win2 driver is installed | `38cad6d4432b52d5bb9409d9ad03b72fdffc4ada4cd3a48fbeca1a2752a8518a` |
| `LICENSE.txt` | usbip-win2's `LICENSE.txt` at v.0.9.8.1 (BSD 2-Clause), the same text as 0.9.7.7's | |
| `tools/usbip.exe`, `tools/libusbip.dll`, `tools/resources.dll` | taken from that installer: installed on a clean Windows 11 24H2 VM (CETUS, 2026-11-19) and copied from `C:\Program Files\USBip`. usbip.exe and libusbip.dll are 0.9.8.1. usbip.exe is signed (valid on 2026-11-19); the two DLLs are unsigned in usbip-win2's own install too. usbip.exe loads resources.dll by name, from its own folder | `817da219fa153b4b37c3bc1aa8fcc63f33e046188c57cc3dd2c60a6ef0883c56`, `f123ef5d288b817a890cbe39a3c2d212b3d8b1765c691c8703ccb76c7337986c`, `4bfc862e5891d9b4c12a8ebd43d3ba871ebf94b65f7a00c22527c905949e0fff` |
| `tools/msvcp140.dll`, `tools/msvcp140_atomic_wait.dll`, `tools/vcruntime140.dll`, `tools/vcruntime140_1.dll` | the Microsoft Visual C++ runtime 14.51.36247, from Visual Studio 2026's redistributable folder (`VC\Redist\MSVC\14.51.36231\x64\Microsoft.VC145.CRT`): every runtime DLL usbip.exe and libusbip.dll load (`dumpbin /dependents`). A clean Windows has none of them; usbip-win2's own setup installs this same version, from the `vc_redist.x64.exe` it carries | `7c26614e1d733892c2deac7e245ce115504b1d80592dd0a01b08e3e5a55f89ca`, `3d0cbfaa1bf3eecf5a3f4491d2960ee803cb994f30292c6adc4a07c498f60e2b`, `d1f4225df2cd877dbf130d5668a021dce3f94118455ff5ec952061c30afc9ce7`, `a7146c08f89fe5b04541ab507cdb59ff7b44534d4ba3c668a426c6450a03434e` |

## When the tools are installed

Only on a PC where the usbip-win2 driver is already installed without
usbip-win2's own `usbip.exe`, which is how Vibepollo installs it. The setup then
puts everything in `tools/` in `usbip-win2\` beside DS5-USBIP.exe, and
DS5-USBIP uses it after usbip-win2's own `C:\Program Files\USBip\usbip.exe`
(`src/app/cli.inl`, `resolve_usbip_exe_path`). Anywhere else, usbip-win2's own
setup brings its own.

⚠️ **The tools are 0.9.8.1's and must match the driver they talk to.** When
Vibepollo moves to another usbip-win2, move this folder with it: the installer,
then the tools from an install of it, then the runtime DLLs the new usbip.exe
and libusbip.dll load. Read that list again each time: 0.9.8.1's usbip.exe
loads `msvcp140_atomic_wait.dll`, which 0.9.7.7's did not.
