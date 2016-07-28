/*
 *	$Id$
 */

#ifndef  ___X_H__
#define  ___X_H__


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


/* === Platform dependent options === */

#define  X_COLOR_HAS_RGB
#define  SUPPORT_TRUE_TRANSPARENT_BG
#undef  TYPE_XCORE_SCALABLE
#undef  MANAGE_WINDOWS_BY_MYSELF
#undef  INLINE_PICTURE_MOVABLE_BETWEEN_DISPLAYS
#define  SUPPORT_POINT_SIZE_FONT


#endif
