/*
 *	$Id$
 */

#include  "../x_color.h"

#include  <string.h>		/* strcmp */
#include  <ml_color.h>

#include  "x_display.h"		/* CMAP_SIZE, x_cmap_get_closest_color */


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
	if( x_cmap_get_closest_color( &xcolor->pixel , red , green , blue))
	{
		x_cmap_get_pixel_rgb( &xcolor->red , &xcolor->green ,
			&xcolor->blue , xcolor->pixel) ;
	}
	else
	{
		xcolor->pixel = RGB_TO_PIXEL(red,green,blue,disp->display->rgbinfo) |
				(disp->depth == 32 ? (alpha << 24) : 0) ;

		xcolor->red = red ;
		xcolor->green = green ;
		xcolor->blue = blue ;
		xcolor->alpha = alpha ;
	}

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
	*red = xcolor->red ;
	*green = xcolor->green ;
	*blue = xcolor->blue ;

	if( alpha)
	{
		*alpha = xcolor->alpha ;
	}

	return  1 ;
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

#if  0
	kik_msg_printf( "Fading R%d G%d B%d => " , red , green , blue) ;
#endif

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	x_unload_xcolor( disp , xcolor) ;

#if  0
	kik_msg_printf( "R%d G%d B%d\n" , red , green , blue) ;
#endif

	return  x_load_rgb_xcolor( disp , xcolor , red , green , blue , alpha) ;
}
