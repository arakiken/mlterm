/*
 *	$Id$
 */

#include  "ml_picture.h"

#include  <kiklib/kik_debug.h>

#ifdef  USE_IMLIB
#include  <Imlib.h>
#endif

#include  "ml_window.h"


/* --- static variables --- */

#ifdef  USE_IMLIB
/* shared by all picture managers */
static ImlibData *  imlib ;
#endif


/* --- static functions --- */

#ifdef  USE_IMLIB

static int
init_imlib(
	ml_window_t *  win
	)
{
	if( imlib == NULL)
	{
		if( ( imlib = Imlib_init( win->display)) == NULL)
		{
		#ifdef  DEBUG
			kik_warn_printf( "failed to init imlib.\n") ;
		#endif

			return  0 ;
		}
	}

	return  1 ;
}

static int
modify_image(
	ImlibImage *  img ,
	ml_picture_modifier_t *  pic_mod
	)
{
	if( pic_mod->brightness < 100)
	{
		ImlibColorModifier  mod ;

		Imlib_get_image_modifier( imlib , img , &mod) ;
		mod.brightness = ( mod.brightness * pic_mod->brightness) / 100 ;
		Imlib_set_image_modifier( imlib , img , &mod) ;
	}
	
	return  1 ;
}

static Pixmap
load_picture(
	ml_window_t *  win ,
	char *  file_path ,
	ml_picture_modifier_t *  pic_mod
	)
{
	ImlibImage *  img ;
	Pixmap  pixmap ;

	if( ! init_imlib( win))
	{
		return  None ;
	}
	
	if( ( img = Imlib_load_image( imlib , file_path)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " Imlib_load_image() failed.\n") ;
	#endif
	
		return  None ;
	}

	if( pic_mod)
	{
		modify_image( img , pic_mod) ;
	}
	
	pixmap = XCreatePixmap( win->display , win->my_window ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
		DefaultDepth( win->display , win->screen)) ;

	Imlib_paste_image( imlib , img , pixmap , 0 , 0 ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	Imlib_kill_image( imlib , img) ;

	return  pixmap ;
}

static Pixmap
get_background_picture(
	ml_window_t *  win ,
	ml_picture_modifier_t *  pic_mod
	)
{
	int  x ;
	int  y ;
	Window  child ;
	Window  src ;
	Pixmap  pixmap ;
	XSetWindowAttributes attr ;
	XEvent event ;
	int counter ;
	ImlibImage *  img ;
	
	if( ! init_imlib( win))
	{
		return  None ;
	}
	
	XTranslateCoordinates( win->display , win->my_window ,
		DefaultRootWindow( win->display) , 0 , 0 ,
		&x , &y , &child) ;
	
	attr.background_pixmap = ParentRelative ;
	attr.backing_store = Always ;
	attr.event_mask = ExposureMask ;
	attr.override_redirect = True ;
	
	src = XCreateWindow(
			win->display , DefaultRootWindow( win->display) ,
			x , y , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) , 0 ,
			CopyFromParent, CopyFromParent, CopyFromParent ,
			CWBackPixmap|CWBackingStore|CWOverrideRedirect|CWEventMask ,
			&attr) ;
	
	XGrabServer( win->display) ;
	XMapRaised( win->display , src) ;
	XSync( win->display , False) ;

	counter = 0 ;
	while( ! XCheckWindowEvent( win->display , src , ExposureMask, &event))
	{
		sleep(1) ;

		if( ++ counter >= 10)
		{
			return  None ;
		}
	}
	
	img = Imlib_create_image_from_drawable( imlib , src , AllPlanes , 0 , 0 ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	XDestroyWindow( win->display , src) ;
	XUngrabServer( win->display) ;

	if( pic_mod)
	{
		modify_image( img , pic_mod) ;
	}
	
	pixmap = XCreatePixmap( win->display , win->my_window ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
		DefaultDepth( win->display , win->screen)) ;

	Imlib_paste_image( imlib , img , pixmap , 0 , 0 , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	Imlib_kill_image( imlib , img) ;

	return  pixmap ;
}

#else

/*
 * only Bitmap picture.
 */
static Pixmap
load_picture(
	ml_window_t *  win ,
	char *  file_path ,
	ml_picture_modifier_t *  pic_mod
	)
{
	return  None ;
}

static Pixmap
get_background_picture(
	ml_window_t *  win ,
	ml_picture_modifier_t *  pic_mod
	)
{
	return  None ;
}

#endif


/* --- global functions --- */

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

	if( ( pic->pixmap = load_picture( pic->win , file_path , pic->mod)) == None)
	{
		return  0 ;
	}
	
	return  1 ;
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

	if( ( pic->pixmap = get_background_picture( pic->win , pic->mod)) == None)
	{
		return  0 ;
	}
	
	return  1 ;
}
