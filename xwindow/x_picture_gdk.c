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

#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>    /* strdup */

#include "x_picture_dep.h"

typedef struct {
	Display * display;
	char * path;
	Pixmap icon;
	Pixmap mask;
	unsigned long * data; 	///< used for _NET_WM_ICON
	int width;
	int height;
} icon_cache_t;

/* --- static variables --- */

static int display_count = 0;
static unsigned char gamma_cache[255];
static icon_cache_t *icon_cache = NULL;
static int cache_size = 0;

/* --- static functions --- */

static icon_cache_t *
icon_cache_lookup(
	Display * display,
	char *path)
{
	int i;

	for (i = 0; i< cache_size; i++){
		if ((icon_cache[i].display == display) && (strcmp(icon_cache[i].path, path) == 0) )
			return &icon_cache[i];
	}
	return NULL;
}

static icon_cache_t *
icon_cache_add(
	Display * display,
	char *path, Pixmap icon,
	Pixmap mask,
	unsigned long *data,
	int width,
	int height)
{
	void *p;
	icon_cache_t * dest;

	dest = icon_cache_lookup(NULL, NULL); /* seek freed ones */
	if (dest == NULL){
		if( !( p = realloc( icon_cache , sizeof( icon_cache_t) * (cache_size + 1) ) ) ){ /* no free area, expand */
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " couldn't expand icon cache.\n") ;
			#endif
			XFreePixmap(display, icon); /* must be cleaned here */
			XFreePixmap(display, mask);
			return NULL; 
		}
		icon_cache = p;
		dest = &icon_cache[cache_size];
		cache_size ++;
	}
	dest->path = strdup(path);
	if (!dest->path){ /* memory exhausted */
		dest->display = 0;
		XFreePixmap(display, icon); /* must be cleaned here */
		XFreePixmap(display, mask);

		return NULL; 
	}
	dest->display = display;
	dest->icon = icon;
	dest->mask = mask;
	dest->data = data;
	dest->width = width;
	dest->height = height;

	return dest;
}

static int
icon_cache_remove_display(Display * display){
	int i;
	int is_shrinked = 0;
	for (i = 0; i< cache_size; i++){
		if (icon_cache[i].display == display){
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " freeing display %d.\n", display) ;
			#endif
			free(icon_cache[i].path);
			free(icon_cache[i].data);
			icon_cache[i].path = NULL;
			icon_cache[i].data = NULL;

			XFreePixmap(icon_cache[i].display, icon_cache[i].icon);
			XFreePixmap(icon_cache[i].display, icon_cache[i].mask);
			icon_cache[i].icon = icon_cache[i].mask = None;
			icon_cache[i].display = NULL;
			/* icon cache never shrink. (cache_size is kept sane here) */
			is_shrinked = 1;
		}
	}
	return is_shrinked;
}

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
modify_image( GdkPixbuf *  pixbuf, x_picture_modifier_t *  pic_mod){
	int i, j;
	int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;

	if( pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return 0;

        if ( !pixbuf )
		return 0;
  
	bytes_per_pixel = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3; /* alpha may supported someday  */
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	
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
			if (pic_mod->gamma != 100){
				pixel[0] = modify_gamma(pixel[0], pic_mod); 
				pixel[1] = modify_gamma(pixel[1], pic_mod);
				pixel[2] = modify_gamma(pixel[2], pic_mod);
			}
			/* alpha is not changed */
			pixel += bytes_per_pixel;
		}
	}
	return 1;
}

/* --- global functions --- */

int
x_picdep_display_opened( Display *  display){
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
x_picdep_display_closed( Display *  display){
	display_count --;
	icon_cache_remove_display(display); /* clean up pixmaps/cardinals */
	/* XXX
         *
         *  there's no way to free mamories alocated from gdk_pixbuf_xlib_init( display, 0 ).
         *  replacement shou be written
         */
	return  1 ; 
}

Pixmap
x_picdep_load_file( x_window_t *  win , char *  file_path , x_picture_modifier_t *  pic_mod){
	GdkPixbuf *  img ;
	GdkPixbuf *  scaled ;
	Pixmap  pixmap ;
	GC gc;

#ifndef OLD_GDK_PIXBUF
	if( ( img = gdk_pixbuf_new_from_file( file_path , NULL )) == NULL)
		return  None ;
#else
	if( ( img = gdk_pixbuf_new_from_file( file_path )) == NULL)
		return  None ;
#endif /*OLD_GDK_PIXBUF*/

	if( pic_mod)
		modify_image( img , pic_mod) ;
	
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
			DefaultDepth( win->display , win->screen)) ;

	scaled = gdk_pixbuf_scale_simple(img, ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				GDK_INTERP_TILES); /* use one of _NEAREST, _TILES, _BILINEAR, _HYPER (speed<->quality) */

	if (scaled){
		gdk_pixbuf_unref( img );
		img = scaled;
	}
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
x_picdep_root_pixmap_available( Display *  display){
	if( XInternAtom( display , "_XROOTPMAP_ID" , True))
		return  1 ;

	return  0 ;
}

Pixmap
x_picdep_load_background( x_window_t *  win , x_picture_modifier_t *  pic_mod){
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
	if( pic_mod)
		modify_image( img , pic_mod) ;
	
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

int x_picdep_set_icon_from_file(
	x_window_t * win,
	char * file_path
	)
{
	XWMHints *hints;
	icon_cache_t * icon;

	if( !file_path || !*file_path)
		return 0; /* XXX unset icon?*/
	icon = icon_cache_lookup(win->display, file_path);

	if (!icon){/* not cached */
		int i, j;
		int width, height, rowstride, bytes_per_pixel;
		unsigned char *line;
		unsigned char *pixel;
		GdkPixbuf * pixbuf;
		unsigned long * data;
		Pixmap pixmap_return;
		Pixmap mask_return;
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG "loading icon form %s .\n", file_path) ;
		#endif

#ifndef OLD_GDK_PIXBUF
		if( ( pixbuf = gdk_pixbuf_new_from_file( file_path , NULL )) == NULL)
			return  0 ;/* XXX error handling? */
#else
		if( ( pixbuf = gdk_pixbuf_new_from_file( file_path )) == NULL)
			return  0 ;
#endif /*OLD_GDK_PIXBUF*/
		if ( !pixbuf )
			return 0;

/* create CARDINAL array for_NET_WM_ICON data */
		bytes_per_pixel = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
		width = gdk_pixbuf_get_width (pixbuf);
		height = gdk_pixbuf_get_height (pixbuf);
		rowstride = gdk_pixbuf_get_rowstride (pixbuf);
		line = gdk_pixbuf_get_pixels (pixbuf);
		
		data = malloc((width * height + 2) *4);
		if (!data){
			gdk_pixbuf_unref(pixbuf);
			return 0;
		}
/* {width, height, ARGB[][]} */
		data[0] = width;
		data[1] = height;
		for (i = 0 ; i < height ; i++) {
			pixel = line;
			line += rowstride;
			
			for (j = 0 ; j < width ; j++) {
				if (bytes_per_pixel == 4) /* alpha support (convert to ARGB format)*/
					data[(i*width+j)+2] = ((((((long)(pixel[3]) << 8) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
				else                      /* completely opaque */
					data[(i*width+j)+2] = ((((((long)(0x0000FF) <<8 ) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
				pixel += bytes_per_pixel;
			}
		}
/* Create Icon pixmap&mask for WMHints. None as result is acceptable.*/
		gdk_pixbuf_xlib_render_pixmap_and_mask
			(pixbuf,
			 &pixmap_return,
			 &mask_return,
			 127);
		gdk_pixbuf_unref(pixbuf);

/* cache the reslut */
		icon = icon_cache_add( win->display, file_path, pixmap_return, mask_return, data, width, height);
	}

/* XXX
 *
 * Following stuff should be moved into x_window.c.  
 * Ths function should be separated into 
 *  - return CARDINAL[] (_NET_WM_ICON data)
 *  - return pixmap     ( WMHints data)
 *  - return mask       ( WMHints data)
 *  - and backend       (maintain cache)
 */ 


/* set extended window manager hint's icon */
	XChangeProperty ( win->display, win->my_window,
			  XInternAtom(win->display, "_NET_WM_ICON", False),
			  XA_CARDINAL, 32,
			  PropModeReplace,
			  (unsigned char *)(icon->data), (icon->width)*(icon->height)+2);

/* set old style window manager hint's icon */		
	hints = XAllocWMHints(); /* can be NULL*/
	if (!hints)
		return 0;
	hints->flags |= IconPixmapHint;
	hints->flags |= IconMaskHint;
	hints->icon_mask = icon->mask;
	hints->icon_pixmap = icon->icon;
	/* old pixmaps are kept in the cache and should be freed later */
	XSetWMHints(win->display, win->my_window, hints);
	XFree(hints);

	return 1;
}
