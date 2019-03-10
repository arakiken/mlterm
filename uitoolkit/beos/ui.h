/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_H__
#define ___UI_H__

#ifndef _INTERFACE_DEFS_H

/* InterfaceDefs.h */
enum {
  B_BACKSPACE   = 0x08,
  B_RETURN      = 0x0a,
  B_ENTER       = 0x0a,
  B_SPACE       = 0x20,
  B_TAB         = 0x09,
  B_ESCAPE      = 0x1b,
  B_SUBSTITUTE  = 0x1a,

  B_LEFT_ARROW  = 0x1c,
  B_RIGHT_ARROW = 0x1d,
  B_UP_ARROW    = 0x1e,
  B_DOWN_ARROW  = 0x1f,

  B_INSERT      = 0x05,
  B_DELETE      = 0x7f,
  B_HOME        = 0x01,
  B_END         = 0x04,
  B_PAGE_UP     = 0x0b,
  B_PAGE_DOWN   = 0x0c,

  B_FUNCTION_KEY = 0x10,

  /* for Japanese and Korean keyboards */
  B_KATAKANA_HIRAGANA = 0xf2,
  B_HANKAKU_ZENKAKU = 0xf3,
  B_HANGUL      = 0xf0,
  B_HANGUL_HANJA = 0xf1
};

enum {
  B_F1_KEY      = 0x02,
  B_F2_KEY      = 0x03,
  B_F3_KEY      = 0x04,
  B_F4_KEY      = 0x05,
  B_F5_KEY      = 0x06,
  B_F6_KEY      = 0x07,
  B_F7_KEY      = 0x08,
  B_F8_KEY      = 0x09,
  B_F9_KEY      = 0x0a,
  B_F10_KEY     = 0x0b,
  B_F11_KEY     = 0x0c,
  B_F12_KEY     = 0x0d,
  B_PRINT_KEY   = 0x0e,
  B_SCROLL_KEY  = 0x0f,
  B_PAUSE_KEY   = 0x10
};

enum {
  B_SHIFT_KEY     = 0x00000001,
  B_COMMAND_KEY   = 0x00000002,
  B_CONTROL_KEY   = 0x00000004,
  B_CAPS_LOCK     = 0x00000008,
  B_SCROLL_LOCK   = 0x00000010,
  B_NUM_LOCK      = 0x00000020,
  B_OPTION_KEY    = 0x00000040,
  B_MENU_KEY      = 0x00000080,
  B_LEFT_SHIFT_KEY  = 0x00000100,
  B_RIGHT_SHIFT_KEY = 0x00000200,
  B_LEFT_COMMAND_KEY  = 0x00000400,
  B_RIGHT_COMMAND_KEY = 0x00000800,
  B_LEFT_CONTROL_KEY  = 0x00001000,
  B_RIGHT_CONTROL_KEY = 0x00002000,
  B_LEFT_OPTION_KEY = 0x00004000,
  B_RIGHT_OPTION_KEY  = 0x00008000
};

#endif

typedef struct { int fd; } Display;

typedef int XIC;
typedef int XID;
typedef void *Window; /* BView/BWindow */
typedef void *Drawable;
typedef void *Pixmap; /* BBitmap */
typedef void *PixmapMask;
typedef int GC;
typedef void *Font; /* BFont */
typedef int Cursor;
typedef int KeyCode;
typedef int KeySym;

typedef struct /* Same as definition in X11/X.h */ {
  int max_keypermod;
  KeyCode *modifiermap;

} XModifierKeymap;

typedef struct /* Same as definition in X11/X.h */ {
  unsigned char byte1;
  unsigned char byte2;

} XChar2b;

#define UI_FOCUS_IN 1
#define UI_FOCUS_OUT 2
#define UI_BUTTON_PRESS 3
#define UI_BUTTON_RELEASE 4
#define UI_BUTTON_MOTION 5
#define UI_KEY_PRESS 6
#define UI_EXPOSE 7
#define UI_SELECTION_REQUESTED 8
#define UI_CLOSE_WINDOW 9
#define UI_KEY_FOCUS_IN 10
#define UI_SELECTION_NOTIFIED 11
#define UI_POINTER_MOTION 12

typedef struct { int type; } XEvent;

typedef struct {
  int type;
  unsigned int state;
  KeySym keysym;
  const char *utf8;

} XKeyEvent;

typedef unsigned long Time; /* Same as definition in X11/X.h */
typedef unsigned long Atom; /* Same as definition in X11/X.h */

typedef struct {
  int type;
  int time;
  int x;
  int y;
  unsigned int state;
  unsigned int button;
  int click_count;

} XButtonEvent;

typedef struct {
  int type;
  int time;
  int x;
  int y;
  unsigned int state;

} XMotionEvent;

typedef struct {
  int type;
  int x;
  int y;
  unsigned int width;
  unsigned int height;
  int force_expose;

} XExposeEvent;

typedef struct {
  int type;
  void *sender;

} XSelectionRequestEvent;

typedef struct {
  int type;
  char *data;
  unsigned int len;

} XSelectionNotifyEvent;

typedef struct {
  Font fid;

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
#define ShiftMask B_SHIFT_KEY
#define LockMask 0
#define ControlMask B_CONTROL_KEY
#define Mod1Mask B_MENU_KEY
#define Mod2Mask 0
#define Mod3Mask 0
#define Mod4Mask 0
#define Mod5Mask 0
#define CommandMask B_COMMAND_KEY
#define Button1Mask 0
#define Button2Mask 0
#define Button3Mask 0
#define Button4Mask 0
#define Button5Mask 0
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5

#define XK_Super_L 0xfffe /* dummy */
#define XK_Super_R 0xfffd /* dummy */
#define XK_Hyper_L 0xfffc /* dummy */
#define XK_Hyper_R 0xfffb /* dummy */
#define XK_BackSpace B_BACKSPACE
#define XK_Tab B_TAB
#define XK_Clear 0xfffa /* dummy */
#define XK_Linefeed 0xfff9 /* dummy */
#define XK_Return B_RETURN

#define XK_Shift_L 0xfff8   /* dummy */
#define XK_Control_L 0xfff7 /* dummy */
#define XK_Alt_L 0xfff6     /* dummy */
#define XK_Shift_R 0xfff5   /* dummy */
#define XK_Control_R 0xfff4 /* dummy */
#define XK_Alt_R 0xfff3     /* dummy */

#define XK_Meta_L 0xfff2 /* dummy */
#define XK_Meta_R 0xfff1 /* dummy */

#define XK_Pause B_PAUSE_KEY
#define XK_Shift_Lock 0xfff0 /* dummy */
#define XK_Caps_Lock 0xffef  /* dummy */
#define XK_Escape B_ESCAPE
#define XK_Prior B_PAGE_UP
#define XK_Next B_PAGE_DOWN
#define XK_End B_END
#define XK_Home B_HOME
#define XK_Left B_LEFT_ARROW
#define XK_Up B_UP_ARROW
#define XK_Right B_RIGHT_ARROW
#define XK_Down B_DOWN_ARROW
#define XK_Select 0xffee /* dummy */
#define XK_Print B_PRINT_KEY
#define XK_Execute 0xffed /* dummy */
#define XK_Insert B_INSERT
#define XK_Delete B_DELETE
#define XK_Help 0xffec /* dummy */
#define XK_F1 (B_F1_KEY | 0xf000)
#define XK_F2 (B_F2_KEY | 0xf000)
#define XK_F3 (B_F3_KEY | 0xf000)
#define XK_F4 (B_F4_KEY | 0xf000)
#define XK_F5 (B_F5_KEY | 0xf000)
#define XK_F6 (B_F6_KEY | 0xf000)
#define XK_F7 (B_F7_KEY | 0xf000)
#define XK_F8 (B_F8_KEY | 0xf000)
#define XK_F9 (B_F9_KEY | 0xf000)
#define XK_F10 (B_F10_KEY | 0xf000)
#define XK_F11 (B_F11_KEY | 0xf000)
#define XK_F12 (B_F12_KEY | 0xf000)
#define XK_F13 0xffeb /* dummy */
#define XK_F14 0xffea /* dummy */
#define XK_F15 0xffe9 /* dummy */
#define XK_F16 0xffe8 /* dummy */
#define XK_F17 0xffe7 /* dummy */
#define XK_F18 0xffe6 /* dummy */
#define XK_F19 0xffe5 /* dummy */
#define XK_F20 0xffe4 /* dummy */
#define XK_F21 0xffe3 /* dummy */
#define XK_F22 0xffe2 /* dummy */
#define XK_F23 0xffe1 /* dummy */
#define XK_F24 0xffe0 /* dummy */
#define XK_FMAX XK_F12
#define XK_Num_Lock 0xffdf /* dummy */
#define XK_Scroll_Lock 0xffde /* dummy */
#define XK_Find 0xffdd /* dummy */
#define XK_Menu 0xffdc /* dummy */
#define XK_Begin 0xffdb /* dummy */
#define XK_Muhenkan 0xffda        /* dummy */
#define XK_Henkan_Mode B_KATAKANA_HIRAGANA
#define XK_Zenkaku_Hankaku B_HANKAKU_ZENKAKU

#define XK_KP_Prior 0xffd9     /* dummy */
#define XK_KP_Next 0xffd8      /* dummy */
#define XK_KP_End 0xffd7       /* dummy */
#define XK_KP_Home 0xffd6      /* dummy */
#define XK_KP_Left 0xffd5      /* dummy */
#define XK_KP_Up 0xffd4        /* dummy */
#define XK_KP_Right 0xffd3     /* dummy */
#define XK_KP_Down 0xffd2      /* dummy */
#define XK_KP_Insert 0xffd1    /* dummy */
#define XK_KP_Delete 0xffd0    /* dummy */
#define XK_KP_F1 0xffcf        /* dummy */
#define XK_KP_F2 0xffce        /* dummy */
#define XK_KP_F3 0xffcd        /* dummy */
#define XK_KP_F4 0xffcc        /* dummy */
#define XK_KP_Begin 0xffcb     /* dummy */
#define XK_KP_Multiply 0xffca  /* dummy */
#define XK_KP_Add 0xffc9       /* dummy */
#define XK_KP_Separator 0xffc8 /* dummy */
#define XK_KP_Subtract 0xffc7  /* dummy */
#define XK_KP_Decimal 0xffc6   /* dummy */
#define XK_KP_Divide 0xffc5    /* dummy */
#define XK_KP_0 0xffc4         /* dummy */
#define XK_KP_1 0xffc3         /* dummy */
#define XK_KP_2 0xffc2         /* dummy */
#define XK_KP_3 0xffc1         /* dummy */
#define XK_KP_4 0xffc0         /* dummy */
#define XK_KP_5 0xffbf         /* dummy */
#define XK_KP_6 0xffbe         /* dummy */
#define XK_KP_7 0xffbd         /* dummy */
#define XK_KP_8 0xffbc         /* dummy */
#define XK_KP_9 0xffbb         /* dummy */

#define IsKeypadKey(ksym) (0)
#define IsModifierKey(ksym) (0)

#define XK_ISO_Left_Tab 0xffba /* dummy */

/* XPoint(short x, short y) in Xlib. POINT(float x, float y) in win32. */
typedef struct {
  short x;
  short y;
} XPoint;

/* XXX dummy */
#define XKeysymToKeycode(disp, ks) (ks)
#define XKeycodeToKeysym(disp, kc, i) (kc)
#define XKeysymToString(ks) ""
#define DefaultScreen(disp) (0)

#define BlackPixel(disp, screen) (0xff000000)
#define WhitePixel(disp, screen) (0xffffffff)

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

#undef UI_COLOR_HAS_RGB
#undef SUPPORT_TRUE_TRANSPARENT_BG
#define TYPE_XCORE_SCALABLE
#undef MANAGE_ROOT_WINDOWS_BY_MYSELF
#undef MANAGE_SUB_WINDOWS_BY_MYSELF
#undef INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#define SUPPORT_POINT_SIZE_FONT
#define XIM_SPOT_IS_LINE_TOP
#undef USE_GC
#undef CHANGEABLE_CURSOR
#undef PLUGIN_MODULE_SUFFIX
#undef KEY_REPEAT_BY_MYSELF
#undef ROTATABLE_DISPLAY
#undef PSEUDO_COLOR_DISPLAY
#undef WALL_PICTURE_SIXEL_REPLACES_SYSTEM_PALETTE
#define SUPPORT_URGENT_BELL
#define FORCE_UNICODE
#undef NEED_DISPLAY_SYNC_EVERY_TIME
#undef DRAW_SCREEN_IN_PIXELS
#define NO_DRAW_IMAGE_STRING
/* libpthread is not linked to mlterm explicitly for now. */
#undef HAVE_PTHREAD
#undef COMPOSE_DECSP_FONT
#undef USE_REAL_VERTICAL_FONT
#undef NO_DISPLAY_FD
#undef FLICK_SCROLL
#define UIWINDOW_SUPPORTS_PREEDITING

#endif
