/*
 *	$Id$
 */

#include  "../x_color.h"

#include  <string.h>		/* strcmp */
#include  <ml_color.h>


#define  BYTE_COLOR_TO_WORD(color)  ((color) << 8 | (color))
#define  WORD_COLOR_TO_BYTE(color)  ((color) & 0xff)


/* --- static variables --- */

static struct fb_cmap *  cmap ;


/* --- static functions --- */

static int
cmap_init(
	Display *  display
	)
{
	ml_color_t  color ;
	u_int8_t  r ;
	u_int8_t  g ;
	u_int8_t  b ;

	/* XXX cmap and its members are not free'ed explicitly. */

	if( ! ( cmap = malloc( sizeof(*cmap) + sizeof(__u16) * 256 * 3)))
	{
		return  0 ;
	}

	cmap->red = (__u16*)(cmap + 1) ;
	cmap->green = cmap->red + 256 ;
	cmap->blue = cmap->green + 256 ;
	cmap->transp = NULL ;
	cmap->start = 0 ;
	cmap->len = 256 ;

	for( color = 0 ; color < 256 ; color ++)
	{
		ml_get_color_rgb( color , &r , &g , &b) ;

		cmap->red[color] = BYTE_COLOR_TO_WORD(r) ;
		cmap->blue[color] = BYTE_COLOR_TO_WORD(g) ;
		cmap->green[color] = BYTE_COLOR_TO_WORD(b) ;
	}

	ioctl( display->fb_fd , FBIOPUTCMAP , cmap) ;

	return  1 ;
}

/* seek the closest color */
static  ml_color_t
closest_color(
	int  red ,
	int  green ,
	int  blue
	)
{
	ml_color_t  closest = 0 ;
	ml_color_t  color ;
	u_long  min = 0xffffff ;
	u_long  diff ;
	int  diff_r , diff_g , diff_b ;

	for( color = 0 ; color < 256 ; color++)
	{
		/* lazy color-space conversion*/
		diff_r = red - WORD_COLOR_TO_BYTE(cmap->red[color]) ;
		diff_g = green - WORD_COLOR_TO_BYTE(cmap->green[color]) ;
		diff_b = blue - WORD_COLOR_TO_BYTE(cmap->blue[color]) ;
		diff = diff_r * diff_r *9 + diff_g * diff_g * 30 + diff_b * diff_b ;
		if ( diff < min)
		{
			min = diff ;
			closest = color ;
			/* no one may notice the difference */
			if ( diff < 31)
			{
				break ;
			}
		}
	}

	return  closest ;
}


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

	if( disp->depth == 8)
	{
		if( ( ! cmap && ! cmap_init( disp->display)) ||
		    ( color = ml_get_color( name)) == ML_UNKNOWN_COLOR)
		{
			return  0 ;
		}

		xcolor->pixel = color ;
		xcolor->red = WORD_COLOR_TO_BYTE(cmap->red[xcolor->pixel]) ;
		xcolor->green = WORD_COLOR_TO_BYTE(cmap->green[xcolor->pixel]) ;
		xcolor->blue = WORD_COLOR_TO_BYTE(cmap->blue[xcolor->pixel]) ;

		return  1 ;
	}
	else if( ( color = ml_get_color( name)) == ML_UNKNOWN_COLOR ||
		! ml_get_color_rgb( color , &red , &green , &blue))
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
	if( disp->depth < 8)
	{
		return  0 ;
	}
	else if( disp->depth == 8)
	{
		if( ! cmap && ! cmap_init( disp->display))
		{
			return  0 ;
		}

		xcolor->pixel = closest_color( red , green , blue) ;
		xcolor->red = WORD_COLOR_TO_BYTE(cmap->red[xcolor->pixel]) ;
		xcolor->green = WORD_COLOR_TO_BYTE(cmap->green[xcolor->pixel]) ;
		xcolor->blue = WORD_COLOR_TO_BYTE(cmap->blue[xcolor->pixel]) ;
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
x_get_xcolor_rgb(
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

	x_get_xcolor_rgb( &red , &green , &blue , &alpha , xcolor) ;

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
