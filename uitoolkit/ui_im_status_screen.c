/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_im_status_screen.h"

#ifdef USE_IM_PLUGIN

#ifdef USE_SDL2
#define USE_STATUS_LINE
#endif

#ifndef USE_STATUS_LINE

#include <pobl/bl_mem.h>
#include <pobl/bl_util.h> /* BL_ARRAY_SIZE */
#include <vt_str.h>
#include <vt_parser.h>
#include "ui_draw_str.h"

#ifdef USE_CONSOLE
#define MARGIN 0
#define LINE_SPACE 0
#else
#define MARGIN 3
#define LINE_SPACE 2
#endif

#if defined(MANAGE_SUB_WINDOWS_BY_MYSELF) && !defined(MANAGE_ROOT_WINDOWS_BY_MYSELF)
/* USE_SDL2 || USE_WAYLAND */
#define DISPLAY(stat_screen) ((stat_screen)->window.disp->display->parent)
#else
#define DISPLAY(stat_screen) ((stat_screen)->window.disp)
#endif

/* --- static functions --- */

static void adjust_window_position_by_size(ui_im_status_screen_t *stat_screen, int *x, int *y) {
#if defined(MANAGE_SUB_WINDOWS_BY_MYSELF) && !defined(MANAGE_ROOT_WINDOWS_BY_MYSELF)
  /* USE_SDL2 || USE_WAYLAND (Display : Window = 1 : 1) */
  if (ACTUAL_HEIGHT(&stat_screen->window) > DISPLAY(stat_screen)->height) {
    /* do nothing */
  } else
#endif
  if (*y + ACTUAL_HEIGHT(&stat_screen->window) > DISPLAY(stat_screen)->height) {
    *y -= ACTUAL_HEIGHT(&stat_screen->window);
    if (!stat_screen->is_vertical) {
      *y -= stat_screen->line_height;
    }
  }

#if defined(MANAGE_SUB_WINDOWS_BY_MYSELF) && !defined(MANAGE_ROOT_WINDOWS_BY_MYSELF)
  /* USE_SDL2 || USE_WAYLAND (Display : Window = 1 : 1) */
  if (ACTUAL_WIDTH(&stat_screen->window) > DISPLAY(stat_screen)->width) {
    /* do nothing */
  } else
#endif
  if (*x + ACTUAL_WIDTH(&stat_screen->window) > DISPLAY(stat_screen)->width) {
    if (stat_screen->is_vertical) {
      /* ui_im_stat_screen doesn't know column width. */
      *x -= (ACTUAL_WIDTH(&stat_screen->window) + stat_screen->line_height);
    } else {
      *x = DISPLAY(stat_screen)->width - ACTUAL_WIDTH(&stat_screen->window);
    }
  }
}

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
static void reset_screen(ui_window_t *win) {
  ui_display_reset_input_method_window();
  ui_window_draw_rect_frame(win, -MARGIN, -MARGIN, win->width + MARGIN - 1,
                            win->height + MARGIN - 1);
}
#endif

static int is_nl(vt_char_t *ch) { return vt_char_cs(ch) == US_ASCII && vt_char_code(ch) == '\n'; }

static void draw_screen(ui_im_status_screen_t *stat_screen, int do_resize,
                        int modified_beg /* for canna */
                        ) {
#define MAX_ROWS (BL_ARRAY_SIZE(stat_screen->head_indexes) - 1)
  ui_font_t *font;
  u_int line_height;
  int *heads;
  u_int i;

  ui_font_manager_set_attr(stat_screen->font_man, 0, 0);
  font = ui_get_usascii_font(stat_screen->font_man);
  line_height = font->height + LINE_SPACE;
  heads = stat_screen->head_indexes;

  /*
   * resize window
   */

  /* width of window */

  if (do_resize) {
    u_int max_width;
    u_int tmp_max_width;
    u_int width;
    u_int rows;

    /* The minimum width of normal display is regarded as 640. */
    if ((max_width = DISPLAY(stat_screen)->width / 2) < 320) {
      max_width = 320;
    }

    tmp_max_width = 0;
    width = 0;
    heads[0] = 0;
    rows = 1;

    for (i = 0; i < stat_screen->filled_len; i++) {
      if (is_nl(&stat_screen->chars[i]) && rows < MAX_ROWS - 1) {
        if (rows == 1 || tmp_max_width < width) {
          tmp_max_width = width;
        }

        heads[rows++] = i + 1;
        width = 0;
      } else {
        u_int ch_width;

        ch_width = ui_calculate_vtchar_width(ui_get_font(stat_screen->font_man,
                                                         vt_char_font(&stat_screen->chars[i])),
                                             &stat_screen->chars[i], NULL);

        if (width + ch_width > max_width) {
          if (rows == 1 || tmp_max_width < width) {
            tmp_max_width = width;
          }

          heads[rows++] = i;

          if (rows == MAX_ROWS) {
            /* Characters after max_width at the last line are never shown. */
            break;
          }

          width = ch_width;
        } else {
          width += ch_width;
        }
      }
    }

    if (width < tmp_max_width) {
      width = tmp_max_width;
    }

    /* for following 'heads[i + 1] - heads[i]' */
    heads[rows] = stat_screen->filled_len;

    if (ui_window_resize(&stat_screen->window, width, line_height * rows, 0)) {
      int x;
      int y;

      x = stat_screen->x;
      y = stat_screen->y;
      adjust_window_position_by_size(stat_screen, &x, &y);

      if (stat_screen->window.x != x || stat_screen->window.y != y) {
        ui_window_move(&stat_screen->window, x, y);
      }

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
      reset_screen(&stat_screen->window);
#endif

      modified_beg = 0;
    }
  }

  /* XXX */
#ifdef USE_SDL2
  if (stat_screen->window.disp->display->resizing) {
    return;
  }
#endif

  for (i = 0; heads[i] < stat_screen->filled_len; i++) {
    if (heads[i + 1] > modified_beg) {
      u_int len;

      len = heads[i + 1] - heads[i];

      if (is_nl(&stat_screen->chars[heads[i + 1] - 1])) {
        len--;
      }

      ui_draw_str_to_eol(&stat_screen->window, stat_screen->font_man, stat_screen->color_man,
                         stat_screen->chars + heads[i], len, 0, line_height * i, line_height,
                         font->ascent + LINE_SPACE / 2, LINE_SPACE / 2,
                         1 /* no need to draw underline */, 0);
    }
  }

  ui_window_flush(&stat_screen->window);
}

/*
 * methods of ui_im_status_screen_t
 */

static void destroy(ui_im_status_screen_t *stat_screen) {
  ui_display_remove_root(stat_screen->window.disp, &stat_screen->window);

  if (stat_screen->chars) {
    vt_str_destroy(stat_screen->chars, stat_screen->num_chars);
  }

#ifdef USE_REAL_VERTICAL_FONT
  if (stat_screen->is_vertical) {
    ui_font_manager_destroy(stat_screen->font_man);
  }
#endif

  free(stat_screen);
}

static void show(ui_im_status_screen_t *stat_screen) { ui_window_map(&stat_screen->window); }

static void hide(ui_im_status_screen_t *stat_screen) {
  ui_window_unmap(&stat_screen->window);
}

static int set_spot(ui_im_status_screen_t *stat_screen, int x, int y) {
  stat_screen->x = x;
  stat_screen->y = y;

  adjust_window_position_by_size(stat_screen, &x, &y);

  if (stat_screen->window.x != x || stat_screen->window.y != y) {
    ui_window_move(&stat_screen->window, x, y);
#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
    reset_screen(&stat_screen->window);
    draw_screen(stat_screen, 0, 0);
#endif

    return 1;
  } else {
    return 0;
  }
}

static int set(ui_im_status_screen_t *stat_screen, ef_parser_t *parser, const u_char *str) {
  int count;
  ef_char_t ch;
  vt_char_t *p;
  vt_char_t *old_chars;
  u_int old_num_chars;
  u_int old_filled_len;
  int modified_beg;

  /*
   * count number of characters to allocate status[index].chars
   */

  (*parser->init)(parser);
  (*parser->set_str)(parser, str, strlen(str));
  for (count = 0; (*parser->next_char)(parser, &ch); count++)
    ;

  old_chars = stat_screen->chars;
  old_num_chars = stat_screen->num_chars;
  old_filled_len = stat_screen->filled_len;

  if (!(stat_screen->chars = vt_str_new(count))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " vt_str_new() failed.\n");
#endif

    return 0;
  }

  stat_screen->num_chars = count;
  stat_screen->filled_len = 0;

  /*
   * u_char -> vt_char_t
   */

  (*parser->init)(parser);
  (*parser->set_str)(parser, str, strlen(str));

  p = stat_screen->chars;

  vt_str_init(p, stat_screen->num_chars);

  while ((*parser->next_char)(parser, &ch)) {
    int is_fullwidth = 0;
    int is_awidth = 0;
    int is_comb = 0;

    /* -1 (== control sequence) is permitted for \n. */
    if (!vt_convert_to_internal_ch(stat_screen->vtparser, &ch)) {
      continue;
    }

    if (ch.property & EF_FULLWIDTH) {
      is_fullwidth = 1;
    } else if (ch.property & EF_AWIDTH) {
      /* TODO: check col_size_of_width_a */
      is_fullwidth = is_awidth = 1;
    }

    if (is_comb) {
      if (vt_char_combine(p - 1, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_awidth, is_comb,
                          VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0)) {
        continue;
      }

      /*
       * if combining failed , char is normally appended.
       */
    }

    if (vt_is_msb_set(ch.cs)) {
      SET_MSB(ch.ch[0]);
    }

    vt_char_set(p, ef_char_to_int(&ch), ch.cs, is_fullwidth, is_awidth, is_comb,
                VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0);

    p++;
    stat_screen->filled_len++;
  }

  for (modified_beg = 0;
       modified_beg < stat_screen->filled_len && modified_beg < old_filled_len &&
           vt_char_code_equal(old_chars + modified_beg, stat_screen->chars + modified_beg);
       modified_beg++)
    ;

  if (old_chars) {
    vt_str_destroy(old_chars, old_num_chars);
  }

  if (modified_beg < old_filled_len || old_filled_len != stat_screen->filled_len) {
    draw_screen(stat_screen, 1, modified_beg);
  }

  return 1;
}

/*
 * callbacks of ui_window events
 */

static void window_realized(ui_window_t *win) {
  ui_im_status_screen_t *stat_screen;

  stat_screen = (ui_im_status_screen_t *)win;

  ui_window_set_type_engine(&stat_screen->window, ui_get_type_engine(stat_screen->font_man));

  ui_window_set_fg_color(win, ui_get_xcolor(stat_screen->color_man, VT_FG_COLOR));
  ui_window_set_bg_color(win, ui_get_xcolor(stat_screen->color_man, VT_BG_COLOR));

  ui_window_set_override_redirect(&stat_screen->window, 1);
}

static void window_exposed(ui_window_t *win, int x, int y, u_int width, u_int height) {
  draw_screen((ui_im_status_screen_t *)win, 0, 0);

  /* draw border (margin area has been already cleared in ui_window.c) */
  ui_window_draw_rect_frame(win, -MARGIN, -MARGIN, win->width + MARGIN - 1,
                            win->height + MARGIN - 1);
}

/* --- global functions --- */

ui_im_status_screen_t *ui_im_status_screen_new(ui_display_t *disp, ui_font_manager_t *font_man,
                                               ui_color_manager_t *color_man, void *vtparser,
                                               int is_vertical, u_int line_height, int x, int y) {
  ui_im_status_screen_t *stat_screen;

  if ((stat_screen = calloc(1, sizeof(ui_im_status_screen_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

#ifdef USE_REAL_VERTICAL_FONT
  if (is_vertical) {
    stat_screen->font_man = ui_font_manager_new(disp->display,
                                                ui_get_type_engine(font_man),
                                                ui_get_font_present(font_man) & ~FONT_VERTICAL,
                                                ui_get_font_size(font_man),
                                                ui_get_current_usascii_font_cs(font_man),
                                                font_man->step_in_changing_font_size,
                                                ui_get_letter_space(font_man),
                                                ui_is_using_bold_font(font_man),
                                                ui_is_using_italic_font(font_man));
  } else
#endif
  {
    stat_screen->font_man = font_man;
  }

  stat_screen->color_man = color_man;
  stat_screen->vtparser = vtparser;

  stat_screen->x = x;
  stat_screen->y = y;
  stat_screen->line_height = line_height;

  stat_screen->is_vertical = is_vertical;

  if (!ui_window_init(&stat_screen->window, MARGIN * 2, MARGIN * 2, MARGIN * 2, MARGIN * 2, 0, 0,
                      MARGIN, MARGIN, /* ceate_gc */ 1, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_window_init failed.\n");
#endif

    goto error;
  }

  /*
   * +------------+
   * | ui_window.c | --- window events ---> +----------------------+
   * +------------+                        | ui_im_status_screen.c |
   * +------------+                        +----------------------+
   * | im plugin  | <------ methods ------
   * +------------+
   */

  /* callbacks for window events */
  stat_screen->window.window_realized = window_realized;
#if 0
  stat_screen->window.window_finalized = window_finalized;
#endif
  stat_screen->window.window_exposed = window_exposed;

  /* methods of ui_im_status_screen_t */
  stat_screen->destroy = destroy;
  stat_screen->show = show;
  stat_screen->hide = hide;
  stat_screen->set_spot = set_spot;
  stat_screen->set = set;

  if (!ui_display_show_root(disp, &stat_screen->window, x, y, XValue | YValue,
                            "mlterm-status-window", NULL, 0)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_display_show_root() failed.\n");
#endif

    goto error;
  }

  return stat_screen;

error:
  free(stat_screen);

  return NULL;
}

#else /* USE_STATUS_LINE */

#include <pobl/bl_mem.h>
#include <vt_parser.h>

/* --- static functions --- */

static void destroy(ui_im_status_screen_t *stat_screen) {
  vt_parser_write_loopback(stat_screen->vtparser, "\x1b[0$~", 5);
  free(stat_screen);
}

static void show(ui_im_status_screen_t *stat_screen) {}

static void hide(ui_im_status_screen_t *stat_screen) {
  vt_parser_write_loopback(stat_screen->vtparser, "\x1b[0$~", 5);
}

static int set_spot(ui_im_status_screen_t *stat_screen, int x, int y) { return 1; }

static void replace_char(char *str, char orig, char new) {
  char *p;

  for (p = str; *p; p++) {
    if (*p == orig) {
      *p = new;
    }
  }
}

static int set(ui_im_status_screen_t *stat_screen, ef_parser_t *parser, u_char *str) {
  u_char *seq;
  size_t len = strlen(str);

  if ((seq = alloca(27 + len))) {
    replace_char(str, '\n', ' ');
    memcpy(seq, "\x1b[2$~\x1b[1$}\x1b[?7l\x1b[2J\x1b[H", 22);
    memcpy(seq + 22, str, len);
    memcpy(seq + 22 + len, "\x1b[0$}", 5);

    vt_parser_write_loopback(stat_screen->vtparser, seq, 27 + len);
  }

  return 1;
}

/* --- global functions --- */

ui_im_status_screen_t *ui_im_status_screen_new(ui_display_t *disp, ui_font_manager_t *font_man,
                                               ui_color_manager_t *color_man, void *vtparser,
                                               int is_vertical, u_int line_height, int x, int y) {
  ui_im_status_screen_t *stat_screen;

  if ((stat_screen = calloc(1, sizeof(ui_im_status_screen_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

  stat_screen->font_man = font_man;
  stat_screen->color_man = color_man;
  stat_screen->vtparser = vtparser;

  stat_screen->x = x;
  stat_screen->y = y;
  stat_screen->line_height = line_height;

  stat_screen->is_vertical = is_vertical;

  /* methods of ui_im_status_screen_t */
  stat_screen->destroy = destroy;
  stat_screen->show = show;
  stat_screen->hide = hide;
  stat_screen->set_spot = set_spot;
  stat_screen->set = set;

  return stat_screen;
}

#endif /* USE_STATUS_LINE */

#else /* ! USE_IM_PLUGIN */

ui_im_status_screen_t *ui_im_status_screen_new(ui_display_t *disp, ui_font_manager_t *font_man,
                                               ui_color_manager_t *color_man, void *vtparser,
                                               int is_vertical, u_int line_height,
                                               int x, int y) {
  return NULL;
}

#endif
