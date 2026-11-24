/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_DISPLAY_FONT_H
#define FM1_DISPLAY_FONT_H
#include <stdint.h>

/* The generated masks use the Android monospace outline, rendered at each
 * target size. Opaque 6x8-scaled cells preserve the existing wire layout. */
#include "display_font_data.inc"

static inline uint16_t fm1_font_blend(uint16_t fg, uint16_t bg, unsigned a)
{
    if (!a) return bg;
    if (a == 15) return fg;
    const unsigned b = 15 - a;
    const unsigned r = ((fg >> 11) * a + (bg >> 11) * b + 7) / 15;
    const unsigned g = (((fg >> 5) & 63) * a + ((bg >> 5) & 63) * b + 7) / 15;
    const unsigned blue = ((fg & 31) * a + (bg & 31) * b + 7) / 15;
    return (uint16_t)(r << 11 | g << 5 | blue);
}

/* Four-bit coverage plus four-bit run length, no font engine or heap on
 * the instrument. Decode directly into the existing RGB565 glyph buffer. */
static inline void fm1_font_draw(uint16_t *dst, unsigned stride, unsigned scale,
                                 unsigned ch, uint16_t fg, uint16_t bg)
{
    if (scale < 1 || scale > 4) return;
    if (ch < 32 || ch > 126) ch = '?';
    unsigned index = (scale - 1) * 95 + ch - 32;
    const uint8_t *p = fm1_font_runs + fm1_font_offsets[index];
    const uint8_t *end = fm1_font_runs + fm1_font_offsets[index + 1];
    const unsigned w = 6 * scale, h = 8 * scale;
    unsigned x = 0, y = 0;
    while (p < end && y < h) {
        const unsigned token = *p++;
        unsigned n = (token & 15) + 1;
        const uint16_t color = fm1_font_blend(fg, bg, token >> 4);
        while (n-- && y < h) {
            dst[y * stride + x] = color;
            if (++x == w) { x = 0; ++y; }
        }
    }
}
#endif
