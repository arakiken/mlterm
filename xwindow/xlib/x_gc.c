/*
 *	$Id$
 */

#include  "../x_gc.h"

#include  <kiklib/kik_mem.h>	/* malloc */

#include  "../x_color.h"


/* --- global functions --- */

x_gc_t *
x_gc_new(
	Display *  display ,
	Drawable  drawable
	)
{
	x_gc_t *  gc ;
	XGCValues  gc_value ;

	if( ( gc = calloc( 1 , sizeof( x_gc_t))) == NULL)
	{
		return  NULL ;
	}

	gc->display = display ;

	if( drawable)
	{
		/* Default value of GC. */
		gc->fg_color = 0xff000000 ;
		gc->bg_color = 0xffffffff ;

		/* Overwriting default value (1) of backgrond and foreground colors. */
		gc_value.foreground = gc->fg_color ;
		gc_value.background = gc->bg_color ;
		gc_value.graphics_exposures = True ;
		gc->gc = XCreateGC( gc->display , drawable ,
				GCForeground | GCBackground | GCGraphicsExposures , &gc_value) ;
	}
	else
	{
		gc->gc = DefaultGC( display , DefaultScreen( display)) ;
		XGetGCValues( display , gc->gc , GCForeground | GCBackground , &gc_value) ;
		gc->fg_color = gc_value.foreground ;
		gc->bg_color = gc_value.background ;
	}

	return  gc ;
}

int
x_gc_delete(
	x_gc_t *  gc
	)
{
	if( ( gc->gc != DefaultGC( gc->display , DefaultScreen(gc->display))))
	{
		XFreeGC( gc->display , gc->gc) ;
	}

	free( gc) ;

	return  1 ;
}

int
x_gc_set_fg_color(
	x_gc_t *  gc ,
	u_long  fg_color
	)
{
	if( fg_color != gc->fg_color)
	{
		XSetForeground( gc->display , gc->gc , fg_color) ;
		gc->fg_color = fg_color ;
	}

	return  1 ;
}

int
x_gc_set_bg_color(
	x_gc_t *  gc ,
	u_long  bg_color
	)
{
	if( bg_color != gc->bg_color)
	{
		XSetBackground( gc->display , gc->gc , bg_color) ;
		gc->bg_color = bg_color ;
	}

	return  1 ;
}

int
x_gc_set_fid(
	x_gc_t *  gc,
	Font  fid
	)
{
	if( gc->fid != fid)
	{
		XSetFont( gc->display , gc->gc , fid) ;
		gc->fid = fid ;
	}

	return  1 ;
}
