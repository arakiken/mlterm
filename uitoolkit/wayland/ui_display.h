/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_DISPLAY_H__
#define ___UI_DISPLAY_H__

#include "../ui_display.h"

#define CLKED 1
#define NLKED 2
#define SLKED 4
#define ALKED 8

#define KeyPress 2      /* Private in wayland/ */
#define ButtonPress 4   /* Private in wayland/ */
#define ButtonRelease 5 /* Private in wayland/ */
#define MotionNotify 6  /* Private in wayland/ */

u_long ui_display_get_pixel(int x, int y, ui_display_t *disp);

void ui_display_put_image(int x, int y, u_char *image, size_t size, int need_fb_pixel,
                          ui_display_t *disp);

void ui_display_fill_with(int x, int y, u_int width, u_int height, u_int8_t pixel);

void ui_display_copy_lines(int src_x, int src_y, int dst_x, int dst_y, u_int width, u_int height,
                           ui_display_t *disp);

int ui_display_create_surface(ui_display_t *disp, u_int width, u_int height);

int ui_display_resize(ui_display_t *disp, u_int width, u_int height);

int ui_cmap_get_closest_color(u_long *closest, int red, int green, int blue);

int ui_cmap_get_pixel_rgb(u_int8_t *red, u_int8_t *green, u_int8_t *blue, u_long pixel);

#endif
