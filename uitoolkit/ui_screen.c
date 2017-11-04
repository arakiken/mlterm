/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_screen.h"

#include <signal.h>
#include <stdio.h>  /* sprintf */
#include <unistd.h> /* fork/execvp */
#include <stdlib.h> /* abs */
#include <sys/stat.h>
#include <pobl/bl_mem.h> /* alloca */
#include <pobl/bl_debug.h>
#include <pobl/bl_str.h>     /* strdup, bl_snprintf */
#include <pobl/bl_util.h>    /* K_MIN */
#include <pobl/bl_def.h>     /* PATH_MAX */
#include <pobl/bl_args.h>    /* bl_arg_str_to_array */
#include <pobl/bl_conf_io.h> /* bl_get_user_rc_path */
#include <vt_str_parser.h>
#include <vt_term_manager.h>
#ifdef USE_BRLAPI
#include "ui_brltty.h"
#endif

#include "ui_xic.h"
#include "ui_draw_str.h"
#include "ui_selection_encoding.h"

#define HAS_SYSTEM_LISTENER(screen, function) \
  ((screen)->system_listener && (screen)->system_listener->function)

#define HAS_SCROLL_LISTENER(screen, function) \
  ((screen)->screen_scroll_listener && (screen)->screen_scroll_listener->function)

#define IS_LIBVTE(screen) (!(screen)->window.parent && (screen)->window.parent_window)

#define line_top_margin(screen) ((int)((screen)->line_space / 2))

#if 1
#define NL_TO_CR_IN_PAST_TEXT
#endif

#if 0
#define __DEBUG
#endif

/*
 * For ui_window_update()
 *
 * XXX
 * Note that vte.c calls ui_window_update( ... , 1), so if you change following
 *enums,
 * vte.c must be changed at the same time.
 */
enum {
  UPDATE_SCREEN = 0x1,
  UPDATE_CURSOR = 0x2,
  UPDATE_SCREEN_BLINK = 0x4,
};

/* --- static variables --- */

static int exit_backscroll_by_pty;
static int allow_change_shortcut;
static char *mod_meta_prefix = "\x1b";
static int trim_trailing_newline_in_pasting;
static ef_parser_t *vt_str_parser; /* XXX leaked */

#ifdef USE_IM_CURSOR_COLOR
static char *im_cursor_color = NULL;
#endif

/* --- static functions --- */

#ifdef USE_OT_LAYOUT
static void *ot_layout_get_ot_layout_font(vt_term_t *term, vt_font_t font) {
  ui_font_t *xfont;

  if (!term->screen->screen_listener ||
      !(xfont =
            ui_get_font(((ui_screen_t *)term->screen->screen_listener->self)->font_man, font)) ||
      !ui_font_has_ot_layout_table(xfont)) {
    return NULL;
  }

  return xfont;
}
#endif

static int convert_row_to_y(ui_screen_t *screen,
                            int row /* Should be 0 >= and <= vt_term_get_rows() */
                            ) {
  /*
   * !! Notice !!
   * assumption: line hight is always the same!
   */

  return ui_line_height(screen) * row;
}

/*
 * If y < 0 , return 0 with *y_rest = 0.
 * If y > screen->window.height , return screen->window.height / line_height
 * with *y_rest =
 * y - screen->window.height.
 */
static int convert_y_to_row(ui_screen_t *screen, u_int *y_rest, int y) {
  int row;

  if (y < 0) {
    y = 0;
  }

  /*
   * !! Notice !!
   * assumption: line hight is always the same!
   */

  if (y >= screen->height) {
    row = (screen->height - 1) / ui_line_height(screen);
  } else {
    row = y / ui_line_height(screen);
  }

  if (y_rest) {
    *y_rest = y - row *ui_line_height(screen);
  }

  return row;
}

static int convert_char_index_to_x(
    ui_screen_t *screen, vt_line_t *line,
    int char_index /* Should be 0 >= and <= vt_line_end_char_index() */
    ) {
  int count;
  int x;

  ui_font_manager_set_attr(screen->font_man, line->size_attr,
                           vt_line_has_ot_substitute_glyphs(line));

  if (vt_line_is_rtl(line)) {
    x = screen->width;

    for (count = vt_line_end_char_index(line); count >= char_index; count--) {
      vt_char_t *ch;

      ch = vt_char_at(line, count);

      if (vt_char_cols(ch) > 0) {
        x -= ui_calculate_mlchar_width(ui_get_font(screen->font_man, vt_char_font(ch)), ch, NULL);
      }
    }
  } else {
    /*
     * excluding the last char width.
     */
    x = 0;
    for (count = 0; count < char_index; count++) {
      vt_char_t *ch;

      ch = vt_char_at(line, count);

      if (vt_char_cols(ch) > 0) {
        x += ui_calculate_mlchar_width(ui_get_font(screen->font_man, vt_char_font(ch)), ch, NULL);
      }
    }
  }

  return x;
}

static int convert_char_index_to_x_with_shape(ui_screen_t *screen, vt_line_t *line,
                                               int char_index) {
  vt_line_t *orig;
  int x;

  orig = vt_line_shape(line);

  x = convert_char_index_to_x(screen, line, char_index);

  if (orig) {
    vt_line_unshape(line, orig);
  }

  return x;
}

/*
 * If x < 0, return 0 with *ui_rest = 0.
 * If x > screen->width, return screen->width / char_width with
 * *ui_rest = x - screen->width.
 */
static int convert_x_to_char_index(ui_screen_t *screen, vt_line_t *line, u_int *ui_rest, int x) {
  int count;
  u_int width;
  int end_char_index;

  ui_font_manager_set_attr(screen->font_man, line->size_attr,
                           vt_line_has_ot_substitute_glyphs(line));

  end_char_index = vt_line_end_char_index(line);

  if (vt_line_is_rtl(line)) {
    if (x > screen->width) {
      x = 0;
    } else {
      x = screen->width - x;
    }

    for (count = end_char_index; count > 0; count--) {
      vt_char_t *ch;

      ch = vt_char_at(line, count);

      if (vt_char_cols(ch) == 0) {
        continue;
      }

      width = ui_calculate_mlchar_width(ui_get_font(screen->font_man, vt_char_font(ch)), ch, NULL);

      if (x <= width) {
        break;
      }

      x -= width;
    }
  } else {
    if (x < 0) {
      x = 0;
    }

    for (count = 0; count < end_char_index; count++) {
      vt_char_t *ch;

      ch = vt_char_at(line, count);

      if (vt_char_cols(ch) == 0) {
        continue;
      }

      width = ui_calculate_mlchar_width(ui_get_font(screen->font_man, vt_char_font(ch)), ch, NULL);

      if (x < width) {
        break;
      }

      x -= width;
    }
  }

  if (ui_rest != NULL) {
    *ui_rest = x;
  }

  return count;
}

static int convert_x_to_char_index_with_shape(ui_screen_t *screen, vt_line_t *line, u_int *ui_rest,
                                              int x) {
  vt_line_t *orig;
  int char_index;

  orig = vt_line_shape(line);

  char_index = convert_x_to_char_index(screen, line, ui_rest, x);

  if (orig) {
    vt_line_unshape(line, orig);
  }

  return char_index;
}

static u_int screen_width(ui_screen_t *screen) {
  /*
   * logical cols/rows => visual width/height.
   */

  if (vt_term_get_vertical_mode(screen->term)) {
    return vt_term_get_logical_rows(screen->term) * ui_col_width(screen);
  } else {
    return vt_term_get_logical_cols(screen->term) * ui_col_width(screen) *
           screen->screen_width_ratio / 100;
  }
}

static u_int screen_height(ui_screen_t *screen) {
  /*
   * logical cols/rows => visual width/height.
   */

  if (vt_term_get_vertical_mode(screen->term)) {
    return vt_term_get_logical_cols(screen->term) * ui_line_height(screen) *
           screen->screen_width_ratio / 100;
  } else {
    return (vt_term_get_logical_rows(screen->term) +
             (vt_term_has_status_line(screen->term) ? 1 : 0)) * ui_line_height(screen);
  }
}

static int activate_xic(ui_screen_t *screen) {
  /*
   * FIXME: This function is a dirty wrapper on ui_xic_activate().
   */

  char *saved_ptr;
  char *xim_name;
  char *xim_locale;

  xim_name = xim_locale = NULL;

  saved_ptr = bl_str_sep(&screen->input_method, ":");
  xim_name = bl_str_sep(&screen->input_method, ":");
  xim_locale = bl_str_sep(&screen->input_method, ":");

  ui_xic_activate(&screen->window, xim_name ? xim_name : "", xim_locale ? xim_locale : "");

  if (xim_name) {
    *(xim_name - 1) = ':';
  }

  if (xim_locale) {
    *(xim_locale - 1) = ':';
  }

  screen->input_method = saved_ptr;

  return 1;
}

/*
 * drawing screen functions.
 */

static int draw_line(ui_screen_t *screen, vt_line_t *line, int y) {
  int beg_x;
  int ret;

  ret = 0;

  if (vt_line_is_empty(line)) {
    ui_window_clear(&screen->window, (beg_x = 0), y, screen->window.width, ui_line_height(screen));
    ret = 1;
  } else {
    int beg_char_index;
    u_int num_of_redrawn;
    int is_cleared_to_end;
    vt_line_t *orig;
    ui_font_present_t present;

    orig = vt_line_shape(line);

    present = ui_get_font_present(screen->font_man);

    if (vt_line_is_cleared_to_end(line) || (present & FONT_VAR_WIDTH)) {
      is_cleared_to_end = 1;
    } else {
      is_cleared_to_end = 0;
    }

    beg_char_index = vt_line_get_beg_of_modified(line);
    num_of_redrawn = vt_line_get_num_of_redrawn_chars(line, is_cleared_to_end);

    if ((present & FONT_VAR_WIDTH) && vt_line_is_rtl(line)) {
      num_of_redrawn += beg_char_index;
      beg_char_index = 0;
    }

    /* don't use _with_shape function since line is already shaped */
    beg_x = convert_char_index_to_x(screen, line, beg_char_index);

    ui_font_manager_set_attr(screen->font_man, line->size_attr,
                             vt_line_has_ot_substitute_glyphs(line));

    if (is_cleared_to_end) {
      if (vt_line_is_rtl(line)) {
        ui_window_clear(&screen->window, 0, y, beg_x, ui_line_height(screen));

        if (!ui_draw_str(&screen->window, screen->font_man, screen->color_man,
                         vt_char_at(line, beg_char_index), num_of_redrawn, beg_x, y,
                         ui_line_height(screen), ui_line_ascent(screen), line_top_margin(screen),
                         screen->hide_underline, screen->underline_offset)) {
          goto end;
        }
      } else {
        if (!ui_draw_str_to_eol(&screen->window, screen->font_man, screen->color_man,
                                vt_char_at(line, beg_char_index), num_of_redrawn, beg_x, y,
                                ui_line_height(screen), ui_line_ascent(screen),
                                line_top_margin(screen), screen->hide_underline,
                                screen->underline_offset)) {
          goto end;
        }
      }
    } else {
      if (!ui_draw_str(&screen->window, screen->font_man, screen->color_man,
                       vt_char_at(line, beg_char_index), num_of_redrawn, beg_x, y,
                       ui_line_height(screen), ui_line_ascent(screen), line_top_margin(screen),
                       screen->hide_underline, screen->underline_offset)) {
        goto end;
      }
    }

    ret = 1;

  end:
    if (orig) {
      vt_line_unshape(line, orig);
    }
  }

  return ret;
}

static int xterm_im_is_active(void *p);

/*
 * Don't call this function directly.
 * Call this function via highlight_cursor.
 */
static int draw_cursor(ui_screen_t *screen) {
  int row;
  int x;
  int y;
  vt_line_t *line;
  vt_line_t *orig;
  vt_char_t ch;
#ifdef USE_IM_CURSOR_COLOR
  char *orig_cursor_bg;
  int cursor_bg_is_replaced = 0;
#endif

  if (screen->is_preediting) {
    return 1;
  }

  if (!vt_term_is_visible_cursor(screen->term)) {
    return 1;
  }

  if ((row = vt_term_cursor_row_in_screen(screen->term)) == -1) {
    return 0;
  }

  y = convert_row_to_y(screen, row);

  if ((line = vt_term_get_cursor_line(screen->term)) == NULL || vt_line_is_empty(line)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " cursor line doesn't exist.\n");
#endif

    return 0;
  }

  orig = vt_line_shape(line);

  /* don't use _with_shape function since line is already shaped */
  x = convert_char_index_to_x(screen, line, vt_term_cursor_char_index(screen->term));

  /*
   * XXX
   * screen_width_ratio < 100 causes segfault on wayland, framebuffer and android without this.
   */
#if 1
  if (x + ui_col_width(screen) > screen->width || y + ui_line_height(screen) > screen->height) {
    /* XXX screen_width_ratio option drives out the cursor outside of the screen. */
    return 0;
  }
#endif

  vt_char_init(&ch);
  vt_char_copy(&ch, vt_char_at(line, vt_term_cursor_char_index(screen->term)));

  if (vt_term_get_cursor_style(screen->term) & (CS_UNDERLINE|CS_BAR)) {
    /* do nothing */
  } else if (screen->window.is_focused) {
#ifdef USE_IM_CURSOR_COLOR
    if (im_cursor_color && xterm_im_is_active(screen)) {
      if ((orig_cursor_bg = ui_color_manager_get_cursor_bg_color(screen->color_man))) {
        orig_cursor_bg = strdup(orig_cursor_bg);
      }

      ui_color_manager_set_cursor_bg_color(screen->color_man, im_cursor_color);
      cursor_bg_is_replaced = 1;
    }
#endif

    /* if fg/bg color should be overriden, reset ch's color to default */
    if (ui_color_manager_adjust_cursor_fg_color(screen->color_man)) {
      /* for curosr's bg */
      vt_char_set_bg_color(&ch, VT_BG_COLOR);
    }

    if (ui_color_manager_adjust_cursor_bg_color(screen->color_man)) {
      /* for cursor's fg */
      vt_char_set_fg_color(&ch, VT_FG_COLOR);
    }

    vt_char_reverse_color(&ch);
  }

  ui_font_manager_set_attr(screen->font_man, line->size_attr,
                           vt_line_has_ot_substitute_glyphs(line));

  ui_draw_str(&screen->window, screen->font_man, screen->color_man, &ch, 1, x, y,
              ui_line_height(screen), ui_line_ascent(screen), line_top_margin(screen),
              screen->hide_underline, screen->underline_offset);

  if (vt_term_get_cursor_style(screen->term) & CS_UNDERLINE) {
    ui_font_t *xfont = ui_get_font(screen->font_man, vt_char_font(&ch));
    ui_window_fill(&screen->window, x, y + ui_line_ascent(screen),
                   ui_calculate_mlchar_width(xfont, &ch, NULL), 2);
  } else if (vt_term_get_cursor_style(screen->term) & CS_BAR) {
    if (vt_term_cursor_is_rtl(screen->term)) {
      ui_font_t *xfont = ui_get_font(screen->font_man, vt_char_font(&ch));
      ui_window_fill(&screen->window, x + ui_calculate_mlchar_width(xfont, &ch, NULL) - 2,
                     y, 2, ui_line_height(screen));
    } else {
      ui_window_fill(&screen->window, x, y, 2, ui_line_height(screen));
    }
  } else if (screen->window.is_focused) {
    /* CS_BLOCK */
    ui_color_manager_adjust_cursor_fg_color(screen->color_man);
    ui_color_manager_adjust_cursor_bg_color(screen->color_man);

#ifdef USE_IM_CURSOR_COLOR
    if (cursor_bg_is_replaced) {
      ui_color_manager_set_cursor_bg_color(screen->color_man, orig_cursor_bg);
      free(orig_cursor_bg);
    }
#endif
  } else {
    /* CS_BLOCK */
    ui_font_t *xfont = ui_get_font(screen->font_man, vt_char_font(&ch));

    ui_window_set_fg_color(&screen->window,
                           ui_get_xcolor(screen->color_man, vt_char_fg_color(&ch)));

    ui_window_draw_rect_frame(&screen->window, x, y,
                              x + ui_calculate_mlchar_width(xfont, &ch, NULL) - 1,
                              y + ui_line_height(screen) - 1);
  }

  vt_char_final(&ch);

  if (orig) {
    vt_line_unshape(line, orig);
  }

  return 1;
}

static int flush_scroll_cache(ui_screen_t *screen, int scroll_actual_screen) {
  int scroll_cache_rows;
  int scroll_region_rows;

  if (!screen->scroll_cache_rows) {
    return 0;
  }

  /*
   * ui_window_scroll_*() can invoke window_exposed event internally,
   * and flush_scroll_cache() is called twice.
   * To avoid this, screen->scroll_cache_row is set 0 here before calling
   * ui_window_scroll_*().
   *
   * 1) Stop processing VT100 sequence.
   * 2) flush_scroll_cache() (ui_screen.c)
   * 3) scroll_region() (ui_window.c)
   *   - XCopyArea
   * 4) Start processing VT100 sequence.
   * 5) Stop processing VT100 sequence.
   * 6) ui_window_update() to redraw data modified by VT100 sequence.
   * 7) flush_scroll_cache()
   * 8) scroll_region()
   *   - XCopyArea
   *   - Wait and process GraphicsExpose caused by 3).
   * 9) flush_scroll_cache()
   * 10)scroll_region() <- avoid this by screen->scroll_cache_rows = 0.
   *   - XCopyArea
   */
  scroll_cache_rows = screen->scroll_cache_rows;
  screen->scroll_cache_rows = 0;

  if (scroll_cache_rows >= (scroll_region_rows = screen->scroll_cache_boundary_end -
                                                 screen->scroll_cache_boundary_start + 1)) {
    return 1;
  }

  if (scroll_actual_screen && ui_window_is_scrollable(&screen->window)) {
    if (!vt_term_get_vertical_mode(screen->term)) {
      int beg_y;
      int end_y;
      u_int scroll_height;

      scroll_height = ui_line_height(screen) * abs(scroll_cache_rows);

      /* scroll_height < screen->height is always true. */
      /* if (scroll_height < screen->height) */ {
        beg_y = convert_row_to_y(screen, screen->scroll_cache_boundary_start);
        end_y = beg_y + ui_line_height(screen) * scroll_region_rows;

        if (scroll_cache_rows > 0) {
          ui_window_scroll_upward_region(&screen->window, beg_y, end_y, scroll_height);
        } else {
          ui_window_scroll_downward_region(&screen->window, beg_y, end_y, scroll_height);
        }
      }
#if 0
      else {
        ui_window_clear_all(&screen->window);
      }
#endif
    } else {
      int beg_x;
      int end_x;
      u_int scroll_width;

      scroll_width = ui_col_width(screen) * abs(scroll_cache_rows);

      /* scroll_width < screen->width is always true. */
      /* if (scroll_width < screen->width) */ {
        beg_x = ui_col_width(screen) * screen->scroll_cache_boundary_start;
        end_x = beg_x + ui_col_width(screen) * scroll_region_rows;

        if (vt_term_get_vertical_mode(screen->term) & VERT_RTL) {
          end_x = screen->width - beg_x;
          beg_x = screen->width - end_x;
          scroll_cache_rows = -(scroll_cache_rows);
        }

        if (scroll_cache_rows > 0) {
          ui_window_scroll_leftward_region(&screen->window, beg_x, end_x, scroll_width);
        } else {
          ui_window_scroll_rightward_region(&screen->window, beg_x, end_x, scroll_width);
        }
      }
#if 0
      else {
        ui_window_clear_all(&screen->window);
      }
#endif
    }
  } else {
/*
 * setting modified mark to the lines within scroll region.
 *
 * XXX
 * Not regarding vertical mode.
 */

#if 0
    if (!vt_term_get_vertical_mode(screen->term)) {
    } else
#endif
    {
      if (scroll_cache_rows > 0) {
        /*
         * scrolling upward.
         */
        vt_term_set_modified_lines_in_screen(screen->term, screen->scroll_cache_boundary_start,
                                             screen->scroll_cache_boundary_end - scroll_cache_rows);
      } else {
        /*
         * scrolling downward.
         */
        vt_term_set_modified_lines_in_screen(
            screen->term, screen->scroll_cache_boundary_start + scroll_cache_rows,
            screen->scroll_cache_boundary_end);
      }
    }
  }

  return 1;
}

static void set_scroll_boundary(ui_screen_t *screen, int boundary_start, int boundary_end) {
  if (screen->scroll_cache_rows) {
    if (screen->scroll_cache_boundary_start != boundary_start ||
        screen->scroll_cache_boundary_end != boundary_end) {
      flush_scroll_cache(screen, 0);
    }
  }

  screen->scroll_cache_boundary_start = boundary_start;
  screen->scroll_cache_boundary_end = boundary_end;
}

/*
 * Don't call this function except from window_exposed or update_window.
 * Call this function via ui_window_update.
 */
static int redraw_screen(ui_screen_t *screen) {
  int count;
  vt_line_t *line;
  int y;
  int line_height;

#ifdef USE_BRLAPI
  ui_brltty_write();
#endif

  flush_scroll_cache(screen, 1);

  count = 0;
  while (1) {
    if ((line = vt_term_get_line_in_screen(screen->term, count)) == NULL) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " nothing is redrawn.\n");
#endif

      return 1;
    }

    if (vt_line_is_modified(line)) {
      break;
    }

    count++;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " redrawing -> line %d\n", count);
#endif

  y = convert_row_to_y(screen, count);

  draw_line(screen, line, y);

  count++;
  y += (line_height = ui_line_height(screen));

  while ((line = vt_term_get_line_in_screen(screen->term, count)) != NULL) {
    if (vt_line_is_modified(line)) {
#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " redrawing -> line %d\n", count);
#endif

      draw_line(screen, line, y);
    }
#ifdef __DEBUG
    else {
      bl_debug_printf(BL_DEBUG_TAG " not redrawing -> line %d\n", count);
    }
#endif

    y += line_height;
    count++;
  }

  vt_term_updated_all(screen->term);

  if (screen->im) {
    ui_im_redraw_preedit(screen->im, screen->window.is_focused);
  }

  return 1;
}

/*
 * Don't call this function except from window_exposed or update_window.
 * Call this function via ui_window_update.
 */
static int highlight_cursor(ui_screen_t *screen) {
  flush_scroll_cache(screen, 1);

  draw_cursor(screen);

  ui_xic_set_spot(&screen->window);

  return 1;
}

static int unhighlight_cursor(ui_screen_t *screen, int revert_visual) {
  return vt_term_unhighlight_cursor(screen->term, revert_visual);
}

/*
 * {enter|exit}_backscroll_mode() and bs_XXX() functions provides backscroll
 *operations.
 *
 * Similar processing to bs_XXX() is done in
 *ui_screen_scroll_{upward|downward|to}().
 */

static void enter_backscroll_mode(ui_screen_t *screen) {
  if (vt_term_is_backscrolling(screen->term)) {
    return;
  }

  vt_term_enter_backscroll_mode(screen->term);

  if (HAS_SCROLL_LISTENER(screen, bs_mode_entered)) {
    (*screen->screen_scroll_listener->bs_mode_entered)(screen->screen_scroll_listener->self);
  }
}

static void exit_backscroll_mode(ui_screen_t *screen) {
  if (!vt_term_is_backscrolling(screen->term)) {
    return;
  }

  vt_term_exit_backscroll_mode(screen->term);
  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

  if (HAS_SCROLL_LISTENER(screen, bs_mode_exited)) {
    (*screen->screen_scroll_listener->bs_mode_exited)(screen->screen_scroll_listener->self);
  }
}

static void bs_scroll_upward(ui_screen_t *screen) {
  if (vt_term_backscroll_upward(screen->term, 1)) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

    if (HAS_SCROLL_LISTENER(screen, scrolled_upward)) {
      (*screen->screen_scroll_listener->scrolled_upward)(screen->screen_scroll_listener->self, 1);
    }
  }
}

static void bs_scroll_downward(ui_screen_t *screen) {
  if (vt_term_backscroll_downward(screen->term, 1)) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

    if (HAS_SCROLL_LISTENER(screen, scrolled_downward)) {
      (*screen->screen_scroll_listener->scrolled_downward)(screen->screen_scroll_listener->self, 1);
    }
  }
}

static void bs_half_page_upward(ui_screen_t *screen) {
  if (vt_term_backscroll_upward(screen->term, vt_term_get_rows(screen->term) / 2)) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

    if (HAS_SCROLL_LISTENER(screen, scrolled_upward)) {
      /* XXX Not necessarily vt_term_get_rows( screen->term) / 2. */
      (*screen->screen_scroll_listener->scrolled_upward)(screen->screen_scroll_listener->self,
                                                         vt_term_get_rows(screen->term) / 2);
    }
  }
}

static void bs_half_page_downward(ui_screen_t *screen) {
  if (vt_term_backscroll_downward(screen->term, vt_term_get_rows(screen->term) / 2)) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

    if (HAS_SCROLL_LISTENER(screen, scrolled_downward)) {
      /* XXX Not necessarily vt_term_get_rows( screen->term) / 2. */
      (*screen->screen_scroll_listener->scrolled_downward)(screen->screen_scroll_listener->self,
                                                           vt_term_get_rows(screen->term) / 2);
    }
  }
}

static void bs_page_upward(ui_screen_t *screen) {
  if (vt_term_backscroll_upward(screen->term, vt_term_get_rows(screen->term))) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

    if (HAS_SCROLL_LISTENER(screen, scrolled_upward)) {
      /* XXX Not necessarily vt_term_get_rows( screen->term). */
      (*screen->screen_scroll_listener->scrolled_upward)(screen->screen_scroll_listener->self,
                                                         vt_term_get_rows(screen->term));
    }
  }
}

static void bs_page_downward(ui_screen_t *screen) {
  if (vt_term_backscroll_downward(screen->term, vt_term_get_rows(screen->term))) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

    if (HAS_SCROLL_LISTENER(screen, scrolled_downward)) {
      /* XXX Not necessarily vt_term_get_rows( screen->term). */
      (*screen->screen_scroll_listener->scrolled_downward)(screen->screen_scroll_listener->self,
                                                           vt_term_get_rows(screen->term));
    }
  }
}

/*
 * Utility function to execute both ui_restore_selected_region_color() and
 * ui_window_update().
 */
static void restore_selected_region_color_instantly(ui_screen_t *screen) {
  if (ui_restore_selected_region_color(&screen->sel)) {
    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
  }
}

static void write_to_pty_intern(vt_term_t *term, u_char *str, /* str may be NULL */
                                size_t len, ef_parser_t *parser /* parser may be NULL */) {
  if (parser && str) {
    (*parser->init)(parser);
    (*parser->set_str)(parser, str, len);
  }

  vt_term_init_encoding_conv(term);

  if (parser) {
    u_char conv_buf[512];
    size_t filled_len;

#ifdef __DEBUG
    {
      size_t i;

      bl_debug_printf(BL_DEBUG_TAG " written str:\n");
      for (i = 0; i < len; i++) {
        bl_msg_printf("[%.2x]", str[i]);
      }
      bl_msg_printf("=>\n");
    }
#endif

    while (!parser->is_eos) {
      if ((filled_len = vt_term_convert_to(term, conv_buf, sizeof(conv_buf), parser)) ==
          0) {
        break;
      }

#ifdef __DEBUG
      {
        size_t i;

        for (i = 0; i < filled_len; i++) {
          bl_msg_printf("[%.2x]", conv_buf[i]);
        }
      }
#endif

      vt_term_write(term, conv_buf, filled_len);
    }
  } else if (str) {
#ifdef __DEBUG
    {
      size_t i;

      bl_debug_printf(BL_DEBUG_TAG " written str: ");
      for (i = 0; i < len; i++) {
        bl_msg_printf("%.2x", str[i]);
      }
      bl_msg_printf("\n");
    }
#endif

    vt_term_write(term, str, len);
  } else {
    return;
  }
}

static void write_to_pty(ui_screen_t *screen, u_char *str, /* str may be NULL */
                         size_t len, ef_parser_t *parser /* parser may be NULL */) {
  if (vt_term_is_broadcasting(screen->term)) {
    vt_term_t **terms;
    u_int num = vt_get_all_terms(&terms);
    u_int count;

    for (count = 0; count < num; count++) {
      if (vt_term_is_broadcasting(terms[count])) {
        write_to_pty_intern(terms[count], str, len, parser);
      }
    }
  } else {
    write_to_pty_intern(screen->term, str, len, parser);
  }
}

static int write_special_key(ui_screen_t *screen, vt_special_key_t key,
                             int modcode, int is_numlock) {
  if (vt_term_is_broadcasting(screen->term)) {
    vt_term_t **terms;
    u_int num = vt_get_all_terms(&terms);
    u_int count;

    for (count = 0; count < num; count++) {
      if (vt_term_is_broadcasting(terms[count])) {
        if (!vt_term_write_special_key(terms[count], key, modcode, is_numlock)) {
          return 0;
        }
      }
    }

    return 1;
  } else {
    return vt_term_write_special_key(screen->term, key, modcode, is_numlock);
  }
}

static int set_wall_picture(ui_screen_t *screen) {
  ui_picture_t *pic;

  if (!screen->pic_file_path) {
    return 0;
  }

  if (!(pic = ui_acquire_bg_picture(&screen->window, ui_screen_get_picture_modifier(screen),
                                    screen->pic_file_path))) {
    bl_msg_printf("Wall picture file %s is not found.\n", screen->pic_file_path);

    free(screen->pic_file_path);
    screen->pic_file_path = NULL;

    ui_window_unset_wall_picture(&screen->window, 1);

    return 0;
  }

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
  if (screen->window.disp->depth == 4 && strstr(screen->pic_file_path, "six")) {
    /*
     * Color pallette of ui_display can be changed by ui_acquire_bg_picture().
     * (see ui_display_set_cmap() called from fb/ui_imagelib.c.)
     */
    ui_screen_reload_color_cache(screen, 1);
  }
#endif

  if (!ui_window_set_wall_picture(&screen->window, pic->pixmap, 1)) {
    ui_release_picture(pic);

    /* Because picture is loaded successfully, screen->pic_file_path retains. */

    return 0;
  }

  if (screen->bg_pic) {
    ui_release_picture(screen->bg_pic);
  }

  screen->bg_pic = pic;

  return 1;
}

static int set_icon(ui_screen_t *screen) {
  ui_icon_picture_t *icon;
  char *path;

  if ((path = vt_term_icon_path(screen->term))) {
    if (screen->icon && strcmp(path, screen->icon->file_path) == 0) {
      /* Not changed. */
      return 0;
    }

    if ((icon = ui_acquire_icon_picture(screen->window.disp, path))) {
      ui_window_set_icon(&screen->window, icon);
    } else {
      ui_window_remove_icon(&screen->window);
    }
  } else {
    if (screen->icon == NULL) {
      /* Not changed. */
      return 0;
    }

    icon = NULL;
    ui_window_remove_icon(&screen->window);
  }

  if (screen->icon) {
    ui_release_icon_picture(screen->icon);
  }

  screen->icon = icon;

  return 1;
}

/* referred in update_special_visual. */
static void change_font_present(ui_screen_t *screen, ui_type_engine_t type_engine,
                                ui_font_present_t font_present);

static int update_special_visual(ui_screen_t *screen) {
  ui_font_present_t font_present;

  if (!vt_term_update_special_visual(screen->term)) {
    /* If special visual is not changed, following processing is not necessary.
     */
    return 0;
  }

  font_present = ui_get_font_present(screen->font_man) & ~FONT_VERTICAL;

  /* Similar if-else conditions exist in vt_term_update_special_visual. */
  if (vt_term_get_vertical_mode(screen->term)) {
    font_present |= vt_term_get_vertical_mode(screen->term);
  }

  change_font_present(screen, ui_get_type_engine(screen->font_man), font_present);

  return 1;
}

static ui_im_t *im_new(ui_screen_t *screen) {
  return ui_im_new(screen->window.disp, screen->font_man, screen->color_man,
                   screen->term->parser, &screen->im_listener, screen->input_method,
                   screen->mod_ignore_mask);
}

/*
 * callbacks of ui_window events
 */

static void window_realized(ui_window_t *win) {
  ui_screen_t *screen;
  char *name;

  screen = (ui_screen_t *)win;

  ui_window_set_type_engine(win, ui_get_type_engine(screen->font_man));

  screen->mod_meta_mask = ui_window_get_mod_meta_mask(win, screen->mod_meta_key);
  screen->mod_ignore_mask = ui_window_get_mod_ignore_mask(win, NULL);

  if (screen->input_method) {
    /* XIM or other input methods? */
    if (strncmp(screen->input_method, "xim", 3) == 0) {
      activate_xic(screen);
    } else {
      ui_xic_activate(&screen->window, "unused", "");

      if (!(screen->im = im_new(screen))) {
        free(screen->input_method);
        screen->input_method = NULL;
      }
    }
  }

  ui_window_set_fg_color(win, ui_get_xcolor(screen->color_man, VT_FG_COLOR));
  ui_window_set_bg_color(win, ui_get_xcolor(screen->color_man, VT_BG_COLOR));

  if (HAS_SCROLL_LISTENER(screen, screen_color_changed)) {
    (*screen->screen_scroll_listener->screen_color_changed)(screen->screen_scroll_listener->self);
  }

  ui_get_xcolor_rgba(&screen->pic_mod.blend_red, &screen->pic_mod.blend_green,
                     &screen->pic_mod.blend_blue, NULL,
                     ui_get_xcolor(screen->color_man, VT_BG_COLOR));

  if ((name = vt_term_window_name(screen->term))) {
    ui_set_window_name(&screen->window, name);
  }

  if ((name = vt_term_icon_name(screen->term))) {
    ui_set_icon_name(&screen->window, name);
  }

  set_icon(screen);

  if (screen->borderless) {
    ui_window_set_borderless_flag(&screen->window, 1);
  }

/* XXX Don't load wall picture until window is resized */
#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
  if (screen->window.is_mapped)
#endif
  {
    set_wall_picture(screen);
  }
}

static void window_exposed(ui_window_t *win, int x, int y, u_int width, u_int height) {
  int beg_row;
  int end_row;
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (vt_term_get_vertical_mode(screen->term)) {
    u_int ncols;

    ncols = vt_term_get_cols(screen->term);

    if ((beg_row = x / ui_col_width(screen)) >= ncols) {
      beg_row = ncols - 1;
    }

    if ((end_row = (x + width) / ui_col_width(screen) + 1) >= ncols) {
      end_row = ncols - 1;
    }

    if (vt_term_get_vertical_mode(screen->term) & VERT_RTL) {
      u_int swp;

      swp = ncols - beg_row - 1;
      beg_row = ncols - end_row - 1;
      end_row = swp;
    }

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " exposed [row] from %d to %d [x] from %d to %d\n", beg_row,
                    end_row, x, x + width);
#endif

    vt_term_set_modified_lines_in_screen(screen->term, beg_row, end_row);
  } else {
    int row;
    vt_line_t *line;
    u_int col_width;

    col_width = ui_col_width(screen);

    beg_row = convert_y_to_row(screen, NULL, y);
    end_row = convert_y_to_row(screen, NULL, y + height);

#ifdef __DEBUG
    bl_debug_printf(BL_DEBUG_TAG " exposed [row] from %d to %d [y] from %d to %d\n", beg_row,
                    end_row, y, y + height);
#endif

    for (row = beg_row; row <= end_row; row++) {
      if ((line = vt_term_get_line_in_screen(screen->term, row))) {
        if (vt_line_is_rtl(line)) {
          vt_line_set_modified_all(line);
        } else {
          int beg;
          int end;
          u_int rest;

          /*
           * Don't add rest/col_width to beg because the
           * character at beg can be full-width.
           */
          beg = convert_x_to_char_index_with_shape(screen, line, &rest, x);

          end = convert_x_to_char_index_with_shape(screen, line, &rest, x + width);
          end += ((rest + col_width - 1) / col_width);

          vt_line_set_modified(line, beg, end);

#ifdef __DEBUG
          bl_debug_printf(BL_DEBUG_TAG " exposed line %d to %d [row %d]\n", beg, end, row);
#endif
        }
      }
    }
  }

  vt_term_select_drcs(screen->term);

  redraw_screen(screen);

  if (beg_row <= vt_term_cursor_row_in_screen(screen->term) &&
      vt_term_cursor_row_in_screen(screen->term) <= end_row) {
    highlight_cursor(screen);
  }
}

static void update_window(ui_window_t *win, int flag) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  vt_term_select_drcs(screen->term);

  if (flag & UPDATE_SCREEN) {
    redraw_screen(screen);
  } else if (flag & UPDATE_SCREEN_BLINK) {
    vt_set_blink_chars_visible(0);
    redraw_screen(screen);
    vt_set_blink_chars_visible(1);
  }

  if (flag & UPDATE_CURSOR) {
    highlight_cursor(screen);
  }
}

static void window_resized(ui_window_t *win) {
  ui_screen_t *screen;
  u_int rows;
  u_int cols;
  u_int width;
  u_int height;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " window is resized => width %d height %d.\n", win->width,
                  win->height);
#endif

  screen = (ui_screen_t *)win;

  /* This is necessary since vt_term_t size is changed. */
  ui_stop_selecting(&screen->sel);
  ui_restore_selected_region_color(&screen->sel);
  exit_backscroll_mode(screen);

  unhighlight_cursor(screen, 1);

  /*
   * visual width/height => logical cols/rows
   */

  if (vt_term_get_vertical_mode(screen->term)) {
    height = screen->window.width;
    width = (screen->window.height * 100) / screen->screen_width_ratio;

    rows = height / ui_col_width(screen);
    cols = width / ui_line_height(screen);
  } else {
    width = (screen->window.width * 100) / screen->screen_width_ratio;
    height = screen->window.height;

    cols = width / ui_col_width(screen);
    rows = height / ui_line_height(screen);
  }

  vt_term_resize(screen->term, cols, rows, width, height);

  screen->width = screen_width(screen);
  screen->height = screen_height(screen);

  set_wall_picture(screen);

  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

  ui_xic_resized(&screen->window);
}

static void window_focused(ui_window_t *win) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (screen->fade_ratio != 100) {
    if (ui_color_manager_unfade(screen->color_man)) {
      ui_window_set_fg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_FG_COLOR));
      ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR));

      vt_term_set_modified_all_lines_in_screen(screen->term);

      ui_window_update(&screen->window, UPDATE_SCREEN);
    }
  }

  ui_window_update(&screen->window, UPDATE_CURSOR);

  if (screen->im) {
    (*screen->im->focused)(screen->im);
  }

  if (vt_term_want_focus_event(screen->term)) {
    write_to_pty(screen, "\x1b[I", 3, NULL);
  }

#ifdef USE_BRLAPI
  ui_brltty_focus(screen->term);
#endif
}

static void window_unfocused(ui_window_t *win) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

/*
 * XXX
 * Unfocus event can be received in deleting window after screen->term was
 * deleted.
 */
#if 1
  if (!screen->term) {
    return;
  }
#endif

  if (screen->fade_ratio != 100) {
    if (ui_color_manager_fade(screen->color_man, screen->fade_ratio)) {
      ui_window_set_fg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_FG_COLOR));
      ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR));

      vt_term_set_modified_all_lines_in_screen(screen->term);

      ui_window_update(&screen->window, UPDATE_SCREEN);
    }
  }

  ui_window_update(&screen->window, UPDATE_CURSOR);

  if (screen->im) {
    (*screen->im->unfocused)(screen->im);
  }

  if (vt_term_want_focus_event(screen->term)) {
    write_to_pty(screen, "\x1b[O", 3, NULL);
  }
}

/*
 * the finalizer of ui_screen_t.
 *
 * ui_display_close or ui_display_remove_root -> ui_window_final ->
 *window_finalized
 */
static void window_finalized(ui_window_t *win) { ui_screen_delete((ui_screen_t *)win); }

static void window_deleted(ui_window_t *win) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (HAS_SYSTEM_LISTENER(screen, close_screen)) {
    (*screen->system_listener->close_screen)(screen->system_listener->self, screen, 1);
  }
}

static void mapping_notify(ui_window_t *win) {
  ui_screen_t *screen;
  screen = (ui_screen_t *)win;

  screen->mod_meta_mask = ui_window_get_mod_meta_mask(win, screen->mod_meta_key);
  screen->mod_ignore_mask = ui_window_get_mod_ignore_mask(win, NULL);
}

static size_t trim_trailing_newline_in_pasting1(u_char *str, size_t len) {
  if (trim_trailing_newline_in_pasting) {
    size_t count;

    for (count = len; count > 0; count--) {
      if (str[count - 1] == '\r' || str[count - 1] == '\n') {
        len--;
      } else {
        break;
      }
    }
  }

  return len;
}

static u_int trim_trailing_newline_in_pasting2(vt_char_t *str, u_int len) {
  if (trim_trailing_newline_in_pasting) {
    size_t count;

    for (count = len; count > 0; count--) {
      if (vt_char_code_is(&str[count - 1], '\r', US_ASCII) ||
          vt_char_code_is(&str[count - 1], '\n', US_ASCII)) {
        len--;
      } else {
        break;
      }
    }
  }

  return len;
}

#ifdef NL_TO_CR_IN_PAST_TEXT
static void convert_nl_to_cr1(u_char *str, size_t len) {
  size_t count;

  for (count = 0; count < len; count++) {
    if (str[count] == '\n') {
      str[count] = '\r';
    }
  }
}

static void convert_nl_to_cr2(vt_char_t *str, u_int len) {
  u_int count;

  for (count = 0; count < len; count++) {
    if (vt_char_code_is(&str[count], '\n', US_ASCII)) {
      vt_char_set_code(&str[count], '\r');
    }
  }
}
#endif

static int yank_event_received(ui_screen_t *screen, Time time) {
  if (ui_window_is_selection_owner(&screen->window)) {
    u_int len;

    if (screen->sel.sel_str == NULL || screen->sel.sel_len == 0) {
      return 0;
    }

    len = trim_trailing_newline_in_pasting2(screen->sel.sel_str, screen->sel.sel_len);
#ifdef NL_TO_CR_IN_PAST_TEXT
    /*
     * Convert normal newline chars to carriage return chars which are
     * common return key sequences.
     */
    convert_nl_to_cr2(screen->sel.sel_str, len);
#endif

    (*vt_str_parser->init)(vt_str_parser);
    vt_str_parser_set_str(vt_str_parser, screen->sel.sel_str, len);

    if (vt_term_is_bracketed_paste_mode(screen->term)) {
      write_to_pty(screen, "\x1b[200~", 6, NULL);
    }

    write_to_pty(screen, NULL, 0, vt_str_parser);

    if (vt_term_is_bracketed_paste_mode(screen->term)) {
      write_to_pty(screen, "\x1b[201~", 6, NULL);
    }

    return 1;
  } else {
    vt_char_encoding_t encoding;

    encoding = vt_term_get_encoding(screen->term);

    if (encoding == VT_UTF8 ||
        (IS_UCS_SUBSET_ENCODING(encoding) && screen->receive_string_via_ucs)) {
      return ui_window_utf_selection_request(&screen->window, time) ||
             ui_window_xct_selection_request(&screen->window, time);
    } else {
      return ui_window_xct_selection_request(&screen->window, time) ||
             ui_window_utf_selection_request(&screen->window, time);
    }
  }
}

static int receive_string_via_ucs(ui_screen_t *screen) {
  vt_char_encoding_t encoding;

  encoding = vt_term_get_encoding(screen->term);

  if (IS_UCS_SUBSET_ENCODING(encoding) && screen->receive_string_via_ucs) {
    return 1;
  } else {
    return 0;
  }
}

/* referred in shortcut_match */
static void change_im(ui_screen_t *, char *);
static void xterm_set_selection(void *, vt_char_t *, u_int, u_char *);

static int shortcut_match(ui_screen_t *screen, KeySym ksym, u_int state) {
  if (ui_shortcut_match(screen->shortcut, OPEN_SCREEN, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, open_screen)) {
      (*screen->system_listener->open_screen)(screen->system_listener->self, screen);
    }

    return 1;
  } else if (ui_shortcut_match(screen->shortcut, OPEN_PTY, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, open_pty)) {
      (*screen->system_listener->open_pty)(screen->system_listener->self, screen, NULL);
    }

    return 1;
  } else if (ui_shortcut_match(screen->shortcut, NEXT_PTY, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, next_pty)) {
      (*screen->system_listener->next_pty)(screen->system_listener->self, screen);
    }

    return 1;
  } else if (ui_shortcut_match(screen->shortcut, PREV_PTY, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, prev_pty)) {
      (*screen->system_listener->prev_pty)(screen->system_listener->self, screen);
    }

    return 1;
  } else if (ui_shortcut_match(screen->shortcut, VSPLIT_SCREEN, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, split_screen)) {
      (*screen->system_listener->split_screen)(screen->system_listener->self, screen, 0, NULL);
    }

    return 1;
  } else if (ui_shortcut_match(screen->shortcut, HSPLIT_SCREEN, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, split_screen)) {
      (*screen->system_listener->split_screen)(screen->system_listener->self, screen, 1, NULL);
    }

    return 1;
  } else if (ui_shortcut_match(screen->shortcut, NEXT_SCREEN, ksym, state) &&
             HAS_SYSTEM_LISTENER(screen, next_screen) &&
             (*screen->system_listener->next_screen)(screen->system_listener->self, screen)) {
    return 1;
  } else if (ui_shortcut_match(screen->shortcut, PREV_SCREEN, ksym, state) &&
             HAS_SYSTEM_LISTENER(screen, prev_screen) &&
             (*screen->system_listener->prev_screen)(screen->system_listener->self, screen)) {
    return 1;
  } else if (ui_shortcut_match(screen->shortcut, CLOSE_SCREEN, ksym, state) &&
             HAS_SYSTEM_LISTENER(screen, close_screen) &&
             (*screen->system_listener->close_screen)(screen->system_listener->self, screen, 0)) {
    return 1;
  } else if (ui_shortcut_match(screen->shortcut, HEXPAND_SCREEN, ksym, state) &&
             HAS_SYSTEM_LISTENER(screen, resize_screen) &&
             (*screen->system_listener->resize_screen)(screen->system_listener->self, screen, 1,
                                                       1)) {
    return 1;
  } else if (ui_shortcut_match(screen->shortcut, VEXPAND_SCREEN, ksym, state) &&
             HAS_SYSTEM_LISTENER(screen, resize_screen) &&
             (*screen->system_listener->resize_screen)(screen->system_listener->self, screen, 0,
                                                       1)) {
    return 1;
  }
  /* for backward compatibility */
  else if (ui_shortcut_match(screen->shortcut, EXT_KBD, ksym, state)) {
    change_im(screen, "kbd");

    return 1;
  }
#ifdef DEBUG
  else if (ui_shortcut_match(screen->shortcut, EXIT_PROGRAM, ksym, state)) {
    if (HAS_SYSTEM_LISTENER(screen, exit)) {
      (*screen->system_listener->exit)(screen->system_listener->self, 1);
    }

    return 1;
  }
#endif
#ifdef __DEBUG
  else if (ksym == XK_F10) {
    /* Performance benchmark */

    struct timeval tv;
    struct timeval tv2;
    vt_char_t *str;
    int count;
    int y;
    u_int height;
    u_int ascent;
    u_int top_margin;
    char ch;

    str = vt_str_alloca(0x5e);
    ch = ' ';
    for (count = 0; count < 0x5e; count++) {
      vt_char_set(str + count, ch, US_ASCII, 0, 0, VT_FG_COLOR, VT_BG_COLOR, 0, 0, 0, 0, 0, 0);
      ch++;
    }

    height = ui_line_height(screen);
    ascent = ui_line_ascent(screen);
    top_margin = line_top_margin(screen);

    gettimeofday(&tv, NULL);

    for (count = 0; count < 5; count++) {
      for (y = 0; y < screen->window.height - height; y += height) {
        ui_draw_str(&screen->window, screen->font_man, screen->color_man, str, 0x5e, 0, y, height,
                    ascent, top_margin, 0, 0);
      }

      ui_window_clear_all(&screen->window);
    }

    gettimeofday(&tv2, NULL);

    bl_debug_printf("Bench(draw) %d usec\n",
                    ((int)(tv2.tv_sec - tv.tv_sec)) * 1000000 + (int)(tv2.tv_usec - tv.tv_usec));

    count = 0;
    for (y = 0; y < screen->window.height - height; y += height) {
      ui_draw_str(&screen->window, screen->font_man, screen->color_man, str, 0x5e, 0, y, height,
                  ascent, top_margin, 0, 0);

      count++;
    }

    gettimeofday(&tv, NULL);

    while (count > 0) {
      ui_window_scroll_upward(&screen->window, height);
      count--;
      ui_window_clear(&screen->window, 0, height * count, screen->window.width, height);
    }

    gettimeofday(&tv2, NULL);

    bl_debug_printf("Bench(scroll) %d usec\n",
                    ((int)(tv2.tv_sec - tv.tv_sec)) * 1000000 + (int)(tv2.tv_usec - tv.tv_usec));

    return 1;
  }
#endif

  if (vt_term_is_backscrolling(screen->term)) {
    if (screen->use_extended_scroll_shortcut) {
      if (ui_shortcut_match(screen->shortcut, SCROLL_UP, ksym, state)) {
        bs_scroll_downward(screen);

        return 1;
      } else if (ui_shortcut_match(screen->shortcut, SCROLL_DOWN, ksym, state)) {
        bs_scroll_upward(screen);

        return 1;
      }
#if 1
      else if (ksym == 'u' || ksym == XK_Prior || ksym == XK_KP_Prior) {
        bs_half_page_downward(screen);

        return 1;
      } else if (ksym == 'd' || ksym == XK_Next || ksym == XK_KP_Next) {
        bs_half_page_upward(screen);

        return 1;
      } else if (ksym == 'k' || ksym == XK_Up || ksym == XK_KP_Up) {
        bs_scroll_downward(screen);

        return 1;
      } else if (ksym == 'j' || ksym == XK_Down || ksym == XK_KP_Down) {
        bs_scroll_upward(screen);

        return 1;
      }
#endif
    }

    if (ui_shortcut_match(screen->shortcut, PAGE_UP, ksym, state)) {
      bs_half_page_downward(screen);

      return 1;
    } else if (ui_shortcut_match(screen->shortcut, PAGE_DOWN, ksym, state)) {
      bs_half_page_upward(screen);

      return 1;
    } else if (ksym == XK_Shift_L || ksym == XK_Shift_R || ksym == XK_Control_L ||
               ksym == XK_Control_R || ksym == XK_Caps_Lock || ksym == XK_Shift_Lock ||
               ksym == XK_Meta_L || ksym == XK_Meta_R || ksym == XK_Alt_L || ksym == XK_Alt_R ||
               ksym == XK_Super_L || ksym == XK_Super_R || ksym == XK_Hyper_L ||
               ksym == XK_Hyper_R || ksym == XK_Escape) {
      /* any modifier keys(X11/keysymdefs.h) */

      return 1;
    } else if (ksym == 0) {
      /* button press -> reversed color is restored in button_press(). */
      return 0;
    } else {
      exit_backscroll_mode(screen);

      /* Continue processing */
    }
  }

  if (screen->use_extended_scroll_shortcut &&
      ui_shortcut_match(screen->shortcut, SCROLL_UP, ksym, state)) {
    enter_backscroll_mode(screen);
    bs_scroll_downward(screen);
  } else if (ui_shortcut_match(screen->shortcut, PAGE_UP, ksym, state)) {
    enter_backscroll_mode(screen);
    bs_half_page_downward(screen);
  } else if (ui_shortcut_match(screen->shortcut, PAGE_DOWN, ksym, state)) {
    /* do nothing */
  } else if (ui_shortcut_match(screen->shortcut, INSERT_SELECTION, ksym, state)) {
    yank_event_received(screen, CurrentTime);
  } else {
    return 0;
  }

  return 1;
}

#ifdef USE_QUARTZ
const char *cocoa_get_bundle_path(void);
#endif

static void start_menu(ui_screen_t *screen, char *str, int x, int y) {
  int global_x;
  int global_y;
  Window child;

  ui_window_translate_coordinates(&screen->window, x, y, &global_x, &global_y, &child);

  /*
   * XXX I don't know why but XGrabPointer() in child processes
   * fails without this.
   */
  ui_window_ungrab_pointer(&screen->window);

#ifdef USE_QUARTZ
  /* If program name was specified without directory, prepend LIBEXECDIR to it.
   */
  if (strchr(str, '/') == NULL) {
    char *new_str;
    const char *bundle_path;

    bundle_path = cocoa_get_bundle_path();
    if ((new_str = alloca(strlen(bundle_path) + 16 + strlen(str) + 1))) {
      sprintf(new_str, "%s/Contents/MacOS/%s", bundle_path, str);
      str = new_str;
    }
  }
#endif

  vt_term_start_config_menu(screen->term, str, global_x, global_y,
#ifdef USE_WAYLAND
                            NULL
#else
                            screen->window.disp->name
#endif
                            );
}

static int shortcut_str(ui_screen_t *screen, KeySym ksym, u_int state, int x, int y) {
  char *str;

  if (!(str = ui_shortcut_str(screen->shortcut, ksym, state))) {
    return 0;
  }

  if (strncmp(str, "menu:", 5) == 0) {
    start_menu(screen, str + 5, x, y);
  } else if (strncmp(str, "exesel:", 7) == 0) {
    size_t str_len;
    char *key;
    size_t key_len;

    str += 7;

    if (screen->sel.sel_str == NULL || screen->sel.sel_len == 0) {
      return 0;
    }

    str_len = strlen(str) + 1;

    key_len = str_len + screen->sel.sel_len * VTCHAR_UTF_MAX_SIZE + 1;
    key = alloca(key_len);

    strcpy(key, str);
    key[str_len - 1] = ' ';

    (*vt_str_parser->init)(vt_str_parser);
    vt_str_parser_set_str(vt_str_parser, screen->sel.sel_str, screen->sel.sel_len);

    vt_term_init_encoding_conv(screen->term);
    key_len =
        vt_term_convert_to(screen->term, key + str_len, key_len - str_len, vt_str_parser) +
        str_len;
    key[key_len] = '\0';

    if (strncmp(key, "mlclient", 8) == 0) {
      ui_screen_exec_cmd(screen, key);
    }
#ifndef USE_WIN32API
    else {
      char **argv;
      int argc;

      argv = bl_arg_str_to_array(&argc, key);

      if (fork() == 0) {
        /* child process */
        execvp(argv[0], argv);
        exit(1);
      }
    }
#endif
  } else if (strncmp(str, "proto:", 6) == 0) {
    char *seq;
    size_t len;

    str += 6;

    len = 7 + strlen(str) + 2;
    if ((seq = alloca(len))) {
      sprintf(seq, "\x1b]5379;%s\x07", str);
      /*
       * processing_vtseq == -1 means loopback processing of vtseq.
       * If processing_vtseq is -1, it is not set 1 in start_vt100_cmd()
       * which is called from vt_term_write_loopback().
       */
      screen->processing_vtseq = -1;
      vt_term_write_loopback(screen->term, seq, len - 1);
      ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
    }
  }
  /* XXX Hack for libvte */
  else if (IS_LIBVTE(screen) && ksym == 0 && state == Button3Mask && strcmp(str, "none") == 0) {
    /* do nothing */
  } else {
    write_to_pty(screen, str, strlen(str), NULL);
  }

  return 1;
}

/* referred in key_pressed. */
static int compare_key_state_with_modmap(void *p, u_int state, int *is_shift, int *is_lock,
                                         int *is_ctl, int *is_alt, int *is_meta, int *is_numlock,
                                         int *is_super, int *is_hyper);

typedef struct ksym_conv {
  KeySym before;
  KeySym after;

} ksym_conv_t;

static KeySym convert_ksym(KeySym ksym, ksym_conv_t *table, u_int table_size) {
  u_int count;

  for (count = 0; count < table_size; count++) {
    if (table[count].before == ksym) {
      return table[count].after;
    }
  }

  /* Not converted. */
  return ksym;
}

static void key_pressed(ui_window_t *win, XKeyEvent *event) {
  ui_screen_t *screen;
  size_t size;
  u_char ch[UTF_MAX_SIZE];
  u_char *kstr;
  KeySym ksym;
  ef_parser_t *parser;
  u_int masked_state;

  screen = (ui_screen_t *)win;

  masked_state = event->state & screen->mod_ignore_mask;

  if ((size = ui_window_get_str(win, ch, sizeof(ch), &parser, &ksym, event)) > sizeof(ch)) {
    if (!(kstr = alloca(size))) {
      return;
    }

    size = ui_window_get_str(win, kstr, size, &parser, &ksym, event);
  } else {
    kstr = ch;
  }

#ifdef USE_BRLAPI
  ui_brltty_speak_key(ksym, kstr, size);
#endif

#if 0
  bl_debug_printf("state %x %x ksym %x str ", event->state, masked_state, ksym);
  {
    size_t i;
    for (i = 0; i < size; i++) {
      bl_msg_printf("%c", kstr[i]);
    }
    bl_msg_printf(" hex ");
    for (i = 0; i < size; i++) {
      bl_msg_printf("%x", kstr[i]);
    }
    bl_msg_printf("\n");
  }
#endif

  if (screen->im) {
    u_char kchar = 0;

    if (ui_shortcut_match(screen->shortcut, IM_HOTKEY, ksym, masked_state) ||
        /* for backward compatibility */
        ui_shortcut_match(screen->shortcut, EXT_KBD, ksym, masked_state)) {
      if ((*screen->im->switch_mode)(screen->im)) {
        return;
      }
    }

    if (size == 1) {
      kchar = kstr[0];
    }
#if defined(USE_WIN32GUI) && defined(UTF16_IME_CHAR)
    else if (size == 2 && kstr[0] == 0) {
      /* UTF16BE */
      kchar = kstr[1];
    }
#endif

    if (!(*screen->im->key_event)(screen->im, kchar, ksym, event)) {
      if (vt_term_is_backscrolling(screen->term)) {
        exit_backscroll_mode(screen);
        ui_window_update(&screen->window, UPDATE_SCREEN);
      }

      return;
    }
  }

#ifdef __DEBUG
  {
    int i;

    bl_debug_printf(BL_DEBUG_TAG " received sequence =>");
    for (i = 0; i < size; i++) {
      bl_msg_printf("%.2x", kstr[i]);
    }
    bl_msg_printf("\n");
  }
#endif

  if (!shortcut_match(screen, ksym, masked_state) &&
      !shortcut_str(screen, ksym, masked_state, 0, 0)) {
    int modcode;
    vt_special_key_t spkey;
    int is_numlock;

    modcode = 0;
    spkey = -1;

    if (event->state) /* Check unmasked (raw) state of event. */
    {
      int is_shift;
      int is_meta;
      int is_alt;
      int is_ctl;

      if (compare_key_state_with_modmap(screen, event->state, &is_shift, NULL, &is_ctl, &is_alt,
                                        &is_meta, &is_numlock, NULL, NULL) &&
          /* compatible with xterm (Modifier codes in input.c) */
          (modcode =
               (is_shift ? 1 : 0) + (is_alt ? 2 : 0) + (is_ctl ? 4 : 0) + (is_meta ? 8 : 0))) {
        int key;

        modcode++;

        if ((key = ksym) < 0x80 ||
            /*
             * XK_BackSpace, XK_Tab and XK_Return are defined as
             * 0xffXX in keysymdef.h, while they are less than
             * 0x80 on win32, framebuffer and wayland.
             */
            (size == 1 && (key = kstr[0]) < 0x20)) {
          if (key == 0 && size > 0) {
            /* For win32gui */
            key = ef_bytes_to_int(kstr, size);
          }

          if (vt_term_write_modified_key(screen->term, key, modcode)) {
            return;
          }
        }
      }
    } else {
      is_numlock = 0;
    }

    if (screen->use_vertical_cursor) {
      if (vt_term_get_vertical_mode(screen->term) & VERT_RTL) {
        ksym_conv_t table[] = {
            {
             XK_Up, XK_Left,
            },
            {
             XK_KP_Up, XK_KP_Left,
            },
            {
             XK_Down, XK_Right,
            },
            {
             XK_KP_Down, XK_KP_Right,
            },
            {
             XK_Left, XK_Down,
            },
            {
             XK_KP_Left, XK_KP_Down,
            },
            {
             XK_Right, XK_Up,
            },
            {
             XK_KP_Right, XK_KP_Up,
            },
        };

        ksym = convert_ksym(ksym, table, sizeof(table) / sizeof(table[0]));
      } else if (vt_term_get_vertical_mode(screen->term) & VERT_LTR) {
        ksym_conv_t table[] = {
            {
             XK_Up, XK_Left,
            },
            {
             XK_KP_Up, XK_KP_Left,
            },
            {
             XK_Down, XK_Right,
            },
            {
             XK_KP_Down, XK_KP_Right,
            },
            {
             XK_Left, XK_Up,
            },
            {
             XK_KP_Left, XK_KP_Up,
            },
            {
             XK_Right, XK_Down,
            },
            {
             XK_KP_Right, XK_KP_Down,
            },
        };

        ksym = convert_ksym(ksym, table, sizeof(table) / sizeof(table[0]));
      }
    }

    if (IsKeypadKey(ksym)) {
      if (ksym == XK_KP_Multiply) {
        spkey = SPKEY_KP_MULTIPLY;
      } else if (ksym == XK_KP_Add) {
        spkey = SPKEY_KP_ADD;
      } else if (ksym == XK_KP_Separator) {
        spkey = SPKEY_KP_SEPARATOR;
      } else if (ksym == XK_KP_Subtract) {
        spkey = SPKEY_KP_SUBTRACT;
      } else if (ksym == XK_KP_Decimal || ksym == XK_KP_Delete) {
        spkey = SPKEY_KP_DELETE;
      } else if (ksym == XK_KP_Divide) {
        spkey = SPKEY_KP_DIVIDE;
      } else if (ksym == XK_KP_F1) {
        spkey = SPKEY_KP_F1;
      } else if (ksym == XK_KP_F2) {
        spkey = SPKEY_KP_F2;
      } else if (ksym == XK_KP_F3) {
        spkey = SPKEY_KP_F3;
      } else if (ksym == XK_KP_F4) {
        spkey = SPKEY_KP_F4;
      } else if (ksym == XK_KP_Insert) {
        spkey = SPKEY_KP_INSERT;
      } else if (ksym == XK_KP_End) {
        spkey = SPKEY_KP_END;
      } else if (ksym == XK_KP_Down) {
        spkey = SPKEY_KP_DOWN;
      } else if (ksym == XK_KP_Next) {
        spkey = SPKEY_KP_NEXT;
      } else if (ksym == XK_KP_Left) {
        spkey = SPKEY_KP_LEFT;
      } else if (ksym == XK_KP_Begin) {
        spkey = SPKEY_KP_BEGIN;
      } else if (ksym == XK_KP_Right) {
        spkey = SPKEY_KP_RIGHT;
      } else if (ksym == XK_KP_Home) {
        spkey = SPKEY_KP_HOME;
      } else if (ksym == XK_KP_Up) {
        spkey = SPKEY_KP_UP;
      } else if (ksym == XK_KP_Prior) {
        spkey = SPKEY_KP_PRIOR;
      } else {
        goto no_keypad;
      }

      goto write_buf;
    }

  no_keypad:
    if ((ksym == XK_Delete
#ifdef USE_XLIB
                && size == 1
#endif
                ) ||
               ksym == XK_KP_Delete) {
      spkey = SPKEY_DELETE;
    }
    /*
     * XXX
     * In some environment, if backspace(1) -> 0-9 or space(2) pressed
     * continuously,
     * ksym in (2) as well as (1) is XK_BackSpace.
     */
    else if (ksym == XK_BackSpace && size == 1 && kstr[0] == 0x8) {
      spkey = SPKEY_BACKSPACE;
    } else if (ksym == XK_Escape) {
      spkey = SPKEY_ESCAPE;
    } else if (size > 0) {
      /* do nothing */
    }
/*
 * following ksym is processed only if no key string is received
 * (size == 0)
 */
#if 1
    else if (ksym == XK_Pause) {
      if (modcode == 0) {
        vt_term_reset_pending_vt100_sequence(screen->term);
      }
    }
#endif
    else if (ksym == XK_Up) {
      spkey = SPKEY_UP;
    } else if (ksym == XK_Down) {
      spkey = SPKEY_DOWN;
    } else if (ksym == XK_Right) {
      spkey = SPKEY_RIGHT;
    } else if (ksym == XK_Left) {
      spkey = SPKEY_LEFT;
    } else if (ksym == XK_Begin) {
      spkey = SPKEY_BEGIN;
    } else if (ksym == XK_End) {
      spkey = SPKEY_END;
    } else if (ksym == XK_Home) {
      spkey = SPKEY_HOME;
    } else if (ksym == XK_Prior) {
      spkey = SPKEY_PRIOR;
    } else if (ksym == XK_Next) {
      spkey = SPKEY_NEXT;
    } else if (ksym == XK_Insert) {
      spkey = SPKEY_INSERT;
    } else if (ksym == XK_Find) {
      spkey = SPKEY_FIND;
    } else if (ksym == XK_Execute) {
      spkey = SPKEY_EXECUTE;
    } else if (ksym == XK_Select) {
      spkey = SPKEY_SELECT;
    } else if (ksym == XK_ISO_Left_Tab) {
      spkey = SPKEY_ISO_LEFT_TAB;
    } else if (ksym == XK_F15 || ksym == XK_Help) {
      spkey = SPKEY_F15;
    } else if (ksym == XK_F16 || ksym == XK_Menu) {
      spkey = SPKEY_F16;
    } else if (XK_F1 <= ksym && ksym <= XK_FMAX) {
      spkey = SPKEY_F1 + ksym - XK_F1;
    }
#ifdef SunXK_F36
    else if (ksym == SunXK_F36) {
      spkey = SPKEY_F36;
    } else if (ksym == SunXK_F37) {
      spkey = SPKEY_F37;
    }
#endif
    else {
      return;
    }

  write_buf:
    /* Check unmasked (raw) state of event. */
    if (screen->mod_meta_mask & event->state) {
      if (screen->mod_meta_mode == MOD_META_OUTPUT_ESC) {
        write_to_pty(screen, mod_meta_prefix, strlen(mod_meta_prefix), NULL);
      } else if (screen->mod_meta_mode == MOD_META_SET_MSB) {
        size_t count;

        if (!IS_8BIT_ENCODING(vt_term_get_encoding(screen->term))) {
          static ef_parser_t *key_parser; /* XXX leaked */

          if (!key_parser) {
            key_parser = vt_char_encoding_parser_new(VT_ISO8859_1);
          }

          parser = key_parser;
        }

#if defined(USE_WIN32GUI) && defined(UTF16_IME_CHAR)
        size /= 2;
        for (count = 0; count < size; count++) {
        /* UTF16BE => 8bit */
          if (0x20 <= kstr[count*2 + 1] && kstr[count*2 + 1] <= 0x7e) {
            kstr[count] = kstr[count*2] | 0x80;
          }
        }
#else
        for (count = 0; count < size; count++) {
          if (0x20 <= kstr[count] && kstr[count] <= 0x7e) {
            kstr[count] |= 0x80;
          }
        }
#endif
      }
    }

    if (spkey != -1 && write_special_key(screen, spkey, modcode, is_numlock)) {
      return;
    }

    if (size > 0) {
      ef_conv_t *utf_conv;

      if (parser && receive_string_via_ucs(screen) && (utf_conv = ui_get_selection_conv(1))) {
        /* XIM Text -> UCS -> PTY ENCODING */

        u_char conv_buf[512];
        size_t filled_len;

        (*parser->init)(parser);
        (*parser->set_str)(parser, kstr, size);

        (*utf_conv->init)(utf_conv);

        while (!parser->is_eos) {
          if ((filled_len = (*utf_conv->convert)(utf_conv, conv_buf,
                                                 sizeof(conv_buf), parser)) == 0) {
            break;
          }

          write_to_pty(screen, conv_buf, filled_len, ui_get_selection_parser(1));
        }
      } else {
        write_to_pty(screen, kstr, size, parser);
      }
    }
  }
}

static void selection_cleared(ui_window_t *win) {
  if (ui_sel_clear(&((ui_screen_t *)win)->sel)) {
    ui_window_update(win, UPDATE_SCREEN | UPDATE_CURSOR);
  }
}

static size_t convert_selection_to_xct(ui_screen_t *screen, u_char *str, size_t len) {
  size_t filled_len;
  ef_conv_t *xct_conv;

#ifdef __DEBUG
  {
    int i;

    bl_debug_printf(BL_DEBUG_TAG " sending internal str: ");
    for (i = 0; i < screen->sel.sel_len; i++) {
      vt_char_dump(&screen->sel.sel_str[i]);
    }
    bl_msg_printf("\n -> converting to ->\n");
  }
#endif

  if (!(xct_conv = ui_get_selection_conv(0))) {
    return 0;
  }

  (*vt_str_parser->init)(vt_str_parser);
  vt_str_parser_set_str(vt_str_parser, screen->sel.sel_str, screen->sel.sel_len);

  (*xct_conv->init)(xct_conv);
  filled_len = (*xct_conv->convert)(xct_conv, str, len, vt_str_parser);

#ifdef __DEBUG
  {
    int i;

    bl_debug_printf(BL_DEBUG_TAG " sending xct str: ");
    for (i = 0; i < filled_len; i++) {
      bl_msg_printf("%.2x", str[i]);
    }
    bl_msg_printf("\n");
  }
#endif

  return filled_len;
}

static size_t convert_selection_to_utf(ui_screen_t *screen, u_char *str, size_t len) {
  size_t filled_len;
  ef_conv_t *utf_conv;

#ifdef __DEBUG
  {
    int i;

    bl_debug_printf(BL_DEBUG_TAG " sending internal str: ");
    for (i = 0; i < screen->sel.sel_len; i++) {
      vt_char_dump(&screen->sel.sel_str[i]);
    }
    bl_msg_printf("\n -> converting to ->\n");
  }
#endif

  if (!(utf_conv = ui_get_selection_conv(1))) {
    return 0;
  }

  (*vt_str_parser->init)(vt_str_parser);
  vt_str_parser_set_str(vt_str_parser, screen->sel.sel_str, screen->sel.sel_len);

  (*utf_conv->init)(utf_conv);
  filled_len = (*utf_conv->convert)(utf_conv, str, len, vt_str_parser);

#ifdef __DEBUG
  {
    int i;

    bl_debug_printf(BL_DEBUG_TAG " sending utf str: ");
    for (i = 0; i < filled_len; i++) {
      bl_msg_printf("%.2x", str[i]);
    }
    bl_msg_printf("\n");
  }
#endif

  return filled_len;
}

static void xct_selection_requested(ui_window_t *win, XSelectionRequestEvent *event, Atom type) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (screen->sel.sel_str == NULL || screen->sel.sel_len == 0) {
    ui_window_send_text_selection(win, event, NULL, 0, 0);
  } else {
    u_char *xct_str;
    size_t xct_len;
    size_t filled_len;

    xct_len = screen->sel.sel_len * VTCHAR_XCT_MAX_SIZE;

    /*
     * Don't use alloca() here because len can be too big value.
     * (VTCHAR_XCT_MAX_SIZE defined in vt_char.h is 160 byte.)
     */
    if ((xct_str = malloc(xct_len)) == NULL) {
      return;
    }

    filled_len = convert_selection_to_xct(screen, xct_str, xct_len);

    ui_window_send_text_selection(win, event, xct_str, filled_len, type);

    free(xct_str);
  }
}

static void utf_selection_requested(ui_window_t *win, XSelectionRequestEvent *event, Atom type) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (screen->sel.sel_str == NULL || screen->sel.sel_len == 0) {
    ui_window_send_text_selection(win, event, NULL, 0, 0);
  } else {
    u_char *utf_str;
    size_t utf_len;
    size_t filled_len;

    utf_len = screen->sel.sel_len * VTCHAR_UTF_MAX_SIZE;

    /*
     * Don't use alloca() here because len can be too big value.
     * (VTCHAR_UTF_MAX_SIZE defined in vt_char.h is 48 byte.)
     */
    if ((utf_str = malloc(utf_len)) == NULL) {
      return;
    }

    filled_len = convert_selection_to_utf(screen, utf_str, utf_len);

    ui_window_send_text_selection(win, event, utf_str, filled_len, type);

    free(utf_str);
  }
}

static void xct_selection_notified(ui_window_t *win, u_char *str, size_t len) {
  ui_screen_t *screen;
  ef_conv_t *utf_conv;
  ef_parser_t *xct_parser;

  if (!(xct_parser = ui_get_selection_parser(0))) {
    return;
  }

  len = trim_trailing_newline_in_pasting1(str, len);
#ifdef NL_TO_CR_IN_PAST_TEXT
  /*
   * Convert normal newline chars to carriage return chars which are
   * common return key sequences.
   */
  convert_nl_to_cr1(str, len);
#endif

  screen = (ui_screen_t *)win;

  if (vt_term_is_bracketed_paste_mode(screen->term)) {
    write_to_pty(screen, "\x1b[200~", 6, NULL);
  }

#ifdef USE_XLIB
  /*
   * XXX
   * parsing UTF-8 sequence designated by ESC % G.
   * It is on xlib alone that it might be received.
   */
  if (len > 3 && strncmp(str, "\x1b%G", 3) == 0) {
#if 0
    int i;
    for (i = 0; i < len; i++) {
      bl_msg_printf("%.2x ", str[i]);
    }
#endif

    write_to_pty(screen, str + 3, len - 3, ui_get_selection_parser(1));
  } else
#endif
  if (receive_string_via_ucs(screen) && (utf_conv = ui_get_selection_conv(1))) {
    /* XCOMPOUND TEXT -> UCS -> PTY ENCODING */

    u_char conv_buf[512];
    size_t filled_len;

    (*xct_parser->init)(xct_parser);
    (*xct_parser->set_str)(xct_parser, str, len);

    (*utf_conv->init)(utf_conv);

    while (!xct_parser->is_eos) {
      if ((filled_len = (*utf_conv->convert)(utf_conv, conv_buf, sizeof(conv_buf),
                                             xct_parser)) == 0) {
        break;
      }

      write_to_pty(screen, conv_buf, filled_len, ui_get_selection_parser(1));
    }
  } else {
    /* XCOMPOUND TEXT -> PTY ENCODING */

    write_to_pty(screen, str, len, xct_parser);
  }

  if (vt_term_is_bracketed_paste_mode(screen->term)) {
    write_to_pty(screen, "\x1b[201~", 6, NULL);
  }
}

static void utf_selection_notified(ui_window_t *win, u_char *str, size_t len) {
  ui_screen_t *screen;

  len = trim_trailing_newline_in_pasting1(str, len);
#ifdef NL_TO_CR_IN_PAST_TEXT
  /*
   * Convert normal newline chars to carriage return chars which are
   * common return key sequences.
   */
  convert_nl_to_cr1(str, len);
#endif

  screen = (ui_screen_t *)win;

  if (vt_term_is_bracketed_paste_mode(screen->term)) {
    write_to_pty(screen, "\x1b[200~", 6, NULL);
  }

  write_to_pty(screen, str, len, ui_get_selection_parser(1));

  if (vt_term_is_bracketed_paste_mode(screen->term)) {
    write_to_pty(screen, "\x1b[201~", 6, NULL);
  }
}

#ifndef DISABLE_XDND
static void set_xdnd_config(ui_window_t *win, char *dev, char *key, char *value) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (strcmp(key, "scp") == 0) {
    if (vt_term_get_slave_fd(screen->term) == -1) /* connecting to remote host. */
    {
      /* value is always UTF-8 */
      vt_term_scp(screen->term, ".", value, VT_UTF8);
    }
  } else {
    ui_screen_set_config(screen, dev, key, value);

    ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
  }
}
#endif

static void report_mouse_tracking(ui_screen_t *screen, int x, int y, int button, int state,
                                  int is_motion,
                                  int is_released /* is_released is 0 if PointerMotion */
                                  ) {
  int key_state;
  int button_state;
  vt_line_t *line;
  int col;
  int row;
  u_int ui_rest;

  if (/* is_motion && */ button == 0) {
    /* PointerMotion */
    key_state = button_state = 0;
  } else {
    /*
     * Shift = 4
     * Meta = 8
     * Control = 16
     * Button Motion = 32
     *
     * NOTE: with Ctrl/Shift, the click is interpreted as region selection at
     *present.
     * So Ctrl/Shift will never be catched here.
     */
    key_state = ((state & ShiftMask) ? 4 : 0) + ((state & screen->mod_meta_mask) ? 8 : 0) +
                ((state & ControlMask) ? 16 : 0) +
                (is_motion /* && (state & (Button1Mask|Button2Mask|Button3Mask)) */
                     ? 32
                     : 0);

    /* for LOCATOR_REPORT */
    button_state = (1 << (button - Button1));
    if (state & Button1Mask) {
      button_state |= 1;
    }
    if (state & Button2Mask) {
      button_state |= 2;
    }
    if (state & Button3Mask) {
      button_state |= 4;
    }
  }

  if (vt_term_get_mouse_report_mode(screen->term) == LOCATOR_PIXEL_REPORT) {
    screen->prev_mouse_report_col = x + 1;
    screen->prev_mouse_report_row = y + 1;

    vt_term_report_mouse_tracking(screen->term, x + 1, y + 1, button, is_released, key_state,
                                  button_state);

    return;
  }

  if (vt_term_get_vertical_mode(screen->term)) {
    col = convert_y_to_row(screen, NULL, y);

    if ((line = vt_term_get_line_in_screen(screen->term, col)) == NULL) {
      return;
    }

    if (vt_term_is_using_multi_col_char(screen->term)) {
      row = x / ui_col_width(screen);
    } else {
      row = vt_convert_char_index_to_col(line,
              convert_x_to_char_index_with_shape(screen, line, &ui_rest, x), 0);
    }

    if (vt_term_is_using_multi_col_char(screen->term)) {
      int count;

      for (count = col; count >= 0; count--) {
        if ((line = vt_term_get_line_in_screen(screen->term, count)) &&
            row < line->num_of_filled_chars && vt_char_cols(&line->chars[row]) > 1) {
          col++;
        }
      }
    }

    if (vt_term_get_vertical_mode(screen->term) & VERT_RTL) {
      row = vt_term_get_cols(screen->term) - row - 1;
    }
  } else {
    u_int width;
    int char_index;

    row = convert_y_to_row(screen, NULL, y);

    if ((line = vt_term_get_line_in_screen(screen->term, row)) == NULL) {
      return;
    }

    char_index = convert_x_to_char_index_with_shape(screen, line, &ui_rest, x);
    if (vt_line_is_rtl(line)) {
      /* XXX */
      char_index = vt_line_convert_visual_char_index_to_logical(line, char_index);
    }

    col = vt_convert_char_index_to_col(line, char_index, 0);

    ui_font_manager_set_attr(screen->font_man, line->size_attr,
                             vt_line_has_ot_substitute_glyphs(line));
    width = ui_calculate_char_width(ui_get_font(screen->font_man, vt_char_font(vt_sp_ch())),
                                    vt_char_code(vt_sp_ch()), US_ASCII, NULL);
    if (ui_rest > width) {
      if ((col += ui_rest / width) >= vt_term_get_cols(screen->term)) {
        col = vt_term_get_cols(screen->term) - 1;
      }
    }
  }

  /* count starts from 1, not 0 */
  col++;
  row++;

  if (is_motion && button <= Button3 && /* not wheel mouse */
      screen->prev_mouse_report_col == col && screen->prev_mouse_report_row == row) {
    /* pointer is not moved. */
    return;
  }

  vt_term_report_mouse_tracking(screen->term, col, row, button, is_released, key_state,
                                button_state);

  screen->prev_mouse_report_col = col;
  screen->prev_mouse_report_row = row;
}

/*
 * Functions related to selection.
 */

static void start_selection(ui_screen_t *screen, int col_r, int row_r, ui_sel_type_t type,
                            int is_rect) {
  int col_l;
  int row_l;
  vt_line_t *line;

  /* XXX */
  if (vt_term_get_vertical_mode(screen->term)) {
    bl_msg_printf("Not supported selection in vertical mode.\n");

    return;
  }

  if ((line = vt_term_get_line(screen->term, row_r)) == NULL) {
    return;
  }

  if (is_rect) {
    if (col_r == 0 || abs(col_r) + 1 == vt_term_get_cols(screen->term)) {
      col_l = col_r;
    } else {
      col_l = col_r - 1;
    }

    row_l = row_r;
  } else if ((!vt_line_is_rtl(line) && col_r == 0) ||
             (vt_line_is_rtl(line) && abs(col_r) == vt_line_end_char_index(line))) {
    if ((line = vt_term_get_line(screen->term, row_r - 1)) == NULL || vt_line_is_empty(line)) {
      /* XXX col_l can be underflowed, but anyway it works. */
      col_l = col_r - 1;
      row_l = row_r;
    } else {
      if (vt_line_is_rtl(line)) {
        col_l = 0;
      } else {
        col_l = vt_line_end_char_index(line);
      }
      row_l = row_r - 1;
    }
  } else {
    col_l = col_r - 1;
    row_l = row_r;
  }

  if (ui_start_selection(&screen->sel, col_l, row_l, col_r, row_r, type, is_rect)) {
    ui_window_update(&screen->window, UPDATE_SCREEN);
  }
}

static void selecting(ui_screen_t *screen, int char_index, int row) {
  /* XXX */
  if (vt_term_get_vertical_mode(screen->term)) {
    bl_msg_printf("Not supported selection in vertical mode.\n");

    return;
  }

  if (ui_selected_region_is_changed(&screen->sel, char_index, row, 1) &&
      ui_selecting(&screen->sel, char_index, row)) {
    ui_window_update(&screen->window, UPDATE_SCREEN);
  }
}

static void selecting_with_motion(ui_screen_t *screen, int x, int y, Time time, int is_rect) {
  int char_index;
  int row;
  int ui_is_outside;
  u_int ui_rest;
  vt_line_t *line;

  if (x < 0) {
    x = 0;
    ui_is_outside = 1;
  } else if (x > screen->width) {
    x = screen->width;
    ui_is_outside = 1;
  } else {
    ui_is_outside = 0;
  }

  if (y < 0) {
    if (vt_term_get_num_of_logged_lines(screen->term) > 0) {
      if (!vt_term_is_backscrolling(screen->term)) {
        enter_backscroll_mode(screen);
      }

      bs_scroll_downward(screen);
    }

    y = 0;
  } else if (y > screen->height) {
    if (vt_term_is_backscrolling(screen->term)) {
      bs_scroll_upward(screen);
    }

    y = screen->height - ui_line_height(screen);
  }

  row = vt_term_convert_scr_row_to_abs(screen->term, convert_y_to_row(screen, NULL, y));

  if ((line = vt_term_get_line(screen->term, row)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " line(%d) not found.\n", row);
#endif

    return;
  }

  char_index = convert_x_to_char_index_with_shape(screen, line, &ui_rest, x);

  if (is_rect || screen->sel.is_rect) {
    /* converting char index to col. */

    char_index = vt_convert_char_index_to_col(line, char_index, 0);

    if (vt_line_is_rtl(line)) {
      char_index += (vt_term_get_cols(screen->term) - vt_line_get_num_of_filled_cols(line));
      char_index -= (ui_rest / ui_col_width(screen));
    } else {
      char_index += (ui_rest / ui_col_width(screen));
    }
  } else if (char_index == vt_line_end_char_index(line) && ui_rest > 0) {
    ui_is_outside = 1;

    /* Inform vt_screen that the mouse position is outside of the line. */
    char_index++;
  }

  if (vt_line_is_rtl(line)) {
    char_index = -char_index;
  }

  if (!ui_is_selecting(&screen->sel)) {
    restore_selected_region_color_instantly(screen);
    start_selection(screen, char_index, row, SEL_CHAR, is_rect);
  } else {
    if (!ui_is_outside) {
      if (ui_is_after_sel_right_base_pos(&screen->sel, char_index, row)) {
        if (abs(char_index) > 0) {
          char_index--;
        }
      }
#if 0
      else if (ui_is_before_sel_left_base_pos(&screen->sel, char_index, row)) {
        if (abs(char_index) < vt_line_end_char_index(line)) {
          char_index++;
        }
      }
#endif
    }

    selecting(screen, char_index, row);
  }
}

static int selecting_picture(ui_screen_t *screen, int char_index, int row) {
  vt_line_t *line;
  vt_char_t *ch;
  ui_inline_picture_t *pic;

  if (!(line = vt_term_get_line(screen->term, row)) || vt_line_is_empty(line) ||
      !(ch = vt_char_at(line, char_index)) || !(ch = vt_get_picture_char(ch)) ||
      !(pic = ui_get_inline_picture(vt_char_picture_id(ch)))) {
    return 0;
  }

  ui_window_send_picture_selection(&screen->window, pic->pixmap, pic->width, pic->height);

  return 1;
}

static void selecting_word(ui_screen_t *screen, int x, int y, Time time) {
  int char_index;
  int row;
  u_int ui_rest;
  int beg_row;
  int beg_char_index;
  int end_row;
  int end_char_index;
  vt_line_t *line;

  row = vt_term_convert_scr_row_to_abs(screen->term, convert_y_to_row(screen, NULL, y));

  if ((line = vt_term_get_line(screen->term, row)) == NULL || vt_line_is_empty(line)) {
    return;
  }

  char_index = convert_x_to_char_index_with_shape(screen, line, &ui_rest, x);

  if (vt_line_end_char_index(line) == char_index && ui_rest > 0) {
    /* over end of line */

    return;
  }

  if (selecting_picture(screen, char_index, row)) {
    return;
  }

  if (vt_term_get_word_region(screen->term, &beg_char_index, &beg_row, &end_char_index, &end_row,
                              char_index, row) == 0) {
    return;
  }

  if (vt_line_is_rtl(vt_term_get_line(screen->term, beg_row))) {
    if (ui_is_selecting(&screen->sel)) {
      beg_char_index = -beg_char_index;
    } else {
      beg_char_index = -beg_char_index + 1;
    }
  }

  if (vt_line_is_rtl(vt_term_get_line(screen->term, end_row))) {
    end_char_index = -end_char_index;
  }

  if (!ui_is_selecting(&screen->sel)) {
    restore_selected_region_color_instantly(screen);
    start_selection(screen, beg_char_index, beg_row, SEL_WORD, 0);
    selecting(screen, end_char_index, end_row);
    ui_sel_lock(&screen->sel);
  } else {
    if (beg_row == end_row && vt_line_is_rtl(vt_term_get_line(screen->term, beg_row))) {
      int tmp;

      tmp = end_char_index;
      end_char_index = beg_char_index;
      beg_char_index = tmp;
    }

    if (ui_is_before_sel_left_base_pos(&screen->sel, beg_char_index, beg_row)) {
      selecting(screen, beg_char_index, beg_row);
    } else {
      selecting(screen, end_char_index, end_row);
    }
  }
}

static void selecting_line(ui_screen_t *screen, int y, Time time) {
  int row;
  int beg_char_index;
  int beg_row;
  int end_char_index;
  int end_row;

  row = vt_term_convert_scr_row_to_abs(screen->term, convert_y_to_row(screen, NULL, y));

  if (vt_term_get_line_region(screen->term, &beg_row, &end_char_index, &end_row, row) == 0) {
    return;
  }

  if (vt_line_is_rtl(vt_term_get_line(screen->term, beg_row))) {
    beg_char_index = -vt_line_end_char_index(vt_term_get_line(screen->term, beg_row));
  } else {
    beg_char_index = 0;
  }

  if (vt_line_is_rtl(vt_term_get_line(screen->term, end_row))) {
    end_char_index -= vt_line_end_char_index(vt_term_get_line(screen->term, end_row));
  }

  if (!ui_is_selecting(&screen->sel)) {
    restore_selected_region_color_instantly(screen);
    start_selection(screen, beg_char_index, beg_row, SEL_LINE, 0);
    selecting(screen, end_char_index, end_row);
    ui_sel_lock(&screen->sel);
  } else if (ui_is_before_sel_left_base_pos(&screen->sel, beg_char_index, beg_row)) {
    selecting(screen, beg_char_index, beg_row);
  } else {
    selecting(screen, end_char_index, end_row);
  }
}

static void change_sb_mode(ui_screen_t *screen, ui_sb_mode_t sb_mode);

static void pointer_motion(ui_window_t *win, XMotionEvent *event) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (!(event->state & (ShiftMask | ControlMask)) &&
      vt_term_get_mouse_report_mode(screen->term) >= ANY_EVENT_MOUSE_REPORT) {
    restore_selected_region_color_instantly(screen);
    report_mouse_tracking(screen, event->x, event->y, 0, event->state, 1, 0);
  }
}

static void button_motion(ui_window_t *win, XMotionEvent *event) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  /*
   * event->state is never 0 because this function is 'button'_motion,
   * not 'pointer'_motion.
   */

  if (!(event->state & (ShiftMask | ControlMask)) && vt_term_get_mouse_report_mode(screen->term)) {
    if (vt_term_get_mouse_report_mode(screen->term) >= BUTTON_EVENT_MOUSE_REPORT) {
      int button;

      if (event->state & Button1Mask) {
        button = Button1;
      } else if (event->state & Button2Mask) {
        button = Button2;
      } else if (event->state & Button3Mask) {
        button = Button3;
      } else {
        return;
      }

      restore_selected_region_color_instantly(screen);
      report_mouse_tracking(screen, event->x, event->y, button, event->state, 1, 0);
    }
  } else if (!(event->state & Button2Mask)) {
    switch (ui_is_selecting(&screen->sel)) {
      case SEL_WORD:
        selecting_word(screen, event->x, event->y, event->time);
        break;

      case SEL_LINE:
        selecting_line(screen, event->y, event->time);
        break;

      default:
        /*
         * XXX
         * Button3 selection is disabled on libvte.
         * If Button3 is pressed after selecting in order to show
         * "copy" menu, button_motion can be called by slight movement
         * of the mouse cursor, then selection is reset unexpectedly.
         */
        if (!(event->state & Button3Mask) || !IS_LIBVTE(screen)) {
          int is_alt;
          int is_meta;

          selecting_with_motion(
              screen, event->x, event->y, event->time,
              (compare_key_state_with_modmap(screen, event->state, NULL, NULL, NULL, &is_alt,
                                             &is_meta, NULL, NULL, NULL) &&
               (is_alt || is_meta)));
        }

        break;
    }
  }
}

static void button_press_continued(ui_window_t *win, XButtonEvent *event) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (ui_is_selecting(&screen->sel) && (event->y < 0 || win->height < event->y)) {
    int is_alt;
    int is_meta;

    selecting_with_motion(screen, event->x, event->y, event->time,
                          (compare_key_state_with_modmap(screen, event->state, NULL, NULL, NULL,
                                                         &is_alt, &is_meta, NULL, NULL, NULL) &&
                           (is_alt || is_meta)));
  }
}

static void button_pressed(ui_window_t *win, XButtonEvent *event, int click_num) {
  ui_screen_t *screen;
  u_int state;

  screen = (ui_screen_t *)win;

  if (vt_term_get_mouse_report_mode(screen->term) && !(event->state & (ShiftMask | ControlMask))) {
    restore_selected_region_color_instantly(screen);
    report_mouse_tracking(screen, event->x, event->y, event->button, event->state, 0, 0);

    return;
  }

  state = (Button1Mask << (event->button - Button1)) | event->state;

  if (event->button == Button1) {
    if (click_num == 2) {
      /* double clicked */
      selecting_word(screen, event->x, event->y, event->time);

      return;
    } else if (click_num == 3) {
      /* triple click */
      selecting_line(screen, event->y, event->time);

      return;
    }
  }

  if (shortcut_match(screen, 0, state) || shortcut_str(screen, 0, state, event->x, event->y)) {
    return;
  }

  if (event->button == Button3) {
    if (ui_sel_is_reversed(&screen->sel)) {
      /* expand if current selection exists. */
      /* FIXME: move sel.* stuff should be in ui_selection.c */
      screen->sel.is_selecting = SEL_CHAR;
      selecting_with_motion(screen, event->x, event->y, event->time, 0);
      /* keep sel as selected to handle succeeding MotionNotify */
    }
  } else if (event->button == Button4) {
    /* wheel mouse */

    enter_backscroll_mode(screen);
    if (event->state & ShiftMask) {
      bs_scroll_downward(screen);
    } else if (event->state & ControlMask) {
      bs_page_downward(screen);
    } else {
      bs_half_page_downward(screen);
    }
  } else if (event->button == Button5) {
    /* wheel mouse */

    enter_backscroll_mode(screen);
    if (event->state & ShiftMask) {
      bs_scroll_upward(screen);
    } else if (event->state & ControlMask) {
      bs_page_upward(screen);
    } else {
      bs_half_page_upward(screen);
    }
  } else if (event->button < Button3) {
    restore_selected_region_color_instantly(screen);
  }
}

static void button_released(ui_window_t *win, XButtonEvent *event) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (vt_term_get_mouse_report_mode(screen->term) && !(event->state & (ShiftMask | ControlMask))) {
    if (event->button >= Button4) {
      /* Release events for the wheel buttons are not reported. */
    } else {
      report_mouse_tracking(screen, event->x, event->y, event->button, event->state, 0, 1);
    }

    /*
     * In case button motion -> set mouse report -> button release,
     * ui_stop_selecting() should be called.
     * (It happens if you release shift key before stopping text selection
     * by moving mouse.)
     */
  } else {
    if (event->button == Button2) {
      if (event->state & ControlMask) {
        /* FIXME: should check whether a menu is really active? */
        return;
      } else {
        yank_event_received(screen, event->time);
      }
    }
  }

  if (ui_stop_selecting(&screen->sel)) {
    ui_window_update(&screen->window, UPDATE_CURSOR);
  }
}

static void idling(ui_window_t *win) {
  ui_screen_t *screen;

  screen = (ui_screen_t *)win;

  if (screen->blink_wait >= 0) {
    if (screen->blink_wait == 5) {
      if (screen->window.is_focused) {
        int update;

        update = vt_term_blink(screen->term);

        if (vt_term_get_cursor_style(screen->term) & CS_BLINK) {
          unhighlight_cursor(screen, 1);
          update = 1;
        }

        if (update) {
          ui_window_update(&screen->window, UPDATE_SCREEN_BLINK);
        }
      }

      screen->blink_wait = -1;
    } else {
      screen->blink_wait++;
    }
  } else {
    if (screen->blink_wait == -6) {
      int flag;

      flag = vt_term_blink(screen->term) ? UPDATE_SCREEN : 0;

      if (vt_term_get_cursor_style(screen->term) & CS_BLINK) {
        flag |= UPDATE_CURSOR;
      }

      if (flag) {
        ui_window_update(&screen->window, flag);
      }

      screen->blink_wait = 0;
    } else {
      screen->blink_wait--;
    }
  }

#ifndef NO_IMAGE
  /*
   * 1-2: wait for the next frame. (0.1sec * 2)
   * 3: animate.
   */
  if (++screen->anim_wait == 3) {
    if ((screen->anim_wait = ui_animate_inline_pictures(screen->term))) {
      ui_window_update(&screen->window, UPDATE_SCREEN);
    }
  }
#endif
}

#ifdef HAVE_REGEX

#include <regex.h>

static int match(size_t *beg, size_t *len, void *regex, u_char *str, int backward) {
  regmatch_t pmatch[1];

  if (regexec(regex, str, 1, pmatch, 0) != 0) {
    return 0;
  }

  *beg = pmatch[0].rm_so;
  *len = pmatch[0].rm_eo - pmatch[0].rm_so;

  if (backward) {
    while (1) {
      str += pmatch[0].rm_eo;

      if (regexec(regex, str, 1, pmatch, 0) != 0) {
        break;
      }

      (*beg) += ((*len) + pmatch[0].rm_so);
      *len = pmatch[0].rm_eo - pmatch[0].rm_so;
    }
  }

  return 1;
}

#else /* HAVE_REGEX */

static int match(size_t *beg, size_t *len, void *regex, u_char *str, int backward) {
  size_t regex_len;
  size_t str_len;
  u_char *p;

  if ((regex_len = strlen(regex)) > (str_len = strlen(str))) {
    return 0;
  }

#if 0
  {
    bl_msg_printf("S T R => ");
    p = str;
    while (*p) {
      bl_msg_printf("%.2x", *p);
      p++;
    }
    bl_msg_printf("\nREGEX => ");

    p = regex;
    while (*p) {
      bl_msg_printf("%.2x", *p);
      p++;
    }
    bl_msg_printf("\n");
  }
#endif

  if (backward) {
    p = str + str_len - regex_len;

    do {
      if (strncasecmp(p, regex, regex_len) == 0) {
        goto found;
      }
    } while (p-- != str);

    return 0;
  } else {
    p = str;

    do {
      if (strncasecmp(p, regex, regex_len) == 0) {
        goto found;
      }
    } while (*(++p));

    return 0;
  }

found:
  *beg = p - str;
  *len = regex_len;

  return 1;
}

#endif /* HAVE_REGEX */

static int search_find(ui_screen_t *screen, u_char *pattern, int backward) {
  int beg_char_index;
  int beg_row;
  int end_char_index;
  int end_row;
#ifdef HAVE_REGEX
  regex_t regex;
#endif

  if (pattern && *pattern
#ifdef HAVE_REGEX
      && regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE) == 0
#endif
      ) {
    vt_term_search_init(screen->term, match);
#ifdef HAVE_REGEX
    if (vt_term_search_find(screen->term, &beg_char_index, &beg_row, &end_char_index, &end_row,
                            &regex, backward))
#else
    if (vt_term_search_find(screen->term, &beg_char_index, &beg_row, &end_char_index, &end_row,
                            pattern, backward))
#endif
    {
#ifdef DEBUG
      bl_debug_printf(BL_DEBUG_TAG " Search find %d %d - %d %d\n", beg_char_index, beg_row,
                      end_char_index, end_row);
#endif

      ui_sel_clear(&screen->sel);
      start_selection(screen, beg_char_index, beg_row, SEL_CHAR, 0);
      selecting(screen, end_char_index, end_row);
      ui_stop_selecting(&screen->sel);

      ui_screen_scroll_to(screen, beg_row);
      if (HAS_SCROLL_LISTENER(screen, scrolled_to)) {
        (*screen->screen_scroll_listener->scrolled_to)(screen->screen_scroll_listener->self,
                                                       beg_row);
      }
    }

#ifdef HAVE_REGEX
    regfree(&regex);
#endif
  } else {
    vt_term_search_final(screen->term);
  }

  return 1;
}

static void resize_window(ui_screen_t *screen) {
  /* screen will redrawn in window_resized() */
  if (ui_window_resize(&screen->window,
                       (screen->width = screen_width(screen)),
                       (screen->height = screen_height(screen)),
                       NOTIFY_TO_PARENT)) {
    /*
     * !! Notice !!
     * ui_window_resize() will invoke ConfigureNotify event but window_resized()
     * won't be called , since xconfigure.width , xconfigure.height are the same
     * as the already resized window.
     */
    window_resized(&screen->window);
  }
}

static void modify_line_space_and_offset(ui_screen_t *screen) {
  u_int font_height = ui_get_usascii_font(screen->font_man)->height;

  if (screen->line_space < 0 && font_height < -screen->line_space * 4) {
    bl_msg_printf("Ignore line_space (%d)\n", screen->line_space);
    screen->line_space = 0;
  }

  if (font_height < abs(screen->baseline_offset) * 8) {
    bl_msg_printf("Ignore baseline_offset (%d)\n", screen->baseline_offset);
    screen->baseline_offset = 0;
  }

  if (screen->underline_offset != 0) {
    if (screen->underline_offset < 0) {
      if (font_height >= -screen->underline_offset * 8) {
        return;
      }
    } else {
      if (screen->underline_offset <= font_height - ui_get_usascii_font(screen->font_man)->ascent +
                                      screen->line_space / 2 /* + screen->line_space % 2 */ -
                                      screen->baseline_offset) {
        /* underline_offset < descent + bottom_margin */
        return;
      }
    }

    bl_msg_printf("Ignore underline_offset (%d)\n", screen->underline_offset);
    screen->underline_offset = 0;
  }
}

static void font_size_changed(ui_screen_t *screen) {
  u_int col_width;
  u_int line_height;

  modify_line_space_and_offset(screen);

  if (HAS_SCROLL_LISTENER(screen, line_height_changed)) {
    (*screen->screen_scroll_listener->line_height_changed)(screen->screen_scroll_listener->self,
                                                           ui_line_height(screen));
  }

  col_width = ui_col_width(screen);
  line_height = ui_line_height(screen);

  ui_window_set_normal_hints(&screen->window, col_width, line_height, col_width, line_height);

  resize_window(screen);
}

static void change_font_size(ui_screen_t *screen, u_int font_size) {
  if (font_size == ui_get_font_size(screen->font_man)) {
    /* not changed */

    return;
  }

  if (!ui_change_font_size(screen->font_man, font_size)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_change_font_size(%d) failed.\n", font_size);
#endif

    return;
  }

  /* redrawing all lines with new fonts. */
  vt_term_set_modified_all_lines_in_screen(screen->term);

  font_size_changed(screen);

  /* this is because font_man->font_set may have changed in
   * ui_change_font_size()
   */
  ui_xic_font_set_changed(&screen->window);
}

static void change_line_space(ui_screen_t *screen, int line_space) {
  if (screen->line_space == line_space) {
    /* not changed */

    return;
  }

  /* letter_space and line_space are ignored on console. */
#ifndef USE_CONSOLE
  screen->line_space = line_space;

  font_size_changed(screen);
#endif
}

static void change_letter_space(ui_screen_t *screen, u_int letter_space) {
  if (!ui_set_letter_space(screen->font_man, letter_space)) {
    return;
  }

  font_size_changed(screen);
}

static void change_screen_width_ratio(ui_screen_t *screen, u_int ratio) {
  if (screen->screen_width_ratio == ratio) {
    return;
  }

  screen->screen_width_ratio = ratio;

  resize_window(screen);
}

static void change_font_present(ui_screen_t *screen, ui_type_engine_t type_engine,
                                ui_font_present_t font_present) {
  if (vt_term_get_vertical_mode(screen->term)) {
    if (font_present & FONT_VAR_WIDTH) {
      bl_msg_printf("Set use_variable_column_width=false forcibly.\n");
      font_present &= ~FONT_VAR_WIDTH;
    }
  }

  if (!ui_change_font_present(screen->font_man, type_engine, font_present)) {
    return;
  }

  /* XXX This function is called from ui_screen_new via update_special_visual.
   */
  if (!screen->window.my_window) {
    return;
  }

  ui_window_set_type_engine(&screen->window, ui_get_type_engine(screen->font_man));

  /* redrawing all lines with new fonts. */
  vt_term_set_modified_all_lines_in_screen(screen->term);

  font_size_changed(screen);
}

static int usascii_font_cs_changed(ui_screen_t *screen, vt_char_encoding_t encoding) {
  ef_charset_t cs;

  if (vt_term_get_unicode_policy(screen->term) & NOT_USE_UNICODE_FONT) {
    cs = ui_get_usascii_font_cs(VT_ISO8859_1);
  } else if (vt_term_get_unicode_policy(screen->term) & ONLY_USE_UNICODE_FONT) {
    cs = ui_get_usascii_font_cs(VT_UTF8);
  } else {
    cs = ui_get_usascii_font_cs(encoding);
  }

  if (ui_font_manager_usascii_font_cs_changed(screen->font_man, cs)) {
    font_size_changed(screen);

    /*
     * this is because font_man->font_set may have changed in
     * ui_font_manager_usascii_font_cs_changed()
     */
    ui_xic_font_set_changed(&screen->window);

    return 1;
  } else {
    return 0;
  }
}

static void change_char_encoding(ui_screen_t *screen, vt_char_encoding_t encoding) {
  if (vt_term_get_encoding(screen->term) == encoding) {
    /* not changed */

    return;
  }

  usascii_font_cs_changed(screen, encoding);

  if (!vt_term_change_encoding(screen->term, encoding)) {
    bl_error_printf("VT100 encoding and Terminal screen encoding are discrepant.\n");
  }

  if (update_special_visual(screen)) {
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }

  if (screen->im) {
    change_im(screen, bl_str_alloca_dup(screen->input_method));
  }
}

static void change_log_size(ui_screen_t *screen, u_int logsize) {
  if (vt_term_get_log_size(screen->term) == logsize) {
    /* not changed */

    return;
  }

  /*
   * this is necessary since vt_logs_t size is changed.
   */
  ui_stop_selecting(&screen->sel);
  restore_selected_region_color_instantly(screen);
  exit_backscroll_mode(screen);

  vt_term_change_log_size(screen->term, logsize);

  if (HAS_SCROLL_LISTENER(screen, log_size_changed)) {
    (*screen->screen_scroll_listener->log_size_changed)(screen->screen_scroll_listener->self,
                                                        logsize);
  }
}

static void change_sb_view(ui_screen_t *screen, char *name) {
  if (HAS_SCROLL_LISTENER(screen, change_view)) {
    (*screen->screen_scroll_listener->change_view)(screen->screen_scroll_listener->self, name);
  }
}

static void change_mod_meta_key(ui_screen_t *screen, char *key) {
  free(screen->mod_meta_key);

  if (strcmp(key, "none") == 0) {
    screen->mod_meta_key = NULL;
  } else {
    screen->mod_meta_key = strdup(key);
  }

  screen->mod_meta_mask = ui_window_get_mod_meta_mask(&(screen->window), screen->mod_meta_key);
}

static void change_mod_meta_mode(ui_screen_t *screen, ui_mod_meta_mode_t mod_meta_mode) {
  screen->mod_meta_mode = mod_meta_mode;
}

static void change_bel_mode(ui_screen_t *screen, ui_bel_mode_t bel_mode) {
  screen->bel_mode = bel_mode;
}

static void change_vertical_mode(ui_screen_t *screen, vt_vertical_mode_t vertical_mode) {
  if (vt_term_get_vertical_mode(screen->term) == vertical_mode) {
    /* not changed */

    return;
  }

  vt_term_set_vertical_mode(screen->term, vertical_mode);

  if (update_special_visual(screen)) {
    /* redrawing under new vertical mode. */
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }
}

static void change_sb_mode(ui_screen_t *screen, ui_sb_mode_t sb_mode) {
  if (HAS_SCROLL_LISTENER(screen, change_sb_mode)) {
    (*screen->screen_scroll_listener->change_sb_mode)(screen->screen_scroll_listener->self,
                                                      sb_mode);
  }
}

static void change_dynamic_comb_flag(ui_screen_t *screen, int use_dynamic_comb) {
  if (vt_term_is_using_dynamic_comb(screen->term) == use_dynamic_comb) {
    /* not changed */

    return;
  }

  vt_term_set_use_dynamic_comb(screen->term, use_dynamic_comb);

  if (update_special_visual(screen)) {
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }
}

static void change_receive_string_via_ucs_flag(ui_screen_t *screen, int flag) {
  screen->receive_string_via_ucs = flag;
}

static void change_fg_color(ui_screen_t *screen, char *name) {
  if (ui_color_manager_set_fg_color(screen->color_man, name) &&
      ui_window_set_fg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_FG_COLOR))) {
    ui_xic_fg_color_changed(&screen->window);

    vt_term_set_modified_all_lines_in_screen(screen->term);

    if (HAS_SCROLL_LISTENER(screen, screen_color_changed)) {
      (*screen->screen_scroll_listener->screen_color_changed)(screen->screen_scroll_listener->self);
    }
  }
}

static void picture_modifier_changed(ui_screen_t *screen);

static void change_bg_color(ui_screen_t *screen, char *name) {
  if (ui_color_manager_set_bg_color(screen->color_man, name) &&
      ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR))) {
    ui_xic_bg_color_changed(&screen->window);

    ui_get_xcolor_rgba(&screen->pic_mod.blend_red, &screen->pic_mod.blend_green,
                       &screen->pic_mod.blend_blue, NULL,
                       ui_get_xcolor(screen->color_man, VT_BG_COLOR));

    picture_modifier_changed(screen);

    vt_term_set_modified_all_lines_in_screen(screen->term);

    if (HAS_SCROLL_LISTENER(screen, screen_color_changed)) {
      (*screen->screen_scroll_listener->screen_color_changed)(screen->screen_scroll_listener->self);
    }
  }
}

static void change_alt_color(ui_screen_t *screen, vt_color_t color, char *name) {
  if (ui_color_manager_set_alt_color(screen->color_man, color, *name ? name : NULL)) {
    vt_term_set_modified_all_lines_in_screen(screen->term);

    vt_term_set_alt_color_mode(
        screen->term,
        *name ? (vt_term_get_alt_color_mode(screen->term) | (1 << (color - VT_BOLD_COLOR)))
              : (vt_term_get_alt_color_mode(screen->term) & ~(1 << (color - VT_BOLD_COLOR))));
  }
}

static void change_sb_fg_color(ui_screen_t *screen, char *name) {
  if (HAS_SCROLL_LISTENER(screen, change_fg_color)) {
    (*screen->screen_scroll_listener->change_fg_color)(screen->screen_scroll_listener->self, name);
  }
}

static void change_sb_bg_color(ui_screen_t *screen, char *name) {
  if (HAS_SCROLL_LISTENER(screen, change_bg_color)) {
    (*screen->screen_scroll_listener->change_bg_color)(screen->screen_scroll_listener->self, name);
  }
}

static void change_use_bold_font_flag(ui_screen_t *screen, int flag) {
  if (ui_set_use_bold_font(screen->font_man, flag)) {
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }
}

static void change_use_italic_font_flag(ui_screen_t *screen, int flag) {
  if (ui_set_use_italic_font(screen->font_man, flag)) {
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }
}

static void change_hide_underline_flag(ui_screen_t *screen, int flag) {
  if (screen->hide_underline != flag) {
    screen->hide_underline = flag;
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }
}

static void change_underline_offset(ui_screen_t *screen, int offset) {
  if (screen->underline_offset != offset) {
    screen->underline_offset = offset;
    modify_line_space_and_offset(screen);
  }
}

static void change_baseline_offset(ui_screen_t *screen, int offset) {
#ifndef USE_CONSOLE
  if (screen->baseline_offset != offset) {
    screen->baseline_offset = offset;
    modify_line_space_and_offset(screen);
  }
#endif
}

static void larger_font_size(ui_screen_t *screen) {
  ui_larger_font(screen->font_man);

  font_size_changed(screen);

  /* this is because font_man->font_set may have changed in ui_larger_font() */
  ui_xic_font_set_changed(&screen->window);

  /* redrawing all lines with new fonts. */
  vt_term_set_modified_all_lines_in_screen(screen->term);
}

static void smaller_font_size(ui_screen_t *screen) {
  ui_smaller_font(screen->font_man);

  font_size_changed(screen);

  /* this is because font_man->font_set may have changed in ui_smaller_font() */
  ui_xic_font_set_changed(&screen->window);

  /* redrawing all lines with new fonts. */
  vt_term_set_modified_all_lines_in_screen(screen->term);
}

static int change_true_transbg_alpha(ui_screen_t *screen, u_int alpha) {
  int ret;

  if ((ret = ui_change_true_transbg_alpha(screen->color_man, alpha)) > 0) {
    /* True transparency works. Same processing as change_bg_color */
    if (ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR))) {
      ui_xic_bg_color_changed(&screen->window);

      vt_term_set_modified_all_lines_in_screen(screen->term);
    }
  }

  return ret;
}

static void change_transparent_flag(ui_screen_t *screen, int is_transparent) {
  if (screen->window.is_transparent == is_transparent
#ifdef USE_XLIB
      /*
       * If wall picture is not still set, do set it.
       * This is necessary for gnome-terminal, because ConfigureNotify event
       * never
       * happens when it opens new tab.
       */
      &&
      screen->window.wall_picture_is_set == is_transparent
#endif
      ) {
    /* not changed */

    return;
  }

  if (is_transparent) {
    /* disable true transparency */
    change_true_transbg_alpha(screen, 255);
    ui_window_set_transparent(&screen->window, ui_screen_get_picture_modifier(screen));
  } else {
    ui_window_unset_transparent(&screen->window);
    set_wall_picture(screen);
  }

  if (HAS_SCROLL_LISTENER(screen, transparent_state_changed)) {
    (*screen->screen_scroll_listener->transparent_state_changed)(
        screen->screen_scroll_listener->self, is_transparent,
        ui_screen_get_picture_modifier(screen));
  }
}

static void change_ctl_flag(ui_screen_t *screen, int use_ctl, vt_bidi_mode_t bidi_mode,
                            int use_ot_layout) {
  int do_update;

  if (vt_term_is_using_ctl(screen->term) == use_ctl &&
      vt_term_get_bidi_mode(screen->term) == bidi_mode &&
      vt_term_is_using_ot_layout(screen->term) == use_ot_layout) {
    /* not changed */

    return;
  }

  /*
   * If use_ctl flag is false and not changed, it is not necessary to update
   * even if
   * bidi_mode flag and use_ot_layout are changed.
   */
  do_update = (use_ctl != vt_term_is_using_ctl(screen->term)) || vt_term_is_using_ctl(screen->term);

  vt_term_set_use_ctl(screen->term, use_ctl);
  vt_term_set_bidi_mode(screen->term, bidi_mode);
  vt_term_set_use_ot_layout(screen->term, use_ot_layout);

  if (do_update && update_special_visual(screen)) {
    vt_term_set_modified_all_lines_in_screen(screen->term);
  }
}

static void change_borderless_flag(ui_screen_t *screen, int flag) {
  if (ui_window_set_borderless_flag(&screen->window, flag)) {
    screen->borderless = flag;
  }
}

static void change_wall_picture(ui_screen_t *screen, char *file_path) {
  if (screen->pic_file_path) {
    if (strcmp(screen->pic_file_path, file_path) == 0) {
      /* not changed */

      return;
    }

    free(screen->pic_file_path);
  }

  if (*file_path == '\0') {
    if (!screen->pic_file_path) {
      return;
    }

    screen->pic_file_path = NULL;
    ui_window_unset_wall_picture(&screen->window, 1);
  } else {
    screen->pic_file_path = strdup(file_path);

    if (set_wall_picture(screen)) {
      return;
    }
  }

  /* disable true transparency */
  change_true_transbg_alpha(screen, 255);
}

static void picture_modifier_changed(ui_screen_t *screen) {
  if (screen->window.is_transparent) {
    ui_window_set_transparent(&screen->window, ui_screen_get_picture_modifier(screen));

    if (HAS_SCROLL_LISTENER(screen, transparent_state_changed)) {
      (*screen->screen_scroll_listener->transparent_state_changed)(
          screen->screen_scroll_listener->self, 1, ui_screen_get_picture_modifier(screen));
    }
  } else {
    set_wall_picture(screen);
  }
}

static void change_brightness(ui_screen_t *screen, u_int brightness) {
  if (screen->pic_mod.brightness == brightness) {
    /* not changed */

    return;
  }

  screen->pic_mod.brightness = brightness;

  picture_modifier_changed(screen);
}

static void change_contrast(ui_screen_t *screen, u_int contrast) {
  if (screen->pic_mod.contrast == contrast) {
    /* not changed */

    return;
  }

  screen->pic_mod.contrast = contrast;

  picture_modifier_changed(screen);
}

static void change_gamma(ui_screen_t *screen, u_int gamma) {
  if (screen->pic_mod.gamma == gamma) {
    /* not changed */

    return;
  }

  screen->pic_mod.gamma = gamma;

  picture_modifier_changed(screen);
}

static void change_alpha(ui_screen_t *screen, u_int alpha) {
  if (!ui_window_has_wall_picture(&screen->window) && change_true_transbg_alpha(screen, alpha)) {
    /* True transparency works. */
    if (alpha == 255) {
      /* Completely opaque. => reset pic_mod.alpha. */
      screen->pic_mod.alpha = 0;
    } else {
      screen->pic_mod.alpha = alpha;
    }
  } else {
    /* True transparency doesn't work. */
    if (screen->pic_mod.alpha == alpha) {
      /* not changed */
      return;
    }

    screen->pic_mod.alpha = alpha;

    picture_modifier_changed(screen);
  }
}

static void change_fade_ratio(ui_screen_t *screen, u_int fade_ratio) {
  if (screen->fade_ratio == fade_ratio) {
    /* not changed */

    return;
  }

  screen->fade_ratio = fade_ratio;

  ui_color_manager_unfade(screen->color_man);

  if (!screen->window.is_focused) {
    if (screen->fade_ratio < 100) {
      ui_color_manager_fade(screen->color_man, screen->fade_ratio);
    }
  }

  ui_window_set_fg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_FG_COLOR));
  ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR));

  vt_term_set_modified_all_lines_in_screen(screen->term);
}

static void change_im(ui_screen_t *screen, char *input_method) {
  ui_im_t *im;

  ui_xic_deactivate(&screen->window);

  /*
   * Avoid to delete anything inside im-module by calling ui_im_delete()
   * after ui_im_new().
   */
  im = screen->im;

  free(screen->input_method);
  screen->input_method = NULL;

  if (!input_method) {
    return;
  }

  screen->input_method = strdup(input_method);

  if (strncmp(screen->input_method, "xim", 3) == 0) {
    activate_xic(screen);
    screen->im = NULL;
  } else {
    ui_xic_activate(&screen->window, "unused", "");

    if ((screen->im = im_new(screen))) {
      if (screen->window.is_focused) {
        screen->im->focused(screen->im);
      }
    } else {
      free(screen->input_method);
      screen->input_method = NULL;
    }
  }

  if (im) {
    ui_im_delete(im);
  }
}

/*
 * Callbacks of ui_config_event_listener_t events.
 */

static void get_config_intern(ui_screen_t *screen, char *dev, /* can be NULL */
                              char *key,                      /* can be "error" */
                              int to_menu, /* -1: don't output to pty and menu. */
                              int *flag    /* 1(true), 0(false) or -1(other) is returned. */
                              ) {
  vt_term_t *term;
  char *value;
  char digit[DIGIT_STR_LEN(u_int) + 1];

  if (dev) {
    if (!(term = vt_get_term(dev))) {
      goto error;
    }
  } else {
    term = screen->term;
  }

  if (vt_term_get_config(term, dev ? screen->term : NULL, key, to_menu, flag)) {
    return;
  }

  value = NULL;

  if (strcmp(key, "fg_color") == 0) {
    value = ui_color_manager_get_fg_color(screen->color_man);
  } else if (strcmp(key, "bg_color") == 0) {
    value = ui_color_manager_get_bg_color(screen->color_man);
  } else if (strcmp(key, "cursor_fg_color") == 0) {
    if ((value = ui_color_manager_get_cursor_fg_color(screen->color_man)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "cursor_bg_color") == 0) {
    if ((value = ui_color_manager_get_cursor_bg_color(screen->color_man)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "bd_color") == 0) {
    if ((value = ui_color_manager_get_alt_color(screen->color_man, VT_BOLD_COLOR)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "it_color") == 0) {
    if ((value = ui_color_manager_get_alt_color(screen->color_man, VT_ITALIC_COLOR)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "ul_color") == 0) {
    if ((value = ui_color_manager_get_alt_color(screen->color_man, VT_UNDERLINE_COLOR)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "bl_color") == 0) {
    if ((value = ui_color_manager_get_alt_color(screen->color_man, VT_BLINKING_COLOR)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "co_color") == 0) {
    if ((value = ui_color_manager_get_alt_color(screen->color_man, VT_CROSSED_OUT_COLOR)) == NULL) {
      value = "";
    }
  } else if (strcmp(key, "sb_fg_color") == 0) {
    if (screen->screen_scroll_listener && screen->screen_scroll_listener->fg_color) {
      value = (*screen->screen_scroll_listener->fg_color)(screen->screen_scroll_listener->self);
    }
  } else if (strcmp(key, "sb_bg_color") == 0) {
    if (screen->screen_scroll_listener && screen->screen_scroll_listener->bg_color) {
      value = (*screen->screen_scroll_listener->bg_color)(screen->screen_scroll_listener->self);
    }
  } else if (strcmp(key, "hide_underline") == 0) {
    if (screen->hide_underline) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "underline_offset") == 0) {
    sprintf(digit, "%d", screen->underline_offset);
    value = digit;
  } else if (strcmp(key, "baseline_offset") == 0) {
    sprintf(digit, "%d", screen->baseline_offset);
    value = digit;
  } else if (strcmp(key, "fontsize") == 0) {
    sprintf(digit, "%d", ui_get_font_size(screen->font_man));
    value = digit;
  } else if (strcmp(key, "line_space") == 0) {
    sprintf(digit, "%d", screen->line_space);
    value = digit;
  } else if (strcmp(key, "letter_space") == 0) {
    sprintf(digit, "%d", ui_get_letter_space(screen->font_man));
    value = digit;
  } else if (strcmp(key, "screen_width_ratio") == 0) {
    sprintf(digit, "%d", screen->screen_width_ratio);
    value = digit;
  } else if (strcmp(key, "scrollbar_view_name") == 0) {
    if (screen->screen_scroll_listener && screen->screen_scroll_listener->view_name) {
      value = (*screen->screen_scroll_listener->view_name)(screen->screen_scroll_listener->self);
    }
  } else if (strcmp(key, "mod_meta_key") == 0) {
    if (screen->mod_meta_key == NULL) {
      value = "none";
    } else {
      value = screen->mod_meta_key;
    }
  } else if (strcmp(key, "mod_meta_mode") == 0) {
    value = ui_get_mod_meta_mode_name(screen->mod_meta_mode);
  } else if (strcmp(key, "bel_mode") == 0) {
    value = ui_get_bel_mode_name(screen->bel_mode);
  } else if (strcmp(key, "scrollbar_mode") == 0) {
    if (screen->screen_scroll_listener && screen->screen_scroll_listener->sb_mode) {
      value = ui_get_sb_mode_name(
          (*screen->screen_scroll_listener->sb_mode)(screen->screen_scroll_listener->self));
    } else {
      value = ui_get_sb_mode_name(SBM_NONE);
    }
  } else if (strcmp(key, "receive_string_via_ucs") == 0 ||
             /* backward compatibility with 2.6.1 or before */
             strcmp(key, "copy_paste_via_ucs") == 0) {
    if (screen->receive_string_via_ucs) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_transbg") == 0) {
    if (screen->window.is_transparent) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "brightness") == 0) {
    sprintf(digit, "%d", screen->pic_mod.brightness);
    value = digit;
  } else if (strcmp(key, "contrast") == 0) {
    sprintf(digit, "%d", screen->pic_mod.contrast);
    value = digit;
  } else if (strcmp(key, "gamma") == 0) {
    sprintf(digit, "%d", screen->pic_mod.gamma);
    value = digit;
  } else if (strcmp(key, "alpha") == 0) {
    if (screen->window.disp->depth < 32) {
      sprintf(digit, "%d", screen->color_man->alpha);
    } else {
      sprintf(digit, "%d", screen->pic_mod.alpha);
    }
    value = digit;
  } else if (strcmp(key, "fade_ratio") == 0) {
    sprintf(digit, "%d", screen->fade_ratio);
    value = digit;
  } else if (strcmp(key, "type_engine") == 0) {
    value = ui_get_type_engine_name(ui_get_type_engine(screen->font_man));
  } else if (strcmp(key, "use_anti_alias") == 0) {
    ui_font_present_t font_present;

    font_present = ui_get_font_present(screen->font_man);
    if (font_present & FONT_AA) {
      value = "true";
    } else if (font_present & FONT_NOAA) {
      value = "false";
    } else {
      value = "default";
    }
  } else if (strcmp(key, "use_variable_column_width") == 0) {
    if (ui_get_font_present(screen->font_man) & FONT_VAR_WIDTH) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_multi_column_char") == 0) {
    if (vt_term_is_using_multi_col_char(screen->term)) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_bold_font") == 0) {
    if (ui_is_using_bold_font(screen->font_man)) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_italic_font") == 0) {
    if (ui_is_using_italic_font(screen->font_man)) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "input_method") == 0) {
    if (screen->input_method) {
      value = screen->input_method;
    } else {
      value = "none";
    }
  } else if (strcmp(key, "default_xim_name") == 0) {
    value = ui_xic_get_default_xim_name();
  } else if (strcmp(key, "borderless") == 0) {
    if (screen->borderless) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "wall_picture") == 0) {
    if (screen->pic_file_path) {
      value = screen->pic_file_path;
    } else {
      value = "";
    }
  } else if (strcmp(key, "gui") == 0) {
    value = GUI_TYPE;
  } else if (strcmp(key, "use_clipboard") == 0) {
    if (ui_is_using_clipboard_selection()) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "allow_osc52") == 0) {
    if (screen->xterm_listener.set_selection) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "use_extended_scroll_shortcut") == 0) {
    if (screen->use_extended_scroll_shortcut) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "pty_list") == 0) {
    value = vt_get_pty_list();
  } else if (strcmp(key, "trim_trailing_newline_in_pasting") == 0) {
    if (trim_trailing_newline_in_pasting) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "fontconfig") == 0) {
    if (to_menu <= 0) {
      char *list = ui_font_manager_dump_font_config(screen->font_man);
      vt_term_response_config(screen->term, list, NULL, -1);
      free(list);

      return;
    }
  } else if (strcmp(key, "use_vertical_cursor") == 0) {
    if (screen->use_vertical_cursor) {
      value = "true";
    } else {
      value = "false";
    }
  } else if (strcmp(key, "click_interval") == 0) {
    sprintf(digit, "%d", ui_get_click_interval());
    value = digit;
  }
#if defined(USE_FREETYPE) && defined(USE_FONTCONFIG)
  else if (strcmp(key, "use_aafont") == 0) {
    if (ui_is_using_aafont()) {
      value = "true";
    } else {
      value = "false";
    }
  }
#endif

  if (value) {
    if (flag) {
      *flag = value ? true_or_false(value) : -1;
    } else {
      vt_term_response_config(screen->term, key, value, to_menu);
    }
  } else {
  error:
    vt_term_response_config(screen->term, "error", NULL, to_menu);
  }
}

static void get_config(void *p, char *dev, /* can be NULL */
                       char *key,          /* can be "error" */
                       int to_menu) {
  get_config_intern(p, dev, key, to_menu, NULL);
}

static void set_font_config(void *p, char *file, /* can be NULL */
                            char *cs, char *val, int save) {
  ui_screen_t *screen;

  screen = p;

  /* USASCII, US-ASCII, US_ASCII */
  if (strncmp(cs, "US", 2) == 0 && (strcmp(cs + 2, "ASCII") == 0 || strcmp(cs + 3, "ASCII") == 0)) {
    cs = ui_get_charset_name(ui_get_current_usascii_font_cs(screen->font_man));
  }

  if (ui_customize_font_file(file, cs, val, save)) {
    screen->font_or_color_config_updated |= 0x1;
  }
}

static void get_font_config(void *p, char *file, /* can be NULL */
                            char *cs, int to_menu) {
  ui_screen_t *screen;
  char *font_name;

  screen = p;

  /* USASCII, US-ASCII, US_ASCII */
  if (strncmp(cs, "US", 2) == 0 && (strcmp(cs + 2, "ASCII") == 0 || strcmp(cs + 3, "ASCII") == 0)) {
    cs = ui_get_charset_name(ui_get_current_usascii_font_cs(screen->font_man));
  }

  font_name = ui_get_config_font_name2(file, ui_get_font_size(screen->font_man), cs);

  vt_term_response_config(screen->term, cs, font_name ? font_name : "", to_menu);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " #%s=%s (%s)\n", cs, font_name, to_menu ? "to menu" : "to pty");
#endif

  free(font_name);

  return;
}

static void set_color_config(void *p, char *file, /* ignored */
                             char *key, char *val, int save)

{
  ui_screen_t *screen;

  screen = p;

  if (vt_customize_color_file(key, val, save)) {
    screen->font_or_color_config_updated |= 0x2;
  }
}

static void get_color_config(void *p, char *key, int to_menu) {
  ui_screen_t *screen;
  vt_color_t color;
  ui_color_t *xcolor;
  u_int8_t red;
  u_int8_t green;
  u_int8_t blue;
  u_int8_t alpha;
  char rgba[10];

  screen = p;

  if ((color = vt_get_color(key)) == VT_UNKNOWN_COLOR ||
      !(xcolor = ui_get_xcolor(screen->color_man, color))) {
    vt_term_response_config(screen->term, "error", NULL, to_menu);

    return;
  }

  ui_get_xcolor_rgba(&red, &green, &blue, &alpha, xcolor);
  sprintf(rgba, alpha == 255 ? "#%.2x%.2x%.2x" : "#%.2x%.2x%.2x%.2x", red, green, blue, alpha);

  vt_term_response_config(screen->term, key, rgba, to_menu);
}

/*
 * callbacks of ui_sel_event_listener_t events.
 */

static void reverse_color(void *p, int beg_char_index, int beg_row, int end_char_index, int end_row,
                          int is_rect) {
  ui_screen_t *screen;
  vt_line_t *line;

  screen = (ui_screen_t *)p;

  /*
   * Char index -1 has special meaning in rtl lines, so don't use abs() here.
   */

  if ((line = vt_term_get_line(screen->term, beg_row)) && vt_line_is_rtl(line)) {
    beg_char_index = -beg_char_index;
  }

  if ((line = vt_term_get_line(screen->term, end_row)) && vt_line_is_rtl(line)) {
    end_char_index = -end_char_index;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " reversing region %d %d %d %d.\n", beg_char_index, beg_row,
                  end_char_index, end_row);
#endif

  vt_term_reverse_color(screen->term, beg_char_index, beg_row, end_char_index, end_row, is_rect);
}

static void restore_color(void *p, int beg_char_index, int beg_row, int end_char_index, int end_row,
                          int is_rect) {
  ui_screen_t *screen;
  vt_line_t *line;

  screen = (ui_screen_t *)p;

  /*
   * Char index -1 has special meaning in rtl lines, so don't use abs() here.
   */

  if ((line = vt_term_get_line(screen->term, beg_row)) && vt_line_is_rtl(line)) {
    beg_char_index = -beg_char_index;
  }

  if ((line = vt_term_get_line(screen->term, end_row)) && vt_line_is_rtl(line)) {
    end_char_index = -end_char_index;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " restoring region %d %d %d %d.\n", beg_char_index, beg_row,
                  end_char_index, end_row);
#endif

  vt_term_restore_color(screen->term, beg_char_index, beg_row, end_char_index, end_row, is_rect);
}

static int select_in_window(void *p, vt_char_t **chars, u_int *len, int beg_char_index, int beg_row,
                            int end_char_index, int end_row, int is_rect) {
  ui_screen_t *screen;
  vt_line_t *line;
  u_int size;

  screen = p;

  /*
   * Char index -1 has special meaning in rtl lines, so don't use abs() here.
   */

  if ((line = vt_term_get_line(screen->term, beg_row)) && vt_line_is_rtl(line)) {
    beg_char_index = -beg_char_index;
  }

  if ((line = vt_term_get_line(screen->term, end_row)) && vt_line_is_rtl(line)) {
    end_char_index = -end_char_index;
  }

  if ((size = vt_term_get_region_size(screen->term, beg_char_index, beg_row, end_char_index,
                                      end_row, is_rect)) == 0) {
    return 0;
  }

  if ((*chars = vt_str_new(size)) == NULL) {
    return 0;
  }

  *len = vt_term_copy_region(screen->term, *chars, size, beg_char_index, beg_row, end_char_index,
                             end_row, is_rect);

#ifdef DEBUG
  bl_debug_printf("SELECTION: ");
  vt_str_dump(*chars, size);
#endif

#ifdef DEBUG
  if (size != *len) {
    bl_warn_printf(BL_DEBUG_TAG
                   " vt_term_get_region_size() == %d and vt_term_copy_region() == %d"
                   " are not the same size !\n",
                   size, *len);
  }
#endif

  if (!ui_window_set_selection_owner(&screen->window, CurrentTime)) {
    vt_str_delete(*chars, size);

    return 0;
  } else {
    return 1;
  }
}

/*
 * callbacks of vt_screen_event_listener_t events.
 */

static int window_scroll_upward_region(void *p, int beg_row, int end_row, u_int size) {
  ui_screen_t *screen;

  screen = p;

  if (!ui_window_is_scrollable(&screen->window)) {
    return 0;
  }

  set_scroll_boundary(screen, beg_row, end_row);
  screen->scroll_cache_rows += size;

  return 1;
}

static int window_scroll_downward_region(void *p, int beg_row, int end_row, u_int size) {
  ui_screen_t *screen;

  screen = p;

  if (!ui_window_is_scrollable(&screen->window)) {
    return 0;
  }

  set_scroll_boundary(screen, beg_row, end_row);
  screen->scroll_cache_rows -= size;

  return 1;
}

static void line_scrolled_out(void *p) {
  ui_screen_t *screen;

  screen = p;

  ui_sel_line_scrolled_out(&screen->sel, -((int)vt_term_get_log_size(screen->term)));

#ifndef NO_IMAGE
  /*
   * Note that scrolled out line hasn't been added to vt_logs_t yet here.
   * (see receive_scrolled_out_line() in vt_screen.c)
   */
  if (vt_term_get_num_of_logged_lines(screen->term) >= -INLINEPIC_AVAIL_ROW) {
    vt_line_t *line;

    if ((line = vt_term_get_line(screen->term, INLINEPIC_AVAIL_ROW))) {
      int count;

      for (count = 0; count < line->num_of_filled_chars; count++) {
        vt_char_t *ch;

        if ((ch = vt_get_picture_char(line->chars + count))) {
          vt_char_copy(ch, vt_sp_ch());
        }
      }
    }
  }
#endif
}

/*
 * callbacks of ui_xim events.
 */

static int get_spot_intern(ui_screen_t *screen, vt_char_t *chars, int segment_offset,
                           int return_line_bottom, int ignore_vertical_mode, int *x, int *y) {
  vt_line_t *line;
  vt_char_t *comb_chars;
  u_int comb_size;
  int i;

  *x = *y = 0;

  if ((line = vt_term_get_cursor_line(screen->term)) == NULL || vt_line_is_empty(line)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " cursor line doesn't exist ?.\n");
#endif

    return 0;
  }

  if (!vt_term_get_vertical_mode(screen->term) || ignore_vertical_mode) {
    int row;

    if ((row = vt_term_cursor_row_in_screen(screen->term)) < 0) {
      return 0;
    }

    *x = convert_char_index_to_x_with_shape(screen, line, vt_term_cursor_char_index(screen->term));
    *y = convert_row_to_y(screen, row);
    if (return_line_bottom) {
      *y += ui_line_height(screen);
    }

    if (segment_offset > 0) {
      ui_font_manager_set_attr(screen->font_man, line->size_attr,
                               vt_line_has_ot_substitute_glyphs(line));

      i = 0;
      do {
        u_int width;

        width = ui_calculate_mlchar_width(ui_get_font(screen->font_man, vt_char_font(&chars[i])),
                                          &chars[i], NULL);

        *x += width;
        if (*x >= screen->width) {
          *x = 0;
          *y += ui_line_height(screen);
        }

        /* not count combining characters */
        comb_chars = vt_get_combining_chars(&chars[i], &comb_size);
        if (comb_chars) {
          i += comb_size;
        }
      } while (++i < segment_offset);
    }
  } else {
    *x = convert_char_index_to_x_with_shape(screen, line, vt_term_cursor_char_index(screen->term));
    *y = convert_row_to_y(screen, vt_term_cursor_row(screen->term));
    *x += ui_col_width(screen);

    if (segment_offset > 0) {
      int width;
      u_int height;

      width = ui_col_width(screen);
      if (vt_term_get_vertical_mode(screen->term) == VERT_RTL) {
        width = -width;
      }

      height = ui_line_height(screen);

      i = 0;
      do {
        *y += height;
        if (*y >= screen->height) {
          *x += width;
          *y = 0;
        }

        /* not count combining characters */
        comb_chars = vt_get_combining_chars(&chars[i], &comb_size);
        if (comb_chars) {
          i += comb_size;
        }
      } while (++i < segment_offset);
    }
  }

  return 1;
}

/*
 * this doesn't consider backscroll mode.
 */
static int get_spot(void *p, int *x, int *y) {
#ifndef  XIM_SPOT_IS_LINE_TOP
#ifdef USE_QUARTZ
  return get_spot_intern(p, NULL, 0, 1, 0, x, y);
#else
  return get_spot_intern(p, NULL, 0, 1, 1, x, y);
#endif
#else
  return get_spot_intern(p, NULL, 0, 0, 1, x, y);
#endif
}

static XFontSet get_fontset(void *p) {
  ui_screen_t *screen;

  screen = p;

  return ui_get_fontset(screen->font_man);
}

static ui_color_t *get_fg_color(void *p) {
  ui_screen_t *screen;

  screen = p;

  return ui_get_xcolor(screen->color_man, VT_FG_COLOR);
}

static ui_color_t *get_bg_color(void *p) {
  ui_screen_t *screen;

  screen = p;

  return ui_get_xcolor(screen->color_man, VT_BG_COLOR);
}

/*
 * callbacks of ui_im events.
 */

static int get_im_spot(void *p, vt_char_t *chars, int segment_offset, int *x, int *y) {
  ui_screen_t *screen = p;
  Window unused;

  if (!get_spot_intern(screen, chars, segment_offset, 1, 0, x, y)) {
    return 0;
  }

  ui_window_translate_coordinates(&screen->window, *x + screen->window.hmargin,
                                  *y + screen->window.vmargin, x, y, &unused);

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " im spot => x %d y %d\n", *x, *y);
#endif

  return 1;
}

static u_int get_line_height(void *p) { return ui_line_height((ui_screen_t *)p); }

static int is_vertical(void *p) {
  if (vt_term_get_vertical_mode(((ui_screen_t *)p)->term)) {
    return 1;
  } else {
    return 0;
  }
}

static void draw_preedit_str(void *p, vt_char_t *chars, u_int num_of_chars, int cursor_offset) {
  ui_screen_t *screen;
  vt_line_t *line;
  ui_font_t *xfont;
  int x;
  int y;
  u_int total_width;
  u_int i;
  u_int start;
  u_int beg_row;
  u_int end_row;
  int preedit_cursor_x;
  int preedit_cursor_y;

  screen = p;

  if (screen->is_preediting) {
    ui_im_t *im;

    vt_term_set_modified_lines_in_screen(screen->term, screen->im_preedit_beg_row,
                                         screen->im_preedit_end_row);

    /* Avoid recursive call of ui_im_redraw_preedit() in redraw_screen(). */
    im = screen->im;
    screen->im = NULL;
    ui_window_update(&screen->window, UPDATE_SCREEN);
    screen->im = im;
  }

  if (!num_of_chars) {
    screen->is_preediting = 0;

    return;
  }

  screen->is_preediting = 1;

  if ((line = vt_term_get_cursor_line(screen->term)) == NULL || vt_line_is_empty(line)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " cursor line doesn't exist ?.\n");
#endif

    return;
  }

  if (!vt_term_get_vertical_mode(screen->term)) {
    int row;

    row = vt_term_cursor_row_in_screen(screen->term);

    if (row < 0) {
      return;
    }

    beg_row = row;
  } else if (vt_term_get_vertical_mode(screen->term) == VERT_RTL) {
    u_int ncols;

    ncols = vt_term_get_cols(screen->term);
    beg_row = vt_term_cursor_col(screen->term);
    beg_row = ncols - beg_row - 1;
  } else /* VERT_LTR */
  {
    beg_row = vt_term_cursor_col(screen->term);
  }
  end_row = beg_row;

  x = convert_char_index_to_x_with_shape(screen, line, vt_term_cursor_char_index(screen->term));
  y = convert_row_to_y(screen, vt_term_cursor_row_in_screen(screen->term));

  preedit_cursor_x = x;
  preedit_cursor_y = y;

  total_width = 0;

#ifdef USE_WIN32GUI
  ui_set_gc(screen->window.gc, GetDC(screen->window.my_window));
#endif

  ui_font_manager_set_attr(screen->font_man, line->size_attr, 0);

  for (i = 0, start = 0; i < num_of_chars; i++) {
    u_int width;
    int need_wraparound = 0;
    int _x;
    int _y;

    xfont = ui_get_font(screen->font_man, vt_char_font(&chars[i]));
    width = ui_calculate_mlchar_width(xfont, &chars[i], NULL);

    total_width += width;

    if (!vt_term_get_vertical_mode(screen->term)) {
      if (x + total_width > screen->width) {
        need_wraparound = 1;
        _x = 0;
        _y = y + ui_line_height(screen);
        end_row++;
      }
    } else {
      need_wraparound = 1;
      _x = x;
      _y = y + ui_line_height(screen);
      start = i;

      if (_y > screen->height) {
        y = 0;
        _y = ui_line_height(screen);

        if (vt_term_get_vertical_mode(screen->term) == VERT_RTL) {
          x -= ui_col_width(screen);

          if (x < 0) {
            goto end;
          }
        } else /* VERT_LRT */
        {
          x += ui_col_width(screen);

          if (x >= screen->width) {
            goto end;
          }
        }
        _x = x;

        end_row++;
      }
    }

    if (i == cursor_offset - 1) {
      if (!vt_term_get_vertical_mode(screen->term)) {
        preedit_cursor_x = x + total_width;
        preedit_cursor_y = y;
      } else {
        preedit_cursor_x = x;
        preedit_cursor_y = _y;
      }
    }

    if (need_wraparound) {
      if (!ui_draw_str(&screen->window, screen->font_man, screen->color_man, &chars[start],
                       i - start + 1, x, y, ui_line_height(screen), ui_line_ascent(screen),
                       line_top_margin(screen), screen->hide_underline,
                       screen->underline_offset)) {
        break;
      }

      if (!vt_term_get_vertical_mode(screen->term) &&
          _y + ui_line_height(screen) > screen->height) {
        goto end;
      }

      x = _x;
      y = _y;
      start = i;
      total_width = width;
    }

    if (vt_term_get_vertical_mode(screen->term)) {
      continue;
    }

    if (i == num_of_chars - 1) /* last? */
    {
      if (!ui_draw_str(&screen->window, screen->font_man, screen->color_man, &chars[start],
                       i - start + 1, x, y, ui_line_height(screen), ui_line_ascent(screen),
                       line_top_margin(screen), screen->hide_underline,
                       screen->underline_offset)) {
        break;
      }
    }
  }

  if (cursor_offset == num_of_chars) {
    if (!vt_term_get_vertical_mode(screen->term)) {
      if ((preedit_cursor_x = x + total_width) == screen->width) {
        preedit_cursor_x--;
      }

      preedit_cursor_y = y;
    } else {
      preedit_cursor_x = x;

      if ((preedit_cursor_y = y) == screen->height) {
        preedit_cursor_y--;
      }
    }
  }

  if (cursor_offset >= 0) {
    if (!vt_term_get_vertical_mode(screen->term)) {
      ui_window_fill(&screen->window, preedit_cursor_x, preedit_cursor_y,
                     1, ui_line_height(screen));
    } else {
      ui_window_fill(&screen->window, preedit_cursor_x, preedit_cursor_y, ui_col_width(screen), 1);
    }
  }

end:
#ifdef USE_WIN32GUI
  ReleaseDC(screen->window.my_window, screen->window.gc->gc);
  ui_set_gc(screen->window.gc, None);
#endif

  screen->im_preedit_beg_row = beg_row;
  screen->im_preedit_end_row = end_row;
}

/* used for changing IM from plugin side */
static void im_changed(void *p, char *input_method) {
  ui_screen_t *screen;
  ui_im_t *new;

  screen = p;

  if (!(input_method = strdup(input_method))) {
    return;
  }

  if (!(new = im_new(screen))) {
    free(input_method);
    return;
  }

  free(screen->input_method);
  screen->input_method = input_method; /* strdup'ed one */

  ui_im_delete(screen->im);
  screen->im = new;
}

static int compare_key_state_with_modmap(void *p, u_int state, int *is_shift, int *is_lock,
                                         int *is_ctl, int *is_alt, int *is_meta, int *is_numlock,
                                         int *is_super, int *is_hyper) {
  ui_screen_t *screen;
  XModifierKeymap *mod_map;
  u_int mod_mask[] = {Mod1Mask, Mod2Mask, Mod3Mask, Mod4Mask, Mod5Mask};

  screen = p;

  if (is_shift) {
    *is_shift = 0;
  }
  if (is_lock) {
    *is_lock = 0;
  }
  if (is_ctl) {
    *is_ctl = 0;
  }
  if (is_alt) {
    *is_alt = 0;
  }
  if (is_meta) {
    *is_meta = 0;
  }
  if (is_numlock) {
    *is_numlock = 0;
  }
  if (is_super) {
    *is_super = 0;
  }
  if (is_hyper) {
    *is_hyper = 0;
  }

  if (is_shift && (state & ShiftMask)) {
    *is_shift = 1;
  }

  if (is_lock && (state & LockMask)) {
    *is_lock = 1;
  }

  if (is_ctl && (state & ControlMask)) {
    *is_ctl = 1;
  }

  if (!(mod_map = ui_window_get_modifier_mapping(&screen->window))) {
    /* Win32 or framebuffer */

    if (is_alt && (state & ModMask)) {
      *is_alt = 1;
    }
  } else {
    int i;

    for (i = 0; i < 5; i++) {
      int index;
      int mod1_index;

      if (!(state & mod_mask[i])) {
        continue;
      }

      /* skip shift/lock/control */
      mod1_index = mod_map->max_keypermod * 3;

      for (index = mod1_index + (mod_map->max_keypermod * i);
           index < mod1_index + (mod_map->max_keypermod * (i + 1)); index++) {
        KeySym sym;

        sym = XKeycodeToKeysym(screen->window.disp->display, mod_map->modifiermap[index], 0);

        switch (sym) {
          case XK_Meta_R:
          case XK_Meta_L:
            if (is_meta) {
              *is_meta = 1;
            }
            break;
          case XK_Alt_R:
          case XK_Alt_L:
            if (is_alt) {
              *is_alt = 1;
            }
            break;
          case XK_Super_R:
          case XK_Super_L:
            if (is_super) {
              *is_super = 1;
            }
            break;
          case XK_Hyper_R:
          case XK_Hyper_L:
            if (is_hyper) {
              *is_hyper = 1;
            }
            break;
          case XK_Num_Lock:
            if (is_numlock) {
              *is_numlock = 1;
            }
          default:
            break;
        }
      }
    }
  }

  return 1;
}

static void write_to_term(void *p, u_char *str, /* must be same as term encoding */
                          size_t len) {
  ui_screen_t *screen;

  screen = p;

#ifdef __DEBUG
  {
    size_t count;

    bl_debug_printf(BL_DEBUG_TAG " written str: ");

    for (count = 0; count < len; count++) {
      bl_msg_printf("%.2x ", str[count]);
    }
    bl_msg_printf("\n");
  }
#endif

  vt_term_write(screen->term, str, len);
}

/*
 * callbacks of vt_xterm_event_listener_t
 */

static void start_vt100_cmd(void *p) {
  ui_screen_t *screen;

  screen = p;

#if 0
  if (!vt_term_is_backscrolling(screen->term) ||
      vt_term_is_backscrolling(screen->term) == BSM_DEFAULT) {
    ui_stop_selecting(&screen->sel);
  }
#endif

  if (screen->sel.is_reversed) {
    if (ui_is_selecting(&screen->sel)) {
      ui_restore_selected_region_color_except_logs(&screen->sel);
    } else {
      ui_restore_selected_region_color(&screen->sel);
    }

    if (!vt_term_logical_visual_is_reversible(screen->term)) {
      /*
       * If vertical logical<=>visual conversion is enabled, ui_window_update()
       * in stop_vt100_cmd() can't reflect ui_restore_selected_region_color*()
       * functions above to screen.
       */
      ui_window_update(&screen->window, UPDATE_SCREEN);
    }
  }

  unhighlight_cursor(screen, 0);

  /*
   * vt_screen_logical() is called in vt_term_unhighlight_cursor(), so
   * not called directly from here.
   */

  /* processing_vtseq == -1 means loopback processing of vtseq. */
  if (screen->processing_vtseq != -1) {
    screen->processing_vtseq = 1;
  }
}

static void stop_vt100_cmd(void *p) {
  ui_screen_t *screen;

  screen = p;

  screen->processing_vtseq = 0;

  if (ui_is_selecting(&screen->sel)) {
    /*
     * XXX Fixme XXX
     * If some lines are scrolled out after start_vt100_cmd(),
     * color of them is not reversed.
     */
    ui_reverse_selected_region_color_except_logs(&screen->sel);
  }

  if (exit_backscroll_by_pty) {
    exit_backscroll_mode(screen);
  }

  if ((screen->font_or_color_config_updated & 0x1) &&
      screen->system_listener->font_config_updated) {
    (*screen->system_listener->font_config_updated)();
  }

  if ((screen->font_or_color_config_updated & 0x2) &&
      screen->system_listener->color_config_updated) {
    (*screen->system_listener->color_config_updated)();
  }

  screen->font_or_color_config_updated = 0;

  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
}

static void interrupt_vt100_cmd(void *p) {
  ui_screen_t *screen;

  screen = p;

  ui_window_update(&screen->window, UPDATE_SCREEN);

  /* Forcibly reflect to the screen. */
  ui_display_sync(screen->window.disp);
}

static void xterm_resize(void *p, u_int width, u_int height) {
  ui_screen_t *screen;

  screen = p;

  if (width == 0 && height == 0) {
    /* vt_term_t is already resized. */
    resize_window(screen);

    return;
  }

  if (width == 0) {
    width = screen->width;
  }
  if (height == 0) {
    height = screen->height;
  }

  if (vt_term_has_status_line(screen->term)) {
    height += ui_line_height(screen);
  }

  /* screen will redrawn in window_resized() */
  if (ui_window_resize(&screen->window, width, height, NOTIFY_TO_PARENT | LIMIT_RESIZE)) {
    /*
     * !! Notice !!
     * ui_window_resize() will invoke ConfigureNotify event but window_resized()
     * won't be called , since xconfigure.width , xconfigure.height are the same
     * as the already resized window.
     */
    if (screen->window.window_resized) {
      (*screen->window.window_resized)(&screen->window);
    }
  }
}

static void xterm_reverse_video(void *p, int do_reverse) {
  ui_screen_t *screen;

  screen = p;

  if (do_reverse) {
    if (!ui_color_manager_reverse_video(screen->color_man)) {
      return;
    }
  } else {
    if (!ui_color_manager_restore_video(screen->color_man)) {
      return;
    }
  }

  ui_window_set_fg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_FG_COLOR));
  ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR));

  vt_term_set_modified_all_lines_in_screen(screen->term);

  ui_window_update(&screen->window, UPDATE_SCREEN);
}

static void xterm_set_mouse_report(void *p) {
  ui_screen_t *screen;

  screen = p;

  if (vt_term_get_mouse_report_mode(screen->term)) {
/* These are called in button_pressed() etc. */
#if 0
    ui_stop_selecting(&screen->sel);
    restore_selected_region_color_instantly(screen);
    exit_backscroll_mode(screen);
#endif
  } else {
    screen->prev_mouse_report_col = screen->prev_mouse_report_row = 0;
  }

  if (vt_term_get_mouse_report_mode(screen->term) < ANY_EVENT_MOUSE_REPORT) {
    /* pointer_motion may be overridden by ui_layout */
    if (screen->window.pointer_motion == pointer_motion) {
      ui_window_remove_event_mask(&screen->window, PointerMotionMask);
    }
  } else {
    ui_window_add_event_mask(&screen->window, PointerMotionMask);
  }
}

static void xterm_request_locator(void *p) {
  ui_screen_t *screen;
  int button;
  int button_state;

  screen = p;

  if (screen->window.button_is_pressing) {
    button = screen->window.prev_clicked_button;
    button_state = (1 << (button - Button1));
  } else {
    /* PointerMotion */
    button = button_state = 0;
  }

  vt_term_report_mouse_tracking(
      screen->term, screen->prev_mouse_report_col > 0 ? screen->prev_mouse_report_col : 1,
      screen->prev_mouse_report_row > 0 ? screen->prev_mouse_report_row : 1, button, 0, 0,
      button_state);
}

static void xterm_bel(void *p) {
  ui_screen_t *screen;

  screen = p;

  ui_window_bell(&screen->window, screen->bel_mode);
}

static int xterm_im_is_active(void *p) {
  ui_screen_t *screen;

  screen = p;

  if (screen->im) {
    return (*screen->im->is_active)(screen->im);
  }

  return ui_xic_is_active(&screen->window);
}

static void xterm_switch_im_mode(void *p) {
  ui_screen_t *screen;

  screen = p;

  if (screen->im) {
    (*screen->im->switch_mode)(screen->im);

    return;
  }

  ui_xic_switch_mode(&screen->window);
}

static void xterm_set_selection(void *p,
                                vt_char_t *str, /* Should be free'ed by the event listener. */
                                u_int len, u_char *targets) {
  ui_screen_t *screen;
  int use_clip_orig;

  screen = p;

  use_clip_orig = ui_is_using_clipboard_selection();

  if (strchr(targets, 'c')) {
    if (!use_clip_orig) {
      ui_set_use_clipboard_selection(1);
    }
  } else if (!strchr(targets, 's') && strchr(targets, 'p')) {
    /* 'p' is specified while 'c' and 's' aren't specified. */

    if (use_clip_orig) {
      ui_set_use_clipboard_selection(0);
    }
  }

  if (ui_window_set_selection_owner(&screen->window, CurrentTime)) {
    if (screen->sel.sel_str) {
      vt_str_delete(screen->sel.sel_str, screen->sel.sel_len);
    }

    screen->sel.sel_str = str;
    screen->sel.sel_len = len;
  }

  if (use_clip_orig != ui_is_using_clipboard_selection()) {
    ui_set_use_clipboard_selection(use_clip_orig);
  }
}

static int xterm_get_rgb(void *p, u_int8_t *red, u_int8_t *green, u_int8_t *blue,
                         vt_color_t color) {
  ui_screen_t *screen;
  ui_color_t *xcolor;

  screen = p;

  if (!(xcolor = ui_get_xcolor(screen->color_man, color))) {
    return 0;
  }

  ui_get_xcolor_rgba(red, green, blue, NULL, xcolor);

  return 1;
}

static int xterm_get_window_size(void *p, u_int *width, u_int *height) {
  ui_screen_t *screen;

  screen = p;

  *width = screen->width;
  *height = screen->height;

  if (vt_term_has_status_line(screen->term)) {
    *height -= ui_line_height(screen);
  }

  return 1;
}

#ifndef NO_IMAGE
static vt_char_t *xterm_get_picture_data(void *p, char *file_path, int *num_of_cols, /* can be 0 */
                                         int *num_of_rows,                           /* can be 0 */
                                         u_int32_t **sixel_palette) {
  ui_screen_t *screen;
  u_int width;
  u_int height;
  u_int col_width;
  u_int line_height;
  int idx;

  screen = p;

  if (vt_term_get_vertical_mode(screen->term)) {
    return NULL;
  }

  width = (*num_of_cols) *(col_width = ui_col_width(screen));
  height = (*num_of_rows) *(line_height = ui_line_height(screen));

  if (sixel_palette) {
    *sixel_palette = ui_set_custom_sixel_palette(*sixel_palette);
  }

  if ((idx = ui_load_inline_picture(screen->window.disp, file_path, &width, &height, col_width,
                                    line_height, screen->term)) != -1) {
    vt_char_t *buf;
    int max_num_of_cols;

    screen->prev_inline_pic = idx;

    max_num_of_cols =
        vt_term_get_cursor_line(screen->term)->num_of_chars - vt_term_cursor_col(screen->term);
    if ((*num_of_cols = (width + col_width - 1) / col_width) > max_num_of_cols) {
      *num_of_cols = max_num_of_cols;
    }

    *num_of_rows = (height + line_height - 1) / line_height;

    if ((buf = vt_str_new((*num_of_cols) * (*num_of_rows)))) {
      vt_char_t *buf_p;
      int col;
      int row;

      buf_p = buf;
      for (row = 0; row < *num_of_rows; row++) {
        for (col = 0; col < *num_of_cols; col++) {
          vt_char_copy(buf_p, vt_sp_ch());

          vt_char_combine_picture(buf_p++, idx, MAKE_INLINEPIC_POS(col, row, *num_of_rows));
        }
      }

      return buf;
    }
  }

  return NULL;
}

static int xterm_get_emoji_data(void *p, vt_char_t *ch1, vt_char_t *ch2) {
  ui_screen_t *screen;
  char *file_path;
  u_int width;
  u_int height;
  int idx;

  screen = p;

  width = ui_col_width(screen) * vt_char_cols(ch1);
  height = ui_line_height(screen);

  if ((file_path = alloca(18 + DIGIT_STR_LEN(u_int32_t) * 2 + 1))) {
    if (ch2) {
      sprintf(file_path, "mlterm/emoji/%x-%x.gif", vt_char_code(ch1), vt_char_code(ch2));
    } else {
      sprintf(file_path, "mlterm/emoji/%x.gif", vt_char_code(ch1));
    }

    if ((file_path = bl_get_user_rc_path(file_path))) {
      struct stat st;

      if (stat(file_path, &st) != 0) {
        strcpy(file_path + strlen(file_path) - 3, "png");
      }

      idx = ui_load_inline_picture(screen->window.disp, file_path, &width, &height,
                                   width / vt_char_cols(ch1), height, screen->term);
      free(file_path);

      if (idx != -1) {
        vt_char_combine_picture(ch1, idx, 0);

        return 1;
      }
    }
  }

  return 0;
}

#ifndef DONT_OPTIMIZE_DRAWING_PICTURE
static void xterm_show_sixel(void *p, char *file_path) {
  ui_screen_t *screen;
  Pixmap pixmap;
  PixmapMask mask;
  u_int width;
  u_int height;

  screen = p;

  if (!vt_term_get_vertical_mode(screen->term) &&
      ui_load_tmp_picture(screen->window.disp, file_path, &pixmap, &mask, &width, &height)) {
    ui_window_copy_area(&screen->window, pixmap, mask, 0, 0, width, height, 0, 0);
    ui_delete_tmp_picture(screen->window.disp, pixmap, mask);

    /*
     * ui_display_sync() is not necessary here because xterm_show_sixel()
     * is never called by DECINVM.
     */
  }
}
#else
#define xterm_show_sixel NULL
#endif

static void xterm_add_frame_to_animation(void *p, char *file_path, int *num_of_cols,
                                         int *num_of_rows) {
  ui_screen_t *screen;
  u_int width;
  u_int height;
  u_int col_width;
  u_int line_height;
  int idx;

  screen = p;

  if (vt_term_get_vertical_mode(screen->term) || screen->prev_inline_pic < 0) {
    return;
  }

  width = (*num_of_cols) *(col_width = ui_col_width(screen));
  height = (*num_of_rows) *(line_height = ui_line_height(screen));

  if ((idx = ui_load_inline_picture(screen->window.disp, file_path, &width, &height, col_width,
                                    line_height, screen->term)) != -1 &&
      screen->prev_inline_pic != idx) {
    ui_add_frame_to_animation(screen->prev_inline_pic, idx);
    screen->prev_inline_pic = idx;
  }
}
#else
#define xterm_get_picture_data NULL
#define xterm_get_emoji_data NULL
#define xterm_show_sixel NULL
#define xterm_add_frame_to_animation NULL
#endif /* NO_IMAGE */

static void xterm_hide_cursor(void *p, int hide) {
  ui_screen_t *screen;

  screen = p;

  if (hide) {
    ui_window_set_cursor(&screen->window, XC_nil);
  } else {
    ui_window_set_cursor(&screen->window, XC_xterm);
  }
}

static int xterm_check_iscii_font(void *p, ef_charset_t cs) {
  return ui_font_cache_get_xfont(((ui_screen_t *)p)->font_man->font_cache,
                                 NORMAL_FONT_OF(cs)) != NULL;
}

/*
 * callbacks of vt_pty_event_listener_t
 */

static void pty_closed(void *p) {
  ui_screen_t *screen;

  screen = p;

  /*
   * Don't use ui_screen_detach() here because screen->term is deleting just
   * now.
   */

  /* This should be done before screen->term is NULL */
  ui_sel_clear(&screen->sel);

  /*
   * term is being deleted in this context.
   * vt_close_dead_terms => vt_term_delete => vt_pty_delete => pty_closed.
   */
  screen->term = NULL;

  (*screen->system_listener->pty_closed)(screen->system_listener->self, screen);
}

static void show_config(void *p, char *msg) { vt_term_show_message(((ui_screen_t *)p)->term, msg); }

/* --- global functions --- */

void ui_exit_backscroll_by_pty(int flag) { exit_backscroll_by_pty = flag; }

void ui_allow_change_shortcut(int flag) { allow_change_shortcut = flag; }

void ui_set_mod_meta_prefix(char *prefix /* allocated memory */
                            ) {
  static int was_replaced;

  if (was_replaced) {
    free(mod_meta_prefix);
  } else {
    was_replaced = 1;
  }

  if ((mod_meta_prefix = prefix) == NULL) {
    mod_meta_prefix = "\x1b";
    was_replaced = 0;
  }
}

void ui_set_trim_trailing_newline_in_pasting(int trim) { trim_trailing_newline_in_pasting = trim; }

#ifdef USE_IM_CURSOR_COLOR
void ui_set_im_cursor_color(char *color) { im_cursor_color = strdup(color); }
#endif

/*
 * If term is NULL, don't call other functions of ui_screen until
 * ui_screen_attach() is called. (ui_screen_attach() must be called
 * before ui_screen_t is realized.)
 */
ui_screen_t *ui_screen_new(vt_term_t *term, /* can be NULL */
                           ui_font_manager_t *font_man, ui_color_manager_t *color_man,
                           u_int brightness, u_int contrast, u_int gamma, u_int alpha,
                           u_int fade_ratio, ui_shortcut_t *shortcut, u_int screen_width_ratio,
                           char *mod_meta_key,
                           ui_mod_meta_mode_t mod_meta_mode, ui_bel_mode_t bel_mode,
                           int receive_string_via_ucs, char *pic_file_path, int use_transbg,
                           int use_vertical_cursor,
                           int use_extended_scroll_shortcut, int borderless, int line_space,
                           char *input_method, int allow_osc52, u_int hmargin,
                           u_int vmargin, int hide_underline, int underline_offset,
                           int baseline_offset) {
  ui_screen_t *screen;
  u_int col_width;
  u_int line_height;

#ifdef USE_OT_LAYOUT
  vt_ot_layout_set_shape_func(ui_convert_text_to_glyphs, ot_layout_get_ot_layout_font);
#endif

  if ((screen = calloc(1, sizeof(ui_screen_t))) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif

    return NULL;
  }

  screen->use_vertical_cursor = use_vertical_cursor;
  screen->font_man = font_man;
  screen->color_man = color_man;

  /* letter_space, line_space and baseline_offset are ignored on console. */
#ifndef USE_CONSOLE
  screen->line_space = line_space;
  screen->baseline_offset = baseline_offset;
#endif
  screen->underline_offset = underline_offset;
  modify_line_space_and_offset(screen);

  screen->sel_listener.self = screen;
  screen->sel_listener.select_in_window = select_in_window;
  screen->sel_listener.reverse_color = reverse_color;
  screen->sel_listener.restore_color = restore_color;

  ui_sel_init(&screen->sel, &screen->sel_listener);

  if (pic_file_path) {
    screen->pic_file_path = strdup(pic_file_path);
  }

  screen->pic_mod.brightness = brightness;
  screen->pic_mod.contrast = contrast;
  screen->pic_mod.gamma = gamma;

/*
 * blend_xxx members will be set in window_realized().
 */
#if 0
  ui_get_xcolor_rgba(&screen->pic_mod.blend_red, &screen->pic_mod.blend_green,
                     &screen->pic_mod.blend_blue, NULL,
                     ui_get_xcolor(screen->color_man, VT_BG_COLOR));
#endif

  if (alpha != 255) {
    screen->pic_mod.alpha = alpha;
  }

  /* True transparency is disabled on pseudo transparency or wall picture. */
  if (!use_transbg && !pic_file_path) {
    ui_change_true_transbg_alpha(color_man, alpha);
  }

  screen->fade_ratio = fade_ratio;

  screen->screen_width_ratio = screen_width_ratio;

  /* screen->term must be set before screen_height() */
  screen->term = term;

  col_width = ui_col_width(screen);
  line_height = ui_line_height(screen);

  /* min: 1x1 */
  if (!ui_window_init(&screen->window,
                      (screen->width = screen->term ? screen_width(screen) : col_width),
                      (screen->height = screen->term ? screen_height(screen) : line_height),
                      col_width, line_height, col_width, line_height, hmargin, vmargin, 0, 1)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_window_init failed.\n");
#endif

    goto error;
  }

  screen->screen_listener.self = screen;
  screen->screen_listener.window_scroll_upward_region = window_scroll_upward_region;
  screen->screen_listener.window_scroll_downward_region = window_scroll_downward_region;
  screen->screen_listener.line_scrolled_out = line_scrolled_out;

  screen->xterm_listener.self = screen;
  screen->xterm_listener.start = start_vt100_cmd;
  screen->xterm_listener.stop = stop_vt100_cmd;
  screen->xterm_listener.interrupt = interrupt_vt100_cmd;
  screen->xterm_listener.resize = xterm_resize;
  screen->xterm_listener.reverse_video = xterm_reverse_video;
  screen->xterm_listener.set_mouse_report = xterm_set_mouse_report;
  screen->xterm_listener.request_locator = xterm_request_locator;
  screen->xterm_listener.set_window_name = ui_set_window_name;
  screen->xterm_listener.set_icon_name = ui_set_icon_name;
  screen->xterm_listener.bel = xterm_bel;
  screen->xterm_listener.im_is_active = xterm_im_is_active;
  screen->xterm_listener.switch_im_mode = xterm_switch_im_mode;
  screen->xterm_listener.set_selection = (allow_osc52 ? xterm_set_selection : NULL);
  screen->xterm_listener.get_rgb = xterm_get_rgb;
  screen->xterm_listener.get_window_size = xterm_get_window_size;
  screen->xterm_listener.get_picture_data = xterm_get_picture_data;
  screen->xterm_listener.get_emoji_data = xterm_get_emoji_data;
  screen->xterm_listener.show_sixel = xterm_show_sixel;
  screen->xterm_listener.add_frame_to_animation = xterm_add_frame_to_animation;
  screen->xterm_listener.hide_cursor = xterm_hide_cursor;
  screen->xterm_listener.check_iscii_font = xterm_check_iscii_font;

  screen->config_listener.self = screen;
  screen->config_listener.exec = ui_screen_exec_cmd;
  screen->config_listener.set = ui_screen_set_config;
  screen->config_listener.get = get_config;
  screen->config_listener.saved = NULL;
  screen->config_listener.set_font = set_font_config;
  screen->config_listener.get_font = get_font_config;
  screen->config_listener.set_color = set_color_config;
  screen->config_listener.get_color = get_color_config;

  screen->pty_listener.self = screen;
  screen->pty_listener.closed = pty_closed;
  screen->pty_listener.show_config = show_config;

  if (screen->term) {
    vt_term_attach(term, &screen->xterm_listener, &screen->config_listener,
                   &screen->screen_listener, &screen->pty_listener);

    /*
     * setup_encoding_aux() in update_special_visual() must be called
     * after ui_window_init()
     */
    update_special_visual(screen);
  }

  screen->xim_listener.self = screen;
  screen->xim_listener.get_spot = get_spot;
  screen->xim_listener.get_fontset = get_fontset;
  screen->xim_listener.get_fg_color = get_fg_color;
  screen->xim_listener.get_bg_color = get_bg_color;
  screen->window.xim_listener = &screen->xim_listener;

  if (input_method) {
    screen->input_method = strdup(input_method);
  }

  screen->im_listener.self = screen;
  screen->im_listener.get_spot = get_im_spot;
  screen->im_listener.get_line_height = get_line_height;
  screen->im_listener.is_vertical = is_vertical;
  screen->im_listener.draw_preedit_str = draw_preedit_str;
  screen->im_listener.im_changed = im_changed;
  screen->im_listener.compare_key_state_with_modmap = compare_key_state_with_modmap;
  screen->im_listener.write_to_term = write_to_term;

  ui_window_set_cursor(&screen->window, XC_xterm);

  /*
   * event call backs.
   */

  ui_window_add_event_mask(&screen->window,
                           ButtonPressMask | ButtonMotionMask | ButtonReleaseMask | KeyPressMask);

  screen->window.window_realized = window_realized;
  screen->window.window_finalized = window_finalized;
  screen->window.window_deleted = window_deleted;
  screen->window.mapping_notify = mapping_notify;
  screen->window.window_exposed = window_exposed;
  screen->window.update_window = update_window;
  screen->window.window_focused = window_focused;
  screen->window.window_unfocused = window_unfocused;
  screen->window.key_pressed = key_pressed;
  screen->window.window_resized = window_resized;
  screen->window.pointer_motion = pointer_motion;
  screen->window.button_motion = button_motion;
  screen->window.button_released = button_released;
  screen->window.button_pressed = button_pressed;
  screen->window.button_press_continued = button_press_continued;
  screen->window.selection_cleared = selection_cleared;
  screen->window.xct_selection_requested = xct_selection_requested;
  screen->window.utf_selection_requested = utf_selection_requested;
  screen->window.xct_selection_notified = xct_selection_notified;
  screen->window.utf_selection_notified = utf_selection_notified;
#ifndef DISABLE_XDND
  screen->window.set_xdnd_config = set_xdnd_config;
#endif
  screen->window.idling = idling;

  if (use_transbg) {
    ui_window_set_transparent(&screen->window, ui_screen_get_picture_modifier(screen));
  }

  screen->shortcut = shortcut;

  if (mod_meta_key && strcmp(mod_meta_key, "none") != 0) {
    screen->mod_meta_key = strdup(mod_meta_key);
  }

  screen->mod_meta_mode = mod_meta_mode;
  screen->mod_meta_mask = 0;    /* set later in get_mod_meta_mask() */
  screen->mod_ignore_mask = ~0; /* set later in get_mod_ignore_mask() */

  screen->bel_mode = bel_mode;
  screen->use_extended_scroll_shortcut = use_extended_scroll_shortcut;
  screen->borderless = borderless;
  screen->font_or_color_config_updated = 0;
  screen->hide_underline = hide_underline;
  screen->prev_inline_pic = -1;
  screen->receive_string_via_ucs = receive_string_via_ucs;

  if (!vt_str_parser) {
    if (!(vt_str_parser = vt_str_parser_new())) {
      goto error;
    }
  }

  return screen;

error:
  free(screen->pic_file_path);
  free(screen->mod_meta_key);
  free(screen->input_method);
  free(screen);

  return NULL;
}

void ui_screen_delete(ui_screen_t *screen) {
  if (screen->term) {
    vt_term_detach(screen->term);
  }

  ui_sel_final(&screen->sel);

  if (screen->bg_pic) {
    ui_release_picture(screen->bg_pic);
  }
  free(screen->pic_file_path);

  if (screen->icon) {
    ui_release_icon_picture(screen->icon);
  }

  free(screen->mod_meta_key);
  free(screen->input_method);

  if (screen->im) {
    ui_im_delete(screen->im);
  }

  free(screen);
}

/*
 * Be careful that mlterm can die if ui_screen_attach is called
 * before ui_screen_t is realized, because callbacks of vt_term
 * may touch uninitialized object of ui_screen_t.
 */
int ui_screen_attach(ui_screen_t *screen, vt_term_t *term) {
  if (screen->term) {
    return 0;
  }

  screen->term = term;

  vt_term_attach(term, &screen->xterm_listener, &screen->config_listener, &screen->screen_listener,
                 &screen->pty_listener);

  if (!screen->window.my_window) {
    return 1;
  }

  if (!usascii_font_cs_changed(screen, vt_term_get_encoding(screen->term))) {
    resize_window(screen);
  }

  update_special_visual(screen);
  /* Even if update_special_visual succeeded or not, all screen should be
   * redrawn. */
  vt_term_set_modified_all_lines_in_screen(screen->term);

  if (HAS_SCROLL_LISTENER(screen, term_changed)) {
    (*screen->screen_scroll_listener->term_changed)(screen->screen_scroll_listener->self,
                                                    vt_term_get_log_size(screen->term),
                                                    vt_term_get_num_of_logged_lines(screen->term));
  }

  /*
   * if vt_term_(icon|window)_name() returns NULL, screen->window.app_name
   * will be used in ui_set_(icon|window)_name().
   */
  ui_set_window_name(&screen->window, vt_term_window_name(screen->term));
  ui_set_icon_name(&screen->window, vt_term_icon_name(screen->term));

  /* reset icon to screen->term's one */
  set_icon(screen);

  if (screen->im) {
    ui_im_t *im;

    im = screen->im;
    screen->im = im_new(screen);
    /*
     * Avoid to delete anything inside im-module by calling ui_im_delete()
     * after ui_im_new().
     */
    ui_im_delete(im);
  }

  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);

  return 1;
}

vt_term_t *ui_screen_detach(ui_screen_t *screen) {
  vt_term_t *term;

  if (screen->term == NULL) {
    return NULL;
  }

  /* This should be done before screen->term is NULL */
  ui_sel_clear(&screen->sel);

#if 1
  exit_backscroll_mode(screen);
#endif

  vt_term_detach(screen->term);

  term = screen->term;
  screen->term = NULL;

  return term;
}

int ui_screen_attached(ui_screen_t *screen) { return (screen->term != NULL); }

void ui_set_system_listener(ui_screen_t *screen, ui_system_event_listener_t *system_listener) {
  screen->system_listener = system_listener;
}

void ui_set_screen_scroll_listener(ui_screen_t *screen,
                                   ui_screen_scroll_event_listener_t *screen_scroll_listener) {
  screen->screen_scroll_listener = screen_scroll_listener;
}

/*
 * for scrollbar scroll.
 *
 * Similar processing is done in bs_xxx().
 */

void ui_screen_scroll_upward(ui_screen_t *screen, u_int size) {
  if (!vt_term_is_backscrolling(screen->term)) {
    enter_backscroll_mode(screen);
  }

  vt_term_backscroll_upward(screen->term, size);

  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
}

void ui_screen_scroll_downward(ui_screen_t *screen, u_int size) {
  if (!vt_term_is_backscrolling(screen->term)) {
    enter_backscroll_mode(screen);
  }

  vt_term_backscroll_downward(screen->term, size);

  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
}

void ui_screen_scroll_to(ui_screen_t *screen, int row) {
  if (!vt_term_is_backscrolling(screen->term)) {
    enter_backscroll_mode(screen);
  }

  vt_term_backscroll_to(screen->term, row);

  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
}

u_int ui_col_width(ui_screen_t *screen) { return ui_get_usascii_font(screen->font_man)->width; }

u_int ui_line_height(ui_screen_t *screen) {
  return ui_get_usascii_font(screen->font_man)->height + screen->line_space;
}

u_int ui_line_ascent(ui_screen_t *screen) {
  return ui_get_usascii_font(screen->font_man)->ascent + line_top_margin(screen) +
         screen->baseline_offset;
}

/*
 * Return value
 *  0 -> Not processed
 *  1 -> Processed (regardless of processing succeeded or not)
 */
int ui_screen_exec_cmd(ui_screen_t *screen, char *cmd) {
  char *arg;

  if (vt_term_exec_cmd(screen->term, cmd)) {
    return 1;
  } else if (strcmp(cmd, "mlconfig") == 0) {
    start_menu(screen, cmd, screen->window.width / 2, screen->window.height / 2);

    return 1;
  } else if (strncmp(cmd, "mlclient", 8) == 0) {
    if (HAS_SYSTEM_LISTENER(screen, mlclient)) {
      /*
       * processing_vtseq == -1: process vtseq in loopback.
       * processing_vtseq == 0 : stop processing vtseq.
       */
      if (screen->processing_vtseq > 0) {
        char *ign_opts[] = {
            "-e",   "-initstr", "-#", "-osc52", "-shortcut",
#ifdef USE_LIBSSH2
            "-scp",
#endif
        };
        char *p;
        size_t count;

        /*
         * Executing value of "-e" or "--initstr" option is dangerous
         * in case 'cat dangerousfile'.
         */

        for (count = 0; count < sizeof(ign_opts) / sizeof(ign_opts[0]); count++) {
          if ((p = strstr(cmd, ign_opts[count])) &&
              (count > 0 || /* not -e option, or */
               (p[2] < 'A'  /* not match --extkey, --exitbs */
#if 1
                /*
                 * XXX for mltracelog.sh which executes
                 * mlclient -e cat.
                 * 3.1.9 or later : mlclient "-e" "cat"
                 * 3.1.8 or before: mlclient "-e"  "cat"
                 */
                &&
                strcmp(p + 4, "\"cat\"") != 0 && strcmp(p + 5, "\"cat\"") != 0
                /* for w3m */
                &&
                strncmp(p + 4, "\"w3m\"", 5) != 0
#endif
                ))) {
            if (p[-1] == '-') {
              p--;
            }

            bl_msg_printf(
                "Remove %s "
                "from mlclient args.\n",
                p - 1);

            p[-1] = '\0'; /* Replace ' ', '\"' or '\''. */
          }
        }
      }

      (*screen->system_listener->mlclient)(screen->system_listener->self,
                                           cmd[8] == 'x' ? screen : NULL, cmd, stdout);
    }

    return 1;
  }

  /* Separate cmd to command string and argument string. */
  if ((arg = strchr(cmd, ' '))) {
    /*
     * If cmd is not matched below, *arg will be restored as ' '
     * at the end of this function.
     */
    *arg = '\0';

    while (*(++arg) == ' ')
      ;
    if (*arg == '\0') {
      arg = NULL;
    }
  }

  /*
   * Backward compatibility with mlterm 3.0.10 or before which accepts
   * '=' like "paste=" is broken.
   */

  if (strcmp(cmd, "paste") == 0) {
    /*
     * for vte.c
     *
     * processing_vtseq == -1: process vtseq in loopback.
     * processing_vtseq == 0 : stop processing vtseq.
     */
    if (screen->processing_vtseq <= 0) {
      yank_event_received(screen, CurrentTime);
    }
  } else if (strcmp(cmd, "open_pty") == 0 || strcmp(cmd, "select_pty") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, open_pty)) {
      /* arg is not NULL if cmd == "select_pty" */
      (*screen->system_listener->open_pty)(screen->system_listener->self, screen, arg);
    }
  } else if (strcmp(cmd, "next_pty") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, next_pty)) {
      (*screen->system_listener->next_pty)(screen->system_listener->self, screen);
    }
  } else if (strcmp(cmd, "prev_pty") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, prev_pty)) {
      (*screen->system_listener->prev_pty)(screen->system_listener->self, screen);
    }
  } else if (strcmp(cmd, "close_pty") == 0) {
    /*
     * close_pty is useful if pty doesn't react anymore and
     * you want to kill it forcibly.
     */

    if (HAS_SYSTEM_LISTENER(screen, close_pty)) {
      /* If arg is NULL, screen->term will be closed. */
      (*screen->system_listener->close_pty)(screen->system_listener->self, screen, arg);
    }
  } else if (strcmp(cmd, "open_screen") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, open_screen)) {
      (*screen->system_listener->open_screen)(screen->system_listener->self, screen);
    }
  } else if (strcmp(cmd, "close_screen") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, close_screen)) {
      (*screen->system_listener->close_screen)(screen->system_listener->self, screen, 0);
    }
  } else if (strcmp(cmd + 1, "split_screen") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, split_screen)) {
      (*screen->system_listener->split_screen)(screen->system_listener->self, screen, *cmd == 'h',
                                               arg);
    }
  } else if (strcmp(cmd + 1, "resize_screen") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, resize_screen)) {
      int step;

      if (bl_str_to_int(&step, arg + (*arg == '+' ? 1 : 0))) {
        (*screen->system_listener->resize_screen)(screen->system_listener->self, screen,
                                                  *cmd == 'h', step);
      }
    }
  } else if (strcmp(cmd, "next_screen") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, next_screen)) {
      (*screen->system_listener->next_screen)(screen->system_listener->self, screen);
    }
  } else if (strcmp(cmd, "prev_screen") == 0) {
    if (HAS_SYSTEM_LISTENER(screen, prev_screen)) {
      (*screen->system_listener->prev_screen)(screen->system_listener->self, screen);
    }
  } else if (strncmp(cmd, "search_", 7) == 0) {
    vt_char_encoding_t encoding;

    if (arg && (encoding = vt_term_get_encoding(screen->term)) != VT_UTF8) {
      char *p;
      size_t len;

      len = UTF_MAX_SIZE * strlen(arg) + 1;
      if ((p = alloca(len))) {
        *(p + vt_char_encoding_convert(p, len - 1, VT_UTF8, arg, strlen(arg), encoding)) = '\0';

        arg = p;
      }
    }

    if (strcmp(cmd + 7, "prev") == 0) {
      search_find(screen, arg, 1);
    } else if (strcmp(cmd + 7, "next") == 0) {
      search_find(screen, arg, 0);
    }
  } else if (strcmp(cmd, "update_all") == 0) {
    ui_window_update_all(ui_get_root_window(&screen->window));
  } else if (strcmp(cmd, "set_shortcut") == 0) {
    /*
     * processing_vtseq == -1: process vtseq in loopback.
     * processing_vtseq == 0 : stop processing vtseq.
     */
    if (screen->processing_vtseq <= 0 || allow_change_shortcut) {
      char *opr;

      if (arg && (opr = strchr(arg, '='))) {
        *(opr++) = '\0';
        ui_shortcut_parse(screen->shortcut, arg, opr);
      }
    }
  } else {
    if (arg) {
      *(cmd + strlen(cmd)) = ' ';
    }

    return 0;
  }

  return 1;
}

/*
 * Return value
 *  0 -> Not processed
 *  1 -> Processed (regardless of processing succeeded or not)
 */
int ui_screen_set_config(ui_screen_t *screen, char *dev, /* can be NULL */
                         char *key, char *value          /* can be NULL */
                         ) {
  vt_term_t *term;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " %s=%s\n", key, value);
#endif

  if (strcmp(value, "switch") == 0) {
    int flag;

    get_config_intern(screen, /* dev */ NULL, key, -1, &flag);

    if (flag == 1) {
      value = "false";
    } else if (flag == 0) {
      value = "true";
    }
  }

  if (dev) {
    /*
     * XXX
     * If vt_term_t isn't attached to ui_screen_t, return 0 because
     * many static functions used below use screen->term internally.
     */
    if ((term = vt_get_term(dev)) && vt_term_is_attached(term)) {
      screen = term->parser->config_listener->self;
    } else {
      return 0;
    }
  } else {
    term = screen->term;
  }

  if (term && /* In case term is not attached yet. (for vte.c) */
      vt_term_set_config(term, key, value)) {
    /* do nothing */
  } else if (strcmp(key, "encoding") == 0) {
    vt_char_encoding_t encoding;

    if ((encoding = vt_get_char_encoding(value)) != VT_UNKNOWN_ENCODING) {
      change_char_encoding(screen, encoding);
    }
  } else if (strcmp(key, "fg_color") == 0) {
    change_fg_color(screen, value);
  } else if (strcmp(key, "bg_color") == 0) {
    change_bg_color(screen, value);
  } else if (strcmp(key, "cursor_fg_color") == 0) {
    ui_color_manager_set_cursor_fg_color(screen->color_man, *value == '\0' ? NULL : value);
  } else if (strcmp(key, "cursor_bg_color") == 0) {
    ui_color_manager_set_cursor_bg_color(screen->color_man, *value == '\0' ? NULL : value);
  } else if (strcmp(key, "bd_color") == 0) {
    change_alt_color(screen, VT_BOLD_COLOR, value);
  } else if (strcmp(key, "it_color") == 0) {
    change_alt_color(screen, VT_ITALIC_COLOR, value);
  } else if (strcmp(key, "ul_color") == 0) {
    change_alt_color(screen, VT_UNDERLINE_COLOR, value);
  } else if (strcmp(key, "bl_color") == 0) {
    change_alt_color(screen, VT_BLINKING_COLOR, value);
  } else if (strcmp(key, "co_color") == 0) {
    change_alt_color(screen, VT_CROSSED_OUT_COLOR, value);
  } else if (strcmp(key, "sb_fg_color") == 0) {
    change_sb_fg_color(screen, value);
  } else if (strcmp(key, "sb_bg_color") == 0) {
    change_sb_bg_color(screen, value);
  } else if (strcmp(key, "hide_underline") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_hide_underline_flag(screen, flag);
    }
  } else if (strcmp(key, "underline_offset") == 0) {
    int offset;

    if (bl_str_to_int(&offset, value)) {
      change_underline_offset(screen, offset);
    }
  } else if (strcmp(key, "baseline_offset") == 0) {
    int offset;

    if (bl_str_to_int(&offset, value)) {
      change_baseline_offset(screen, offset);
    }
  } else if (strcmp(key, "logsize") == 0) {
    u_int log_size;

    if (strcmp(value, "unlimited") == 0) {
      vt_term_unlimit_log_size(screen->term);
    } else if (bl_str_to_uint(&log_size, value)) {
      change_log_size(screen, log_size);
    }
  } else if (strcmp(key, "fontsize") == 0) {
    u_int font_size;

    if (strcmp(value, "larger") == 0) {
      larger_font_size(screen);
    } else if (strcmp(value, "smaller") == 0) {
      smaller_font_size(screen);
    } else {
      if (bl_str_to_uint(&font_size, value)) {
        change_font_size(screen, font_size);
      }
    }
  } else if (strcmp(key, "line_space") == 0) {
    int line_space;

    if (bl_str_to_int(&line_space, value)) {
      change_line_space(screen, line_space);
    }
  } else if (strcmp(key, "letter_space") == 0) {
    u_int letter_space;

    if (bl_str_to_uint(&letter_space, value)) {
      change_letter_space(screen, letter_space);
    }
  } else if (strcmp(key, "screen_width_ratio") == 0) {
    u_int ratio;

    if (bl_str_to_uint(&ratio, value)) {
      change_screen_width_ratio(screen, ratio);
    }
  } else if (strcmp(key, "scrollbar_view_name") == 0) {
    change_sb_view(screen, value);
  } else if (strcmp(key, "mod_meta_key") == 0) {
    change_mod_meta_key(screen, value);
  } else if (strcmp(key, "mod_meta_mode") == 0) {
    change_mod_meta_mode(screen, ui_get_mod_meta_mode_by_name(value));
  } else if (strcmp(key, "bel_mode") == 0) {
    change_bel_mode(screen, ui_get_bel_mode_by_name(value));
  } else if (strcmp(key, "vertical_mode") == 0) {
    change_vertical_mode(screen, vt_get_vertical_mode(value));
  } else if (strcmp(key, "scrollbar_mode") == 0) {
    change_sb_mode(screen, ui_get_sb_mode_by_name(value));
  } else if (strcmp(key, "exit_backscroll_by_pty") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      ui_exit_backscroll_by_pty(flag);
    }
  } else if (strcmp(key, "use_dynamic_comb") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_dynamic_comb_flag(screen, flag);
    }
  } else if (strcmp(key, "receive_string_via_ucs") == 0 ||
             /* backward compatibility with 2.6.1 or before */
             strcmp(key, "copy_paste_via_ucs") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_receive_string_via_ucs_flag(screen, flag);
    }
  } else if (strcmp(key, "use_transbg") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_transparent_flag(screen, flag);
    }
  } else if (strcmp(key, "brightness") == 0) {
    u_int brightness;

    if (bl_str_to_uint(&brightness, value)) {
      change_brightness(screen, brightness);
    }
  } else if (strcmp(key, "contrast") == 0) {
    u_int contrast;

    if (bl_str_to_uint(&contrast, value)) {
      change_contrast(screen, contrast);
    }
  } else if (strcmp(key, "gamma") == 0) {
    u_int gamma;

    if (bl_str_to_uint(&gamma, value)) {
      change_gamma(screen, gamma);
    }
  } else if (strcmp(key, "alpha") == 0) {
    u_int alpha;

    if (bl_str_to_uint(&alpha, value)) {
      change_alpha(screen, alpha);
    }
  } else if (strcmp(key, "fade_ratio") == 0) {
    u_int fade_ratio;

    if (bl_str_to_uint(&fade_ratio, value)) {
      change_fade_ratio(screen, fade_ratio);
    }
  } else if (strcmp(key, "type_engine") == 0) {
    change_font_present(screen, ui_get_type_engine_by_name(value),
                        ui_get_font_present(screen->font_man));
  } else if (strcmp(key, "use_anti_alias") == 0) {
    ui_font_present_t font_present;

    font_present = ui_get_font_present(screen->font_man);

    if (strcmp(value, "true") == 0) {
      font_present &= ~FONT_NOAA;
      font_present |= FONT_AA;
    } else if (strcmp(value, "false") == 0) {
      font_present |= FONT_NOAA;
      font_present &= ~FONT_AA;
    } else /* if( strcmp( value , "default") == 0) */
    {
      font_present &= ~FONT_AA;
      font_present &= ~FONT_NOAA;
    }

    change_font_present(screen, ui_get_type_engine(screen->font_man), font_present);
  } else if (strcmp(key, "use_variable_column_width") == 0) {
    ui_font_present_t font_present;

    font_present = ui_get_font_present(screen->font_man);

    if (strcmp(value, "true") == 0) {
      font_present |= FONT_VAR_WIDTH;
    } else if (strcmp(value, "false") == 0) {
      font_present &= ~FONT_VAR_WIDTH;
    } else {
      return 1;
    }

    change_font_present(screen, ui_get_type_engine(screen->font_man), font_present);
  } else if (strcmp(key, "use_multi_column_char") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      vt_term_set_use_multi_col_char(screen->term, flag);
    }
  } else if (strcmp(key, "use_bold_font") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_use_bold_font_flag(screen, flag);
    }
  } else if (strcmp(key, "use_italic_font") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_use_italic_font_flag(screen, flag);
    }
  } else if (strcmp(key, "use_ctl") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_ctl_flag(screen, flag, vt_term_get_bidi_mode(term), vt_term_is_using_ot_layout(term));
    }
  } else if (strcmp(key, "bidi_mode") == 0) {
    change_ctl_flag(screen, vt_term_is_using_ctl(term), vt_get_bidi_mode(value),
                    vt_term_is_using_ot_layout(term));
  } else if (strcmp(key, "bidi_separators") == 0) {
    vt_term_set_bidi_separators(screen->term, value);
    if (update_special_visual(screen)) {
      vt_term_set_modified_all_lines_in_screen(screen->term);
    }
  }
#ifdef USE_OT_LAYOUT
  else if (strcmp(key, "use_ot_layout") == 0) {
    change_ctl_flag(screen, vt_term_is_using_ctl(term), vt_term_get_bidi_mode(term),
                    strcmp(value, "true") == 0);
  }
#endif
  else if (strcmp(key, "input_method") == 0) {
    change_im(screen, value);
  } else if (strcmp(key, "borderless") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      change_borderless_flag(screen, flag);
    }
  } else if (strcmp(key, "wall_picture") == 0) {
    change_wall_picture(screen, value);
  } else if (strcmp(key, "icon_path") == 0) {
    vt_term_set_icon_path(term, value);
    set_icon(screen);
  } else if (strcmp(key, "use_clipboard") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      ui_set_use_clipboard_selection(flag);
    }
  } else if (strcmp(key, "auto_restart") == 0) {
    vt_set_auto_restart_cmd(strcmp(value, "false") == 0 ? NULL : value);
  } else if (strcmp(key, "allow_osc52") == 0) {
    /*
     * processing_vtseq == -1: process vtseq in loopback.
     * processing_vtseq == 0 : stop processing vtseq.
     */
    if (screen->processing_vtseq <= 0) {
      if (true_or_false(value) > 0) {
        screen->xterm_listener.set_selection = xterm_set_selection;
      } else {
        screen->xterm_listener.set_selection = NULL;
      }
    }
  } else if (strcmp(key, "allow_scp") == 0) {
    /*
     * processing_vtseq == -1: process vtseq in loopback.
     * processing_vtseq == 0 : stop processing vtseq.
     */
    if (screen->processing_vtseq <= 0) {
      int flag;

      if ((flag = true_or_false(value)) >= 0) {
        vt_set_use_scp_full(flag);
      }
    }
  } else if (strcmp(key, "use_urgent_bell") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      ui_set_use_urgent_bell(flag);
    }
  } else if (strcmp(key, "use_extended_scroll_shortcut") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      screen->use_extended_scroll_shortcut = flag;
    }
  } else if (strstr(key, "_use_unicode_font")) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      if (strncmp(key, "only", 4) == 0) {
        /* only_use_unicode_font */

        vt_term_set_unicode_policy(
            screen->term, flag
                              ? (vt_term_get_unicode_policy(screen->term) | ONLY_USE_UNICODE_FONT) &
                                    ~NOT_USE_UNICODE_FONT
                              : vt_term_get_unicode_policy(screen->term) & ~ONLY_USE_UNICODE_FONT);
      } else if (strncmp(key, "not", 3) == 0) {
        /* not_use_unicode_font */

        vt_term_set_unicode_policy(
            screen->term, flag
                              ? (vt_term_get_unicode_policy(screen->term) | NOT_USE_UNICODE_FONT) &
                                    ~ONLY_USE_UNICODE_FONT
                              : vt_term_get_unicode_policy(screen->term) & ~NOT_USE_UNICODE_FONT);
      } else {
        return 1;
      }

      usascii_font_cs_changed(screen, vt_term_get_encoding(screen->term));
    }
  } else if (strcmp(key, "trim_trailing_newline_in_pasting") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      trim_trailing_newline_in_pasting = flag;
    }
  } else if (strcmp(key, "use_vertical_cursor") == 0) {
    int flag;

    if ((flag = true_or_false(value)) != -1) {
      screen->use_vertical_cursor = flag;
    }
  } else if (strcmp(key, "click_interval") == 0) {
    int interval;

    if (bl_str_to_int(&interval, value)) {
      ui_set_click_interval(interval);
    }
  }
#ifdef ROTATABLE_DISPLAY
  else if (strcmp(key, "rotate_display") == 0) {
    ui_display_rotate(strcmp(value, "right") == 0 ? 1 : (strcmp(value, "left") == 0 ? -1 : 0));
  }
#endif
#ifdef USE_CONSOLE
  else if (strcmp(key, "console_encoding") == 0) {
    vt_char_encoding_t encoding;

    if ((encoding = vt_get_char_encoding(value)) != VT_UNKNOWN_ENCODING) {
      ui_display_set_char_encoding(screen->window.disp, encoding);
    }
  } else if (strcmp(key, "console_sixel_colors") == 0) {
    ui_display_set_sixel_colors(screen->window.disp, value);
  }
#endif
  else {
    return 0;
  }

  /*
   * processing_vtseq == -1 means loopback processing of vtseq.
   * If processing_vtseq is -1, it is not set 1 in start_vt100_cmd()
   * which is called from vt_term_write_loopback().
   */
  if (screen->processing_vtseq == -1) {
    char *msg;

    if ((msg = alloca(8 + strlen(key) + 1 + strlen(value) + 1))) {
      sprintf(msg, "Config: %s=%s", key, value);
      vt_term_show_message(screen->term, msg);

      /*
       * screen->processing_vtseq = 0 in
       * vt_term_show_message() -> stop_vt100_cmd().
       */
      screen->processing_vtseq = -1;
    }
  }

  return 1;
}

void ui_screen_reset_view(ui_screen_t *screen) {
  ui_color_manager_reload(screen->color_man);

  ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR));
  vt_term_set_modified_all_lines_in_screen(screen->term);
  font_size_changed(screen);
  ui_xic_font_set_changed(&screen->window);
  ui_window_update(&screen->window, UPDATE_SCREEN | UPDATE_CURSOR);
}

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
void ui_screen_reload_color_cache(ui_screen_t *screen, int do_unload) {
  if (do_unload) {
    ui_color_cache_unload(screen->color_man->color_cache);
  }

  ui_color_manager_reload(screen->color_man);
  ui_window_set_fg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_FG_COLOR));
  ui_xic_fg_color_changed(&screen->window);
  /* XXX should change scrollbar fg color */

  ui_window_set_bg_color(&screen->window, ui_get_xcolor(screen->color_man, VT_BG_COLOR));
  ui_xic_bg_color_changed(&screen->window);
  /* XXX should change scrollbar bg color */
}
#endif

ui_picture_modifier_t *ui_screen_get_picture_modifier(ui_screen_t *screen) {
  if (ui_picture_modifier_is_normal(&screen->pic_mod)) {
    return NULL;
  } else {
    return &screen->pic_mod;
  }
}
