# attic

Kept here, not built, not shipped, and not to be run. Each entry says why it
is kept rather than deleted.

## flydigi_apex4_identity.map, flydigi_apex4_usb.profile

The Flydigi Apex 4 scaffold, kept on 2026-08-23 before its only copy was
deleted.

## device-config-panel.ps1, device-config-panel-edge.ps1 and their .bat files

The first settings panels for `ctm-device-config.txt`, Windows Forms, from
2026-08-02 and 2026-08-12, before the settings page existed. They were never
shipped, and the settings page replaced them.

Do not run them: they write the shared file badly (code review, 2026-10-05).
Every change writes every key they manage, so a volume the file left to the
device's own default is pinned at the panel's value, 100 unless moved. And
they save with a byte-order mark (`Set-Content -Encoding UTF8` in Windows
PowerShell 5.1), which the listener's reader does not strip, so the first
section header, `[ds5]`, is not seen and the keys under it are not read as
its own. Moved here from
`tools/` so that nothing presents them as a tool.
