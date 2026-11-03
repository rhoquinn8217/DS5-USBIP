/*
 * Bluetooth low-complexity, subband codec (SBC)
 *
 * Copyright (C) 2017  Aurelien Jacobs <aurel@gnuage.org>
 * Copyright (C) 2012-2013  Intel Corporation
 * Copyright (C) 2008-2010  Nokia Corporation
 * Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 * Copyright (C) 2004-2005  Henryk Ploetz <henryk@ploetzli.ch>
 * Copyright (C) 2005-2008  Brad Midgley <bmidgley@xmission.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Changed for DS5-USBIP, 2026-11-02: taken from FFmpeg 8.0 (tag n8.0) and cut
 * down to work without the rest of FFmpeg. README.md in this folder lists what
 * changed and why.
 */

/**
 * @file
 * SBC encoder implementation
 */

#include <stdlib.h>

#include "sbc.h"
#include "sbcdsp.h"
#include "sbc_encoder.h"

struct SbcEncoder {
    DECLARE_ALIGNED(SBC_ALIGN, struct sbc_frame, frame);
    DECLARE_ALIGNED(SBC_ALIGN, SBCDSPContext, dsp);
};

static int sbc_analyze_audio(SBCDSPContext *s, struct sbc_frame *frame)
{
    int ch, blk;
    int16_t *x;

    switch (frame->subbands) {
    case 4:
        for (ch = 0; ch < frame->channels; ch++) {
            x = &s->X[ch][s->position - 4 *
                    s->increment + frame->blocks * 4];
            for (blk = 0; blk < frame->blocks;
                        blk += s->increment) {
                s->sbc_analyze_4s(
                    s, x,
                    frame->sb_sample_f[blk][ch],
                    frame->sb_sample_f[blk + 1][ch] -
                    frame->sb_sample_f[blk][ch]);
                x -= 4 * s->increment;
            }
        }
        return frame->blocks * 4;

    case 8:
        for (ch = 0; ch < frame->channels; ch++) {
            x = &s->X[ch][s->position - 8 *
                    s->increment + frame->blocks * 8];
            for (blk = 0; blk < frame->blocks;
                        blk += s->increment) {
                s->sbc_analyze_8s(
                    s, x,
                    frame->sb_sample_f[blk][ch],
                    frame->sb_sample_f[blk + 1][ch] -
                    frame->sb_sample_f[blk][ch]);
                x -= 8 * s->increment;
            }
        }
        return frame->blocks * 8;

    default:
        return -1;
    }
}

/*
 * Packs the SBC frame from frame into the memory at data.
 * Returns the length of the packed frame.
 */
static size_t sbc_pack_frame(uint8_t *data, int size, struct sbc_frame *frame,
                             int joint, int msbc)
{
    PutBitContext pb;

    /* Will copy the header parts for CRC-8 calculation here */
    uint8_t crc_header[11] = { 0 };
    int crc_pos;

    uint32_t audio_sample;

    int ch, sb, blk;        /* channel, subband, block and bit counters */
    int bits[2][8];         /* bits distribution */
    uint32_t levels[2][8];  /* levels are derived from that */
    uint32_t sb_sample_delta[2][8];

    if (msbc) {
        data[0] = MSBC_SYNCWORD;
        data[1] = 0;
        data[2] = 0;
    } else {
        data[0] = SBC_SYNCWORD;

        data[1]  = (frame->frequency           & 0x03) << 6;
        data[1] |= (((frame->blocks >> 2) - 1) & 0x03) << 4;
        data[1] |= (frame->mode                & 0x03) << 2;
        data[1] |= (frame->allocation          & 0x01) << 1;
        data[1] |= ((frame->subbands == 8)     & 0x01) << 0;

        data[2] = frame->bitpool;

        if (frame->bitpool > frame->subbands << (4 + (frame->mode == STEREO
                                                   || frame->mode == JOINT_STEREO)))
            return -5;
    }

    /* Can't fill in crc yet */
    crc_header[0] = data[1];
    crc_header[1] = data[2];
    crc_pos = 16;

    init_put_bits(&pb, data + 4, size - 4);

    if (frame->mode == JOINT_STEREO) {
        put_bits(&pb, frame->subbands, joint);
        crc_header[crc_pos >> 3] = joint;
        crc_pos += frame->subbands;
    }

    for (ch = 0; ch < frame->channels; ch++) {
        for (sb = 0; sb < frame->subbands; sb++) {
            put_bits(&pb, 4, frame->scale_factor[ch][sb] & 0x0F);
            crc_header[crc_pos >> 3] <<= 4;
            crc_header[crc_pos >> 3] |= frame->scale_factor[ch][sb] & 0x0F;
            crc_pos += 4;
        }
    }

    /* align the last crc byte */
    if (crc_pos % 8)
        crc_header[crc_pos >> 3] <<= 8 - (crc_pos % 8);

    data[3] = ff_sbc_crc8(crc_header, crc_pos);

    ff_sbc_calculate_bits(frame, bits);

    for (ch = 0; ch < frame->channels; ch++) {
        for (sb = 0; sb < frame->subbands; sb++) {
            levels[ch][sb] = ((1 << bits[ch][sb]) - 1) <<
                (32 - (frame->scale_factor[ch][sb] +
                    SCALE_OUT_BITS + 2));
            sb_sample_delta[ch][sb] = (uint32_t) 1 <<
                (frame->scale_factor[ch][sb] +
                    SCALE_OUT_BITS + 1);
        }
    }

    for (blk = 0; blk < frame->blocks; blk++) {
        for (ch = 0; ch < frame->channels; ch++) {
            for (sb = 0; sb < frame->subbands; sb++) {

                if (bits[ch][sb] == 0)
                    continue;

                audio_sample = ((uint64_t) levels[ch][sb] *
                    (sb_sample_delta[ch][sb] +
                    frame->sb_sample_f[blk][ch][sb])) >> 32;

                put_bits(&pb, bits[ch][sb], audio_sample);
            }
        }
    }

    flush_put_bits(&pb);

    return put_bytes_output(&pb);
}

SbcEncoder *sbc_encoder_create(uint8_t frequency, uint8_t blocks, uint8_t mode,
                               uint8_t allocation, uint8_t subbands, uint8_t bitpool)
{
    SbcEncoder *sbc = calloc(1, sizeof(*sbc));
    struct sbc_frame *frame;

    if (!sbc)
        return NULL;

    frame = &sbc->frame;
    frame->frequency = frequency;
    frame->blocks = blocks;
    frame->mode = mode;
    frame->channels = 2;
    frame->allocation = allocation;
    frame->subbands = subbands;
    frame->bitpool = bitpool;
    frame->codesize = frame->subbands * frame->blocks * frame->channels * 2;

    memset(&sbc->dsp.X, 0, sizeof(sbc->dsp.X));
    sbc->dsp.position = (SBC_X_BUFFER_SIZE - frame->subbands * 9) & ~7;
    sbc->dsp.increment = 4;
    ff_sbcdsp_init(&sbc->dsp);

    return sbc;
}

void sbc_encoder_destroy(SbcEncoder *sbc)
{
    free(sbc);
}

size_t sbc_encoder_frame_length(const SbcEncoder *sbc)
{
    const struct sbc_frame *frame = &sbc->frame;
    uint8_t joint = frame->mode == SBC_MODE_JOINT_STEREO;
    uint8_t dual  = frame->mode == SBC_MODE_DUAL_CHANNEL;

    return 4 + (4 * frame->subbands * frame->channels) / 8
             + ((frame->blocks * frame->bitpool * (1 + dual)
             + joint * frame->subbands) + 7) / 8;
}

int sbc_encoder_encode(SbcEncoder *sbc, const int16_t *pcm, uint8_t *out, size_t out_size)
{
    struct sbc_frame *frame = &sbc->frame;
    size_t frame_length = sbc_encoder_frame_length(sbc);
    int j = 0;

    if (out_size < frame_length)
        return -1;

    memset(out, 0, frame_length);

    /* Select the needed input data processing function and call it */
    if (frame->subbands == 8)
        sbc->dsp.position = sbc->dsp.sbc_enc_process_input_8s(
                sbc->dsp.position, (const uint8_t *)pcm, sbc->dsp.X,
                frame->subbands * frame->blocks, frame->channels);
    else
        sbc->dsp.position = sbc->dsp.sbc_enc_process_input_4s(
                sbc->dsp.position, (const uint8_t *)pcm, sbc->dsp.X,
                frame->subbands * frame->blocks, frame->channels);

    sbc_analyze_audio(&sbc->dsp, &sbc->frame);

    if (frame->mode == JOINT_STEREO)
        j = sbc->dsp.sbc_calc_scalefactors_j(frame->sb_sample_f,
                                             frame->scale_factor,
                                             frame->blocks,
                                             frame->subbands);
    else
        sbc->dsp.sbc_calc_scalefactors(frame->sb_sample_f,
                                       frame->scale_factor,
                                       frame->blocks,
                                       frame->channels,
                                       frame->subbands);

    if (sbc_pack_frame(out, (int)frame_length, frame, j, 0) > frame_length)
        return -1;

    return (int)frame_length;
}
