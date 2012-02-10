/*
 *	$Id$
 */

#ifndef  __X_H__
#define  __X_H__


#ifdef  USE_WIN32GUI
#include  "win32/x_win32.h"
#else
/* This must be included ahead of Xft.h on XFree86-4.0.x or before. */
#include  <X11/Xlib.h>
#include  <X11/keysym.h>	/* XK_xxx */
#include  <X11/Xatom.h>		/* XA_xxx */
#include  <X11/Xutil.h>		/* IsKeypadKey */
#endif

/*
 * Xlib utility definitions.
 */

#define  ModMask  (Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask)


#endif
