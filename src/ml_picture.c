/*
 *	$Id$
 */

#include  "ml_picture.h"

#include  <stdlib.h>			/* abs */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>		/* realloc/free */

#ifdef  USE_IMLIB
#include  <Imlib.h>
#endif

#include  "ml_window.h"


#ifdef  USE_IMLIB

typedef struct  imlib
{
	Display *  display ;
	ImlibData *  imlib ;

} imlib_t ;


/* --- static variables --- */

static imlib_t *  imlibs ;
static u_int  num_of_imlibs ;


/* --- static functions --- */

static ImlibData *
get_imlib(
	Display *  display
	)
{
	int  counter ;

	for( counter = 0 ; counter < num_of_imlibs ; counter ++)
	{
		if( imlibs[counter].display == display)
		{
			if( imlibs[counter].imlib == NULL &&
				(imlibs[counter].imlib = Imlib_init( display)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( "failed to init imlib.\n") ;
			#endif

				return  None ;
			}

			return  imlibs[counter].imlib ;
		}
	}

	return  NULL ;
}

static int
modify_image(
	ImlibData *  imlib ,
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
	ImlibData *  imlib ;
	ImlibImage *  img ;
	Pixmap  pixmap ;

	if( ! ( imlib = get_imlib( win->display)))
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
		modify_image( imlib , img , pic_mod) ;
	}
	
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
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
	u_int  width ;
	u_int  height ;
	int  pix_x ;
	int  pix_y ;
	Window  child ;
	Window  src ;
	Pixmap  pixmap ;
	XSetWindowAttributes attr ;
	XEvent event ;
	int counter ;
	ImlibData *  imlib ;
	ImlibImage *  img ;
	Atom id ;
	
	if( ! ( imlib = get_imlib( win->display)))
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
	
	if( x < 0)
	{
		if( ACTUAL_WIDTH(win) <= abs(x))
		{
			return  None ;
		}

		pix_x = abs(x) ;
		
		width = ACTUAL_WIDTH(win) - abs(x) ;
		x = 0 ;
	}
	else
	{
		pix_x = 0 ;
		width = ACTUAL_WIDTH(win) ;
	}

	if( y < 0)
	{
		if( ACTUAL_HEIGHT(win) <= abs(y))
		{
			return  None ;
		}

		pix_y = abs(y) ;

		height = ACTUAL_HEIGHT(win) - abs(y) ;
		y = 0 ;
	}
	else
	{
		pix_y = 0 ;
		height = ACTUAL_HEIGHT(win) ;
	}

	if( x + width > DisplayWidth( win->display , win->screen))
	{
		width = DisplayWidth( win->display , win->screen) - x ;
	}

	if( y + height > DisplayHeight( win->display , win->screen))
	{
		height = DisplayHeight( win->display , win->screen) - y ;
	}

	if( ( id = XInternAtom( win->display , "_XROOTPMAP_ID" , True)))
	{
		Atom act_type ;
		int  act_format ;
		u_long  nitems ;
		u_long  bytes_after ;
		u_char *  prop ;
		
		if( XGetWindowProperty( win->display , DefaultRootWindow(win->display) , id , 0 , 1 ,
			False , XA_PIXMAP , &act_type , &act_format , &nitems , &bytes_after , &prop)
			== Success)
		{
			if( prop && *prop)
			{
			#ifdef  __DEBUG
				kik_debug_printf( KIK_DEBUG_TAG " root pixmap %d found.\n" , *prop) ;
			#endif

				img = Imlib_create_image_from_drawable( imlib , *((Drawable *) prop) ,
					AllPlanes , x , y , width , height) ;
					
				XFree(prop) ;
				
				if( img)
				{
					goto  found ;
				}
			}
		}
	}

	src = XCreateWindow( win->display , DefaultRootWindow( win->display) ,
			x , y , width , height , 0 ,
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
			XDestroyWindow( win->display , src) ;
			XUngrabServer( win->display) ;

			return  None ;
		}
	}

	img = Imlib_create_image_from_drawable( imlib , src , AllPlanes , 0 , 0 , width , height) ;

	XDestroyWindow( win->display , src) ;
	XUngrabServer( win->display) ;

	if( ! img)
	{		
		return  None ;
	}

found:
	if( pic_mod)
	{
		modify_image( imlib , img , pic_mod) ;
	}
	
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	Imlib_paste_image( imlib , img , pixmap , pix_x , pix_y , width , height) ;

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

#ifdef  USE_IMLIB

int
ml_picture_display_opened(
	Display *  display
	)
{
	void *  p ;

	if( ( p = realloc( imlibs , sizeof( imlib_t) * (num_of_imlibs + 1))) == NULL)
	{
		return  0 ;
	}

	imlibs = p ;

	imlibs[num_of_imlibs].display = display ;
	imlibs[num_of_imlibs].imlib = NULL ;
	
	num_of_imlibs ++ ;

	return  1 ;
}

int
ml_picture_display_closed(
	Display *  display
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < num_of_imlibs ; counter ++)
	{
		if( imlibs[counter].display == display)
		{
			/*
			 * XXX
			 * How to destroy ImlibData ?
			 * static data of this display are still left in imlib , which
			 * results in unexpected behaviors(maybe core dumps)
			 */

			if( imlibs[counter].imlib)
			{
				Imlib_free_colors( imlibs[counter].imlib) ;
				free( imlibs[counter].imlib) ;
			}
			
			if( -- num_of_imlibs == 0)
			{
				free( imlibs) ;
				imlibs = NULL ;
			}
			else
			{
				imlibs[counter] = imlibs[num_of_imlibs] ;
			}

			return  1 ;
		}
	}

	return  1 ;
}

#else

int
ml_picture_display_opened(
	Display *  display
	)
{
	return  1 ;
}

int
ml_picture_display_closed(
	Display *  display
	)
{
	return  1 ;
}

#endif

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
