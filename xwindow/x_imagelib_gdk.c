/** @file
 *  @brief image handring functions using gdk-pixbuf
 *	$Id$
 */

#include <math.h>
#include <X11/Xatom.h>                   /* XInternAtom */

#ifdef OLD_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf-xlib.h>  
#else
#include <gdk-pixbuf-xlib/gdk-pixbuf-xlib.h>  
#endif

#include <kiklib/kik_types.h> /* u_int32_t */
#define CARDINAL u_int32_t
#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>    /* strdup */

#include "x_imagelib.h"

struct pixmap_store_tag {
	Display  *display;
	Pixmap icon;
	Pixmap mask;
	struct pixmap_store_tag *next;
} pixmap_store_t;

/* --- static variables --- */

static int display_count = 0;
static unsigned char gamma_cache[255];
static struct pixmap_store_t * pixmap_store;

/* --- static functions --- */


static unsigned char
modify_gamma(unsigned char value, x_picture_modifier_t *  pic_mod){
	if (value == 255)
		return 255;
	if (value == 0)
		return 0;

	if (gamma_cache[value])
		return gamma_cache[value];
	else 
		return gamma_cache[value] = 255 * pow((double)value / 255, (double)pic_mod->gamma / 100);
}

static unsigned char
modify_color(unsigned char value, x_picture_modifier_t *  pic_mod){
	int result;

	result = pic_mod->contrast*(value - 128)/100 + 128 * pic_mod->brightness/100;
	if (result >= 255)
		return 255;
 	if (result <= 0)
		return 0;
	return (unsigned char)(result);
}

static int
modify_image( GdkPixbuf *  pixbuf, GdkPixbuf * root, x_picture_modifier_t *  pic_mod){
	int i, j;
	int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;

	if ( !pixbuf )
			return 0;

	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3 ;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	if( !pic_mod)
		goto marge;

	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return 0;
	
       	line = gdk_pixbuf_get_pixels (pixbuf);
	if ((pic_mod->gamma != 100) &&
		    (pic_mod->gamma != gamma_cache[0])){
		memset(gamma_cache, 0, sizeof(gamma_cache));
		gamma_cache[0] = pic_mod->gamma %256 ; /* corrision will never happen actually */
		}
	for (i = 0 ; i < height ; i++) {
		pixel = line;
		line += rowstride;
		
		for (j = 0 ; j < width ; j++) {
/* XXX keeps neither hue nor saturation. MUST be replaced using better color model(CIE Yxy? lab?)*/
			pixel[0] = modify_color(pixel[0], pic_mod); 
			pixel[1] = modify_color(pixel[1], pic_mod);
			pixel[2] = modify_color(pixel[2], pic_mod);
			/* alpha is not changed */
			pixel += bytes_per_pixel;
		}
	}
		
	if (pic_mod->gamma != 100){
		line = gdk_pixbuf_get_pixels (pixbuf);
		for (i = 0 ; i < height ; i++) {
			pixel = line;
			line += rowstride;
			for (j = 0 ; j < width ; j++) {
				pixel[0] = modify_gamma(pixel[0], pic_mod); 
				pixel[1] = modify_gamma(pixel[1], pic_mod);
				pixel[2] = modify_gamma(pixel[2], pic_mod);
				pixel += bytes_per_pixel;
			}
		}
	}
 marge:
	if (root){
		int r_width, r_height, r_rowstride;
		unsigned char *r_line;
		unsigned char *r_pixel;
		r_line = gdk_pixbuf_get_pixels( root) ;
		r_rowstride = gdk_pixbuf_get_rowstride (root);
		line = gdk_pixbuf_get_pixels( pixbuf) ;
		for (i = 0 ; i < height ; i++) {
			pixel = line;
			line += rowstride;

			r_pixel = r_line;
			r_line += r_rowstride;
			for (j = 0 ; j < width ; j++) {
				pixel[0] = ((long)(pixel[0])*pixel[3]+(long)(r_pixel[0])*(255-pixel[3]))/255; 
				pixel[1] = ((long)(pixel[1])*pixel[3]+(long)(r_pixel[1])*(255-pixel[3]))/255; 
				pixel[2] = ((long)(pixel[2])*pixel[3]+(long)(r_pixel[2])*(255-pixel[3]))/255; 
				pixel += 4; /* should have alpha */
				r_pixel += 3; /* should not have alpha */
			}
		}
		gdk_pixbuf_unref( root );
	}
	return 1;
}

GdkPixbuf *
get_root_pixmap( x_window_t *  win){
	int  x ;
	int  y ;
	int  pix_x ;
	int  pix_y ;
	u_int  width ;
	u_int  height ;
	Window  src ;
	XSetWindowAttributes attr ;
	XEvent event ;
	int count ;
	GdkPixbuf *  img ;
	Atom id ;
	
	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
		return  None ;
	
	if( ( id = XInternAtom( win->display , "_XROOTPMAP_ID" , True))){
		Atom act_type ;
		int  act_format ;
		u_long  nitems ;
		u_long  bytes_after ;
		u_char *  prop ;
		
		if( XGetWindowProperty( win->display , DefaultRootWindow(win->display) , id , 0 , 1 ,
					False , XA_PIXMAP , &act_type , &act_format ,
					&nitems , &bytes_after , &prop) == Success){
			if( prop && *prop) {
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
					goto  found ;
			}
			else if( prop)
				XFree( prop) ;
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
	while( ! XCheckWindowEvent( win->display , src , ExposureMask, &event)){
		kik_usleep( 50000) ;

		if( ++ count >= 10){
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
		return  None ;

found:
	return  img ;
}

/* --- global functions --- */

int
x_imagelib_display_opened( Display *  display){
	if (display_count == 0){
#ifndef OLD_GDK_PIXBUF
		g_type_init();
#endif
		gdk_pixbuf_xlib_init( display, 0 ); 

	}
	display_count ++;
	return  1 ;
}

int
x_imagelib_display_closed( Display *  display){
	display_count --;

	/* XXX
         *
         *  there's no way to free mamories alocated from gdk_pixbuf_xlib_init( display, 0 ).
         *  replacement shou be written
         */
	return  1 ; 
}

Pixmap
x_imagelib_load_file_for_background( x_window_t *  win , char *  file_path , x_picture_modifier_t *  pic_mod){
	GdkPixbuf *  img ;
	GdkPixbuf *  scaled ;
	GdkPixbuf * root;
	Pixmap  pixmap ;
	GC gc;

#ifndef OLD_GDK_PIXBUF
	if( ( img = gdk_pixbuf_new_from_file( file_path , NULL )) == NULL)
		return  None ;
#else
	if( ( img = gdk_pixbuf_new_from_file( file_path )) == NULL)
		return  None ;
#endif /*OLD_GDK_PIXBUF*/

	if (gdk_pixbuf_get_has_alpha( img)){
		root = get_root_pixmap( win);
	}else{
		root = NULL;
	}

	
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	scaled = gdk_pixbuf_scale_simple(img, ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				GDK_INTERP_TILES); /* use one of _NEAREST, _TILES, _BILINEAR, _HYPER (speed<->quality) */

	if (scaled){
		gdk_pixbuf_unref( img );
		img = scaled;
	}
	if( pic_mod || root)
		modify_image( img , root, pic_mod) ;

	gc = XCreateGC( win->display, win->my_window, 0, 0 );
	gdk_pixbuf_xlib_render_to_drawable( img , 
					    pixmap,
					    gc , 
					    0 , 0 , /* src */
					    0 , 0 , /* dest */
					    ACTUAL_WIDTH(win) ,
					    ACTUAL_HEIGHT(win) ,
					    XLIB_RGB_DITHER_NORMAL , /* Dither */
					    0 , 0 /* dither x,y */) ;
	XFreeGC( win->display , gc );
	gdk_pixbuf_unref( img );
	return  pixmap ;
}

int
x_imagelib_root_pixmap_available( Display *  display){
	if( XInternAtom( display , "_XROOTPMAP_ID" , True))
		return  1 ;

	return  0 ;
}

Pixmap
x_imagelib_get_transparent_background( x_window_t *  win , x_picture_modifier_t *  pic_mod){
	int  x ;
	int  y ;
	int  pix_x ;
	int  pix_y ;
	u_int  width ;
	u_int  height ;
	Pixmap  pixmap ;
	GC gc;
	GdkPixbuf * img;

	img = get_root_pixmap( win);
	if( !img)
		return None;

	if( pic_mod)
		modify_image( img , NULL, pic_mod) ;
	
	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
		return  None ;

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
					    XLIB_RGB_DITHER_NONE, /* depth should be equal */
					    0 , 0 /* dither x,y */ ) ;
	XFreeGC( win->display , gc );
	gdk_pixbuf_unref( img ) ;

	return  pixmap ;
}

int x_imagelib_load_file(
	Display * display,
	char * path,
	u_int32_t **cardinal,
	Pixmap *pixmap,
	Pixmap *mask)
{
	GdkPixbuf * pixbuf;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "loading icon form %s .\n", path) ;
#endif	
#ifndef OLD_GDK_PIXBUF
	pixbuf = gdk_pixbuf_new_from_file( path , NULL );
#else
	pixbuf = gdk_pixbuf_new_from_file( path );
#endif /*OLD_GDK_PIXBUF*/
	if ( !pixbuf )
		return 0;

	if ( cardinal){
		int width, height, rowstride, bytes_per_pixel;
		unsigned char *line;
		unsigned char *pixel;
		int i, j;

/* create CARDINAL array for_NET_WM_ICON data */
		bytes_per_pixel = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
		width = gdk_pixbuf_get_width (pixbuf);
		height = gdk_pixbuf_get_height (pixbuf);
		rowstride = gdk_pixbuf_get_rowstride (pixbuf);
		line = gdk_pixbuf_get_pixels (pixbuf);
		if (*cardinal = malloc((width * height + 2) *4)){
			
/* {width, height, ARGB[][]} */
			(*cardinal)[0] = width;
			(*cardinal)[1] = height;
			for (i = 0 ; i < height ; i++) {
				pixel = line;
				line += rowstride;
				
				for (j = 0 ; j < width ; j++) {
					if (bytes_per_pixel == 4) /* alpha support (convert to ARGB format)*/
						(*cardinal)[(i*width+j)+2] = ((((((CARDINAL)(pixel[3]) << 8) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
					else                      /* completely opaque */
						(*cardinal)[(i*width+j)+2] = ((((((CARDINAL)(0x0000FF) <<8 ) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
					pixel += bytes_per_pixel;
				}
			}
		}
		
	}
/* Create Icon pixmap&mask for WMHints. None as result is acceptable.*/
	if (pixmap || mask)
		gdk_pixbuf_xlib_render_pixmap_and_mask( pixbuf, pixmap, mask, 127);
	gdk_pixbuf_unref(pixbuf);
	    
	return 1;
}
