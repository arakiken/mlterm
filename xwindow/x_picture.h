/*
 *	$Id$
 */

#ifndef  __X_PICTURE_H__
#define  __X_PICTURE_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>		/* u_int8_t */


/* stub */
typedef struct x_window *  x_window_ptr_t ;

typedef struct x_picture_modifier
{
	u_int16_t  brightness ;		/* 0 - 65536 */
	u_int16_t  contrast ;		/* 0 - 65536 */
	u_int16_t  gamma ;		/* 0 - 65536 */
	
} x_picture_modifier_t ;

typedef struct x_picture
{
	x_window_ptr_t  win ;
	Pixmap  pixmap ;
	x_picture_modifier_t *  mod ;
	
} x_picture_t ;


int  x_picture_display_opened( Display *  display) ;

int  x_picture_display_closed( Display *  display) ;

int  x_picture_init( x_picture_t *  pic , x_window_ptr_t  win , x_picture_modifier_t *  mod) ;

int  x_picture_final( x_picture_t *  pic) ;

int  x_picture_load_file( x_picture_t *  pic , char *  file_path) ;

int  x_root_pixmap_available( Display *  display) ;

int  x_picture_load_background( x_picture_t *  pic) ;

int  x_picture_modifier_is_normal( x_picture_modifier_t *  pic_mod) ;


#endif
