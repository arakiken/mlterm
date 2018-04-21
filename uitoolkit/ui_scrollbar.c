/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_scrollbar.h"

#include <stdlib.h> /* abs */
#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h> /* free */
#include <pobl/bl_str.h> /* strdup */

#include "ui_sb_view_factory.h"

#define HEIGHT_MARGIN(sb) ((sb)->top_margin + (sb)->bottom_margin)
#define IS_TOO_SMALL(sb) ((sb)->window.height <= HEIGHT_MARGIN(sb))

#ifdef DEBUG
#define MAX_BAR_HEIGHT(sb)                                                    \
  (IS_TOO_SMALL(sb)                                                           \
       ? 0 & bl_debug_printf(BL_DEBUG_TAG                                     \
                             " scroll bar is too small , but MAX_BAR_HEIGHT " \
                             "was refered.\n")                                \
       : (sb)->window.height - HEIGHT_MARGIN(sb))
#else
#define MAX_BAR_HEIGHT(sb) ((sb)->window.height - HEIGHT_MARGIN(sb))
#endif

#if 0
#define __DEBUG
#endif

/*
 * For ui_window_update()
 */
enum {
  UPDATE_UPBUTTON = 0x1,
  UPDATE_DOWNBUTTON = 0x2,
  UPDATE_BUTTON = UPDATE_UPBUTTON | UPDATE_DOWNBUTTON,

  UPDATE_SCROLLBAR = 0x4,

  FGCOLOR_CHANGED = 0x8,
  BGCOLOR_CHANGED = 0x10,
};

/* --- static functions --- */

static void set_redraw_area(ui_scrollbar_t *sb, int y, /* Should exclude sb->top_margin. */
                            u_int height               /* Should be over 0. Can be over
                                                          sb->window.height -
                                                          sb->bottom_margin. */
                            ) {
  if (sb->redraw_height == 0) {
    sb->redraw_y = y;
    sb->redraw_height = height;
  } else {
    if (y < sb->redraw_y) {
      sb->redraw_height += (sb->redraw_y - y);
      sb->redraw_y = y;
    }

    if (y + height > sb->redraw_y + sb->redraw_height) {
      sb->redraw_height = y + height - sb->redraw_y;
    }
  }
}

/*
 * Don't call directly draw_xxx functions.
 * Call ui_window_update() instead.
 */
static void draw_scrollbar(ui_scrollbar_t *sb) {
  if (IS_TOO_SMALL(sb)) {
    ui_window_blank(&sb->window);

    return;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " updating scrollbar from %d height %d\n", sb->bar_top_y,
                  sb->bar_height);
#endif

  if (sb->view->draw_scrollbar) {
    (*sb->view->draw_scrollbar)(sb->view, sb->top_margin + sb->bar_top_y, sb->bar_height);
  }
}

/*
 * Don't call directly draw_xxx functions.
 * Call ui_window_update() instead.
 */
static void draw_background(ui_scrollbar_t *sb) {
  if (IS_TOO_SMALL(sb)) {
    return;
  }

  if (sb->view->draw_background && sb->redraw_height > 0) {
    int y;
    int height;

    /* Redraw upward area of bar. */
    if (sb->redraw_y < sb->bar_top_y) {
      y = sb->redraw_y;

      if (y + sb->redraw_height > sb->bar_top_y) {
        /* Redraw except bar area. */
        height = sb->bar_top_y - y;
      } else {
        height = sb->redraw_height;
      }

#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " updating background from %d height %d\n", y + sb->top_margin,
                      height);
#endif
      (*sb->view->draw_background)(sb->view, y + sb->top_margin, height);
    }

    /* Redraw downward area of bar. */
    if (sb->redraw_y < sb->window.height - HEIGHT_MARGIN(sb) &&
        sb->bar_top_y + sb->bar_height < sb->redraw_y + sb->redraw_height) {
      if (sb->redraw_y < sb->bar_top_y + sb->bar_height) {
        y = sb->bar_top_y + sb->bar_height;
      } else {
        y = sb->redraw_y;
      }

      if (sb->redraw_y + sb->redraw_height > sb->window.height - HEIGHT_MARGIN(sb)) {
        /* Redraw except bar area. */
        height = sb->window.height - HEIGHT_MARGIN(sb) - y;
      } else {
        height = sb->redraw_y + sb->redraw_height - y;
      }

#ifdef __DEBUG
      bl_debug_printf(BL_DEBUG_TAG " updating background from %d height %d\n", y + sb->top_margin,
                      height);
#endif
      (*sb->view->draw_background)(sb->view, y + sb->top_margin, height);
    }

    sb->redraw_y = 0;
    sb->redraw_height = 0;
  }
}

/*
 * Don't call directly draw_xxx functions.
 * Call ui_window_update() instead.
 */
static void draw_button(ui_scrollbar_t *sb, int upbutton, int downbutton) {
  if (IS_TOO_SMALL(sb)) {
    return;
  }

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " drawing button.\n");
#endif

  if (upbutton && sb->view->draw_up_button) {
    (*sb->view->draw_up_button)(sb->view, sb->is_pressing_up_button);
  }

  if (downbutton && sb->view->draw_down_button) {
    (*sb->view->draw_down_button)(sb->view, sb->is_pressing_down_button);
  }
}

/*
 * depends on sb->bar_height.
 */
static int calculate_bar_top_y(ui_scrollbar_t *sb) {
  if (IS_TOO_SMALL(sb) || MAX_BAR_HEIGHT(sb) == sb->bar_height ||
      abs(sb->current_row) == sb->num_filled_log_lines) {
    return 0;
  } else {
    return (sb->current_row + sb->num_filled_log_lines) * (MAX_BAR_HEIGHT(sb) - sb->bar_height) /
           sb->num_filled_log_lines;
  }
}

/*
 * depends on sb->bar_height.
 */
static int calculate_current_row(ui_scrollbar_t *sb) {
  if (IS_TOO_SMALL(sb) || MAX_BAR_HEIGHT(sb) == sb->bar_height) {
    return 0;
  } else {
    /*
     * sb->bar_top_y / (sb->num_filled_log_lines /
     *	(MAX_BAR_HEIGHT(sb) - sb->bar_height))
     * => (sb->num_filled_log_lines / (MAX_BAR_HEIGHT(sb) - sb->bar_height))
     *    = pixel per line
     */
    return sb->bar_top_y * sb->num_filled_log_lines / (MAX_BAR_HEIGHT(sb) - sb->bar_height) -
           sb->num_filled_log_lines;
  }
}

static u_int calculate_bar_height(ui_scrollbar_t *sb) {
  if (IS_TOO_SMALL(sb) || sb->num_filled_log_lines + sb->num_scr_lines == 0) {
    return 0;
  } else {
    u_int bar_height;

    bar_height = (sb->num_scr_lines * MAX_BAR_HEIGHT(sb)) /
                 (sb->num_filled_log_lines + sb->num_scr_lines);

    if (bar_height < MAX_BAR_HEIGHT(sb) / 20) {
      bar_height = MAX_BAR_HEIGHT(sb) / 20;
    }

    return bar_height;
  }
}

static int is_updown_button_event(
    ui_scrollbar_t *sb, int y /* this value must include margin or be y on actual window */
    ) {
  int up_button_y;
  int down_button_y;

  /*
   * minus value means y from the bottom.
   */

  if (sb->up_button_y < 0) {
    up_button_y = sb->window.height + sb->up_button_y;
  } else {
    up_button_y = sb->up_button_y;
  }

  if (sb->down_button_y < 0) {
    down_button_y = sb->window.height + sb->down_button_y;
  } else {
    down_button_y = sb->down_button_y;
  }

  if (up_button_y <= y && y <= up_button_y + sb->up_button_height) {
#ifdef __DEBUG
    bl_debug_printf("up button pressed\n");
#endif

    return 1;
  } else if (down_button_y <= y && y <= down_button_y + sb->down_button_height) {
#ifdef __DEBUG
    bl_debug_printf("down button pressed\n");
#endif

    return -1;
  } else {
    return 0;
  }
}

/*
 * callbacks of ui_window_t events.
 */

static void trigger_sb_view_realized(ui_scrollbar_t *sb) {
  if (sb->view->realized) {
    (*sb->view->realized)(sb->view, sb->window.disp->display, sb->window.disp->screen,
                          sb->window.my_window, ui_window_get_fg_gc(&sb->window),
                          sb->window.height);
  }

  /*
   * FGCOLOR_CHANGED|BGCOLOR_CHANGED is necessary in order for
   * ui_sb_view_t::color_changed to be called. If it is not called,
   * fg or bg color of buttons are not correctly drawn especially
   * in changing transparent flag.
   */
  ui_window_update(&sb->window, FGCOLOR_CHANGED | BGCOLOR_CHANGED);
}

static void window_realized(ui_window_t *win) {
  ui_scrollbar_t *sb;

  sb = (ui_scrollbar_t*)win;

  if (ui_load_named_xcolor(win->disp, &sb->fg_xcolor, sb->fg_color)) {
    ui_window_set_fg_color(win, &sb->fg_xcolor);
  }

  if (ui_load_named_xcolor(win->disp, &sb->bg_xcolor, sb->bg_color)) {
    ui_window_set_bg_color(win, &sb->bg_xcolor);
  }

  trigger_sb_view_realized(sb);
}

static void window_resized(ui_window_t *win) {
  ui_scrollbar_t *sb;

#ifdef __DEBUG
  bl_debug_printf(BL_DEBUG_TAG " scrollbar resized.\n");
#endif

  sb = (ui_scrollbar_t*)win;

  if (IS_TOO_SMALL(sb)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " scrollbar is too small to be drawn.\n");
#endif

    sb->num_scr_lines = 0;
    sb->bar_height = 0;
    sb->bar_top_y = 0;
  } else {
    sb->num_scr_lines = MAX_BAR_HEIGHT(sb) / sb->line_height;
    sb->bar_height = calculate_bar_height(sb);
    sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height;
  }

  if (sb->view->resized) {
    (*sb->view->resized)(sb->view, sb->window.my_window, sb->window.height);
  }

  set_redraw_area(sb, 0, sb->window.height);
  ui_window_update(&sb->window, UPDATE_SCROLLBAR | UPDATE_BUTTON);
}

static void window_exposed(ui_window_t *win, int x, int y, u_int width, u_int height) {
  ui_scrollbar_t *sb;

  sb = (ui_scrollbar_t*)win;

  if (y < sb->top_margin) {
    height -= (sb->top_margin - y);
    y = 0;
  } else {
    y -= sb->top_margin;
    height -= sb->top_margin;
  }

  set_redraw_area(sb, y, height);

  /*
   * XXX
   * GC values should be set(in ui_window_get_gc) before sb->view->func is
   * called.
   * For win32: Current gc is set every time window_exposed and update_window.
   */
  sb->view->gc = ui_window_get_bg_gc(&sb->window);
  draw_background(sb);

  sb->view->gc = ui_window_get_fg_gc(&sb->window);
  draw_scrollbar(sb);
  draw_button(sb, 1, 1);
}

static void update_window(ui_window_t *win, int flag) {
  ui_scrollbar_t *sb;

  if (flag == 0 || !win->is_mapped /* ui_scrollbar_line_is_added() is called if is_mapped = 0 */) {
    return;
  }

  sb = (ui_scrollbar_t*)win;

  if (flag & (FGCOLOR_CHANGED | BGCOLOR_CHANGED)) {
    if (sb->view->color_changed) {
      if (flag & FGCOLOR_CHANGED) {
        sb->view->gc = ui_window_get_fg_gc(&sb->window);
        (*sb->view->color_changed)(sb->view, 1);
      }

      if (flag & BGCOLOR_CHANGED) {
        sb->view->gc = ui_window_get_bg_gc(&sb->window);
        (*sb->view->color_changed)(sb->view, 0);
      }
    }
  }

  if (flag & UPDATE_SCROLLBAR) {
    /*
     * XXX
     * GC values should be set(in ui_window_get_gc) before sb->view->func is
     * called.
     * For win32: Current gc is set every time window_exposed and update_window.
     */
    sb->view->gc = ui_window_get_bg_gc(&sb->window);
    draw_background(sb);

    sb->view->gc = ui_window_get_fg_gc(&sb->window);
    draw_scrollbar(sb);
  } else {
    sb->view->gc = ui_window_get_fg_gc(&sb->window);
  }

  if (flag & ~UPDATE_SCROLLBAR) {
    draw_button(sb, (flag & UPDATE_UPBUTTON) != 0, (flag & UPDATE_DOWNBUTTON) != 0);
  }
}

static void up_button_pressed(ui_scrollbar_t *sb) {
  if (!ui_scrollbar_move_upward(sb, 1)) {
    return;
  }

  if (sb->sb_listener->screen_scroll_downward) {
    /* up button scrolls *down* screen */
    (*sb->sb_listener->screen_scroll_downward)(sb->sb_listener->self, 1);
  }
}

static void down_button_pressed(ui_scrollbar_t *sb) {
  if (!ui_scrollbar_move_downward(sb, 1)) {
    return;
  }

  if (sb->sb_listener->screen_scroll_upward) {
    /* down button scrolls *up* screen */
    (*sb->sb_listener->screen_scroll_upward)(sb->sb_listener->self, 1);
  }
}

static void button_pressed(ui_window_t *win, XButtonEvent *event, int click_num) {
  ui_scrollbar_t *sb;
  int result;
  int y;

  sb = (ui_scrollbar_t*)win;

  if (IS_TOO_SMALL(sb)) {
    return;
  }

  result = is_updown_button_event(sb, event->y);

  y = event->y - sb->top_margin;

  if (result == 0) {
    if (y < sb->bar_top_y) {
      ui_scrollbar_move_upward(sb, sb->num_scr_lines);

      if (sb->sb_listener->screen_scroll_downward) {
        /* down button scrolls *down* screen */
        (*sb->sb_listener->screen_scroll_downward)(sb->sb_listener->self, sb->num_scr_lines);
      }
    } else if (y > sb->bar_top_y + sb->bar_height) {
      ui_scrollbar_move_downward(sb, sb->num_scr_lines);

      if (sb->sb_listener->screen_scroll_upward) {
        /* down button scrolls *up* screen */
        (*sb->sb_listener->screen_scroll_upward)(sb->sb_listener->self, sb->num_scr_lines);
      }
    }
  } else if (result == 1) {
    sb->is_pressing_up_button = 1;
    ui_window_update(&sb->window, UPDATE_UPBUTTON);

    up_button_pressed(sb);
  } else if (result == -1) {
    sb->is_pressing_down_button = 1;
    ui_window_update(&sb->window, UPDATE_DOWNBUTTON);

    down_button_pressed(sb);
  }
}

static void button_press_continued(ui_window_t *win, XButtonEvent *event) {
  ui_scrollbar_t *sb;
  int result;

  sb = (ui_scrollbar_t*)win;

  result = is_updown_button_event(sb, event->y);

  if (sb->is_pressing_up_button && result == 1) {
    up_button_pressed(sb);
  } else if (sb->is_pressing_down_button && result == -1) {
    down_button_pressed(sb);
  }
}

static void button_motion(ui_window_t *win, XMotionEvent *event) {
  ui_scrollbar_t *sb;
  int new_row;
  int up_to_top_now;
  int y;
  int old_bar_top_y;
  int old_bar_height;

  sb = (ui_scrollbar_t*)win;

  if (sb->is_pressing_up_button || sb->is_pressing_down_button ||
      is_updown_button_event(sb, event->y) != 0 || IS_TOO_SMALL(sb)) {
    return;
  }

  y = event->y - sb->top_margin;
  old_bar_top_y = sb->bar_top_y;
  old_bar_height = sb->bar_height;

  if (sb->bar_top_y == 0) {
    up_to_top_now = 1;
  } else {
    up_to_top_now = 0;
  }

  if (sb->is_motion == 0) {
    if (sb->bar_top_y <= y && y <= sb->bar_top_y + sb->bar_height) {
      /* on the bar */

      sb->y_on_bar = y - sb->bar_top_y;
    } else {
      /* out of the bar */

      sb->y_on_bar = sb->bar_height / 2;

      if (y < sb->y_on_bar) {
        sb->bar_top_y = 0;
      } else {
        sb->bar_top_y = y - sb->y_on_bar;
      }
    }

    sb->is_motion = 1;
  } else {
    if (y < sb->y_on_bar) {
      if (sb->bar_top_y != 0) {
        sb->bar_top_y = 0;
      } else {
        return;
      }
    } else if (y - sb->y_on_bar + sb->bar_height > MAX_BAR_HEIGHT(sb)) {
      sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height;
    } else {
      sb->bar_top_y = y - sb->y_on_bar;
    }
  }

  if (!up_to_top_now && sb->bar_top_y == 0) {
    /* up to the top this time */

    up_to_top_now = 1;
  } else {
    /* if bar is on the top , it is not *this* time(maybe previous...) */

    up_to_top_now = 0;
  }

  new_row = calculate_current_row(sb);

  /*
   * if bar reaches the top this time , it doesn't return but draw_scrollbar().
   */
  if (!up_to_top_now && sb->current_row == new_row) {
    /* Restore bar_top_y and bar_height */

    sb->bar_top_y = old_bar_top_y;
    sb->bar_height = old_bar_height;

    return;
  }

  sb->current_row = new_row;

  if (sb->sb_listener->screen_scroll_to) {
    (*sb->sb_listener->screen_scroll_to)(sb->sb_listener->self, sb->current_row);
  }

  set_redraw_area(sb, old_bar_top_y, old_bar_height);
  ui_window_update(&sb->window, UPDATE_SCROLLBAR);
}

static void button_released(ui_window_t *win, XButtonEvent *event) {
  ui_scrollbar_t *sb;

  sb = (ui_scrollbar_t*)win;

  if (sb->is_pressing_up_button) {
    sb->is_pressing_up_button = 0;
    ui_window_update(&sb->window, UPDATE_UPBUTTON);
  }

  if (sb->is_pressing_down_button) {
    sb->is_pressing_down_button = 0;
    ui_window_update(&sb->window, UPDATE_DOWNBUTTON);
  }

  if (sb->is_motion) {
    sb->is_motion = 0;
  }
}

/* --- global functions --- */

int ui_scrollbar_init(ui_scrollbar_t *sb, ui_scrollbar_event_listener_t *sb_listener,
                      char *view_name, char *fg_color, char *bg_color, u_int height,
                      u_int line_height, u_int num_log_lines, u_int num_filled_log_lines,
                      int use_transbg, ui_picture_modifier_t *pic_mod) {
  u_int width;

  /* dynamically allocated */
  sb->view_name = NULL;
  sb->view = NULL;
  sb->fg_color = NULL;
  sb->bg_color = NULL;

  if (view_name) {
    sb->view_name = strdup(view_name);
  } else {
    sb->view_name = strdup("simple");
  }

  if (sb->view_name == NULL) {
    goto error;
  }

  if (use_transbg) {
    if ((sb->view = ui_transparent_sb_view_new(sb->view_name))) {
      goto view_created;
    }
  }

  if ((sb->view = ui_sb_view_new(sb->view_name)) == NULL) {
    free(sb->view_name);

    if ((sb->view_name = strdup("simple")) == NULL) {
      goto error;
    }

    if (use_transbg) {
      if ((sb->view = ui_transparent_sb_view_new(sb->view_name))) {
        goto view_created;
      }
    }

    if ((sb->view = ui_sb_view_new(sb->view_name)) == NULL) {
      goto error;
    }
  }

  use_transbg = 0;

view_created:
  sb->view->win = &sb->window;

  sb->sb_listener = sb_listener;

  (*sb->view->get_geometry_hints)(sb->view, &width, &sb->top_margin, &sb->bottom_margin,
                                  &sb->up_button_y, &sb->up_button_height, &sb->down_button_y,
                                  &sb->down_button_height);

  if (sb->view->get_default_color) {
    char* _fg_color;
    char* _bg_color;

    (*sb->view->get_default_color)(sb->view, &_fg_color, &_bg_color);

    if (fg_color == NULL) {
      fg_color = _fg_color;
    }

    if (bg_color == NULL) {
      bg_color = _bg_color;
    }
  } else {
    if (fg_color == NULL) {
      fg_color = "black";
    }

    if (bg_color == NULL) {
      bg_color = "white";
    }
  }

  sb->fg_color = strdup(fg_color);
  sb->bg_color = strdup(bg_color);

  sb->is_pressing_up_button = 0;
  sb->is_pressing_down_button = 0;

  if (!ui_window_init(&sb->window, width, height, width, 0, 0, 0, 0, 0, 0, 0)) {
    goto error;
  }

  sb->line_height = line_height;

  if (IS_TOO_SMALL(sb)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " scrollbar is too small to be drawn.\n");
#endif

    sb->bar_height = 0;
    sb->num_scr_lines = 0;
  } else {
    sb->bar_height = height - HEIGHT_MARGIN(sb);
    sb->num_scr_lines = sb->bar_height / sb->line_height;
  }

  sb->num_log_lines = num_log_lines;
  sb->num_filled_log_lines = num_filled_log_lines;
  sb->bar_top_y = 0;
  sb->y_on_bar = 0;
  sb->current_row = 0;
  sb->redraw_y = 0;
  sb->redraw_height = 0;
  sb->is_motion = 0;

  if (use_transbg) {
    ui_window_set_transparent(&sb->window, pic_mod);
  }

  ui_window_set_cursor(&sb->window, XC_left_ptr);

  /*
   * event callbacks.
   */
  ui_window_add_event_mask(&sb->window, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask);
  sb->window.window_realized = window_realized;
  sb->window.button_pressed = button_pressed;
  sb->window.button_released = button_released;
  sb->window.button_press_continued = button_press_continued;
  sb->window.button_motion = button_motion;
  sb->window.window_resized = window_resized;
  sb->window.window_exposed = window_exposed;
  sb->window.update_window = update_window;

  return 1;

error:
  free(sb->fg_color);
  free(sb->bg_color);
  free(sb->view_name);

  if (sb->view) {
    (*sb->view->delete)(sb->view);
  }

  return 0;
}

void ui_scrollbar_final(ui_scrollbar_t *sb) {
  (*sb->view->delete)(sb->view);
  ui_unload_scrollbar_view_lib(sb->view_name);

  ui_unload_xcolor(sb->window.disp, &sb->fg_xcolor);
  ui_unload_xcolor(sb->window.disp, &sb->bg_xcolor);
  free(sb->fg_color);
  free(sb->bg_color);
  free(sb->view_name);
}

void ui_scrollbar_set_num_log_lines(ui_scrollbar_t *sb, u_int num_log_lines) {
  if (sb->num_log_lines == num_log_lines) {
    return;
  }

  sb->num_log_lines = num_log_lines;

  if (sb->num_filled_log_lines > sb->num_log_lines) {
    sb->num_filled_log_lines = sb->num_log_lines;
  }

  set_redraw_area(sb, sb->bar_top_y, sb->bar_height);

  sb->bar_height = calculate_bar_height(sb);
  sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height;

  ui_window_update(&sb->window, UPDATE_SCROLLBAR);
}

void ui_scrollbar_set_num_filled_log_lines(ui_scrollbar_t *sb, u_int lines) {
  if (lines > sb->num_log_lines) {
    lines = sb->num_log_lines;
  }

  if (sb->num_filled_log_lines == lines) {
    return;
  }

  sb->num_filled_log_lines = lines;

  set_redraw_area(sb, sb->bar_top_y, sb->bar_height);

  sb->bar_height = calculate_bar_height(sb);
  sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height;

  ui_window_update(&sb->window, UPDATE_SCROLLBAR);
}

int ui_scrollbar_line_is_added(ui_scrollbar_t *sb) {
  int old_bar_top_y;
  u_int old_bar_height;

  if ((*sb->sb_listener->screen_is_static)(sb->sb_listener->self)) {
    if (sb->num_filled_log_lines < sb->num_log_lines) {
      sb->num_filled_log_lines++;
    }

    sb->current_row--;
  } else if (sb->num_filled_log_lines == sb->num_log_lines) {
    return 0;
  } else {
    sb->num_filled_log_lines++;
  }

  old_bar_height = sb->bar_height;
  sb->bar_height = calculate_bar_height(sb);

  old_bar_top_y = sb->bar_top_y;
  sb->bar_top_y = calculate_bar_top_y(sb);

  if (old_bar_top_y == sb->bar_top_y && old_bar_height == sb->bar_height) {
    return 1;
  } else {
    set_redraw_area(sb, old_bar_top_y, old_bar_height);
    ui_window_update(&sb->window, UPDATE_SCROLLBAR);

    return 1;
  }
}

void ui_scrollbar_reset(ui_scrollbar_t *sb) {
  if (sb->is_motion || sb->bar_top_y + sb->bar_height < MAX_BAR_HEIGHT(sb)) {
    set_redraw_area(sb, sb->bar_top_y, sb->bar_height);

    sb->bar_top_y = MAX_BAR_HEIGHT(sb) - sb->bar_height;
    sb->is_motion = 0;
    sb->current_row = 0;

    ui_window_update(&sb->window, UPDATE_SCROLLBAR);
  }
}

int ui_scrollbar_move_upward(ui_scrollbar_t *sb, u_int size) {
#if 0
  if (sb->bar_top_y == 0)
#else
  /*
   * XXX Adhoc solution
   * Fix ui_screen.c:bs_{half_}page_{up|down}ward() instead.
   */
  if (sb->current_row + sb->num_filled_log_lines == 0)
#endif
  {
    return 0;
  }

  return ui_scrollbar_move(sb, sb->current_row - size);
}

int ui_scrollbar_move_downward(ui_scrollbar_t *sb, u_int size) {
  if (sb->current_row >= 0) {
    return 0;
  }

  return ui_scrollbar_move(sb, sb->current_row + size);
}

int ui_scrollbar_move(ui_scrollbar_t *sb, int row) {
  if (0 < row) {
    row = 0;
  } else if (row + (int)sb->num_filled_log_lines < 0) {
    row = -(sb->num_filled_log_lines);
  }

  if (sb->current_row == row) {
    return 0;
  }

  sb->current_row = row;

  set_redraw_area(sb, sb->bar_top_y, sb->bar_height);

  sb->bar_top_y = calculate_bar_top_y(sb);

  ui_window_update(&sb->window, UPDATE_SCROLLBAR);

  return 1;
}

int ui_scrollbar_set_line_height(ui_scrollbar_t *sb, u_int line_height) {
  if (sb->line_height == line_height) {
    return 0;
  }

  sb->line_height = line_height;

  set_redraw_area(sb, sb->bar_top_y, sb->bar_height);

  sb->bar_height = calculate_bar_height(sb);

  ui_window_update(&sb->window, UPDATE_SCROLLBAR);

  return 1;
}

int ui_scrollbar_set_fg_color(ui_scrollbar_t *sb, char *fg_color) {
  free(sb->fg_color);
  ui_unload_xcolor(sb->window.disp, &sb->fg_xcolor);

  sb->fg_color = strdup(fg_color);

  if (ui_load_named_xcolor(sb->window.disp, &sb->fg_xcolor, sb->fg_color)) {
    ui_window_set_fg_color(&sb->window, &sb->fg_xcolor);

    set_redraw_area(sb, 0, sb->window.height);
    ui_window_update(&sb->window, UPDATE_SCROLLBAR | UPDATE_BUTTON | FGCOLOR_CHANGED);
  }

  return 1;
}

int ui_scrollbar_set_bg_color(ui_scrollbar_t *sb, char *bg_color) {
  free(sb->bg_color);
  ui_unload_xcolor(sb->window.disp, &sb->bg_xcolor);

  sb->bg_color = strdup(bg_color);

  if (ui_load_named_xcolor(sb->window.disp, &sb->bg_xcolor, sb->bg_color)) {
    ui_window_set_bg_color(&sb->window, &sb->bg_xcolor);

    set_redraw_area(sb, 0, sb->window.height);
    ui_window_update(&sb->window, UPDATE_SCROLLBAR | UPDATE_BUTTON | BGCOLOR_CHANGED);
  }

  return 1;
}

int ui_scrollbar_change_view(ui_scrollbar_t *sb, char *name) {
  ui_sb_view_t *view;
  u_int width;

  if (strcmp(name, sb->view_name) == 0 || (name = strdup(name)) == NULL) {
    return 0;
  }

  if (sb->window.is_transparent) {
    if ((view = ui_transparent_sb_view_new(name)) == NULL) {
      /* nothing is done */

      free(name);

      return 0;
    }
  } else {
    if ((view = ui_sb_view_new(name)) == NULL) {
      free(name);

      return 0;
    }
  }

  view->win = &sb->window;

  if (sb->view) {
    (*sb->view->delete)(sb->view);
    ui_unload_scrollbar_view_lib(sb->view_name);
  }

  sb->view = view;

  free(sb->view_name);

  /* name is dynamically allocated above */
  sb->view_name = name;

  (*sb->view->get_geometry_hints)(sb->view, &width, &sb->top_margin, &sb->bottom_margin,
                                  &sb->up_button_y, &sb->up_button_height, &sb->down_button_y,
                                  &sb->down_button_height);

  sb->bar_height = calculate_bar_height(sb);
  sb->bar_top_y = calculate_bar_top_y(sb);

  trigger_sb_view_realized(sb);

  if (sb->window.width != width) {
    ui_window_set_normal_hints(&sb->window, width, sb->window.min_height, 0, 0);

    ui_window_resize(&sb->window, width, sb->window.height, NOTIFY_TO_PARENT);
  }

  set_redraw_area(sb, 0, sb->window.height);

  ui_window_update(&sb->window, UPDATE_SCROLLBAR | UPDATE_BUTTON);

  return 1;
}

int ui_scrollbar_set_transparent(ui_scrollbar_t *sb, ui_picture_modifier_t *pic_mod, int force) {
  ui_sb_view_t *view;

  if (!force && sb->window.is_transparent) {
    /* already set */

    return 1;
  }

  if ((view = ui_transparent_sb_view_new(sb->view_name)) == NULL) {
    /* nothing is done */

    return 0;
  }

  view->win = &sb->window;

  if (sb->view) {
    (*sb->view->delete)(sb->view);
  }

  sb->view = view;

  /* This should be done before ui_window_set_untransparent() , which calls
   * exposed event. */
  trigger_sb_view_realized(sb);

  ui_window_set_transparent(&sb->window, pic_mod);

  set_redraw_area(sb, 0, sb->window.height);
  ui_window_update(&sb->window, UPDATE_SCROLLBAR | UPDATE_BUTTON);

  return 1;
}

int ui_scrollbar_unset_transparent(ui_scrollbar_t *sb) {
  ui_sb_view_t *view;

  if (!sb->window.is_transparent) {
    /* already unset */

    return 1;
  }

  if ((view = ui_sb_view_new(sb->view_name)) == NULL) {
    /* nothing is done */

    return 0;
  }

  view->win = &sb->window;

  if (sb->view) {
    (*sb->view->delete)(sb->view);
  }

  sb->view = view;

  /* This should be done before ui_window_set_untransparent() , which calls
   * exposed event. */
  trigger_sb_view_realized(sb);

  ui_window_unset_transparent(&sb->window);

  set_redraw_area(sb, 0, sb->window.height);
  ui_window_update(&sb->window, UPDATE_SCROLLBAR | UPDATE_BUTTON);

  return 1;
}
