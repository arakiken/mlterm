/*
 *	$Id$
 */

#include  "ml_picture.h"

#include  <stdlib.h>			/* abs */
#include  <X11/Xatom.h>			/* XA_PIXMAP */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_unistd.h>		/* kik_usleep */

#ifdef  USE_IMLIB
#include  <Imlib.h>
#endif

#include  "ml_window.h"


#if  1
#define  PSEUDO_TRANSPARENT
#endif


#ifdef  USE_IMLIB

typedef struct  imlib
{
	Display *  display ;
	ImlibData *  imlib ;

} imlib_t ;

#endif


/* --- static variables --- */

#ifdef  USE_IMLIB

static imlib_t *  imlibs ;
static u_int  num_of_imlibs ;

#endif


/* --- static functions --- */

static int
get_visible_window_geometry(
	ml_window_t *  win ,
	int *  x ,		/* x relative to parent window */
	int *  y ,		/* y relative to parent window */
	int *  my_x ,		/* x relative to my window */
	int *  my_y ,		/* y relative to my window */
	u_int *  width ,
	u_int *  height
	)
{
	Window  child ;
	
	XTranslateCoordinates( win->display , win->my_window ,
		DefaultRootWindow( win->display) , 0 , 0 ,
		x , y , &child) ;

	if( *x < 0)
	{
		if( ACTUAL_WIDTH(win) <= abs(*x))
		{
			/* no visible window */
			
			return  0 ;
		}

		*my_x = abs(*x) ;
		
		*width = ACTUAL_WIDTH(win) - abs(*x) ;
		*x = 0 ;
	}
	else
	{
		*my_x = 0 ;
		*width = ACTUAL_WIDTH(win) ;
	}

	if( *y < 0)
	{
		if( ACTUAL_HEIGHT(win) <= abs(*y))
		{
			/* no visible window */
			
			return  0 ;
		}

		*my_y = abs(*y) ;

		*height = ACTUAL_HEIGHT(win) - abs(*y) ;
		*y = 0 ;
	}
	else
	{
		*my_y = 0 ;
		*height = ACTUAL_HEIGHT(win) ;
	}

	if( *x + *width > DisplayWidth( win->display , win->screen))
	{
		*width = DisplayWidth( win->display , win->screen) - *x ;
	}

	if( *y + *height > DisplayHeight( win->display , win->screen))
	{
		*height = DisplayHeight( win->display , win->screen) - *y ;
	}

	return  1 ;
}

#ifdef  USE_IMLIB

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
	if( pic_mod->brightness != 100)
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

#ifdef  PSEUDO_TRANSPARENT

static Pixmap
get_background_picture(
	ml_window_t *  win ,
	ml_picture_modifier_t *  pic_mod
	)
{
	int  x ;
	int  y ;
	int  pix_x ;
	int  pix_y ;
	u_int  width ;
	u_int  height ;
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

	if( ! get_visible_window_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
	{
		return  None ;
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
					
				XFree( prop) ;
				
				if( img)
				{
					goto  found ;
				}
			}
			else if( prop)
			{
				XFree( prop) ;
			}
		}
	}

	attr.background_pixmap = ParentRelative ;
	attr.backing_store = Always ;
	attr.event_mask = ExposureMask ;
	attr.override_redirect = True ;
	
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
		kik_usleep( 50000) ;

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

static Pixmap
get_background_picture(
	ml_window_t *  win ,
	ml_picture_modifier_t *  pic_mod
	)
{
	int  x ;
	int  y ;
	int  pix_x ;
	int  pix_y ;
	u_int  width ;
	u_int  height ;
	Pixmap  pixmap ;
	ImlibData *  imlib ;
	ImlibImage *  img ;
	ml_window_t *  root ;
	int  counter ;
	XEvent *  queued_events ;
	u_int  num_of_queued ;
	XEvent  event ;

	if( ! ( imlib = get_imlib( win->display)))
	{
		return  None ;
	}

	root = ml_get_root_window(win) ;
	
	if( ! get_visible_window_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
	{
		return  None ;
	}

	/* XXX already queued StructureNotifyMask events are backuped */
	queued_events = NULL ;
	num_of_queued = 0 ;
	while( XCheckWindowEvent( root->display , root->my_window , StructureNotifyMask , &event))
	{
		void *  p ;
		
		if( ( p = realloc( queued_events , sizeof( XEvent) * ( num_of_queued + 1))) == NULL)
		{
			break ;
		}

		queued_events = p ;
		queued_events[num_of_queued ++] = event ;
	}

	ml_window_remove_event_mask( root , StructureNotifyMask /* | SubstructureNotifyMask */) ;

	/*
	 * StructureNotifyMask events are ignored from here.
	 */
	
	ml_window_unmap( root) ;

	/* XXX waiting for all exposed windows actually redrawn. */
	XSync( root->display , False) ;
	kik_usleep( 25000) ;
	
	img = Imlib_create_image_from_drawable( imlib , DefaultRootWindow( win->display) ,
		AllPlanes , x , y , width , height) ;

	/* XXX ingoring all queued Expose events */
	while( XCheckWindowEvent( root->display , root->my_window , ExposureMask, &event)) ;
	
	ml_window_map( root) ;

	/*
	 * StructureNoitfyMask events are ignored till here.
	 */
	
	ml_window_add_event_mask( root , StructureNotifyMask /* | SubstructureNotifyMask */) ;

	/* XXX waiting for all StructureNotifyMask events responded */
	XSync( root->display , False) ;
	kik_usleep( 25000) ;

	/* XXX ignoreing all queued StructureNotifyMask events */
	while( XCheckWindowEvent( root->display , root->my_window , StructureNotifyMask , &event)) ;

	/* XXX restoring all backuped StructureNotifyMask events */
	for( counter = 0 ; counter < num_of_queued ; counter ++)
	{
		XPutBackEvent( root->display , &queued_events[counter]) ;
	}
	free( queued_events) ;
	
	/* XXX waiting for root window actually mapped. */
	counter = 0 ;
	while( ! XCheckWindowEvent( root->display , root->my_window , ExposureMask, &event))
	{
		kik_usleep( 50000) ;

		if( ++ counter >= 10)
		{
			return  None ;
		}
	}
	XPutBackEvent( root->display , &event) ;

	if( ! img)
	{
		return  None ;
	}

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

#endif

#else

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
	Atom id ;
	int  x ;
	int  y ;
	int  _x ;
	int  _y ;
	u_int  width ;
	u_int  height ;
	Pixmap  pixmap ;
	Atom act_type ;
	int  act_format ;
	u_long  nitems ;
	u_long  bytes_after ;
	u_char *  prop ;

	if( pic_mod && pic_mod->brightness != 100)
	{
		/*
		 * XXX
		 * image is never modified without Imlib.
		 */
		
		return  None ;
	}
	
	if( ( id = XInternAtom( win->display , "_XROOTPMAP_ID" , True)) == None)
	{
		return  None ;
	}
	
	if( ! get_visible_window_geometry( win , &x , &y , &_x , &_y , &width , &height))
	{
		return  None ;
	}
	
	if( XGetWindowProperty( win->display , DefaultRootWindow(win->display) , id , 0 , 1 , False ,
		XA_PIXMAP , &act_type , &act_format , &nitems , &bytes_after , &prop) != Success ||
		prop == NULL)
	{
		return  None ;
	}
	
	if( ! *prop)
	{
		XFree( prop) ;

		return  None ;
	}
	
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " root pixmap %d found.\n" , *prop) ;
#endif

	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	XCopyArea( win->display , (*(Drawable*)prop) , pixmap , win->gc ,
		x , y , width , height , _x , _y) ;

	return  pixmap ;
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
ml_root_pixmap_available(
	Display *  display
	)
{
#ifdef  PSEUDO_TRANSPARENT
	if( XInternAtom( display , "_XROOTPMAP_ID" , True))
	{
		return  1 ;
	}

	return  0 ;
#else
	return  1 ;
#endif
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
