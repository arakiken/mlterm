/*
 *	$Id$
 */

#ifndef  __X_PICTURE_DEP_H__
#define  __X_PICTURE_DEP_H__


#include  "x_window.h"
#include  "x_picture.h"


int  x_picdep_display_opened( Display *  disp) ;

int  x_picdep_display_closed( Display *  disp) ;

int  x_picdep_root_pixmap_available( Display *  disp) ;

Pixmap  x_picdep_load_file( x_window_t *  win , char *  file_path , x_picture_modifier_t *  pic_mod) ;

Pixmap  x_picdep_load_background( x_window_t *  win , x_picture_modifier_t *  pic_mod) ;

int  x_picdep_load_icon( x_window_ptr_t * win, char * path, u_int32_t **cardinal, Pixmap *pixmap, Pixmap *mask) ;


#endif
