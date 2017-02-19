/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_DISPLAY_H__
#define __UI_DISPLAY_H__

#include <pobl/bl_types.h> /* u_int */

#include "ui.h"
#include "ui_gc.h"

#define XC_nil 1000

/* Defined in ui_window.h */
typedef struct ui_window *ui_window_ptr_t;

typedef struct ui_modifier_mapping {
  u_long serial;
  XModifierKeymap *map;

} ui_modifier_mapping_t;

typedef struct ui_display {
  /*
   * Public(read only)
   */
  Display *display; /* Don't change position, which pixmap_engine depends on. */
  int screen;       /* DefaultScreen */
  char *name;

  Window my_window; /* DefaultRootWindow */

#ifdef USE_XLIB
  /* Only one visual, colormap or depth is permitted per display. */
  Visual *visual;
  Colormap colormap;
#endif
  u_int depth;
  ui_gc_t *gc;

  u_int width;
  u_int height;

  /*
   * Private
   */
  ui_window_ptr_t *roots;
  u_int num_of_roots;

  ui_window_ptr_t selection_owner;

  ui_modifier_mapping_t modmap;

#if !defined(USE_FRAMEBUFFER) && !defined(USE_CONSOLE) && !defined(USE_WAYLAND)
  Cursor cursors[3];
#endif

} ui_display_t;

ui_display_t *ui_display_open(char *disp_name, u_int depth);

int ui_display_close(ui_display_t *disp);

int ui_display_close_all(void);

ui_display_t **ui_get_opened_displays(u_int *num);

int ui_display_fd(ui_display_t *disp);

int ui_display_show_root(ui_display_t *disp, ui_window_ptr_t root, int x, int y, int hint,
                         char *app_name, Window parent_window);

int ui_display_remove_root(ui_display_t *disp, ui_window_ptr_t root);

void ui_display_idling(ui_display_t *disp);

int ui_display_receive_next_event(ui_display_t *disp);

#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE) || defined(USE_QUARTZ)
#define ui_display_sync(disp) (0)
#elif defined(USE_WIN32GUI)
#define ui_display_sync(disp) ui_display_receive_next_event(disp)
#else
void ui_display_sync(ui_display_t *disp);
#endif

/*
 * Folloing functions called from ui_window.c
 */

int ui_display_own_selection(ui_display_t *disp, ui_window_ptr_t win);

int ui_display_clear_selection(ui_display_t *disp, ui_window_ptr_t win);

XModifierKeymap *ui_display_get_modifier_mapping(ui_display_t *disp);

void ui_display_update_modifier_mapping(ui_display_t *disp, u_int serial);

XID ui_display_get_group_leader(ui_display_t *disp);

#if defined(USE_FRAMEBUFFER) || defined(USE_CONSOLE)
void ui_display_set_use_ansi_colors(int use);

int ui_display_reset_cmap(void);

void ui_display_rotate(int rotate);

void ui_display_reset_input_method_window(void);
#else
#define ui_display_set_use_ansi_colors(use) (0)
#define ui_display_reset_cmap() (0)
#endif

#ifdef USE_CONSOLE
#include <vt_char_encoding.h>

void ui_display_set_char_encoding(ui_display_t *disp, vt_char_encoding_t encoding);

#ifdef USE_LIBSIXEL
void ui_display_set_sixel_colors(ui_display_t *disp, const char *colors);
#else
#define ui_display_set_sixel_colors(disp, colors) (0)
#endif

void ui_display_set_default_cell_size(u_int width, u_int height);
#endif

#endif
