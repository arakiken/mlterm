/*
 *	$Id$
 */

#ifndef  __X_PICTURE_H__
#define  __X_PICTURE_H__


#include  <kiklib/kik_types.h>		/* u_int16_t */

#include  "x.h"				/* XA_PIXMAP */
#include  "x_window.h"


typedef struct x_picture_modifier
{
	u_int16_t  brightness ;		/* 0 - 65536 */
	u_int16_t  contrast ;		/* 0 - 65536 */
	u_int16_t  gamma ;		/* 0 - 65536 */

	u_long  blend_color ;		/* Not used */
	u_int8_t  alpha ;		/* Used only in win32. */
	
} x_picture_modifier_t ;

typedef struct x_bg_picture
{
	x_window_t *  win ;
	x_picture_modifier_t *  mod ;
	
	Pixmap  pixmap ;

} x_bg_picture_t ;

typedef struct x_icon_picture
{
	Display *  display ;
	char *  file_path ;
	
	Pixmap  pixmap ;
	Pixmap  mask ;
	u_int32_t *  cardinal ;

	u_int  ref_count ;
	
} x_icon_picture_t ;


int  x_picture_display_opened( Display *  display) ;

int  x_picture_display_closed( Display *  display) ;

int  x_picture_modifier_is_normal( x_picture_modifier_t *  pic_mod) ;

int  x_root_pixmap_available( Display *  display) ;

int  x_bg_picture_init( x_bg_picture_t *  pic , x_window_t *  win , x_picture_modifier_t *  mod) ;

int  x_bg_picture_final( x_bg_picture_t *  pic) ;

int  x_bg_picture_load_file( x_bg_picture_t *  pic , char *  file_path) ;

int  x_bg_picture_get_transparency( x_bg_picture_t *  pic) ;

x_icon_picture_t *  x_acquire_icon_picture( Display *  display , char *  file_path) ;

int  x_release_icon_picture( x_icon_picture_t *  pic) ;


#endif
