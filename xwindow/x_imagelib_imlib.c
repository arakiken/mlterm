/*
 *	$Id$
 */

#include  "x_imagelib.h"

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
	int  count ;

	for( count = 0 ; count < num_of_imlibs ; count ++)
	{
		if( imlibs[count].display == display)
		{
			if( imlibs[count].imlib == NULL &&
				(imlibs[count].imlib = Imlib_init( display)) == NULL)
			{
			#ifdef  DEBUG
				kik_warn_printf( "failed to init imlib.\n") ;
			#endif

				return  None ;
			}

			return  imlibs[count].imlib ;
		}
	}

	return  NULL ;
}

static int
modify_image(
	ImlibData *  imlib ,
	ImlibImage *  img ,
	x_picture_modifier_t *  pic_mod
	)
{
	if( pic_mod->brightness != 100 || pic_mod->contrast != 100 || pic_mod->gamma != 100)
	{
		ImlibColorModifier  mod ;

		Imlib_get_image_modifier( imlib , img , &mod) ;
		mod.brightness = ( mod.brightness * pic_mod->brightness) / 100 ;
		mod.contrast = ( mod.contrast * pic_mod->contrast) / 100 ;
		mod.gamma = pic_mod->gamma > 0 ? ( ( mod.gamma * 100) / pic_mod->gamma) : ( mod.gamma * 1000) ;
		Imlib_set_image_modifier( imlib , img , &mod) ;
	}
	
	return  1 ;
}


/* --- global functions --- */

int
x_imagelib_display_opened(
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
x_imagelib_display_closed(
	Display *  display
	)
{
	int  count ;
	
	for( count = 0 ; count < num_of_imlibs ; count ++)
	{
		if( imlibs[count].display == display)
		{
			/*
			 * XXX
			 * How to destroy ImlibData ?
			 * static data of this display are still left in imlib , which
			 * results in unexpected behaviors(maybe core dumps)
			 */

			if( imlibs[count].imlib)
			{
				Imlib_free_colors( imlibs[count].imlib) ;
				free( imlibs[count].imlib) ;
			}
			
			if( -- num_of_imlibs == 0)
			{
				free( imlibs) ;
				imlibs = NULL ;
			}
			else
			{
				imlibs[count] = imlibs[num_of_imlibs] ;
			}

			return  1 ;
		}
	}

	return  1 ;
}

Pixmap
x_imagelib_load_file_for_background(
	x_window_t *  win ,
	char *  file_path ,
	x_picture_modifier_t *  pic_mod
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
x_imagelib_root_pixmap_available(
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
x_imagelib_get_transparent_background(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
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
	int count ;
	ImlibData *  imlib ;
	ImlibImage *  img ;
	Atom id ;
	
	if( ! ( imlib = get_imlib( win->display)))
	{
		return  None ;
	}

	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
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

	count = 0 ;
	while( ! XCheckWindowEvent( win->display , src , ExposureMask, &event))
	{
		kik_usleep( 50000) ;

		if( ++ count >= 10)
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
x_imagelib_root_pixmap_available(
	Display *  display
	)
{
	return  1 ;
}

Pixmap
x_imagelib_get_transparent_background(
	x_window_t *  win ,
	x_picture_modifier_t *  pic_mod
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
	x_window_t *  root ;
	int  count ;
	XEvent *  queued_events ;
	u_int  num_of_queued ;
	XEvent  event ;

	if( ! ( imlib = get_imlib( win->display)))
	{
		return  None ;
	}

	root = x_get_root_window(win) ;
	
	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
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

	x_window_remove_event_mask( root , StructureNotifyMask /* | SubstructureNotifyMask */) ;

	/*
	 * StructureNotifyMask events are ignored from here.
	 */
	
	x_window_unmap( root) ;

	/* XXX waiting for all exposed windows actually redrawn. */
	XSync( root->display , False) ;
	kik_usleep( 25000) ;
	
	img = Imlib_create_image_from_drawable( imlib , DefaultRootWindow( win->display) ,
		AllPlanes , x , y , width , height) ;

	/* XXX ingoring all queued Expose events */
	while( XCheckWindowEvent( root->display , root->my_window , ExposureMask, &event)) ;
	
	x_window_map( root) ;

	/*
	 * StructureNoitfyMask events are ignored till here.
	 */
	
	x_window_add_event_mask( root , StructureNotifyMask /* | SubstructureNotifyMask */) ;

	/* XXX waiting for all StructureNotifyMask events responded */
	XSync( root->display , False) ;
	kik_usleep( 25000) ;

	/* XXX ignoreing all queued StructureNotifyMask events */
	while( XCheckWindowEvent( root->display , root->my_window , StructureNotifyMask , &event)) ;

	/* XXX restoring all backuped StructureNotifyMask events */
	for( count = 0 ; count < num_of_queued ; count ++)
	{
		XPutBackEvent( root->display , &queued_events[count]) ;
	}
	free( queued_events) ;
	
	/* XXX waiting for root window actually mapped. */
	count = 0 ;
	while( ! XCheckWindowEvent( root->display , root->my_window , ExposureMask, &event))
	{
		kik_usleep( 50000) ;

		if( ++ count >= 10)
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

int x_imagelib_load_file(
	Display *  display,
	char *  path,
	u_int32_t **  cardinal,
	Pixmap *  pixmap,
	Pixmap *  mask,
	unsigned int *  width,
	unsigned int *  height
	)
{
	ImlibData *  imlib ;
	ImlibImage *  img ;

	unsigned int  dst_width, dst_height ;

	if( ! ( imlib = get_imlib( display)))
	{
		return  0 ;
	}
	
	if( ( img = Imlib_load_image( imlib , path)) == NULL)
	{		
		return  0 ;
	}

	if( cardinal)
	{
		*cardinal = NULL ;
	}
	
	if( (!width) || *width == 0)
	{
		dst_width = img->rgb_width;
		if( width)
		{
			*width = dst_width ;
		}
	}
	else
	{
		dst_width = *width ;
	}

	if( (!height) || *height == 0)
	{
		dst_height = img->rgb_height;
		if( height)
		{
			*height = dst_height ;
		}
	}
	else
	{
		dst_height = *height ;
	}

	Imlib_render( imlib, img, dst_width, dst_height) ;
	
	if( pixmap)
	{
		*pixmap = Imlib_move_image( imlib, img) ;
	}
	if( mask)
	{
		*mask = Imlib_move_mask( imlib, img) ;
	}

	Imlib_kill_image( imlib , img) ;
	
	return 1 ;
}
