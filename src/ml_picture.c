/*
 *	$Id$
 */

#include  "ml_picture.h"

#include  <X11/Xatom.h>			/* XA_PIXMAP */
#include  <kiklib/kik_debug.h>

#include  "ml_window.h"
#include  "ml_picture_dep.h"


#if  1
#define  PSEUDO_TRANSPARENT
#endif


/* --- global functions --- */

int
ml_picture_display_opened(
	Display *  display
	)
{
	return  ml_picdep_display_opened( display) ;
}

int
ml_picture_display_closed(
	Display *  display
	)
{
	return  ml_picdep_display_closed( display) ;
}

int
ml_picture_init(
	ml_picture_t *  pic ,
	ml_window_t *  win ,
	ml_picture_modifier_t *  mod
	)
{
	pic->pixmap = 0 ;
	pic->win = win ;
	pic->mod = mod ;

	return  1 ;
}

int
ml_picture_final(
	ml_picture_t *  pic
	)
{
	if( pic->pixmap)
	{
		XFreePixmap( pic->win->display , pic->pixmap) ;
	}

	return  1 ;
}

int
ml_picture_load_file(
	ml_picture_t *  pic ,
	char *  file_path
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}

	if( ( pic->pixmap = ml_picdep_load_file( pic->win , file_path , pic->mod)) == None)
	{
		return  0 ;
	}
	
	return  1 ;
}

int
ml_root_pixmap_available(
	Display *  display
	)
{
	return  ml_picdep_root_pixmap_available( display) ;
}
	
int
ml_picture_load_background(
	ml_picture_t *  pic
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}

	if( ( pic->pixmap = ml_picdep_load_background( pic->win , pic->mod)) == None)
	{
		return  0 ;
	}

	return  1 ;
}
