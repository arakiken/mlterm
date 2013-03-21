/*
 *	$Id$
 */

#ifndef  __X_H__
#define  __X_H__


#if  defined(USE_WIN32GUI)

#include  "win32/x.h"

#elif  defined(USE_FRAMEBUFFER)

#include  "fb/x.h"

#else	/* USE_WIN32GUI/USE_FRAMEBUFFER */

/* This must be included ahead of Xft.h on XFree86-4.0.x or before. */
#include  <X11/Xlib.h>
#include  <X11/keysym.h>	/* XK_xxx */
#include  <X11/Xatom.h>		/* XA_xxx */
#include  <X11/Xutil.h>		/* IsKeypadKey */
#include  <X11/cursorfont.h>	/* for cursor shape */

typedef Pixmap PixmapMask ;

#ifdef  XK_F21
#define  XK_FMAX  XK_F35
#else
#define  XK_FMAX  XK_F20
#endif

#endif	/* USE_WIN32GUI/USE_FRAMEBUFFER */

/*
 * Xlib utility definitions.
 */

#define  ModMask  (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)

#ifndef  Button6
#define  Button6  6
#endif
#ifndef  Button6Mask
#define  Button6Mask  (Button5Mask << 1)
#endif
#ifndef  Button7
#define  Button7  7
#endif
#ifndef  Button7Mask
#define  Button7Mask  (Button5Mask << 2)
#endif
#define  ButtonMask \
	(Button1Mask | Button2Mask | Button3Mask | Button4Mask | Button5Mask | \
	 Button6Mask | Button7Mask)


#endif
