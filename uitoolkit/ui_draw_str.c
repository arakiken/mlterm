/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_draw_str.h"

#include <pobl/bl_mem.h> /* alloca */
#include <vt_drcs.h>

#ifndef NO_IMAGE
#include "ui_picture.h"

#define INLINEPIC_ID(glyph) (((glyph) >> PICTURE_POS_BITS) & (MAX_INLINE_PICTURES - 1))
#define INLINEPIC_POS(glyph) ((glyph) & ((1 << PICTURE_POS_BITS) - 1))
#endif

#if 0
#define PERF_DEBUG
#endif

/* --- static functions --- */

static u_int calculate_char_width(ui_font_t *font, u_int code, ef_charset_t cs, vt_char_t *comb,
                                  u_int comb_size, int *draw_alone) {
  u_int width;

  width = ui_calculate_char_width(font, code, cs, draw_alone);

  if (cs == ISO10646_UCS4_1_V) {
    int w;

    for (; comb_size > 0; comb_size--, comb++) {
#ifdef DEBUG
      if (vt_char_cs(comb) != ISO10646_UCS4_1_V) {
        bl_debug_printf(BL_DEBUG_TAG
                        " %x cs is unexpectedly"
                        " combined to ISO10646_UCS4_1_V.\n");

        continue;
      }
#endif

      if ((w = vt_char_get_offset(comb) + (int)vt_char_get_width(comb)) > (int)width) {
        width = w;
      }
    }
  }

  return width;
}

#ifndef USE_CONSOLE
static void draw_line(ui_window_t *window, ui_color_t *color, int is_vertical, int line_style,
                      int x, int y, u_int width, u_int height, u_int ascent, int top_margin) {
  u_int w;
  u_int h;
  int x2;
  int y2;

  if (is_vertical) {
    w = 1;
    h = height;

    if (line_style == LS_UNDERLINE_DOUBLE) {
      x2 = x + 2;
      y2 = y;
    } else {
      w += ((ascent - top_margin) / 16);

      if (line_style == LS_CROSSED_OUT) {
        x += ((width - 1) / 2);
      } else if (line_style == LS_OVERLINE) {
        x += (width - (width >= 2 ? 2 : 1));
      }
    }
  } else {
    w = width;
    h = 1;

    if (line_style == LS_UNDERLINE_DOUBLE) {
      x2 = x;

      if (ascent + 2 >= height) {
        y2 = y + height - 1;
        y = y2 - 2;
      } else {
        y += ascent;
        y2 = y + 2;
      }
    } else {
      h += ((ascent - top_margin) / 16);

      if (line_style == LS_CROSSED_OUT) {
        y += ((height + 1) / 2);
      } else if (line_style == LS_OVERLINE) {
        /* do nothing */
      } else {
        y += ascent;
      }
    }
  }

  ui_window_fill_with(window, color, x, y, w, h);

  if (line_style == LS_UNDERLINE_DOUBLE) {
    ui_window_fill_with(window, color, x2, y2, w, h);
  }
}
#endif

#ifndef NO_IMAGE
static int draw_picture(ui_window_t *window, u_int32_t *glyphs, u_int num_glyphs, int dst_x,
                        int dst_y, u_int ch_width, u_int line_height) {
  u_int count;
  ui_inline_picture_t *cur_pic;
  u_int num_rows;
  int src_x;
  int src_y;
  u_int src_width;
  u_int src_height;
  u_int dst_width;
  int need_clear;
  int is_end;

  cur_pic = NULL;
  is_end = 0;

  for (count = 0; count < num_glyphs; count++) {
    ui_inline_picture_t *pic;
    int pos;
    int x;
    u_int w;

    if (!(pic = ui_get_inline_picture(INLINEPIC_ID(glyphs[count])))) {
      continue;
    }

    /*
     * XXX
     * pic->col_width isn't used in this function, so it can be
     * removed in the future.
     */

    if (pic != cur_pic) {
      num_rows = (pic->height + pic->line_height - 1) / pic->line_height;
    }

    pos = INLINEPIC_POS(glyphs[count]);
    x = (pos / num_rows) * ch_width;

    if (x + ch_width > pic->width) {
      w = pic->width > x ? pic->width - x : 0;
    } else {
      w = ch_width;
    }

    if (count == 0) {
      goto new_picture;
    } else if (w > 0 && pic == cur_pic && src_x + src_width == x) {
      if (!need_clear && w < ch_width) {
        ui_window_clear(window, dst_x + dst_width, dst_y, ch_width, line_height);
      }

      src_width += w;
      dst_width += ch_width;

      if (count + 1 < num_glyphs) {
        continue;
      }

      is_end = 1;
    }

    if (need_clear > 0) {
      ui_window_clear(window, dst_x, dst_y, dst_width, line_height);
    }

    if (src_width > 0 && src_height > 0
#ifndef INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
        && cur_pic->disp == window->disp
#endif
        ) {
#ifdef __DEBUG
      bl_debug_printf("Drawing picture at %d %d (pix %p mask %p x %d y %d w %d h %d)\n", dst_x,
                      dst_y, cur_pic->pixmap, cur_pic->mask, src_x, src_y, src_width, src_height);
#endif

      ui_window_copy_area(window, cur_pic->pixmap, cur_pic->mask, src_x, src_y, src_width,
                          src_height, dst_x, dst_y);
    }

    if (is_end) {
      return 1;
    }

    dst_x += dst_width;

  new_picture:
    src_y = (pos % num_rows) * line_height;
    src_x = x;
    dst_width = ch_width;
    cur_pic = pic;
    need_clear = 0;

    if (cur_pic->mask) {
      need_clear = 1;
    }

    if (src_y + line_height > pic->height) {
      need_clear = 1;
      src_height = pic->height > src_y ? pic->height - src_y : 0;
    } else {
      src_height = line_height;
    }

    if (strstr(cur_pic->file_path, "mlterm/animx") && cur_pic->next_frame >= 0) {
      /* Don't clear if cur_pic is 2nd or later GIF Animation frame. */
      need_clear = -1;
    }

    if ((src_width = w) < ch_width && !need_clear) {
      ui_window_clear(window, dst_x, dst_y, ch_width, line_height);
    }
  }

  if (need_clear > 0) {
    ui_window_clear(window, dst_x, dst_y, dst_width, line_height);
  }

#ifdef __DEBUG
  bl_debug_printf("Drawing picture at %d %d (pix %p mask %p x %d y %d w %d h %d)\n", dst_x, dst_y,
                  cur_pic->pixmap, cur_pic->mask, src_x, src_y, src_width, src_height);
#endif

  if (src_width > 0 && src_height > 0
#ifndef INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
      && cur_pic->disp == window->disp
#endif
      ) {
    ui_window_copy_area(window, cur_pic->pixmap, cur_pic->mask, src_x, src_y, src_width, src_height,
                        dst_x, dst_y);
  }

  return 1;
}
#endif

#ifndef USE_CONSOLE
static int get_drcs_bitmap(char *glyph, u_int width, int x, int y) {
  return (glyph[(y / 6) * (width + 1) + x] - '?') & (1 << (y % 6));
}

static int draw_drcs(ui_window_t *window, char **glyphs, u_int num_glyphs, int x, int y,
                     u_int ch_width, u_int line_height, ui_color_t *fg_xcolor, int size_attr) {
  int y_off = 0;

  if (size_attr >= DOUBLE_HEIGHT_TOP) {
    if (size_attr == DOUBLE_HEIGHT_BOTTOM) {
      y_off = line_height;
    }

    line_height *= 2;
  }

  for (; y_off < line_height; y_off++) {
    u_int w;
    int x_off_sum; /* x_off for all glyphs */
    int x_off;     /* x_off for each glyph */
    char *glyph;
    u_int glyph_width;
    u_int glyph_height;
    u_int smpl_width;
    u_int smpl_height;

    w = 0;

    for (x_off_sum = 0; x_off_sum < ch_width * num_glyphs; x_off_sum++) {
      int left_x;
      int top_y;
      int hit;
      int n_smpl;
      int smpl_x;
      int smpl_y;

      if ((x_off = x_off_sum % ch_width) == 0) {
        glyph = glyphs[x_off_sum / ch_width];

        glyph_width = glyph[0];
        glyph_height = glyph[1];
        glyph += 2;

        if ((smpl_width = glyph_width / ch_width + 1) >= 3) {
          smpl_width = 2;
        }

        if ((smpl_height = glyph_height / line_height + 1) >= 3) {
          smpl_height = 2;
        }
      }

      left_x = (x_off * glyph_width * 10 / ch_width + 5) / 10 - smpl_width / 2;
      top_y = (y_off * glyph_height * 10 / line_height + 5) / 10 - smpl_height / 2;
      /*
       * If top_y < 0 or top_y >= glyph_height, w is always 0
       * regardless of content of glyph.
       */
      if (top_y < 0) {
        top_y = 0;
      } else if (top_y >= glyph_height) {
        top_y = glyph_height - 1;
      }

#if 0
      bl_debug_printf(BL_DEBUG_TAG "x_off %d: center x %f -> %d\n", x_off,
                      (double)(x_off * glyph_width) / (double)ch_width, left_x + smpl_width / 2);
      bl_debug_printf(BL_DEBUG_TAG "y_off %d: center y %f -> %d\n", y_off,
                      (double)(y_off * glyph_height) / (double)line_height,
                      top_y + smpl_height / 2);
#endif

      hit = n_smpl = 0;

      for (smpl_y = 0; smpl_y < smpl_height; smpl_y++) {
        for (smpl_x = 0; smpl_x < smpl_width; smpl_x++) {
          if (0 <= left_x + smpl_x && left_x + smpl_x < glyph_width && 0 <= top_y + smpl_y &&
              top_y + smpl_y < glyph_height) {
            if (get_drcs_bitmap(glyph, glyph_width, left_x + smpl_x, top_y + smpl_y)) {
              hit++;
            }
            n_smpl++;
          }
        }
      }

      if (n_smpl <= hit * 2) {
        w++;

        if (x_off_sum + 1 == ch_width * num_glyphs) {
          /* for x_off - w */
          x_off_sum++;
        } else {
          continue;
        }
      } else if (w == 0) {
        continue;
      }

      if (size_attr >= DOUBLE_HEIGHT_TOP) {
        int exp_y;

        if (size_attr == DOUBLE_HEIGHT_TOP) {
          if (y_off >= line_height / 2) {
            return 1;
          }

          exp_y = y + y_off;
        } else {
          exp_y = y + y_off - line_height / 2;
        }

        ui_window_fill_with(window, fg_xcolor, x + x_off_sum - w, exp_y, w, 1);
      } else {
        ui_window_fill_with(window, fg_xcolor, x + x_off_sum - w, y + y_off, w, 1);
      }

      w = 0;
    }
  }

  return 1;
}
#endif

static int get_state(ef_charset_t ch_cs, u_int32_t ch_code, vt_char_t *comb_chars,
                     u_int32_t *pic_glyph, char **drcs_glyph, int *draw_alone) {
#ifndef NO_IMAGE
  if (comb_chars && vt_char_cs(comb_chars) == PICTURE_CHARSET) {
    *draw_alone = 0; /* forcibly set 0 regardless of uifont. */
    *pic_glyph = vt_char_code(comb_chars) | (vt_char_picture_id(comb_chars) << PICTURE_POS_BITS);
    *drcs_glyph = NULL;

    return 4;
  } else
#endif
  {
    *pic_glyph = 0;

    if ((*drcs_glyph = vt_drcs_get_glyph(ch_cs, ch_code))) {
      *draw_alone = 0; /* forcibly set 0 regardless of uifont. */

      return 3;
    } else {
      if (comb_chars) {
        *draw_alone = 1;
      }

      if (ch_cs == DEC_SPECIAL) {
        return 1;
      } else {
        return 0;
      }
    }
  }
}

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)

static void fc_draw_combining_chars(ui_window_t *window, ui_font_manager_t *font_man,
                                    ui_color_t *xcolor, /* fg color */
                                    vt_char_t *chars, u_int size, int x, int y) {
  u_int count;
  u_int32_t ch_code;
  ef_charset_t ch_cs;
  ui_font_t *uifont;

  for (count = 0; count < size; count++) {
    if (vt_char_cols(&chars[count]) == 0) {
      continue;
    }

    ch_code = vt_char_code(&chars[count]);
    ch_cs = vt_char_cs(&chars[count]);
    uifont = ui_get_font(font_man, vt_char_font(&chars[count]));

    if (ch_cs == DEC_SPECIAL) {
      u_char c;

      c = ch_code;
      ui_window_draw_decsp_string(window, uifont, xcolor, x, y, &c, 1);
    }
    /* ISCII characters never have combined ones. */
    else if (ch_cs == US_ASCII || ch_cs == ISO8859_1_R /* || IS_ISCII(ch_cs) */) {
      u_char c;

      c = ch_code;
      ui_window_ft_draw_string8(window, uifont, xcolor, x, y, &c, 1);
    } else {
      /* FcChar32 */ u_int32_t ucs4;

      if (ch_cs == ISO10646_UCS4_1_V) {
        ui_window_ft_draw_string32(window, uifont, xcolor, x + vt_char_get_offset(&chars[count]), y,
                                   &ch_code, 1);
      } else if ((ucs4 = ui_convert_to_xft_ucs4(ch_code, ch_cs))) {
        ui_window_ft_draw_string32(window, uifont, xcolor, x, y, &ucs4, 1);
      }
    }
  }
}

static int fc_draw_str(ui_window_t *window, ui_font_manager_t *font_man,
                       ui_color_manager_t *color_man, u_int *updated_width, vt_char_t *chars,
                       u_int num_chars, int x, int y, u_int height, u_int ascent,
                       int top_margin, int hide_underline, int underline_offset) {
  int count;
  int start_draw;
  int end_of_str;
  u_int current_width;
  /* FcChar8 */ u_int8_t *str8;
  /* FcChar32 */ u_int32_t *str32;
  u_int str_len;

  u_int32_t ch_code;
  u_int ch_width;
  ef_charset_t ch_cs;

  int state;
  ui_font_t *uifont;
  vt_font_t font;
  vt_color_t fg_color;
  vt_color_t bg_color;
  int line_style;
  vt_char_t *comb_chars;
  u_int comb_size;
  int draw_alone;
  u_int32_t pic_glyph;
  u_int32_t *pic_glyphs;
  char *drcs_glyph;
  char **drcs_glyphs;

  int next_state;
  ui_font_t *next_uifont;
  vt_font_t next_font;
  vt_color_t next_fg_color;
  vt_color_t next_bg_color;
  int next_line_style;
  vt_char_t *next_comb_chars;
  u_int next_comb_size;
  u_int next_ch_width;
  int next_draw_alone;

#ifdef PERF_DEBUG
  int draw_count = 0;
#endif

  if (num_chars == 0) {
#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " input chars length is 0(ui_window_draw_str).\n");
#endif

    return 1;
  }

  start_draw = 0;
  end_of_str = 0;

  count = 0;
  while (vt_char_cols(&chars[count]) == 0) {
    if (++count >= num_chars) {
      return 1;
    }
  }

  ch_code = vt_char_code(&chars[count]);
  uifont = ui_get_font(font_man, (font = vt_char_font(&chars[count])));
  ch_cs = FONT_CS(font);
  comb_chars = vt_get_combining_chars(&chars[count], &comb_size);

  ch_width = calculate_char_width(uifont, ch_code, ch_cs, comb_chars, comb_size, &draw_alone);

  if ((current_width = x + ch_width) > window->width || y + height > window->height) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " draw string outside screen. (x %d w %d y %d h %d)\n", x, ch_width,
                   y, height);
#endif

    return 0;
  }

  if ((state = get_state(ch_cs, ch_code, comb_chars, &pic_glyph, &drcs_glyph, &draw_alone)) == 0 &&
      ch_cs != US_ASCII && ch_cs != ISO8859_1_R && !IS_ISCII(ch_cs)) {
    state = 2;
  }

  fg_color = vt_char_fg_color(&chars[count]);
  bg_color = vt_char_bg_color(&chars[count]);

  line_style = vt_char_line_style(&chars[count]);

  if (!(str8 = str32 = pic_glyphs = drcs_glyphs =
            alloca(K_MAX(sizeof(*str8),
                         K_MAX(sizeof(*str32), K_MAX(sizeof(*pic_glyphs), sizeof(*drcs_glyphs)))) *
                   num_chars))) {
    return 0;
  }

  str_len = 0;

  while (1) {
    if (state <= 1) {
      str8[str_len++] = ch_code;
    } else if (state >= 3) {
      if (drcs_glyph) {
        drcs_glyphs[str_len++] = drcs_glyph;
      } else /* if( pic_glyph) */
      {
        pic_glyphs[str_len++] = pic_glyph;
      }
    } else /* if( state == 2) */
    {
      u_int32_t ucs4;

      if (!(ucs4 = ui_convert_to_xft_ucs4(ch_code, ch_cs))) {
#ifdef DEBUG
        bl_warn_printf(BL_DEBUG_TAG " strange character 0x%x, ignored.\n", ch_code);
#endif
      } else {
        str32[str_len++] = ucs4;
      }
    }

    /*
     * next character.
     */

    do {
      if (++count >= num_chars) {
        start_draw = 1;
        end_of_str = 1;

        break;
      }
    } while (vt_char_cols(&chars[count]) == 0);

    if (!end_of_str) {
      ch_code = vt_char_code(&chars[count]);
      next_uifont = ui_get_font(font_man, (next_font = vt_char_font(&chars[count])));
      ch_cs = FONT_CS(next_font);
      next_fg_color = vt_char_fg_color(&chars[count]);
      next_bg_color = vt_char_bg_color(&chars[count]);
      next_line_style = vt_char_line_style(&chars[count]);
      next_comb_chars = vt_get_combining_chars(&chars[count], &next_comb_size);
      next_ch_width = calculate_char_width(next_uifont, ch_code, ch_cs, next_comb_chars,
                                           next_comb_size, &next_draw_alone);

      if ((next_state = get_state(ch_cs, ch_code, next_comb_chars, &pic_glyph, &drcs_glyph,
                                  &next_draw_alone)) == 0 &&
          ch_cs != US_ASCII && ch_cs != ISO8859_1_R && !IS_ISCII(ch_cs)) {
        next_state = 2;
      }

      if (current_width + next_ch_width > window->width) {
        start_draw = 1;
        end_of_str = 1;
      }
      /*
       * !! Notice !!
       * next_uifont != uifont doesn't necessarily detect change of 'state'
       * (for example, same Unicode font is used for both US_ASCII and
       * other half-width unicode characters) and 'bold'(ui_get_font()
       * might substitute normal fonts for bold ones), 'next_state' and
       * 'font & FONT_BOLD' is necessary.
       */
      else if (next_uifont != uifont || next_fg_color != fg_color || next_bg_color != bg_color ||
               next_line_style != line_style ||
               /* If line_style > 0, line is drawn one by one in vertical mode. */
               (line_style && uifont->is_vertical) || state != next_state ||
               draw_alone || next_draw_alone ||
               /* FONT_BOLD flag is not the same. */
               ((font ^ next_font) & FONT_BOLD)) {
        start_draw = 1;
      } else {
        start_draw = 0;
      }
    }

    if (start_draw) {
      /*
       * status is changed.
       */

      ui_color_t *fg_xcolor;
      ui_color_t *bg_xcolor;

#ifdef PERF_DEBUG
      draw_count++;
#endif

#ifndef NO_IMAGE
      if (state == 4) {
        draw_picture(window, pic_glyphs, str_len, x, y, ch_width, height);

        goto end_draw;
      }
#endif

      fg_xcolor = ui_get_xcolor(color_man, fg_color);
      bg_xcolor = ui_get_xcolor(color_man, bg_color);

      /*
       * clearing background
       */
      if (bg_color == VT_BG_COLOR) {
        if (updated_width) {
          ui_window_clear(window, x, y, current_width - x, height);
        }
      } else {
        ui_window_fill_with(window, bg_xcolor, x, y, current_width - x, height);
      }

      /*
       * drawing string
       */
      if (fg_color == bg_color) {
        /* don't draw it */
      } else if (state == 0) {
        ui_window_ft_draw_string8(window, uifont, fg_xcolor, x, y + ascent, str8, str_len);
      } else if (state == 1) {
        ui_window_draw_decsp_string(window, uifont, fg_xcolor, x, y + ascent, str8, str_len);
      } else if (state == 2) {
        ui_window_ft_draw_string32(window, uifont, fg_xcolor, x, y + ascent, str32, str_len);
      } else /* if( state == 3) */
      {
        draw_drcs(window, drcs_glyphs, str_len, x, y, ch_width, height, fg_xcolor,
                  font_man->size_attr);
      }

      if (comb_chars) {
        fc_draw_combining_chars(window, font_man, fg_xcolor, comb_chars, comb_size,
/*
 * 'current_width' is for some thai fonts which
 * automatically draw combining chars.
 * e.g.)
 *  -thai-fixed-medium-r-normal--14-100-100-100-m-70-tis620.2529-1
 *  (distributed by ZzzThai
 *   http://zzzthai.fedu.uec.ac.jp/ZzzThai/)
 *  win32 unicode font.
 */
#if 0
                                current_width
#else
                                current_width - ch_width
#endif
                                ,
                                y + ascent);
      }

      if (line_style) {
        if ((line_style & LS_UNDERLINE) && !hide_underline) {
          draw_line(window, fg_xcolor, uifont->is_vertical,
                    line_style & LS_UNDERLINE,
                    x, y, current_width - x, height, ascent + underline_offset, top_margin);
        }

        if (line_style & LS_CROSSED_OUT) {
          draw_line(window, fg_xcolor, uifont->is_vertical,
                    LS_CROSSED_OUT, x, y, current_width - x, height, ascent, top_margin);
        }

        if (line_style & LS_OVERLINE) {
          draw_line(window, fg_xcolor, uifont->is_vertical,
                    LS_OVERLINE, x, y, current_width - x, height, ascent, top_margin);
        }
      }

    end_draw:
      start_draw = 0;

      x = current_width;
      str_len = 0;
    }

    if (end_of_str) {
      break;
    }

    line_style = next_line_style;
    uifont = next_uifont;
    font = next_font;
    fg_color = next_fg_color;
    bg_color = next_bg_color;
    state = next_state;
    draw_alone = next_draw_alone;
    comb_chars = next_comb_chars;
    comb_size = next_comb_size;
    current_width += (ch_width = next_ch_width);
  }

  if (updated_width != NULL) {
    *updated_width = current_width;
  }

#ifdef PERF_DEBUG
  bl_debug_printf(" drawing %d times in a line.\n", draw_count);
#endif

  return 1;
}

#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)

#ifndef USE_CONSOLE
static int xcore_draw_combining_chars(ui_window_t *window, ui_font_manager_t *font_man,
                                      ui_color_t *xcolor, /* fg color */
                                      vt_char_t *chars, u_int size, int x, int y) {
  u_int count;
  u_int32_t ch_code;
  ef_charset_t ch_cs;
  ui_font_t *uifont;
  int x_off;

  for (count = 0; count < size; count++) {
    if (vt_char_cols(&chars[count]) == 0) {
      continue;
    }

    ch_code = vt_char_code(&chars[count]);
    ch_cs = vt_char_cs(&chars[count]);
    uifont = ui_get_font(font_man, vt_char_font(&chars[count]));

    if (ch_cs == DEC_SPECIAL) {
      u_char c;

      c = ch_code;
      ui_window_draw_decsp_string(window, uifont, xcolor, x, y, &c, 1);
    } else {
      if (ch_cs == ISO10646_UCS4_1_V) {
        x_off = vt_char_get_offset(&chars[count]);
      } else {
        x_off = 0;
      }

      if (ch_code < 0x100) {
        u_char c;

        c = ch_code;
        ui_window_draw_string(window, uifont, xcolor, x + x_off, y, &c, 1);
      } else {
        /* UCS4 */

        /* [2] is for surroage pair. */
        XChar2b xch[2];
        u_int len;

        if (IS_ISO10646_UCS4(ch_cs)) {
          if ((len = ui_convert_ucs4_to_utf16(xch, ch_code) / 2) == 0) {
            continue;
          }
        } else {
          xch[0].byte1 = (ch_code >> 8) & 0xff;
          xch[0].byte2 = ch_code & 0xff;
          len = 1;
        }

        ui_window_draw_string16(window, uifont, xcolor, x, y, xch, len);
      }
    }
  }

  return 1;
}
#endif

static int xcore_draw_str(ui_window_t *window, ui_font_manager_t *font_man,
                          ui_color_manager_t *color_man, u_int *updated_width, vt_char_t *chars,
                          u_int num_chars, int x, int y, u_int height, u_int ascent,
                          int top_margin, int hide_underline, int underline_offset) {
  int count;
  int start_draw;
  int end_of_str;
  u_int current_width;
  u_char *str;
  XChar2b *str2b;
  u_int str_len;
  u_int32_t ch_code;
  ef_charset_t ch_cs;

  int state; /* 0(8bit),1(decsp),2(16bit) */
  vt_char_t *comb_chars;
  u_int comb_size;
  u_int ch_width;
  ui_font_t *uifont;
  vt_font_t font;
  vt_color_t fg_color;
  vt_color_t bg_color;
  int line_style;
  int draw_alone;
  u_int32_t pic_glyph;
  u_int32_t *pic_glyphs;
  char *drcs_glyph;
  char **drcs_glyphs;

  int next_state;
  vt_char_t *next_comb_chars;
  u_int next_comb_size;
  u_int next_ch_width;
  ui_font_t *next_uifont;
  vt_font_t next_font;
  vt_color_t next_fg_color;
  vt_color_t next_bg_color;
  int next_line_style;
  int next_draw_alone;
#ifdef PERF_DEBUG
  int draw_count = 0;
#endif

  if (num_chars == 0) {
#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " input chars length is 0(ui_window_draw_str).\n");
#endif

    return 1;
  }

  count = 0;
  while (vt_char_cols(&chars[count]) == 0) {
    if (++count >= num_chars) {
      return 1;
    }
  }

  start_draw = 0;
  end_of_str = 0;

  ch_code = vt_char_code(&chars[count]);
  uifont = ui_get_font(font_man, (font = vt_char_font(&chars[count])));
  ch_cs = FONT_CS(font);
  comb_chars = vt_get_combining_chars(&chars[count], &comb_size);

  ch_width = calculate_char_width(uifont, ch_code, ch_cs, comb_chars, comb_size, &draw_alone);

  if ((current_width = x + ch_width) > window->width || y + height > window->height) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " draw string outside screen. (x %d w %d y %d h %d)\n", x, ch_width,
                   y, height);
#endif

    return 0;
  }

  if ((state = get_state(ch_cs, ch_code, comb_chars, &pic_glyph, &drcs_glyph, &draw_alone)) == 0 &&
      ch_code >= 0x100) {
    state = 2;
  }

  fg_color = vt_char_fg_color(&chars[count]);
  bg_color = vt_char_bg_color(&chars[count]);
  line_style = vt_char_line_style(&chars[count]);

  if (!(str2b = str = pic_glyphs = drcs_glyphs =
            /* '* 2' is for UTF16 surrogate pair. */
        alloca(K_MAX(sizeof(*str2b) * 2,
                     K_MAX(sizeof(*str), K_MAX(sizeof(*pic_glyphs), sizeof(*drcs_glyphs)))) *
               num_chars))) {
    return 0;
  }

  str_len = 0;

  while (1) {
    if (state <= 1) {
      str[str_len++] = ch_code;
    } else if (state >= 3) {
      if (pic_glyph) {
        pic_glyphs[str_len++] = pic_glyph;
      } else /* if( drcs_glyph) */
      {
        drcs_glyphs[str_len++] = drcs_glyph;
      }
    } else if (!IS_ISO10646_UCS4(ch_cs)) {
      str2b[str_len].byte1 = (ch_code >> 8) & 0xff;
      str2b[str_len].byte2 = ch_code & 0xff;
      str_len++;
    } else {
      /* UCS4 */

      str_len += (ui_convert_ucs4_to_utf16(str2b + str_len, ch_code) / 2);
    }

    /*
     * next character.
     */

    do {
      if (++count >= num_chars) {
        start_draw = 1;
        end_of_str = 1;

        break;
      }
    } while (vt_char_cols(&chars[count]) == 0);

    if (!end_of_str) {
      ch_code = vt_char_code(&chars[count]);
      next_uifont = ui_get_font(font_man, (next_font = vt_char_font(&chars[count])));
      ch_cs = FONT_CS(next_font);
      next_fg_color = vt_char_fg_color(&chars[count]);
      next_bg_color = vt_char_bg_color(&chars[count]);
      next_line_style = vt_char_line_style(&chars[count]);
      next_comb_chars = vt_get_combining_chars(&chars[count], &next_comb_size);
      next_ch_width = calculate_char_width(next_uifont, ch_code, ch_cs, next_comb_chars,
                                           next_comb_size, &next_draw_alone);

      if ((next_state = get_state(ch_cs, ch_code, next_comb_chars, &pic_glyph, &drcs_glyph,
                                  &next_draw_alone)) == 0 &&
          ch_code >= 0x100) {
        next_state = 2;
      }

      if (current_width + next_ch_width > window->width) {
        start_draw = 1;
        end_of_str = 1;
      }
      /*
       * !! Notice !!
       * next_uifont != uifont doesn't necessarily detect change of 'state'
       * (for example, same Unicode font is used for both US_ASCII and
       * other half-width unicode characters) and 'bold'(ui_get_font()
       * might substitute normal fonts for bold ones), 'next_state' and
       * 'font & FONT_BOLD' is necessary.
       */
      else if (next_uifont != uifont || next_fg_color != fg_color || next_bg_color != bg_color ||
               next_line_style != line_style ||
               /* If line_style > 0, line is drawn one by one in vertical mode. */
               (line_style && uifont->is_vertical) || next_state != state ||
               draw_alone || next_draw_alone ||
               /* FONT_BOLD flag is not the same */
               ((font ^ next_font) & FONT_BOLD)) {
        start_draw = 1;
      } else {
        start_draw = 0;
      }
    }

    if (start_draw) {
      /*
       * status is changed.
       */

      ui_color_t *fg_xcolor;
      ui_color_t *bg_xcolor;

#ifdef PERF_DEBUG
      draw_count++;
#endif

#ifndef NO_IMAGE
      if (state == 4) {
        draw_picture(window, pic_glyphs, str_len, x, y, ch_width, height);

        goto end_draw;
      }
#endif

      fg_xcolor = ui_get_xcolor(color_man, fg_color);

#ifdef DRAW_SCREEN_IN_PIXELS
      if (ui_window_has_wall_picture(window) && bg_color == VT_BG_COLOR) {
        bg_xcolor = NULL;
      } else
#endif
      {
        bg_xcolor = ui_get_xcolor(color_man, bg_color);
      }

#ifdef USE_CONSOLE
      /* XXX DRCS (state == 3) is ignored */
      if (state < 3) {
        u_int comb_count;

        for (comb_count = 0; comb_count < comb_size; comb_count++) {
          u_int comb_code;

          comb_code = vt_char_code(&comb_chars[comb_count]);

          if (state <= 1) {
            str[str_len++] = comb_code;
          } else if (!IS_ISO10646_UCS4(ch_cs)) {
            str2b[str_len].byte1 = (comb_code >> 8) & 0xff;
            str2b[str_len].byte2 = comb_code & 0xff;
            str_len++;
          } else {
            /* UCS4 */
            str_len += (ui_convert_ucs4_to_utf16(str2b + str_len, comb_code) / 2);
          }
        }

        /* XXX Wall picture is overwritten by bg_xcolor. */

        if (state == 2) {
          ui_window_console_draw_string16(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str2b,
                                          str_len, line_style);
        } else if (state == 1) {
          ui_window_console_draw_decsp_string(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent,
                                              str, str_len, line_style);
        } else /* if( state == 0) */
        {
          ui_window_console_draw_string(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str,
                                        str_len, line_style);
        }
      }
#else /* !USE_CONSOLE */
#ifdef USE_SDL2
      if (uifont->height != height || state == 3 ||
          (uifont->is_proportional && ui_window_has_wall_picture(window))) {
        if (bg_color == VT_BG_COLOR) {
          ui_window_clear(window, x, y, current_width - x, height);
        } else {
          ui_window_fill_with(window, bg_xcolor, x, y, current_width - x, height);
        }
      }

      if (state == 2) {
        ui_window_draw_image_string16(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str2b,
                                      str_len);
      } else if (state == 1) {
        ui_window_draw_decsp_image_string(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str,
                                          str_len);
      } else if( state == 0) {
        ui_window_draw_image_string(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str,
                                    str_len);
      } else /* if( state == 3) */ {
        draw_drcs(window, drcs_glyphs, str_len, x, y, ch_width, height, fg_xcolor,
                  font_man->size_attr);
      }
#else /* !USE_SDL2 */
#ifndef NO_DRAW_IMAGE_STRING
      if (
#ifdef DRAW_SCREEN_IN_PIXELS
#ifdef USE_FREETYPE
          /*
           * ISCII or ISO10646_UCS4_1_V
           * (see #ifdef USE_FREETYPE #endif in draw_string() in ui_window.c)
           */
          (uifont->is_proportional && ui_window_has_wall_picture(window)) ||
#endif
          /* draw_alone || */       /* draw_alone is always false on framebuffer. */
#else /* DRAW_SCREEN_IN_PIXELS */
#if defined(USE_WIN32GUI) && defined(USE_OT_LAYOUT)
          /*
           * U+2022 is ambiguous and should be drawn one by one, but
           * ui_calculate_char_width() can't tell it as ambigous if
           * use_ot_layout is true because ch_code is glyph index.
           */
          (uifont->use_ot_layout /* && uifont->ot_font */) ||
#endif
          (ui_window_has_wall_picture(window) && bg_color == VT_BG_COLOR) || draw_alone ||
#endif /* DRAW_SCREEN_IN_PIXELS */
          uifont->height != height || state == 3)
#endif /* NO_DRAW_IMAGE_STRING */
      {
        if (bg_color == VT_BG_COLOR) {
          ui_window_clear(window, x, y, current_width - x, height);
        } else {
          ui_window_fill_with(window, bg_xcolor, x, y, current_width - x, height);
        }

        if (fg_color == bg_color) {
          /* don't draw it */
        } else if (state == 2) {
          ui_window_draw_string16(window, uifont, fg_xcolor, x, y + ascent, str2b, str_len);
        } else if (state == 1) {
          ui_window_draw_decsp_string(window, uifont, fg_xcolor, x, y + ascent, str, str_len);
        } else if (state == 0) {
          ui_window_draw_string(window, uifont, fg_xcolor, x, y + ascent, str, str_len);
        } else /* if( state == 3) */
        {
          draw_drcs(window, drcs_glyphs, str_len, x, y, ch_width, height, fg_xcolor,
                    font_man->size_attr);
        }
      }
#ifndef NO_DRAW_IMAGE_STRING
      else {
        if (state == 2) {
          ui_window_draw_image_string16(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str2b,
                                        str_len);
        } else if (state == 1) {
          ui_window_draw_decsp_image_string(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str,
                                            str_len);
        } else /* if( state == 0) */
        {
          ui_window_draw_image_string(window, uifont, fg_xcolor, bg_xcolor, x, y + ascent, str,
                                      str_len);
        }
      }
#endif
#endif /* USE_SDL2 */

      if (comb_chars) {
        xcore_draw_combining_chars(window, font_man, fg_xcolor, comb_chars, comb_size,
/*
 * 'current_width' is for some thai fonts which automatically
 * draw combining chars.
 * e.g.)
 *  -thai-fixed-medium-r-normal--14-100-100-100-m-70-tis620.2529-1
 *  (distributed by ZzzThai http://zzzthai.fedu.uec.ac.jp/ZzzThai/)
 *  win32 unicode font.
 */
#if 0
                                   current_width
#else
                                   current_width - ch_width
#endif
                                   ,
                                   y + ascent);
      }

      if (line_style) {
        if ((line_style & LS_UNDERLINE) && !hide_underline) {
          draw_line(window, fg_xcolor, uifont->is_vertical,
                    line_style & LS_UNDERLINE,
                    x, y, current_width - x, height, ascent + underline_offset, top_margin);
        }

        if (line_style & LS_CROSSED_OUT) {
          draw_line(window, fg_xcolor, uifont->is_vertical,
                    LS_CROSSED_OUT, x, y, current_width - x, height, ascent, top_margin);
        }

        if (line_style & LS_OVERLINE) {
          draw_line(window, fg_xcolor, uifont->is_vertical,
                    LS_OVERLINE, x, y, current_width - x, height, ascent, top_margin);
        }
      }
#endif /* USE_CONSOLE */

    end_draw:
      start_draw = 0;

      x = current_width;
      str_len = 0;
    }

    if (end_of_str) {
      break;
    }

    uifont = next_uifont;
    font = next_font;
    fg_color = next_fg_color;
    bg_color = next_bg_color;
    line_style = next_line_style;
    state = next_state;
    draw_alone = next_draw_alone;
    comb_chars = next_comb_chars;
    comb_size = next_comb_size;
    current_width += (ch_width = next_ch_width);
  }

  if (updated_width != NULL) {
    *updated_width = current_width;
  }

#ifdef PERF_DEBUG
  bl_debug_printf(" drawing %d times in a line.\n", draw_count);
#endif

  return 1;
}

#endif

/* --- global functions --- */

int ui_draw_str(ui_window_t *window, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
                vt_char_t *chars, u_int num_chars, int x, int y, u_int height, u_int ascent,
                int top_margin, int hide_underline, int underline_offset) {
  u_int updated_width;
  int ret;

#ifdef __DEBUG
  bl_debug_printf("Draw %d characters.\n", num_chars);
#endif

  if (font_man->size_attr >= DOUBLE_HEIGHT_TOP) {
    ui_window_set_clip(window, x, y, window->width - x, height);
    ascent = height - (height - ascent) * 2;

    if (font_man->size_attr == DOUBLE_HEIGHT_TOP) {
      ascent += height;
    }
  }

  switch (ui_get_type_engine(font_man)) {
    default:
      ret = 0;
      break;

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
    case TYPE_XFT:
    case TYPE_CAIRO:
      ret = fc_draw_str(window, font_man, color_man, &updated_width, chars, num_chars, x, y,
                        height, ascent, top_margin, hide_underline, underline_offset);

      break;
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
    case TYPE_XCORE:
      ret = xcore_draw_str(window, font_man, color_man, &updated_width, chars, num_chars, x, y,
                           height, ascent, top_margin, hide_underline, underline_offset);

      break;
#endif
  }

  if (font_man->size_attr >= DOUBLE_HEIGHT_TOP) {
    ui_window_unset_clip(window);
  }

  return ret;
}

int ui_draw_str_to_eol(ui_window_t *window, ui_font_manager_t *font_man,
                       ui_color_manager_t *color_man, vt_char_t *chars, u_int num_chars, int x,
                       int y, u_int height, u_int ascent, int top_margin,
                       int hide_underline, int underline_offset) {
  u_int updated_width;
  int ret;

#ifdef __DEBUG
  bl_debug_printf("Draw %d characters to eol.\n", num_chars);
#endif

  if (font_man->size_attr >= DOUBLE_HEIGHT_TOP) {
    ui_window_set_clip(window, x, y, window->width - x, height);
    ascent = height - (height - ascent) * 2;

    if (font_man->size_attr == DOUBLE_HEIGHT_TOP) {
      ascent += height;
    }
  }

  switch (ui_get_type_engine(font_man)) {
    default:
      ret = 0;
      break;

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
    case TYPE_XFT:
    case TYPE_CAIRO:
      ui_window_clear(window, x, y, window->width - x, height);

      ret = fc_draw_str(
          window, font_man, color_man, NULL /* NULL disables ui_window_clear() in fc_draw_str() */,
          chars, num_chars, x, y, height, ascent, top_margin, hide_underline,
          underline_offset);

      break;
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
    case TYPE_XCORE:
      ret = xcore_draw_str(window, font_man, color_man, &updated_width, chars, num_chars, x, y,
                           height, ascent, top_margin, hide_underline, underline_offset);

      if (updated_width < window->width) {
        ui_window_clear(window, updated_width, y, window->width - updated_width, height);
      }

      break;
#endif
  }

  if (font_man->size_attr >= DOUBLE_HEIGHT_TOP) {
    ui_window_unset_clip(window);
  }

  return ret;
}

u_int ui_calculate_vtchar_width(ui_font_t *font, vt_char_t *ch, int *draw_alone) {
  ef_charset_t cs;
  vt_char_t *comb;
  u_int comb_size;

  if ((cs = FONT_CS(font->id)) == ISO10646_UCS4_1_V) {
    comb = vt_get_combining_chars(ch, &comb_size);
  } else {
    comb = NULL;
    comb_size = 0;
  }

  return calculate_char_width(font, vt_char_code(ch), cs, comb, comb_size, draw_alone);
}
