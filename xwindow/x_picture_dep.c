/*
 *	$Id$
 */

#if   defined(USE_IMLIB)

#include  "x_picture_imlib.c"

#elif defined(USE_GDK_PIXBUF)

#include  "x_picture_gdk.c"

#else

#include  "x_picture_none.c"

#endif

