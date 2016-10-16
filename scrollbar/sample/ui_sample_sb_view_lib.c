/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_sample_sb_view_lib.h"

#include <stdio.h>

/* --- global functions --- */

Pixmap ui_get_icon_pixmap(ui_sb_view_t* view, GC gc, char** data, unsigned int width,
                          unsigned int height, unsigned int depth, unsigned long black,
                          unsigned long white) {
  Pixmap pix;
  char cur;
  int x;
  int y;

  pix = XCreatePixmap(view->display, view->window, width, height, depth);

  cur = '\0';
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (cur != data[y][x]) {
        if (data[y][x] == ' ') {
          XSetForeground(view->display, gc, white);
        } else if (data[y][x] == '#') {
          XSetForeground(view->display, gc, black);
        } else {
          continue;
        }

        cur = data[y][x];
      }

      XDrawPoint(view->display, pix, gc, x, y);
    }

    x = 0;
  }

  return pix;
}

int ui_draw_icon_pixmap_fg(ui_sb_view_t* view, Pixmap arrow, char** data, unsigned int width,
                           unsigned int height) {
  int x;
  int y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (data[y][x] == '-') {
        XDrawPoint(view->display, arrow, view->gc, x, y);
      }
    }
  }

  return 1;
}
