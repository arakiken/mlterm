/** @file
 *  @brief image handling functions using gdk-pixbuf
 *	$Id$
 */

#include <math.h>                        /* pow */
#include <sys/time.h>                    /* gettimeofdy */
#include <X11/Xatom.h>                   /* XInternAtom */
#include <string.h>                      /* memcpy */
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <kiklib/kik_types.h> /* u_int32_t/u_int16_t */
#include <kiklib/kik_unistd.h>
#include <kiklib/kik_str.h>    /* strdup */

#include "x_imagelib.h"

/** Pixmap cache per display. */
typedef struct display_store_tag {
	Display *display; /**<Display (primary key)*/
	Pixmap root;      /**<Root pixmap !!! NOT owned by mlterm !!!*/
	Pixmap cooked;    /**<Background pixmap cache*/
	x_picture_modifier_t  pic_mod; /**<modification applied to "cooked"*/
	struct timeval tval; /**<The time when this cache entry was created*/
	struct display_store_tag *next;
} display_store_t;

/* --- static variables --- */

static int display_count = 0;
static unsigned char gamma_cache[256 +1];
static display_store_t * display_store = NULL;
static char * wp_cache_name = NULL;
static GdkPixbuf * wp_cache_data = NULL;
static int wp_cache_width = 0;
static int wp_cache_height = 0;
static GdkPixbuf * wp_cache_scaled = NULL;

/* --- static functions --- */

/**Remove a cache for the display
 *
 *\param display The display to be removed from cache.
 *
 */
static void
cache_delete(Display * display){
	display_store_t * cache;
	display_store_t * o;
	
	o = NULL;
	cache = display_store;
	while( cache){
		if ( cache->display == display){
			if ( o == NULL){
				display_store = cache->next;
				if (cache->cooked)
					XFreePixmap( display, cache->cooked);
				free( cache);
				cache = display_store;
			}else{
				o->next = cache->next;
				if (cache->cooked)
					XFreePixmap( display, cache->cooked);
				free( cache);
				cache = o->next;			
			}
		}else{
			o = cache;
			cache = cache->next;
		}
	}
}

/**judge whether pic_mod is equal or not
 *\param a,b picture modifier
 *\return 1 when they are same. 0 when not.
 */
static int
is_picmod_eq(
		x_picture_modifier_t * a,
		x_picture_modifier_t * b
	){
	if(a->brightness == b->brightness && a->contrast == b->contrast && a->gamma == b->gamma)
		return 1;
	return 0;
}

/**Return position of least significant bit
 *
 *\param val value to count
 *
 */
static int
lsb(
	unsigned int val){
	int nth = 0;
	
	while((val & 1) == 0){
		val = val >>1;
		nth ++;
	}
	return nth;
}

/**Return position of most significant bit
 *
 *\param val value to count
 *
 */
static int
msb(
	unsigned int val){
	int nth;

	nth = lsb( val) +1;
	while(val & (1 << nth)){
		nth++;
	}
	return nth;
}


static void
/**convert pixbuf into depth 16 pixmap
 *
 *\param display
 *\param screen
 *\param pixbuf source
 *\param pixmap where image is rendered(should be created before calling this function)
 *
 */
pixbuf_to_pixmap_16t(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf,
	Pixmap pixmap){

	u_int16_t *data;
	XImage * image;

	unsigned int i, j;
	unsigned int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;
	
	int r_offset, g_offset, b_offset;
	int r_limit, g_limit, b_limit;
	long r_mask, g_mask, b_mask;
	int matched;
	XVisualInfo *vinfolist;
	XVisualInfo vinfo;

	if (!pixbuf)
		return;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	data = (u_int16_t *)malloc( width * height * sizeof(long));
	if( !data)
		return;
	image = XCreateImage( display, DefaultVisual( display , screen),
			      DefaultDepth( display, screen), ZPixmap, 0,
			      (char *)data,
			      gdk_pixbuf_get_width( pixbuf),
			      gdk_pixbuf_get_height( pixbuf),
			      16,
			      gdk_pixbuf_get_width( pixbuf) * 2);	
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display , screen));
	if (!vinfo.visualid){
		free( data);
		return;
	}
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched);
	if ( (!matched) || (!vinfolist) ){
		free( data);
		XDestroyImage( image);
		return;
	}
	r_mask = vinfolist[0].red_mask;
	g_mask = vinfolist[0].green_mask;
	b_mask = vinfolist[0].blue_mask;
	r_offset = lsb( r_mask);
	g_offset = lsb( g_mask);
	b_offset = lsb( b_mask);
	r_limit = 8 + r_offset - msb( r_mask);
	g_limit = 8 + g_offset - msb( g_mask);
	b_limit = 8 + b_offset - msb( b_mask);
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       	line = gdk_pixbuf_get_pixels (pixbuf);
	for (i = 0; i < height; i++) {
		pixel = line;
		for (j = 0; j < width; j++) {
			data[i*width +j ] =
				(((pixel[0] >> r_limit) << r_offset) & r_mask) |
				(((pixel[1] >> g_limit) << g_offset) & g_mask) |
				(((pixel[2] >> b_limit) << b_offset) & b_mask);
			pixel +=bytes_per_pixel;
		}
		line += rowstride;
	}

	XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
		   gdk_pixbuf_get_width( pixbuf),gdk_pixbuf_get_height( pixbuf));
	XFree(vinfolist);
	XDestroyImage( image);
}

static void
pixbuf_to_pixmap_24t(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf,
	Pixmap pixmap){

	u_int32_t *data;
	XImage * image;

	unsigned int i, j;
	unsigned int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;
	
	int r_offset, g_offset, b_offset;	
	int matched;
	XVisualInfo *vinfolist;
	XVisualInfo vinfo;

	if (!pixbuf)
		return;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	data = (u_int32_t *)malloc( width * height * sizeof(long) );
	if( !data)
		return;
	image = XCreateImage( display, DefaultVisual( display , screen),
			      DefaultDepth( display, screen), ZPixmap, 0,
			      (char *)data,
			      gdk_pixbuf_get_width( pixbuf),
			      gdk_pixbuf_get_height( pixbuf),
			      32 ,
			      gdk_pixbuf_get_width( pixbuf) * 4);
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display , screen));
	if (!vinfo.visualid){
		free( data);
		return;
	}
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched);
	if ( (!matched) || (!vinfolist) ){
		free( data);
		XDestroyImage( image);
		return;
	}
	r_offset = lsb( vinfolist[0].red_mask);
	g_offset = lsb( vinfolist[0].green_mask);
	b_offset = lsb( vinfolist[0].blue_mask);
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       	line = gdk_pixbuf_get_pixels (pixbuf);
	for( i = 0; i < height; i++){
		pixel = line;
		for( j = 0; j < width; j++){
			data[i*width +j ] = pixel[0] <<r_offset | pixel[1] <<g_offset | pixel[2]<<b_offset;
			pixel +=bytes_per_pixel;
		}
		line += rowstride;
	}
	XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
		   gdk_pixbuf_get_width( pixbuf),gdk_pixbuf_get_height( pixbuf));
	XFree(vinfolist);
	XDestroyImage( image);
}

static void
pixbuf_to_pixmap(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf ,
	Pixmap pixmap){

	switch (DefaultDepth( display , screen)){
		
	case 1:
		/* XXX not yet supported */
		break;
	case 8:
		/* XXX not yet supported */
		break;
	case 15:
	case 16:
		pixbuf_to_pixmap_16t(
			display,
			screen,
			pixbuf ,
			pixmap);
		break;
	case 24:
		/*FALL THROUGH*/
	case 32:
		pixbuf_to_pixmap_24t(
			display,
			screen,
			pixbuf ,
			pixmap);
		break;
	default:
	}
}

static void
compose_to_pixmap_16t(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf,
	Pixmap pixmap){

	XImage * image;

	unsigned int i, j;
	unsigned int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;
	
	int r_offset, g_offset, b_offset;	
	int r_limit, g_limit, b_limit;
	long r_mask, g_mask, b_mask;
	long r, g, b;
	int matched;
	XVisualInfo *vinfolist;
	XVisualInfo vinfo;

	if (!pixbuf)
		return;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap);
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display , screen));
	if (!vinfo.visualid)
		return;       
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched);
	if ( (!matched) || (!vinfolist) ){
		XDestroyImage( image);
		return;
	}
	r_mask = vinfolist[0].red_mask;
	g_mask = vinfolist[0].green_mask;
	b_mask = vinfolist[0].blue_mask;
	r_offset = lsb( r_mask);
	g_offset = lsb( g_mask);
	b_offset = lsb( b_mask);
	r_limit = 8 + r_offset - msb( r_mask);
	g_limit = 8 + g_offset - msb( g_mask);
	b_limit = 8 + b_offset - msb( b_mask);
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       	line = gdk_pixbuf_get_pixels (pixbuf);
	for( i = 0; i < height; i++){
		pixel = line;
		for( j = 0; j < width; j++){
			r = ((((u_int32_t *)image->data)[ i*width + j]) & r_mask ) >>r_offset;
			g = ((((u_int32_t *)image->data)[ i*width + j]) & g_mask ) >>g_offset;
			b = ((((u_int32_t *)image->data)[ i*width + j]) & b_mask ) >>b_offset;
			r = (r*(255 - pixel[3]) + (pixel[0] * pixel[3])>>r_limit)/255;
			g = (g*(255 - pixel[3]) + (pixel[1] * pixel[3])>>g_limit)/255;
			b = (b*(255 - pixel[3]) + (pixel[2] * pixel[3])>>b_limit)/255;
			((u_int32_t *)image->data)[i*width +j ] = 
				((r <<r_offset ) & r_mask) |
				((g <<g_offset ) & g_mask) |
				((b <<b_offset ) & b_mask);
			pixel +=bytes_per_pixel;
		}
		line += rowstride;
	}
	XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
		   gdk_pixbuf_get_width( pixbuf),gdk_pixbuf_get_height( pixbuf));
	XFree(vinfolist);
	XDestroyImage( image);
}

static void
compose_to_pixmap_24t(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf,
	Pixmap pixmap){

	XImage * image;

	unsigned int i, j;
	unsigned int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;
	
	int r_offset, g_offset, b_offset;	
	long r_mask, g_mask, b_mask;
	long r, g, b;
	int matched;
	u_int32_t * data;
	XVisualInfo *vinfolist;
	XVisualInfo vinfo;

	if (!pixbuf)
		return;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap);
	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display , screen));
	if (!vinfo.visualid)
		return;       
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched);
	if ( (!matched) || (!vinfolist) ){
		XDestroyImage( image);
		return;
	}
	r_mask = vinfolist[0].red_mask;
	g_mask = vinfolist[0].green_mask;
	b_mask = vinfolist[0].blue_mask;
	r_offset = lsb( r_mask);
	g_offset = lsb( g_mask);
	b_offset = lsb( b_mask);
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3;
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
       	line = gdk_pixbuf_get_pixels (pixbuf);
	data = (u_int32_t *)image->data;
	for( i = 0; i < height; i++){
		pixel = line;
		for( j = 0; j < width; j++){
			r = ((*data) & r_mask ) >>r_offset;
			g = ((*data) & g_mask ) >>g_offset;
			b = ((*data) & b_mask ) >>b_offset;
			r = (r*(256 - pixel[3]) + pixel[0] * pixel[3])>>8;
			g = (g*(256 - pixel[3]) + pixel[1] * pixel[3])>>8;
			b = (b*(256 - pixel[3]) + pixel[2] * pixel[3])>>8;
			*data = 
				((r <<r_offset ) & r_mask) |
				((g <<g_offset ) & g_mask) |
				((b <<b_offset ) & b_mask);
			data++;
			pixel +=bytes_per_pixel;
		}
		line += rowstride;
	}
	XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
		   gdk_pixbuf_get_width( pixbuf),gdk_pixbuf_get_height( pixbuf));
	XFree(vinfolist);
	XDestroyImage( image);
}

static void
compose_to_pixmap(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf ,
	Pixmap pixmap){

	switch (DefaultDepth( display , screen)){
		
	case 1:
		/* XXX not yet supported */
		break;
	case 8:
		/* XXX not yet supported */
		break;
	case 15:
	case 16:
		break;
	case 24:
		/*FALL THROUGH*/
	case 32:
		compose_to_pixmap_24t(
			display,
			screen,
			pixbuf ,
			pixmap);
		break;
	default:
	}
}

static void
pixbuf_to_pixmap_and_mask(
	Display * display,
	int screen,
	GdkPixbuf * pixbuf,
	Pixmap pixmap,
	Pixmap mask){

	GC gc;
	XGCValues gcv;	
	int i, j;
	int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;
	
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	pixbuf_to_pixmap( display, screen, pixbuf, pixmap);

	gc = XCreateGC (display, mask, 0, &gcv);
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3;
	if (bytes_per_pixel == 3){ /* no mask */
		XSetForeground( display, gc, 1);
		XFillRectangle( display, mask, gc,
				0, 0, width, height);
		
	}else{
		XSetForeground( display, gc, 0);
		XFillRectangle( display, mask, gc,
				0, 0, width, height);
		XSetForeground( display, gc, 1);
		line = gdk_pixbuf_get_pixels (pixbuf);
		rowstride = gdk_pixbuf_get_rowstride (pixbuf);
		for (i = 0; i < height; i++) {
			pixel = line;
       			line += rowstride;
			for (j = 0; j < width; j++) {
				if( pixel[3] > 127)
					XDrawPoint( display, mask, gc, j, i);
				pixel += 4;
			}
		}
	}
	XFreeGC( display, gc);

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
static void
gamma_cache_refresh(int gamma){
	int i;
	double real_gamma;

	if( gamma == gamma_cache[256])
		return;
	memset(gamma_cache, 0, sizeof(gamma_cache));
	gamma_cache[256] = gamma %256; /* corride will never happen actually */
	
	real_gamma = (double)gamma / 100;
	i = 128;
	while( i > 0){
		gamma_cache[i] = 255 * pow((double)i/255, real_gamma);
		if (gamma_cache[i--] == 0)
			break;
	}
	while( i >= 0)
		gamma_cache[i--] = 0;	
	i = 128;
	while( i < 254){
		gamma_cache[i] = 255 * pow((double)i / 255, real_gamma);
		if (gamma_cache[i++] == 255)				
			break;
	}
	while( i <= 255)
		gamma_cache[i++] = 255;
}

/** modify GdkPixbuf according to pic_mod
 * GdkPixbuf is a client side resource and it's not efficient
 * to render an GdkPixbuf to Pixmap and convert Pixmap to XImage
 * and edit XImage's data and XPutImage() them.
 */

static int
modify_image( GdkPixbuf * pixbuf, x_picture_modifier_t * pic_mod){
	int i, j;
	int width, height, rowstride, bytes_per_pixel;
	unsigned char *line;
	unsigned char *pixel;

	if ( !pixbuf )
			return 0;
	bytes_per_pixel = (gdk_pixbuf_get_has_alpha( pixbuf)) ? 4:3;
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	if( !pic_mod)
		return 1;
	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return 1;
	
       	line = gdk_pixbuf_get_pixels (pixbuf);
	for (i = 0; i < height; i++) {
		pixel = line;
		line += rowstride;
		
		for (j = 0; j < width; j++) {
/* XXX keeps neither hue nor saturation. MUST be replaced using better color model(CIE Yxy? lab?)*/
				pixel[0] = modify_color(pixel[0], pic_mod);
				pixel[1] = modify_color(pixel[1], pic_mod);
				pixel[2] = modify_color(pixel[2], pic_mod);
			pixel += bytes_per_pixel;
		}
	}

	if( pic_mod->gamma != 100){
		gamma_cache_refresh( pic_mod->gamma);
		line = gdk_pixbuf_get_pixels (pixbuf);
		for (i = 0; i < height; i++) {
			pixel = line;
			line += rowstride;
			for (j = 0; j < width; j++) {				
				pixel[0] = gamma_cache[pixel[0]];
				pixel[1] = gamma_cache[pixel[1]];
				pixel[2] = gamma_cache[pixel[2]];
				/* alpha is not changed */
				pixel += bytes_per_pixel;
			}
		}
	}
	return 1;
}

static int
modify_pixmap( Display * display, int screen, Pixmap pixmap, x_picture_modifier_t * pic_mod){
	int i, j;
	int width, height;
	int border, depth, x, y;
	Window root;
	XImage * image;
	unsigned char r,g,b;

	int r_offset, g_offset, b_offset;
	int r_mask, g_mask, b_mask;
	int r_limit, g_limit, b_limit;
	int matched;
	XVisualInfo *vinfolist;
	XVisualInfo vinfo;
	unsigned long data;

	if ( !pixmap )
		return 0;

	if(pic_mod->brightness == 100 && pic_mod->contrast == 100 && pic_mod->gamma == 100)
		return 0;
	XGetGeometry( display, pixmap, &root, &x, &y,
		      &width, &height, &border, &depth);

	image = XGetImage( display, pixmap, 0, 0, width, height, AllPlanes, ZPixmap);

	vinfo.visualid = XVisualIDFromVisual( DefaultVisual( display , screen));
	vinfolist = XGetVisualInfo( display, VisualIDMask, &vinfo, &matched);

	r_mask = vinfolist[0].red_mask;
	g_mask = vinfolist[0].green_mask;
	b_mask = vinfolist[0].blue_mask;
	r_offset = lsb( r_mask);
	g_offset = lsb( g_mask);
	b_offset = lsb( b_mask);
	r_limit = 8 + r_offset - msb( r_mask);
	g_limit = 8 + g_offset - msb( g_mask);
	b_limit = 8 + b_offset - msb( b_mask);
	XFree(vinfolist);
	switch( depth){
	case 1:
		/* XXX not yet supported */
		break;
	case 8:
		/* XXX not yet supported */
		break;
	case 15:
	case 16:
		if (pic_mod->gamma == 100){
			for (i = 0; i < height; i++) {
				for (j = 0; j < width; j++) {	
					data = ((u_int16_t *)(image->data))[i*width+j];
					r = ((data & r_mask) >> r_offset)<<r_limit;
					g = ((data & g_mask) >> g_offset)<<g_limit;
					b = ((data & b_mask) >> b_offset)<<b_limit;
					
					r = modify_color( r, pic_mod);
					g = modify_color( g, pic_mod);
					b = modify_color( b, pic_mod);
					
					data = (r >> r_limit) << r_offset |
						(g >> g_limit)<< g_offset |
						(b >> b_limit)<< b_offset;
					((u_int16_t *)(image->data))[i*width+j] = data;
				}
			}
		}else{
			gamma_cache_refresh( pic_mod->gamma);
			for (i = 0; i < height; i++) {
				for (j = 0; j < width; j++) {
					data = ((u_int16_t *)(image->data))[i*width+j];
					r = ((data & r_mask) >> r_offset)<<r_limit;
					g = ((data & g_mask) >> g_offset)<<g_limit;
					b = ((data & b_mask) >> b_offset)<<b_limit;
					
					r = gamma_cache[modify_color( r, pic_mod)];
					g = gamma_cache[modify_color( g, pic_mod)];
					b = gamma_cache[modify_color( b, pic_mod)];
					
					data = (r >> r_limit) << r_offset |
						(g >> g_limit)<< g_offset |
						(b >> b_limit)<< b_offset;
					((u_int16_t *)(image->data))[i*width+j] = data;
				}
			}
		}
		XPutImage( display, pixmap, DefaultGC( display, screen),
			   image, 0, 0, 0, 0, width, height);
		break;
	case 24:
	case 32:
		if (pic_mod->gamma == 100){
			for (i = 0; i < height; i++) {
				for (j = 0; j < width; j++) {	
					data = ((u_int32_t *)(image->data))[i*width+j];
					r = (data & r_mask) >> r_offset;
					g = (data & g_mask) >> g_offset;
					b = (data & b_mask) >> b_offset;
					
					r = modify_color( r, pic_mod);
					g = modify_color( g, pic_mod);
					b = modify_color( b, pic_mod);
					
					data = r << r_offset | g << g_offset | b << b_offset;
					((u_int32_t *)(image->data))[i*width+j] = data;
				}
			}
		}else{
			gamma_cache_refresh( pic_mod->gamma);
			for (i = 0; i < height; i++) {
				for (j = 0; j < width; j++) {
					data = ((u_int32_t *)(image->data))[i*width+j];
					r = (data & r_mask) >> r_offset;
					g = (data & g_mask) >> g_offset;
					b = (data & b_mask) >> b_offset;
					
					r = gamma_cache[modify_color( r, pic_mod)];
					g = gamma_cache[modify_color( g, pic_mod)];
					b = gamma_cache[modify_color( b, pic_mod)];
					
					data = r << r_offset | g << g_offset | b << b_offset;
					((u_int32_t *)(image->data))[i*width+j] = data;
				}
			}
		}
		XPutImage( display, pixmap, DefaultGC( display, screen), image, 0, 0, 0, 0,
			   width, height);
		break;
	default:
	}
	XDestroyImage( image);
	return 1;
}

/* --- global functions --- */

int
x_imagelib_display_opened( Display * display){
	int i;
	if (display_count == 0){
#ifndef OLD_GDK_PIXBUF
		g_type_init();
#endif
		for (i = 0; i < 256; i++){
			gamma_cache[i] = i;
		}
		gamma_cache[256] = 100;
	}
	display_count ++;
	return 1;
}

int
x_imagelib_display_closed( Display * display){
	display_count --;
	cache_delete( display);
	return 1;
}

/** Load an image from the specified file. 
 *\param win mlterm window.
 *\param path File full path.
 *\param pic_mod picture modifier.
 *
 *\return Pixmap to be used as a window's background.
 */
Pixmap
x_imagelib_load_file_for_background( x_window_t * win , char * file_path , x_picture_modifier_t * pic_mod){
	GdkPixbuf * pixbuf;
	Pixmap pixmap;
	GC gc;
	if(!file_path)
		return None;
	if( wp_cache_name && (strcmp( wp_cache_name, file_path) == 0) && wp_cache_data ){		
		pixbuf = wp_cache_data;
	}else{
		if ( wp_cache_data){
			gdk_pixbuf_unref( wp_cache_data );
			wp_cache_data = NULL;
		}
		if ( wp_cache_scaled){
			gdk_pixbuf_unref( wp_cache_scaled );
			wp_cache_scaled = NULL;
		}
		if ( wp_cache_name){
			free( wp_cache_name);
			wp_cache_name = NULL;
		}
#ifndef OLD_GDK_PIXBUF
		if( ( pixbuf = gdk_pixbuf_new_from_file( file_path , NULL )) == NULL)
			return None;
#else
		if( ( pixbuf = gdk_pixbuf_new_from_file( file_path )) == NULL)
			return None;
#endif /*OLD_GDK_PIXBUF*/
		wp_cache_data = pixbuf;
		wp_cache_name = strdup( file_path);

		wp_cache_height = 0;
		wp_cache_width = 0;
	}
	
	pixmap = XCreatePixmap( win->display, win->my_window,
				ACTUAL_WIDTH(win), ACTUAL_HEIGHT(win),
				DefaultDepth( win->display , win->screen));		

	if (wp_cache_width != ACTUAL_WIDTH(win) || ACTUAL_HEIGHT(win) != wp_cache_height){
		/* it's new pixbuf */
		pixbuf = gdk_pixbuf_scale_simple(pixbuf, ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
						 GDK_INTERP_TILES); /* use one of _NEAREST, _TILES, _BILINEAR, _HYPER (speed<->quality) */
		
		if( pic_mod)
			modify_image( pixbuf, pic_mod);
		
		wp_cache_scaled = pixbuf;
		wp_cache_width = ACTUAL_WIDTH(win);
		wp_cache_height = ACTUAL_HEIGHT(win);
	}else
		pixbuf = wp_cache_scaled;
	
	if( gdk_pixbuf_get_has_alpha ( pixbuf) ){
		pixmap = x_imagelib_get_transparent_background( win , NULL);
		compose_to_pixmap( win->display,
				   win->screen,
				   pixbuf,
				   pixmap);
	}else{
		pixbuf_to_pixmap( win->display,
				  win->screen,
				  pixbuf,
				  pixmap);
	}
	return pixmap;
}

/** Answer whether pseudo transparency is available
 *\param display connection to X server.
 * 
 *\return Success => 1, Failure => 0
 */
int
x_imagelib_root_pixmap_available( Display * display){
	if( XInternAtom( display , "_XROOTPMAP_ID" , True))
		return 1;
	return 0;
}

/** Create an pixmap from root window
 *\param win window structure
 *\param pic_mod picture modifier
 * 
 *\return Newly allocated Pixmap (or None in the case of failure)
 */
Pixmap
x_imagelib_get_transparent_background( x_window_t * win , x_picture_modifier_t * pic_mod){
	int x;
	int y;
	int pix_x;
	int pix_y;
	u_int width;
	u_int height;
	Window src;
	Atom id;
	display_store_t * cache;
	struct timeval tval;
	struct timezone tz;
	Pixmap pixmap;
	GC gc;
	cache = display_store;	
	while( cache){
		if (cache->display == win->display)
			break;
		cache = cache->next;
	}
	if (!cache){
		cache = malloc( sizeof( display_store_t));
		if (!cache)
			return None;
		cache->display = win->display;
		cache->root = None;
		cache->cooked = None;
		memset( &(cache->pic_mod), 0, sizeof(x_picture_modifier_t));
		cache->tval.tv_sec = 0;
		cache->tval.tv_usec = 0;
		cache->next = display_store;
		display_store = cache;
	}
	
/* discard old cached root pixmap */
	gettimeofday(&tval, &tz);
	if ( (tval.tv_sec - cache->tval.tv_sec) > 500){
		if (cache->root != None)
			cache->root = None; /* pixmap should not be freed. (not owned by mlterm)*/
		if (cache->cooked != None){
			XFreePixmap( win->display, cache->cooked);
			cache->cooked = None;
		}
		cache->tval.tv_sec = tval.tv_sec;
	}
	if( ! x_window_get_visible_geometry( win , &x , &y , &pix_x , &pix_y , &width , &height))
		return None;
	if (cache->root == None){
		/* Get background pixmap from _XROOTMAP_ID */
		id = XInternAtom( win->display , "_XROOTPMAP_ID" , True);
		if( id){
			Atom act_type;
			int act_format;
			u_long nitems;
			u_long bytes_after;
			u_char * prop;
			if( XGetWindowProperty( win->display , DefaultRootWindow(win->display) , id , 0 , 1 ,
						False , XA_PIXMAP , &act_type , &act_format ,
						&nitems , &bytes_after , &prop) == Success){
				if( prop){
					cache->root = *((Drawable *)prop);
					XFree( prop);
				}
			}
		}
	}
	pixmap = XCreatePixmap( win->display , win->my_window , ACTUAL_WIDTH(win) , ACTUAL_HEIGHT(win) ,
				DefaultDepth( win->display , win->screen));
	gc = XCreateGC( win->display, win->my_window, 0, 0 );	
	if (pic_mod){
		if ( cache->cooked == None ){
			cache->cooked = XCreatePixmap( win->display , win->my_window ,
						       DisplayWidth( win->display, win->screen) ,
						       DisplayHeight( win->display, win->screen),
						       DefaultDepth( win->display , win->screen));
			XCopyArea( win->display, cache->root, pixmap, gc, 0, 0,
				   DisplayWidth( win->display, win->screen) ,
				   DisplayHeight( win->display, win->screen),
				   0, 0);
		}
		if ( !is_picmod_eq( &(cache->pic_mod), pic_mod)){
			memcpy( &(cache->pic_mod), pic_mod, sizeof(x_picture_modifier_t));
			XCopyArea( win->display, cache->root, cache->cooked, gc, 0, 0,
				   DisplayWidth( win->display, win->screen) ,
				   DisplayHeight( win->display, win->screen),
				   0, 0);
			modify_pixmap( win->display, win->screen, cache->cooked , pic_mod);
		}
		XCopyArea( win->display, cache->cooked, pixmap, gc, x, y, width , height, pix_x, pix_y);
	}else{
		XCopyArea( win->display, cache->root, pixmap, gc, x, y, width , height, pix_x, pix_y);
	}
	XFreeGC( win->display , gc );
	return pixmap;
}

/** Load an image from the specified file with alpha plane and returns as pixmap and mask.
 *\param display connection to X server.
 *\param path File full path.
 *\param cardinal Returns pointer to a data structure for the extended WM hint spec.
 *\param pixmap Returns image pixmap for the old WM hint.
 *\param mask Returns mask bitmap for the old WM hint. 
 * 
 *\return Success => 1, Failure => 0
 */
int x_imagelib_load_file(
	Display * display,
	char * path,
	u_int32_t **cardinal,
	Pixmap *pixmap,
	Pixmap *mask)
{
	GdkPixbuf * pixbuf;
	int width, height, rowstride, bytes_per_pixel;
#ifndef OLD_GDK_PIXBUF
	pixbuf = gdk_pixbuf_new_from_file( path , NULL );
#else
	pixbuf = gdk_pixbuf_new_from_file( path );
#endif /*OLD_GDK_PIXBUF*/
	if ( !pixbuf )
		return 0;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	if ( cardinal){
		unsigned char *line;
		unsigned char *pixel;
		int i, j;

/* create CARDINAL array for_NET_WM_ICON data */
		bytes_per_pixel = gdk_pixbuf_get_has_alpha (pixbuf) ? 4 : 3;
		rowstride = gdk_pixbuf_get_rowstride (pixbuf);
		line = gdk_pixbuf_get_pixels (pixbuf);
		*cardinal = malloc((width * height + 2) *4);
		if (!(*cardinal))
			return 0;
			
/* {width, height, ARGB[][]} */
		(*cardinal)[0] = width;
		(*cardinal)[1] = height;
		if (bytes_per_pixel == 4){ /* alpha support (convert to ARGB format)*/
			for (i = 0; i < height; i++) {
				pixel = line;
				line += rowstride;
				
				for (j = 0; j < width; j++) {
					(*cardinal)[(i*width+j)+2] = ((((((u_int32_t)(pixel[3]) << 8) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
					pixel += bytes_per_pixel;
				}
			}
		}else{
			for (i = 0; i < height; i++) {
				pixel = line;
				line += rowstride;
				
				for (j = 0; j < width; j++) {
					(*cardinal)[(i*width+j)+2] = ((((((u_int32_t)(0x0000FF) <<8 ) + pixel[0]) << 8) + pixel[1]) << 8) + pixel[2];
					pixel += bytes_per_pixel;
				}
			}
		}
	}
/* Create Icon pixmap&mask for WMHints. None as result is acceptable.*/
	if (pixmap && mask){
		*pixmap = XCreatePixmap( display , DefaultRootWindow( display) , width , height,
					DefaultDepth( display , DefaultScreen( display)));
		*mask = XCreatePixmap( display , DefaultRootWindow( display) , width, height,
					1);
		pixbuf_to_pixmap_and_mask( display , DefaultScreen( display), pixbuf, *pixmap, *mask);
	}

	gdk_pixbuf_unref(pixbuf);
	return 1;
}
