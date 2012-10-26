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


#endif
