/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_H__
#define ___UI_H__

#ifndef COMPAT_LIBVTE
#define GTK_PRIMARY
#define ZWP_PRIMARY
#define XDG_SHELL
#define ZXDG_SHELL_V6
#endif

#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>
#ifdef GTK_PRIMARY
#include "gtk-primary-selection.h"
#endif
#ifdef ZWP_PRIMARY
#include "primary-selection-unstable-v1-client-protocol.h"
#endif
#ifdef XDG_SHELL
#include "xdg-shell-client-protocol.h"
#endif
#ifdef ZXDG_SHELL_V6
#include "xdg-shell-unstable-v6-client-protocol.h"
#endif
#ifndef COMPAT_LIBVTE
#include "xdg-decoration-unstable-v1-client-protocol.h"
#endif
#include "text-input-unstable-v3-client-protocol.h"

typedef int KeyCode; /* Same as type of wparam */
typedef int KeySym;  /* Same as type of wparam */
typedef unsigned long Atom; /* Same as definition in X11/X.h */
typedef uint32_t Time;

typedef struct {
  int type;
  Time time;
  unsigned int state;
  KeySym ksym;
  unsigned int keycode;
  const char *utf8;

} XKeyEvent;

typedef struct {
  int type;
  Time time;
  int x;
  int y;
  unsigned int state;
  unsigned int button;

} XButtonEvent;

typedef struct {
  int type;
  Time time;
  int x;
  int y;
  unsigned int state;

} XMotionEvent;

typedef struct {
  int type;
  struct ui_window *target;

} XSelectionRequestEvent;

typedef union {
  int type;
  XKeyEvent xkey;
  XButtonEvent xbutton;
  XMotionEvent xmotion;
  XSelectionRequestEvent xselectionrequest;

} XEvent;

typedef struct {
  struct wl_display *display;
  struct wl_output *output;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct wl_cursor_theme *cursor_theme;
  struct wl_cursor *cursor[10];
  struct wl_surface *cursor_surface;
  struct wl_seat *seat;
  struct wl_keyboard *keyboard;
  struct wl_pointer *pointer;

  struct ui_xkb {
    struct xkb_context *ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
    xkb_mod_index_t ctrl;
    xkb_mod_index_t alt;
    xkb_mod_index_t shift;
    xkb_mod_index_t logo;
    unsigned int mods;
  } * xkb;

  struct wl_surface *current_kbd_surface;
  struct wl_surface *current_pointer_surface;

  int pointer_x;
  int pointer_y;
  int pointer_button;

  int current_cursor;

  int kbd_repeat_wait;
  XKeyEvent prev_kev;
  unsigned int kbd_repeat_count;

  struct wl_data_device_manager *data_device_manager;
  struct wl_data_device *data_device;
  struct wl_data_offer *dnd_offer;
  struct wl_surface *data_surface;
  uint32_t dnd_action;
  struct wl_data_offer *sel_offer;
  char *sel_offer_mime;
  struct wl_data_source *sel_source;

#ifdef GTK_PRIMARY
  struct gtk_primary_selection_device_manager *gxsel_device_manager;
  struct gtk_primary_selection_device *gxsel_device;
  struct gtk_primary_selection_source *gxsel_source;
  struct gtk_primary_selection_offer *gxsel_offer;
#endif
#ifdef ZWP_PRIMARY
  struct zwp_primary_selection_device_manager_v1 *zxsel_device_manager;
  struct zwp_primary_selection_device_v1 *zxsel_device;
  struct zwp_primary_selection_source_v1 *zxsel_source;
  struct zwp_primary_selection_offer_v1 *zxsel_offer;
#endif
  char *xsel_offer_mime;

  int32_t sel_fd;
  uint32_t serial;

  int ref_count;

  /*
   * These members should be placed at the end of this structure because
   * ui_*.c except ui_display.c refers this structure correctly
   * if COMPAT_LIBVTE is defined or not.
   */
#ifdef COMPAT_LIBVTE
  struct wl_subcompositor *subcompositor;
  /* for sway */
  int shell_type; /* 0: Not determined, 1: wl_shell, 2: zxdg_shell, 3: xdg_shell */
#else
  struct wl_shell *shell;
  struct zxdg_decoration_manager_v1 *decoration_manager;
#ifdef XDG_SHELL
  struct xdg_wm_base *xdg_shell;
#endif
#ifdef ZXDG_SHELL_V6
  struct zxdg_shell_v6 *zxdg_shell;
#endif
#endif

  struct zwp_text_input_manager_v3 *text_input_manager;

} ui_wlserv_t;

typedef struct {
  ui_wlserv_t *wlserv;
  unsigned char *fb;
  struct wl_buffer *buffer;
  struct wl_surface *surface;

  unsigned int bytes_per_pixel;
  unsigned int line_length;

  int lock_state;

  struct rgb_info {
    unsigned int r_limit;
    unsigned int g_limit;
    unsigned int b_limit;
    unsigned int r_offset;
    unsigned int g_offset;
    unsigned int b_offset;

  } rgbinfo;

  unsigned int width;
  unsigned int height;

  int damage_x;
  int damage_y;
  unsigned int damage_width;
  unsigned int damage_height;

  int is_resizing;

  struct ui_display *parent;

  /*
   * These members should be placed at the end of this structure because
   * ui_*.c except ui_display.c refers this structure correctly
   * if COMPAT_LIBVTE is defined or not.
   */
#ifdef COMPAT_LIBVTE
  struct wl_surface *parent_surface;
  struct wl_subsurface *subsurface;
  int x;
  int y;
#else /* COMPAT_LIBVTE */
  struct wl_shell_surface *shell_surface;
#ifdef ZXDG_SHELL_V6
  struct zxdg_surface_v6 *zxdg_surface;
  struct zxdg_toplevel_v6 *zxdg_toplevel;
  struct zxdg_popup_v6 *zxdg_popup;
  int zxdg_surface_configured;
#endif
#ifdef XDG_SHELL
  struct xdg_surface *xdg_surface;
  struct xdg_toplevel *xdg_toplevel;
  struct xdg_popup *xdg_popup;
  int xdg_surface_configured;
  struct zxdg_toplevel_decoration_v1 *toplevel_decoration;
#endif
#endif /* COMPAT_LIBVTE */

  struct zwp_text_input_v3 *text_input;
  char *preedit_text;
  int text_input_x;
  int text_input_y;
  unsigned int text_input_line_height;

} Display;

#define PIXEL_RED(pixel, rgbinfo) (((pixel) >> (rgbinfo).r_offset) << (rgbinfo).r_limit)
#define PIXEL_BLUE(pixel, rgbinfo) (((pixel) >> (rgbinfo).b_offset) << (rgbinfo).b_limit)
#define PIXEL_GREEN(pixel, rgbinfo) (((pixel) >> (rgbinfo).g_offset) << (rgbinfo).g_limit)
#define RGB_TO_PIXEL(r, g, b, rgbinfo)                  \
  ((((r) >> (rgbinfo).r_limit) << (rgbinfo).r_offset) | \
   (((g) >> (rgbinfo).g_limit) << (rgbinfo).g_offset) | \
   (((b) >> (rgbinfo).b_limit) << (rgbinfo).b_offset))
#define ALPHA_TO_PIXEL(a, rgbinfo, depth) ((depth) == 32 ? ((a) << 24) : 0)

typedef int XIC; /* dummy */
typedef void *XID; /* dummy */
typedef void *Window; /* dummy */
typedef void *Drawable; /* dummy */
typedef struct {
  unsigned char *image;
  unsigned int width;
  unsigned int height;
} * Pixmap;
typedef unsigned char *PixmapMask;
typedef int GC;
typedef int Font;
typedef int Cursor;

typedef struct /* Same as definition in X11/X.h */
    {
  int max_keypermod;
  KeyCode *modifiermap;

} XModifierKeymap;

typedef struct /* Same as definition in X11/X.h */
    {
  unsigned char byte1;
  unsigned char byte2;

} XChar2b;

/*
 * XFontStruct must be the same in fb, wayland, sdl2 and Android.
 */
typedef struct _XFontStruct {
  char *file;

  int32_t num_glyphs; /* Not used on freetype */
  unsigned char *glyphs; /* Cast to unsigned char** on FreeType */
  void *glyph_indeces; /* PCF: int16_t*, decsp: int16_t*, FreeType: u_int32_t* */

  /* If is_aa is true, don't use glyph_width_bytes except in next_glyph_buf(). */
  int32_t glyph_width_bytes;

  uint16_t width;
  /* Width of full width characters or max width of half width characters. */
  uint16_t width_full;
  uint16_t height;
  uint16_t ascent;
  uint8_t has_each_glyph_width_info;

  /* for pcf */
  int16_t min_char_or_byte2;
  int16_t max_char_or_byte2;
  int16_t min_byte1;
  int16_t max_byte1;
  int32_t *glyph_offsets;

#ifdef USE_FREETYPE
  /*
   * XXX
   *
   * (fontsize|FONT_BOLD|FONT_ITALIC|FONT_ROTATED) on freetype.
   * fontsize is 0-0x1ff.
   */
  int32_t format;
  void *face;
  uint32_t num_indeces;
  uint32_t num_glyph_bufs;
  uint32_t glyph_buf_left;
  int is_aa;

#ifdef USE_FONTCONFIG
  struct _XFontStruct **compl_xfonts;
#endif
#endif

  unsigned int ref_count;

} XFontStruct;

typedef int XFontSet; /* dummy */

#define None 0L     /* Same as definition in X11/X.h */
#define NoSymbol 0L /* Same as definition in X11/X.h */

#define CurrentTime 0L /* Same as definition in X11/X.h */

/* Same as definition in X11/X.h */
#define NoEventMask 0L
#define KeyPressMask (1L << 0)
#define KeyReleaseMask (1L << 1)
#define ButtonPressMask (1L << 2)
#define ButtonReleaseMask (1L << 3)
#define EnterWindowMask (1L << 4)
#define LeaveWindowMask (1L << 5)
#define PointerMotionMask (1L << 6)
#define PointerMotionHintMask (1L << 7)
#define Button1MotionMask (1L << 8)
#define Button2MotionMask (1L << 9)
#define Button3MotionMask (1L << 10)
#define Button4MotionMask (1L << 11)
#define Button5MotionMask (1L << 12)
#define ButtonMotionMask (1L << 13)
#define KeymapStateMask (1L << 14)
#define ExposureMask (1L << 15)
#define VisibilityChangeMask (1L << 16)
#define StructureNotifyMask (1L << 17)
#define ResizeRedirectMask (1L << 18)
#define SubstructureNotifyMask (1L << 19)
#define SubstructureRedirectMask (1L << 20)
#define FocusChangeMask (1L << 21)
#define PropertyChangeMask (1L << 22)
#define ColormapChangeMask (1L << 23)
#define OwnerGrabButtonMask (1L << 24)
#define ShiftMask (1 << 0)
#define LockMask (1 << 1)
#define ControlMask (1 << 2)
#define Mod1Mask (1 << 3)
#define Mod2Mask (1 << 4)
#define Mod3Mask (1 << 5)
#define Mod4Mask (1 << 6)
#define Mod5Mask (1 << 7)
#define Button1Mask (1 << 8)
#define Button2Mask (1 << 9)
#define Button3Mask (1 << 10)
#define Button4Mask (1 << 11)
#define Button5Mask (1 << 12)
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5

#define XK_Super_L XKB_KEY_Super_L
#define XK_Super_R XKB_KEY_Super_R
#define XK_Hyper_L XKB_KEY_Hyper_L
#define XK_Hyper_R XKB_KEY_Hyper_R
#define XK_BackSpace XKB_KEY_BackSpace
#define XK_Tab XKB_KEY_Tab
#define XK_Clear XKB_KEY_Clear
#define XK_Linefeed XKB_KEY_Linefeed
#define XK_Return XKB_KEY_Return

#define XK_Shift_L XKB_KEY_Shift_L
#define XK_Control_L XKB_KEY_Control_L
#define XK_Alt_L XKB_KEY_Alt_L
#define XK_Shift_R XKB_KEY_Shift_R
#define XK_Control_R XKB_KEY_Control_R
#define XK_Alt_R XKB_KEY_Alt_R

#define XK_Meta_L XKB_KEY_Meta_L
#define XK_Meta_R XKB_KEY_Meta_R

#define XK_Pause XKB_KEY_Pause
#define XK_Shift_Lock XKB_KEY_Shift_Lock
#define XK_Caps_Lock XKB_KEY_Caps_Lock
#define XK_Escape XKB_KEY_Escape
#define XK_Prior XKB_KEY_Prior
#define XK_Next XKB_KEY_Next
#define XK_End XKB_KEY_End
#define XK_Home XKB_KEY_Home
#define XK_Left XKB_KEY_Left
#define XK_Up XKB_KEY_Up
#define XK_Right XKB_KEY_Right
#define XK_Down XKB_KEY_Down
#define XK_Select XKB_KEY_Select
#define XK_Print XKB_KEY_Print
#define XK_Execute XKB_KEY_Execute
#define XK_Insert XKB_KEY_Insert
#define XK_Delete XKB_KEY_Delete
#define XK_Help XKB_KEY_Help
#define XK_F1 XKB_KEY_F1
#define XK_F2 XKB_KEY_F2
#define XK_F3 XKB_KEY_F3
#define XK_F4 XKB_KEY_F4
#define XK_F5 XKB_KEY_F5
#define XK_F6 XKB_KEY_F6
#define XK_F7 XKB_KEY_F7
#define XK_F8 XKB_KEY_F8
#define XK_F9 XKB_KEY_F9
#define XK_F10 XKB_KEY_F10
#define XK_F11 XKB_KEY_F11
#define XK_F12 XKB_KEY_F12
#define XK_F13 XKB_KEY_F13
#define XK_F14 XKB_KEY_F14
#define XK_F15 XKB_KEY_F15
#define XK_F16 XKB_KEY_F16
#define XK_F17 XKB_KEY_F17
#define XK_F18 XKB_KEY_F18
#define XK_F19 XKB_KEY_F19
#define XK_F20 XKB_KEY_F20
#define XK_F21 XKB_KEY_F21
#define XK_F22 XKB_KEY_F22
#define XK_F23 XKB_KEY_F23
#define XK_F24 XKB_KEY_F24
#define XK_FMAX XKB_KEY_F24
#define XK_Num_Lock XKB_KEY_Num_Lock
#define XK_Scroll_Lock XKB_KEY_Scroll_Lock
#define XK_Find XKB_KEY_Find
#define XK_Menu XKB_KEY_Menu
#define XK_Begin XKB_KEY_Begin
#define XK_Muhenkan XKB_KEY_Muhenkan
#define XK_Henkan_Mode XKB_KEY_Henkan_Mode
#define XK_Zenkaku_Hankaku XKB_KEY_Zenkaku_Hankaku
#define XK_Hiragana_Katakana 0xfffe /* dummy */

#define XK_KP_Prior XKB_KEY_KP_Prior
#define XK_KP_Next XKB_KEY_KP_Next
#define XK_KP_End XKB_KEY_KP_End
#define XK_KP_Home XKB_KEY_KP_Home
#define XK_KP_Left XKB_KEY_KP_Left
#define XK_KP_Up XKB_KEY_KP_Up
#define XK_KP_Right XKB_KEY_KP_Right
#define XK_KP_Down XKB_KEY_KP_Down
#define XK_KP_Insert XKB_KEY_KP_Insert
#define XK_KP_Delete XKB_KEY_KP_Delete
#define XK_KP_F1 XKB_KEY_KP_F1
#define XK_KP_F2 XKB_KEY_KP_F2
#define XK_KP_F3 XKB_KEY_KP_F3
#define XK_KP_F4 XKB_KEY_KP_F4
#define XK_KP_Begin XKB_KEY_KP_Begin
#define XK_KP_Multiply XKB_KEY_KP_Multiply
#define XK_KP_Add XKB_KEY_KP_Add
#define XK_KP_Separator XKB_KEY_KP_Separator
#define XK_KP_Subtract XKB_KEY_KP_Subtract
#define XK_KP_Decimal XKB_KEY_KP_Decimal
#define XK_KP_Divide XKB_KEY_KP_Divide
#define XK_KP_0 XKB_KEY_KP_0
#define XK_KP_1 XKB_KEY_KP_1
#define XK_KP_2 XKB_KEY_KP_2
#define XK_KP_3 XKB_KEY_KP_3
#define XK_KP_4 XKB_KEY_KP_4
#define XK_KP_5 XKB_KEY_KP_5
#define XK_KP_6 XKB_KEY_KP_6
#define XK_KP_7 XKB_KEY_KP_7
#define XK_KP_8 XKB_KEY_KP_8
#define XK_KP_9 XKB_KEY_KP_9

#define IsKeypadKey(ksym) (XKB_KEY_KP_Space <= (ksym) && (ksym) < XKB_KEY_F1)
#define IsModifierKey(ksym) (0)

#define XK_ISO_Left_Tab XKB_KEY_ISO_Left_Tab

typedef struct {
  short x;
  short y;
} XPoint;

/* XXX dummy */
#define XKeysymToKeycode(disp, ks) (ks)
#define XKeycodeToKeysym(disp, kc, i) (kc)
#define XKeysymToString(ks) ""
#define DefaultScreen(disp) (0)

#define BlackPixel(disp, screen) (0xff000000 | RGB(0, 0, 0))
#define WhitePixel(disp, screen) (0xff000000 | RGB(0xff, 0xff, 0xff))

/* Same as definition in X11/cursorfont.h */
#define XC_xterm 152
#define XC_left_ptr 68

/* Same as definition in X11/Xutil.h */
#define NoValue 0x0000
#define XValue 0x0001
#define YValue 0x0002
#define WidthValue 0x0004
#define HeightValue 0x0008
#define AllValues 0x000F
#define XNegative 0x0010
#define YNegative 0x0020

int XParseGeometry(char *str, int *x, int *y, unsigned int *width, unsigned int *height);

KeySym XStringToKeysym(const char *str);

/* === Platform dependent options === */

#define UI_COLOR_HAS_RGB
#define SUPPORT_TRUE_TRANSPARENT_BG
#ifdef USE_FREETYPE
#define TYPE_XCORE_SCALABLE
#else
#undef TYPE_XCORE_SCALABLE
#endif
#define MANAGE_SUB_WINDOWS_BY_MYSELF
/* ui_im_{candidate|status}_screen.c, ui_window.c */
#undef MANAGE_ROOT_WINDOWS_BY_MYSELF
#define INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#undef SUPPORT_POINT_SIZE_FONT
#define XIM_SPOT_IS_LINE_TOP
#undef USE_GC
#undef CHANGEABLE_CURSOR
#define PLUGIN_MODULE_SUFFIX "wl"
#define KEY_REPEAT_BY_MYSELF
#define ROTATABLE_DISPLAY
#undef PSEUDO_COLOR_DISPLAY
#undef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
#undef SUPPORT_URGENT_BELL
#undef FORCE_UNICODE
#define NEED_DISPLAY_SYNC_EVERY_TIME
#define DRAW_SCREEN_IN_PIXELS
#undef NO_DRAW_IMAGE_STRING
/* libpthread is not linked to mlterm explicitly for now. */
#undef HAVE_PTHREAD
#define COMPOSE_DECSP_FONT
#ifdef USE_FREETYPE
#define USE_REAL_VERTICAL_FONT
#else
#undef USE_REAL_VERTICAL_FONT
#endif
#undef NO_DISPLAY_FD
#undef FLICK_SCROLL
#define UIWINDOW_SUPPORTS_PREEDITING
#undef SELECTION_STYLE_CHANGEABLE

#endif
