# DS5-USBIP

![platform](https://img.shields.io/badge/platform-Windows%2010%2F11%20x64-0078D6?logo=windows&logoColor=white)
![language](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)
![transport](https://img.shields.io/badge/transport-USB%2FIP-2ea44f)
![fork of CTM-Bridge/CTM-USBIP](https://img.shields.io/badge/fork%20of-CTM--Bridge%2FCTM--USBIP-lightgrey)

DS5-USBIP is a fork of [ciprianmisaila's
CTM-USBIP](https://github.com/CTM-Bridge/CTM-USBIP) that offers a DualSense
connected to a webOS TV a connection to a host PC over the network or the
internet, with native features: gyro, touchpad, **audio-based rumble** and
**speaker audio**.
This fork brings additional DualSense specific features, including
**microphone support on USB**, **DualSense Edge support** and
**[DS5Dongle](https://github.com/awalol/DS5Dongle)** support.

This fork also adds an optional host-side controller rebinder with
stick/gyro/touchpad-to-mouse pre-sets and an integrated virtual keyboard,
intended to let you drive the stream with only a controller, as well as switch
controller configs mid-game.

[rhoquinn8217/aurora-tv](https://github.com/rhoquinn8217/aurora-tv) on a webOS
TV is required to bridge DualSense controllers to DS5-USBIP.

Some webOS TVs do not support DualSense over Bluetooth. Use a USB port on
those, or a [DS5Dongle](https://github.com/awalol/DS5Dongle) to stay wireless.

## Start up guide

**Prerequisites**

| Requirement | Notes |
|:---|:---|
| **DualSense** | Your controller |
| **Windows** | Your host machine's operating system |
| **Moonlight-compatible host** | Streaming software on that Windows machine:<br>[Sunshine](https://github.com/LizardByte/Sunshine), [Apollo](https://github.com/ClassicOldSong/Apollo), [Vibepollo](https://github.com/Nonary/Vibepollo), [Vibeshine](https://github.com/Nonary/vibeshine), etc. |
| **[rhoquinn8217/aurora-tv](https://github.com/rhoquinn8217/aurora-tv)** | Install the ipk (pending) on your webOS TV |

**Set up DS5-USBIP**

1. Download the installer from the [releases page](https://github.com/rhoquinn8217/DS5-USBIP/releases) (pending).
2. Run the installer. It is unsigned, so you need to select **Run anyway**.<br>
   *Note: installation includes the required
   [usbip-win2](https://github.com/vadimgrn/usbip-win2/releases) driver and will
   require a restart.*
3. Start DS5-USBIP from the Start menu. It lives in the tray.

**Bridge and play**

1. Connect a DualSense to the television: via Bluetooth or USB port.
2. Start rhoquinn8217/aurora-tv on the television.
3. Turn on **Enable Device Bridging** in **Settings (⚙️) → USB Bridge**.
4. Start the stream to the host.
5. Press and hold the touchpad with two fingers for a second.
6. DS5-USBIP will open showing that the DualSense is natively connected.<br>
   *Optional (Recommended): Create and set a new "DS5-DS4-touchpad-to-mouse"
   pre-set and try it out.*

**Start using the DualSense with gyro, touchpad, audio-based rumble and
speaker audio (microphone on USB only).**

*Note: DS5-USBIP can be set up, stopped and started through the same stream.*

## Why this exists

Before ciprianmisaila's ctm-bridge-webos/CTM-USBIP, a DualSense connected to a
USB port on a webOS TV could already send its input to a PC through the
streaming software, and Windows saw it as a device with gyro, touchpad and
adaptive triggers.
However, it is limited in that it does not support audio-based rumble, speaker
audio or microphone.

USB/IP allows devices to fully connect to a host over a network or the internet,
providing the DualSense's audio features that the streaming software doesn't
have. However, a USB/IP client has to reach out over the network or internet to
the server that presents the device, but reaching out to a webOS TV is
difficult, especially over the internet.

DS5-USBIP resolves this issue by being a relay that runs on the host, waiting
for a webOS TV to make the request to connect a DualSense to it. It then uses
USB/IP to present the DualSense to the host as a native device, with gyro,
touchpad, audio-based rumble and speaker audio as if connected
directly to the PC.

Being a relay also made the controller rebinder and Controller Configs cheap to
add. Every report already passes through DS5-USBIP, so rebinding needed no new
plumbing. Having it run on the Windows host machine meant custom controller
rebindings could be stored and managed.

## What this fork adds

In [ciprianmisaila](https://github.com/ciprianmisaila)'s Bluetooth path, report
packets are tunnelled over the network and straight to the controller, bypassing
the TV's own audio pathways, so a DualSense reaches the PC with its speaker,
rumble and adaptive triggers. ciprianmisaila's Bluetooth solution works cleanly
with the exception of the microphone, which is a limitation of webOS rather than
anything in that design.

This fork's goal is to build out the USB path, expand DualSense support and
provide a controller rebinder that gives you the ability to drive the stream
with only a controller.

| Addition | &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; What it does |
|:---|:---|
| **DualSense audio over the TV's USB port** | A DualSense's speaker, audio-based rumble and microphone travel as isochronous audio on the controller's own audio interfaces, not as HID traffic. Bluetooth carries input and audio in one report stream that can be forwarded whole; USB splits the audio onto endpoints of its own. This fork rebuilds the pad as the composite device presenting those interfaces and carries the streams both ways, so the speaker, rumble and microphone behave as they would on a cable. |
| **DualSense Edge support** | The Edge's product ID was already recognised when listing devices, but DS5-USBIP rebuilt every DualSense from the same descriptor, so Windows saw a plain DualSense. The Edge differs by more than its product ID: a larger HID report descriptor, extra feature reports and different endpoint polling. This fork adds a descriptor for it, captured from the hardware's own kernel descriptor files, so Windows sees a DualSense Edge. |
| **DS5Dongle support** | A DS5Dongle presents itself over USB as a DualSense or DualSense Edge, so a wireless controller still reaches the PC with its speaker, rumble, adaptive triggers and microphone. Using a DS5Dongle is also an alternative to a wireless DualSense connected over Bluetooth; the difference is that over Bluetooth the microphone doesn't work, while through a DS5Dongle it does, since it connects through the TV's USB path. You also have access to the DS5Dongle features from various [community forks](https://github.com/awalol/DS5Dongle#community-fork). |
| **Optional: controller rebinder and virtual keyboard** | A controller rebinder with stick, gyro and touchpad to mouse pre-sets. The pre-sets can be modified and saved, and a built-in virtual keyboard is available to the controller. The rebinder and the virtual keyboard both isolate the controller's input so a game does not receive it at the same time. |

> The webOS limitation: its input driver reads Bluetooth microphone audio from the
> DualSense as controller button inputs. The captured audio produces a flood of
> random inputs that can reach the host during a stream. The driver is
> system-locked, so a permanent fix needs root-level access. The USB path does
> not have this problem: microphone audio arrives on its own isochronous
> endpoint rather than inside the report stream, so nothing reads it as button
> presses.

## Clean-room

**Load-bearing, not a formality.** All controller protocol here is derived from
this project's own observation (sysfs reads and on-wire captures), **not** from
third-party or kernel driver sources. ciprianmisaila's CTM-USBIP holds the same
line, and this fork continues it.

## Acknowledgements

- **[ciprianmisaila](https://github.com/ciprianmisaila)**: [CTM-USBIP](https://github.com/CTM-Bridge/CTM-USBIP)
  and [ctm-bridge-webos](https://github.com/CTM-Bridge/ctm-bridge-webos), which
  this is built on. The bridge itself, the map-driven translation pipeline, the
  USB/IP hosting and the DualSense audio work over Bluetooth are all
  ciprianmisaila's.
- **[usbip-win2](https://github.com/vadimgrn/usbip-win2)** by **vadimgrn**: the
  USB/IP client for Windows and its signed virtual host-controller driver.
- **[GamepadMotionHelpers](https://github.com/JibbSmart/GamepadMotionHelpers)** by
  **Jibb Smart** (MIT): the motion maths behind the gyro-to-mouse pre-sets,
  bundled in `third_party/`.
- **[DS5Dongle](https://github.com/awalol/DS5Dongle)** by **awalol**: the
  on-hardware reference the gyro report offsets and gate logic were ported from.

## License

[GNU General Public License v3.0](LICENSE) (GPL-3.0-or-later).

Copyright (C) 2026 Ciprian Teodor Misaila. Fork additions copyright (C) 2026
rhoquinn8217, under the same license.

Not a license term, an ask from ciprianmisaila's CTM-USBIP that this fork
honours: if you integrate CTM Bridge into your own app or fork, overlay the CTM
Bridge badge on your app's icon, the way the
[aurora-tv](https://github.com/CTM-Bridge/aurora-tv) and
[moonlight-tv](https://github.com/CTM-Bridge/moonlight-tv) forks do.
