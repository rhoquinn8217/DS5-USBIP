# ctm-usbdisplay: not part of the listener

Nothing in this folder is compiled into `ctm-usbip.exe`. It is a separate
program, `ctm-usbdisplay.exe`, built only with `build.ps1 -WithUsbDisplay`
(`app\ctm-usbdisplay.vcxproj`).

It is an experiment: it presents a DisplayLink gen1 USB graphics device over
USB/IP, so that the stock Windows DisplayLink driver binds to it, then decodes
the pixel stream the driver sends and shows it in a preview window.
`docs\displaylink_protocol.md` describes the protocol.

It came with Ciprian Misaila's original CTM-USBIP and is kept as it was.
Neither the listener nor the TV app uses it.
