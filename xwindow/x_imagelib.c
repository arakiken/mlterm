/*
 *	$Id$
 */

#if   defined(USE_IMLIB)

#include  "x_imagelib_imlib.c"

#elif defined(USE_GDK_PIXBUF)

#include  "x_imagelib_gdk.c"

#else

#include  "x_imagelib_none.c"

#endif

