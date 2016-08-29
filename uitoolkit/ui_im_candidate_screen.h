/*
 *	$Id$
 */

#ifndef __UI_IM_CANDIDATE_SCREEN_H__
#define __UI_IM_CANDIDATE_SCREEN_H__

#include <vt_char.h>
#include <vt_parser.h> /* vt_unicode_policy_t */

#include "ui_window.h"
#include "ui_display.h"
#include "ui_font_manager.h"
#include "ui_color_manager.h"

typedef struct ui_im_candidate {
  u_short info; /* to store misc. info from IM plugins */

  vt_char_t *chars;
  u_int num_of_chars; /* == array size */
  u_int filled_len;

} ui_im_candidate_t;

typedef struct ui_im_candidate_event_listener {
  void *self;
  void (*selected)(void *p, u_int index);

} ui_im_candidate_event_listener_t;

typedef struct ui_im_candidate_screen {
  ui_window_t window;

  ui_font_manager_t *font_man;   /* same as attached screen */
  ui_color_manager_t *color_man; /* same as attached screen */

  ui_im_candidate_t *candidates;
  u_int num_of_candidates; /* == array size          */

  u_int num_per_window;

  u_int index; /* current selected index of candidates   */

  int x;             /* not adjusted by window size            */
  int y;             /* not adjusted by window size            */
  u_int line_height; /* line height of attaced screen          */

  int8_t is_vertical_term;
  int8_t is_vertical_direction;
  int8_t need_redraw;
  int dummy; /* XXX keep ABI */

  vt_unicode_policy_t unicode_policy;

  /* ui_im_candidate_screen.c -> im plugins */
  ui_im_candidate_event_listener_t listener;

  /*
   * methods for ui_im_candidate_screen_t which is called from im
   */
  int (*delete)(struct ui_im_candidate_screen *);
  int (*show)(struct ui_im_candidate_screen *);
  int (*hide)(struct ui_im_candidate_screen *);
  int (*set_spot)(struct ui_im_candidate_screen *, int, int);
  int (*init)(struct ui_im_candidate_screen *, u_int, u_int);
  int (*set)(struct ui_im_candidate_screen *, ef_parser_t *, u_char *, u_int);
  int (*select)(struct ui_im_candidate_screen *cand_screen, u_int);

} ui_im_candidate_screen_t;

ui_im_candidate_screen_t *ui_im_candidate_screen_new(
    ui_display_t *disp, ui_font_manager_t *font_man, ui_color_manager_t *color_man,
    int is_vertical_term, int is_vertical_direction, vt_unicode_policy_t unicode_policy,
    u_int line_height_of_screen, int x, int y);

#endif
