/*
 *	$Id$
 */

#include  "../x_gc.h"

#include  <kiklib/kik_mem.h>	/* malloc */

#include  "../x_color.h"

#define  ARGB_TO_RGB(pixel)  ((pixel) & 0x00ffffff)


/* --- global functions --- */

x_gc_t *
x_gc_new(
	Display *  display ,
	Drawable  drawable
	)
{
	x_gc_t *  gc ;

	if( ( gc = calloc( 1 , sizeof( x_gc_t))) == NULL)
	{
		return  NULL ;
	}

	gc->display = display ;

	/* Default value of GC. */
	gc->fg_color = RGB(0,0,0) ;
	gc->bg_color = RGB(0xff,0xff,0xff) ;

	return  gc ;
}

int
x_gc_delete(
	x_gc_t *  gc
	)
{
	free( gc) ;

	return  1 ;
}

int
x_set_gc(
	x_gc_t *  gc,
	GC  _gc
	)
{
	gc->gc = _gc ;

	SetTextAlign( gc->gc, TA_LEFT|TA_BASELINE) ;

	gc->fg_color = RGB(0,0,0) ;	/* black */
#if  0
	/* black is default value */
	SetTextColor( gc->gc, gc->fg_color) ;
#endif

	gc->bg_color = RGB(0xff,0xff,0xff) ;	/* white */
#if  0
	/* white is default value */
	SetBkColor( gc->gc, gc->bg_color) ;
#endif

	gc->fid = None ;
	gc->pen = None ;
	gc->brush = None ;

	return  1 ;
}

int
x_gc_set_fg_color(
	x_gc_t *  gc ,
	u_long  fg_color
	)
{
	if( ARGB_TO_RGB(fg_color) != gc->fg_color)
	{
		SetTextColor( gc->gc,
			(gc->fg_color = ARGB_TO_RGB(fg_color))) ;
	}

	return  1 ;
}

int
x_gc_set_bg_color(
	x_gc_t *  gc ,
	u_long  bg_color
	)
{
	if( ARGB_TO_RGB(bg_color) != gc->bg_color)
	{
		SetBkColor( gc->gc,
			(gc->bg_color = ARGB_TO_RGB(bg_color))) ;
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
		SelectObject( gc->gc, fid) ;
		gc->fid = fid ;
	}

	return  1 ;
}

HPEN
x_gc_set_pen(
	x_gc_t *  gc,
	HPEN  pen
	)
{
	if( gc->pen != pen)
	{
		gc->pen = pen ;
		
		return  SelectObject( gc->gc, pen) ;
	}

	return  None ;
}

HBRUSH
x_gc_set_brush(
	x_gc_t *  gc,
	HBRUSH  brush
	)
{
	if( gc->brush != brush)
	{
		gc->brush = brush ;

		return  SelectObject( gc->gc, brush) ;
	}

	return  None ;
}
