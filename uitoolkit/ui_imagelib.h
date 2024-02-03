/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_IMAGELIB_H__
#define __UI_IMAGELIB_H__

#include "ui_window.h"
#include "ui_picture.h"

typedef struct _GdkPixbuf* GdkPixbufPtr;

void ui_imagelib_display_opened(Display *disp);

void ui_imagelib_display_closed(Display *disp);

Pixmap ui_imagelib_load_file_for_background(ui_window_t *win, char *path,
                                            ui_picture_modifier_t *pic_mod);

Pixmap ui_imagelib_get_transparent_background(ui_window_t *win, ui_picture_modifier_t *pic_mod);

int ui_imagelib_load_file(ui_display_t *disp, char *path, int keep_aspect,
                          u_int32_t **cardinal, Pixmap *pixmap, PixmapMask *mask,
                          u_int *width, u_int *height, int *transparent);

void ui_destroy_image(Display *display, Pixmap pixmap);

#ifdef USE_XLIB
Pixmap ui_imagelib_pixbuf_to_pixmap(ui_window_t *win, ui_picture_modifier_t *pic_mod,
                                    GdkPixbufPtr pixbuf);

#define ui_destroy_mask(display, mask) if (mask) { ui_destroy_image(display, mask); }
#else
#define ui_imagelib_pixbuf_to_pixmap(win, pic_mod, pixbuf) (None)
void ui_destroy_mask(Display *display, PixmapMask mask);
#endif

#endif
