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

#include "x_picture_dep.h"

typedef struct icon_cache_tag{
	Display * display;
	char * path;
	Pixmap icon;
	Pixmap mask;
	CARDINAL * data; 	///< used for _NET_WM_ICON
	struct icon_cache_tag * next; 
} icon_cache_t;

/* --- static variables --- */

static int display_count = 0;
static unsigned char gamma_cache[255];
static icon_cache_t *icon_cache = NULL;

/* --- static functions --- */
static icon_cache_t *
icon_cache_add(
	Display * display,
	char *path)
{
	void *p;
	icon_cache_t * dest;
	int i, j;
	int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;
	GdkPixbuf * pixbuf;

	Pixmap pixmap_return;
	Pixmap mask_return;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG "loading icon form %s .\n", path) ;
#endif	
	dest = malloc( sizeof( icon_cache_t));
	if (!dest)
		return NULL;

	dest->display = NULL; /* mark as empty for now*/

#ifndef OLD_GDK_PIXBUF
	pixbuf = gdk_pixbuf_new_from_file( path , NULL );
#else
	pixbuf = gdk_pixbuf_new_from_file( path );
#endif /*OLD_GDK_PIXBUF*/
	if ( !pixbuf ){
		free(dest);
		return NULL;
	}

/* create CARDINAL array for_NET_WM_ICON data */
	bytes_per_pixel = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	line = gdk_pixbuf_get_pixels (pixbuf);
	
	dest->data = malloc((width * height + 2) *4);
	if (!dest->data){
		gdk_pixbuf_unref(pixbuf);
		free(dest);
		return NULL;
	}
/* {width, height, ARGB[][]} */
	dest->data[0] = width;
	dest->data[1] = height;
	for (i = 0 ; i < height ; i++) {
		pixel = line;
		line += rowstride;
		
		for (j = 0 ; j < width ; j++) {
			if (bytes_per_pixel == 4) /* alpha support (convert to ARGB format)*/
				dest->data[(i*width+j)+2] = ((((((CARDINAL)(pixel[3]) << 8) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
			else                      /* completely opaque */
				dest->data[(i*width+j)+2] = ((((((CARDINAL)(0x0000FF) <<8 ) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
			pixel += bytes_per_pixel;
		}
	}

/* Create Icon pixmap&mask for WMHints. None as result is acceptable.*/
	gdk_pixbuf_xlib_render_pixmap_and_mask
		(pixbuf,
		 &(dest->icon),
		 &(dest->mask),
		 127);
	gdk_pixbuf_unref(pixbuf);

	dest->path = strdup(path);

	if (!dest->path){ /* memory exhausted */
		XFreePixmap(display, dest->icon);
		XFreePixmap(display, dest->mask);
		free(dest->data);
		free(dest);
		return NULL;  
	}
	dest->display = display;
	dest->next = icon_cache;
	icon_cache = dest;
	return dest;
}

static icon_cache_t *
icon_cache_lookup(
	Display * display,
	char *path)
{
	icon_cache_t *p;
	
	p = icon_cache;
	while(p){
		if ((p->display == display) && (strcmp(p->path, path) == 0) )
			return p;
		p = p->next;
	}
	return icon_cache_add(display, path);
}


static int
icon_cache_remove_display(Display * display){
	icon_cache_t * p;
	icon_cache_t * p_prev;

	p_prev = NULL;
	p = icon_cache;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " freeing display %d.\n", display) ;
#endif
	while (p){
		if (p->display == display){
			free(p->path);
			p->data = NULL;
			free(p->data);
			p->path = NULL;

			XFreePixmap(p->display, p->icon);
			XFreePixmap(p->display, p->mask);
			p->icon = p->mask = None;
			p->display = NULL;
			
			if (p_prev == NULL){ /* head */
				icon_cache = p->next;
				free(p);
				p = icon_cache;
			}else{
				p_prev->next = p->next;
				free(p);
				p = p_prev->next;
			}
		}else{
			p_prev = p;
			p = p->next;
		}	
	}
	return 0;
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
	icon_cache_remove_display( display); /* clean up pixmaps/cardinals */

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

int x_picdep_load_icon(
	x_window_t * win,
	char * path,
	u_int32_t **cardinal,
	Pixmap *pixmap,
	Pixmap *mask)
{
	icon_cache_t * icon;
	icon = icon_cache_lookup(win->display, path);

	if (!icon)
		icon = icon_cache_add(win->display, path);
	if (!icon)
		return 0;

	if (cardinal)
		*cardinal = icon->data;
	if (pixmap)
		*pixmap = icon->icon;
	if (mask)
		*mask = icon->mask;
	return 1;
}

int x_picdep_set_icon_from_file(
	x_window_t * win,
	char * file_path
	)
{
	XWMHints *hints;
	CARDINAL *cardinal;
	Pixmap pixmap;
	Pixmap mask;

	if( !file_path || !*file_path)
		return 0; 
	x_picdep_load_icon(
		win,
		file_path,
		&cardinal,
		&pixmap,
		&mask);
	
/* set extended window manager hint's icon */
	if (cardinal)
		XChangeProperty ( win->display, win->my_window,
				  XInternAtom(win->display, "_NET_WM_ICON", False),
				  XA_CARDINAL, 32,
				  PropModeReplace,
				  (unsigned char *)(cardinal), (cardinal[0])*(cardinal[1])+2);

/* set old style window manager hint's icon */		
	hints = NULL;
	if (pixmap & mask)
		hints = XAllocWMHints(); /* can be NULL*/
	if (!hints)
		return 0;
	hints->flags |= IconPixmapHint;
	hints->flags |= IconMaskHint;
	hints->icon_mask = mask;
	hints->icon_pixmap = pixmap;
	/* old pixmaps are kept in the cache and should be freed later */
	XSetWMHints(win->display, win->my_window, hints);
	XFree(hints);

	return 1;
}
