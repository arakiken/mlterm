/*
 *	$Id$
 */

#ifndef  __X_H__
#define  __X_H__


#if  defined(USE_WIN32GUI)

#include  "win32/x.h"

#elif  defined(USE_CONSOLE)

#include  "console/x.h"

#elif  defined(USE_FRAMEBUFFER)

#include  "fb/x.h"

#elif  defined(USE_QUARTZ)

#include  "quartz/x.h"

#else

#include  "xlib/x.h"
#define  USE_XLIB

#endif

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
