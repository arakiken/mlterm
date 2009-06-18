/*
 *	$Id$
 */

#include  "x_color.h"

#include  <string.h>		/* memcpy,strcmp */
#include  <stdio.h>		/* sscanf */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

#include  <ml_color.h>


/* --- static functions --- */

static int
parse_rgb_color_name(
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	char *  name
	)
{
	if( *name == '#')
	{
		u_int  _red ;
		u_int  _green ;
		u_int  _blue ;

		name++;
		
		if( strlen( name) == 6 &&
			sscanf( name, "%2x%2x%2x" , &_red , &_green , &_blue) == 3)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " %x %x %x\n" , _red , _green , _blue) ;
		#endif

			*red = (_red << 8) + _red;
			*green = (_green << 8) + _green ;
			*blue = (_blue << 8) + _blue ;
			
			return  1 ;
		}
		else if( (strlen( name) == 12) &&
			(sscanf( name, "%4x%4x%4x" , &_red , &_green , &_blue) == 3) )
		{
			*red = _red ;
			*green = _green ;
			*blue = _blue ;

			return 1;
		}
	}
	
	return  0 ;
}


/* --- global functions --- */

int
x_load_named_xcolor(
	Display *  display ,
	int  screen ,
	XColor *  xcolor ,
	char *  name
	)
{
	ml_color_t  color ;
	u_short  red ;
	u_short  green ;
	u_short  blue ;

	if( parse_rgb_color_name( &red , &green , &blue , name))
	{
		return  x_load_rgb_xcolor( display , screen , xcolor , red , green , blue) ;
	}

	if( ( color = ml_get_color( name)) == ML_UNKNOWN_COLOR ||
		! ml_get_color_rgb( color, &red, &green, &blue))
	{
		return  0 ;
	}

	return  x_load_rgb_xcolor( display, screen, xcolor, red, green, blue) ;
}

int
x_load_rgb_xcolor(
	Display *  display ,
	int  screen ,
	XColor *  xcolor ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	xcolor->pixel = RGB(red,green,blue) ;

	return  1 ;
}

int
x_unload_xcolor(
	Display *  display ,
	int  screen ,
	XColor *  xcolor
	)
{
	return  1 ;
}

int
x_get_xcolor_rgb(
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	XColor *  xcolor
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
	u_short  red ;
	u_short  green ;
	u_short  blue ;
	
	x_get_xcolor_rgb( &red , &green , &blue , xcolor) ;

	red = (red * fade_ratio) / 100 ;
	green = (green * fade_ratio) / 100 ;
	blue = (blue * fade_ratio) / 100 ;

	x_unload_xcolor( display , screen , xcolor) ;

	return  x_load_rgb_xcolor( display , screen , xcolor , red , green , blue) ;
}
