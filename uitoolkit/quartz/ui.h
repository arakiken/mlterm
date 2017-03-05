/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef ___UI_H__
#define ___UI_H__

#ifndef MAC_OS_X_VERSION_MAX_ALLOWED

/* NSText.h */

/* Various important Unicode code points */
enum {
  NSEnterCharacter = 0x0003,
  NSBackspaceCharacter = 0x0008,
  NSTabCharacter = 0x0009,
  NSNewlineCharacter = 0x000a,
  NSFormFeedCharacter = 0x000c,
  NSCarriageReturnCharacter = 0x000d,
  NSBackTabCharacter = 0x0019,
  NSDeleteCharacter = 0x007f,
  NSLineSeparatorCharacter = 0x2028,
  NSParagraphSeparatorCharacter = 0x2029
};

/* NSObjCRuntime.h */

#include <limits.h>
#define NSUIntegerMax ULONG_MAX

/* NSEvent.h */

enum { /* various types of events */
       NSLeftMouseDown = 1,
       NSLeftMouseUp = 2,
       NSRightMouseDown = 3,
       NSRightMouseUp = 4,
       NSMouseMoved = 5,
       NSLeftMouseDragged = 6,
       NSRightMouseDragged = 7,
       NSMouseEntered = 8,
       NSMouseExited = 9,
       NSKeyDown = 10,
       NSKeyUp = 11,
       NSFlagsChanged = 12,
       NSAppKitDefined = 13,
       NSSystemDefined = 14,
       NSApplicationDefined = 15,
       NSPeriodic = 16,
       NSCursorUpdate = 17,
       NSScrollWheel = 22,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
       NSTabletPoint = 23,
       NSTabletProximity = 24,
#endif
       NSOtherMouseDown = 25,
       NSOtherMouseUp = 26,
       NSOtherMouseDragged = 27,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
       /* The following event types are available on some hardware on 10.5.2 and
          later */
       NSEventTypeGesture = 29,
       NSEventTypeMagnify = 30,
       NSEventTypeSwipe = 31,
       NSEventTypeRotate = 18,
       NSEventTypeBeginGesture = 19,
       NSEventTypeEndGesture = 20
#endif
};

enum { /* masks for the types of events */
       NSLeftMouseDownMask = 1 << NSLeftMouseDown,
       NSLeftMouseUpMask = 1 << NSLeftMouseUp,
       NSRightMouseDownMask = 1 << NSRightMouseDown,
       NSRightMouseUpMask = 1 << NSRightMouseUp,
       NSMouseMovedMask = 1 << NSMouseMoved,
       NSLeftMouseDraggedMask = 1 << NSLeftMouseDragged,
       NSRightMouseDraggedMask = 1 << NSRightMouseDragged,
       NSMouseEnteredMask = 1 << NSMouseEntered,
       NSMouseExitedMask = 1 << NSMouseExited,
       NSKeyDownMask = 1 << NSKeyDown,
       NSKeyUpMask = 1 << NSKeyUp,
       NSFlagsChangedMask = 1 << NSFlagsChanged,
       NSAppKitDefinedMask = 1 << NSAppKitDefined,
       NSSystemDefinedMask = 1 << NSSystemDefined,
       NSApplicationDefinedMask = 1 << NSApplicationDefined,
       NSPeriodicMask = 1 << NSPeriodic,
       NSCursorUpdateMask = 1 << NSCursorUpdate,
       NSScrollWheelMask = 1 << NSScrollWheel,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
       NSTabletPointMask = 1 << NSTabletPoint,
       NSTabletProximityMask = 1 << NSTabletProximity,
#endif
       NSOtherMouseDownMask = 1 << NSOtherMouseDown,
       NSOtherMouseUpMask = 1 << NSOtherMouseUp,
       NSOtherMouseDraggedMask = 1 << NSOtherMouseDragged,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_5
       /* The following event masks are available on some hardware on 10.5.2 and
          later */
       NSEventMaskGesture = 1 << NSEventTypeGesture,
       NSEventMaskMagnify = 1 << NSEventTypeMagnify,
       NSEventMaskSwipe = 1U << NSEventTypeSwipe,
       NSEventMaskRotate = 1 << NSEventTypeRotate,
       NSEventMaskBeginGesture = 1 << NSEventTypeBeginGesture,
       NSEventMaskEndGesture = 1 << NSEventTypeEndGesture,
#endif
       NSAnyEventMask = NSUIntegerMax
};

/* Device-independent bits found in event modifier flags */
enum {
  NSAlphaShiftKeyMask = 1 << 16,
  NSShiftKeyMask = 1 << 17,
  NSControlKeyMask = 1 << 18,
  NSAlternateKeyMask = 1 << 19,
  NSCommandKeyMask = 1 << 20,
  NSNumericPadKeyMask = 1 << 21,
  NSHelpKeyMask = 1 << 22,
  NSFunctionKeyMask = 1 << 23,
#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_4
  NSDeviceIndependentModifierFlagsMask = 0xffff0000UL
#endif
};

/* Unicodes we reserve for function keys on the keyboard,  OpenStep reserves the
 * range 0xF700-0xF8FF for this purpose.  The availability of various keys will
 * be system dependent. */
enum {
  NSUpArrowFunctionKey = 0xF700,
  NSDownArrowFunctionKey = 0xF701,
  NSLeftArrowFunctionKey = 0xF702,
  NSRightArrowFunctionKey = 0xF703,
  NSF1FunctionKey = 0xF704,
  NSF2FunctionKey = 0xF705,
  NSF3FunctionKey = 0xF706,
  NSF4FunctionKey = 0xF707,
  NSF5FunctionKey = 0xF708,
  NSF6FunctionKey = 0xF709,
  NSF7FunctionKey = 0xF70A,
  NSF8FunctionKey = 0xF70B,
  NSF9FunctionKey = 0xF70C,
  NSF10FunctionKey = 0xF70D,
  NSF11FunctionKey = 0xF70E,
  NSF12FunctionKey = 0xF70F,
  NSF13FunctionKey = 0xF710,
  NSF14FunctionKey = 0xF711,
  NSF15FunctionKey = 0xF712,
  NSF16FunctionKey = 0xF713,
  NSF17FunctionKey = 0xF714,
  NSF18FunctionKey = 0xF715,
  NSF19FunctionKey = 0xF716,
  NSF20FunctionKey = 0xF717,
  NSF21FunctionKey = 0xF718,
  NSF22FunctionKey = 0xF719,
  NSF23FunctionKey = 0xF71A,
  NSF24FunctionKey = 0xF71B,
  NSF25FunctionKey = 0xF71C,
  NSF26FunctionKey = 0xF71D,
  NSF27FunctionKey = 0xF71E,
  NSF28FunctionKey = 0xF71F,
  NSF29FunctionKey = 0xF720,
  NSF30FunctionKey = 0xF721,
  NSF31FunctionKey = 0xF722,
  NSF32FunctionKey = 0xF723,
  NSF33FunctionKey = 0xF724,
  NSF34FunctionKey = 0xF725,
  NSF35FunctionKey = 0xF726,
  NSInsertFunctionKey = 0xF727,
  NSDeleteFunctionKey = 0xF728,
  NSHomeFunctionKey = 0xF729,
  NSBeginFunctionKey = 0xF72A,
  NSEndFunctionKey = 0xF72B,
  NSPageUpFunctionKey = 0xF72C,
  NSPageDownFunctionKey = 0xF72D,
  NSPrintScreenFunctionKey = 0xF72E,
  NSScrollLockFunctionKey = 0xF72F,
  NSPauseFunctionKey = 0xF730,
  NSSysReqFunctionKey = 0xF731,
  NSBreakFunctionKey = 0xF732,
  NSResetFunctionKey = 0xF733,
  NSStopFunctionKey = 0xF734,
  NSMenuFunctionKey = 0xF735,
  NSUserFunctionKey = 0xF736,
  NSSystemFunctionKey = 0xF737,
  NSPrintFunctionKey = 0xF738,
  NSClearLineFunctionKey = 0xF739,
  NSClearDisplayFunctionKey = 0xF73A,
  NSInsertLineFunctionKey = 0xF73B,
  NSDeleteLineFunctionKey = 0xF73C,
  NSInsertCharFunctionKey = 0xF73D,
  NSDeleteCharFunctionKey = 0xF73E,
  NSPrevFunctionKey = 0xF73F,
  NSNextFunctionKey = 0xF740,
  NSSelectFunctionKey = 0xF741,
  NSExecuteFunctionKey = 0xF742,
  NSUndoFunctionKey = 0xF743,
  NSRedoFunctionKey = 0xF744,
  NSFindFunctionKey = 0xF745,
  NSHelpFunctionKey = 0xF746,
  NSModeSwitchFunctionKey = 0xF747
};

#endif

typedef struct { int fd; } Display;

typedef int XIC;
typedef int XID;
typedef void* Window; /* NSView */
typedef int Drawable;
typedef struct CGImage* Pixmap;
typedef struct CGImage* PixmapMask;
typedef int GC;
typedef int Font;
#ifndef __QUICKDRAWTYPES__
typedef int Cursor;
#endif
typedef int KeyCode; /* Same as type of wparam */
typedef int KeySym;  /* Same as type of wparam */

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
#define ShiftMask NSShiftKeyMask
#define LockMask 0
#define ControlMask NSControlKeyMask
#define Mod1Mask NSAlternateKeyMask
#define Mod2Mask NSCommandKeyMask
#define Mod3Mask 0
#define Mod4Mask 0
#define Mod5Mask 0
#define CommandMask NSCommandKeyMask
#define Button1Mask NSLeftMouseDownMask
#define Button2Mask 0
#define Button3Mask NSRightMouseDownMask
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
#define XK_BackSpace NSDeleteCharacter
#define XK_Tab NSTabCharacter
#define XK_Clear NSClearDisplayFunctionKey
#define XK_Linefeed 0xfffa /* dummy */
#define XK_Return NSCarriageReturnCharacter

#define XK_Shift_L 0xfff9   /* dummy */
#define XK_Control_L 0xfff8 /* dummy */
#define XK_Alt_L 0xfff7     /* dummy */
#define XK_Shift_R 0xfff6   /* dummy */
#define XK_Control_R 0xfff5 /* dummy */
#define XK_Alt_R 0xfff4     /* dummy */

#define XK_Meta_L 0xfff3 /* dummy */
#define XK_Meta_R 0xfff2 /* dummy */

#define XK_Pause NSPauseFunctionKey
#define XK_Shift_Lock 0xfff1 /* dummy */
#define XK_Caps_Lock 0xfff0  /* dummy */
#define XK_Escape 0x1b
#define XK_Prior NSPageUpFunctionKey
#define XK_Next NSPageDownFunctionKey
#define XK_End NSEndFunctionKey
#define XK_Home NSHomeFunctionKey
#define XK_Left NSLeftArrowFunctionKey
#define XK_Up NSUpArrowFunctionKey
#define XK_Right NSRightArrowFunctionKey
#define XK_Down NSDownArrowFunctionKey
#define XK_Select NSSelectFunctionKey
#define XK_Print NSPrintFunctionKey
#define XK_Execute NSExecuteFunctionKey
#define XK_Insert NSHelpFunctionKey
#define XK_Delete NSDeleteFunctionKey
#define XK_Help NSHelpFunctionKey
#define XK_F1 NSF1FunctionKey
#define XK_F2 NSF2FunctionKey
#define XK_F3 NSF3FunctionKey
#define XK_F4 NSF4FunctionKey
#define XK_F5 NSF5FunctionKey
#define XK_F6 NSF6FunctionKey
#define XK_F7 NSF7FunctionKey
#define XK_F8 NSF8FunctionKey
#define XK_F9 NSF9FunctionKey
#define XK_F10 NSF10FunctionKey
#define XK_F11 NSF11FunctionKey
#define XK_F12 NSF12FunctionKey
#define XK_F13 NSF13FunctionKey
#define XK_F14 NSF14FunctionKey
#define XK_F15 NSF15FunctionKey
#define XK_F16 NSF16FunctionKey
#define XK_F17 NSF17FunctionKey
#define XK_F18 NSF18FunctionKey
#define XK_F19 NSF19FunctionKey
#define XK_F20 NSF20FunctionKey
#define XK_F21 NSF21FunctionKey
#define XK_F22 NSF22FunctionKey
#define XK_F23 NSF23FunctionKey
#define XK_F24 NSF24FunctionKey
#define XK_FMAX XK_F24
#define XK_Num_Lock 0xffef /* dummy */
#define XK_Scroll_Lock NSScrollLockFunctionKey
#define XK_Find NSFindFunctionKey
#define XK_Menu NSMenuFunctionKey
#define XK_Begin NSBeginFunctionKey
#define XK_Muhenkan 0xffee        /* dummy */
#define XK_Henkan_Mode 0xffed     /* dummy */
#define XK_Zenkaku_Hankaku 0xffec /* dummy */

#define XK_KP_Prior 0xffe8     /* dummy */
#define XK_KP_Next 0xffe7      /* dummy */
#define XK_KP_End 0xffe6       /* dummy */
#define XK_KP_Home 0xffe5      /* dummy */
#define XK_KP_Left 0xffe4      /* dummy */
#define XK_KP_Up 0xffe3        /* dummy */
#define XK_KP_Right 0xffe2     /* dummy */
#define XK_KP_Down 0xffe1      /* dummy */
#define XK_KP_Insert 0xffe0    /* dummy */
#define XK_KP_Delete 0xffdf    /* dummy */
#define XK_KP_F1 0xffde        /* dummy */
#define XK_KP_F2 0xffdd        /* dummy */
#define XK_KP_F3 0xffdc        /* dummy */
#define XK_KP_F4 0xffdb        /* dummy */
#define XK_KP_Begin 0xffda     /* dummy */
#define XK_KP_Multiply 0xffd9  /* dummy */
#define XK_KP_Add 0xffd8       /* dummy */
#define XK_KP_Separator 0xffd7 /* dummy */
#define XK_KP_Subtract 0xffd6  /* dummy */
#define XK_KP_Decimal 0xffd5   /* dummy */
#define XK_KP_Divide 0xffd4    /* dummy */
#define XK_KP_0 0xffd3         /* dummy */
#define XK_KP_1 0xffd2         /* dummy */
#define XK_KP_2 0xffd1         /* dummy */
#define XK_KP_3 0xffd0         /* dummy */
#define XK_KP_4 0xffcf         /* dummy */
#define XK_KP_5 0xffce         /* dummy */
#define XK_KP_6 0xffcd         /* dummy */
#define XK_KP_7 0xffcc         /* dummy */
#define XK_KP_8 0xffcb         /* dummy */
#define XK_KP_9 0xffca         /* dummy */

#define IsKeypadKey(ksym) (0)
#define IsModifierKey(ksym) (0)

#define XK_ISO_Left_Tab NSBackTabCharacter

/* XPoint(short x, short y) in Xlib. POINT(float x, float y) in win32. */
#define XPoint NSPoint

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
#define SUPPORT_TRUE_TRANSPARENT_BG
#define TYPE_XCORE_SCALABLE
#undef MANAGE_ROOT_WINDOWS_BY_MYSELF
#undef MANAGE_SUB_WINDOWS_BY_MYSELF
#undef INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#define SUPPORT_POINT_SIZE_FONT
#undef XIM_SPOT_IS_LINE_TOP
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

#endif
