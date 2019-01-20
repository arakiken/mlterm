/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_window.h"

#include <cairo/cairo.h>
#include <cairo/cairo-ft.h> /* FcChar32 */
#include <cairo/cairo-xlib.h>

#include <pobl/bl_mem.h> /* alloca */
#include <vt_char.h>     /* UTF_MAX_SIZE */

#define DIVIDE_ROUNDINGUP(a, b) (((int)((a)*10 + (b)*10 - 1)) / ((int)((b)*10)))

#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
static cairo_glyph_t *glyph_buf;
static int num_glyph_buf;
#endif

/* Implemented in ui_font_ft.c */
size_t ui_convert_ucs4_to_utf8(u_char *utf8, u_int32_t ucs);
int ui_search_next_cairo_font(ui_font_t *font, int ch);

/* --- static functions --- */

#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
static void adjust_glyphs(ui_font_t *font, cairo_glyph_t *glyphs, int num_glyphs) {
  if (!font->is_var_col_width) {
    int count;
    double prev_x;
    int adjust;
    int std_width;

    adjust = 0;
    prev_x = glyphs[0].x;
    for (count = 1; count < num_glyphs; count++) {
      int w;

      w = glyphs[count].x - prev_x;
      prev_x = glyphs[count].x;

      if (!adjust) {
        if (w == font->width) {
          continue;
        }
        adjust = 1;
        std_width = font->width - font->x_off * 2;
      }

      glyphs[count].x = glyphs[count - 1].x + font->width;
      glyphs[count - 1].x += ((std_width - w) / 2);
    }
  }
}

static void flush_glyphs(cairo_t *cr) {
  if (num_glyph_buf > 0) {
    u_long pixel = cairo_get_user_data(cr, 2);

    cairo_set_scaled_font(cr, cairo_get_user_data(cr, 1));
    cairo_set_source_rgba(cr,
                          (double)((pixel >> 16) & 0xff) / 255.0,
                          (double)((pixel >> 8) & 0xff) / 255.0,
                          (double)(pixel & 0xff) / 255.0,
                          (double)((pixel >> 24) & 0xff) / 255.0);
    cairo_show_glyphs(cr, glyph_buf, num_glyph_buf);
    num_glyph_buf = 0;
  }
}

static void add_glyphs(cairo_glyph_t *glyphs, int num) {
  static int buf_size;

  if (num_glyph_buf + num > buf_size) {
    void *p;

    buf_size = num_glyph_buf + num + 100;

    if (!(p = realloc(glyph_buf, sizeof(cairo_glyph_t) * buf_size))) {
      return;
    }

    glyph_buf = p;
  }

  memcpy(glyph_buf + num_glyph_buf, glyphs, sizeof(cairo_glyph_t) * num);
  num_glyph_buf += num;
}

static int show_iscii(cairo_t *cr, cairo_scaled_font_t *xfont, ui_font_t *font,
                      ui_color_t *fg_color, int x, int y,
                      u_char *str, u_int str_len) {
  cairo_glyph_t *glyphs;
  u_int count;
  cairo_text_extents_t extents;
  FT_Face face;
  int drawn_x;

  if (cairo_get_user_data(cr, 1) != xfont) {
    flush_glyphs(cr);
    cairo_set_user_data(cr, 1, xfont, NULL);
  }

  /*
   * If cairo_get_user_data() returns NULL, it means that source rgb value is
   * default one (black == 0).
   */
  if ((u_long)cairo_get_user_data(cr, 2) !=
      (u_long)((fg_color->red << 16) | (fg_color->green << 8) |
               (fg_color->blue) | (fg_color->alpha << 24))) {
    flush_glyphs(cr);
    cairo_set_user_data(cr, 2,
                        (u_long)((fg_color->red << 16) | (fg_color->green << 8) |
                                 (fg_color->blue) | (fg_color->alpha << 24)),
                        NULL);
  }

  if (font->size_attr == DOUBLE_WIDTH) {
    flush_glyphs(cr);
    x /= 2;
    font->width /= 2;
    cairo_scale(cr, 2.0, 1.0);
  }

  if (!(glyphs = alloca(sizeof(*glyphs) * (str_len + 1)))) {
    return 0;
  }

  face = cairo_ft_scaled_font_lock_face(xfont);
  FT_Select_Charmap(face, FT_ENCODING_APPLE_ROMAN);

  glyphs[0].x = x;
  glyphs[0].y = y;
  for (count = 0; count < str_len; count++) {
    glyphs[count].index = FT_Get_Char_Index(face, str[count]);
    cairo_scaled_font_glyph_extents(xfont, glyphs + count, 1, &extents);
    glyphs[count + 1].x = glyphs[count].x + extents.x_advance;
    glyphs[count + 1].y = y;
  }

  cairo_ft_scaled_font_unlock_face(xfont);

  add_glyphs(glyphs, str_len);

  if (font->double_draw_gap) {
    for (count = 0; count < str_len; count++) {
      glyphs[count].x += font->double_draw_gap;
    }

    add_glyphs(glyphs, str_len);
  }

  drawn_x = glyphs[str_len].x;

  if (font->size_attr == DOUBLE_WIDTH) {
    flush_glyphs(cr);
    font->width *= 2;
    cairo_scale(cr, 0.5, 1.0);
  }

  return drawn_x;
}
#endif

static int show_text(cairo_t *cr, cairo_scaled_font_t *xfont, ui_font_t *font,
                     ui_color_t *fg_color, int x, int y,
                     u_char *str /* NULL-terminated UTF8 or FcChar32* */,
                     u_int str_len) {
#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
  int drawn_x;
#endif

#if CAIRO_VERSION_ENCODE(1, 4, 0) <= CAIRO_VERSION
  if (cairo_get_user_data(cr, 1) != xfont)
#endif
  {
#if CAIRO_VERSION_ENCODE(1, 8, 0) > CAIRO_VERSION
    cairo_set_scaled_font(cr, xfont);
#else
    flush_glyphs(cr);
#endif
#if CAIRO_VERSION_ENCODE(1, 4, 0) <= CAIRO_VERSION
    cairo_set_user_data(cr, 1, xfont, NULL);
#endif
  }

#if CAIRO_VERSION_ENCODE(1, 4, 0) <= CAIRO_VERSION
  /*
   * If cairo_get_user_data() returns NULL, it means that source rgb value is
   * default one (black == 0).
   */
  if ((u_long)cairo_get_user_data(cr, 2) !=
      (u_long)((fg_color->red << 16) | (fg_color->green << 8) |
               (fg_color->blue) | (fg_color->alpha << 24)))
#endif
  {
#if CAIRO_VERSION_ENCODE(1, 8, 0) > CAIRO_VERSION
    cairo_set_source_rgba(cr, (double)fg_color->red / 255.0, (double)fg_color->green / 255.0,
                          (double)fg_color->blue / 255.0, (double)fg_color->alpha / 255.0);
#else
    flush_glyphs(cr);
#endif

#if CAIRO_VERSION_ENCODE(1, 4, 0) <= CAIRO_VERSION
    cairo_set_user_data(cr, 2,
                        (u_long)((fg_color->red << 16) | (fg_color->green << 8) |
                                 (fg_color->blue) | (fg_color->alpha << 24)),
                        NULL);
#endif
  }

  if (font->size_attr == DOUBLE_WIDTH) {
    flush_glyphs(cr);
    x /= 2;
    font->width /= 2;
    cairo_scale(cr, 2.0, 1.0);
  }

#if CAIRO_VERSION_ENCODE(1, 8, 0) > CAIRO_VERSION
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, str);

  if (font->double_draw_gap) {
    cairo_move_to(cr, x + font->double_draw_gap, y);
    cairo_show_text(cr, str);
  }

  if (font->size_attr == DOUBLE_WIDTH) {
    font->width *= 2;
    cairo_scale(cr, 0.5, 1.0);
  }

  return 1;
#else
#ifdef USE_OT_LAYOUT
  if (font->use_ot_layout /* && font->ot_font */) {
    cairo_glyph_t *glyphs;
    u_int count;
    cairo_text_extents_t extent;

    if (!(glyphs = alloca(sizeof(*glyphs) * (str_len + 1)))) {
      return 0;
    }

    glyphs[0].x = x;
    for (count = 0; count < str_len;) {
      glyphs[count].index = ((FcChar32*)str)[count];
      glyphs[count].y = y;
      cairo_scaled_font_glyph_extents(xfont, glyphs + count, 1, &extent);

      count++;

      glyphs[count].x = glyphs[count - 1].x + extent.x_advance;
    }

    adjust_glyphs(font, glyphs, str_len + 1);
    add_glyphs(glyphs, str_len);

    if (font->double_draw_gap) {
      for (count = 0; count < str_len; count++) {
        glyphs[count].x += font->double_draw_gap;
      }

      add_glyphs(glyphs, str_len);
    }

    drawn_x = glyphs[str_len].x;
  } else
#endif
  {
    static cairo_glyph_t *glyphs;
    static int num_glyphs;
    cairo_glyph_t *orig_glyphs;
    u_char *str2;

    orig_glyphs = glyphs;

    if (!(str2 = alloca(str_len + 2))) {
      return 0;
    }

    memcpy(str2, str, str_len);
    str2[str_len] = ' '; /* dummy */
    str2[str_len + 1] = '\0';

    if (cairo_scaled_font_text_to_glyphs(xfont, x, y, str2, str_len + 1, &glyphs, &num_glyphs,
                                         NULL, NULL, NULL) == CAIRO_STATUS_SUCCESS) {
      adjust_glyphs(font, glyphs, num_glyphs);
      num_glyphs--; /* remove dummy */
      add_glyphs(glyphs, num_glyphs);

      if (font->double_draw_gap) {
        int count;

        for (count = 0; count < num_glyphs; count++) {
          glyphs[count].x += font->double_draw_gap;
        }

        add_glyphs(glyphs, num_glyphs);
      }
    }

    if (orig_glyphs != glyphs) {
      cairo_glyph_free(orig_glyphs);
    }

    drawn_x = glyphs[num_glyphs].x;
  }

  if (font->size_attr == DOUBLE_WIDTH) {
    flush_glyphs(cr);
    font->width *= 2;
    cairo_scale(cr, 0.5, 1.0);
  }

  return drawn_x;
#endif
}

static int draw_string32(ui_window_t *win, cairo_scaled_font_t *xfont, ui_font_t *font,
                         ui_color_t *fg_color, int x, int y, FcChar32* str, u_int len) {
  u_char *buf;
  u_int count;
  char *p;

#ifdef USE_OT_LAYOUT
  if (font->use_ot_layout /* && font->ot_font */) {
    buf = str;
  } else
#endif
  {
    if (!(p = buf = alloca(UTF_MAX_SIZE * len + 1))) {
      return 0;
    }

    for (count = 0; count < len; count++) {
      p += ui_convert_ucs4_to_utf8(p, str[count]);
    }
    *p = '\0';
    len = strlen(buf);
  }

  return show_text(win->cairo_draw, xfont, font, fg_color, x + win->hmargin, y + win->vmargin,
                   buf, len);
}

/* --- global functions --- */

int ui_window_set_use_cairo(ui_window_t *win, int use_cairo) {
  if (use_cairo) {
    cairo_surface_t *surface = cairo_xlib_surface_create(win->disp->display,
                                                         win->my_window, win->disp->visual,
                                                         ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win));

    if (surface) {
      win->cairo_draw = cairo_create(surface);
      cairo_surface_destroy(surface);
#if 0
      cairo_set_operator(win->cairo_draw, CAIRO_OPERATOR_CLEAR /*CAIRO_OPERATOR_ATOP*/);
      cairo_set_antialias(win->cairo_draw, CAIRO_ANTIALIAS_NONE);
#endif

      if (win->cairo_draw) {
        return 1;
      }
    }
  } else {
    cairo_destroy(win->cairo_draw);
    win->cairo_draw = NULL;

    return 1;
  }

  return 0;
}

void ui_window_cairo_draw_string8(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                  int y, u_char *str, size_t len) {
  u_char *buf;
  size_t count;
  u_char *p;

#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
  ef_charset_t cs = FONT_CS(font->id);

  if (IS_ISCII(cs)) {
    show_iscii(win->cairo_draw, font->cairo_font, font, fg_color, x + font->x_off + win->hmargin,
               y + win->vmargin, str, len);
    return;
  }
#endif

  /* Removing trailing spaces. */
  while (1) {
    if (len == 0) {
      return;
    }

    if (*(str + len - 1) == ' ') {
      len--;
    } else {
      break;
    }
  }

  /* Max utf8 size of 0x80 - 0xff is 2 */
  if (!(p = buf = alloca(2 * len + 1))) {
    return;
  }

  for (count = 0; count < len; count++) {
    p += ui_convert_ucs4_to_utf8(p, (u_int32_t)(str[count]));
  }
  *p = '\0';

  show_text(win->cairo_draw, font->cairo_font, font, fg_color, x + font->x_off + win->hmargin,
            y + win->vmargin, buf, strlen(buf));
}

void ui_window_cairo_draw_string32(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                   int y, FcChar32* str, u_int len) {
  cairo_scaled_font_t *xfont;

  if (str == NULL) {
#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
    flush_glyphs(win->cairo_draw);
#endif

    return;
  }

  xfont = font->cairo_font;

/* draw_string32() before 1.8.0 doesn't return x position. */
#if CAIRO_VERSION_ENCODE(1, 8, 0) <= CAIRO_VERSION
  if (
#ifdef USE_OT_LAYOUT
      (!font->use_ot_layout /* || ! font->ot_font */) &&
#endif
      font->compl_fonts) {
    u_int count;

    for (count = 0; count < len;) {
      if (!FcCharSetHasChar(font->compl_fonts[0].charset, str[count])) {
        int compl_idx;

        if ((compl_idx = ui_search_next_cairo_font(font, str[count])) >= 0) {
          FcChar32* substr;
          int x_off;

          if (count > 0) {
            x = draw_string32(win, xfont, font, fg_color, x + font->x_off, y, str, count);
          }

          substr = str + count;

          for (count++;
               count < len && !FcCharSetHasChar(font->compl_fonts[0].charset, str[count]) &&
                   FcCharSetHasChar(font->compl_fonts[compl_idx + 1].charset, str[count]);
               count++);

          x_off = font->x_off;
          font->x_off = 0;
          x = draw_string32(win, font->compl_fonts[compl_idx].next, font, fg_color,
                            x + font->x_off, y, substr, substr - str + count);
          font->x_off = x_off;

          str += count;
          len -= count;
          count = 0;

          continue;
        }
      }

      count ++;
    }
  }
#endif

  draw_string32(win, xfont, font, fg_color, x + font->x_off, y, str, len);
}

void cairo_resize(ui_window_t *win) {
  cairo_xlib_surface_set_size(cairo_get_target(win->cairo_draw), ACTUAL_WIDTH(win),
                              ACTUAL_HEIGHT(win));
}

void cairo_set_clip(ui_window_t *win, int x, int y, u_int width, u_int height) {
  cairo_rectangle(win->cairo_draw, x, y, width, height);
  cairo_clip(win->cairo_draw);
}

void cairo_unset_clip(ui_window_t *win) { cairo_reset_clip(win->cairo_draw); }
