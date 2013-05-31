/*
 *	$Id$
 */

#ifndef  ___X_DISPLAY_H__
#define  ___X_DISPLAY_H__


#include  "../x_display.h"

#ifdef  __FreeBSD__
#include  <sys/kbio.h>	/* NLKED */
#endif

#define KeyPress			2	/* Private in fb/ */
#define ButtonPress			4	/* Private in fb/ */
#define ButtonRelease			5	/* Private in fb/ */
#define MotionNotify			6	/* Private in fb/ */


u_long  x_display_get_pixel( int  x , int  y) ;

void  x_display_put_image( int  x , int  y , u_char *  image , size_t  size , int  need_fb_pixel) ;

void  x_display_fill_with( int  x , int  y , u_int  width , u_int  height , u_int8_t  pixel) ;

void  x_display_copy_lines( int  src_x , int  src_y ,
		int  dst_x , int  dst_y , u_int  width , u_int  height) ;

int  x_display_check_visibility_of_im_window( void) ;

int  x_cmap_reset(void) ;

int  x_cmap_get_closest_color( u_long *  closest , int  red , int  green , int  blue) ;

int  x_cmap_get_pixel_rgb( u_int8_t *  red , u_int8_t *  green ,
	u_int8_t *  blue , u_long  pixel) ;


#endif
