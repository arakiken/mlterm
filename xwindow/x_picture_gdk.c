/*
 *	$Id$
 */

#if  1
#define  PSEUDO_TRANSPARENT
#endif

#include <X11/Xatom.h>                   /* XInternAtom */
#include <gdk-pixbuf/gdk-pixbuf-xlib.h>  
#include <kiklib/kik_unistd.h>

#include "x_picture_dep.h"



/* --- static variables --- */

/* gdk-pixbuf does not need a black magic using static variables */

/* --- static functions --- */


static unsigned char
illuminate(unsigned char value, int luminosity){
	
	if (luminosity >100)  
		return 255 - (255 - value)*100/luminosity;
	if (luminosity ==100)  
		return value;
	if (luminosity >0)  
		return luminosity*value/100;
	if (luminosity ==0)  
		return 0;
	if (luminosity > -100)  
		return (-100-luminosity)*value/100;
	/* XXX who cares? */
		return 0 ;
}

static int
modify_image(
	GdkPixbuf *  pixbuf ,
	x_picture_modifier_t *  pic_mod
	)
{
	int i, j;
	int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;

	if( pic_mod->brightness == 100)
		return 0;

        if ( !pixbuf )
		return 0;
  
	bytes_per_pixel = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3; /* alpha may supported someday  */
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
	line = gdk_pixbuf_get_pixels (pixbuf);
		
	for (i = 0 ; i < height ; i++) {
		pixel = line;
		line += rowstride;
		
		for (j = 0 ; j < width ; j++) {
/* XXX keeps neither hue nor saturation. MUST be replaced using better color model(CIE Yxy? lab?)*/
			pixel[0] = illuminate(pixel[0], pic_mod->brightness); 
			pixel[1] = illuminate(pixel[1], pic_mod->brightness);
			pixel[2] = illuminate(pixel[2], pic_mod->brightness);
			/* alpha is not changed */
			pixel += bytes_per_pixel;
		}
	}
	return 1;
}

/* --- global functions --- */

int
x_picdep_display_opened(
	Display *  display
	)
{
/* XXX it should be harmless to call them many many times*/
#ifndef OLD_GDK_PIXBUF
	g_type_init();
#endif
	gdk_pixbuf_xlib_init( display, 0 ); 
	return  1 ;
}

int
x_picdep_display_closed(
	Display *  display
	)
{
	return  1 ; /* XXX who frees cmap? */
}

Pixmap
x_picdep_load_file( x_window_t *  win ,
		     char *  file_path ,
		     x_picture_modifier_t *  pic_mod)
{
	GdkPixbuf *  img ;
	Pixmap  pixmap ;
	GC gc;

#ifndef OLD_GDK_PIXBUF
	if( ( img = gdk_pixbuf_new_from_file( file_path , NULL )) == NULL)
	{/* XXX error handling? */
		return  None ;
	}
#else
	if( ( img = gdk_pixbuf_new_from_file( file_path )) == NULL)
	{
		return  None ;
	}
#endif /*OLD_GDK_PIXBUF*/

	if( pic_mod)
	{
		modify_image( img , pic_mod) ;
	}
	
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;
	

	gc = XCreateGC( win->display, win->my_window, 0, 0 );
	gdk_pixbuf_xlib_render_to_drawable( img , 
					    pixmap,
					    gc , 
					    0 , 0 , /* src */
					    0 , 0 , /* dest */
					    ACTUAL_WIDTH(win) ,
					    ACTUAL_HEIGHT(win) ,
					    XLIB_RGB_DITHER_NORMAL , /* Dither */
					    0 , 0 /* dither x,y */
		) ;
	XFreeGC( win->display , gc );
	gdk_pixbuf_unref( img );
	return  pixmap ;
}

#ifdef  PSEUDO_TRANSPARENT

int
x_picdep_root_pixmap_available(
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
x_picdep_load_background(
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
	GdkPixbuf *  img ;
	GC gc;
	Atom id ;
	
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

				img = gdk_pixbuf_xlib_get_from_drawable( NULL , /*create new pixbuf*/
									 *((Drawable *) prop) , /* source */
									 xlib_rgb_get_cmap() , 
									 xlib_rgb_get_visual() ,
									 x , y , /* src */
									 0 , 0 , /* dest */
									 width , height) ;
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

	img = gdk_pixbuf_xlib_get_from_drawable( NULL , /* create new one */ 
						 src ,
						 xlib_rgb_get_cmap() , 
						 xlib_rgb_get_visual() ,
						 0 , 0 , /* src */
						 0 , 0 , /* dest */
						 width , height) ;

	XDestroyWindow( win->display , src) ;
	XUngrabServer( win->display) ;
	if( ! img)
	{		
		return  None ;
	}

found:
	if( pic_mod)
	{
		modify_image( img , pic_mod) ;
	}
	
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	gc = XCreateGC( win->display, win->my_window, 0, 0 );
	gdk_pixbuf_xlib_render_to_drawable( img , 
					    pixmap,
					    gc , 
					    0 , 0 , /* src */
					    pix_x , pix_y , /* dest */
					    width ,
					    height ,
					    XLIB_RGB_DITHER_NONE,
					    0 , 0 /* dither x,y */ ) ;
	XFreeGC( win->display , gc );
	gdk_pixbuf_unref( img ) ;

	return  pixmap ;
}

#else /* ! PSEUDO_TRANSPARENT */

int
x_picdep_root_pixmap_available(
	Display *  display
	)
{
	return  1 ;
}

Pixmap
x_picdep_load_background(
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
	GdkPixbuf *  img ;
	x_window_t *  root ;
	int  count ;
	XEvent *  queued_events ;
	u_int  num_of_queued ;
	XEvent  event ;


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
	
	img = gdk_pixbuf_xlib_get_from_drawable( NULL ,
						 DefaultRootWindow( win->display) ,
						 NULL, NULL , x , y , 0 , 0 , width , height) ;

	/* XXX ignoring all queued Expose events */
	while( XCheckWindowEvent( root->display , root->my_window , ExposureMask, &event)) ;
	
	x_window_map( root) ;

	/*
	 * StructureNoitfyMask events are ignored till here.
	 */
	
	x_window_add_event_mask( root , StructureNotifyMask /* | SubstructureNotifyMask */) ;

	/* XXX waiting for all StructureNotifyMask events responded */
	XSync( root->display , False) ;
	kik_usleep( 25000) ;

	/* XXX ignoring all queued StructureNotifyMask events */
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
		modify_image( img , pic_mod) ;
	}

	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;
	gdk_pixbuf_xlib_render_to_drawable( img , 
					    pixmap,
					    gc , 
					    0 , 0 , pix_x , pix_y ,
					    ACTUAL_WIDTH(win) ,
					    ACTUAL_HEIGHT(win) ,
					    XLIB_RGB_DITHER_NONE,
					    0 , 0) ;

	gdk_pixbuf_unref( img) ;

	return  pixmap ;
}

#endif /* PSEUDO_TRANSPARENT */	

