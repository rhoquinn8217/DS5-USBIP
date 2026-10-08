# ctm-capture: not part of the listener

Nothing in this folder is compiled into `ctm-usbip.exe`. It is a separate
program, `ctm-capture.exe`, built only with `build.ps1 -WithCapture`
(`app\ctm-capture.vcxproj`).

It captures a Windows display through DXGI Desktop Duplication, typically the
virtual monitor from `third_party\vdd`, with audio through WASAPI loopback,
and shows it in a resizable preview window. `amf_encoder.inl` and
`amf_decoder.inl` use the vendored AMD AMF headers in `third_party\AMF`.
`docs\ctm-capture-controls.md` describes its controls.

It came with Ciprian Misaila's original CTM-USBIP and is kept as it was.
Neither the listener nor the TV app uses it.
