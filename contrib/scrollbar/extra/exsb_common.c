/*
 *	$Id$
 */

#include  <stdio.h>
#include  <stdlib.h>
#include  <X11/Xlib.h>

#define ABS(x)	((x < 0) ? -(x) : (x))

/* --- static functions --- */

void
get_closest_xcolor_pseudo(
	Display * display ,
	int screen ,
	Colormap cmap ,
	XColor color ,
	XColor * closest_color
	)
{
	XColor *  all_colors ;
	int  i ;
	int  index ;
	int  min ;
	int  sum ;
	int  diff_r , diff_g , diff_b ;
	unsigned long  ret = BlackPixel( display , screen) ;
	int ncells = DisplayCells( display , screen) ;

	all_colors = malloc( ncells * sizeof( XColor)) ;

	/* get all colors from default colormap */
	for( i = 0 ; i < ncells ; i ++)
	{
		all_colors[i].pixel = i ;
	}
	XQueryColors( display , cmap , all_colors, ncells) ;

	/* compare */
	for( i = 0 ; i < ncells ; i ++)
	{
		diff_r = color.red - all_colors[i].red ;
		diff_g = color.green - all_colors[i].green ;
		diff_r = color.blue - all_colors[i].blue ;

		sum = ABS( diff_r) + ABS( diff_g) + ABS( diff_b) ;

		if ( sum < min) /* closest ? */
		{
			min = sum ;
			index = i ;
		}
	}

	closest_color->pixel = all_colors[index].pixel ;
	closest_color->red = all_colors[index].red ;
	closest_color->green = all_colors[index].green ;
	closest_color->blue = all_colors[index].blue ;

	free( all_colors) ;
}

/* --- global functions -- */

unsigned long
exsb_get_pixel(
	Display *  display ,
	int  screen ,
	char *  color_name
	)
{
	XColor  color ;
	XColor  closest_color ;
	Colormap cmap = DefaultColormap( display , screen) ;
	Visual * visual = DefaultVisual( display , screen) ;

	if ( XParseColor( display , cmap , color_name , &color )  == 0)
	{
		return BlackPixel( display , screen ) ;
	}

	if ( XAllocColor( display , cmap , &color ) == 0)
	{
		if( visual->class == PseudoColor || visual->class == GrayScale)
		{
			get_closest_xcolor_pseudo( display , screen , cmap ,
						  color ,
						  &closest_color) ;
			return closest_color.pixel ;
		}

		return BlackPixel( display , screen ) ;
	}

	return color.pixel ;
}
