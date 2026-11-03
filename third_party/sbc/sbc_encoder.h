/*
 * The SBC encoder's four calls, in place of what plugged it into the rest of
 * FFmpeg. Written for DS5-USBIP, under the same licence as the copied files: the
 * GNU Lesser General Public License, version 2.1 or later. LICENSE.txt in this
 * folder.
 */

#ifndef SBC_ENCODER_H
#define SBC_ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sbc.h"

typedef struct SbcEncoder SbcEncoder;

/*
 * Two channels, always. frequency is one of SBC_FREQ_*, mode one of SBC_MODE_*,
 * allocation one of SBC_AM_*; blocks is 4, 8, 12 or 16 and subbands 4 or 8, and
 * the caller has checked them. NULL when out of memory.
 */
SbcEncoder *sbc_encoder_create(uint8_t frequency, uint8_t blocks, uint8_t mode,
                               uint8_t allocation, uint8_t subbands, uint8_t bitpool);

void sbc_encoder_destroy(SbcEncoder *sbc);

/* How many bytes one frame takes with these settings. */
size_t sbc_encoder_frame_length(const SbcEncoder *sbc);

/*
 * One frame. pcm holds subbands * blocks samples for each channel, left and right
 * in turn. Returns the bytes written to out, or -1 if out is too small. Keeps the
 * sound's history between calls, so a stream goes through one encoder in order.
 */
int sbc_encoder_encode(SbcEncoder *sbc, const int16_t *pcm, uint8_t *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* SBC_ENCODER_H */
