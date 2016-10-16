/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_FONT_H__
#define ___UI_FONT_H__

#include "../ui_font.h"

u_char* ui_get_bitmap(XFontStruct* xfont, u_char* ch, size_t len, int use_ot_layout);

#define ui_get_bitmap_line(xfont, bitmap, y, bitmap_line)                             \
  ((bitmap) &&                                                                        \
   memcmp(((bitmap_line) = (bitmap) + (y) * (xfont)->glyph_width_bytes), "\x0\x0\x0", \
          (xfont)->glyph_width_bytes) != 0)

/* x & 7 == x % 8 */
#define ui_get_bitmap_cell(bitmap_line, x) ((bitmap_line)[(x) / 8] & (1 << (8 - ((x)&7) - 1)))

/*
 * !!! Available only if xfont->is_aa is true !!!
 * (xfont)->width_full == (xfont)->glyph_width_bytes / 3
 */
#define ui_get_bitmap_width(xfont) (xfont)->width_full

#endif
