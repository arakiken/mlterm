/*
 *	$Id$
 */

#ifndef  __X_GDIOBJ_POOL_H__
#define  __X_GDIOBJ_POOL_H__


#include  <kiklib/kik_types.h>

#include  "x_win32.h"


int  x_gdiobj_pool_init(void) ;

int  x_gdiobj_pool_final(void) ;

HPEN  x_acquire_pen( u_long  rgb) ;

int  x_release_pen( HPEN  pen) ;

HBRUSH  x_acquire_brush( u_long  rgb) ;

int  x_release_brush( HBRUSH  brush) ;


#endif
