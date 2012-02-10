/*
 *	$Id$
 */

#ifndef  __X_GC_H__
#define  __X_GC_H__


#include  <kiklib/kik_types.h>	/* u_int */

#include  "x.h"


typedef struct x_gc
{
	Display *  display ;
	GC  gc ;
	u_long  fg_color ;	/* alpha bits are always 0 in win32. */
	u_long  bg_color ;	/* alpha bits are always 0 in win32. */
	Font  fid ;
#ifdef  USE_WIN32GUI
	HPEN  pen ;
	HBRUSH  brush ;
#endif

} x_gc_t ;


x_gc_t *  x_gc_new( Display *  display , Drawable  drawable) ;

int  x_gc_delete( x_gc_t *  gc) ;

int  x_gc_set_fg_color( x_gc_t *  gc, u_long  fg_color) ;

int  x_gc_set_bg_color( x_gc_t *  gc, u_long  bg_color) ;

int  x_gc_set_fid( x_gc_t *  gc, Font  fid) ;

#ifdef  USE_WIN32GUI

int  x_set_gc( x_gc_t *  gc, GC  _gc) ;

HPEN  x_gc_set_pen( x_gc_t *  gc, HPEN  pen) ;

HBRUSH  x_gc_set_brush( x_gc_t *  gc, HBRUSH  brush) ;

#endif


#endif
