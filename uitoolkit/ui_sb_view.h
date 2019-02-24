/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/*
 * !! Notice !!
 * This file must be kept as independent to specific systems as possible.
 * So types like u_xxx which may not be defined in some environments must
 * not be used here.
 */

#ifndef __UI_SB_VIEW_H__
#define __UI_SB_VIEW_H__

#include "ui.h"

typedef struct ui_display *__ui_display_ptr_t;
typedef struct ui_window *__ui_window_ptr_t;

typedef struct ui_sb_view {
  Display *display;
  int screen;
  Window window;
  GC gc; /* If you change gc values in ui_sb_view, restore them before return.
            */
  unsigned int height;

  /*
   * Set 1 when create ui_sb_view_t.
   * ui_sb_view_t of version 0 doesn't have this 'version' member, so
   * ui_sb_view_t->version designates ui_sb_view->get_geometry_hints actually.
   * It is assumed that ui_sb_view_t->version of version 0 is not 1.
   */
  int version;

  void (*get_geometry_hints)(struct ui_sb_view *, unsigned int *width, unsigned int *top_margin,
                             unsigned int *bottom_margin, int *up_button_y,
                             unsigned int *up_button_height, int *down_button_y,
                             unsigned int *down_button_height);
  void (*get_default_color)(struct ui_sb_view *, char **fg_color, char **bg_color);

  /* Win32: GC is None. */
  void (*realized)(struct ui_sb_view *, Display *, int screen, Window, GC, unsigned int height);
  void (*resized)(struct ui_sb_view *, Window, unsigned int height);
  void (*color_changed)(struct ui_sb_view *, int);
  void (*destroy)(struct ui_sb_view *);

  /*
   * Win32: ui_sb_view_t::gc is set by ui_scrollbar.c before following draw_XXX
   *        functions is called.
   */

  /* drawing bar only. */
  void (*draw_scrollbar)(struct ui_sb_view *, int bar_top_y, unsigned int bar_height);
  /* drawing background of bar. */
  void (*draw_background)(struct ui_sb_view *, int, unsigned int);
  void (*draw_up_button)(struct ui_sb_view *, int);
  void (*draw_down_button)(struct ui_sb_view *, int);

  /* ui_scrollbar sets this after ui_*_sb_view_new(). */
  __ui_window_ptr_t win;

} ui_sb_view_t;

typedef struct ui_sb_view_rc {
  char *key;
  char *value;

} ui_sb_view_rc_t;

typedef struct ui_sb_view_conf {
  char *sb_name;
  char *engine_name;
  char *dir;
  ui_sb_view_rc_t *rc;
  unsigned int rc_num;
  unsigned int use_count;

  int (*load_image)(__ui_display_ptr_t disp, char *path,
                    /* u_int32_t */ unsigned int **cardinal, Pixmap *pixmap, Pixmap *mask,
                    unsigned int *width, unsigned int *height);

} ui_sb_view_conf_t;

#endif
