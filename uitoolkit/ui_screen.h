/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SCREEN_H__
#define __UI_SCREEN_H__

#include <stdio.h> /* FILE */
#include <pobl/bl_types.h> /* u_int/int8_t/size_t */
#include <vt_term.h>

#include "ui_window.h"
#include "ui_selection.h"
#include "ui_shortcut.h"
#include "ui_mod_meta_mode.h"
#include "ui_bel_mode.h"
#include "ui_sb_mode.h"
#include "ui_im.h"
#include "ui_picture.h"
#include "ui_copymode.h"

typedef struct ui_screen *ui_screen_ptr_t;

typedef struct ui_system_event_listener {
  void *self;

  void (*open_screen)(void *, ui_screen_ptr_t);
  void (*split_screen)(void *, ui_screen_ptr_t, int, const char *);
  int (*close_screen)(void *, ui_screen_ptr_t, int);
  int (*next_screen)(void *, ui_screen_ptr_t);
  int (*prev_screen)(void *, ui_screen_ptr_t);
  int (*resize_screen)(void *, ui_screen_ptr_t, int, const char *);
  void (*open_pty)(void *, ui_screen_ptr_t, char *);
  void (*next_pty)(void *, ui_screen_ptr_t);
  void (*prev_pty)(void *, ui_screen_ptr_t);
  void (*close_pty)(void *, ui_screen_ptr_t, char *);

  void (*pty_closed)(void *, ui_screen_ptr_t);

  int (*mlclient)(void *, ui_screen_ptr_t, char *, FILE *);

  void (*font_config_updated)(void);
  void (*color_config_updated)(void);

  /* for debug */
  void (*exit)(void *, int);

} ui_system_event_listener_t;

typedef struct ui_screen_scroll_event_listener {
  void *self;

  void (*bs_mode_entered)(void *);
  void (*bs_mode_exited)(void *);
  void (*scrolled_upward)(void *, u_int);
  void (*scrolled_downward)(void *, u_int);
  void (*scrolled_to)(void *, int);
  void (*log_size_changed)(void *, u_int);
  void (*line_height_changed)(void *, u_int);
  void (*change_fg_color)(void *, const char *);
  char *(*fg_color)(void *);
  void (*change_bg_color)(void *, const char *);
  char *(*bg_color)(void *);
  void (*change_view)(void *, const char *);
  char *(*view_name)(void *);
  void (*transparent_state_changed)(void *, int, ui_picture_modifier_t *);
  ui_sb_mode_t (*sb_mode)(void *);
  void (*change_sb_mode)(void *, ui_sb_mode_t);
  void (*term_changed)(void *, u_int, u_int);
  void (*screen_color_changed)(void *);

} ui_screen_scroll_event_listener_t;

typedef struct ui_screen {
  ui_window_t window;

  ui_font_manager_t *font_man;

  ui_color_manager_t *color_man;

  vt_term_t *term;

  ui_selection_t sel;

  vt_screen_event_listener_t screen_listener;
  vt_xterm_event_listener_t xterm_listener;
  vt_config_event_listener_t config_listener;
  vt_pty_event_listener_t pty_listener;

  ui_sel_event_listener_t sel_listener;
  ui_xim_event_listener_t xim_listener;
  ui_im_event_listener_t im_listener;

  ui_shortcut_t *shortcut;

  char *mod_meta_key;
  u_int mod_meta_mask;
  u_int mod_ignore_mask;
  /* ui_mod_meta_mode_t */ int8_t mod_meta_mode;

  /* ui_bel_mode_t */ int8_t bel_mode;

  int8_t autoscroll_count;
#ifdef FLICK_SCROLL
  int8_t grab_scroll;
  Time flick_time;
  int16_t flick_base_x;
  int16_t flick_cur_y;
  int16_t flick_base_y;
#endif

  int8_t is_preediting;
  u_int im_preedit_beg_row;
  u_int im_preedit_end_row;
  char *input_method;
  ui_im_t *im;

  /*
   * ui_window_t::{width|height} might contain right and bottom margins if window is maximized.
   * ui_screen_t::{width|height} never contains no margins.
   */
  u_int width;
  u_int height;

  u_int screen_width_ratio;

  ui_system_event_listener_t *system_listener;
  ui_screen_scroll_event_listener_t *screen_scroll_listener;

  int scroll_cache_rows;
  int scroll_cache_boundary_start;
  int scroll_cache_boundary_end;

  char *pic_file_path;
  ui_picture_modifier_t pic_mod;
  ui_picture_t *bg_pic;

  ui_icon_picture_t *icon;

  ui_copymode_t *copymode;

  int16_t prev_inline_pic;

  u_int16_t prev_mouse_report_col;
  u_int16_t prev_mouse_report_row;

  u_int8_t fade_ratio;
  int8_t line_space;
  int8_t receive_string_via_ucs;
  int8_t use_vertical_cursor;
  int8_t borderless;
  int8_t font_or_color_config_updated; /* 0x1 = font updated, 0x2 = color
                                          updated */
  int8_t blink_wait;
  int8_t hide_underline;
  int8_t underline_offset;
  int8_t baseline_offset;
  int8_t processing_vtseq;
  int8_t anim_wait;
  int8_t hide_pointer;
  int8_t mark_drawn;

} ui_screen_t;

void ui_exit_backscroll_by_pty(int flag);

void ui_allow_change_shortcut(int flag);

void ui_set_mod_meta_prefix(char *prefix);

#define ui_free_mod_meta_prefix() ui_set_mod_meta_prefix(NULL)

void ui_set_trim_trailing_newline_in_pasting(int trim);

void ui_set_mod_keys_to_stop_mouse_report(const char *keys);

#ifdef USE_IM_CURSOR_COLOR
void ui_set_im_cursor_color(char *color);
#endif

ui_screen_t *ui_screen_new(vt_term_t *term, ui_font_manager_t *font_man,
                           ui_color_manager_t *color_man, u_int brightness, u_int contrast,
                           u_int gamma, u_int alpha, u_int fade_ratio, ui_shortcut_t *shortcut,
                           u_int screen_width_ratio, char *mod_meta_key,
                           ui_mod_meta_mode_t mod_meta_mode, ui_bel_mode_t bel_mode,
                           int receive_string_via_ucs, char *pic_file_path, int use_transbg,
                           int use_vertical_cursor, int borderless, int line_space,
                           char *input_method, int allow_osc52, u_int hmargin,
                           u_int vmargin, int hide_underline, int underline_offset,
                           int baseline_offset);

void ui_screen_destroy(ui_screen_t *screen);

int ui_screen_attach(ui_screen_t *screen, vt_term_t *term);

int ui_screen_attached(ui_screen_t *screen);

vt_term_t *ui_screen_detach(ui_screen_t *screen);

void ui_set_system_listener(ui_screen_t *screen, ui_system_event_listener_t *system_listener);

void ui_set_screen_scroll_listener(ui_screen_t *screen,
                                   ui_screen_scroll_event_listener_t *screen_scroll_listener);

void ui_screen_scroll_upward(ui_screen_t *screen, u_int size);

void ui_screen_scroll_downward(ui_screen_t *screen, u_int size);

void ui_screen_scroll_to(ui_screen_t *screen, int row);

u_int ui_col_width(ui_screen_t *screen);

u_int ui_line_height(ui_screen_t *screen);

u_int ui_line_ascent(ui_screen_t *screen);

int ui_screen_exec_cmd(ui_screen_t *screen, char *cmd);

int ui_screen_set_config(ui_screen_t *screen, const char *dev, const char *key, const char *value);

void ui_screen_reset_view(ui_screen_t *screen);

#ifdef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
void ui_screen_reload_color_cache(ui_screen_t *screen, int do_unload);
#endif

ui_picture_modifier_t *ui_screen_get_picture_modifier(ui_screen_t *screen);

void ui_screen_set_pointer_motion_event_mask(ui_screen_t *screen, int flag);

#endif
