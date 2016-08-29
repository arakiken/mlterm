/*
 *	$Id$
 */

#ifndef __UI_IM_STATUS_SCREEN_H__
#define __UI_IM_STATUS_SCREEN_H__

#include <vt_char.h>
#include "ui_window.h"
#include "ui_display.h"
#include "ui_font_manager.h"
#include "ui_color_manager.h"

typedef struct ui_im_status_screen {
  ui_window_t window;

  ui_font_manager_t *font_man; /* is the same as attaced screen */

  ui_color_manager_t *color_man; /* is the same as attaced screen */

  vt_char_t *chars;
  u_int num_of_chars; /* == array size */
  u_int filled_len;

  int x;             /* not adjusted by window size */
  int y;             /* not adjusted by window size */
  u_int line_height; /* line height of attaced screen */

  int is_vertical;

  /*
   * methods of ui_im_status_screen_t which is called from im
   */
  int (*delete)(struct ui_im_status_screen *);
  int (*show)(struct ui_im_status_screen *);
  int (*hide)(struct ui_im_status_screen *);
  int (*set_spot)(struct ui_im_status_screen *, int, int);
  int (*set)(struct ui_im_status_screen *, ef_parser_t *, u_char *);

  int head_indexes[10];

} ui_im_status_screen_t;

ui_im_status_screen_t *ui_im_status_screen_new(ui_display_t *disp, ui_font_manager_t *font_man,
                                               ui_color_manager_t *color_man, int is_vertical,
                                               u_int line_height, int x, int y);

#endif
