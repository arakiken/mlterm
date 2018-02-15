/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_H__
#define ___UI_H__

#include <SDL.h>

#ifdef USE_FREETYPE
#include <pobl/bl_types.h> /* u_int32_t etc */
#endif

#if 0
#define USE_BG_TEXTURE
#endif

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
	SDL_Window *window;
	SDL_Renderer *renderer;
  SDL_Texture *texture;
  unsigned char *fb;

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

  int damaged;

  struct ui_display *parent;

#ifdef USE_BG_TEXTURE
  SDL_Texture *bg_texture;
#endif

} Display;

#define PIXEL_RED(pixel, rgbinfo) (((pixel) >> (rgbinfo).r_offset) << (rgbinfo).r_limit)
#define PIXEL_BLUE(pixel, rgbinfo) (((pixel) >> (rgbinfo).b_offset) << (rgbinfo).b_limit)
#define PIXEL_GREEN(pixel, rgbinfo) (((pixel) >> (rgbinfo).g_offset) << (rgbinfo).g_limit)
#define RGB_TO_PIXEL(r, g, b, rgbinfo)                  \
  ((((r) >> (rgbinfo).r_limit) << (rgbinfo).r_offset) | \
   (((g) >> (rgbinfo).g_limit) << (rgbinfo).g_offset) | \
   (((b) >> (rgbinfo).b_limit) << (rgbinfo).b_offset))

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

typedef struct /* Same as definition in X11/X.h */ {
  int max_keypermod;
  KeyCode *modifiermap;

} XModifierKeymap;

typedef struct /* Same as definition in X11/X.h */ {
  unsigned char byte1;
  unsigned char byte2;

} XChar2b;

typedef struct _XFontStruct {
  char *file;

#ifdef USE_FREETYPE
  int32_t format; /* XXX (fontsize|FONT_BOLD|FONT_ITALIC) on freetype. */
#endif

  int32_t num_glyphs;
  unsigned char *glyphs;

  int32_t glyph_width_bytes;

  unsigned char width;
  unsigned char width_full;
  unsigned char height;
  unsigned char ascent;

#if 0
  u_int16_t *glyph_indeces;
#else
  unsigned short *glyph_indeces;
#endif

  /* for pcf */
  int16_t min_char_or_byte2;
  int16_t max_char_or_byte2;
  int16_t min_byte1;
  int16_t max_byte1;
  int32_t *glyph_offsets;

#ifdef USE_FREETYPE
  /* for freetype */
  void *face;
  u_int32_t num_indeces;
  u_int32_t glyph_size;
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

#define XK_Super_L 0xfffe
#define XK_Super_R 0xfffd
#define XK_Hyper_L 0xfffc
#define XK_Hyper_R 0xfffb
#define XK_BackSpace SDLK_BACKSPACE
#define XK_Tab SDLK_TAB
#define XK_Clear 0xfffa
#define XK_Linefeed 0xfff9
#define XK_Return SDLK_RETURN

#define XK_Shift_L SDLK_LSHIFT
#define XK_Control_L SDLK_LCTRL
#define XK_Alt_L SDLK_LALT
#define XK_Shift_R SDLK_RSHIFT
#define XK_Control_R SDLK_RCTRL
#define XK_Alt_R SDLK_RALT

#define XK_Meta_L 0xfff8
#define XK_Meta_R 0xfff7

#define XK_Pause SDLK_PAUSE
#define XK_Shift_Lock 0xfff6
#define XK_Caps_Lock SDLK_CAPSLOCK
#define XK_Escape SDLK_ESCAPE
#define XK_Prior SDLK_PAGEUP
#define XK_Next SDLK_PAGEDOWN
#define XK_End SDLK_END
#define XK_Home SDLK_HOME
#define XK_Left SDLK_LEFT
#define XK_Up SDLK_UP
#define XK_Right SDLK_RIGHT
#define XK_Down SDLK_DOWN
#define XK_Select SDLK_SELECT
#define XK_Print SDLK_PRINTSCREEN
#define XK_Execute SDLK_EXECUTE
#define XK_Insert SDLK_INSERT
#define XK_Delete SDLK_DELETE
#define XK_Help SDLK_HELP
#define XK_F1 SDLK_F1
#define XK_F2 SDLK_F2
#define XK_F3 SDLK_F3
#define XK_F4 SDLK_F4
#define XK_F5 SDLK_F5
#define XK_F6 SDLK_F6
#define XK_F7 SDLK_F7
#define XK_F8 SDLK_F8
#define XK_F9 SDLK_F9
#define XK_F10 SDLK_F10
#define XK_F11 SDLK_F11
#define XK_F12 SDLK_F12
#define XK_F13 0xfff5
#define XK_F14 0xfff4
#define XK_F15 0xfff3
#define XK_F16 0xfff2
#define XK_F17 0xfff1
#define XK_F18 0xfff0
#define XK_F19 0xffef
#define XK_F20 0xffee
#define XK_F21 0xffed
#define XK_F22 0xffec
#define XK_F23 0xffeb
#define XK_F24 0xffea
#define XK_FMAX SDLK_F12
#define XK_Num_Lock SDLK_NUMLOCKCLEAR
#define XK_Scroll_Lock SDLK_SCROLLLOCK
#define XK_Find SDLK_FIND
#define XK_Menu SDLK_MENU
#define XK_Begin 0xffe9
#define XK_Muhenkan 0xffe8
#define XK_Henkan_Mode 0xffe7
#define XK_Zenkaku_Hankaku 0xffe6
#define XK_Hiragana_Katakana 0xffe5

#define XK_KP_Prior 0xffe4
#define XK_KP_Next 0xffe3
#define XK_KP_End 0xffe2
#define XK_KP_Home 0xffe1
#define XK_KP_Left 0xffe0
#define XK_KP_Up 0xffdf
#define XK_KP_Right 0xffde
#define XK_KP_Down 0xffdd
#define XK_KP_Insert 0xffdc
#define XK_KP_Delete 0xffdb
#define XK_KP_F1 0xffda
#define XK_KP_F2 0xffd9
#define XK_KP_F3 0xffd8
#define XK_KP_F4 0xffd7
#define XK_KP_Begin 0xffd6
#define XK_KP_Multiply SDLK_KP_MULTIPLY
#define XK_KP_Add SDLK_KP_PLUS
#define XK_KP_Separator 0xffd5
#define XK_KP_Subtract SDLK_KP_MINUS
#define XK_KP_Decimal SDLK_KP_DECIMAL
#define XK_KP_Divide SDLK_KP_DIVIDE
#define XK_KP_0 SDLK_KP_0
#define XK_KP_1 SDLK_KP_1
#define XK_KP_2 SDLK_KP_2
#define XK_KP_3 SDLK_KP_3
#define XK_KP_4 SDLK_KP_4
#define XK_KP_5 SDLK_KP_5
#define XK_KP_6 SDLK_KP_6
#define XK_KP_7 SDLK_KP_7
#define XK_KP_8 SDLK_KP_8
#define XK_KP_9 SDLK_KP_9

#define IsKeypadKey(ksym) (0) /* XXX */
#define IsModifierKey(ksym) (0)

#define XK_ISO_Left_Tab (0) /* XXX */

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

KeySym XStringToKeysym(char *str);

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
#undef XIM_SPOT_IS_LINE_TOP
#undef USE_GC
#undef CHANGEABLE_CURSOR
#define PLUGIN_MODULE_SUFFIX "sdl2"
#undef KEY_REPEAT_BY_MYSELF
#define ROTATABLE_DISPLAY
#undef PSEUDO_COLOR_DISPLAY
#undef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
#undef SUPPORT_URGENT_BELL
#undef FORCE_UNICODE
#undef NEED_DISPLAY_SYNC_EVERY_TIME
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
#define NO_DISPLAY_FD

#endif
