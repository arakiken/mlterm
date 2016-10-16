/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <ui_sb_view.h>

#include "motif_data.h"

#define WIDTH 15
#define V_MARGIN 14
#define BUTTON_SIZE 11

typedef struct motif_sb_view {
  ui_sb_view_t view;

  GC gc;
  Colormap cmap;

  unsigned long fg_lighter_color;
  unsigned long fg_darker_color;
  unsigned long bg_lighter_color;
  unsigned long bg_darker_color;

  int is_transparent;

} motif_sb_view_t;

/* --- static functions --- */

static void get_geometry_hints(ui_sb_view_t* view, unsigned int* width, unsigned int* top_margin,
                               unsigned int* bottom_margin, int* up_button_y,
                               unsigned int* up_button_height, int* down_button_y,
                               unsigned int* down_button_height) {
  *width = WIDTH;
  *top_margin = V_MARGIN;
  *bottom_margin = V_MARGIN;
  *up_button_y = 0;
  *up_button_height = V_MARGIN;
  *down_button_y = -V_MARGIN;
  *down_button_height = V_MARGIN;
}

static void get_default_color(ui_sb_view_t* view, char** fg_color, char** bg_color) {
  *fg_color = "gray";
  *bg_color = "lightgray";
}

static void realized(ui_sb_view_t* view, Display* display, int screen, Window window, GC gc,
                     unsigned int height) {
  motif_sb_view_t* motif_sb;
  XWindowAttributes attr;
  XGCValues gc_value;

  motif_sb = (motif_sb_view_t*)view;

  view->display = display;
  view->screen = screen;
  view->window = window;
  view->gc = gc;
  view->height = height;

  gc_value.foreground = BlackPixel(view->display, view->screen);
  gc_value.background = WhitePixel(view->display, view->screen);
  gc_value.graphics_exposures = 0;

  motif_sb->gc = XCreateGC(view->display, view->window,
                           GCForeground | GCBackground | GCGraphicsExposures, &gc_value);

  XGetWindowAttributes(view->display, view->window, &attr);
  motif_sb->cmap = attr.colormap;
}

static void resized(ui_sb_view_t* view, Window window, unsigned int height) {
  motif_sb_view_t* motif_sb;

  motif_sb = (motif_sb_view_t*)view;

  view->window = window;
  view->height = height;
}

static void delete (ui_sb_view_t* view) {
  motif_sb_view_t* motif_sb;

  motif_sb = (motif_sb_view_t*)view;

  if (motif_sb) {
    XFreeGC(view->display, motif_sb->gc);
    free(motif_sb);
  }
}

static unsigned short adjust_rgb(unsigned short v, float fac) {
  if (v == 0 && fac > 0) {
    v = 0x7070;
  }

  if (v * fac > 0xffff) {
    return 0xffff;
  } else {
    return (unsigned short)(v * fac);
  }
}

static void color_changed(ui_sb_view_t* view, int is_fg) {
  motif_sb_view_t* motif_sb;
  XColor color;
  XColor color_lighter;
  XColor color_darker;
  XGCValues gc_value_ret;

  motif_sb = (motif_sb_view_t*)view;

  if (motif_sb->is_transparent) {
    motif_sb->fg_lighter_color = motif_sb->bg_lighter_color =
        WhitePixel(view->display, view->screen);
    motif_sb->fg_darker_color = motif_sb->bg_darker_color = BlackPixel(view->display, view->screen);
    return;
  }

  /* query current fg/bg color pixel */
  XGetGCValues(view->display, view->gc, GCForeground | GCBackground, &gc_value_ret);

  color_darker.flags = color_lighter.flags = DoRed | DoGreen | DoBlue;

  /* fg highlight color / shade color */
  color.pixel = gc_value_ret.foreground;
  XQueryColor(view->display, motif_sb->cmap, &color);
  color_lighter.red = adjust_rgb(color.red, 1.5);
  color_lighter.green = adjust_rgb(color.green, 1.5);
  color_lighter.blue = adjust_rgb(color.blue, 1.5);
  color_darker.red = adjust_rgb(color.red, 0.5);
  color_darker.green = adjust_rgb(color.green, 0.5);
  color_darker.blue = adjust_rgb(color.blue, 0.5);
  if (XAllocColor(view->display, motif_sb->cmap, &color_lighter)) {
    motif_sb->fg_lighter_color = color_lighter.pixel;
  } else {
    motif_sb->fg_lighter_color = WhitePixel(view->display, view->screen);
  }
  if (XAllocColor(view->display, motif_sb->cmap, &color_darker)) {
    motif_sb->fg_darker_color = color_darker.pixel;
  } else {
    motif_sb->fg_darker_color = BlackPixel(view->display, view->screen);
  }

  /* bg highlight color / shade color */
  color.pixel = gc_value_ret.background;
  XQueryColor(view->display, motif_sb->cmap, &color);
  color_lighter.red = adjust_rgb(color.red, 1.5);
  color_lighter.green = adjust_rgb(color.green, 1.5);
  color_lighter.blue = adjust_rgb(color.blue, 1.5);
  color_darker.red = adjust_rgb(color.red, 0.5);
  color_darker.green = adjust_rgb(color.green, 0.5);
  color_darker.blue = adjust_rgb(color.blue, 0.5);
  if (XAllocColor(view->display, motif_sb->cmap, &color_lighter)) {
    motif_sb->bg_lighter_color = color_lighter.pixel;
  } else {
    motif_sb->bg_lighter_color = WhitePixel(view->display, view->screen);
  }
  if (XAllocColor(view->display, motif_sb->cmap, &color_darker)) {
    motif_sb->bg_darker_color = color_darker.pixel;
  } else {
    motif_sb->bg_darker_color = BlackPixel(view->display, view->screen);
  }
}

static void draw_button(ui_sb_view_t* view, char** data, unsigned int offset_y) {
  motif_sb_view_t* motif_sb;
  char cur = '\0';
  int x;
  int y;
  GC gc = NULL;
  XPoint xpoint[BUTTON_SIZE * BUTTON_SIZE];
  int i = 0;

  motif_sb = (motif_sb_view_t*)view;

  for (y = 0; y < BUTTON_SIZE; y++) {
    for (x = 0; x < BUTTON_SIZE; x++) {
      if (cur != data[y][x]) {
        if (i) {
          /* before setting gc,
                           draw stocked points */
          XDrawPoints(view->display, view->window, gc, xpoint, i, CoordModeOrigin);
          i = 0;
        }

        /* changing gc */
        if (data[y][x] == '.') {
          XSetForeground(view->display, motif_sb->gc, motif_sb->fg_lighter_color);
          gc = motif_sb->gc;
        } else if (data[y][x] == '#') {
          XSetForeground(view->display, motif_sb->gc, motif_sb->fg_darker_color);
          gc = motif_sb->gc;
        } else if (data[y][x] == ':') {
          if (motif_sb->is_transparent) {
            continue;
          }
          gc = view->gc;
        } else if (data[y][x] == ' ') {
          continue;
        }

        cur = data[y][x];
      }

      /* stocking point */
      xpoint[i].x = x + 2;
      xpoint[i].y = y + offset_y;
      i++;
    }
  }

  if (i) {
    XDrawPoints(view->display, view->window, gc, xpoint, i, CoordModeOrigin);
  }
}

static void draw_up_button(ui_sb_view_t* view, int is_pressed) {
  motif_sb_view_t* motif_sb;
  char** src;
  XSegment line[4];

  motif_sb = (motif_sb_view_t*)view;

  if (is_pressed) {
    src = arrow_up_pressed_src;
  } else {
    src = arrow_up_src;
  }

  XClearArea(view->display, view->window, 0, 0, V_MARGIN, V_MARGIN, 0);

  draw_button(view, src, 2);

  XSetForeground(view->display, motif_sb->gc, motif_sb->bg_darker_color);
  line[0].x1 = 0;
  line[0].y1 = 0;
  line[0].x2 = 14;
  line[0].y2 = 0;
  line[1].x1 = 0;
  line[1].y1 = 1;
  line[1].x2 = 13;
  line[1].y2 = 1;
  line[2].x1 = 0;
  line[2].y1 = 2;
  line[2].x2 = 0;
  line[2].y2 = 13;
  line[3].x1 = 1;
  line[3].y1 = 2;
  line[3].x2 = 1;
  line[3].y2 = 13;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 4);

  XSetForeground(view->display, motif_sb->gc, motif_sb->bg_lighter_color);
  line[0].x1 = 13;
  line[0].y1 = 2;
  line[0].x2 = 13;
  line[0].y2 = 13;
  line[1].x1 = 14;
  line[1].y1 = 1;
  line[1].x2 = 14;
  line[1].y2 = 13;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 2);
}

static void draw_down_button(ui_sb_view_t* view, int is_pressed) {
  motif_sb_view_t* motif_sb;
  char** src;
  XSegment line[4];

  motif_sb = (motif_sb_view_t*)view;

  if (is_pressed) {
    src = arrow_down_pressed_src;
  } else {
    src = arrow_down_src;
  }

  XClearArea(view->display, view->window, 0, view->height - V_MARGIN, V_MARGIN, V_MARGIN, 0);

  draw_button(view, src, view->height - BUTTON_SIZE - 2);

  XSetForeground(view->display, motif_sb->gc, motif_sb->bg_darker_color);
  line[0].x1 = 0;
  line[0].y1 = view->height - V_MARGIN;
  line[0].x2 = 0;
  line[0].y2 = view->height - 1;
  line[1].x1 = 1;
  line[1].y1 = view->height - V_MARGIN;
  line[1].x2 = 1;
  line[1].y2 = view->height - 2;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 2);

  XSetForeground(view->display, motif_sb->gc, motif_sb->bg_lighter_color);
  line[0].x1 = 13;
  line[0].y1 = view->height - V_MARGIN;
  line[0].x2 = 13;
  line[0].y2 = view->height - 1;
  line[1].x1 = 14;
  line[1].y1 = view->height - V_MARGIN;
  line[1].x2 = 14;
  line[1].y2 = view->height - 1;
  line[2].x1 = 2;
  line[2].y1 = view->height - 2;
  line[2].x2 = 12;
  line[2].y2 = view->height - 2;
  line[3].x1 = 1;
  line[3].y1 = view->height - 1;
  line[3].x2 = 12;
  line[3].y2 = view->height - 1;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 4);
}

static void draw_scrollbar(ui_sb_view_t* view, int bar_top_y, unsigned int bar_height) {
  motif_sb_view_t* motif_sb;
  XSegment line[4];

  motif_sb = (motif_sb_view_t*)view;

  /* clear */
  XClearArea(view->display, view->window, 2, V_MARGIN, WIDTH - 4, view->height - V_MARGIN * 2, 0);

  /* bar */
  if (!motif_sb->is_transparent) {
    XFillRectangle(view->display, view->window, view->gc, 2, bar_top_y, WIDTH - 4, bar_height);
  }

  /* bar's highlight */
  XSetForeground(view->display, motif_sb->gc, motif_sb->fg_lighter_color);
  line[0].x1 = 2;
  line[0].y1 = bar_top_y;
  line[0].x2 = WIDTH - 3;
  line[0].y2 = bar_top_y;
  line[1].x1 = 2;
  line[1].y1 = bar_top_y + 1;
  line[1].x2 = WIDTH - 4;
  line[1].y2 = bar_top_y + 1;
  line[2].x1 = 2;
  line[2].y1 = bar_top_y + 2;
  line[2].x2 = 2;
  line[2].y2 = bar_top_y + bar_height - 1;
  line[3].x1 = 3;
  line[3].y1 = bar_top_y + 1;
  line[3].x2 = 3;
  line[3].y2 = bar_top_y + bar_height - 2;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 4);

  /* bar's shade */
  XSetForeground(view->display, motif_sb->gc, motif_sb->fg_darker_color);
  line[0].x1 = WIDTH - 3;
  line[0].y1 = bar_top_y + 1;
  line[0].x2 = WIDTH - 3;
  line[0].y2 = bar_top_y + bar_height - 1;
  line[1].x1 = WIDTH - 4;
  line[1].y1 = bar_top_y + 2;
  line[1].x2 = WIDTH - 4;
  line[1].y2 = bar_top_y + bar_height - 1;
  line[2].x1 = 4;
  line[2].y1 = bar_top_y + bar_height - 2;
  line[2].x2 = WIDTH - 5;
  line[2].y2 = bar_top_y + bar_height - 2;
  line[3].x1 = 3;
  line[3].y1 = bar_top_y + bar_height - 1;
  line[3].x2 = WIDTH - 5;
  line[3].y2 = bar_top_y + bar_height - 1;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 4);

  /* scrollview's shade */
  XSetForeground(view->display, motif_sb->gc, motif_sb->bg_darker_color);
  line[0].x1 = 0;
  line[0].y1 = V_MARGIN;
  line[0].x2 = 0;
  line[0].y2 = view->height - V_MARGIN;
  line[1].x1 = 1;
  line[1].y1 = V_MARGIN;
  line[1].x2 = 1;
  line[1].y2 = view->height - V_MARGIN;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 2);

  /* scrollview's highlight */
  XSetForeground(view->display, motif_sb->gc, motif_sb->bg_lighter_color);
  line[0].x1 = WIDTH - 2;
  line[0].y1 = V_MARGIN;
  line[0].x2 = WIDTH - 2;
  line[0].y2 = view->height - V_MARGIN;
  line[1].x1 = WIDTH - 1;
  line[1].y1 = V_MARGIN;
  line[1].x2 = WIDTH - 1;
  line[1].y2 = view->height - V_MARGIN;
  XDrawSegments(view->display, view->window, motif_sb->gc, line, 2);
}

/* --- global functions --- */

ui_sb_view_t* ui_motif_sb_view_new(void) {
  motif_sb_view_t* motif_sb;

  if ((motif_sb = calloc(1, sizeof(motif_sb_view_t))) == NULL) {
    return NULL;
  }

  motif_sb->view.version = 1;

  motif_sb->view.get_geometry_hints = get_geometry_hints;
  motif_sb->view.get_default_color = get_default_color;
  motif_sb->view.realized = realized;
  motif_sb->view.resized = resized;
  motif_sb->view.delete = delete;

  motif_sb->view.color_changed = color_changed;
  motif_sb->view.draw_scrollbar = draw_scrollbar;
  motif_sb->view.draw_up_button = draw_up_button;
  motif_sb->view.draw_down_button = draw_down_button;

  return (ui_sb_view_t*)motif_sb;
}

ui_sb_view_t* ui_motif_transparent_sb_view_new(void) {
  motif_sb_view_t* motif_sb;

  if ((motif_sb = calloc(1, sizeof(motif_sb_view_t))) == NULL) {
    return NULL;
  }

  motif_sb->view.version = 1;

  motif_sb->view.get_geometry_hints = get_geometry_hints;
  motif_sb->view.get_default_color = get_default_color;
  motif_sb->view.realized = realized;
  motif_sb->view.resized = resized;
  motif_sb->view.delete = delete;

  motif_sb->view.color_changed = color_changed;
  motif_sb->view.draw_scrollbar = draw_scrollbar;
  motif_sb->view.draw_up_button = draw_up_button;
  motif_sb->view.draw_down_button = draw_down_button;

  motif_sb->is_transparent = 1;

  return (ui_sb_view_t*)motif_sb;
}
