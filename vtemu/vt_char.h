/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __VT_CHAR_H__
#define __VT_CHAR_H__

#include <pobl/bl_types.h>
#include <pobl/bl_def.h>     /* WORDS_BIGENDIAN */
#include <mef/ef_charset.h> /* ef_charset_t */

#include "vt_font.h"
#include "vt_color.h"

#define MAX_COMB_SIZE 7 /* Used in vt_shape.c,x_screen.c,vt_screen.c */
#define UTF_MAX_SIZE 6  /* see skk/dict.c */
                        /*
                         * XXX
                         * char prefixes are max 4 bytes.
                         * additional 3 bytes + cs name len ("viscii1.1-1" is max 11 bytes) =
                         * 14 bytes for iso2022 extension.
                         * char length is max 2 bytes.
                         * (total 20 bytes)
                         */
#define XCT_MAX_SIZE 20
#define MLCHAR_UTF_MAX_SIZE (UTF_MAX_SIZE * (MAX_COMB_SIZE + 1))
#define MLCHAR_XCT_MAX_SIZE (XCT_MAX_SIZE * (MAX_COMB_SIZE + 1))

/* For inline pictures (see x_picture.c) */
#define PICTURE_CHARSET 0x1ff
#define PICTURE_ID_BITS 9   /* fg or bg color */
#define PICTURE_POS_BITS 23 /* code */

enum {
  UNDERLINE_NONE,
  UNDERLINE_NORMAL,
  UNDERLINE_DOUBLE,
};

/*
 * This object size should be kept as small as possible.
 * (ILP32: 64bit) (LP64: 64bit)
 *
 * If LSB of vt_char_t.u.ch.attr is 0,
 * vt_char_t.u.ch is invalid.
 * vt_char_t.u.multi_ch -> vt_char_t [main char]
 *                      -> vt_char_t [first combining char]
 *                      -> vt_char_t [second combining char]
 *                      .....
 */
typedef struct vt_char {
  union {
    struct {
/*
 * attr member contents.
 * Total 23 bit
 * 2 bit : underline_style(0 or 1 or 2)
 * 1 bit : is_zerowidth(0 or 1)
 * 1 bit : is_protected(0 or 1)
 * 1 bit : is_blinking(0 or 1)
 * 1 bit : is unicode area cs(0 or 1)
 * 1 bit : is_italic(0 or 1)
 * 1 bit : is_bold(0 or 1)
 * 1 bit : is_fullwidth(0 or 1)
 * 9 bit : charset(0x0 - 0x1ff) or
 *         1 bit: CS_REVISION_1(ISO10646_UCS4_1_V)
 *         8 bit: unicode area id
 * 1 bit : is_reversed(0 or 1)	... used for X Selection
 * 1 bit : is_crossed_out
 * 1 bit : is_comb(0 or 1)
 * 1 bit : is_comb_trailing(0 or 1)
 * ---
 * 1 bit : is_single_ch(0 or 1)
 */
#ifdef WORDS_BIGENDIAN
      u_int code : 23;
      u_int fg_color : 9;
      u_int bg_color : 9;
      u_int attr : 23;
#else
      u_int attr : 23;
      u_int fg_color : 9;
      u_int bg_color : 9;
      u_int code : 23;
#endif
    } ch;

    /*
     * 32 bits(on ILP32) or 64 bits(on LP64).
     * LSB(used for is_single_ch) is considered 0.
     */
    struct vt_char *multi_ch;

  } u;

} vt_char_t;

int vt_set_use_multi_col_char(int use_it);

vt_font_t vt_get_unicode_area_font(u_int32_t min, u_int32_t max);

void vt_set_blink_chars_visible(int visible);

int vt_char_init(vt_char_t *ch);

int vt_char_final(vt_char_t *ch);

int vt_char_set(vt_char_t *ch, u_int32_t code, ef_charset_t cs, int is_fullwidth, int is_comb,
                vt_color_t fg_color, vt_color_t bg_color, int is_bold, int is_italic,
                int underline_style, int is_crossed_out, int is_blinking, int is_protected);

void vt_char_change_attr(vt_char_t *ch, int is_bold, int is_italic, int underline_style,
                         int is_blinking, int is_reversed, int crossed_out);

void vt_char_reverse_attr(vt_char_t *ch, int bold, int italic, int underlined,
                          int blinking, int reversed, int crossed_out);

vt_char_t *vt_char_combine(vt_char_t *ch, u_int32_t code, ef_charset_t cs, int is_fullwidth,
                           int is_comb, vt_color_t fg_color, vt_color_t bg_color, int is_bold,
                           int is_italic, int underline_style, int is_crossed_out,
                           int is_blinking, int is_protected);

/* set both fg and bg colors for reversing. */
#define vt_char_combine_picture(ch, id, pos) \
  vt_char_combine(ch, pos, PICTURE_CHARSET, 0, 0, id, id, 0, 0, 0, 0, 0, 0)

vt_char_t *vt_char_combine_simple(vt_char_t *ch, vt_char_t *comb);

vt_char_t *vt_get_base_char(vt_char_t *ch);

vt_char_t *vt_get_combining_chars(vt_char_t *ch, u_int *size);

vt_char_t *vt_get_picture_char(vt_char_t *ch);

#if 0
/*
 * Not used for now.
 */
int vt_char_move(vt_char_t *dst, vt_char_t *src);
#endif

int vt_char_copy(vt_char_t *dst, vt_char_t *src);

u_int32_t vt_char_code(vt_char_t *ch);

int vt_char_set_code(vt_char_t *ch, u_int32_t code);

ef_charset_t vt_char_cs(vt_char_t *ch);

int vt_char_set_cs(vt_char_t *ch, ef_charset_t cs);

int vt_char_is_comb(vt_char_t *ch);

vt_font_t vt_char_font(vt_char_t *ch);

u_int vt_char_cols(vt_char_t *ch);

int vt_char_is_fullwidth(vt_char_t *ch);

vt_color_t vt_char_fg_color(vt_char_t *ch);

int vt_char_set_fg_color(vt_char_t *ch, vt_color_t color);

#define vt_char_picture_id(ch) vt_char_fg_color(ch)
#define vt_char_set_picture_id(ch, idx) vt_char_set_fg_color(ch, idx)

vt_color_t vt_char_bg_color(vt_char_t *ch);

int vt_char_set_bg_color(vt_char_t *ch, vt_color_t color);

int vt_char_get_offset(vt_char_t *ch);

u_int vt_char_get_width(vt_char_t *ch);

int vt_char_set_position(vt_char_t *ch, u_int8_t offset, u_int8_t width);

int vt_char_underline_style(vt_char_t *ch);

int vt_char_is_crossed_out(vt_char_t *ch);

int vt_char_is_blinking(vt_char_t *ch);

int vt_char_is_protected(vt_char_t *ch);

int vt_char_reverse_color(vt_char_t *ch);

int vt_char_restore_color(vt_char_t *ch);

int vt_char_is_null(vt_char_t *ch);

int vt_char_equal(vt_char_t *ch1, vt_char_t *ch2);

int vt_char_code_is(vt_char_t *ch, u_int32_t code, ef_charset_t cs);

int vt_char_code_equal(vt_char_t *ch1, vt_char_t *ch2);

vt_char_t *vt_sp_ch(void);

vt_char_t *vt_nl_ch(void);

#ifdef DEBUG

void vt_char_dump(vt_char_t *ch);

#endif

#endif
