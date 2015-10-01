/*
 *	$Id$
 */

#include  "../x_gc.h"

#include  <kiklib/kik_mem.h>
#include  <stdio.h>


/* --- global functions --- */

x_gc_t *
x_gc_new(
	Display *  display ,
	Drawable  drawable
	)
{
	x_gc_t *  gc ;

	if( ( gc = calloc( 1 , sizeof(x_gc_t))))
	{
		/* XXX dummy */
		gc->gc = 1 ;
	}

	return  gc ;
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
