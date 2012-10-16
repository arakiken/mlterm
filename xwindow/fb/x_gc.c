/*
 *	$Id$
 */

#include  "../x_gc.h"


/* --- global functions --- */

x_gc_t *
x_gc_new(
	Display *  display ,
	Drawable  drawable
	)
{
	return  NULL ;
}

int
x_gc_delete(
	x_gc_t *  gc
	)
{
	return  1 ;
}

int
x_gc_set_fg_color(
	x_gc_t *  gc ,
	u_long  fg_color
	)
{
	return  1 ;
}

int
x_gc_set_bg_color(
	x_gc_t *  gc ,
	u_long  bg_color
	)
{
	return  1 ;
}

int
x_gc_set_fid(
	x_gc_t *  gc,
	Font  fid
	)
{
	return  1 ;
}
