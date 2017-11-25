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
#define FocusOut 10     /* Private in wayland/ */

/* common functions for ui_window.c */

u_long ui_display_get_pixel(ui_display_t *disp, int x, int y);

void ui_display_put_image(ui_display_t *disp, int x, int y, u_char *image, size_t size,
                          int need_fb_pixel);

void ui_display_copy_lines(ui_display_t *disp, int src_x, int src_y, int dst_x, int dst_y,
                           u_int width, u_int height);

/* common functions for ui_window.c (pseudo color) */

#define ui_display_fill_with(x, y, width, height, pixel) (0)

#define ui_cmap_get_closest_color(closest, red, green, blue) (0)

#define ui_cmap_get_pixel_rgb(red, green, blue, pixel) (0)

/* platform specific functions for ui_window.c */

int ui_display_receive_next_event_singly(ui_display_t *disp);

int ui_display_resize(ui_display_t *disp, u_int width, u_int height);

int ui_display_move(ui_display_t *disp, int x, int y);

void ui_display_request_text_selection(ui_display_t *disp);

void ui_display_send_text_selection(ui_display_t *disp, XSelectionRequestEvent *ev,
                                    u_char *sel_data, size_t sel_len);

u_char ui_display_get_char(KeySym ksym);

void ui_display_set_title(ui_display_t *disp, const u_char *name);

void ui_display_set_maximized(ui_display_t *disp);

#endif
