/*
 *	$Id$
 */

#ifndef  __X_IMAGELIB_H__
#define  __X_IMAGELIB_H__


#include  "x_window.h"
#include  "x_picture.h"


typedef struct _GdkPixbuf *  GdkPixbufPtr  ;


int  x_imagelib_display_opened( Display *  disp) ;

int  x_imagelib_display_closed( Display *  disp) ;

Pixmap  x_imagelib_load_file_for_background( x_window_t *  win ,
	char *  path , x_picture_modifier_t *  pic_mod) ;

Pixmap  x_imagelib_get_transparent_background( x_window_t *  win ,
	x_picture_modifier_t *  pic_mod) ;

int  x_imagelib_load_file( x_display_t *  disp , char *  path , u_int32_t **  cardinal ,
	Pixmap *  pixmap , PixmapMask *  mask , u_int *  width , u_int *   height) ;

Pixmap  x_imagelib_pixbuf_to_pixmap( x_window_t *  win , x_picture_modifier_t *  pic_mod ,
	GdkPixbufPtr  pixbuf) ;

int  x_delete_image( Display *  display , Pixmap  pixmap) ;

#ifdef  USE_XLIB
#define  x_delete_mask(display, mask)  ((mask) && x_delete_image(display, mask))
#else
int  x_delete_mask( Display *  display , PixmapMask  mask) ;
#endif


#endif
