/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __COCOA_H__
#define __COCOA_H__

#ifdef __UI_WINDOW_H__

/* for MLTermView */

void view_alloc(ui_window_t *uiwindow);

void view_dealloc(void *view);

void view_update(void *view, int flag);

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

/* for NSView */

void view_set_input_focus(void *view);

void view_set_rect(void *view, int x, int y, u_int width, u_int height);

void view_set_hidden(void *view, int flag);

/* for NSWindow */

void window_alloc(ui_window_t *root);

void window_dealloc(void *window);

void window_resize(void *window, int width, int height);

void window_move_resize(void *window, int x, int y, int width, int height);

void window_accepts_mouse_moved_events(void *window, int accept);

void window_set_normal_hints(void *window, u_int width_inc, u_int height_inc);

void window_get_position(void *window, int *x, int *y);

void window_set_title(void *window, const char *title);

/* for NSApp */

void app_urgent_bell(int on);

/* for NSScroller */

void scroller_update(void *scroller, float pos, float knob);

/* for Clipboard */

void cocoa_clipboard_own(void *view);

void cocoa_clipboard_set(const u_char *utf8, size_t len);

const char *cocoa_clipboard_get(void);

void cocoa_beep(void);

#endif /* __UI_WINDOW_H__ */

#ifdef __UI_FONT_H__

/* for CGFont */

void *cocoa_create_font(const char *font_family);

char *cocoa_get_font_path(void *cg_font);

void cocoa_release_font(void *cg_font);

u_int cocoa_font_get_advance(void *cg_font, u_int fontsize, int size_attr, u_int16_t *utf16,
                             u_int len, u_int32_t glyph);

#endif

#ifdef __UI_IMAGELIB_H__

/* for CGImage */

Pixmap cocoa_load_image(const char *path, u_int *width, u_int *height);

#endif

/* Utility */

int cocoa_add_fd(int fd, void (*handler)(void));

int cocoa_remove_fd(int fd);

char *cocoa_dialog_password(const char *msg);

int cocoa_dialog_okcancel(const char *msg);

int cocoa_dialog_alert(const char *msg);

#endif
