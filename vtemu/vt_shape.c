/*
 *	$Id$
 */

#include "vt_shape.h"

#include <pobl/bl_mem.h> /* alloca */
#include "vt_ctl_loader.h"
#include "vt_ot_layout.h"

/* --- static functions --- */

static int combine_replacing_code(vt_char_t *dst, vt_char_t *src, u_int new_code, int8_t offset,
                                  u_int8_t width, int was_vcol) {
  u_int code;

  dst = vt_char_combine_simple(dst, src);
  code = vt_char_code(src);

  if ((0x900 <= code && code <= 0xd7f) || (code == 0 && was_vcol)) {
    vt_char_set_cs(dst, ISO10646_UCS4_1_V);
    vt_char_set_position(dst, offset, width);
    was_vcol = 1;
  } else {
    vt_char_set_cs(dst, ISO10646_UCS4_1);
    was_vcol = 0;
  }

  vt_char_set_code(dst, new_code);

  return was_vcol;
}

static int replace_code(vt_char_t *ch, u_int new_code, int was_vcol) {
  u_int code;

  code = vt_char_code(ch);

  if ((0x900 <= code && code <= 0xd7f) || (code == 0 && was_vcol)) {
    vt_char_set_cs(ch, ISO10646_UCS4_1_V);
    was_vcol = 1;
  } else {
    vt_char_set_cs(ch, ISO10646_UCS4_1);
    was_vcol = 0;
  }

  vt_char_set_code(ch, new_code);

  return was_vcol;
}

/* --- global functions --- */

#ifndef NO_DYNAMIC_LOAD_CTL

#ifdef __APPLE__
u_int vt_shape_arabic(vt_char_t *, u_int, vt_char_t *, u_int) __attribute__((weak));
u_int16_t vt_is_arabic_combining(vt_char_t *, vt_char_t *, vt_char_t *) __attribute__((weak));
u_int vt_shape_iscii(vt_char_t *, u_int, vt_char_t *, u_int) __attribute__((weak));
#endif

u_int vt_shape_arabic(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len) {
  u_int (*func)(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len);

  if (!(func = vt_load_ctl_bidi_func(VT_SHAPE_ARABIC))) {
    return 0;
  }

  return (*func)(dst, dst_len, src, src_len);
}

u_int16_t vt_is_arabic_combining(vt_char_t *prev2, /* can be NULL */
                                 vt_char_t *prev,  /* must be ISO10646_UCS4_1 character */
                                 vt_char_t *ch     /* must be ISO10646_UCS4_1 character */
                                 ) {
  u_int16_t (*func)(vt_char_t *, vt_char_t *, vt_char_t *);

  if (!(func = vt_load_ctl_bidi_func(VT_IS_ARABIC_COMBINING))) {
    return 0;
  }

  return (*func)(prev2, prev, ch);
}

u_int vt_shape_iscii(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len) {
  u_int (*func)(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len);

  if (!(func = vt_load_ctl_iscii_func(VT_SHAPE_ISCII))) {
    return 0;
  }

  return (*func)(dst, dst_len, src, src_len);
}

#endif

#ifdef USE_OT_LAYOUT
u_int vt_shape_ot_layout(vt_char_t *dst, u_int dst_len, vt_char_t *src, u_int src_len,
                         ctl_info_t ctl_info) {
  int src_pos;
  u_int dst_filled;
  u_int32_t *ucs_buf;
  u_int ucs_filled;
  u_int32_t *shaped_buf;
  u_int shaped_filled;
  int8_t *offsets;
  u_int8_t *widths;
  vt_char_t *ch;
  vt_char_t *dst_shaped;
  u_int count;
  vt_font_t prev_font;
  vt_font_t cur_font;
  void *xfont;
  vt_char_t *comb;
  u_int comb_size;
  int src_pos_mark;
  int was_vcol;

#define DST_LEN (dst_len * (MAX_COMB_SIZE + 1))
  if ((ucs_buf = alloca(src_len * (MAX_COMB_SIZE + 1) * sizeof(*ucs_buf))) == NULL ||
      (shaped_buf = alloca(DST_LEN * sizeof(*shaped_buf))) == NULL ||
      (offsets = alloca(DST_LEN * sizeof(*offsets))) == NULL ||
      (widths = alloca(DST_LEN * sizeof(*widths))) == NULL) {
    return 0;
  }

  dst_filled = 0;
  ucs_filled = 0;
  dst_shaped = NULL;
  prev_font = UNKNOWN_CS;
  xfont = NULL;
  for (src_pos = 0; src_pos < src_len; src_pos++) {
    ch = &src[src_pos];
    cur_font = vt_char_font(ch);

    if (FONT_CS(cur_font) == US_ASCII /* && vt_char_code(ch) == ' ' */) {
      cur_font &= ~US_ASCII;
      cur_font |= ISO10646_UCS4_1;
    }

    if (prev_font != cur_font) {
      if (ucs_filled) {
        shaped_filled = vt_ot_layout_shape(xfont, shaped_buf, DST_LEN, offsets, widths, NULL,
                                           ucs_buf, ucs_filled);

        /*
         * If EOL char is a ot_layout byte which presents two ot_layouts
         * and its second ot_layout is out of screen, 'shaped_filled' is
         * greater than 'dst + dst_len - dst_shaped'.
         */
        if (shaped_filled > dst + dst_len - dst_shaped) {
          shaped_filled = dst + dst_len - dst_shaped;
        }

#ifdef __DEBUG
        {
          int i;

          for (i = 0; i < ucs_filled; i++) {
            bl_msg_printf("%.2x ", ucs_buf[i]);
          }
          bl_msg_printf("=>\n");

          for (i = 0; i < shaped_filled; i++) {
            bl_msg_printf("%.2x ", shaped_buf[i]);
          }
          bl_msg_printf("\n");
        }
#endif

        was_vcol = 0;
        for (count = 0; count < shaped_filled; count++, dst_shaped++, src_pos_mark++) {
          if (offsets[count] || widths[count]) {
            was_vcol =
                combine_replacing_code(--dst_shaped, vt_get_base_char(&src[--src_pos_mark]),
                                       shaped_buf[count], offsets[count], widths[count], was_vcol);
          } else {
            was_vcol = replace_code(dst_shaped, shaped_buf[count], was_vcol);
          }
        }

        dst_filled = dst_shaped - dst;

        ucs_filled = 0;
        dst_shaped = NULL;
      }

      if (FONT_CS(cur_font) == ISO10646_UCS4_1) {
        xfont = vt_ot_layout_get_font(ctl_info.ot_layout->term, cur_font);
      } else {
        xfont = NULL;
      }
    }

    prev_font = cur_font;

    if (xfont) {
      if (dst_shaped == NULL) {
        dst_shaped = &dst[dst_filled];
        src_pos_mark = src_pos;
      }

      if (!vt_char_is_null(ch)) {
        ucs_buf[ucs_filled++] = vt_char_code(ch);

        comb = vt_get_combining_chars(ch, &comb_size);
        for (count = 0; count < comb_size; count++) {
          ucs_buf[ucs_filled++] = vt_char_code(&comb[count]);
        }
      }

      vt_char_copy(&dst[dst_filled++], vt_get_base_char(ch));

      if (dst_filled >= dst_len) {
        break;
      }
    } else {
      vt_char_copy(&dst[dst_filled++], ch);

      if (dst_filled >= dst_len) {
        return dst_filled;
      }
    }
  }

  if (ucs_filled) {
    shaped_filled =
        vt_ot_layout_shape(xfont, shaped_buf, DST_LEN, offsets, widths, NULL, ucs_buf, ucs_filled);

    /*
     * If EOL char is a ot_layout byte which presents two ot_layouts and its
     * second
     * ot_layout is out of screen, 'shaped_filled' is greater then
     * 'dst + dst_len - dst_shaped'.
     */
    if (shaped_filled > dst + dst_len - dst_shaped) {
      shaped_filled = dst + dst_len - dst_shaped;
    }

    was_vcol = 0;
    for (count = 0; count < shaped_filled; count++, dst_shaped++, src_pos_mark++) {
      if (offsets[count] || widths[count]) {
        was_vcol =
            combine_replacing_code(--dst_shaped, vt_get_base_char(&src[--src_pos_mark]),
                                   shaped_buf[count], offsets[count], widths[count], was_vcol);
      } else {
        was_vcol = replace_code(dst_shaped, shaped_buf[count], was_vcol);
      }
    }

    dst_filled = dst_shaped - dst;
  }

  return dst_filled;
}
#endif
