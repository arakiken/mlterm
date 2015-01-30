/*
 *	$Id$
 */

#ifndef  ___X_DISPLAY_H__
#define  ___X_DISPLAY_H__


#include  <kiklib/kik_types.h>

#include  "../x_display.h"


#define CLKED  1
#define NLKED  2
#define SLKED  4
#define ALKED  8

#define KeyPress			2	/* Private in fb/ */
#define ButtonPress			4	/* Private in fb/ */
#define ButtonRelease			5	/* Private in fb/ */
#define MotionNotify			6	/* Private in fb/ */


int  x_display_init( struct android_app *  app) ;

void  x_display_final(void) ;

int  x_display_process_event( struct android_poll_source *  source , int  ident) ;

void  x_display_unlock(void) ;

size_t  x_display_get_str( u_char *  seq , size_t  seq_len) ;

u_long  x_display_get_pixel( int  x , int  y) ;

void  x_display_put_image( int  x , int  y , u_char *  image , size_t  size , int  need_fb_pixel) ;

void  x_display_fill_with( int  x , int  y , u_int  width , u_int  height , u_int8_t  pixel) ;

void  x_display_copy_lines( int  src_x , int  src_y ,
		int  dst_x , int  dst_y , u_int  width , u_int  height) ;

int  x_display_check_visibility_of_im_window( void) ;

int  x_cmap_get_closest_color( u_long *  closest , int  red , int  green , int  blue) ;

int  x_cmap_get_pixel_rgb( u_int8_t *  red , u_int8_t *  green ,
	u_int8_t *  blue , u_long  pixel) ;

u_char *  x_display_get_bitmap( char *  path , u_int *  width , u_int *  height) ;

void  x_display_resize( int  yoffset , int  width , int  height) ;


#endif
