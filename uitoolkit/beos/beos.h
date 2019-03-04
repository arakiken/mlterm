/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BEOS_H__
#define __BEOS_H__

#include <pobl/bl_types.h>

#ifdef __UI_WINDOW_H__

/* for MLView */

void view_alloc(ui_window_t *uiwindow, int x, int y, u_int width, u_int height);

void view_dealloc(void *view);

void view_update(void *view);

void view_set_clip(void *view, int x, int y, u_int width, u_int height);

void view_unset_clip(void *view);

void view_draw_string(void *view, ui_font_t *font, ui_color_t *fg_color, int x, int y, char *str,
                      size_t len);

void view_draw_string16(void *view, ui_font_t *font, ui_color_t *fg_color, int x, int y,
                        XChar2b *str, size_t len);

void view_fill_with(void *view, ui_color_t *color, int x, int y, u_int width, u_int height);

void view_draw_rect_frame(void *view, ui_color_t *color, int x1, int y1, int x2, int y2);

void view_copy_area(void *view, Pixmap src, int src_x, int src_y, u_int width, u_int height,
                    int dst_x, int dst_y);

void view_scroll(void *view, int src_x, int src_y, u_int width, u_int height, int dst_x, int dst_y);

void view_bg_color_changed(void *view);

void view_visual_bell(void *view);

/* for BView */

void view_set_input_focus(void *view);

void view_resize(void *view, u_int width, u_int height);

void view_move(void *view, int x, int y);

void view_set_hidden(void *view, int flag);

void view_reset_uiwindow(ui_window_t *uiwindow);

/* for BWindow */
void window_alloc(ui_window_t *root, u_int width, u_int height);

void window_show(ui_window_t *root);

void window_dealloc(void *window);

void window_resize(void *window, int width, int height);

void window_move_resize(void *window, int x, int y, int width, int height);

void window_get_position(void *window, int *x, int *y);

void window_set_title(void *window, const char *title);

/* for BApplication */

void app_urgent_bell(int on);

/* for Clipboard */

void beos_clipboard_set(const u_char *utf8, size_t len);

int beos_clipboard_get(u_char **utf8, size_t *len);

void beos_beep(void);

#endif /* __UI_WINDOW_H__ */

void *beos_create_font(const char *font_family, float size, int is_italic, int is_bold);

char *beos_get_font_path(void *bfont);

void beos_release_font(void *bfont);

void beos_font_get_metrics(void *bfont, u_int *width, u_int *height, u_int *ascent);

u_int beos_font_get_advance(void *bfont, int size_attr, u_int16_t *utf16,
                            u_int len, u_int32_t glyph);

void *beos_create_image(const void *data, u_int len, u_int width, u_int height);

void beos_destroy_image(void *bitmap);

void *beos_load_image(const char *path, u_int *width, u_int *height);

void *beos_resize_image(void *bitmap, u_int width, u_int height);

/* Utility */

char *beos_dialog_password(const char *msg);

int beos_dialog_okcancel(const char *msg);

int beos_dialog_alert(const char *msg);

#endif
