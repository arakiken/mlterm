/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_gc.h"

#include <pobl/bl_mem.h> /* malloc */

#include "../ui_color.h"

#define ARGB_TO_RGB(pixel) ((pixel)&0x00ffffff)

/* --- global functions --- */

ui_gc_t* ui_gc_new(Display* display, Drawable drawable) {
  ui_gc_t* gc;

  if ((gc = calloc(1, sizeof(ui_gc_t))) == NULL) {
    return NULL;
  }

  gc->display = display;

  /* Default value of GC. */
  gc->fg_color = RGB(0, 0, 0);
  gc->bg_color = RGB(0xff, 0xff, 0xff);

  return gc;
}

int ui_gc_delete(ui_gc_t* gc) {
  free(gc);

  return 1;
}

int ui_set_gc(ui_gc_t* gc, GC _gc) {
  gc->gc = _gc;

  SetTextAlign(gc->gc, TA_LEFT | TA_BASELINE);

  gc->fg_color = RGB(0, 0, 0); /* black */
#if 0
  /* black is default value */
  SetTextColor(gc->gc, gc->fg_color);
#endif

  gc->bg_color = RGB(0xff, 0xff, 0xff); /* white */
#if 0
  /* white is default value */
  SetBkColor(gc->gc, gc->bg_color);
#endif

  gc->fid = None;
  gc->pen = None;
  gc->brush = None;

  return 1;
}

int ui_gc_set_fg_color(ui_gc_t* gc, u_long fg_color) {
  if (ARGB_TO_RGB(fg_color) != gc->fg_color) {
    SetTextColor(gc->gc, (gc->fg_color = ARGB_TO_RGB(fg_color)));
  }

  return 1;
}

int ui_gc_set_bg_color(ui_gc_t* gc, u_long bg_color) {
  if (ARGB_TO_RGB(bg_color) != gc->bg_color) {
    SetBkColor(gc->gc, (gc->bg_color = ARGB_TO_RGB(bg_color)));
  }

  return 1;
}

int ui_gc_set_fid(ui_gc_t* gc, Font fid) {
  if (gc->fid != fid) {
    SelectObject(gc->gc, fid);
    gc->fid = fid;
  }

  return 1;
}

HPEN ui_gc_set_pen(ui_gc_t* gc, HPEN pen) {
  if (gc->pen != pen) {
    gc->pen = pen;

    return SelectObject(gc->gc, pen);
  }

  return None;
}

HBRUSH
ui_gc_set_brush(ui_gc_t* gc, HBRUSH brush) {
  if (gc->brush != brush) {
    gc->brush = brush;

    return SelectObject(gc->gc, brush);
  }

  return None;
}
