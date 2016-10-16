/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SAMPLE_SB_VIEW_LIB_H__
#define __UI_SAMPLE_SB_VIEW_LIB_H__

#include <ui_sb_view.h>

typedef struct sample_sb_view {
  ui_sb_view_t view;

  GC gc;

  unsigned long black_pixel;
  unsigned long white_pixel;

  Pixmap arrow_up;
  Pixmap arrow_up_dent;
  Pixmap arrow_down;
  Pixmap arrow_down_dent;

} sample_sb_view_t;

Pixmap ui_get_icon_pixmap(ui_sb_view_t* view, GC gc,
#ifdef USE_WIN32GUI
                          GC memgc,
#endif
                          char** data, unsigned int width, unsigned int height, unsigned int depth,
                          unsigned long black, unsigned long white);

int ui_draw_icon_pixmap_fg(ui_sb_view_t* view,
#ifdef USE_WIN32GUI
                           GC gc,
#else
                           Pixmap arrow,
#endif
                           char** data, unsigned int width, unsigned int height);

#endif
