/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <stdlib.h>

#include <pobl/bl_debug.h>
#include <pobl/bl_mem.h>

#include "ui_sb_view.h"
#include "ui_window.h"

#define TOP_MARGIN 14
#define BOTTOM_MARGIN 14
#define HEIGHT_MARGIN (TOP_MARGIN + BOTTOM_MARGIN)
#define WIDTH 13

typedef struct simple_sb_view {
  ui_sb_view_t view;

  ui_color_t black;
  ui_color_t white;
  ui_color_t gray;

  int8_t dpr;
  int8_t is_transparent;

} simple_sb_view_t;

/* --- static variables --- */

static char *arrow_up_src[] = {
  "            #",
  " -----------#",
  " -----+-----#",
  " -----#-----#",
  " ----+#+----#",
  " ----###----#",
  " ---+###+---#",
  " ---#####---#",
  " --+#####+--#",
  " --#######--#",
  " -+#######+-#",
  " -----------#",
  " -----------#",
  "#############"

};

static char *arrow_down_src[] = {
  "            #",
  " -----------#",
  " -----------#",
  " -+#######+-#",
  " --#######--#",
  " --+#####+--#",
  " ---#####---#",
  " ---+###+---#",
  " ----###----#",
  " ----+#+----#",
  " -----#-----#",
  " -----+-----#",
  " -----------#",
  "#############"

};

static char *arrow_up_dent_src[] = {
  "            #",
  " -----------#",
  " ----- -----#",
  " ----- -----#",
  " ----   ----#",
  " ----   ----#",
  " ---     ---#",
  " ---     ---#",
  " --       --#",
  " --       --#",
  " -         -#",
  " -----------#",
  " -----------#",
  "#############"

};

static char *arrow_down_dent_src[] = {
  "            #",
  " -----------#",
  " -----------#",
  " -         -#",
  " --       --#",
  " --       --#",
  " ---     ---#",
  " ---     ---#",
  " ----   ----#",
  " ----   ----#",
  " ----- -----#",
  " ----- -----#",
  " -----------#",
  "#############"

};

/* --- static functions --- */

static size_t count_char(char c, char *str) {
  char *p;

  p = str;

  while (*p == c) {
    p++;
  }

  return p - str;
}

static int draw_icon(ui_sb_view_t *view, int y, char **data, unsigned int width,
                     unsigned int height) {
  simple_sb_view_t *simple_sb = (simple_sb_view_t*)view;
  int dpr = simple_sb->dpr;
  int x_off;
  int y_off;

  if (simple_sb->is_transparent) {
    ui_window_clear(view->win, 0, y, width, height);
  }

  for (y_off = 0; y_off < height; y_off++) {
    for (x_off = 0; x_off < width;) {
      size_t count;

      count = count_char(data[y_off][x_off], data[y_off] + x_off);

      switch(data[y_off][x_off]) {
      case '-':
        if (!simple_sb->is_transparent) {
          ui_window_fill(view->win, x_off * dpr,
                         y + y_off * dpr, count * dpr, 1 * dpr);
        }
        break;

      case '#':
        ui_window_fill_with(view->win, &simple_sb->black,
                            x_off * dpr, y + y_off * dpr,
                            count * dpr, 1 * dpr);
        break;

      case '+':
        ui_window_fill_with(view->win, &simple_sb->gray,
                            x_off * dpr, y + y_off * dpr,
                            count * dpr, 1 * dpr);
        break;

      default:
        ui_window_fill_with(view->win, &simple_sb->white,
                            x_off * dpr, y + y_off * dpr,
                            count * dpr, 1 * dpr);
        break;
      }

      x_off += count;
    }
  }

  return 1;
}

static void get_geometry_hints(ui_sb_view_t *view, unsigned int *width, unsigned int *top_margin,
                               unsigned int *bottom_margin, int *up_button_y,
                               unsigned int *up_button_height, int *down_button_y,
                               unsigned int *down_button_height) {
  int dpr = ((simple_sb_view_t*)view)->dpr;

  *width = WIDTH * dpr;
  *top_margin = TOP_MARGIN * dpr;
  *bottom_margin = BOTTOM_MARGIN * dpr;
  *up_button_y = 0;
  *up_button_height = TOP_MARGIN * dpr;
  *down_button_y = -BOTTOM_MARGIN * dpr;
  *down_button_height = BOTTOM_MARGIN * dpr;
}

static void get_default_color(ui_sb_view_t *view, char **fg_color, char **bg_color) {
  *fg_color = "gray";
  *bg_color = "lightgray";
}

static void realized(ui_sb_view_t *view, Display *display, int screen, Window window, GC gc,
                     unsigned int height) {
  simple_sb_view_t *simple_sb;

  simple_sb = (simple_sb_view_t*)view;

  view->display = display;
  view->screen = screen;
  view->window = window;
  view->gc = gc;
  view->height = height;

  ui_load_rgb_xcolor(view->win->disp, &simple_sb->black, 0x00, 0x00, 0x00, 0xff);
  ui_load_rgb_xcolor(view->win->disp, &simple_sb->gray, 0x50, 0x50, 0x50, 0xff);
  ui_load_rgb_xcolor(view->win->disp, &simple_sb->white, 0xff, 0xff, 0xff, 0xff);
}

static void resized(ui_sb_view_t *view, Window window, unsigned int height) {
  view->window = window;
  view->height = height;
}

static void destroy(ui_sb_view_t *view) { free(view); }

static void draw_arrow_up_icon(ui_sb_view_t *view, int is_dent) {
  draw_icon(view, 0, is_dent ? arrow_up_dent_src : arrow_up_src,
            WIDTH, TOP_MARGIN);
}

static void draw_arrow_down_icon(ui_sb_view_t *view, int is_dent) {
  draw_icon(view, view->height - BOTTOM_MARGIN * ((simple_sb_view_t*)view)->dpr,
            is_dent ? arrow_down_dent_src : arrow_down_src,
            WIDTH, BOTTOM_MARGIN);
}

static void draw_scrollbar(ui_sb_view_t *view, int bar_top_y, unsigned int bar_height) {
  simple_sb_view_t *simple_sb = (simple_sb_view_t*)view;
  int dpr = simple_sb->dpr;

  /* drawing bar */
  if (simple_sb->is_transparent) {
    ui_window_clear(view->win, 1 * dpr, bar_top_y, (WIDTH - 1) * dpr, bar_height);
  } else {
    ui_window_fill(view->win, 1 * dpr, bar_top_y, (WIDTH - 1) * dpr, bar_height);
  }

  /* left side shade */
  ui_window_fill_with(view->win, &simple_sb->white, 0, bar_top_y, 1 * dpr, bar_height);

  /* up side shade */
  ui_window_fill_with(view->win, &simple_sb->white, 0, bar_top_y, WIDTH * dpr, 1);

  /* right side shade */
  ui_window_fill_with(view->win, &simple_sb->black, (WIDTH - 1) * dpr, bar_top_y,
                      1 * dpr, bar_height);

  /* down side shade */
  ui_window_fill_with(view->win, &simple_sb->black, 1 * dpr, bar_top_y + bar_height - 1,
                      (WIDTH - 1) * dpr, 1);
}

static void draw_background(ui_sb_view_t *view, int y, unsigned int height) {
  ui_window_clear(view->win, 0, y, WIDTH * ((simple_sb_view_t*)view)->dpr, height);
}

static void draw_up_button(ui_sb_view_t *view, int is_pressed) {
  draw_arrow_up_icon(view, is_pressed);
}

static void draw_down_button(ui_sb_view_t *view, int is_pressed) {
  draw_arrow_down_icon(view, is_pressed);
}

/* --- global functions --- */

ui_sb_view_t *ui_simple_sb_view_new(int dpr) {
  simple_sb_view_t *simple_sb;

  if ((simple_sb = calloc(1, sizeof(simple_sb_view_t))) == NULL) {
    return NULL;
  }

  simple_sb->view.version = 1;

  simple_sb->view.get_geometry_hints = get_geometry_hints;
  simple_sb->view.get_default_color = get_default_color;
  simple_sb->view.realized = realized;
  simple_sb->view.resized = resized;
  simple_sb->view.destroy = destroy;

  simple_sb->view.draw_scrollbar = draw_scrollbar;
  simple_sb->view.draw_background = draw_background;
  simple_sb->view.draw_up_button = draw_up_button;
  simple_sb->view.draw_down_button = draw_down_button;

  simple_sb->dpr = dpr;

  return &simple_sb->view;
}

ui_sb_view_t *ui_simple_transparent_sb_view_new(int dpr) {
  simple_sb_view_t *simple_sb;

  if ((simple_sb = (simple_sb_view_t*)ui_simple_sb_view_new(dpr)) == NULL) {
    return NULL;
  }

  simple_sb->is_transparent = 1;

  return &simple_sb->view;
}
