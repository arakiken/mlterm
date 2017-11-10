/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_IM_STATUS_SCREEN_H__
#define __UI_IM_STATUS_SCREEN_H__

#include <vt_char.h>
#include "ui_window.h"
#include "ui_display.h"
#include "ui_font_manager.h"
#include "ui_color_manager.h"

typedef struct ui_im_status_screen {
  ui_window_t window;

  ui_font_manager_t *font_man;   /* is the same as attached screen */
  ui_color_manager_t *color_man; /* is the same as attached screen */
  void *vtparser;                /* is the same as attached screen */

  vt_char_t *chars;
  u_int num_chars; /* == array size */
  u_int filled_len;

  int x;             /* not adjusted by window size */
  int y;             /* not adjusted by window size */
  u_int line_height; /* line height of attaced screen */

  int is_vertical;

  /*
   * methods of ui_im_status_screen_t which is called from im
   */
  void (*delete)(struct ui_im_status_screen *);
  void (*show)(struct ui_im_status_screen *);
  void (*hide)(struct ui_im_status_screen *);
  int (*set_spot)(struct ui_im_status_screen *, int, int);
  int (*set)(struct ui_im_status_screen *, ef_parser_t *, u_char *);

  int head_indexes[15]; /* 14 lines (13 candidates and N/N) + 1 */

} ui_im_status_screen_t;

ui_im_status_screen_t *ui_im_status_screen_new(ui_display_t *disp, ui_font_manager_t *font_man,
                                               ui_color_manager_t *color_man, void *vtparser,
                                               int is_vertical, u_int line_height, int x, int y);

#endif
