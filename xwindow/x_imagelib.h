/*
 *	$Id$
 */

#ifndef  __X_IMAGELIB_H__
#define  __X_IMAGELIB_H__


#include  "x_window.h"
#include  "x_picture.h"


int  x_imagelib_display_opened( Display *  disp) ;

int  x_imagelib_display_closed( Display *  disp) ;

int  x_imagelib_root_pixmap_available( Display *  disp) ;

Pixmap  x_imagelib_load_file_for_background( x_window_t *  win , char *  file_path , x_picture_modifier_t *  pic_mod) ;

Pixmap  x_imagelib_get_transparent_background( x_window_t *  win , x_picture_modifier_t *  pic_mod) ;

int  x_imagelib_load_file( Display *  display, char *  path, u_int32_t **  cardinal, Pixmap *  pixmap, Pixmap *  mask,
			   int *  width, int *   height) ;


#endif
