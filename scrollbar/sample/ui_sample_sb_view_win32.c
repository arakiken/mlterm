/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include <stdio.h>
#include <stdlib.h>
#include <ui_sb_view.h>

#include "ui_sample_sb_view_lib.h"
#include "ui_arrow_data.h"

#define TOP_MARGIN 0
#define BOTTOM_MARGIN 28
#define HEIGHT_MARGIN (TOP_MARGIN + BOTTOM_MARGIN)
#define WIDTH 13

/* --- static functions --- */

static void get_geometry_hints(ui_sb_view_t *view, unsigned int *width, unsigned int *top_margin,
                               unsigned int *bottom_margin, int *up_button_y,
                               unsigned int *up_button_height, int *down_button_y,
                               unsigned int *down_button_height) {
  *width = WIDTH;
  *top_margin = TOP_MARGIN;
  *bottom_margin = BOTTOM_MARGIN;
  *up_button_y = -BOTTOM_MARGIN;
  *up_button_height = BOTTOM_MARGIN / 2;
  *down_button_y = -(BOTTOM_MARGIN / 2);
  *down_button_height = BOTTOM_MARGIN / 2;
}

static void get_default_color(ui_sb_view_t *view, char **fg_color, char **bg_color) {
  *fg_color = "gray";
  *bg_color = "lightgray";
}

static void realized(ui_sb_view_t *view, Display *display, int screen, Window window,
                     GC gc, /* is None in win32 */
                     unsigned int height) {
  sample_sb_view_t *sample;

  sample = (sample_sb_view_t*)view;

  view->display = display;
  view->screen = screen;
  view->window = window;
  view->gc = gc;
  view->height = height;

  gc = GetDC(window);
  sample->gc = CreateCompatibleDC(gc);

  sample->arrow_up =
      ui_get_icon_pixmap(view, gc, sample->gc, arrow_up_src, WIDTH, BOTTOM_MARGIN / 2, 24, 0, 0);
  sample->arrow_down =
      ui_get_icon_pixmap(view, gc, sample->gc, arrow_down_src, WIDTH, BOTTOM_MARGIN / 2, 24, 0, 0);
  sample->arrow_up_dent = ui_get_icon_pixmap(view, gc, sample->gc, arrow_up_dent_src, WIDTH,
                                             BOTTOM_MARGIN / 2, 24, 0, 0);
  sample->arrow_down_dent = ui_get_icon_pixmap(view, gc, sample->gc, arrow_down_dent_src, WIDTH,
                                               BOTTOM_MARGIN / 2, 24, 0, 0);

  ReleaseDC(window, gc);
}

static void resized(ui_sb_view_t *view, Window window, unsigned int height) {
  view->window = window;
  view->height = height;
}

static void color_changed(ui_sb_view_t *view, int is_fg /* 1=fg , 0=bg */
                          ) {
  if (is_fg) {
    sample_sb_view_t *sample;
    HPEN pen;

    sample = (sample_sb_view_t*)view;

    pen = SelectObject(view->gc, GetStockObject(NULL_PEN));
    SelectObject(sample->gc, pen);

    SelectObject(sample->gc, sample->arrow_up);
    ui_draw_icon_pixmap_fg(view, sample->gc, arrow_up_src, WIDTH, BOTTOM_MARGIN / 2);

    SelectObject(sample->gc, sample->arrow_down);
    ui_draw_icon_pixmap_fg(view, sample->gc, arrow_down_src, WIDTH, BOTTOM_MARGIN / 2);

    SelectObject(sample->gc, sample->arrow_up_dent);
    ui_draw_icon_pixmap_fg(view, sample->gc, arrow_up_dent_src, WIDTH, BOTTOM_MARGIN / 2);

    SelectObject(sample->gc, sample->arrow_down_dent);
    ui_draw_icon_pixmap_fg(view, sample->gc, arrow_down_dent_src, WIDTH, BOTTOM_MARGIN / 2);

    SelectObject(view->gc, pen);
  }
}

static void destroy(ui_sb_view_t *view) {
  sample_sb_view_t *sample;

  sample = (sample_sb_view_t*)view;

  if (sample) {
    if (sample->gc) {
      DeleteDC(sample->gc);
      DeleteObject(sample->arrow_up);
      DeleteObject(sample->arrow_up_dent);
      DeleteObject(sample->arrow_down);
      DeleteObject(sample->arrow_down_dent);
    }

    free(sample);
  }
}

static void draw_arrow_up_icon(ui_sb_view_t *view, int is_dent) {
  sample_sb_view_t *sample;
  Pixmap arrow;

  sample = (sample_sb_view_t*)view;

  if (is_dent) {
    arrow = sample->arrow_up_dent;
  } else {
    arrow = sample->arrow_up;
  }

  SelectObject(sample->gc, arrow);
  BitBlt(view->gc, 0, view->height - BOTTOM_MARGIN, WIDTH, BOTTOM_MARGIN / 2, sample->gc, 0, 0,
         SRCCOPY);
}

static void draw_arrow_down_icon(ui_sb_view_t *view, int is_dent) {
  sample_sb_view_t *sample;
  Pixmap arrow;

  sample = (sample_sb_view_t*)view;

  if (is_dent) {
    arrow = sample->arrow_down_dent;
  } else {
    arrow = sample->arrow_down;
  }

  SelectObject(sample->gc, arrow);
  BitBlt(view->gc, 0, view->height - BOTTOM_MARGIN / 2, WIDTH, BOTTOM_MARGIN / 2, sample->gc, 0, 0,
         SRCCOPY);
}

static void draw_scrollbar(ui_sb_view_t *view, int bar_top_y, unsigned int bar_height) {
  HPEN old_pen;

  /* drawing bar */
  Rectangle(view->gc, 1, bar_top_y, WIDTH, bar_top_y + bar_height);

  /* left side shade */
  old_pen = SelectObject(view->gc, GetStockObject(WHITE_PEN));
  MoveToEx(view->gc, 0, bar_top_y, NULL);
  LineTo(view->gc, 0, bar_top_y + bar_height);

  /* up side shade */
  MoveToEx(view->gc, 0, bar_top_y, NULL);
  LineTo(view->gc, WIDTH - 1, bar_top_y);

  /* right side shade */
  SelectObject(view->gc, GetStockObject(BLACK_PEN));
  MoveToEx(view->gc, WIDTH - 1, bar_top_y, NULL);
  LineTo(view->gc, WIDTH - 1, bar_top_y + bar_height - 1);

  /* down side shade */
  MoveToEx(view->gc, 1, bar_top_y + bar_height - 1, NULL);
  LineTo(view->gc, WIDTH, bar_top_y + bar_height - 1);

  SelectObject(view->gc, old_pen);
}

static void draw_background(ui_sb_view_t *view, int y, unsigned int height) {
  /* XXX Garbage is left in screen in scrolling without +1. Related to NULL_PEN
   * ? */
  Rectangle(view->gc, 0, y, WIDTH + 1, y + height + 1);
}

static void draw_up_button(ui_sb_view_t *view, int is_pressed) {
  draw_arrow_up_icon(view, is_pressed);
}

static void draw_down_button(ui_sb_view_t *view, int is_pressed) {
  draw_arrow_down_icon(view, is_pressed);
}

/* --- global functions --- */

ui_sb_view_t *ui_sample_sb_view_new(void) {
  sample_sb_view_t *sample;

  if ((sample = calloc(1, sizeof(sample_sb_view_t))) == NULL) {
    return NULL;
  }

  sample->view.version = 1;

  sample->view.get_geometry_hints = get_geometry_hints;
  sample->view.get_default_color = get_default_color;
  sample->view.realized = realized;
  sample->view.resized = resized;
  sample->view.color_changed = color_changed;
  sample->view.destroy = destroy;

  sample->view.draw_scrollbar = draw_scrollbar;
  sample->view.draw_background = draw_background;
  sample->view.draw_up_button = draw_up_button;
  sample->view.draw_down_button = draw_down_button;

  return (ui_sb_view_t*)sample;
}
