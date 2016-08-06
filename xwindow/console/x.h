/*
 *	$Id$
 */

#ifndef  ___X_H__
#define  ___X_H__


#include  <stdio.h>
#include  <mkf/mkf_conv.h>


typedef struct
{
	FILE *  fp ;

	/* Actual width, while x_display_t.width excludes virtual kbd area. */
	unsigned int  width ;
	/* Actual height, while x_display_t.height excludes virtual kbd area. */
	unsigned int  height ;

	unsigned int  col_width ;
	unsigned int  line_height ;

	int  key_state ;
	int  lock_state ;

	mkf_conv_t *  conv ;

	unsigned char  buf[512] ;
	unsigned int  buf_len ;
	int  is_pressing ;

	void *  sixel_output ;
	void *  sixel_dither ;

	int  support_hmargin ;

} Display ;

typedef int  XIM ;		/* dummy */
typedef int  XIC ;		/* dummy */
typedef int  XIMStyle ;		/* dummy */

typedef void *  XID ;		/* dummy */
typedef void *  Window ;		/* dummy */
typedef void *  Drawable ;		/* dummy */

typedef struct
{
	unsigned char *  image ;
	unsigned int  width ;
	unsigned int  height ;

} *  Pixmap ;

typedef unsigned char *  PixmapMask ;
typedef int  GC ;		/* dummy */
typedef int  Font ;		/* dummy */
typedef int  Cursor ;		/* dummy */
typedef int  KeyCode ;
typedef int  KeySym ;

typedef struct		/* Same as definition in X11/X.h */
{
	int  max_keypermod ;
	KeyCode *  modifiermap ;

} XModifierKeymap ;

typedef struct		/* Same as definition in X11/X.h */
{
	unsigned char  byte1 ;
	unsigned char  byte2 ;

} XChar2b ;

typedef struct
{
	int  type ;
	unsigned int  state ;
	KeySym  ksym ;
	unsigned int  keycode ;

} XKeyEvent ;

typedef unsigned long  Time ;	/* Same as definition in X11/X.h */
typedef unsigned long  Atom ;	/* Same as definition in X11/X.h */

typedef struct
{
	int  type ;
	Time  time ;
	int  x ;
	int  y ;
	unsigned int  state ;
	unsigned int  button ;

} XButtonEvent ;

typedef struct
{
	int  type ;
	Time  time ;
	int  x ;
	int  y ;
	unsigned int  state ;

} XMotionEvent ;

typedef union
{
	int  type ;
	XKeyEvent  xkey ;
	XButtonEvent  xbutton ;
	XMotionEvent  xmotion ;

} XEvent ;

typedef int XSelectionRequestEvent ;	/* dummy */

typedef struct
{
	void *  dummy ;

} XFontStruct ;

typedef int XFontSet ;	/* dummy */

#define None		0L	/* Same as definition in X11/X.h */
#define NoSymbol	0L	/* Same as definition in X11/X.h */

#define CurrentTime	0L	/* Same as definition in X11/X.h */

/* Same as definition in X11/X.h */
#define NoEventMask			0L
#define KeyPressMask			(1L<<0)
#define KeyReleaseMask			(1L<<1)
#define ButtonPressMask			(1L<<2)
#define ButtonReleaseMask		(1L<<3)
#define EnterWindowMask			(1L<<4)
#define LeaveWindowMask			(1L<<5)
#define PointerMotionMask		(1L<<6)
#define PointerMotionHintMask		(1L<<7)
#define Button1MotionMask		(1L<<8)
#define Button2MotionMask		(1L<<9)
#define Button3MotionMask		(1L<<10)
#define Button4MotionMask		(1L<<11)
#define Button5MotionMask		(1L<<12)
#define ButtonMotionMask		(1L<<13)
#define KeymapStateMask			(1L<<14)
#define ExposureMask			(1L<<15)
#define VisibilityChangeMask		(1L<<16)
#define StructureNotifyMask		(1L<<17)
#define ResizeRedirectMask		(1L<<18)
#define SubstructureNotifyMask		(1L<<19)
#define SubstructureRedirectMask	(1L<<20)
#define FocusChangeMask			(1L<<21)
#define PropertyChangeMask		(1L<<22)
#define ColormapChangeMask		(1L<<23)
#define OwnerGrabButtonMask		(1L<<24)
#define ShiftMask	(1<<0)
#define LockMask	(1<<1)
#define ControlMask	(1<<2)
#define Mod1Mask	(1<<3)
#define Mod2Mask	(1<<4)
#define Mod3Mask	(1<<5)
#define Mod4Mask	(1<<6)
#define Mod5Mask	(1<<7)
#define Button1Mask	(1<<8)
#define Button2Mask	(1<<9)
#define Button3Mask	(1<<10)
#define Button4Mask	(1<<11)
#define Button5Mask	(1<<12)
#define Button1		1
#define Button2		2
#define Button3		3
#define Button4		4
#define Button5		5


#define XK_Super_L	0xff01
#define XK_Super_R	0xff02
#define XK_Hyper_L	0xff03
#define XK_Hyper_R	0xff04
#define XK_BackSpace	0x08
#define XK_Tab		0x09
#define XK_Clear	0xff05
#define XK_Linefeed	0xff06
#define XK_Return	0x0d

#define XK_Shift_L	0xff07
#define XK_Control_L	0xff08
#define XK_Alt_L	0xff09
#define XK_Shift_R	0xff0a
#define XK_Control_R	0xff0b
#define XK_Alt_R	0xff0c

#define XK_Meta_L	0xff0d
#define XK_Meta_R	0xff0e

#define XK_Pause	0xff0f
#define XK_Shift_Lock	0xff10
#define XK_Caps_Lock	0xff11
#define XK_Escape	0xff12
#define XK_Prior	0xff13
#define XK_Next		0xff14
#define XK_End		0xff15
#define XK_Home		0xff16
#define XK_Left		0xff17
#define XK_Up		0xff18
#define XK_Right	0xff19
#define XK_Down		0xff1a
#define XK_Select	0xff1b
#define XK_Print	0xff1c
#define XK_Execute	0xff1d
#define XK_Insert	0xff1e
#define XK_Delete	0xff1f
#define XK_Help		0xff20
#define XK_F1		0xff21
#define XK_F2		0xff22
#define XK_F3		0xff23
#define XK_F4		0xff24
#define XK_F5		0xff25
#define XK_F6		0xff26
#define XK_F7		0xff27
#define XK_F8		0xff28
#define XK_F9		0xff29
#define XK_F10		0xff2a
#define XK_F11		0xff2b
#define XK_F12		0xff2c
#define XK_F13		0xff2d
#define XK_F14		0xff2e
#define XK_F15		0xff2f
#define XK_F16		0xff30
#define XK_F17		0xff31
#define XK_F18		0xff32
#define XK_F19		0xff33
#define XK_F20		0xff34
#define XK_F21		0xff35
#define XK_F22		0xff36
#define XK_F23		0xff37
#define XK_F24		0xff38
#define XK_FMAX		XK_F24
#define XK_Num_Lock	0xff39
#define XK_Scroll_Lock	0xff3a
#define XK_Find		0xff3b
#define XK_Menu		0xff3c
#define XK_Begin	0xff3d
#define XK_Muhenkan	0xff3e
#define XK_Henkan_Mode	0xff3f
#define XK_Zenkaku_Hankaku	0xff40
#define XK_Hiragana_Katakana	0xff41

#define XK_KP_Prior	0xff42
#define XK_KP_Next	0xff43
#define XK_KP_End	0xff44
#define XK_KP_Home	0xff45
#define XK_KP_Left	0xff46
#define XK_KP_Up	0xff47
#define XK_KP_Right	0xff48
#define XK_KP_Down	0xff49
#define XK_KP_Insert	0xff4a
#define XK_KP_Delete	0xff4b
#define XK_KP_F1	0xff4c
#define XK_KP_F2	0xff4d
#define XK_KP_F3	0xff4e
#define XK_KP_F4	0xff4f
#define XK_KP_Begin	0xff50
#define XK_KP_Multiply	0xff51
#define XK_KP_Add	0xff52
#define XK_KP_Separator	0xff53
#define XK_KP_Subtract	0xff54
#define XK_KP_Decimal	0xff55
#define XK_KP_Divide	0xff56
#define XK_KP_0		0xff57
#define XK_KP_1		0xff58
#define XK_KP_2		0xff59
#define XK_KP_3		0xff5a
#define XK_KP_4		0xff5b
#define XK_KP_5		0xff5c
#define XK_KP_6		0xff5d
#define XK_KP_7		0xff5e
#define XK_KP_8		0xff5f
#define XK_KP_9		0xff60

#define IsKeypadKey(ksym)	(XK_KP_Prior <= (ksym) && (ksym) <= XK_KP_9)
#define IsModifierKey(ksym)      (XK_Shift_L <= (ksym) && (ksym) <= XK_Alt_R)

#define XK_ISO_Left_Tab	0xff61	/* dummy */


/* Same as definition in X11/X.h */
typedef struct
{
	short  x ;
	short  y ;

} XPoint ;


/* XXX dummy */
#define XKeysymToKeycode(disp,ks)  (ks)
#define XKeycodeToKeysym(disp,kc,i)  (kc)
#define XKeysymToString(ks)	""
#define DisplayString(disp)	":0.0"
#define DefaultScreen(disp)	(0)

#define BlackPixel(disp,screen)	 (0)
#define WhitePixel(disp,screen)	 (-1)

/* Same as definition in X11/cursorfont.h */
#define XC_xterm 152
#define XC_left_ptr 68

/* Same as definition in X11/Xutil.h */
#define NoValue         0x0000
#define XValue          0x0001
#define YValue          0x0002
#define WidthValue      0x0004
#define HeightValue     0x0008
#define AllValues       0x000F
#define XNegative       0x0010
#define YNegative       0x0020


int  XParseGeometry( char *  str , int *  x , int *  y ,
	unsigned int  *  width , unsigned int  *  height) ;

KeySym  XStringToKeysym( char *  str) ;


/* === Platform dependent options === */

#define  X_COLOR_HAS_RGB
#undef  SUPPORT_TRUE_TRANSPARENT_BG
/* Actually, fonts aren't scalable, but define TYPE_XCORE_SCALABLE to avoid double drawing. */
#define  TYPE_XCORE_SCALABLE
#define  MANAGE_WINDOWS_BY_MYSELF
/* See also console/x_display.c where x_picture_display_closed() is never called. */
#define  INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#undef  SUPPORT_POINT_SIZE_FONT


#endif
