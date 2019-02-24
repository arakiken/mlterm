/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <ui_sb_view.h>

#define WIDTH 14

typedef struct athena_sb_view {
  ui_sb_view_t view;

  int is_transparent;

} athena_sb_view_t;

/* --- static functions --- */

static void get_geometry_hints(ui_sb_view_t *view, unsigned int *width, unsigned int *top_margin,
                               unsigned int *bottom_margin, int *up_button_y,
                               unsigned int *up_button_height, int *down_button_y,
                               unsigned int *down_button_height) {
  *width = WIDTH;
  *top_margin = 1;
  *bottom_margin = 1;
  *up_button_y = 0;
  *up_button_height = 0;
  *down_button_y = 0;
  *down_button_height = 0;
}

static void get_default_color(ui_sb_view_t *view, char **fg_color, char **bg_color) {
  *fg_color = "black";
  *bg_color = "white";
}

static void realized(ui_sb_view_t *view, Display *display, int screen, Window window, GC gc,
                     unsigned int height) {
  athena_sb_view_t *athena_sb;

  athena_sb = (athena_sb_view_t *)view;

  view->display = display;
  view->screen = screen;
  view->window = window;
  view->gc = gc;
  view->height = height;
}

static void resized(ui_sb_view_t *view, Window window, unsigned int height) {
  view->window = window;
  view->height = height;
}

static void destroy(ui_sb_view_t *view) {
  athena_sb_view_t *athena_sb;

  athena_sb = (athena_sb_view_t *)view;

  if (athena_sb) {
    free(athena_sb);
  }
}

static void draw_scrollbar(ui_sb_view_t *view, int bar_top_y, unsigned int bar_height) {
  athena_sb_view_t *athena_sb;
  XPoint *points;
  unsigned short x;
  unsigned short y;
  int i = 0;
  int j;
  athena_sb = (athena_sb_view_t *)view;

  /* clear */
  XClearArea(view->display, view->window, 0, 0, WIDTH, view->height - 1, 0);

  if ((points = malloc((WIDTH * view->height) * sizeof(XPoint))) == NULL) {
    return;
  }

  /* bar */
  j = 1;
  for (y = bar_top_y; y < bar_top_y + bar_height; y++) {
    for (x = j; x < WIDTH - 1; x += 2) {
      points[i].x = x;
      points[i].y = y;
      i++;
    }
    j++;
    if (j == 3) {
      j = 1;
    }
  }
  XDrawPoints(view->display, view->window, view->gc, points, i, CoordModeOrigin);

  free(points);
}

/* --- global functions --- */

ui_sb_view_t *ui_athena_sb_view_new(void) {
  athena_sb_view_t *athena_sb;

  if ((athena_sb = calloc(1, sizeof(athena_sb_view_t))) == NULL) {
    return NULL;
  }

  athena_sb->view.version = 1;

  athena_sb->view.get_geometry_hints = get_geometry_hints;
  athena_sb->view.get_default_color = get_default_color;
  athena_sb->view.realized = realized;
  athena_sb->view.resized = resized;
  athena_sb->view.destroy = destroy;

  athena_sb->view.draw_scrollbar = draw_scrollbar;

  return (ui_sb_view_t *)athena_sb;
}

ui_sb_view_t *ui_athena_transparent_sb_view_new(void) {
  athena_sb_view_t *athena_sb;

  if ((athena_sb = calloc(1, sizeof(athena_sb_view_t))) == NULL) {
    return NULL;
  }

  athena_sb->view.version = 1;

  athena_sb->view.get_geometry_hints = get_geometry_hints;
  athena_sb->view.get_default_color = get_default_color;
  athena_sb->view.realized = realized;
  athena_sb->view.resized = resized;
  athena_sb->view.destroy = destroy;

  athena_sb->view.draw_scrollbar = draw_scrollbar;

  athena_sb->is_transparent = 1;

  return (ui_sb_view_t *)athena_sb;
}
