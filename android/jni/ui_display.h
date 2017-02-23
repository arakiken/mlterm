/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_DISPLAY_H__
#define ___UI_DISPLAY_H__

#include <pobl/bl_types.h>

#include "../ui_display.h"

#define CLKED 1
#define NLKED 2
#define SLKED 4
#define ALKED 8

#define KeyPress 2      /* Private in fb/ */
#define ButtonPress 4   /* Private in fb/ */
#define ButtonRelease 5 /* Private in fb/ */
#define MotionNotify 6  /* Private in fb/ */

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

int ui_display_init(struct android_app *app);

void ui_display_final(void);

int ui_display_process_event(struct android_poll_source *source, int ident);

void ui_display_unlock(void);

size_t ui_display_get_str(u_char *seq, size_t seq_len);

u_char *ui_display_get_bitmap(char *path, u_int *width, u_int *height);

void ui_display_request_text_selection(void);

void ui_display_send_text_selection(u_char *sel_data, size_t sel_len);

void ui_display_show_dialog(char *server);

void ui_display_resize(int yoffset, int width, int height,
                       int (*need_resize)(u_int, u_int, u_int, u_int));

void ui_display_update_all();

void ui_window_set_mapped_flag(ui_window_ptr_t win, int flag);

#endif
