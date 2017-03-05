/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_layout.h"

#include <pobl/bl_types.h> /* u_int */
#include <pobl/bl_str.h>

#ifdef USE_CONSOLE
static u_int dummy_arg;
#define SEPARATOR_WIDTH ui_get_opened_displays(&dummy_arg)[0]->display->col_width
#define SEPARATOR_HEIGHT ui_get_opened_displays(&dummy_arg)[0]->display->line_height
#else
#define SEPARATOR_WIDTH 1
#define SEPARATOR_HEIGHT 1
#endif
#define SCROLLBAR_WIDTH(scrollbar) (ACTUAL_WIDTH(&(scrollbar).window) + SEPARATOR_WIDTH)

/* --- static functions --- */

static u_int modify_separator(u_int separator, u_int total, u_int next_min, u_int unit, u_int fixed,
                              u_int margin) {
  u_int surplus;
  int dont_round_up;

  if (total < separator + margin + next_min) {
    separator = total - next_min - margin;
    dont_round_up = 1;
  } else {
    dont_round_up = 0;
  }

  if ((surplus = (separator - fixed) % unit) > 0) {
    if (!dont_round_up && surplus > unit / 2) {
      separator += (unit - surplus);
    } else if (separator - fixed > surplus) {
      separator -= surplus;
    } else {
      separator = fixed + unit;
    }
  }

  return separator;
}

static void reset_layout(struct terminal *term, int x, int y, u_int width, u_int height) {
  u_int child_width;
  u_int child_height;

  if (term->separator_x > 0) {
    term->separator_x = child_width = modify_separator(
        term->separator_x, width,
        term->next[0]->screen->window.hmargin * 2 + ui_col_width(term->next[0]->screen) +
            (term->next[0]->sb_mode != SBM_NONE ? SCROLLBAR_WIDTH(term->next[0]->scrollbar) : 0),
        ui_col_width(term->screen),
        term->screen->window.hmargin * 2 +
            (term->sb_mode != SBM_NONE ? SCROLLBAR_WIDTH(term->scrollbar) : 0),
        SEPARATOR_WIDTH);
  } else {
    child_width = width;
  }

  if (term->separator_y > 0) {
    term->separator_y = child_height = modify_separator(
        term->separator_y, height,
        term->next[1]->screen->window.vmargin * 2 + ui_line_height(term->next[1]->screen),
        ui_line_height(term->screen), term->screen->window.vmargin * 2, SEPARATOR_HEIGHT);
  } else {
    child_height = height;
  }

  if (term->sb_mode != SBM_NONE) {
    int sep_x;
    int do_redraw;

    if (term->sb_mode == SBM_RIGHT) {
      ui_window_move(&term->screen->window, x, y);
      do_redraw = ui_window_move(&term->scrollbar.window,
                                 x + child_width - ACTUAL_WIDTH(&term->scrollbar.window), y);
      sep_x = x + child_width - SCROLLBAR_WIDTH(term->scrollbar);
    } else {
      ui_window_move(&term->screen->window, x + SCROLLBAR_WIDTH(term->scrollbar), y);
      do_redraw = ui_window_move(&term->scrollbar.window, x, y);
      sep_x = x + ACTUAL_WIDTH(&term->scrollbar.window);
    }

    /*
     * The order of resizing: ui_scrollbar_t -> ui_screen_t
     *
     * ui_window_resize_with_margin(screen) may call ui_scrollbar_line_is_added()
     * which redraws screen by ui_window_update(), so you should resize ui_scrollbar_t
     * before ui_screen_t.
     */
    ui_window_resize_with_margin(&term->scrollbar.window, ACTUAL_WIDTH(&term->scrollbar.window),
                                 child_height, NOTIFY_TO_MYSELF);
    ui_window_resize_with_margin(&term->screen->window,
                                 child_width - SCROLLBAR_WIDTH(term->scrollbar), child_height,
                                 NOTIFY_TO_MYSELF);

#ifdef MANAGE_SUB_WINDOWS_BY_MYSELF
    ui_window_fill_with(&UI_SCREEN_TO_LAYOUT(term->screen)->window,
                        &UI_SCREEN_TO_LAYOUT(term->screen)->window.fg_color, sep_x, y,
                        SEPARATOR_WIDTH, child_height);
#elif defined(USE_WIN32GUI)
    /*
     * Scrollbar 3 isn't clearly redrawn without this.
     *
     *  1              2       3
     * +-+------------+-+-----+-+----+
     * | |$ hresize_sc| |$    | |$   |
     * | |reen +1     | |     | |    |
     * | |            | |     | |    |
     */
    if (do_redraw) {
      ui_window_update_all(&term->scrollbar.window);
    }
#endif
  } else {
    ui_window_move(&term->screen->window, x, y);
    ui_window_resize_with_margin(&term->screen->window, child_width, child_height,
                                 NOTIFY_TO_MYSELF);
  }

#ifdef USE_QUARTZ
  /* ui_window_move() clears screen in true transparency on MacOSX/Cocoa. */
  if (term->screen->color_man->alpha < 255) {
    ui_window_update_all(&term->screen->window);
  }
#endif

  if (term->yfirst) {
    if (term->next[1] && term->separator_y > 0) {
      reset_layout(term->next[1], x, y + term->separator_y + SEPARATOR_HEIGHT, width,
                   height - term->separator_y - SEPARATOR_HEIGHT);
#ifdef MANAGE_SUB_WINDOWS_BY_MYSELF
      ui_window_fill_with(&UI_SCREEN_TO_LAYOUT(term->screen)->window,
                          &UI_SCREEN_TO_LAYOUT(term->screen)->window.fg_color, x,
                          y + term->separator_y, width, SEPARATOR_HEIGHT);
#endif
    }

    if (term->next[0] && term->separator_x > 0) {
      reset_layout(term->next[0], x + term->separator_x + SEPARATOR_WIDTH, y,
                   width - term->separator_x - SEPARATOR_WIDTH, child_height);
#ifdef MANAGE_SUB_WINDOWS_BY_MYSELF
      ui_window_fill_with(&UI_SCREEN_TO_LAYOUT(term->screen)->window,
                          &UI_SCREEN_TO_LAYOUT(term->screen)->window.fg_color,
                          x + term->separator_x, y, SEPARATOR_WIDTH, child_height);
#endif
    }
  } else {
    if (term->next[0] && term->separator_x > 0) {
      reset_layout(term->next[0], x + term->separator_x + SEPARATOR_WIDTH, y,
                   width - term->separator_x - SEPARATOR_WIDTH, height);
#ifdef MANAGE_SUB_WINDOWS_BY_MYSELF
      ui_window_fill_with(&UI_SCREEN_TO_LAYOUT(term->screen)->window,
                          &UI_SCREEN_TO_LAYOUT(term->screen)->window.fg_color,
                          x + term->separator_x, y, SEPARATOR_WIDTH, height);
#endif
    }

    if (term->next[1] && term->separator_y > 0) {
      reset_layout(term->next[1], x, y + term->separator_y + SEPARATOR_HEIGHT, child_width,
                   height - term->separator_y - SEPARATOR_HEIGHT);
#ifdef MANAGE_SUB_WINDOWS_BY_MYSELF
      ui_window_fill_with(&UI_SCREEN_TO_LAYOUT(term->screen)->window,
                          &UI_SCREEN_TO_LAYOUT(term->screen)->window.fg_color, x,
                          y + term->separator_y, child_width, SEPARATOR_HEIGHT);
#endif
    }
  }
}

static struct terminal *search_term(struct terminal *term, ui_screen_t *screen) {
  struct terminal *hit;

  if (term->screen == screen) {
    return term;
  }

  if ((term->next[0] && (hit = search_term(term->next[0], screen))) ||
      (term->next[1] && (hit = search_term(term->next[1], screen)))) {
    return hit;
  }

  return NULL;
}

static struct terminal *search_parent_term(struct terminal *term, struct terminal *child) {
  struct terminal *parent;

  if (term->next[0] == child || term->next[1] == child) {
    return term;
  }

  if ((term->next[0] && (parent = search_parent_term(term->next[0], child))) ||
      (term->next[1] && (parent = search_parent_term(term->next[1], child)))) {
    return parent;
  }

  return NULL;
}

static ui_screen_t *search_next_screen(struct terminal *term, ui_screen_t *screen, int *found) {
  ui_screen_t *hit;
  int idx;
  int count;

  if (term->screen == screen) {
    *found = 1;
  }

  if (term->yfirst) {
    idx = 0;
  } else {
    idx = 1;
  }

  for (count = 0; count < 2; count++) {
    if (term->next[idx]) {
      if (*found) {
        return term->next[idx]->screen;
      } else if ((hit = search_next_screen(term->next[idx], screen, found))) {
        return hit;
      }
    }

    idx = !idx;
  }

  return NULL;
}

static ui_screen_t *search_prev_screen(struct terminal *term, ui_screen_t *screen,
                                       ui_screen_t **prev) {
  int idx;
  int count;

  if (*prev && term->screen == screen) {
    return *prev;
  }

  *prev = term->screen;

  if (term->yfirst) {
    idx = 0;
  } else {
    idx = 1;
  }

  for (count = 0; count < 2; count++) {
    if (term->next[idx] && search_prev_screen(term->next[idx], screen, prev)) {
      return *prev;
    }

    idx = !idx;
  }

  return NULL;
}

static struct terminal *get_current_term(struct terminal *term) {
  struct terminal *current;

  if (term->screen->window.inputtable > 0) {
    return term;
  }

  if ((term->next[0] && (current = get_current_term(term->next[0]))) ||
      (term->next[1] && (current = get_current_term(term->next[1])))) {
    return current;
  }

  return NULL;
}

static ui_window_t *get_current_window(ui_layout_t *layout) {
  struct terminal *term;

  if ((term = get_current_term(&layout->term))) {
    return &term->screen->window;
  } else {
    return &layout->term.screen->window;
  }
}

static u_int get_separator_x(ui_screen_t *screen, u_int sb_width, u_int percent /* < 100 */
                             ) {
  u_int width;
  u_int fixed_width;
  u_int sep_x;

  width = screen->window.width;
  fixed_width = screen->window.hmargin * 2 + sb_width;

  sep_x = (width + fixed_width) * percent / 100;
  if (sep_x < fixed_width + ui_col_width(screen)) {
    return 0;
  }

#ifdef PSEUDO_COLOR_DISPLAY
  if (screen->window.disp->display->pixels_per_byte > 1) {
    return sep_x - sep_x % screen->window.disp->display->pixels_per_byte - SEPARATOR_WIDTH;
  } else
#endif
  {
    return sep_x - (sep_x - fixed_width) % ui_col_width(screen);
  }
}

static u_int get_separator_y(ui_screen_t *screen, u_int percent /* < 100 */
                             ) {
  u_int height;
  u_int fixed_height;
  u_int sep_y;

  height = screen->window.height;
  fixed_height = screen->window.vmargin * 2;

  sep_y = (height + fixed_height) * percent / 100;
  if (sep_y < fixed_height + ui_line_height(screen)) {
    return 0;
  }

  return sep_y - (sep_y - fixed_height) % ui_line_height(screen);
}

/*
 * callbacks of ui_window_t events.
 */

static void window_finalized(ui_window_t *win) {
  ui_layout_t *layout;

  layout = (ui_layout_t *)win;

  ui_layout_delete(layout);
}

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
static void reload_color_cache(struct terminal *term, int do_unload) {
  ui_screen_reload_color_cache(term->screen, do_unload);

  if (term->next[0]) {
    reload_color_cache(term->next[0], 0);
  }

  if (term->next[1]) {
    reload_color_cache(term->next[1], 0);
  }
}
#endif

static void window_resized(ui_window_t *win) {
  ui_layout_t *layout;
  ui_picture_t *pic;

  layout = (ui_layout_t *)win;

  if (layout->bg_pic &&
      (pic = ui_acquire_bg_picture(&layout->window, &layout->pic_mod, layout->pic_file_path))) {
    ui_window_set_wall_picture(&layout->window, pic->pixmap, 0);
    ui_release_picture(layout->bg_pic);
    layout->bg_pic = pic;

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
    if (layout->window.disp->depth == 4 && strstr(layout->pic_file_path, "six")) {
      /*
       * Color pallette of ui_display can be changed by ui_acquire_bg_picture().
       * (see ui_display_set_cmap() called from fb/ui_imagelib.c.)
       */
      reload_color_cache(&layout->term, 1);
    }
#endif
  }

  reset_layout(&layout->term, 0, 0, win->width, win->height);
}

static void child_window_resized(ui_window_t *win, ui_window_t *child) {
  ui_layout_t *layout;
  u_int actual_width;

  layout = (ui_layout_t *)win;

  if (layout->term.next[0] || layout->term.next[1]) {
    reset_layout(&layout->term, 0, 0, layout->window.width, layout->window.height);

    return;
  }

  if (&layout->term.screen->window == child) {
    if (layout->term.sb_mode == SBM_NONE) {
      actual_width = ACTUAL_WIDTH(child);
    } else {
      actual_width =
          ACTUAL_WIDTH(child) + ACTUAL_WIDTH(&layout->term.scrollbar.window) + SEPARATOR_WIDTH;
    }

    ui_window_resize_with_margin(&layout->window, actual_width, ACTUAL_HEIGHT(child),
                                 NOTIFY_TO_NONE);

    ui_window_resize_with_margin(&layout->term.scrollbar.window,
                                 ACTUAL_WIDTH(&layout->term.scrollbar.window), ACTUAL_HEIGHT(child),
                                 NOTIFY_TO_MYSELF);

    if (layout->term.sb_mode == SBM_RIGHT) {
      ui_window_move(&layout->term.scrollbar.window,
                     ACTUAL_WIDTH(&layout->term.screen->window) + SEPARATOR_WIDTH, 0);
    }
  } else if (&layout->term.scrollbar.window == child) {
    if (layout->term.sb_mode == SBM_NONE) {
      return;
    }

    ui_window_resize_with_margin(
        &layout->window,
        ACTUAL_WIDTH(child) + ACTUAL_WIDTH(&layout->term.screen->window) + SEPARATOR_WIDTH,
        ACTUAL_HEIGHT(child), NOTIFY_TO_NONE);

    if (layout->term.sb_mode == SBM_LEFT) {
      ui_window_move(&layout->term.screen->window,
                     ACTUAL_WIDTH(&layout->term.scrollbar.window) + SEPARATOR_WIDTH, 0);
    }
  } else {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " illegal child. this event should be invoken only by screen.\n");
#endif
  }
}

static void window_exposed(ui_window_t *win, int x, int y, u_int width, u_int height) {
  ui_window_fill_with(win, &((ui_layout_t *)win)->term.screen->window.fg_color, x, y, width,
                      height);
}

static void key_pressed(ui_window_t *win, XKeyEvent *event) {
  ui_layout_t *layout;
  ui_window_t *child;

  layout = (ui_layout_t *)win;
  child = get_current_window(layout);

  /* dispatch to screen */
  (*child->key_pressed)(child, event);
}

static void utf_selection_notified(ui_window_t *win, u_char *buf, size_t len) {
  ui_layout_t *layout;
  ui_window_t *child;

  layout = (ui_layout_t *)win;
  child = get_current_window(layout);

  /* dispatch to screen */
  (*child->utf_selection_notified)(child, buf, len);
}

static void xct_selection_notified(ui_window_t *win, u_char *buf, size_t len) {
  ui_layout_t *layout;
  ui_window_t *child;

  layout = (ui_layout_t *)win;
  child = get_current_window(layout);

  /* dispatch to screen */
  (*child->xct_selection_notified)(child, buf, len);
}

#ifndef DISABLE_XDND
static void set_xdnd_config(ui_window_t *win, char *dev, char *buf, char *value) {
  ui_layout_t *layout;
  ui_window_t *child;

  layout = (ui_layout_t *)win;
  child = get_current_window(layout);

  /* dispatch to screen */
  (*child->set_xdnd_config)(child, dev, buf, value);
}
#endif

static void window_deleted_intern(struct terminal *term) {
  /* dispatch to screen */
  (*term->screen->window.window_deleted)(&term->screen->window);

  if (term->next[0]) {
    window_deleted_intern(term->next[0]);
  }

  if (term->next[1]) {
    window_deleted_intern(term->next[1]);
  }
}

static void window_deleted(ui_window_t *win) {
  ui_layout_t *layout;

  layout = (ui_layout_t *)win;

  window_deleted_intern(&layout->term);
}

#ifndef USE_QUARTZ
static void change_sb_mode_intern(struct terminal *term, ui_sb_mode_t new_mode, int dynamic_change);

static void check_scrollbar_visible(struct terminal *term) {
  if (term->autohide_scrollbar && term->sb_mode == SBM_RIGHT) {
    if (term->scrollbar.window.button_is_pressing) {
      term->idling_count = 0;
    } else if (++term->idling_count == 30) {
      term->idling_count = 0;
      change_sb_mode_intern(term, SBM_NONE, 1);
    }
  }

  if (term->next[0]) {
    check_scrollbar_visible(term->next[0]);
  }

  if (term->next[1]) {
    check_scrollbar_visible(term->next[1]);
  }
}

static void window_idling(ui_window_t *win) {
  ui_layout_t *layout;

  layout = (ui_layout_t *)win;

  check_scrollbar_visible(&layout->term);
}
#endif

#if 0
/*
 * Overriding methods of ui_scrollbar_t.
 */

static void sb_key_pressed(ui_window_t *win, XKeyEvent *event) {
  struct terminal term;
  ui_screen_t **screen;

  screen = ((void *)win) - (((void *)&term.scrollbar) - ((void *)&term.screen));

  /* dispatch to screen */
  (*(*screen)->window.key_pressed)(&(*screen)->window, event);
}
#endif

/*
 * Overriding methods of vt_screen_listener_t of ui_screen_t.
 */

static void line_scrolled_out(void *p /* must be ui_screen_t(, or child of ui_layout_t) */
                              ) {
  ui_layout_t *layout;
  struct terminal *term;

  layout = UI_SCREEN_TO_LAYOUT((ui_screen_t *)p);

  (*layout->line_scrolled_out)(p);

  if ((term = search_term(&layout->term, p))) {
    if (vt_term_log_size_is_unlimited(((ui_screen_t *)p)->term)) {
      ui_scrollbar_set_num_of_log_lines(&term->scrollbar,
                                        vt_term_get_log_size(((ui_screen_t *)p)->term));
    }

    ui_scrollbar_line_is_added(&term->scrollbar);
  }
}

/*
 * callbacks of ui_sb_event_listener_t events.
 */

static int screen_scroll_to(void *p, int row) {
  struct terminal *term;

  term = p;

  ui_screen_scroll_to(term->screen, row);

  return 1;
}

static int screen_scroll_upward(void *p, u_int size) {
  struct terminal *term;

  term = p;

  ui_screen_scroll_upward(term->screen, size);

  return 1;
}

static int screen_scroll_downward(void *p, u_int size) {
  struct terminal *term;

  term = p;

  ui_screen_scroll_downward(term->screen, size);

  return 1;
}

static int screen_is_static(void *p) {
  struct terminal *term;

  term = p;

  if (vt_term_is_backscrolling(term->screen->term) == BSM_STATIC) {
    return 1;
  } else {
    return 0;
  }
}

/*
 * callbacks of ui_screen_scroll_event_listener_t events.
 */

static void bs_mode_exited(void *p) {
  struct terminal *term;

  term = p;

  ui_scrollbar_reset(&term->scrollbar);
}

static void scrolled_upward(void *p, u_int size) {
  struct terminal *term;

  term = p;

  ui_scrollbar_move_downward(&term->scrollbar, size);
}

static void scrolled_downward(void *p, u_int size) {
  struct terminal *term;

  term = p;

  ui_scrollbar_move_upward(&term->scrollbar, size);
}

static void scrolled_to(void *p, int row) {
  struct terminal *term;

  term = p;

  ui_scrollbar_move(&term->scrollbar, row);
}

static void log_size_changed(void *p, u_int log_size) {
  struct terminal *term;

  term = p;

  ui_scrollbar_set_num_of_log_lines(&term->scrollbar, log_size);
}

static void line_height_changed(void *p, u_int line_height) {
  struct terminal *term;

  term = p;

  ui_scrollbar_set_line_height(&term->scrollbar, line_height);
}

static void change_fg_color(void *p, char *color) {
  struct terminal *term;

  term = p;

  ui_scrollbar_set_fg_color(&term->scrollbar, color);
}

static char *get_fg_color(void *p) {
  struct terminal *term;

  term = p;

  return term->scrollbar.fg_color;
}

static void change_bg_color(void *p, char *color) {
  struct terminal *term;

  term = p;

  ui_scrollbar_set_bg_color(&term->scrollbar, color);
}

static char *get_bg_color(void *p) {
  struct terminal *term;

  term = p;

  return term->scrollbar.bg_color;
}

static void change_view(void *p, char *name) {
  struct terminal *term;

  term = p;

  ui_scrollbar_change_view(&term->scrollbar, name);
}

static char *get_view_name(void *p) {
  struct terminal *term;

  term = p;

  return term->scrollbar.view_name;
}

static void transparent_state_changed(void *p, int is_transparent, ui_picture_modifier_t *pic_mod) {
  struct terminal *term;

  term = p;

  if (is_transparent == 1) {
    ui_scrollbar_set_transparent(&term->scrollbar, pic_mod, 1);
  } else {
    ui_scrollbar_unset_transparent(&term->scrollbar);
  }
}

static ui_sb_mode_t sb_mode(void *p) {
  struct terminal *term;

  term = p;

  if (term->autohide_scrollbar) {
    return SBM_AUTOHIDE;
  } else {
    return term->sb_mode;
  }
}

static void screen_color_changed(void *p) {
  ui_layout_t *layout;

  layout = UI_SCREEN_TO_LAYOUT(((struct terminal *)p)->screen);

  if (&layout->term == p) {
    ui_window_set_fg_color(&layout->window, &layout->term.screen->window.fg_color);
    ui_window_set_bg_color(&layout->window, &layout->term.screen->window.bg_color);
  }
}

static u_int total_width(struct terminal *term) {
  u_int width;

  width = ACTUAL_WIDTH(&term->screen->window);

  if (term->sb_mode != SBM_NONE) {
    width += (ACTUAL_WIDTH(&term->scrollbar.window) + SEPARATOR_WIDTH);
  }

  if (term->separator_x > 0 && term->next[0]) {
    width += (total_width(term->next[0]) + SEPARATOR_WIDTH);
  }

  return width;
}

static u_int total_height(struct terminal *term) {
  u_int height;

  height = ACTUAL_HEIGHT(&term->screen->window);

  if (term->separator_y > 0 && term->next[1]) {
    height += (total_height(term->next[1]) + SEPARATOR_HEIGHT);
  }

  return height;
}

/* XXX */
static void total_hint_size(struct terminal *term, u_int *width, u_int *height, u_int *width_inc,
                            u_int *height_inc) {
  u_int size;

  if (*width_inc < (size = ui_col_width(term->screen))) {
    *width_inc = size;
  }

  if (*height_inc < (size = ui_line_height(term->screen))) {
    *height_inc = size;
  }

  if (term->sb_mode != SBM_NONE) {
    *width += SEPARATOR_WIDTH;
  }

  if (term->next[0]) {
    *width += SEPARATOR_WIDTH;

    total_hint_size(term->next[0], width, height, width_inc, height_inc);
  }

  if (term->next[1]) {
    *height += SEPARATOR_HEIGHT;

    total_hint_size(term->next[1], width, height, width_inc, height_inc);
  }
}

static void update_normal_hints(ui_layout_t *layout) {
  u_int min_width = 0;
  u_int min_height = 0;
  u_int width_inc = 0;
  u_int height_inc = 0;

  total_hint_size(&layout->term, &min_width, &min_height, &width_inc, &height_inc);

  ui_window_set_normal_hints(&layout->window, min_width, min_height, width_inc, height_inc);
}

#ifndef USE_QUARTZ
static int need_idling_event(struct terminal *term) {
  if (!term->autohide_scrollbar && (!term->next[0] || !need_idling_event(term->next[0])) &&
      (!term->next[1] || !need_idling_event(term->next[1]))) {
    return 0;
  } else {
    return 1;
  }
}
#endif

static void change_sb_mode_intern(struct terminal *term, ui_sb_mode_t new_mode,
                                  int dynamic_change) {
  ui_layout_t *layout;
  ui_sb_mode_t old_mode;

  layout = UI_SCREEN_TO_LAYOUT(term->screen);

  if (new_mode == SBM_AUTOHIDE) {
    if (term->autohide_scrollbar) {
      return;
    }

    new_mode = SBM_NONE;
    term->autohide_scrollbar = 1;
    ui_window_add_event_mask(&term->screen->window, PointerMotionMask);
#ifndef USE_QUARTZ
    layout->window.idling = window_idling;
#endif
  } else if (!dynamic_change) {
    term->autohide_scrollbar = 0;
    ui_window_remove_event_mask(&term->screen->window, PointerMotionMask);
    (*term->screen->xterm_listener.set_mouse_report)(term->screen->xterm_listener.self);

#ifndef USE_QUARTZ
    if (!need_idling_event(&layout->term)) {
      layout->window.idling = NULL;
    }
#endif
  }

  if ((old_mode = term->sb_mode) == new_mode) {
    return;
  }

  /*
   * term->sb_mode should be changed before ui_window_unmap/ui_window_map
   * for framebuffer. (see fb/ui_window.c)
   */
  term->sb_mode = new_mode;

  if (new_mode == SBM_NONE) {
    ui_window_unmap(&term->scrollbar.window);
  }
#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
  else if (old_mode == SBM_NONE) {
    /* scrollbar's height may have been changed. */
    ui_window_resize_with_margin(&term->scrollbar.window, ACTUAL_WIDTH(&term->scrollbar.window),
                                 ACTUAL_HEIGHT(&term->screen->window), NOTIFY_TO_MYSELF);

    /* Don't call ui_window_map() because scrollbar's position may be
     * inappropriate. */
    term->scrollbar.window.is_mapped = 1;
    term->scrollbar.window.x = -1; /* To redraw scrollbar in ui_window_move() */

    if (new_mode == SBM_LEFT) {
      /* Shrink window size before ui_window_move() in reset_layout(). */
      ui_window_resize_with_margin(&term->screen->window, ACTUAL_WIDTH(&term->screen->window) -
                                                              SCROLLBAR_WIDTH(term->scrollbar),
                                   ACTUAL_HEIGHT(&term->screen->window), 0);
    }
  }

  reset_layout(&layout->term, 0, 0, layout->window.width, layout->window.height);

  if (new_mode == SBM_NONE) {
    /* Window size was enlarged but not exposed. */
    (*term->screen->window.window_exposed)(
        &term->screen->window,
        ACTUAL_WIDTH(&term->screen->window) - SCROLLBAR_WIDTH(term->scrollbar), 0,
        SCROLLBAR_WIDTH(term->scrollbar), ACTUAL_HEIGHT(&term->screen->window));
  }
#else
  else if (old_mode == SBM_NONE) {
    ui_window_map(&term->scrollbar.window);
  } else {
    goto noresize;
  }

  if ((term->screen->window.x + ACTUAL_WIDTH(&term->screen->window) +
           (old_mode != SBM_NONE ? SCROLLBAR_WIDTH(term->scrollbar) : 0) <
       ACTUAL_WIDTH(&layout->window)) ||
      !ui_window_resize(&layout->window, total_width(&layout->term), total_height(&layout->term),
                        NOTIFY_TO_MYSELF)) {
  noresize:
    reset_layout(&layout->term, 0, 0, layout->window.width, layout->window.height);
  }
#endif

  update_normal_hints(layout);
}

static void change_sb_mode(void *p, ui_sb_mode_t new_mode) {
  change_sb_mode_intern(p, new_mode, 0);
}

static void term_changed(void *p, u_int log_size, u_int logged_lines) {
  struct terminal *term;

  term = p;

  ui_scrollbar_set_num_of_log_lines(&term->scrollbar, log_size);
  ui_scrollbar_set_num_of_filled_log_lines(&term->scrollbar, logged_lines);
}

static void screen_pointer_motion(ui_window_t *win, XMotionEvent *event) {
  struct terminal *term;
  ui_layout_t *layout;

  layout = UI_SCREEN_TO_LAYOUT((ui_screen_t *)win);
  (*layout->pointer_motion)(win, event);

  term = search_term(&layout->term, (ui_screen_t *)win);

  if (term->autohide_scrollbar) {
    ui_sb_mode_t mode;

    if (((int)win->width) - 2 <= event->x && 0 <= event->y && event->y < win->height) {
      if (term->scrollbar.window.is_mapped) {
        return;
      }

      mode = SBM_RIGHT;
    } else {
      if (!term->scrollbar.window.is_mapped) {
        return;
      }

      mode = SBM_NONE;
    }

    change_sb_mode_intern(term, mode, 1);
  }
}

static void delete_term(struct terminal *term) {
  ui_scrollbar_final(&term->scrollbar);

  if (term->next[0]) {
    delete_term(term->next[0]);
    free(term->next[0]);
  }

  if (term->next[1]) {
    delete_term(term->next[1]);
    free(term->next[1]);
  }
}

static void delete_screen_bg_pic(ui_screen_t *screen) {
  free(screen->pic_file_path);
  screen->pic_file_path = NULL;
  ui_release_picture(screen->bg_pic);
  screen->bg_pic = NULL;
}

static void delete_layout_bg_pic(ui_layout_t *layout) {
  free(layout->pic_file_path);
  layout->pic_file_path = NULL;
  ui_release_picture(layout->bg_pic);
  layout->bg_pic = NULL;
}

static void save_screen_bg_pic(ui_layout_t *layout) {
  /* layout->bg_pic has been already set. */
  ui_release_picture(layout->term.screen->bg_pic);
  layout->term.screen->bg_pic = NULL;

  layout->pic_file_path = layout->term.screen->pic_file_path;
  layout->term.screen->pic_file_path = NULL;

  layout->pic_mod = layout->term.screen->pic_mod;
}

static void restore_screen_bg_pic(ui_layout_t *layout) {
  layout->term.screen->pic_file_path = layout->pic_file_path;
  layout->pic_file_path = NULL;
  layout->term.screen->bg_pic = layout->bg_pic;
  layout->bg_pic = NULL;
  layout->term.screen->pic_mod = layout->pic_mod;
}

/* --- global functions --- */

ui_layout_t *ui_layout_new(ui_screen_t *screen, char *view_name, char *fg_color, char *bg_color,
                           ui_sb_mode_t mode, u_int hmargin, u_int vmargin) {
  ui_layout_t *layout;
  u_int actual_width;
  u_int min_width;
  int screen_x;
  int sb_x;
  int sb_map;

  if ((layout = calloc(1, sizeof(ui_layout_t))) == NULL) {
    return NULL;
  }

  /*
   * event callbacks.
   */
  layout->term.sb_listener.self = &layout->term;
  layout->term.sb_listener.screen_scroll_to = screen_scroll_to;
  layout->term.sb_listener.screen_scroll_upward = screen_scroll_upward;
  layout->term.sb_listener.screen_scroll_downward = screen_scroll_downward;
  layout->term.sb_listener.screen_is_static = screen_is_static;

  if (ui_scrollbar_init(
          &layout->term.scrollbar, &layout->term.sb_listener, view_name, fg_color, bg_color,
          ACTUAL_HEIGHT(&screen->window), ui_line_height(screen),
          vt_term_get_log_size(screen->term), vt_term_get_num_of_logged_lines(screen->term),
          screen->window.is_transparent, ui_screen_get_picture_modifier(screen)) == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_scrollbar_init() failed.\n");
#endif

    goto error;
  }
#if 0
  layout->term.scrollbar.window.key_pressed = sb_key_pressed;
#endif
  layout->term.screen = screen;

  /*
   * event callbacks.
   */
  layout->term.screen_scroll_listener.self = &layout->term;
  layout->term.screen_scroll_listener.bs_mode_entered = NULL;
  layout->term.screen_scroll_listener.bs_mode_exited = bs_mode_exited;
  layout->term.screen_scroll_listener.scrolled_upward = scrolled_upward;
  layout->term.screen_scroll_listener.scrolled_downward = scrolled_downward;
  layout->term.screen_scroll_listener.scrolled_to = scrolled_to;
  layout->term.screen_scroll_listener.log_size_changed = log_size_changed;
  layout->term.screen_scroll_listener.line_height_changed = line_height_changed;
  layout->term.screen_scroll_listener.change_fg_color = change_fg_color;
  layout->term.screen_scroll_listener.fg_color = get_fg_color;
  layout->term.screen_scroll_listener.change_bg_color = change_bg_color;
  layout->term.screen_scroll_listener.bg_color = get_bg_color;
  layout->term.screen_scroll_listener.change_view = change_view;
  layout->term.screen_scroll_listener.view_name = get_view_name;
  layout->term.screen_scroll_listener.transparent_state_changed = transparent_state_changed;
  layout->term.screen_scroll_listener.sb_mode = sb_mode;
  layout->term.screen_scroll_listener.change_sb_mode = change_sb_mode;
  layout->term.screen_scroll_listener.term_changed = term_changed;
  layout->term.screen_scroll_listener.screen_color_changed = screen_color_changed;

  ui_set_screen_scroll_listener(screen, &layout->term.screen_scroll_listener);

  layout->line_scrolled_out = screen->screen_listener.line_scrolled_out;
  screen->screen_listener.line_scrolled_out = line_scrolled_out;

  if (mode == SBM_AUTOHIDE) {
    layout->term.sb_mode = SBM_NONE;
    layout->term.autohide_scrollbar = 1;
  } else {
    layout->term.sb_mode = mode;
  }
  layout->pointer_motion = screen->window.pointer_motion;
  screen->window.pointer_motion = screen_pointer_motion;

  if (layout->term.sb_mode == SBM_NONE) {
    actual_width = ACTUAL_WIDTH(&screen->window);

    min_width = 0;
  } else {
    actual_width = ACTUAL_WIDTH(&screen->window) + SCROLLBAR_WIDTH(layout->term.scrollbar);

    min_width = SEPARATOR_WIDTH;
  }

  if (ui_window_init(&layout->window, actual_width, ACTUAL_HEIGHT(&screen->window), min_width, 0, 0,
                     0, hmargin, vmargin, 0, 0) == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " ui_window_init() failed.\n");
#endif

    goto error;
  }

  if (layout->term.sb_mode == SBM_RIGHT) {
    screen_x = 0;
    sb_x = ACTUAL_WIDTH(&screen->window) + SEPARATOR_WIDTH;
    sb_map = 1;
  } else if (layout->term.sb_mode == SBM_LEFT) {
    screen_x = ACTUAL_WIDTH(&layout->term.scrollbar.window) + SEPARATOR_WIDTH;
    sb_x = 0;
    sb_map = 1;
  } else /* if( layout->term.sb_mode == SBM_NONE) */
  {
    screen_x = sb_x = 0; /* overlaying scrollbar window */
    sb_map = 0;
  }

  if (!ui_window_add_child(&layout->window, &layout->term.scrollbar.window, sb_x, 0, sb_map) ||
      !ui_window_add_child(&layout->window, &screen->window, screen_x, 0, 1)) {
    goto error;
  }

  /*
   * event call backs.
   */
  ui_window_add_event_mask(&layout->window, KeyPressMask);
  layout->window.window_finalized = window_finalized;
  layout->window.window_resized = window_resized;
  layout->window.child_window_resized = child_window_resized;
  layout->window.window_exposed = window_exposed;
  layout->window.key_pressed = key_pressed;
  layout->window.utf_selection_notified = utf_selection_notified;
  layout->window.xct_selection_notified = xct_selection_notified;
  layout->window.window_deleted = window_deleted;
#ifndef DISABLE_XDND
  layout->window.set_xdnd_config = set_xdnd_config;
#endif

  if (layout->term.autohide_scrollbar) {
#ifndef USE_QUARTZ
    layout->window.idling = window_idling;
#endif
    ui_window_add_event_mask(&screen->window, PointerMotionMask);
  }

  return layout;

error:
  free(layout);

  return NULL;
}

int ui_layout_delete(ui_layout_t *layout) {
  if (layout->bg_pic) {
    delete_layout_bg_pic(layout);
  }

  delete_term(&layout->term);
  free(layout);

  return 1;
}

int ui_layout_add_child(ui_layout_t *layout, ui_screen_t *screen, int horizontal,
                        const char *sep_str /* "XX%" or "XX" */
                        ) {
  struct terminal *next;
  struct terminal *term;
  u_int sep = 50;
  int is_percent = 1;

  if (sep_str) {
    char *p;

    if ((p = strchr(sep_str, '%'))) {
      if (!bl_str_n_to_uint(&sep, sep_str, p - sep_str) || sep >= 100 || sep == 0) {
        return 0;
      }
    } else {
      if (!bl_str_to_uint(&sep, sep_str) || sep == 0) {
        return 0;
      }

      is_percent = 0;
    }
  }

  if (!(next = calloc(1, sizeof(struct terminal)))) {
    return 0;
  }

  if (!(term = get_current_term(&layout->term))) {
    term = &layout->term;
  }

  if (horizontal) {
    u_int orig_separator_x;
    u_int sb_width;

    orig_separator_x = term->separator_x;

    if (term->sb_mode != SBM_NONE) {
      sb_width = SCROLLBAR_WIDTH(term->scrollbar);
    } else {
      sb_width = 0;
    }

    if (is_percent ? ((term->separator_x = get_separator_x(term->screen, sb_width, sep)) == 0)
                   : ((term->separator_x = screen->window.hmargin * 2 +
                                           ui_col_width(term->screen) * sep + sb_width) >=
                      ACTUAL_WIDTH(&term->screen->window) + sb_width)) {
      term->separator_x = orig_separator_x;
      free(next);

      return 0;
    }

    next->next[0] = term->next[0];
    term->next[0] = next;

    if (orig_separator_x > 0) {
      next->separator_x = orig_separator_x - term->separator_x;
    }
  } else {
    u_int orig_separator_y;

    orig_separator_y = term->separator_y;

    if (is_percent ? ((term->separator_y = get_separator_y(term->screen, sep)) == 0)
                   : ((term->separator_y = screen->window.vmargin * 2 +
                                           ui_line_height(term->screen) * sep) >=
                      ACTUAL_HEIGHT(&term->screen->window))) {
      term->separator_y = orig_separator_y;
      free(next);

      return 0;
    }

    if (term->separator_x == 0) {
      term->yfirst = 1;
    }

    next->next[1] = term->next[1];
    term->next[1] = next;

    if (orig_separator_y > 0) {
      next->separator_y = orig_separator_y - term->separator_y;
    }
  }

  next->sb_listener = term->sb_listener;
  next->sb_listener.self = next;

  ui_scrollbar_init(
      &next->scrollbar, &next->sb_listener, term->scrollbar.view_name, term->scrollbar.fg_color,
      term->scrollbar.bg_color, ACTUAL_HEIGHT(&screen->window), ui_line_height(screen),
      vt_term_get_log_size(screen->term), vt_term_get_num_of_logged_lines(screen->term),
      screen->window.is_transparent, ui_screen_get_picture_modifier(screen));
#if 0
  next->scrollbar.window.key_pressed = sb_key_pressed;
#endif
  ui_window_add_child(&layout->window, &next->scrollbar.window, 0, 0, 0);
  ui_window_show(&next->scrollbar.window, 0);

  next->screen_scroll_listener = term->screen_scroll_listener;
  next->screen_scroll_listener.self = next;

  next->screen = screen;
  ui_set_screen_scroll_listener(screen, &next->screen_scroll_listener);
  screen->screen_listener.line_scrolled_out = line_scrolled_out;
  ui_window_add_child(&layout->window, &screen->window, 0, 0, 0);

  if (screen->pic_file_path) {
    delete_screen_bg_pic(screen);
  }

  ui_window_show(&screen->window, 0);

  if (!ui_window_has_wall_picture(&layout->window) && layout->term.screen->pic_file_path) {
    if ((layout->bg_pic = ui_acquire_bg_picture(&layout->window, &layout->term.screen->pic_mod,
                                                layout->term.screen->pic_file_path))) {
      save_screen_bg_pic(layout);
      ui_window_set_wall_picture(&layout->window, layout->bg_pic->pixmap, 0);

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
      if (layout->window.disp->depth == 4 && strstr(layout->pic_file_path, "six")) {
        /*
         * Color pallette of ui_display can be changed by
         * ui_acquire_bg_picture().
         * (see ui_display_set_cmap() called from fb/ui_imagelib.c.)
         */
        reload_color_cache(&layout->term, 1);
      }
#endif
    }
  }

  next->sb_mode = term->sb_mode;
  if ((next->autohide_scrollbar = term->autohide_scrollbar)) {
    ui_window_add_event_mask(&screen->window, PointerMotionMask);
  }
  screen->window.pointer_motion = screen_pointer_motion;

  reset_layout(&layout->term, 0, 0, layout->window.width, layout->window.height);

  if (term->sb_mode != SBM_NONE) {
    ui_window_map(&next->scrollbar.window);
  }
  ui_window_map(&screen->window);

  ui_window_set_input_focus(&term->screen->window);

  update_normal_hints(layout);

  return 1;
}

int ui_layout_remove_child(ui_layout_t *layout, ui_screen_t *screen) {
  struct terminal *term;
  int idx2;
#ifndef MANAGE_SUB_WINDOWS_BY_MYSELF
  u_int w_surplus;
  u_int h_surplus;
#endif

  if (layout->term.next[0] == NULL && layout->term.next[1] == NULL) {
    return 0;
  }

  term = search_term(&layout->term, screen);

  ui_scrollbar_final(&term->scrollbar);
  ui_window_unmap(&term->scrollbar.window);
  ui_window_final(&term->scrollbar.window);

  if (term->yfirst) {
    idx2 = 0;
  } else {
    idx2 = 1;
  }

  if (!term->next[idx2]) {
    idx2 = !idx2;
  }

  if (term == &layout->term) {
    struct terminal *next;

    if (idx2 == 0) {
      if (term->separator_y > 0) {
        term->next[0]->separator_y = term->separator_y;
      }
    } else {
      if (term->separator_x > 0) {
        term->next[1]->separator_x = term->separator_x;
      }
    }
    term->next[idx2]->yfirst = term->yfirst;

    next = term->next[idx2];

    if (term->next[!idx2]) {
      struct terminal *parent;

      parent = search_parent_term(next, NULL);
      if (parent->next[!idx2] == NULL) {
        parent->next[!idx2] = term->next[!idx2];
      } else {
        parent->next[idx2] = term->next[!idx2];
      }
    }

    *term = *next;
    term->screen_scroll_listener.self = term;
    ui_set_screen_scroll_listener(term->screen, &term->screen_scroll_listener);
    term->sb_listener.self = term;
    term->scrollbar.sb_listener = &term->sb_listener;
#ifndef USE_QUARTZ
    term->scrollbar.view->win = &term->scrollbar.window;
#endif

    term = next;
  } else {
    struct terminal *parent;
    int idx;

    parent = search_parent_term(&layout->term, term);

    if (parent->next[0] == term) {
      idx = 0;
    } else {
      idx = 1;
    }

    if (term->next[idx2]) {
      if (idx2 == 0) {
        if (term->separator_y > 0) {
          term->next[0]->separator_y = term->separator_y;
        }
      } else {
        if (term->separator_x > 0) {
          term->next[1]->separator_x = term->separator_x;
        }
      }
      term->next[idx2]->yfirst = term->yfirst;

      parent->next[idx] = term->next[idx2];

      if (term->next[!idx2]) {
        parent = search_parent_term(parent->next[idx], NULL);
        if (parent->next[!idx2] == NULL) {
          parent->next[!idx2] = term->next[!idx2];
        } else {
          parent->next[idx2] = term->next[!idx2];
        }
      }
    } else {
      parent->next[idx] = NULL;

      if (idx == 0) {
        parent->separator_x = 0;
      } else {
        parent->separator_y = 0;
      }
    }
  }

  ui_window_remove_child(&layout->window, &term->scrollbar.window);
  ui_window_remove_child(&layout->window, &screen->window);
  ui_window_unmap(&screen->window);

  if (layout->bg_pic && layout->term.next[0] == NULL && layout->term.next[1] == NULL) {
    restore_screen_bg_pic(layout);
    ui_window_unset_wall_picture(&layout->window, 0);
  }

#ifndef MANAGE_SUB_WINDOWS_BY_MYSELF
  if (!layout->term.next[0] && !layout->term.next[1]) {
    w_surplus = (layout->window.width - layout->term.screen->window.hmargin * 2 -
                 (layout->term.sb_mode != SBM_NONE ? SCROLLBAR_WIDTH(layout->term.scrollbar) : 0)) %
                ui_col_width(layout->term.screen);
    h_surplus = (layout->window.height - layout->term.screen->window.vmargin * 2) %
                ui_line_height(layout->term.screen);
  } else {
    w_surplus = h_surplus = 0;
  }

  if (w_surplus > 0 || h_surplus > 0) {
    ui_window_resize(&layout->window, layout->window.width - w_surplus,
                     layout->window.height - h_surplus, NOTIFY_TO_MYSELF);
  } else
#endif
  {
    reset_layout(&layout->term, 0, 0, layout->window.width, layout->window.height);
  }

  update_normal_hints(layout);

#ifndef MANAGE_SUB_WINDOWS_BY_MYSELF
  if (ui_screen_attached(screen)) {
    /* Revert to the original size. */
    ui_window_resize_with_margin(
        &screen->window, ACTUAL_WIDTH(&layout->window) -
                             (term->sb_mode != SBM_NONE ? SCROLLBAR_WIDTH(term->scrollbar) : 0),
        ACTUAL_HEIGHT(&layout->window), NOTIFY_TO_MYSELF);
  }
#endif

  free(term);

  ui_window_set_input_focus(&layout->term.screen->window);

#ifndef USE_QUARTZ
  if (layout->window.idling && !need_idling_event(&layout->term)) {
    layout->window.idling = NULL;
  }
#endif

  return 1;
}

int ui_layout_switch_screen(ui_layout_t *layout, int prev) {
  ui_screen_t *screen;

  if (!layout->term.next[0] && !layout->term.next[1]) {
    return 0;
  }

  if (prev) {
    screen = NULL;

    search_prev_screen(&layout->term, (ui_screen_t *)get_current_window(layout), &screen);
  } else {
    int found = 0;

    if (!(screen = search_next_screen(&layout->term, (ui_screen_t *)get_current_window(layout),
                                      &found))) {
      screen = layout->term.screen;
    }
  }

  ui_window_set_input_focus(&screen->window);

  return 1;
}

int ui_layout_resize(ui_layout_t *layout, ui_screen_t *screen, int horizontal, int step) {
  struct terminal *term;
  struct terminal *child = NULL;

  if (step == 0) {
    return 0;
  }

  if (!(term = get_current_term(&layout->term))) {
    term = &layout->term;
  }

  if (horizontal) {
    while (term->separator_x == 0) {
      if (!(term = search_parent_term(&layout->term, (child = term)))) {
        return 0;
      }
    }

    if (term->next[0] == child) {
      step = -step;
    }

    step *= ui_col_width(term->screen);

    if (step < 0 && term->separator_x < abs(step)) {
      return 0;
    }

    term->separator_x += step;
  } else {
    while (term->separator_y == 0) {
      if (!(term = search_parent_term(&layout->term, (child = term)))) {
        return 0;
      }
    }

    if (term->next[1] == child) {
      step = -step;
    }

    step *= ui_line_height(term->screen);

    if (step < 0 && term->separator_y < abs(step)) {
      return 0;
    }

    term->separator_y += step;
  }

  reset_layout(&layout->term, 0, 0, layout->window.width, layout->window.height);

  return 1;
}
