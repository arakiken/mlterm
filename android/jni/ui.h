/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_H__
#define ___UI_H__

#include <jni.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>

typedef struct {
  struct android_app *app;

  ANativeWindow_Buffer buf;
  unsigned int bytes_per_pixel;
  unsigned int yoffset;

  struct rgb_info {
    unsigned int r_limit;
    unsigned int g_limit;
    unsigned int b_limit;
    unsigned int r_offset;
    unsigned int g_offset;
    unsigned int b_offset;

  } rgbinfo;

  ASensorManager *sensor_man;
  const ASensor *accel_sensor;
  ASensorEventQueue *sensor_evqueue;

  int key_state;
  int button_state;
  int lock_state;

  unsigned int long_press_counter;

} Display;

#define PIXEL_RED(pixel, rgbinfo) (((pixel) >> (rgbinfo).r_offset) << (rgbinfo).r_limit)
#define PIXEL_BLUE(pixel, rgbinfo) (((pixel) >> (rgbinfo).b_offset) << (rgbinfo).b_limit)
#define PIXEL_GREEN(pixel, rgbinfo) (((pixel) >> (rgbinfo).g_offset) << (rgbinfo).g_limit)
#define RGB_TO_PIXEL(r, g, b, rgbinfo)                  \
  ((((r) >> (rgbinfo).r_limit) << (rgbinfo).r_offset) | \
   (((g) >> (rgbinfo).g_limit) << (rgbinfo).g_offset) | \
   (((b) >> (rgbinfo).b_limit) << (rgbinfo).b_offset))

typedef int XIC;      /* dummy */
typedef void *XID;      /* dummy */
typedef void *Window;   /* dummy */
typedef void *Drawable; /* dummy */

typedef struct {
  unsigned char *image;
  unsigned int width;
  unsigned int height;

} * Pixmap;

typedef unsigned char *PixmapMask;
typedef int GC;     /* dummy */
typedef int Font;   /* dummy */
typedef int Cursor; /* dummy */
typedef int KeyCode;
typedef int KeySym;

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

typedef struct {
  int type;
  unsigned int state;
  KeySym ksym;
  unsigned int keycode;

} XKeyEvent;

typedef unsigned long Time; /* Same as definition in X11/X.h */
typedef unsigned long Atom; /* Same as definition in X11/X.h */

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

typedef union {
  int type;
  XKeyEvent xkey;
  XButtonEvent xbutton;
  XMotionEvent xmotion;

} XEvent;

typedef int XSelectionRequestEvent; /* dummy */

typedef struct {
  char *file;

  int32_t format; /* XXX (fontsize|FONT_BOLD|FONT_ITALIC) on freetype. */

  int32_t num_glyphs;
  unsigned char *glyphs;

  int32_t glyph_width_bytes;

  unsigned char width;
  /* Width of full width characters or max width of half width characters. */
  unsigned char width_full;
  unsigned char height;
  unsigned char ascent;

  int16_t *glyph_indeces;

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

/* Not defined in android/keycode.h */
#define AKEYCODE_ESCAPE 0x6f
#define AKEYCODE_CONTROL_LEFT 0x71
#define AKEYCODE_CONTROL_RIGHT 0x72

#define XK_Super_L 0xfffe /* dummy */
#define XK_Super_R 0xfffd /* dummy */
#define XK_Hyper_L 0xfffc /* dummy */
#define XK_Hyper_R 0xfffb /* dummy */
#define XK_BackSpace 0x08
#define XK_Tab 0x09
#define XK_Clear (AKEYCODE_CLEAR + 0x100)
#define XK_Linefeed 0xfffa /* dummy */
#define XK_Return 0x0d

#define XK_Shift_L (AKEYCODE_SHIFT_LEFT + 0x100)
#define XK_Control_L (AKEYCODE_CONTROL_LEFT + 0x100)
#define XK_Alt_L (AKEYCODE_ALT_LEFT + 0x100)
#define XK_Shift_R (AKEYCODE_SHIFT_RIGHT + 0x100)
#define XK_Control_R (AKEYCODE_CONTROL_RIGHT + 0x100)
#define XK_Alt_R (AKEYCODE_ALT_RIGHT + 0x100)

#define XK_Meta_L 0xfff7 /* dummy */
#define XK_Meta_R 0xfff6 /* dummy */

#define XK_Pause 0xfff5      /* dummy */
#define XK_Shift_Lock 0xfff4 /* dummy */
#define XK_Caps_Lock 0xfff3  /* dummy */
#define XK_Escape 0x1b
#define XK_Prior (AKEYCODE_PAGE_UP + 0x100)
#define XK_Next (AKEYCODE_PAGE_DOWN + 0x100)
#define XK_End 0x17b
#define XK_Home 0x17a
#define XK_Left (AKEYCODE_DPAD_LEFT + 0x100)
#define XK_Up (AKEYCODE_DPAD_UP + 0x100)
#define XK_Right (AKEYCODE_DPAD_RIGHT + 0x100)
#define XK_Down (AKEYCODE_DPAD_DOWN + 0x100)
#define XK_Select 0xfff1  /* dummy */
#define XK_Print 0xfff0   /* dummy */
#define XK_Execute 0xffef /* dummy */
#define XK_Insert 0x17c
#define XK_Delete 0x170
#define XK_Help 0xffed /* dummy */
#define XK_F1 0x183
#define XK_F2 0x184
#define XK_F3 0x185
#define XK_F4 0x186
#define XK_F5 0x187
#define XK_F6 0x188
#define XK_F7 0x189
#define XK_F8 0x18a
#define XK_F9 0x18b
#define XK_F10 0x18c
#define XK_F11 0x18d
#define XK_F12 0x18e
#define XK_F13 0xffe0 /* dummy */
#define XK_F14 0xffdf /* dummy */
#define XK_F15 0xffde /* dummy */
#define XK_F16 0xffdd /* dummy */
#define XK_F17 0xffdc /* dummy */
#define XK_F18 0xffdb /* dummy */
#define XK_F19 0xffda /* dummy */
#define XK_F20 0xffd9 /* dummy */
#define XK_F21 0xffd8 /* dummy */
#define XK_F22 0xffd7 /* dummy */
#define XK_F23 0xffd6 /* dummy */
#define XK_F24 0xffd5 /* dummy */
#define XK_FMAX XK_F12
#define XK_Num_Lock 0xffd4          /* dummy */
#define XK_Scroll_Lock 0xffd3       /* dummy */
#define XK_Find 0xffd2              /* dummy */
#define XK_Menu 0xffd1              /* dummy */
#define XK_Begin 0xffd0             /* dummy */
#define XK_Muhenkan 0xffcf          /* dummy */
#define XK_Henkan_Mode 0xffce       /* dummy */
#define XK_Zenkaku_Hankaku 0xffcd   /* dummy */
#define XK_Hiragana_Katakana 0xffcc /* dummy */

#define XK_KP_Prior 0xffcd     /* dummy */
#define XK_KP_Next 0xffcc      /* dummy */
#define XK_KP_End 0xffcb       /* dummy */
#define XK_KP_Home 0xffca      /* dummy */
#define XK_KP_Left 0xffc9      /* dummy */
#define XK_KP_Up 0xffc8        /* dummy */
#define XK_KP_Right 0xffc7     /* dummy */
#define XK_KP_Down 0xffc6      /* dummy */
#define XK_KP_Insert 0xffc5    /* dummy */
#define XK_KP_Delete 0xffc4    /* dummy */
#define XK_KP_F1 0xffc3        /* dummy */
#define XK_KP_F2 0xffc2        /* dummy */
#define XK_KP_F3 0xffc1        /* dummy */
#define XK_KP_F4 0xffc0        /* dummy */
#define XK_KP_Begin 0xffbf     /* dummy */
#define XK_KP_Multiply 0xffbe  /* dummy */
#define XK_KP_Add 0xffbd       /* dummy */
#define XK_KP_Separator 0xffbc /* dummy */
#define XK_KP_Subtract 0xffbb  /* dummy */
#define XK_KP_Decimal 0xffba   /* dummy */
#define XK_KP_Divide 0xffb9    /* dummy */
#define XK_KP_0 0xffb8         /* dummy */
#define XK_KP_1 0xffb7         /* dummy */
#define XK_KP_2 0xffb6         /* dummy */
#define XK_KP_3 0xffb5         /* dummy */
#define XK_KP_4 0xffb4         /* dummy */
#define XK_KP_5 0xffb3         /* dummy */
#define XK_KP_6 0xffb2         /* dummy */
#define XK_KP_7 0xffb1         /* dummy */
#define XK_KP_8 0xffb0         /* dummy */
#define XK_KP_9 0xffaf         /* dummy */

#define IsKeypadKey(ksym) (1)
#define IsModifierKey(ksym) (0)

#define XK_ISO_Left_Tab 0xffae /* dummy */

/* Same as definition in X11/X.h */
typedef struct {
  short x;
  short y;

} XPoint;

/* XXX dummy */
#define XKeysymToKeycode(disp, ks) (ks)
#define XKeycodeToKeysym(disp, kc, i) (kc)
#define XKeysymToString(ks) ""
#define DisplayString(disp) ":0.0"
#define DefaultScreen(disp) (0)

#define BlackPixel(disp, screen) (0)
#define WhitePixel(disp, screen) (-1)

/* Same as definition in X11/cursorfont.h */
#define XC_xterm 152

#define XC_left_ptr 0

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
#undef SUPPORT_TRUE_TRANSPARENT_BG
#ifdef USE_FREETYPE
/* XXX pcf fonts isn't scalable, though... */
#define TYPE_XCORE_SCALABLE
#else
#undef TYPE_XCORE_SCALABLE
#endif
#define MANAGE_ROOT_WINDOWS_BY_MYSELF
#define MANAGE_SUB_WINDOWS_BY_MYSELF
/* See also fb/ui_display.c where ui_picture_display_closed() is never called. */
#define INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#undef SUPPORT_POINT_SIZE_FONT
#undef USE_GC
#undef CHANGEABLE_CURSOR
#undef PLUGIN_MODULE_SUFFIX
#undef KEY_REPEAT_BY_MYSELF
#undef ROTATABLE_DISPLAY
#undef PSEUDO_COLOR_DISPLAY
#undef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
#undef SUPPORT_URGENT_BELL
#undef FORCE_UNICODE
#undef NEED_DISPLAY_SYNC_EVERY_TIME
#define DRAW_SCREEN_IN_PIXELS
#undef NO_DRAW_IMAGE_STRING
#define HAVE_PTHREAD
#define COMPOSE_DECSP_FONT
#ifdef USE_FREETYPE
#define USE_REAL_VERTICAL_FONT
#else
#undef USE_REAL_VERTICAL_FONT
#endif
#undef NO_DISPLAY_FD
#define FLICK_SCROLL
#undef UIWINDOW_SUPPORTS_PREEDITING

#endif
