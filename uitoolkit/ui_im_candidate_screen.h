/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_IM_CANDIDATE_SCREEN_H__
#define __UI_IM_CANDIDATE_SCREEN_H__

#include <vt_char.h>

#include "ui_window.h"
#include "ui_display.h"
#include "ui_font_manager.h"
#include "ui_color_manager.h"

typedef struct ui_im_candidate {
  u_short info; /* to store misc. info from IM plugins */

  vt_char_t *chars;
  u_int num_chars; /* == array size */
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
  void *vtparser;                /* same as attached screen */

  ui_im_candidate_t *candidates;
  u_int num_candidates; /* == array size          */

  u_int num_per_window;

  u_int index; /* current selected index of candidates   */

  int x;             /* not adjusted by window size            */
  int y;             /* not adjusted by window size            */
  u_int line_height; /* line height of attaced screen          */

  int8_t is_vertical_term;
  int8_t is_vertical_direction;
  int8_t need_redraw;

  /* ui_im_candidate_screen.c -> im plugins */
  ui_im_candidate_event_listener_t listener;

  /*
   * methods for ui_im_candidate_screen_t which is called from im
   */
  void (*destroy)(struct ui_im_candidate_screen *);
  void (*show)(struct ui_im_candidate_screen *);
  void (*hide)(struct ui_im_candidate_screen *);
  int (*set_spot)(struct ui_im_candidate_screen *, int, int);
  int (*init)(struct ui_im_candidate_screen *, u_int, u_int);
  int (*set)(struct ui_im_candidate_screen *, ef_parser_t *, const u_char *, u_int);
  int (*select)(struct ui_im_candidate_screen *cand_screen, u_int);

} ui_im_candidate_screen_t;

ui_im_candidate_screen_t *ui_im_candidate_screen_new(ui_display_t *disp,
                                                     ui_font_manager_t *font_man,
                                                     ui_color_manager_t *color_man,
                                                     void *vtparser, int is_vertical_term,
                                                     int is_vertical_direction,
                                                     u_int line_height_of_screen, int x, int y);

#endif
