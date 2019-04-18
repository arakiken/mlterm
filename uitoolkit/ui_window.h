/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_WINDOW_H__
#define __UI_WINDOW_H__

#include <pobl/bl_types.h>
#include <mef/ef_parser.h>

#include "ui_display.h"
#include "ui_font.h"
#include "ui_color.h"
#include "ui_gc.h"
#include "ui_bel_mode.h"

#define ACTUAL_WIDTH(win) ((win)->width + (win)->hmargin * 2)
#define ACTUAL_HEIGHT(win) ((win)->height + (win)->vmargin * 2)

/*
 * Don't use win->parent in xlib to check if win is root window or not
 * because mlterm can work as libvte.
 *   vte window
 *      |
 *   mlterm window ... ui_window_t::parent == NULL
 *                     ui_window_t::parent_window == vte window
 */
#define PARENT_WINDOWID_IS_TOP(win) ((win)->parent_window == (win)->disp->my_window)

typedef enum ui_resize_flag {
  NOTIFY_TO_NONE = 0x0,
  NOTIFY_TO_CHILDREN = 0x01,
  NOTIFY_TO_PARENT = 0x02,
  NOTIFY_TO_MYSELF = 0x04,

  LIMIT_RESIZE = 0x08,

} ui_resize_flag_t;

typedef enum ui_maximize_flag {
  MAXIMIZE_RESTORE = 0x0,
  MAXIMIZE_VERTICAL = 0x1,
  MAXIMIZE_HORIZONTAL = 0x2,
  MAXIMIZE_FULL = 0x3,

} ui_maximize_flag_t;

typedef enum ui_sizehint_flag {
  SIZEHINT_NONE = 0x0,
  SIZEHINT_WIDTH = 0x1,
  SIZEHINT_HEIGHT = 0x2,
} ui_sizehint_flag_t;

typedef struct ui_xim_event_listener {
  void *self;

  int (*get_spot)(void *, int *, int *);
  XFontSet (*get_fontset)(void *);
  ui_color_t *(*get_fg_color)(void *);
  ui_color_t *(*get_bg_color)(void *);

} ui_xim_event_listener_t;

/* Defined in ui_xic.h */
typedef struct ui_xic *ui_xic_ptr_t;

/* Defined in ui_xim.h */
typedef struct ui_xim *ui_xim_ptr_t;

/* Defined in ui_dnd.h */
typedef struct ui_dnd_context *ui_dnd_context_ptr_t;

/* Defined in ui_picture.h */
typedef struct ui_picture_modifier *ui_picture_modifier_ptr_t;
typedef struct ui_icon_picture *ui_icon_picture_ptr_t;
typedef struct _XftDraw *xft_draw_ptr_t;
typedef struct _cairo *cairo_ptr_t;

typedef struct ui_window {
  ui_display_t *disp;

  Window my_window;

#ifdef  USE_XLIB
  /*
   * Don't remove if USE_XFT and USE_CAIRO are not defined to keep the size of
   * ui_window_t
   * for ui_im_xxx_screen_t.
   */
  xft_draw_ptr_t xft_draw;
  cairo_ptr_t cairo_draw;
#endif

  ui_color_t fg_color;
  ui_color_t bg_color;

  ui_gc_t *gc;

  Window parent_window;     /* This member of root window is DefaultRootWindow */
  struct ui_window *parent; /* This member of root window is NULL */
  struct ui_window **children;
  u_int num_children;

  u_int cursor_shape;

  long event_mask;

  int x; /* relative to a root window. */
  int y; /* relative to a root window. */
  u_int width;
  u_int height;
  u_int min_width;
  u_int min_height;
  u_int width_inc;
  u_int height_inc;

  /* actual window size is +margin on north/south/east/west */
  u_int16_t hmargin;
  u_int16_t vmargin;

/*
 * mlterm-con doesn't use these members, but they are necessary to keep
 * the size of ui_window_t as the same as mlterm-fb for input method plugins.
 */
#ifdef MANAGE_SUB_WINDOWS_BY_MYSELF
  u_int clip_y;
  u_int clip_height;
#endif

  /* used by ui_xim */
  ui_xim_ptr_t xim;
  ui_xim_event_listener_t *xim_listener;
  ui_xic_ptr_t xic; /* Only root window manages xic in win32 */

#if defined(USE_WIN32GUI)
  WORD update_window_flag;
  int cmd_show;
#elif defined(USE_QUARTZ) || defined(USE_BEOS)
  int update_window_flag;
#endif

  /* button */
  Time prev_clicked_time;
  int prev_clicked_button;
  XButtonEvent prev_button_press_event;

  ui_picture_modifier_ptr_t pic_mod;

  /*
   * XDND
   */
  /*
   * Don't remove if DISABLE_XDND is defined to keep the size of ui_window_t for
   * ui_im_xxx_screen_t.
   */
  ui_dnd_context_ptr_t dnd;

  /*
   * XClassHint
   */
  char *app_name;

/*
 * flags etc.
 */

#ifdef USE_XLIB
  int8_t wall_picture_is_set;     /* Actually set picture (including transparency)
                                     or not. */
  int8_t wait_copy_area_response; /* Used for XCopyArea() */
  int8_t configure_root;
#else
  Pixmap wall_picture;
#endif
#ifdef USE_WIN32GUI
  int8_t is_sel_owner;
#endif
  int8_t is_transparent;
  int8_t is_scrollable;
  int8_t is_focused;
  int8_t inputtable; /* 1: focused, -1: unfocused */
  int8_t is_mapped;
#ifdef __ANDROID__
  int8_t saved_mapped;
#endif
  int8_t create_gc;

  /* button */
  int8_t button_is_pressing;
  int8_t click_num;

  u_int8_t sizehint_flag;

  void (*window_realized)(struct ui_window *);
  void (*window_finalized)(struct ui_window *);
  void (*window_destroyed)(struct ui_window *);
  void (*mapping_notify)(struct ui_window *);
  /* Win32: gc->gc is not None. */
  void (*window_exposed)(struct ui_window *, int, int, u_int, u_int);
  /* Win32: gc->gc is not None. */
  void (*update_window)(struct ui_window *, int);
  void (*window_focused)(struct ui_window *);
  void (*window_unfocused)(struct ui_window *);
  void (*key_pressed)(struct ui_window *, XKeyEvent *);
  void (*pointer_motion)(struct ui_window *, XMotionEvent *);
  void (*button_motion)(struct ui_window *, XMotionEvent *);
  void (*button_released)(struct ui_window *, XButtonEvent *);
  void (*button_pressed)(struct ui_window *, XButtonEvent *, int);
  void (*button_press_continued)(struct ui_window *, XButtonEvent *);
  void (*window_resized)(struct ui_window *);
  void (*child_window_resized)(struct ui_window *, struct ui_window *);
  void (*selection_cleared)(struct ui_window *);
  void (*xct_selection_requested)(struct ui_window *, XSelectionRequestEvent *, Atom);
  void (*utf_selection_requested)(struct ui_window *, XSelectionRequestEvent *, Atom);
  void (*xct_selection_notified)(struct ui_window *, u_char *, size_t);
  void (*utf_selection_notified)(struct ui_window *, u_char *, size_t);
  /*
   * Don't remove if DISABLE_XDND is defined to keep the size of ui_window_t
   * for ui_im_xxx_screen_t.
   */
  void (*set_xdnd_config)(struct ui_window *, char *, char *, char *);
  void (*idling)(struct ui_window *);
#ifdef UIWINDOW_SUPPORTS_PREEDITING
  void (*preedit)(struct ui_window *, const char *, const char *);
#endif

} ui_window_t;

int ui_window_init(ui_window_t *win, u_int width, u_int height, u_int min_width, u_int min_height,
                   u_int width_inc, u_int height_inc, u_int hmargin, u_int vmargin, int create_gc,
                   int inputtable);

void ui_window_final(ui_window_t *win);

void ui_window_set_type_engine(ui_window_t *win, ui_type_engine_t type_engine);

void ui_window_add_event_mask(ui_window_t *win, long event_mask);

void ui_window_remove_event_mask(ui_window_t *win, long event_mask);

#define ui_window_check_event_mask(win, mask) ((win)->event_mask & (mask))

/* int  ui_window_grab_pointer( ui_window_t *  win) ; */

void ui_window_ungrab_pointer(ui_window_t *win);

int ui_window_set_wall_picture(ui_window_t *win, Pixmap pic, int do_expose);

int ui_window_unset_wall_picture(ui_window_t *win, int do_expose);

#ifdef USE_XLIB
#define ui_window_has_wall_picture(win) ((win)->wall_picture_is_set)
#else
#define ui_window_has_wall_picture(win) ((win)->wall_picture != None)
#endif

int ui_window_set_transparent(ui_window_t *win, ui_picture_modifier_ptr_t pic_mod);

int ui_window_unset_transparent(ui_window_t *win);

void ui_window_set_cursor(ui_window_t *win, u_int cursor_shape);

int ui_window_set_fg_color(ui_window_t *win, ui_color_t *fg_color);

int ui_window_set_bg_color(ui_window_t *win, ui_color_t *bg_color);

int ui_window_add_child(ui_window_t *win, ui_window_t *child, int x, int y, int map);

int ui_window_remove_child(ui_window_t *win, ui_window_t *child);

ui_window_t *ui_get_root_window(ui_window_t *win);

GC ui_window_get_fg_gc(ui_window_t *win);

GC ui_window_get_bg_gc(ui_window_t *win);

int ui_window_show(ui_window_t *win, int hint);

void ui_window_map(ui_window_t *win);

void ui_window_unmap(ui_window_t *win);

int ui_window_resize(ui_window_t *win, u_int width, u_int height, ui_resize_flag_t flag);

int ui_window_resize_with_margin(ui_window_t *win, u_int width, u_int height,
                                 ui_resize_flag_t flag);

void ui_window_set_maximize_flag(ui_window_t *win, ui_maximize_flag_t flag);

void ui_window_set_normal_hints(ui_window_t *win, u_int min_width, u_int min_height,
                                u_int width_inc, u_int height_inc);

void ui_window_set_override_redirect(ui_window_t *win, int flag);

int ui_window_set_borderless_flag(ui_window_t *win, int flag);

int ui_window_move(ui_window_t *win, int x, int y);

void ui_window_clear(ui_window_t *win, int x, int y, u_int width, u_int height);

void ui_window_clear_all(ui_window_t *win);

void ui_window_fill(ui_window_t *win, int x, int y, u_int width, u_int height);

void ui_window_fill_with(ui_window_t *win, ui_color_t *color, int x, int y, u_int width,
                        u_int height);

void ui_window_blank(ui_window_t *win);

#if 0
/* Not used */
void ui_window_blank_with(ui_window_t *win, ui_color_t *color);
#endif

/* if flag is 0, no update. */
void ui_window_update(ui_window_t *win, int flag);

void ui_window_update_all(ui_window_t *win);

void ui_window_idling(ui_window_t *win);

int ui_window_receive_event(ui_window_t *win, XEvent *event);

size_t ui_window_get_str(ui_window_t *win, u_char *seq, size_t seq_len, ef_parser_t **parser,
                         KeySym *keysym, XKeyEvent *event);

#ifdef MANAGE_ROOT_WINDOWS_BY_MYSELF
int ui_window_is_scrollable(ui_window_t *win);
#else
#define ui_window_is_scrollable(win) ((win)->is_scrollable)
#endif

int ui_window_scroll_upward(ui_window_t *win, u_int height);

int ui_window_scroll_upward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                   u_int height);

int ui_window_scroll_downward(ui_window_t *win, u_int height);

int ui_window_scroll_downward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                     u_int height);

int ui_window_scroll_leftward(ui_window_t *win, u_int width);

int ui_window_scroll_leftward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                     u_int width);

int ui_window_scroll_rightward_region(ui_window_t *win, int boundary_start, int boundary_end,
                                      u_int width);

int ui_window_scroll_rightward(ui_window_t *win, u_int width);

int ui_window_copy_area(ui_window_t *win, Pixmap src, PixmapMask mask, int src_x, int src_y,
                        u_int width, u_int height, int dst_x, int dst_y);

void ui_window_set_clip(ui_window_t *win, int x, int y, u_int width, u_int height);

void ui_window_unset_clip(ui_window_t *win);

void ui_window_draw_decsp_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                 int y, u_char *str, u_int len);

void ui_window_draw_decsp_image_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                       ui_color_t *bg_color, int x, int y, u_char *str, u_int len);

/*
 * ui_window_draw_*_string functions are used by ui_draw_str.[ch].
 * Use ui_draw_str* functions usually.
 */

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
void ui_window_draw_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x, int y,
                           u_char *str, u_int len);

void ui_window_draw_string16(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x, int y,
                             XChar2b *str, u_int len);

void ui_window_draw_image_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                 ui_color_t *bg_color, int x, int y, u_char *str, u_int len);

void ui_window_draw_image_string16(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                   ui_color_t *bg_color, int x, int y, XChar2b *str, u_int len);
#endif

#ifdef USE_CONSOLE
void ui_window_console_draw_decsp_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                         ui_color_t *bg_color, int x, int y, u_char *str, u_int len,
                                         int line_style);

void ui_window_console_draw_string(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                   ui_color_t *bg_color, int x, int y, u_char *str, u_int len,
                                   int line_style);

void ui_window_console_draw_string16(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color,
                                     ui_color_t *bg_color, int x, int y, XChar2b *str, u_int len,
                                     int line_style);
#endif

#if !defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
void ui_window_ft_draw_string8(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                               int y, u_char *str, size_t len);

void ui_window_ft_draw_string32(ui_window_t *win, ui_font_t *font, ui_color_t *fg_color, int x,
                                int y, /* FcChar32 */ u_int32_t *str, u_int len);
#endif

void ui_window_draw_rect_frame(ui_window_t *win, int x1, int y1, int x2, int y2);

void ui_set_use_clipboard_selection(int use_it);

int ui_is_using_clipboard_selection(void);

int ui_window_set_selection_owner(ui_window_t *win, Time time);

#define ui_window_is_selection_owner(win) ((win) == (win)->disp->selection_owner)

int ui_window_string_selection_request(ui_window_t *win, Time time);

int ui_window_xct_selection_request(ui_window_t *win, Time time);

int ui_window_utf_selection_request(ui_window_t *win, Time time);

void ui_window_send_picture_selection(ui_window_t *win, Pixmap pixmap, u_int width, u_int height);

void ui_window_send_text_selection(ui_window_t *win, XSelectionRequestEvent *event,
                                   u_char *sel_data, size_t sel_len, Atom sel_type);

void ui_set_window_name(ui_window_t *win, u_char *name);

void ui_set_icon_name(ui_window_t *win, u_char *name);

void ui_window_set_icon(ui_window_t *win, ui_icon_picture_ptr_t icon);

void ui_window_remove_icon(ui_window_t *win);

void ui_set_click_interval(int interval);

int ui_get_click_interval(void);

#define ui_window_get_modifier_mapping(win) ui_display_get_modifier_mapping((win)->disp)

u_int ui_window_get_mod_ignore_mask(ui_window_t *win, KeySym *keysyms);

u_int ui_window_get_mod_meta_mask(ui_window_t *win, char *mod_key);

#ifdef SUPPORT_URGENT_BELL
void ui_set_use_urgent_bell(int use);
#else
#define ui_set_use_urgent_bell(use) (0)
#endif

void ui_window_bell(ui_window_t *win, ui_bel_mode_t mode);

void ui_window_translate_coordinates(ui_window_t *win, int x, int y, int *global_x, int *global_y);

void ui_window_set_input_focus(ui_window_t *win);

#ifdef USE_XLIB
void ui_window_flush(ui_window_t *win);
#else
#define ui_window_flush(win) (0)
#endif

#ifdef DEBUG
void ui_window_dump_children(ui_window_t *win);
#endif

#endif
