/*
 *	$Id$
 */

#include  "../x_color.h"

#include  <string.h>		/* memcpy,strcmp */
#include  <stdio.h>		/* sscanf */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  <ml_color.h>


/* --- global functions --- */

int
x_load_named_xcolor(
	x_display_t *  disp ,
	x_color_t *  xcolor ,
	char *  name
	)
{
	ml_color_t  color ;
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;

	if( ml_color_parse_rgb_name( &red , &green , &blue , &alpha , name))
	{
		return  x_load_rgb_xcolor( disp , xcolor , red , green , blue , alpha) ;
	}

	if( ( color = ml_get_color( name)) != ML_UNKNOWN_COLOR &&
	    IS_VTSYS_BASE_COLOR(color))
	{
		/*
		 * 0 : 0x00, 0x00, 0x00
		 * 1 : 0xff, 0x00, 0x00
		 * 2 : 0x00, 0xff, 0x00
		 * 3 : 0xff, 0xff, 0x00
		 * 4 : 0x00, 0x00, 0xff
		 * 5 : 0xff, 0x00, 0xff
		 * 6 : 0x00, 0xff, 0xff
		 * 7 : 0xe5, 0xe5, 0xe5
		 */
		red = (color & 0x1) ? 0xff : 0 ;
		green = (color & 0x2) ? 0xff : 0 ;
		blue = (color & 0x4) ? 0xff : 0 ;
	}
	else
	{
		if( strcmp( name , "gray") == 0)
		{
			red = green = blue = 190 ;
		}
		else if( strcmp( name , "lightgray") == 0)
		{
			red = green = blue = 211 ;
		}
		else
		{
			return  0 ;
		}
	}

	return  x_load_rgb_xcolor( disp , xcolor , red , green , blue , 0xff) ;
}

int
x_load_rgb_xcolor(
	x_display_t *  disp ,
	x_color_t *  xcolor ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue ,
	u_int8_t  alpha
	)
{
	xcolor->pixel = (red << 16) | (green << 8) | blue | (alpha << 24) ;

	return  1 ;
}

int
x_unload_xcolor(
	x_display_t *  disp ,
	x_color_t *  xcolor
	)
{
	return  1 ;
}

int
x_get_xcolor_rgba(
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	u_int8_t *  alpha ,	/* can be NULL */
	x_color_t *  xcolor
	)
{
	if( alpha)
	{
		*alpha = (xcolor->pixel >> 24) & 0xff ;
	}

	*red = (xcolor->pixel >> 16) & 0xff ;
	*green = (xcolor->pixel >> 8) & 0xff ;
	*blue = xcolor->pixel & 0xff ;

	return  0 ;
}

int
x_xcolor_fade(
	x_display_t *  disp ,
	x_color_t *  xcolor ,
	u_int  fade_ratio
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	u_int8_t  alpha ;

	x_get_xcolor_rgba( &red , &green , &blue , &alpha , xcolor) ;

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	x_unload_xcolor( disp , xcolor) ;

	return  x_load_rgb_xcolor( disp , xcolor , red , green , blue , alpha) ;
}
