/*
 *	$Id$
 */

#ifndef  ___X_H__
#define  ___X_H__


#undef  _WIN32_WINNT
#define  _WIN32_WINNT  0x0500	/* for WS_EX_LAYERED , LWA_XXX , SetLayeredWindowAttributes */
#define  _WINSOCK2_H		/* Don't include winsock2.h */
#include  <windows.h>
#include  <imm.h>


/* for msys-1.0 dvlpr */
#ifndef  WM_IME_CHAR
#define  WM_IME_CHAR 0x286
#endif


typedef struct
{
	HINSTANCE  hinst ;
	int  fd ;

} Display ;

typedef int  XIM ;		/* dummy */
typedef HIMC  XIC ;
typedef int  XIMStyle ;		/* dummy */

typedef HANDLE XID ;
typedef HANDLE Window ;
typedef HDC Drawable ;
typedef HDC Pixmap ;
typedef HDC GC ;
typedef HFONT Font ;
typedef HCURSOR Cursor ;
typedef WORD KeyCode ;	/* Same as type of wparam */
typedef WORD KeySym ;	/* Same as type of wparam */

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
	Window  window ;
  	UINT msg ;
  	WPARAM wparam ;
  	LPARAM lparam ;

} XEvent ;

typedef struct
{
	unsigned int  state ;
	WORD  ch ;	/* unsigned short(16bit) defined in windef.h */

} XKeyEvent ;

typedef unsigned long  Time ;	/* Same as definition in X11/X.h */
typedef unsigned long  Atom ;	/* Same as definition in X11/X.h */

typedef struct
{
	Time  time ;
	int  x ;
	int  y ;
	unsigned int  state ;
	unsigned int  button ;

} XButtonEvent ;

typedef struct
{
	Time  time ;
	int  x ;
	int  y ;
	unsigned int  state ;

} XMotionEvent ;

typedef int XSelectionRequestEvent ;	/* dummy */

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
#define XK_BackSpace	VK_BACK
#define XK_Tab		VK_TAB
#define XK_Clear	VK_CLEAR
#define XK_Linefeed	0xfffa	/* dummy */
#define XK_Return	VK_RETURN

/*
 * VK_L... and VK_R... are available by GetKeyState(VK_SHIFT, VK_CONTROL or VK_MENU),
 * but mlerm doesn't support fow now.
 */
#if  1
#define XK_Shift_L	VK_SHIFT
#define XK_Control_L	VK_CONTROL
#define XK_Alt_L	VK_MENU
#else
#define XK_Shift_L	VK_LSHIFT
#define XK_Control_L	VK_LCONTROL
#define XK_Alt_L	VK_LMENU
#endif
#define XK_Shift_R	VK_RSHIFT
#define XK_Control_R	VK_RCONTROL
#define XK_Alt_R	VK_RMENU

#define XK_Meta_L	0xfff9	/* dummy */
#define XK_Meta_R	0xfff8	/* dummy */

#define XK_Pause	VK_PAUSE
#define XK_Shift_Lock	0xfff7	/* dummy */
#define XK_Caps_Lock	VK_CAPITAL
#define XK_Escape	VK_ESCAPE
/* #define XXX	VK_SPACE */
#define XK_Prior	VK_PRIOR
#define XK_Next		VK_NEXT
#define XK_End		VK_END
#define XK_Home		VK_HOME
#define XK_Left		VK_LEFT
#define XK_Up		VK_UP
#define XK_Right	VK_RIGHT
#define XK_Down		VK_DOWN
#define XK_Select	VK_SELECT
#define XK_Print	VK_PRINT
#define XK_Execute	VK_EXECUTE
/* #define XXX	VK_SNAPSHOT  ... PrintScreen key */
#define XK_Insert	VK_INSERT
#define XK_Delete	VK_DELETE
#define XK_Help		VK_HELP
#define XK_F1		VK_F1
#define XK_F2		VK_F2
#define XK_F3		VK_F3
#define XK_F4		VK_F4
#define XK_F5		VK_F5
#define XK_F6		VK_F6
#define XK_F7		VK_F7
#define XK_F8		VK_F8
#define XK_F9		VK_F9
#define XK_F10		VK_F10
#define XK_F11		VK_F11
#define XK_F12		VK_F12
#define XK_F13		VK_F13
#define XK_F14		VK_F14
#define XK_F15		VK_F15
#define XK_F16		VK_F16
#define XK_F17		VK_F17
#define XK_F18		VK_F18
#define XK_F19		VK_F19
#define XK_F20		VK_F20
#define XK_F21		VK_F21
#define XK_F22		VK_F22
#define XK_F23		VK_F23
#define XK_F24		VK_F24
#define XK_FMAX		XK_F24
#define XK_Num_Lock	VK_NUMLOCK
#define XK_Scroll_Lock	VK_SCROLL
#define XK_Find		0xffeb	/* dummy */
#define XK_Menu		0xffea	/* dummy */
#define XK_Begin	0xffe9	/* dummy */
/* #define XXX	VK_PLAY */
/* #define XXX	VK_ZOOM */

#define XK_KP_Prior	0xffe8	/* dummy */
#define XK_KP_Next	0xffe7	/* dummy */
#define XK_KP_End	0xffe6	/* dummy */
#define XK_KP_Home	0xffe5	/* dummy */
#define XK_KP_Left	0xffe4	/* dummy */
#define XK_KP_Up	0xffe3	/* dummy */
#define XK_KP_Right	0xffe2	/* dummy */
#define XK_KP_Down	0xffe1	/* dummy */
#define XK_KP_Insert	0xffe0	/* dummy */
#define XK_KP_Delete	0xffdf	/* dummy */
#define XK_KP_F1	0xffde	/* dummy */
#define XK_KP_F2	0xffdd	/* dummy */
#define XK_KP_F3	0xffdc	/* dummy */
#define XK_KP_F4	0xffdb	/* dummy */
#define XK_KP_Begin	0xffda	/* dummy */
#define XK_KP_Multiply	VK_MULTIPLY
#define XK_KP_Add	VK_ADD
#define XK_KP_Separator	VK_SEPARATOR
#define XK_KP_Subtract	VK_SUBTRACT
#define XK_KP_Decimal	VK_DECIMAL
#define XK_KP_Divide	VK_DIVIDE
#define XK_KP_0		VK_NUMPAD0
#define XK_KP_1		VK_NUMPAD1
#define XK_KP_2		VK_NUMPAD2
#define XK_KP_3		VK_NUMPAD3
#define XK_KP_4		VK_NUMPAD4
#define XK_KP_5		VK_NUMPAD5
#define XK_KP_6		VK_NUMPAD6
#define XK_KP_7		VK_NUMPAD7
#define XK_KP_8		VK_NUMPAD8
#define XK_KP_9		VK_NUMPAD9

#define XK_ISO_Level3_Lock	0xffd9	/* dummy */
#define XK_u	0xffd8	/* dummy */
#define XK_d	0xffd7	/* dummy */
#define XK_k	0xffd6	/* dummy */
#define XK_j	0xffd5	/* dummy */

/* For msys-dtk */
#ifndef  VK_OEM_2
#define  VK_OEM_2  0xbf
#endif
#ifndef  VK_OEM_7
#define  VK_OEM_7  0xde
#endif
#ifndef  VK_OEM_102
#define  VK_OEM_102  0xe2
#endif


/* XPoint(short x, short y) in Xlib. POINT(long x, long y) in win32. */
#define XPoint  POINT

/* XXX dummy */
#define XKeysymToKeycode(disp,ks)  (ks)
#define XKeycodeToKeysym(disp,kc,i)  (kc)
#define XKeysymToString(ks)	""
#define DisplayString(disp)	":0.0"
#define DefaultScreen(disp)	(0)
/* VK_NUMPAD0 = 0x60, VK_DIVIDE = 0x6f */
#define IsKeypadKey(ksym)	(VK_NUMPAD0 <= (ksym) && (ksym) <= VK_DIVIDE)

#define BlackPixel(disp,screen)	(0xff000000 | RGB(0,0,0))
#define WhitePixel(disp,screen)	(0xff000000 | RGB(0xff,0xff,0xff))

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

/* tchar.h doesn't exist in /usr/include/w32api in cygwin. */
#ifndef  _T
#if  defined(_UNICODE) || defined(UNICODE)
#define  _T(a) L##a
#else
#define  _T(a) a
#endif
#endif	/* _T */

#if  1
/* Use xxxxW functions for RegisterClass etc. */
#define  UTF16_IME_CHAR
#endif

#ifdef  UTF16_IME_CHAR
 #undef  GetMessage
 #define  GetMessage(a,b,c,d) GetMessageW(a,b,c,d)
 #undef  DispatchMessage
 #define  DispatchMessage(a) DispatchMessageW(a)
 #undef  PeekMessage
 #define  PeekMessage(a,b,c,d,e) PeekMessageW(a,b,c,d,e)
 #undef  DefWindowProc
 #define  DefWindowProc(a,b,c,d) DefWindowProcW(a,b,c,d)
 #undef  WNDCLASS
 #define  WNDCLASS WNDCLASSW
 #undef  RegisterClass
 #define  RegisterClass(a) RegisterClassW(a)
 #undef  CreateWindowEx
 #define  CreateWindowEx(a,b,c,d,e,f,g,h,i,j,k,l) CreateWindowExW(a,b,c,d,e,f,g,h,i,j,k,l)
 #define  __(a) L##a
#else /* UTF16_IME_CHAR */
 #define  __(a) _T(a)
#endif	/* UTF16_IME_CHAR */


#define  XC_sb_v_double_arrow  0
#define  XC_left_ptr  0


int  XParseGeometry( char *  str , int *  x , int *  y ,
	unsigned int  *  width , unsigned int  *  height) ;

KeySym  XStringToKeysym( char *  str) ;


#endif
