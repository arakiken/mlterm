/*
 *	$Id$
 */

#ifndef  ___X_H__
#define  ___X_H__


#if defined (__linux__)
#include  <linux/fb.h>
#elif defined (__FreeBSD__)
#include  <osreldate.h>
#if __FreeBSD_version >= 410000
#include  <sys/fbio.h>
#else
#include  <machine/console.h>
#endif
#elif defined (__NetBSD__) || defined (__OpenBSD__)
#include  <dev/wscons/wsconsio.h>
#endif

#include  <kiklib/kik_types.h>


typedef struct
{
	int  fd ;

	int  fb_fd ;
	unsigned char *  fp ;
	unsigned int  smem_len ;
	unsigned int  line_length ;
	unsigned int  xoffset ;
	unsigned int  yoffset ;
	unsigned int  bytes_per_pixel ;

	struct rgb_info
	{
		unsigned int  r_limit ;
		unsigned int  g_limit ;
		unsigned int  b_limit ;
		unsigned int  r_offset ;
		unsigned int  g_offset ;
		unsigned int  b_offset ;

	} rgbinfo ;

} Display ;

#define  PIXEL_RED(pixel,rgbinfo) \
	(((pixel) >> (rgbinfo).r_offset) << (rgbinfo).r_limit)
#define  PIXEL_BLUE(pixel,rgbinfo) \
	(((pixel) >> (rgbinfo).b_offset) << (rgbinfo).b_limit)
#define  PIXEL_GREEN(pixel,rgbinfo) \
	(((pixel) >> (rgbinfo).g_offset) << (rgbinfo).g_limit)
#define  RGB_TO_PIXEL(r,g,b,rgbinfo) \
	((((r) >> (rgbinfo).r_limit) << (rgbinfo).r_offset) | \
	 (((g) >> (rgbinfo).g_limit) << (rgbinfo).g_offset) | \
	 (((b) >> (rgbinfo).b_limit) << (rgbinfo).b_offset))

typedef int  XIM ;		/* dummy */
typedef int  XIC ;		/* dummy */
typedef int  XIMStyle ;		/* dummy */

typedef int  XID ;		/* dummy */
typedef int  Window ;		/* dummy */
typedef int  Drawable ;		/* dummy */

typedef struct
{
	unsigned char *  image ;
	unsigned int  width ;
	unsigned int  height ;

} *  Pixmap ;

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
	unsigned int  ch ;

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
	char *  file ;

	int32_t  format ;

	int32_t  num_of_glyphs ;
	unsigned char *  glyphs ;
	int32_t *  glyph_offsets ;
	int32_t  glyph_width_bytes ;
	int  glyphs_same_endian ;

	int16_t  min_char_or_byte2 ;
	int16_t  max_char_or_byte2 ;
	int16_t  min_byte1 ;
	int16_t  max_byte1 ;
	int16_t *  glyph_indeces ;

	unsigned char  width ;
	unsigned char  height ;
	unsigned char  ascent ;

	unsigned int  ref_count ;

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

#define XK_Super_L	0xfffe	/* dummy */
#define XK_Super_R	0xfffd	/* dummy */
#define XK_Hyper_L	0xfffc	/* dummy */
#define XK_Hyper_R	0xfffb	/* dummy */
#define XK_BackSpace	0x08
#define XK_Tab		0x09
#define XK_Clear	0xfffa	/* dummy */
#define XK_Linefeed	0x0a	/* dummy */
#define XK_Return	0x0a	/* dummy */

#define XK_Shift_L	0xfff9	/* dummy */
#define XK_Control_L	0xfff8	/* dummy */
#define XK_Alt_L	0xfff7	/* dummy */
#define XK_Shift_R	0xfff6	/* dummy */
#define XK_Control_R	0xfff5	/* dummy */
#define XK_Alt_R	0xfff4	/* dummy */

#define XK_Meta_L	0xfff3	/* dummy */
#define XK_Meta_R	0xfff2	/* dummy */

#define XK_Pause	0xfff1	/* dummy */
#define XK_Shift_Lock	0xfff0	/* dummy */
#define XK_Caps_Lock	0xffef	/* dummy */
#define XK_Escape	0x1b
#define XK_Prior	0xffee	/* dummy */
#define XK_Next		0xffed	/* dummy */
#define XK_End		0xffec	/* dummy */
#define XK_Home		0xffeb	/* dummy */
#define XK_Left		0xffea	/* dummy */
#define XK_Up		0xffe9	/* dummy */
#define XK_Right	0xffe8	/* dummy */
#define XK_Down		0xffe7	/* dummy */
#define XK_Select	0xffe6	/* dummy */
#define XK_Print	0xffe5	/* dummy */
#define XK_Execute	0xffe4	/* dummy */
#define XK_Insert	0xffe3	/* dummy */
#define XK_Delete	0xffe2	/* dummy */
#define XK_Help		0xffe1	/* dummy */
#define XK_F1		0xffe0	/* dummy */
#define XK_F2		0xffdf	/* dummy */
#define XK_F3		0xffde	/* dummy */
#define XK_F4		0xffdd	/* dummy */
#define XK_F5		0xffdc	/* dummy */
#define XK_F6		0xffdb	/* dummy */
#define XK_F7		0xffda	/* dummy */
#define XK_F8		0xffd9	/* dummy */
#define XK_F9		0xffd8	/* dummy */
#define XK_F10		0xffd7	/* dummy */
#define XK_F11		0xffd6	/* dummy */
#define XK_F12		0xffd5	/* dummy */
#define XK_F13		0xffd4	/* dummy */
#define XK_F14		0xffd3	/* dummy */
#define XK_F15		0xffd2	/* dummy */
#define XK_F16		0xffd1	/* dummy */
#define XK_F17		0xffd0	/* dummy */
#define XK_F18		0xffcf	/* dummy */
#define XK_F19		0xffce	/* dummy */
#define XK_F20		0xffcd	/* dummy */
#define XK_F21		0xffcc	/* dummy */
#define XK_F22		0xffcb	/* dummy */
#define XK_F23		0xffca	/* dummy */
#define XK_F24		0xffc9	/* dummy */
#define XK_FMAX		0xffc8	/* dummy */
#define XK_Num_Lock	0xffc7	/* dummy */
#define XK_Scroll_Lock	0xffc6	/* dummy */
#define XK_Find		0xffc5	/* dummy */
#define XK_Menu		0xffc4	/* dummy */
#define XK_Begin	0xffc3	/* dummy */

#define XK_KP_Prior	0xffc2	/* dummy */
#define XK_KP_Next	0xffc1	/* dummy */
#define XK_KP_End	0xffc0	/* dummy */
#define XK_KP_Home	0xffbf	/* dummy */
#define XK_KP_Left	0xffbe	/* dummy */
#define XK_KP_Up	0xffbd	/* dummy */
#define XK_KP_Right	0xffbc	/* dummy */
#define XK_KP_Down	0xffbb	/* dummy */
#define XK_KP_Insert	0xffba	/* dummy */
#define XK_KP_Delete	0xffb9	/* dummy */
#define XK_KP_F1	0xffb8	/* dummy */
#define XK_KP_F2	0xffb7	/* dummy */
#define XK_KP_F3	0xffb6	/* dummy */
#define XK_KP_F4	0xffb5	/* dummy */
#define XK_KP_Begin	0xffb4	/* dummy */
#define XK_KP_Multiply	0xffb3	/* dummy */
#define XK_KP_Add	0xffb2	/* dummy */
#define XK_KP_Separator	0xffb1	/* dummy */
#define XK_KP_Subtract	0xffb0	/* dummy */
#define XK_KP_Decimal	0xffaf	/* dummy */
#define XK_KP_Divide	0xffae	/* dummy */
#define XK_KP_0		0xffad	/* dummy */
#define XK_KP_1		0xffac	/* dummy */
#define XK_KP_2		0xffab	/* dummy */
#define XK_KP_3		0xffaa	/* dummy */
#define XK_KP_4		0xffa9	/* dummy */
#define XK_KP_5		0xffa8	/* dummy */
#define XK_KP_6		0xffa7	/* dummy */
#define XK_KP_7		0xffa6	/* dummy */
#define XK_KP_8		0xffa5	/* dummy */
#define XK_KP_9		0xffa4	/* dummy */

#define XK_ISO_Level3_Lock	0xffa3	/* dummy */
#define XK_u	0xffa2	/* dummy */
#define XK_d	0xffa1	/* dummy */
#define XK_k	0xffa0	/* dummy */
#define XK_j	0xff9f	/* dummy */


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
#define IsKeypadKey(ksym)	(0)

#define BlackPixel(disp,screen)	 (0)
#define WhitePixel(disp,screen)	 (-1)

/* Same as definition in X11/cursorfont.h */
#define XC_xterm 152

/* Same as definition in X11/Xutil.h */
#define NoValue         0x0000
#define XValue          0x0001
#define YValue          0x0002
#define WidthValue      0x0004
#define HeightValue     0x0008
#define AllValues       0x000F
#define XNegative       0x0010
#define YNegative       0x0020


#define  XC_sb_v_double_arrow  0
#define  XC_left_ptr  0


int  XParseGeometry( char *  str , int *  x , int *  y ,
	unsigned int  *  width , unsigned int  *  height) ;

KeySym  XStringToKeysym( char *  str) ;


#endif
