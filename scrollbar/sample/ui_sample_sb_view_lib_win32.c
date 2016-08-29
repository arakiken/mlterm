/*
 *	$Id$
 */

#include "ui_sample_sb_view_lib.h"

#include <stdio.h>

/* --- global functions --- */

Pixmap ui_get_icon_pixmap(ui_sb_view_t* view, GC gc, GC memgc, char** data, unsigned int width,
                          unsigned int height, unsigned int depth, /* Not used */
                          unsigned long black,                     /* Not used */
                          unsigned long white                      /* Not used */
                          ) {
  Pixmap pix;
  char cur;
  int x;
  int y;

  pix = CreateCompatibleBitmap(gc, width, height);
  SelectObject(memgc, pix);

  cur = '\0';
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (cur != data[y][x]) {
        if (data[y][x] == ' ') {
          SelectObject(memgc, GetStockObject(WHITE_PEN));
        } else if (data[y][x] == '#') {
          SelectObject(memgc, GetStockObject(BLACK_PEN));
        } else {
          continue;
        }

        cur = data[y][x];
      }

      MoveToEx(memgc, x, y, NULL);
      LineTo(memgc, x + 1, y + 1);
    }

    x = 0;
  }

  return pix;
}

int ui_draw_icon_pixmap_fg(ui_sb_view_t* view, GC gc, char** data, unsigned int width,
                           unsigned int height) {
  int x;
  int y;
  int start;

  start = 0;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (data[y][x] == '-') {
        if (!start) {
          MoveToEx(gc, x, y, NULL);
          start = 1;
        }
      } else if (start) {
        LineTo(gc, x, y);
        start = 0;
      }
    }
  }

  return 1;
}
