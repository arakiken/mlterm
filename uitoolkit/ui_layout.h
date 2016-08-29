/*
 *	$Id$
 */

#ifndef __UI_LAYOUT_H__
#define __UI_LAYOUT_H__

#include <pobl/bl_types.h> /* int8_t */

#include "ui_screen.h"
#include "ui_scrollbar.h"
#include "ui_color_manager.h"

#define UI_SCREEN_TO_LAYOUT(screen) ((ui_layout_t *)(screen)->window.parent)

typedef struct ui_layout {
  ui_window_t window;

  struct terminal {
    ui_scrollbar_t scrollbar;
    ui_screen_t *screen;
    ui_sb_mode_t sb_mode;
    ui_scrollbar_event_listener_t sb_listener;
    ui_screen_scroll_event_listener_t screen_scroll_listener;

    u_int16_t separator_x;
    u_int16_t separator_y;
    int yfirst;
    int8_t autohide_scrollbar;
    int8_t idling_count;

    /* 0: right, 1: down */
    struct terminal *next[2];

  } term;

  char *pic_file_path;
  ui_picture_modifier_t pic_mod;
  ui_picture_t *bg_pic;

  void (*line_scrolled_out)(void *);
  void (*pointer_motion)(ui_window_t *, XMotionEvent *);

} ui_layout_t;

ui_layout_t *ui_layout_new(ui_screen_t *screen, char *view_name, char *fg_color, char *bg_color,
                           ui_sb_mode_t mode, u_int hmargin, u_int vmargin);

int ui_layout_delete(ui_layout_t *layout);

int ui_layout_add_child(ui_layout_t *layout, ui_screen_t *screen, int horizontal,
                        const char *percent);

int ui_layout_remove_child(ui_layout_t *layout, ui_screen_t *screen);

int ui_layout_switch_screen(ui_layout_t *layout, int prev);

int ui_layout_resize(ui_layout_t *layout, ui_screen_t *screen, int horizontal, int step);

#define ui_layout_has_one_child(layout) \
  ((layout)->term.next[0] == NULL && ((layout)->term.next[1]) == NULL)

#endif
