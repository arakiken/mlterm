/*
 *	$Id$
 */

#ifndef __UI_GC_H__
#define __UI_GC_H__

#include <pobl/bl_types.h> /* u_int */

#include "ui.h"

typedef struct ui_gc {
#ifdef USE_FRAMEBUFFER
  int gc; /* dummy */
#else
  Display* display;
  GC gc;
  u_long fg_color; /* alpha bits are always 0 in win32. */
  u_long bg_color; /* alpha bits are always 0 in win32. */
  Font fid;
#ifdef USE_WIN32GUI
  HPEN pen;
  HBRUSH brush;
#else
  PixmapMask mask;
#endif
#endif /* USE_FRAMEBUFFER */

} ui_gc_t;

ui_gc_t* ui_gc_new(Display* display, Drawable drawable);

int ui_gc_delete(ui_gc_t* gc);

int ui_gc_set_fg_color(ui_gc_t* gc, u_long fg_color);

int ui_gc_set_bg_color(ui_gc_t* gc, u_long bg_color);

int ui_gc_set_fid(ui_gc_t* gc, Font fid);

#ifdef USE_WIN32GUI

int ui_set_gc(ui_gc_t* gc, GC _gc);

HPEN ui_gc_set_pen(ui_gc_t* gc, HPEN pen);

HBRUSH ui_gc_set_brush(ui_gc_t* gc, HBRUSH brush);

#endif

#endif
