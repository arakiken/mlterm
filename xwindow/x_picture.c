/*
 *	$Id$
 */

#include  "x_picture.h"

#include  <X11/Xatom.h>			/* XA_PIXMAP */
#include  <kiklib/kik_debug.h>

#include  "x_window.h"
#include  "x_picture_dep.h"


#if  1
#define  PSEUDO_TRANSPARENT
#endif


/* --- global functions --- */

int
x_picture_display_opened(
	Display *  display
	)
{
	return  x_picdep_display_opened( display) ;
}

int
x_picture_display_closed(
	Display *  display
	)
{
	return  x_picdep_display_closed( display) ;
}

int
x_picture_init(
	x_picture_t *  pic ,
	x_window_t *  win ,
	x_picture_modifier_t *  mod
	)
{
	pic->pixmap = 0 ;
	pic->win = win ;
	pic->mod = mod ;

	return  1 ;
}

int
x_picture_final(
	x_picture_t *  pic
	)
{
	if( pic->pixmap)
	{
		XFreePixmap( pic->win->display , pic->pixmap) ;
	}

	return  1 ;
}

int
x_picture_load_file(
	x_picture_t *  pic ,
	char *  file_path
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}

	if( ( pic->pixmap = x_picdep_load_file( pic->win , file_path , pic->mod)) == None)
	{
		return  0 ;
	}
	
	return  1 ;
}

int
x_root_pixmap_available(
	Display *  display
	)
{
	return  x_picdep_root_pixmap_available( display) ;
}
	
int
x_picture_load_background(
	x_picture_t *  pic
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}

	if( ( pic->pixmap = x_picdep_load_background( pic->win , pic->mod)) == None)
	{
		return  0 ;
	}

	return  1 ;
}
