/*
 *	$Id$
 */

#ifndef  __EXSB_COMMON_H__
#define  __EXSB_COMMON_H__

#include  <stdio.h>
#include  <stdlib.h>
#include  <X11/Xlib.h>

/* --- static functions --- */

static void
get_closest_xcolor_pseudo(
	Display *  display ,
	int  screen ,
	Colormap  cmap ,
	XColor *  color ,
	XColor *  closest_color
	)
{
	XColor *  all_colors ;
	int  i ;
	int  closest_index = 0 ;
	unsigned long  min = 0xffffffff ;
	unsigned long  diff ;
	unsigned long  diff_r = 0 , diff_g = 0 , diff_b = 0 ;
	int ncells = DisplayCells( display , screen) ;

	all_colors = malloc( ncells * sizeof( XColor)) ;

	/* get all colors from default colormap */
	for( i = 0 ; i < ncells ; i ++)
	{
		all_colors[i].pixel = i ;
	}
	XQueryColors( display , cmap , all_colors, ncells) ;

	/* find closest color */
	for( i = 0 ; i < ncells ; i ++)
	{
		diff_r = (color->red - all_colors[i].red) >> 8 ;
		diff_g = (color->green - all_colors[i].green) >> 8 ;
		diff_b = (color->blue - all_colors[i].blue) >> 8 ;

		diff = diff_r * diff_r + diff_g * diff_g + diff_b * diff_b ;

		if ( diff < min) /* closest ? */
		{
			min = diff ;
			closest_index = i ;
		}
	}

	closest_color->red = all_colors[closest_index].red ;
	closest_color->green = all_colors[closest_index].green ;
	closest_color->blue = all_colors[closest_index].blue ;
	closest_color->flags = DoRed | DoGreen | DoBlue;

	free( all_colors) ;

	if ( XAllocColor( display , cmap , closest_color) == 0)
	{
		closest_color->pixel = BlackPixel( display , screen) ;
	}
}

/* --- global functions -- */

static unsigned long
exsb_get_pixel(
	Display *  display ,
	int  screen ,
	Colormap  cmap ,
	Visual *  visual ,
	char *  color_name
	)
{
	XColor  color ;
	XColor  closest_color ;

	if ( XParseColor( display , cmap , color_name , &color )  == 0)
	{
		return BlackPixel( display , screen ) ;
	}

	if ( XAllocColor( display , cmap , &color) == 0)
	{
		if( visual->class == PseudoColor || visual->class == GrayScale)
		{
			get_closest_xcolor_pseudo( display , screen , cmap ,
						  &color ,
						  &closest_color) ;
			return closest_color.pixel ;
		}

		return BlackPixel( display , screen ) ;
	}

	return color.pixel ;
}

#endif

