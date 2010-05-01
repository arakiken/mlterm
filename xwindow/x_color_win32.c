/*
 *	$Id$
 */

#include  "x_color.h"

#include  <string.h>		/* memcpy,strcmp */
#include  <stdio.h>		/* sscanf */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  <ml_color.h>


/* --- global functions --- */

int
x_load_named_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	char *  name
	)
{
	ml_color_t  color ;
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;

	if( ml_color_parse_rgb_name( &red , &green , &blue , name))
	{
		return  x_load_rgb_xcolor( display , screen , xcolor , red , green , blue) ;
	}

	if( ( color = ml_get_color( name)) == ML_UNKNOWN_COLOR ||
		! ml_get_color_rgb( color, &red, &green, &blue))
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

	return  x_load_rgb_xcolor( display, screen, xcolor, red, green, blue) ;
}

int
x_load_rgb_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	u_int8_t  red ,
	u_int8_t  green ,
	u_int8_t  blue
	)
{
	xcolor->pixel = RGB(red,green,blue) ;

	return  1 ;
}

int
x_unload_xcolor(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor
	)
{
	return  1 ;
}

int
x_get_xcolor_rgb(
	u_int8_t *  red ,
	u_int8_t *  green ,
	u_int8_t *  blue ,
	x_color_t *  xcolor
	)
{
	*red = GetRValue( xcolor->pixel) ;
	*green = GetGValue( xcolor->pixel) ;
	*blue = GetBValue( xcolor->pixel) ;

	return  1 ;
}

int
x_xcolor_fade(
	Display *  display ,
	int  screen ,
	x_color_t *  xcolor ,
	u_int  fade_ratio
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	
	x_get_xcolor_rgb( &red , &green , &blue , xcolor) ;

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	x_unload_xcolor( display , screen , xcolor) ;

	return  x_load_rgb_xcolor( display , screen , xcolor , red , green , blue) ;
}
