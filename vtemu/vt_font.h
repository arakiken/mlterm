/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_FONT_H__
#define __VT_FONT_H__

#include <mef/ef_charset.h>

#undef MAX_CHARSET
#define MAX_CHARSET 0x1ff
#define FONT_CS(font) ((font) & MAX_CHARSET)
#define FONT_STYLES (FONT_BOLD | FONT_ITALIC)
#define FONT_STYLE_INDEX(font) ((((font) & FONT_STYLES) >> 10) - 1)
#define UNICODE_AREA(font) (((font) >> 12) & 0xff)
#define NORMAL_FONT_OF(cs) (IS_FULLWIDTH_CS(cs) ? (cs) | FONT_FULLWIDTH : (cs))
#define SIZE_ATTR_FONT(font, size_attr) ((font) | (((int)(size_attr)) << 21))
#define SIZE_ATTR_OF(font) (((font) >> 21) & 0x3)
#define NO_SIZE_ATTR(font) ((font) & ~(0x3 << 21))
#define IS_ISO10646_UCS4(cs) (((cs) & ~CS_REVISION_1(0)) == ISO10646_UCS4_1)

enum {
  DOUBLE_WIDTH = 0x1,
  DOUBLE_HEIGHT_TOP = 0x2,
  DOUBLE_HEIGHT_BOTTOM = 0x3,
};

typedef enum vt_font {
  /* 0x00 - MAX_CHARSET(0x1ff) is reserved for ef_charset_t */

  /* for unicode half or full width tag */
  FONT_FULLWIDTH = 0x200u, /* (default) half width */

  /* for font thickness */
  FONT_BOLD = 0x400u, /* (default) medium */

  /* for font slant */
  FONT_ITALIC = 0x800u, /* (default) roman */

  /*
   * Note that FONT_ROTATED is defined in fb/ui_font.c as (FONT_ITALIC << 1),
   * so be careful to add new entries after FONT_ITALIC.
   */
#if 0
  /* font width */
  FONT_SEMICONDENSED /* (default) normal */
#endif

  /*
   * 0x1000 - 0xff000 is used for Unicode range mark (see vt_get_unicode_area_font)
   * 0x200000 - 0x700000 is used for size_attr (see ui_font_manager.c)
   */

} vt_font_t;

#endif
