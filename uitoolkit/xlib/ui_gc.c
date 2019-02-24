/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "../ui_gc.h"

#include <pobl/bl_mem.h> /* malloc */

#include "../ui_color.h"

/* --- global functions --- */

ui_gc_t *ui_gc_new(Display *display, Drawable drawable) {
  ui_gc_t *gc;
  XGCValues gc_value;

  if ((gc = calloc(1, sizeof(ui_gc_t))) == NULL) {
    return NULL;
  }

  gc->display = display;

  if (drawable) {
    /* Default value of GC. */
    gc->fg_color = 0xff000000;
    gc->bg_color = 0xffffffff;

    /* Overwriting default value (1) of backgrond and foreground colors. */
    gc_value.foreground = gc->fg_color;
    gc_value.background = gc->bg_color;
    gc_value.graphics_exposures = True;
    gc->gc = XCreateGC(gc->display, drawable, GCForeground | GCBackground | GCGraphicsExposures,
                       &gc_value);
  } else {
    gc->gc = DefaultGC(display, DefaultScreen(display));
    XGetGCValues(display, gc->gc, GCForeground | GCBackground, &gc_value);
    gc->fg_color = gc_value.foreground;
    gc->bg_color = gc_value.background;
  }

  return gc;
}

void ui_gc_destroy(ui_gc_t *gc) {
  if ((gc->gc != DefaultGC(gc->display, DefaultScreen(gc->display)))) {
    XFreeGC(gc->display, gc->gc);
  }

  free(gc);
}

void ui_gc_set_fg_color(ui_gc_t *gc, u_long fg_color) {
  /* Cooperate with ui_window_copy_area(). */
  if (gc->mask) {
    XSetClipMask(gc->display, gc->gc, None);
    gc->mask = None;
  }

  if (fg_color != gc->fg_color) {
    XSetForeground(gc->display, gc->gc, fg_color);
    gc->fg_color = fg_color;
  }
}

void ui_gc_set_bg_color(ui_gc_t *gc, u_long bg_color) {
  /* Cooperate with ui_window_copy_area(). */
  if (gc->mask) {
    XSetClipMask(gc->display, gc->gc, None);
    gc->mask = None;
  }

  if (bg_color != gc->bg_color) {
    XSetBackground(gc->display, gc->gc, bg_color);
    gc->bg_color = bg_color;
  }
}

void ui_gc_set_fid(ui_gc_t *gc, Font fid) {
/* XXX Lazy skip (maybe harmless) */
#if 0
  if (gc->mask) {
    XSetClipMask(gc->display, gc->gc, None);
    gc->mask = None;
  }
#endif

  if (gc->fid != fid) {
    XSetFont(gc->display, gc->gc, fid);
    gc->fid = fid;
  }
}
