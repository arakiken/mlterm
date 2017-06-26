/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_IM_H__
#define __UI_IM_H__

#include <pobl/bl_dlfcn.h>
#include <vt_char_encoding.h>
#include <vt_iscii.h>

#include "ui.h" /* KeySym, XKeyEvent */
#include "ui_im_candidate_screen.h"
#include "ui_im_status_screen.h"

#define UI_IM_PREEDIT_NOCURSOR -1

/*
 * information for the current preedit string
 */
typedef struct ui_im_preedit {
  vt_char_t *chars;
  u_int num_of_chars; /* == array size */
  u_int filled_len;

  int segment_offset;
  int cursor_offset;

} ui_im_preedit_t;

typedef struct ui_im_event_listener {
  void *self;

  int (*get_spot)(void *, vt_char_t *, int, int *, int *);
  u_int (*get_line_height)(void *);
  int (*is_vertical)(void *);
  int (*draw_preedit_str)(void *, vt_char_t *, u_int, int);
  void (*im_changed)(void *, char *);
  int (*compare_key_state_with_modmap)(void *, u_int, int *, int *, int *, int *, int *, int *,
                                       int *, int *);
  void (*write_to_term)(void *, u_char *, size_t);

} ui_im_event_listener_t;

/*
 * dirty hack to replace -export-dynamic option of libtool
 */
typedef struct ui_im_export_syms {
  int (*vt_str_init)(vt_char_t *, u_int);
  int (*vt_str_delete)(vt_char_t *, u_int);
  vt_char_t *(*vt_char_combine)(vt_char_t *, u_int32_t, ef_charset_t, int, int, vt_color_t,
                                vt_color_t, int, int, int, int, int, int);
  int (*vt_char_set)(vt_char_t *, u_int32_t, ef_charset_t cs, int, int, vt_color_t, vt_color_t,
                     int, int, int, int, int, int);
  char *(*vt_get_char_encoding_name)(vt_char_encoding_t);
  vt_char_encoding_t (*vt_get_char_encoding)(const char *);
  int (*vt_convert_to_internal_ch)(void *, ef_char_t *);
  vt_isciikey_state_t (*vt_isciikey_state_new)(int);
  int (*vt_isciikey_state_delete)(vt_isciikey_state_t);
  size_t (*vt_convert_ascii_to_iscii)(vt_isciikey_state_t, u_char *, size_t, u_char *, size_t);
  ef_parser_t *(*vt_char_encoding_parser_new)(vt_char_encoding_t);
  ef_conv_t *(*vt_char_encoding_conv_new)(vt_char_encoding_t);

  ui_im_candidate_screen_t *(*ui_im_candidate_screen_new)(ui_display_t *, ui_font_manager_t *,
                                                          ui_color_manager_t *, void *, int,
                                                          int, u_int, int, int);
  ui_im_status_screen_t *(*ui_im_status_screen_new)(ui_display_t *, ui_font_manager_t *,
                                                    ui_color_manager_t *, void *, int,
                                                    u_int, int, int);
  int (*ui_event_source_add_fd)(int, void (*handler)(void));
  int (*ui_event_source_remove_fd)(int);

} ui_im_export_syms_t;

/*
 * input method module object
 */
typedef struct ui_im {
  bl_dl_handle_t handle;
  char *name;

  ui_display_t *disp;
  ui_font_manager_t *font_man;
  ui_color_manager_t *color_man;
  void *vtparser;

  ui_im_event_listener_t *listener;

  ui_im_candidate_screen_t *cand_screen;
  ui_im_status_screen_t *stat_screen;

  ui_im_preedit_t preedit;

  /*
   * methods
   */

  int (*delete)(struct ui_im *);
  /* Return 1 if key event to be processed is still left. */
  int (*key_event)(struct ui_im *, u_char, KeySym, XKeyEvent *);
  /* Return 1 if switching is succeeded. */
  int (*switch_mode)(struct ui_im *);
  /* Return 1 if input method is active. */
  int (*is_active)(struct ui_im *);
  void (*focused)(struct ui_im *);
  void (*unfocused)(struct ui_im *);

} ui_im_t;

ui_im_t *ui_im_new(ui_display_t *disp, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
                   void *vtparser, ui_im_event_listener_t *im_listener,
                   char *input_method, u_int mod_ignore_mask);

void ui_im_delete(ui_im_t *xim);

void ui_im_redraw_preedit(ui_im_t *im, int is_focused);

#define IM_API_VERSION 0x0a
#define IM_API_COMPAT_CHECK_MAGIC                                       \
  (((IM_API_VERSION & 0x0f) << 28) | ((sizeof(ui_im_t) & 0xff) << 20) | \
   ((sizeof(ui_im_export_syms_t) & 0xff) << 12) | (sizeof(ui_im_candidate_screen_t) & 0xfff))

#endif
