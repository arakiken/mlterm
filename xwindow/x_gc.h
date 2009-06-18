/*
 *	$Id$
 */

#ifndef  __X_GC_H__
#define  __X_GC_H__


#include  "x.h"


typedef struct x_gc
{
	Display *  display ;
	GC  gc ;
	u_long  fg_color ;
	u_long  bg_color ;
	Font  fid ;
#ifdef  USE_WIN32API
	HPEN  pen ;
	HBRUSH  brush ;
#endif

} x_gc_t ;


x_gc_t *  x_gc_new( Display *  display) ;

int  x_gc_delete( x_gc_t *  gc) ;

int  x_gc_set_fg_color( x_gc_t *  gc, u_long  fg_color) ;

int  x_gc_set_bg_color( x_gc_t *  gc, u_long  bg_color) ;

int  x_gc_set_fid( x_gc_t *  gc, Font  fid) ;

#ifdef  USE_WIN32API

int  x_set_gc( x_gc_t *  gc, GC  _gc) ;

int  x_gc_set_pen( x_gc_t *  gc, HPEN  pen) ;

int  x_gc_set_brush( x_gc_t *  gc, HBRUSH  brush) ;

#endif


#endif
