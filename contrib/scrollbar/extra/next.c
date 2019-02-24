/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <ui_sb_view.h>

#include "exsb_common.h"
#include "next_data.h"

#define WIDTH 18
#define BOTTOM_MARGIN 35

#define BUTTON_SIZE 16
#define UP_BUTTON_Y(view_height) ((view_height)-BUTTON_SIZE * 2 - 2)
#define DOWN_BUTTON_Y(view_height) ((view_height)-BUTTON_SIZE - 1)

#define BAR_RELIEF_SIZE 6
#define BAR_RELIEF_X 5

typedef struct next_sb_view {
  ui_sb_view_t view;

  GC gc;

  unsigned int depth;

  Pixmap background;
  Pixmap bar_relief;
  Pixmap arrow_up;
  Pixmap arrow_up_pressed;
  Pixmap arrow_down;
  Pixmap arrow_down_pressed;

  unsigned long gray_light;
  unsigned long gray_dark;

  int has_scrollbuf;
  int is_transparent;

} next_sb_view_t;

/* --- static functions --- */

static Pixmap get_icon_pixmap(ui_sb_view_t *view, GC gc, char **data, unsigned int width,
                              unsigned int height) {
  Pixmap pix;
  next_sb_view_t *next_sb;
  char cur = '\0';
  short x;
  short y;
  XPoint *xpoint;
  int i = 0;

  next_sb = (next_sb_view_t *)view;

  pix = XCreatePixmap(view->display, view->window, width, height, next_sb->depth);

  if ((xpoint = malloc((width * height) * sizeof(XPoint))) == NULL) {
    return pix;
  }

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (cur != data[y][x]) {
        if (i) {
          /* before setting gc,
                           draw stocked points */
          XDrawPoints(view->display, pix, gc, xpoint, i, CoordModeOrigin);
          i = 0;
        }

        /* changing gc */
        if (data[y][x] == ' ') {
          XSetForeground(view->display, gc, WhitePixel(view->display, view->screen));
        } else if (data[y][x] == '#') {
          XSetForeground(view->display, gc, BlackPixel(view->display, view->screen));
        } else if (data[y][x] == '+') {
          XSetForeground(view->display, gc, next_sb->gray_dark);
        } else if (data[y][x] == '-') {
          XSetForeground(view->display, gc, next_sb->gray_light);
        }

        cur = data[y][x];
      }

      /* stocking point */
      xpoint[i].x = x;
      xpoint[i].y = y;
      i++;
    }
  }

  if (i) {
    XDrawPoints(view->display, pix, gc, xpoint, i, CoordModeOrigin);
  }

  free(xpoint);

  return pix;
}

static void get_geometry_hints(ui_sb_view_t *view, unsigned int *width, unsigned int *top_margin,
                               unsigned int *bottom_margin, int *up_button_y,
                               unsigned int *up_button_height, int *down_button_y,
                               unsigned int *down_button_height) {
  *width = WIDTH;
  *top_margin = 0;
  *bottom_margin = BOTTOM_MARGIN;
  *up_button_y = -(BUTTON_SIZE + 1) * 2;
  *up_button_height = BUTTON_SIZE;
  *down_button_y = -(BUTTON_SIZE + 1);
  *down_button_height = BUTTON_SIZE;
}

static void get_default_color(ui_sb_view_t *view, char **fg_color, char **bg_color) {
  *fg_color = "gray";
  *bg_color = "lightgray";
}

static Pixmap create_bg(ui_sb_view_t *view, int width, int height) {
  Pixmap pix;
  next_sb_view_t *next_sb;
  short x;
  short y;
  XPoint *xpoint;
  int i = 0;

  next_sb = (next_sb_view_t *)view;

  pix = XCreatePixmap(view->display, view->window, width, height, ((next_sb_view_t *)view)->depth);

  XSetForeground(view->display, next_sb->gc, next_sb->gray_light);
  XFillRectangle(view->display, pix, next_sb->gc, 0, 0, width, height);

  if ((xpoint = malloc((width * height / 2) * sizeof(XPoint))) == NULL) {
    return pix;
  }

  XSetForeground(view->display, next_sb->gc, next_sb->gray_dark);
  for (y = 0; y < height; y += 2) {
    for (x = 1; x < width - 1; x += 2) {
      xpoint[i].x = x;
      xpoint[i].y = y;
      i++;
    }
  }
  for (y = 1; y < height; y += 2) {
    for (x = 2; x < width - 1; x += 2) {
      xpoint[i].x = x;
      xpoint[i].y = y;
      i++;
    }
  }
  XDrawPoints(view->display, pix, next_sb->gc, xpoint, i, CoordModeOrigin);

  free(xpoint);

  return pix;
}

static void realized(ui_sb_view_t *view, Display *display, int screen, Window window, GC gc,
                     unsigned int height) {
  next_sb_view_t *next_sb;
  XWindowAttributes attr;
  XGCValues gc_value;

  next_sb = (next_sb_view_t *)view;

  view->display = display;
  view->screen = screen;
  view->window = window;
  view->gc = gc;
  view->height = height;

  gc_value.foreground = BlackPixel(view->display, view->screen);
  gc_value.background = WhitePixel(view->display, view->screen);
  gc_value.graphics_exposures = 0;

  next_sb->gc = XCreateGC(view->display, view->window,
                          GCForeground | GCBackground | GCGraphicsExposures, &gc_value);

  XGetWindowAttributes(view->display, view->window, &attr);
  next_sb->depth = attr.depth;

  next_sb->gray_light =
      exsb_get_pixel(view->display, view->screen, attr.colormap, attr.visual, "rgb:ae/aa/ae");
  next_sb->gray_dark =
      exsb_get_pixel(view->display, view->screen, attr.colormap, attr.visual, "rgb:51/55/51");

  next_sb->background = create_bg(view, WIDTH, view->height);
  next_sb->bar_relief =
      get_icon_pixmap(view, next_sb->gc, bar_relief_src, BAR_RELIEF_SIZE, BAR_RELIEF_SIZE);
  next_sb->arrow_up = get_icon_pixmap(view, next_sb->gc, arrow_up_src, BUTTON_SIZE, BUTTON_SIZE);
  next_sb->arrow_down =
      get_icon_pixmap(view, next_sb->gc, arrow_down_src, BUTTON_SIZE, BUTTON_SIZE);
  next_sb->arrow_up_pressed =
      get_icon_pixmap(view, next_sb->gc, arrow_up_pressed_src, BUTTON_SIZE, BUTTON_SIZE);
  next_sb->arrow_down_pressed =
      get_icon_pixmap(view, next_sb->gc, arrow_down_pressed_src, BUTTON_SIZE, BUTTON_SIZE);

  XCopyArea(view->display, next_sb->background, view->window, view->gc, 0, 0, WIDTH, view->height,
            0, 0);
}

static void resized(ui_sb_view_t *view, Window window, unsigned int height) {
  next_sb_view_t *next_sb;

  next_sb = (next_sb_view_t *)view;

  view->window = window;
  view->height = height;

  /* create new background pixmap to fit well with resized scroll view */
  XFreePixmap(view->display, next_sb->background);
  next_sb->background = create_bg(view, WIDTH, view->height);
}

static void destroy(ui_sb_view_t *view) {
  next_sb_view_t *next_sb;

  next_sb = (next_sb_view_t *)view;

  if (next_sb) {
    XFreePixmap(view->display, next_sb->background);
    XFreePixmap(view->display, next_sb->bar_relief);
    XFreePixmap(view->display, next_sb->arrow_up);
    XFreePixmap(view->display, next_sb->arrow_up_pressed);
    XFreePixmap(view->display, next_sb->arrow_down);
    XFreePixmap(view->display, next_sb->arrow_down_pressed);

    XFreeGC(view->display, next_sb->gc);

    free(next_sb);
  }
}

static void draw_up_button(ui_sb_view_t *view, int is_pressed) {
  next_sb_view_t *next_sb;
  Pixmap arrow;
  char **src;
  int x;
  int y;

  next_sb = (next_sb_view_t *)view;

  /* clear */
  if (next_sb->is_transparent) {
    XClearArea(view->display, view->window, 1, UP_BUTTON_Y(view->height), BUTTON_SIZE, BUTTON_SIZE,
               0);
  } else {
    XCopyArea(view->display, next_sb->background, view->window, view->gc, 0,
              UP_BUTTON_Y(view->height) - 1, WIDTH, BUTTON_SIZE + 2, 0,
              UP_BUTTON_Y(view->height) - 1);
  }

  /* if no scrollback buffer, not draw */
  if (!next_sb->has_scrollbuf) {
    return;
  }

  if (is_pressed) {
    arrow = next_sb->arrow_up_pressed;
    src = arrow_up_pressed_src;
  } else {
    arrow = next_sb->arrow_up;
    src = arrow_up_src;
  }

  /* drowing upper arrow button */
  if (next_sb->is_transparent) {
    for (y = 0; y < BUTTON_SIZE; y++) {
      for (x = 0; x < BUTTON_SIZE; x++) {
        if (src[y][x] == '-') {
          XCopyArea(view->display, view->window, arrow, view->gc, x + 1,
                    y + UP_BUTTON_Y(view->height), 1, 1, x, y);
        }
      }
    }
  }
  XCopyArea(view->display, arrow, view->window, view->gc, 0, 0, BUTTON_SIZE, BUTTON_SIZE, 1,
            UP_BUTTON_Y(view->height));
}

static void draw_down_button(ui_sb_view_t *view, int is_pressed) {
  next_sb_view_t *next_sb;
  Pixmap arrow;
  char **src;
  int x;
  int y;

  next_sb = (next_sb_view_t *)view;

  /* clear */
  if (next_sb->is_transparent) {
    XClearArea(view->display, view->window, 1, DOWN_BUTTON_Y(view->height), BUTTON_SIZE,
               BUTTON_SIZE, 0);
  } else {
    XCopyArea(view->display, next_sb->background, view->window, view->gc, 0,
              DOWN_BUTTON_Y(view->height), WIDTH, BUTTON_SIZE + 1, 0, DOWN_BUTTON_Y(view->height));
  }

  /* if no scrollback buffer, not draw */
  if (!next_sb->has_scrollbuf) {
    return;
  }

  if (is_pressed) {
    arrow = next_sb->arrow_up_pressed;
    src = arrow_up_pressed_src;
  } else {
    arrow = next_sb->arrow_up;
    src = arrow_up_src;
  }

  if (is_pressed) {
    arrow = next_sb->arrow_down_pressed;
    src = arrow_down_pressed_src;
  } else {
    arrow = next_sb->arrow_down;
    src = arrow_down_src;
  }

  /* drowing down arrow button */
  if (next_sb->is_transparent) {
    for (y = 0; y < BUTTON_SIZE; y++) {
      for (x = 0; x < BUTTON_SIZE; x++) {
        if (src[y][x] == '-') {
          XCopyArea(view->display, view->window, arrow, view->gc, x + 1,
                    y + DOWN_BUTTON_Y(view->height), 1, 1, x, y);
        }
      }
    }
  }
  XCopyArea(view->display, arrow, view->window, view->gc, 0, 0, BUTTON_SIZE, BUTTON_SIZE, 1,
            DOWN_BUTTON_Y(view->height));
}

static void draw_scrollbar(ui_sb_view_t *view, int bar_top_y, unsigned int bar_height) {
  next_sb_view_t *next_sb;
  XSegment line[2];

  next_sb = (next_sb_view_t *)view;

  if (bar_top_y == 0 && bar_height == view->height - BOTTOM_MARGIN) {
    /* drawing scroll view background to clear */
    /* FIXME: should use XSetWindowBackgroundPixmap() */
    if (!next_sb->is_transparent) {
      XCopyArea(view->display, next_sb->background, view->window, view->gc, 0, 0, WIDTH,
                view->height - BOTTOM_MARGIN, 0, 0);
    } else {
      XClearArea(view->display, view->window, 1, 0, WIDTH - 2, view->height - BOTTOM_MARGIN, 0);
    }
    return; /* if no scrollback buffer, not draw bar */
  }

  /* rise up/down button */
  if (next_sb->has_scrollbuf == 0) {
    next_sb->has_scrollbuf = 1;
    draw_up_button(view, 0);
    draw_down_button(view, 0);
  }

  /* clear */
  if (next_sb->is_transparent) {
    XClearArea(view->display, view->window, 1, 0, WIDTH - 2, view->height - BOTTOM_MARGIN, 0);
  } else {
    /* FIXME: should use XSetWindowBackgroundPixmap() */
    XCopyArea(view->display, next_sb->background, view->window, view->gc, 0, 0, WIDTH, bar_top_y, 0,
              0);
    XCopyArea(view->display, next_sb->background, view->window, view->gc, 0, bar_top_y, WIDTH,
              view->height - bar_top_y - bar_height - BOTTOM_MARGIN, 0, bar_top_y + bar_height);
    XSetForeground(view->display, next_sb->gc, next_sb->gray_light);
    line[0].x1 = 0;
    line[0].y1 = bar_top_y;
    line[0].x2 = 0;
    line[0].y2 = bar_top_y + view->height - 1;
    line[1].x1 = WIDTH - 1;
    line[1].y1 = bar_top_y;
    line[1].x2 = WIDTH - 1;
    line[1].y2 = bar_top_y + bar_height - 1;
    XDrawSegments(view->display, view->window, next_sb->gc, line, 2);
  }

  /* drawing bar */
  if (!next_sb->is_transparent) {
    XSetForeground(view->display, next_sb->gc, next_sb->gray_light);
    XFillRectangle(view->display, view->window, next_sb->gc, 1, bar_top_y, WIDTH - 2, bar_height);
  }

  /* drawing relief */
  if (bar_height >= BAR_RELIEF_SIZE) {
    XCopyArea(view->display, next_sb->bar_relief, view->window, next_sb->gc, 1, 0,
              BAR_RELIEF_SIZE - 2, 1, BAR_RELIEF_X + 1,
              bar_top_y + (bar_height - BAR_RELIEF_SIZE) / 2);
    XCopyArea(view->display, next_sb->bar_relief, view->window, next_sb->gc, 0, 1, BAR_RELIEF_SIZE,
              BAR_RELIEF_SIZE - 2, BAR_RELIEF_X,
              bar_top_y + (bar_height - BAR_RELIEF_SIZE) / 2 + 1);
    XCopyArea(view->display, next_sb->bar_relief, view->window, next_sb->gc, 1, 5,
              BAR_RELIEF_SIZE - 2, 1, BAR_RELIEF_X + 1,
              bar_top_y + (bar_height - BAR_RELIEF_SIZE) / 2 + 5);
#if 0
    XCopyArea(view->display, next_sb->bar_relief, view->window, next_sb->gc, 0, 0, BAR_RELIEF_SIZE,
              BAR_RELIEF_SIZE, BAR_RELIEF_X, bar_top_y + (bar_height - BAR_RELIEF_SIZE) / 2);
#endif
  }

  /* bar's highlight */
  XSetForeground(view->display, next_sb->gc, WhitePixel(view->display, view->screen));
  line[0].x1 = 1;
  line[0].y1 = bar_top_y;
  line[0].x2 = 1;
  line[0].y2 = bar_top_y + bar_height - 1;
  line[1].x1 = 2;
  line[1].y1 = bar_top_y;
  line[1].x2 = WIDTH - 3;
  line[1].y2 = bar_top_y;
  XDrawSegments(view->display, view->window, next_sb->gc, line, 2);

  /* bar's shade (black) */
  XSetForeground(view->display, next_sb->gc, BlackPixel(view->display, view->screen));
  line[0].x1 = WIDTH - 2;
  line[0].y1 = bar_top_y;
  line[0].x2 = WIDTH - 2;
  line[0].y2 = bar_top_y + bar_height - 1;
  line[1].x1 = 1;
  line[1].y1 = bar_top_y + bar_height - 1;
  line[1].x2 = WIDTH - 3;
  line[1].y2 = bar_top_y + bar_height - 1;
  XDrawSegments(view->display, view->window, next_sb->gc, line, 2);

  /* bar's shade (nextish dark gray) */
  XSetForeground(view->display, next_sb->gc, next_sb->gray_dark);
  line[0].x1 = WIDTH - 3;
  line[0].y1 = bar_top_y + 1;
  line[0].x2 = WIDTH - 3;
  line[0].y2 = bar_top_y + bar_height - 2;
  line[1].x1 = 2;
  line[1].y1 = bar_top_y + bar_height - 2;
  line[1].x2 = WIDTH - 4;
  line[1].y2 = bar_top_y + bar_height - 2;
  XDrawSegments(view->display, view->window, next_sb->gc, line, 2);
}

/* --- global functions --- */

ui_sb_view_t *ui_next_sb_view_new(void) {
  next_sb_view_t *next_sb;

  if ((next_sb = calloc(1, sizeof(next_sb_view_t))) == NULL) {
    return NULL;
  }

  next_sb->view.version = 1;

  next_sb->view.get_geometry_hints = get_geometry_hints;
  next_sb->view.get_default_color = get_default_color;
  next_sb->view.realized = realized;
  next_sb->view.resized = resized;
  next_sb->view.destroy = destroy;

  next_sb->view.draw_scrollbar = draw_scrollbar;

  next_sb->view.draw_up_button = draw_up_button;
  next_sb->view.draw_down_button = draw_down_button;

  return (ui_sb_view_t *)next_sb;
}

ui_sb_view_t *ui_next_transparent_sb_view_new(void) {
  next_sb_view_t *next_sb;

  if ((next_sb = calloc(1, sizeof(next_sb_view_t))) == NULL) {
    return NULL;
  }

  next_sb->view.version = 1;

  next_sb->view.get_geometry_hints = get_geometry_hints;
  next_sb->view.get_default_color = get_default_color;
  next_sb->view.realized = realized;
  next_sb->view.resized = resized;
  next_sb->view.destroy = destroy;

  next_sb->view.draw_scrollbar = draw_scrollbar;

  next_sb->view.draw_up_button = draw_up_button;
  next_sb->view.draw_down_button = draw_down_button;

  next_sb->is_transparent = 1;

  return (ui_sb_view_t *)next_sb;
}
