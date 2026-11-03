/*
 * The few things this copy of FFmpeg's SBC encoder used to take from the rest of
 * FFmpeg, so the copied files can keep their own lines nearly untouched. They are
 * written for DS5-USBIP and under the same licence as those files: the GNU Lesser
 * General Public License, version 2.1 or later. LICENSE.txt in this folder.
 */

#ifndef SBC_COMPAT_H
#define SBC_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER)
#define DECLARE_ALIGNED(n, t, v) __declspec(align(n)) t v
#define av_always_inline __forceinline
#else
#define DECLARE_ALIGNED(n, t, v) t __attribute__((aligned(n))) v
#define av_always_inline inline __attribute__((always_inline))
#endif

#define av_cold

#define FFABS(a) ((a) >= 0 ? (a) : (-(a)))

static inline uint16_t AV_RN16(const void *p)
{
    uint16_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}

static inline int ff_clz(uint32_t x)
{
    int n = 0;
    while (n < 32 && !(x & 0x80000000u)) {
        x <<= 1;
        n++;
    }
    return n;
}

/* Writes bits into a buffer, the first bit at the top of the first byte. */
typedef struct PutBitContext {
    uint8_t *start;
    uint8_t *ptr;
    uint8_t *end;
    uint64_t pending;
    int pending_bits;
} PutBitContext;

static inline void init_put_bits(PutBitContext *s, uint8_t *buffer, int size)
{
    s->start = buffer;
    s->ptr = buffer;
    s->end = buffer + size;
    s->pending = 0;
    s->pending_bits = 0;
}

static inline void put_bits(PutBitContext *s, int n, uint32_t value)
{
    s->pending = (s->pending << n) | value;
    s->pending_bits += n;
    while (s->pending_bits >= 8) {
        s->pending_bits -= 8;
        if (s->ptr < s->end)
            *s->ptr++ = (uint8_t)(s->pending >> s->pending_bits);
    }
}

static inline void flush_put_bits(PutBitContext *s)
{
    if (s->pending_bits > 0) {
        if (s->ptr < s->end)
            *s->ptr++ = (uint8_t)(s->pending << (8 - s->pending_bits));
        s->pending_bits = 0;
    }
}

static inline int put_bytes_output(const PutBitContext *s)
{
    return (int)(s->ptr - s->start);
}

#endif /* SBC_COMPAT_H */
