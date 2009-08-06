/*
 *	$Id$
 */

#include  <stdio.h>
#include  "x_imagelib.h"

#include  <X11/Xatom.h>			/* XA_PIXMAP */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>		/* realloc/free */
#include  <kiklib/kik_unistd.h>		/* kik_usleep */
#include  <Imlib2.h>


#if  1
#define  PSEUDO_TRANSPARENT
#endif


typedef struct  imlib
{
	Display *  display ;
	Imlib_Image *  image ;

} imlib_t ;


/* --- static variables --- */

static imlib_t **  imlibs ;
static u_int  num_of_imlibs ;


/* --- static functions --- */

static imlib_t *
get_imlib(
	Display *  display
	)
{
	int  count ;

	for( count = 0 ; count < num_of_imlibs ; count ++)
	{
		if( imlibs[count]->display == display)
		{
			return  imlibs[count] ;
		}
	}

	return  NULL ;
}

static int
modify_image(
	x_picture_modifier_t *  pic_mod
	)
{
	Imlib_Color_Modifier  color_modifier ;

	if( pic_mod->brightness != 100 || pic_mod->contrast != 100 || pic_mod->gamma != 100)
	{
		color_modifier = imlib_create_color_modifier( ) ;
		imlib_context_set_color_modifier( color_modifier) ;
		imlib_modify_color_modifier_brightness( 1.0 - ( pic_mod->brightness / 100.0)) ;
		imlib_modify_color_modifier_contrast( pic_mod->contrast / 100.0) ;
		imlib_modify_color_modifier_gamma( pic_mod->gamma / 100.0) ;
		imlib_apply_color_modifier( ) ;
		imlib_free_color_modifier( ) ;
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
	imlib_t *  imlib ;


	if( ( p = realloc( imlibs , sizeof( imlib_t *) * (num_of_imlibs + 1))) == NULL)
	{
		return  0 ;
	}

	imlibs = p ;

	if( ( imlib = malloc( sizeof( imlib_t) * 1)) == NULL)
	{
		return  0 ;
	}

	imlib->display = display ;
	imlib->image = NULL ;

	imlibs[num_of_imlibs] = imlib ;

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
		if( imlibs[count]->display == display)
		{
			if( imlibs[count]->image)
			{
				imlib_context_set_image( imlibs[count]->image) ;
				imlib_free_image_and_decache( ) ;
				imlibs[count]->image = NULL ;
			}
			
			if( -- num_of_imlibs == 0)
			{
				free( imlibs) ;
				imlibs = NULL ;
			}
			else
			{
				/* XXX this is a copy, shouldn't we 'imlibs[num_of_imlibs] = NULL' after ? */
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
	imlib_t *  imlib ;
	Imlib_Image *  img ;
	Pixmap  pixmap ;
	Pixmap  mask ;


	if( ! ( imlib = get_imlib( win->display)))
	{
		return  None ;
	}
	
	imlib_context_set_display( win->display) ;
	imlib_context_set_visual( DefaultVisual( win->display, DefaultScreen( win->display))) ;
	imlib_context_set_colormap( DefaultColormap( win->display, DefaultScreen( win->display))) ;
	imlib_context_set_drawable( win->my_window) ;

	if( ( img = imlib_load_image( file_path)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG , " imlib_load_image() failed.\n") ;
	#endif
	
		return  None ;
	}


	imlib_context_set_image( img);

	imlib->image = img ;

	if( pic_mod)
	{
		modify_image( pic_mod) ;
	}

	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	imlib_render_pixmaps_for_whole_image_at_size( &pixmap , &mask ,
		ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win)) ;

	imlib_free_image_and_decache( ) ;

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
	imlib_t *  imlib ;
	Imlib_Image *  img ;
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

				img = imlib_create_image_from_drawable( DefaultRootWindow( win->display) ,
					x , y , width , height , 0) ;
					
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

	img = imlib_create_image_from_drawable( src , 0 , 0 , width , height , 0) ;

	XDestroyWindow( win->display , src) ;
	XUngrabServer( win->display) ;

	if( ! img)
	{		
		return  None ;
	}

found:
	imlib_context_set_display( win->display) ;
	imlib_context_set_visual( DefaultVisual( win->display, DefaultScreen( win->display))) ;
	imlib_context_set_colormap( DefaultColormap( win->display, DefaultScreen( win->display))) ;
	imlib_context_set_drawable( win->my_window) ;
	
	imlib_context_set_image( img) ;

	imlib->image = img ;

	if( pic_mod)
	{
		modify_image( pic_mod) ;
	}

	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	imlib_render_pixmaps_for_whole_image_at_size( &pixmap , NULL ,
			width , height) ;

	imlib_free_image_and_decache( ) ;


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
	imlib_t *  imlib ;
	Imlib_Image *  img ;
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

	img = imlib_create_image_from_drawable( DefaultRootWindow( win->display) ,
			x , y , width , height , AllPlanes) ;

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

	imlib_context_set_display( win->display) ;
	imlib_context_set_visual( DefaultVisual( win->display, DefaultScreen( win->displaydisp))) ;
	imlib_context_set_colormap( DefaultColormap( win->display, DefaultScreen( win->display))) ;
	imlib_context_set_drawable( win->my_window) ;

	imlib_context_set_image( img) ;

	imlib->image = img ;

	if( pic_mod)
	{
		modify_image( img , pic_mod) ;
	}

	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	/* XXX humm ??? */
	imlib_render_pixmaps_for_whole_image_at_size( &pixmap , NULL ,
			/* pix_x , pix_y */ , width , height) ;

	imlib_free_image_and_decache( ) ;


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
	imlib_t *  imlib ;
	Imlib_Image *  img ;

	unsigned int  dst_width, dst_height ;


	if( ! ( imlib = get_imlib( display)))
	{
		return  0 ;
	}
	
	imlib_context_set_display( display) ;
	imlib_context_set_visual( DefaultVisual( display, DefaultScreen( display))) ;
	imlib_context_set_colormap( DefaultColormap( display, DefaultScreen( display))) ;
	imlib_context_set_drawable( DefaultRootWindow( display)) ;

	if( ( img = imlib_load_image( path)) == NULL)
	{		
		return  0 ;
	}

	imlib_context_set_image( img) ;

	imlib->image = img ;

	if( cardinal)
	{
		*cardinal = NULL ;
	}
	
	if( (!width) || *width == 0)
	{
		dst_width = imlib_image_get_width( ) ;
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
		dst_height = imlib_image_get_height( ) ;
		if( height)
		{
			*height = dst_height ;
		}
	}
	else
	{
		dst_height = *height ;
	}

	imlib_render_pixmaps_for_whole_image_at_size( pixmap, mask, dst_width, dst_height) ;
	
	imlib_free_image_and_decache( ) ;
	
	return 1 ;
}
