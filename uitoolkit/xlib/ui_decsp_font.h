/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_DECSP_FONT_H__
#define __UI_DECSP_FONT_H__

#include "../ui.h"

typedef struct ui_decsp_font {
  Pixmap glyphs[0x20];
  u_int width;
  u_int height;
  u_int ascent;

} ui_decsp_font_t;

ui_decsp_font_t *ui_decsp_font_new(Display *display, u_int width, u_int height, u_int ascent);

int ui_decsp_font_delete(ui_decsp_font_t *vtgr, Display *display);

int ui_decsp_font_draw_string(ui_decsp_font_t *vtgr, Display *display, Drawable drawable, GC gc,
                              int x, int y, u_char *str, u_int len);

int ui_decsp_font_draw_image_string(ui_decsp_font_t *font, Display *display, Drawable drawable,
                                    GC gc, int x, int y, u_char *str, u_int len);

#endif
