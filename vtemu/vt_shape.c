/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_shape.h"

#include <pobl/bl_mem.h> /* alloca */
#include "vt_ctl_loader.h"
#include "vt_ot_layout.h"

#if 0
#include <pobl/bl_debug.h>
#define __DEBUG
#endif


/* --- static functions --- */

static int combine_replacing_code(vt_char_t *dst, vt_char_t *src, u_int new_code, int8_t xoffset,
                                  int8_t yoffset, u_int8_t advance, int was_vcol) {
  u_int code = vt_char_code(src);

  if (IS_VAR_WIDTH_CHAR(code) || (code == 0 && was_vcol)) {
    was_vcol = 1;
  } else {
    was_vcol = 0;
  }

  dst = vt_char_combine_simple(dst, src);

  vt_char_set_cs(dst, ISO10646_UCS4_1_V);
  vt_char_set_position(dst, xoffset, yoffset, advance);
  vt_char_set_code(dst, new_code);

  return was_vcol;
}

static int replace_code(vt_char_t *ch, u_int new_code, int was_vcol) {
  u_int code = vt_char_code(ch);

  if (IS_VAR_WIDTH_CHAR(code) || (code == 0 && was_vcol)) {
    vt_char_set_cs(ch, ISO10646_UCS4_1_V);
    was_vcol = 1;
  } else {
    vt_char_set_cs(ch, ISO10646_UCS4_1);
    was_vcol = 0;
  }

  vt_char_set_code(ch, new_code);

  return was_vcol;
}

#ifdef USE_OT_LAYOUT
static u_int get_shape_info(vt_ot_layout_state_t state, u_int32_t **glyphs,
                            int8_t **xoffsets, int8_t **yoffsets, u_int8_t **advances,
                            int *cur_pos) {
  u_int count;

  if (*cur_pos >= state->num_glyphs) {
    return 0;
  }

  *glyphs = state->glyphs + *cur_pos;
  *xoffsets = state->xoffsets + *cur_pos;
  *yoffsets = state->yoffsets + *cur_pos;
  *advances = state->advances + *cur_pos;

  for (count = 0; (*glyphs)[count]; count++);
  *cur_pos += (count + 1);

#ifdef __DEBUG
  {
    int i;

    for (i = 0; i < count; i++) {
      bl_msg_printf("%.2x(x%d y%d a%d) ",
                    (*glyphs)[i], (*xoffsets)[i], (*yoffsets)[i], (*advances)[i]);
    }
    bl_msg_printf("\n");
  }
#endif

  return count;
}
#endif

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
  u_int ucs_filled;
  int cur_pos;
  u_int32_t *shape_glyphs;
  u_int num_shape_glyphs;
  int8_t *xoffsets;
  int8_t *yoffsets;
  u_int8_t *advances;
  u_int prev_advance;
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

  dst_filled = 0;
  ucs_filled = 0;
  dst_shaped = NULL;
  prev_font = UNKNOWN_CS;
  xfont = NULL;
  cur_pos = 0;
  for (src_pos = 0; src_pos < src_len; src_pos++) {
    ch = &src[src_pos];
    cur_font = vt_char_font(ch);
    comb = vt_get_combining_chars(ch, &comb_size);

    if (FONT_CS(cur_font) == US_ASCII && (!comb || vt_char_cs(comb) != PICTURE_CHARSET)) {
      cur_font &= ~US_ASCII;
      cur_font |= ISO10646_UCS4_1;
    }

    if (prev_font != cur_font) {
      if (ucs_filled) {
        num_shape_glyphs = get_shape_info(ctl_info.ot_layout, &shape_glyphs, &xoffsets, &yoffsets,
                                          &advances, &cur_pos);

        /*
         * If EOL char is a ot_layout byte which presents two ot_layouts
         * and its second ot_layout is out of screen, 'num_shape_glyphs' is
         * greater than 'dst + dst_len - dst_shaped'.
         */
        if (num_shape_glyphs > dst + dst_len - dst_shaped) {
          num_shape_glyphs = dst + dst_len - dst_shaped;
        }

#ifdef __DEBUG
        {
          int i;

          for (i = 0; i < num_shape_glyphs; i++) {
            bl_msg_printf("%.2x ", shape_glyphs[i]);
          }
          bl_msg_printf("\n");
        }
#endif

        was_vcol = replace_code(dst_shaped++, shape_glyphs[0], 0);
        src_pos_mark++;
        prev_advance = advances[0];
        for (count = 1; count < num_shape_glyphs; count++, dst_shaped++, src_pos_mark++) {
          if (OTL_IS_COMB(count)) {
            was_vcol = combine_replacing_code(--dst_shaped, vt_get_base_char(&src[--src_pos_mark]),
                                              shape_glyphs[count],
                                              prev_advance + xoffsets[count],
                                              yoffsets[count], advances[count], was_vcol);
            if (advances[count] > 0) {
              /*
               * XXX
               * xoff adv
               *  0    0 -> base
               *  0    5 -> comb
               *  -1   2 -> comb
               *  -3   0 -> comb (Drawn at the base char (-7), but xoffset is 2 - 3 = -1)
               */
              prev_advance = advances[count];
            }
          } else {
            was_vcol = replace_code(dst_shaped, shape_glyphs[count], was_vcol);
            prev_advance = advances[count];
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
        ucs_filled += (1 + comb_size);
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
    num_shape_glyphs = get_shape_info(ctl_info.ot_layout, &shape_glyphs, &xoffsets, &yoffsets,
                                      &advances, &cur_pos);

    /*
     * If EOL char is a ot_layout byte which presents two ot_layouts and its
     * second
     * ot_layout is out of screen, 'num_shape_glyphs' is greater then
     * 'dst + dst_len - dst_shaped'.
     */
    if (num_shape_glyphs > dst + dst_len - dst_shaped) {
      num_shape_glyphs = dst + dst_len - dst_shaped;
    }

    was_vcol = replace_code(dst_shaped++, shape_glyphs[0], 0);
    src_pos_mark++;
    prev_advance = advances[0];
    for (count = 1; count < num_shape_glyphs; count++, dst_shaped++, src_pos_mark++) {
      if (OTL_IS_COMB(count)) {
        was_vcol = combine_replacing_code(--dst_shaped, vt_get_base_char(&src[--src_pos_mark]),
                                          shape_glyphs[count],
                                          prev_advance + xoffsets[count],
                                          yoffsets[count], advances[count], was_vcol);
        if (advances[count] > 0) {
          /*
           * XXX
           * xoff adv
           *  0    0 -> base
           *  0    5 -> comb
           *  -1   2 -> comb
           *  -3   0 -> comb (Drawn at the base char, but xoffset is 2 - 3 = -1)
           */
          prev_advance = advances[count];
        }
      } else {
        was_vcol = replace_code(dst_shaped, shape_glyphs[count], was_vcol);
        prev_advance = advances[count];
      }
    }

    dst_filled = dst_shaped - dst;
  }

  return dst_filled;
}
#endif
