/*
 *	$Id$
 */

#if   defined(USE_IMLIB)

#include  "ml_picture_imlib.c"

#elif defined(USE_GDK_PIXBUF)

#include  "ml_picture_gdk.c"

#else

#include  "ml_picture_none.c"

#endif

