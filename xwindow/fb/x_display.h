/*
 *	$Id$
 */

#ifndef  ___X_DISPLAY_H__
#define  ___X_DISPLAY_H__


#include  "../x_display.h"

#ifdef  __FreeBSD__
#include  <sys/kbio.h>	/* NLKED */
#else
#define CLKED  1
#define NLKED  2
#define SLKED  4
#define ALKED  8
#endif

#define KeyPress	2	/* Private in fb/ */
#define ButtonPress	4	/* Private in fb/ */
#define ButtonRelease	5	/* Private in fb/ */
#define MotionNotify	6	/* Private in fb/ */

#ifdef  USE_GRF
#define  TP_COLOR  12
#endif


u_long  x_display_get_pixel( int  x , int  y) ;

void  x_display_put_image( int  x , int  y , u_char *  image , size_t  size , int  need_fb_pixel) ;

void  x_display_fill_with( int  x , int  y , u_int  width , u_int  height , u_int8_t  pixel) ;

void  x_display_copy_lines( int  src_x , int  src_y ,
		int  dst_x , int  dst_y , u_int  width , u_int  height) ;

int  x_display_check_visibility_of_im_window( void) ;

int  x_cmap_get_closest_color( u_long *  closest , int  red , int  green , int  blue) ;

int  x_cmap_get_pixel_rgb( u_int8_t *  red , u_int8_t *  green ,
	u_int8_t *  blue , u_long  pixel) ;

#ifdef  USE_GRF
int  x68k_tvram_is_enabled(void) ;

void  x68k_set_use_tvram_colors( int  use) ;

void  x68k_set_tvram_cmap( u_int32_t *  cmap , u_int  cmap_size) ;

int  x68k_tvram_set_wall_picture( u_short *  image , u_int  width , u_int  height) ;
#endif


#endif
