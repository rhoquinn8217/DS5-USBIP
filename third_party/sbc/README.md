# SBC encoder, from FFmpeg 8.0

Turns sound into SBC, the compressed format a DS4's speaker takes over Bluetooth.
`maps/ds4_usb_over_ds4_bt.map` asks for it and `src/map/runtime.cpp` runs it. Nothing
else in DS5-USBIP uses it, and a DualSense never does: over Bluetooth it takes Opus,
and on a cable raw sound.

## Where it came from

FFmpeg 8.0, tag `n8.0` (tag object `a4044e04486d1136022498891088a90baf5b2775`), from
`libavcodec`: `sbc.c`, `sbc.h`, `sbcenc.c`, `sbcdsp.c`, `sbcdsp.h`, `sbcdsp_data.h`.

<https://github.com/FFmpeg/FFmpeg/tree/n8.0/libavcodec>

Those files are under the GNU Lesser General Public License, version 2.1 or later:
`LICENSE.txt`. Each keeps its authors' names and its licence at the top.

## What changed, 2026-11-02

- **All six:** their includes of the rest of FFmpeg became `sbc_compat.h`.
- **`sbc.c`, `sbc.h`:** the frame's checksum is worked out bit by bit here, instead of
  through FFmpeg's table.
- **`sbcdsp.c`, `sbcdsp.h`:** only the C versions. FFmpeg's assembly versions for x86
  and ARM stayed behind.
- **`sbcenc.c`:** `sbc_encode_init`, `sbc_encode_frame` and what registered the encoder
  with FFmpeg became the four calls in `sbc_encoder.h`, and `sbc_pack_frame` writes to
  a plain buffer. The rest of the file is as it was.
- **`sbc_compat.h`, `sbc_encoder.h`:** new, written for DS5-USBIP, under the same
  licence.

## How we know it is the same encoder

`tools/sbc-golden-gen` runs FFmpeg's own encoder and this copy side by side and compares
every byte. On 2026-11-02 it compared 5,000,000 frames across five settings and ten
sounds, some of them at full scale, and none differed. `tests/sbc_encoder_test.cpp` keeps
hashes of what FFmpeg produced, so the check still runs now FFmpeg is gone.
