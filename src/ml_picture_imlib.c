/*
 *	$Id$
 */

#include  "ml_picture_dep.h"

#include  <X11/Xatom.h>			/* XA_PIXMAP */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_unistd.h>		/* kik_usleep */
#include  <Imlib.h>


#if  1
#define  PSEUDO_TRANSPARENT
#endif


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
	if( pic_mod->brightness != 100)
	{
		ImlibColorModifier  mod ;

		Imlib_get_image_modifier( imlib , img , &mod) ;
		mod.brightness = ( mod.brightness * pic_mod->brightness) / 100 ;
		Imlib_set_image_modifier( imlib , img , &mod) ;
	}
	
	return  1 ;
}


/* --- global functions --- */

int
ml_picdep_display_opened(
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
ml_picdep_display_closed(
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

Pixmap
ml_picdep_load_file(
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

int
ml_picdep_root_pixmap_available(
	Display *  display
	)
{
	if( XInternAtom( display , "_XROOTPMAP_ID" , True))
	{
		return  1 ;
	}

	return  0 ;
}
	
Pixmap
ml_picdep_load_background(
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

	if( ! ml_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
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

int
ml_picdep_root_pixmap_available(
	Display *  display
	)
{
	return  1 ;
}

Pixmap
ml_picdep_load_background(
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
	
	if( ! ml_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
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
