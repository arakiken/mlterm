/*
 *	$Id$
 */

#ifndef  __EXSB_COMMON_H__
#define  __EXSB_COMMON_H__

/* If color_name is not found in default color map, returns nearest pixel */
unsigned long exsb_get_pixel( Display * display , int screen , char * color_name) ;

#endif
