/*
 *	$Id$
 */

#include  "ml_picture.h"

#include  <kiklib/kik_debug.h>

#ifdef  USE_IMLIB
#include  <Imlib.h>
#endif


/* --- static variables --- */

#ifdef  USE_IMLIB
/* shared by all picture managers */
static ImlibData *  imlib ;
#endif


/* --- static functions --- */

#ifdef  USE_IMLIB

static Pixmap
load_picture(
	ml_picture_t *  pic ,
	char *  file_path ,
	u_int  shade_ratio
	)
{
	ImlibImage *  img ;
	ImlibColorModifier mod ;
	Pixmap  pixmap ;

	if( imlib == NULL)
	{
		if( ( imlib = Imlib_init( pic->win->display)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( "failed to init imlib.\n") ;
		#endif

			return  0 ;
		}
	}
	
	if( ( img = Imlib_load_image( imlib , file_path)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " Imlib_load_image() failed.\n") ;
	#endif
	
		return  0 ;
	}

	if( shade_ratio < 100)
	{
		Imlib_get_image_modifier( imlib , img , &mod) ;

		/* usually 256 */
		mod.brightness = (mod.brightness * shade_ratio) / 100 ;

		Imlib_set_image_modifier( imlib , img , &mod) ;
	}
	
	pixmap = XCreatePixmap( pic->win->display , pic->win->my_window ,
		ACTUAL_WIDTH(pic->win) , ACTUAL_HEIGHT(pic->win) ,
		DefaultDepth( pic->win->display , pic->win->screen)) ;

	Imlib_paste_image( imlib , img , pixmap , 0 , 0 ,
		ACTUAL_WIDTH(pic->win) , ACTUAL_HEIGHT(pic->win)) ;

	Imlib_kill_image( imlib , img) ;

	return  pixmap ;
}

#else

/*
 * only Bitmap picture.
 */
static Pixmap
load_picture(
	ml_picture_t *  pic ,
	char *  file_path
	)
{
	return  0 ;
}

#endif


/* --- global functions --- */

int
ml_picture_init(
	ml_picture_t *  pic ,
	ml_window_t *  win
	)
{
	pic->pixmap = 0 ;
	pic->win = win ;

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
ml_picture_load(
	ml_picture_t *  pic ,
	char *  file_path ,
	u_int  shade_ratio
	)
{
	if( pic->pixmap)
	{
		/* already loaded */
		
		return  0 ;
	}

	if( ( pic->pixmap = load_picture( pic , file_path , shade_ratio)) == 0)
	{
		return  0 ;
	}
	
	return  1 ;
}
