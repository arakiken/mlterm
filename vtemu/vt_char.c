/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_char.h"

#include <string.h> /* memset/memcpy */
#include <pobl/bl_debug.h>
#include <pobl/bl_util.h> /* K_MIN */
#include <pobl/bl_mem.h>  /* malloc */

#define LINE_STYLE(attr) (((attr) >> 19) & 0xf)

#define IS_UNICODE_AREA_CS(attr) ((attr) & (0x1 << 17))

/* Combination of IS_ITALIC, IS_BOLD, IS_FULLWIDTH, CS_REVISION_1 and
 * CHARSET(UNICODE AREA) */
#define VTFONT(attr)                                                            \
  IS_UNICODE_AREA_CS(attr)                                                      \
      ? ((((attr) >> 5) & 0xf00) | ISO10646_UCS4_1 | (((attr) << 7) & 0xff000)) \
      : (((attr) >> 5) & 0xfff)

#define IS_BLINKING(attr) ((attr) & (0x1 << 18))
#define IS_ITALIC(attr) ((attr) & (0x1 << 16))
#define IS_BOLD(attr) ((attr) & (0x1 << 15))
#define IS_FULLWIDTH(attr) ((attr) & (0x1 << 14))
#define CHARSET(attr) \
  IS_UNICODE_AREA_CS(attr) ? (ISO10646_UCS4_1 | (((attr) >> 5) & 0x100)) : (((attr) >> 5) & 0x1ff)

#define IS_REVERSED(attr) ((attr) & (0x1 << 4))
#define REVERSE_COLOR(attr) ((attr) |= (0x1 << 4))
#define RESTORE_COLOR(attr) ((attr) &= ~(0x1 << 4))

#define IS_PROTECTED(attr) ((attr) & (0x1 << 3))

#define IS_COMB(attr) ((attr) & (0x1 << 2))

#define IS_COMB_TRAILING(attr) ((attr) & (0x1 << 1))
#define SET_COMB_TRAILING(attr) ((attr) |= (0x1 << 1))
#define UNSET_COMB_TRAILING(attr) ((attr) &= 0xfffffd)

#define IS_SINGLE_CH(attr) ((attr)&0x1)
#define USE_MULTI_CH(attr) ((attr) &= 0xfffffe)
#define UNUSE_MULTI_CH(attr) ((attr) |= 0x1)

#define COMPOUND_ATTR(charset, is_fullwidth, is_bold, is_italic, is_unicode_area_cs, \
                      line_style, is_blinking, is_protected, is_comb) \
  (((line_style) << 19) | ((is_blinking) << 18) | ((is_unicode_area_cs) << 17) | \
   ((is_italic) << 16) | ((is_bold) << 15) | ((is_fullwidth) << 14) | ((charset) << 5) | \
   ((is_protected) << 3) | ((is_comb) << 2) | 0x1)

/* --- static variables --- */

static int use_multi_col_char = 1;

static struct {
  u_int32_t min;
  u_int32_t max;

} * unicode_areas;
static u_int num_unicode_areas;
static u_int unicode_area_min;
static u_int unicode_area_max;

static int blink_visible = 1;

/* --- static functions --- */

inline static u_int get_comb_size(vt_char_t *multi_ch) {
  u_int size;

  size = 0;
  while (IS_COMB_TRAILING(multi_ch->u.ch.attr)) {
    size++;
    multi_ch++;
  }

  return size;
}

static vt_char_t *new_comb(vt_char_t *ch, u_int *comb_size_ptr) {
  vt_char_t *multi_ch;
  u_int comb_size;

  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    if (ch->u.ch.cols == 0) {
      /*
       * Zero width characters must not be combined to
       * show string like U+09b0 + U+200c + U+09cd + U+09af correctly.
       */
      return NULL;
    }

    if ((multi_ch = malloc(sizeof(vt_char_t) * 2)) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " malloc() failed.\n");
#endif

      return NULL;
    }

#ifndef __GLIBC__
    if (sizeof(multi_ch) >= 8 && ((long)(multi_ch)&0x1UL) != 0) {
      bl_msg_printf(
          "Your malloc() doesn't return 2 bits aligned address."
          "Character combining is not supported.\n");

      return NULL;
    }
#endif

    vt_char_init(multi_ch);
    vt_char_copy(multi_ch, ch);

    comb_size = 1;

    if (comb_size > 10) {
      abort();
    }
  } else {
    if (ch->u.multi_ch->u.ch.cols == 0) {
      /*
       * Zero width characters must not be combined to
       * show string like U+09b0 + U+200c + U+09cd + U+09af correctly.
       */
      return NULL;
    }

    if ((comb_size = get_comb_size(ch->u.multi_ch)) >= MAX_COMB_SIZE) {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG
                      " This char is already combined by %d chars, so no more combined.\n",
                      comb_size);
#endif

      return NULL;
    }

    if ((multi_ch = realloc(ch->u.multi_ch, sizeof(vt_char_t) * (comb_size + 2))) == NULL) {
      return NULL;
    }

#ifndef __GLIBC__
    if (sizeof(multi_ch) >= 8 && ((long)(multi_ch)&0x1UL) != 0) {
      bl_msg_printf(
          "Your malloc() doesn't return 2 bits aligned address."
          "Character combining is not supported.\n");

      return 0;
    }
#endif

    comb_size++;
  }

  SET_COMB_TRAILING((multi_ch[comb_size - 1]).u.ch.attr);

  ch->u.multi_ch = multi_ch;
  USE_MULTI_CH(ch->u.ch.attr); /* necessary for 64bit big endian */

  *comb_size_ptr = comb_size;

  return multi_ch + comb_size;
}

#ifdef USE_NORMALIZE
#include <pobl/bl_mem.h>
int vt_normalize(u_int16_t *str, int num);

static void normalize(vt_char_t *ch, u_int comb_size) {
  u_int16_t *str;

  if ((str = alloca(sizeof(*str) * (comb_size + 1)))) {
    vt_char_t *multi_ch;
    u_int count;

    multi_ch = ch->u.multi_ch;
    for (count = 0; count < comb_size + 1; count++) {
      str[count] = vt_char_code(multi_ch + count);
    }

    if (vt_normalize(str, comb_size + 1) == 1) {
      *ch = *multi_ch;
      ch->u.ch.code = str[0];
      UNSET_COMB_TRAILING(ch->u.ch.attr);
      free(multi_ch);
    }
  }
}
#endif

/* --- global functions --- */

void vt_set_use_multi_col_char(int use_it) {
  use_multi_col_char = use_it;
}

int vt_get_unicode_area(vt_font_t font, int *min, int *max) {
  u_int idx;

  if (/* FONT_CS(font) == ISO10646_UCS4_1 && */ (idx = UNICODE_AREA(font)) &&
      idx <= num_unicode_areas) {
    *min = unicode_areas[idx - 1].min;
    *max = unicode_areas[idx - 1].max;

    return 1;
  } else {
    return 0;
  }
}

vt_font_t vt_get_unicode_area_font(u_int32_t min, /* min <= max */
                                   u_int32_t max  /* min <= max */) {
  u_int idx;
  void *p;

  for (idx = num_unicode_areas; idx > 0; idx--) {
    if (min == unicode_areas[idx - 1].min && max == unicode_areas[idx - 1].max) {
      return ISO10646_UCS4_1 | (idx << 12);
    }
  }

  if (num_unicode_areas == 255 /* Max is 2^8-1 */ ||
      !(p = realloc(unicode_areas, sizeof(*unicode_areas) * (num_unicode_areas + 1)))) {
    bl_msg_printf("No more unicode areas.\n");

    return UNKNOWN_CS;
  }

  if (num_unicode_areas == 0) {
    unicode_area_min = min;
    unicode_area_max = max;
  } else {
    if (unicode_area_min > min) {
      unicode_area_min = min;
    }

    if (unicode_area_max < max) {
      unicode_area_max = max;
    }
  }

  unicode_areas = p;
  unicode_areas[num_unicode_areas].min = min;
  unicode_areas[num_unicode_areas++].max = max;

  return ISO10646_UCS4_1 | (num_unicode_areas << 12);
}

void vt_set_blink_chars_visible(int visible) {
  blink_visible = visible;
}

/*
 * character functions
 */

void vt_char_init(vt_char_t *ch) {
  if (sizeof(vt_char_t *) != sizeof(vt_char_t)) {
    /*ILP32*/
    memset(ch, 0, sizeof(vt_char_t));
    /* set u.ch.is_single_ch */
    ch->u.ch.attr = 0x1;
  } else {
    /*LP64*/
    /* LSB of multi_ch must be "is_single_ch"  */
    ch->u.multi_ch = (vt_char_t *)0x1;
  }
}

void vt_char_final(vt_char_t *ch) {
  if (!IS_SINGLE_CH(ch->u.ch.attr)) {
    free(ch->u.multi_ch);
  }
}

void vt_char_set(vt_char_t *ch, u_int32_t code, ef_charset_t cs, int is_fullwidth, int is_comb,
                 vt_color_t fg_color, vt_color_t bg_color, int is_bold, int is_italic,
                 int line_style, int is_blinking, int is_protected) {
  u_int idx;

  vt_char_final(ch);

  ch->u.ch.code = code;

  if (unicode_area_min <= code && cs == ISO10646_UCS4_1 && code <= unicode_area_max) {
    /*
     * If code == 0, unicode_area_max == 0 and unicode_area_min == 0,
     * enter this block unexpectedly, but harmless.
     */
    for (idx = num_unicode_areas; idx > 0; idx--) {
      if (unicode_areas[idx - 1].min <= code && code <= unicode_areas[idx - 1].max) {
        cs = idx;
        break;
      }
    }
  } else {
    idx = 0;
  }

#if 1
  /*
   * 0 should be returned for all zero-width characters of Unicode,
   * but 0 is returned for following characters alone for now.
   * 200C;ZERO WIDTH NON-JOINER
   * 200D;ZERO WIDTH JOINER
   * 200E;LEFT-TO-RIGHT MARK
   * 200F;RIGHT-TO-LEFT MARK
   * 202A;LEFT-TO-RIGHT EMBEDDING
   * 202B;RIGHT-TO-LEFT EMBEDDING
   * 202C;POP DIRECTIONAL FORMATTING
   * 202D;LEFT-TO-RIGHT OVERRIDE
   * 202E;RIGHT-TO-LEFT OVERRIDE
   *
   * see is_noconv_unicode() in vt_parser.c
   */
  if ((code & ~0x2f) == 0x2000 /* 0x2000-0x2000f or 0x2020-0x202f */ && cs == ISO10646_UCS4_1 &&
      ((0x200c <= code && code <= 0x200f) || (0x202a <= code && code <= 0x202e))) {
    ch->u.ch.cols = 0;
  } else {
    ch->u.ch.cols = is_fullwidth ? 2 : 1;
  }
#endif

  ch->u.ch.attr = COMPOUND_ATTR(cs, is_fullwidth != 0, is_bold != 0, is_italic != 0, idx > 0,
                                line_style, is_blinking != 0, is_protected != 0, is_comb != 0);
  ch->u.ch.fg_color = fg_color;
  ch->u.ch.bg_color = bg_color;
}

void vt_char_change_attr(vt_char_t *ch, int is_bold, /* 0: don't change, 1: set, -1: unset */
                         int is_italic,              /* 0: don't change, 1: set, -1: unset */
                         int underline_style,        /* 0: don't change, 1,2: set, -1: unset */
                         int is_blinking,            /* 0: don't change, 1: set, -1: unset */
                         int is_reversed,            /* 0: don't change, 1: set, -1: unset */
                         int is_crossed_out,         /* 0: don't change, 1: set, -1: unset */
                         int is_overlined            /* 0: don't change, 1: set, -1: unset */) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr)) {
    int line_style = LINE_STYLE(attr);

    if (is_overlined) {
      if (is_overlined > 0) {
        line_style |= LS_OVERLINE;
      } else {
        line_style &= ~LS_OVERLINE;
      }
    }

    if (is_crossed_out) {
      if (is_crossed_out > 0) {
        line_style |= LS_CROSSED_OUT;
      } else {
        line_style &= ~LS_CROSSED_OUT;
      }
    }

    if (underline_style) {
      line_style &= ~LS_UNDERLINE;
      if (underline_style > 0) {
        line_style |= underline_style;
      }
    }

    ch->u.ch.attr =
      COMPOUND_ATTR(CHARSET(attr), IS_FULLWIDTH(attr) != 0,
                    is_bold ? is_bold > 0 : IS_BOLD(attr) != 0,
                    is_italic ? is_italic > 0 : IS_ITALIC(attr) != 0,
                    IS_UNICODE_AREA_CS(attr) != 0, line_style,
                    is_blinking ? is_blinking > 0 : IS_BLINKING(attr) != 0,
                    IS_PROTECTED(attr) != 0, IS_COMB(attr) != 0) |
      (is_reversed ? (is_reversed > 0 ? IS_REVERSED(0xffffff) : IS_REVERSED(0)) :
                     IS_REVERSED(attr));
  }
}

void vt_char_reverse_attr(vt_char_t *ch, int bold, int italic, int underline_style, int blinking,
                          int reversed, int crossed_out, int overlined) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr)) {
    int line_style = LINE_STYLE(attr);

    if (overlined) {
      if (line_style & LS_OVERLINE) {
        line_style &= ~LS_OVERLINE;
      } else {
        line_style |= LS_OVERLINE;
      }
    }

    if (crossed_out) {
      if (line_style & LS_CROSSED_OUT) {
        line_style &= ~LS_CROSSED_OUT;
      } else {
        line_style |= LS_CROSSED_OUT;
      }
    }

    if (underline_style) {
      if (line_style & LS_UNDERLINE) {
        line_style &= ~LS_UNDERLINE;
      } else {
        line_style = (line_style & ~LS_UNDERLINE) |
                     (underline_style > 0 ? underline_style : LS_UNDERLINE_SINGLE);
      }
    }

    ch->u.ch.attr =
      COMPOUND_ATTR(CHARSET(attr), IS_FULLWIDTH(attr) != 0,
                    bold ? !IS_BOLD(attr) : IS_BOLD(attr) != 0,
                    italic ? !IS_ITALIC(attr) : IS_ITALIC(attr) != 0,
                    IS_UNICODE_AREA_CS(attr) != 0, line_style,
                    blinking ? !IS_BLINKING(attr) : IS_BLINKING(attr) != 0,
                    IS_PROTECTED(attr) != 0, IS_COMB(attr) != 0) |
      (reversed ? (IS_REVERSED(attr) ? IS_REVERSED(0) : IS_REVERSED(0xffffff)) :
                  IS_REVERSED(attr));
  }
}

vt_char_t *vt_char_combine(vt_char_t *ch, u_int32_t code, ef_charset_t cs, int is_fullwidth,
                           int is_comb, vt_color_t fg_color, vt_color_t bg_color, int is_bold,
                           int is_italic, int line_style, int is_blinking, int is_protected) {
  vt_char_t *comb;
  u_int comb_size;

/*
 * This check should be excluded, because characters whose is_comb flag
 * (combining property of mef) is NULL can be combined
 * if vt_is_arabic_combining(them) returns non-NULL.
 */
#if 0
  if (!is_comb) {
    return NULL;
  }
#endif

#ifdef DEBUG
  if (!IS_ISCII(vt_char_cs(ch)) && IS_ISCII(cs)) {
    bl_debug_printf(BL_DEBUG_TAG " combining iscii char to non-iscii char.\n");
  }
#endif

  if (!(comb = new_comb(ch, &comb_size))) {
    return NULL;
  }

  vt_char_init(comb);
  vt_char_set(comb, code, cs, is_fullwidth, is_comb, fg_color, bg_color, is_bold, is_italic,
              line_style, is_blinking, is_protected);

#ifdef USE_NORMALIZE
  normalize(ch, comb_size);
#endif

  return comb;
}

vt_char_t *vt_char_combine_simple(vt_char_t *ch, vt_char_t *src) {
  vt_char_t *comb;
  u_int comb_size;

/*
 * This check should be excluded, because characters whose is_comb flag
 * (combining property of mef) is NULL can be combined
 * if vt_is_arabic_combining(them) returns non-NULL.
 */
#if 0
  if (!is_comb) {
    return NULL;
  }
#endif

  if (!(comb = new_comb(ch, &comb_size))) {
    return NULL;
  }

  *comb = *src;
  UNSET_COMB_TRAILING(comb->u.ch.attr);

#ifdef USE_NORMALIZE
  normalize(ch, comb_size);
#endif

  return comb;
}

vt_char_t *vt_get_base_char(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return ch;
  } else {
    return ch->u.multi_ch;
  }
}

vt_char_t *vt_get_combining_chars(vt_char_t *ch, u_int *size) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    *size = 0;

    return NULL;
  } else {
    *size = get_comb_size(ch->u.multi_ch);

    return ch->u.multi_ch + 1;
  }
}

vt_char_t *vt_get_picture_char(vt_char_t *ch) {
  if (!IS_SINGLE_CH(ch->u.ch.attr)) {
    ch = ch->u.multi_ch;

    if (IS_COMB_TRAILING(ch->u.ch.attr) && CHARSET(ch[1].u.ch.attr) == PICTURE_CHARSET) {
      return ch + 1;
    }
  }

  return NULL;
}

/*
 * Not used for now.
 */
#if 0
int vt_char_move(vt_char_t *dst, vt_char_t *src) {
  if (dst == src) {
    return 0;
  }

  vt_char_final(dst);

  memcpy(dst, src, sizeof(vt_char_t));

#if 0
  /* invalidated src */
  vt_char_init(src);
#endif

  return 1;
}
#endif

int vt_char_copy(vt_char_t *dst, vt_char_t *src) {
  if (dst == src) {
    return 0;
  }

  vt_char_final(dst);

  memcpy(dst, src, sizeof(vt_char_t));

  if (!IS_SINGLE_CH(src->u.ch.attr)) {
    vt_char_t *multi_ch;
    u_int comb_size;

    comb_size = get_comb_size(src->u.multi_ch);

    if ((multi_ch = malloc(sizeof(vt_char_t) * (comb_size + 1))) == NULL) {
#ifdef DEBUG
      bl_warn_printf(BL_DEBUG_TAG " failed to malloc.\n");
#endif

      return 0;
    }

    memcpy(multi_ch, src->u.multi_ch, sizeof(vt_char_t) * (comb_size + 1));

    dst->u.multi_ch = multi_ch;
    USE_MULTI_CH(dst->u.ch.attr); /* necessary for 64bit big endian */
  }

  return 1;
}

u_int32_t vt_char_code(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return ch->u.ch.code;
  } else {
    return vt_char_code(ch->u.multi_ch);
  }
}

void vt_char_set_code(vt_char_t *ch, u_int32_t code) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    ch->u.ch.code = code;
  } else {
    vt_char_set_code(ch->u.multi_ch, code);
  }
}

ef_charset_t vt_char_cs(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return CHARSET(ch->u.ch.attr);
  } else {
    return vt_char_cs(ch->u.multi_ch);
  }
}

int vt_char_set_cs(vt_char_t *ch, ef_charset_t cs) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    if (IS_UNICODE_AREA_CS(ch->u.ch.attr)) {
      if (cs == ISO10646_UCS4_1_V) {
        ch->u.ch.attr |= (0x100 << 5);
      } else /* if( cs == ISO10646_UCS4_1) */
      {
        ch->u.ch.attr &= ~(0x100 << 5);
      }
    } else {
      ch->u.ch.attr &= ~(MAX_CHARSET << 5); /* ~0x1ff (vt_font.h) */
      ch->u.ch.attr |= (cs << 5);
    }
  } else {
    vt_char_set_cs(ch->u.multi_ch, cs);
  }

  return 1;
}

vt_font_t vt_char_font(vt_char_t *ch) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr)) {
    return VTFONT(attr);
  } else {
    return vt_char_font(ch->u.multi_ch);
  }
}

/*
 * Return the number of columns when ch is shown in the screen.
 * (If vt_char_cols(ch) returns 0, nothing is shown in the screen.)
 */
u_int vt_char_cols(vt_char_t *ch) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr)) {
    if (!use_multi_col_char) {
      return ch->u.ch.cols ? 1 : 0;
    } else {
      return ch->u.ch.cols;
    }
  } else {
    return vt_char_cols(ch->u.multi_ch);
  }
}

/*
 * 'use_multi_col_char' not concerned.
 */
int vt_char_is_fullwidth(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return IS_FULLWIDTH(ch->u.ch.attr);
  } else {
    return vt_char_is_fullwidth(ch->u.multi_ch);
  }
}

int vt_char_is_comb(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return IS_COMB(ch->u.ch.attr);
  } else {
    return vt_char_is_comb(ch->u.multi_ch);
  }
}

vt_color_t vt_char_fg_color(vt_char_t *ch) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr)) {
    if (IS_REVERSED(attr)) {
      return (!IS_BLINKING(attr) || blink_visible) ? ch->u.ch.bg_color : ch->u.ch.fg_color;
    } else {
      return (!IS_BLINKING(attr) || blink_visible) ? ch->u.ch.fg_color : ch->u.ch.bg_color;
    }
  } else {
    return vt_char_fg_color(ch->u.multi_ch);
  }
}

void vt_char_set_fg_color(vt_char_t *ch, vt_color_t color) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    ch->u.ch.fg_color = color;
  } else {
    u_int count;
    u_int comb_size;

    comb_size = get_comb_size(ch->u.multi_ch);
    for (count = 0; count < comb_size + 1; count++) {
#ifdef DEBUG
      if (CHARSET(ch->u.multi_ch[count].u.ch.attr) == ISO10646_UCS4_1_V) {
        bl_debug_printf(BL_DEBUG_TAG
                        " Don't change bg_color"
                        " of ISO10646_UCS4_1_V char after shaped.\n");
      }
#endif

      vt_char_set_fg_color(ch->u.multi_ch + count, color);
    }
  }
}

vt_color_t vt_char_bg_color(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return IS_REVERSED(ch->u.ch.attr) ? ch->u.ch.fg_color : ch->u.ch.bg_color;
  } else {
    return vt_char_bg_color(ch->u.multi_ch);
  }
}

void vt_char_set_bg_color(vt_char_t *ch, vt_color_t color) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    ch->u.ch.bg_color = color;
  } else {
    u_int count;
    u_int comb_size;

    comb_size = get_comb_size(ch->u.multi_ch);
    for (count = 0; count < comb_size + 1; count++) {
#ifdef DEBUG
      if (CHARSET(ch->u.multi_ch[count].u.ch.attr) == ISO10646_UCS4_1_V) {
        bl_debug_printf(BL_DEBUG_TAG
                        " Don't change bg_color"
                        " of ISO10646_UCS4_1_V char after shaped.\n");
      }
#endif

      vt_char_set_bg_color(ch->u.multi_ch + count, color);
    }
  }
}

int vt_char_get_offset(vt_char_t *ch) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr) && CHARSET(attr) == ISO10646_UCS4_1_V) {
    int8_t offset;

    offset = ch->u.ch.bg_color; /* unsigned -> signed */

    return offset;
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG
                    " vt_char_get_position() accepts "
                    "ISO10646_UCS4_1_V char alone, but received %x charset.\n",
                    vt_char_cs(ch));
#endif

    return 0;
  }
}

u_int vt_char_get_width(vt_char_t *ch) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr) && CHARSET(attr) == ISO10646_UCS4_1_V) {
    return ch->u.ch.fg_color;
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG
                    " vt_char_get_width() accepts "
                    "ISO10646_UCS4_1_V char alone, but received %x charset.\n",
                    vt_char_cs(ch));
#endif

    return 0;
  }
}

int vt_char_set_position(vt_char_t *ch, u_int8_t offset, /* signed -> unsigned */
                         u_int8_t width) {
  u_int attr;

  attr = ch->u.ch.attr;

  if (IS_SINGLE_CH(attr) && CHARSET(attr) == ISO10646_UCS4_1_V) {
    ch->u.ch.bg_color = offset;
    ch->u.ch.fg_color = width;

    return 1;
  } else {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG
                    " vt_char_set_position() accepts "
                    "ISO10646_UCS4_1_V char alone, but received %x charset.\n",
                    vt_char_cs(ch));
#endif

    return 0;
  }
}

int vt_char_line_style(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return LINE_STYLE(ch->u.ch.attr);
  } else {
    return vt_char_line_style(ch->u.multi_ch);
  }
}

int vt_char_is_blinking(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return IS_BLINKING(ch->u.ch.attr);
  } else {
    return vt_char_is_blinking(ch->u.multi_ch);
  }
}

int vt_char_is_protected(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return IS_PROTECTED(ch->u.ch.attr);
  } else {
    return vt_char_is_protected(ch->u.multi_ch);
  }
}

int vt_char_reverse_color(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    if (IS_REVERSED(ch->u.ch.attr)) {
      return 0;
    }

    REVERSE_COLOR(ch->u.ch.attr);

    return 1;
  } else {
    u_int count;
    u_int comb_size;

    comb_size = get_comb_size(ch->u.multi_ch);
    for (count = 0; count < comb_size + 1; count++) {
      vt_char_reverse_color(ch->u.multi_ch + count);
    }

    return 1;
  }
}

int vt_char_restore_color(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    if (!IS_REVERSED(ch->u.ch.attr)) {
      return 0;
    }

    RESTORE_COLOR(ch->u.ch.attr);

    return 1;
  } else {
    u_int count;
    u_int comb_size;

    comb_size = get_comb_size(ch->u.multi_ch);
    for (count = 0; count < comb_size + 1; count++) {
      vt_char_restore_color(ch->u.multi_ch + count);
    }

    return 1;
  }
}

int vt_char_is_null(vt_char_t *ch) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    return ch->u.ch.code == 0;
  } else {
    return vt_char_is_null(ch->u.multi_ch);
  }
}

/*
 * XXX
 * Returns inaccurate result in dealing with combined characters.
 * Even if they have the same code, false is returned since
 * vt_char_t:multi_ch-s never point the same address.)
 */
int vt_char_equal(vt_char_t *ch1, vt_char_t *ch2) {
  return memcmp(ch1, ch2, sizeof(vt_char_t)) == 0;
}

int vt_char_code_is(vt_char_t *ch, u_int32_t code, ef_charset_t cs) {
  if (IS_SINGLE_CH(ch->u.ch.attr)) {
    /*
     * XXX
     * gcc 4.8.2 output codes to cause unexpected result without
     * () before and after &&.
     */
    if ((CHARSET(ch->u.ch.attr) == cs) && (ch->u.ch.code == code)) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return vt_char_code_is(ch->u.multi_ch, code, cs);
  }
}

int vt_char_code_equal(vt_char_t *ch1, vt_char_t *ch2) {
  vt_char_t *comb1;
  vt_char_t *comb2;
  u_int comb1_size;
  u_int comb2_size;
  u_int count;

  if (vt_char_code(ch1) != vt_char_code(ch2)) {
    return 0;
  }

  comb1 = vt_get_combining_chars(ch1, &comb1_size);
  comb2 = vt_get_combining_chars(ch2, &comb2_size);

  if (comb1_size != comb2_size) {
    return 0;
  }

  for (count = 0; count < comb1_size; count++) {
    if (comb1[count].u.ch.code != comb2[count].u.ch.code) {
      return 0;
    }
  }

  return 1;
}

vt_char_t *vt_sp_ch(void) {
  static vt_char_t sp_ch;

  if (sp_ch.u.ch.attr == 0) {
    vt_char_init(&sp_ch);
    vt_char_set(&sp_ch, ' ', US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0);
  }

  return &sp_ch;
}

vt_char_t *vt_nl_ch(void) {
  static vt_char_t nl_ch;

  if (nl_ch.u.ch.attr == 0) {
    vt_char_init(&nl_ch);
    vt_char_set(&nl_ch, '\n', US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0);
  }

  return &nl_ch;
}

#ifdef DEBUG

#if 0
#define DUMP_HEX
#endif

/*
 * for debugging.
 */
void vt_char_dump(vt_char_t *ch) {
  u_int comb_size;
  vt_char_t *comb_chars;

#ifdef DUMP_HEX
  bl_msg_printf("[%.4x]", vt_char_code(ch));
#else
  if (vt_char_code(ch) >= 0x100) {
    if (vt_char_cs(ch) == JISX0208_1983) {
      /* only eucjp */

      bl_msg_printf("%c%c", ((vt_char_code(ch) >> 8) & 0xff) | 0x80,
                    (vt_char_code(ch) & 0xff) | 0x80);
    } else {
      bl_msg_printf("**");
    }
  } else {
    bl_msg_printf("%c", vt_char_code(ch));
  }
#endif

  comb_chars = vt_get_combining_chars(ch, &comb_size);
  for (; comb_size > 0; comb_size--) {
    vt_char_dump(comb_chars++);
  }
}

#endif
