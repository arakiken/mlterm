/*
 *	$Id$
 */

#ifndef  ___X_DISPLAY_H__


#include  "../x_display.h"


#define KeyPress			2	/* Private in fb/ */
#define ButtonPress			4	/* Private in fb/ */
#define ButtonRelease			5	/* Private in fb/ */
#define MotionNotify			6	/* Private in fb/ */


u_char *  x_display_get_fb( Display *  display , int  x , int  y) ;

void  x_display_put_image( Display *  display , int  x , int  y , u_char *  image , size_t  size) ;

void  x_display_copy_line( Display *  display , int  src_x , int  src_y ,
		int  dst_x , int  dst_y , u_int  width) ;


#endif