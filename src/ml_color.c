/*
 *	$Id$
 */

#include  "ml_color.h"

#include  <string.h>		/* memcpy */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>

/* --- static functions --- */

static int
alloc_closest_xcolor_pseudo(
	Display *  display ,
	int  screen ,
	unsigned short  red ,		/* 0 to 0xffff */
	unsigned short  green ,		/* 0 to 0xffff */
	unsigned short  blue ,		/* 0 to 0xffff */
#ifdef ANTI_ALIAS
	XftColor *  ret_xcolor
#else
	XColor *  ret_xcolor
#endif
	)
{
	Visual *  visual = DefaultVisual( display , screen) ;
	XColor *  all_colors ;	/* colors exist in the shared color map */
	XColor  closest_color ;

	Colormap  cmap = DefaultColormap( display , screen) ;

	int  closest_index = -1 ;
	unsigned long  min_diff = 0xffffffff ;
	unsigned long  diff ;
	unsigned long  diff_r = 0 , diff_g = 0 , diff_b = 0 ;
	int  ncells = DisplayCells( display , screen) ;
	int  i ;

	/* FIXME: When visual class is StaticColor, should not be return? */
	if ( ! visual->class == PseudoColor && ! visual->class == GrayScale)
	{
		return  0 ;
	}

	if( ( all_colors = malloc( ncells * sizeof( XColor))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}

	/* get all colors from default colormap */
	for( i = 0 ; i < ncells ; i ++)
	{
		all_colors[i].pixel = i ;
	}
	XQueryColors( display , cmap , all_colors, ncells) ;

	/* find the closest color */
	for( i = 0 ; i < ncells ; i ++)
	{
		diff_r = (red - all_colors[i].red) >> 8 ;
		diff_g = (green - all_colors[i].green) >> 8 ;
		diff_r = (blue - all_colors[i].blue) >> 8 ;

		diff = diff_r * diff_r + diff_g * diff_g + diff_b * diff_b ;

		if ( diff < min_diff) /* closest ? */
		{
			min_diff = diff ;
			closest_index = i ;
		}
	}

	if( closest_index == -1)	/* unable to find closest color */
	{
		return  0 ;
	}

	closest_color.red = all_colors[closest_index].red ;
	closest_color.green = all_colors[closest_index].green ;
	closest_color.blue = all_colors[closest_index].blue ;
	closest_color.flags = DoRed | DoGreen | DoBlue;

	if ( ! XAllocColor( display , cmap , &closest_color))
	{
		return 0 ;
	}

#ifdef ANTI_ALIAS
	ret_xcolor->pixel = closest_color.pixel ;
	ret_xcolor->color.red = closest_color.red ;
	ret_xcolor->color.green = closest_color.green ;
	ret_xcolor->color.blue = closest_color.blue ;
	ret_xcolor->color.alpha = 0xffff ;
#else
	ret_xcolor->pixel = closest_color.pixel ;
	ret_xcolor->red = closest_color.red ;
	ret_xcolor->green = closest_color.green ;
	ret_xcolor->blue = closest_color.blue ;
#endif
	return 1 ;
}


/* --- global functions --- */

ml_color_table_t
ml_color_table_dup(
	ml_color_table_t  color_table
	)
{
	ml_color_table_t  new_color_table ;

	if( ( new_color_table = malloc( sizeof( x_color_t *) * MAX_COLORS)) == NULL)
	{
		return  NULL ;
	}

	memcpy( new_color_table , color_table , sizeof( x_color_t *) * MAX_COLORS) ;

	return  new_color_table ;
}

#ifdef  ANTI_ALIAS

int
ml_load_named_xcolor(
	Display *  display ,
	int  screen ,
	XftColor *  xcolor ,
	char *  name
	)
{
	if( ! XftColorAllocName( display , DefaultVisual( display , screen) ,
		DefaultColormap( display , screen) , name , xcolor))
	{
		XColor  exact ;

		/* try to find closest color */
		if( XParseColor( display , DefaultColormap( display , screen) ,
				name , &exact))
		{
			return  alloc_closest_xcolor_pseudo( display , screen ,
					exact.red , exact.green , exact.blue ,
					xcolor) ;
		}

		return 0 ;
	}

	return  1 ;

}

int
ml_load_rgb_xcolor(
	Display *  display ,
	int  screen ,
	XftColor *  xcolor ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	XRenderColor  rend_color ;
	
	rend_color.red = red ;
	rend_color.green = green ;
	rend_color.blue = blue ;
	rend_color.alpha = 0xffff ;

	if( ! XftColorAllocValue( display , DefaultVisual( display , screen) ,
		DefaultColormap( display , screen) , &rend_color , xcolor))
	{
		/* try to find closest color */
		return  alloc_closest_xcolor_pseudo( display , screen ,
				red , green , blue , xcolor);
	}

	return  1 ;
}

int
ml_unload_xcolor(
	Display *  display ,
	int  screen ,
	XftColor *  xcolor
	)
{
	XftColorFree( display , DefaultVisual( display , screen) ,
		DefaultColormap( display , screen) , xcolor) ;

	return  1 ;
}

int
ml_get_xcolor_rgb(
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	XftColor *  xcolor
	)
{
	*red = xcolor->color.red ;
	*blue = xcolor->color.blue ;
	*green = xcolor->color.green ;

	return  1 ;
}

#else

int
ml_load_named_xcolor(
	Display *  display ,
	int  screen ,
	XColor *  xcolor ,
	char *  name
	)
{
	XColor  exact ;

	if( ! XAllocNamedColor( display , DefaultColormap( display , screen) , name , xcolor , &exact))
	{
		XColor  exact ;

		/* try to find closest color */
		if( XParseColor( display , DefaultColormap( display , screen) ,
				name , &exact))
		{
			return  alloc_closest_xcolor_pseudo( display , screen ,
					exact.red , exact.green , exact.blue ,
					xcolor) ;
		}

		return  0 ;
	}

	return  1 ;

}

int
ml_load_rgb_xcolor(
	Display *  display ,
	int  screen ,
	XColor *  xcolor ,
	u_short  red ,
	u_short  green ,
	u_short  blue
	)
{
	xcolor->red = red ;
	xcolor->green = green ;
	xcolor->blue = blue ;
	xcolor->flags = 0 ;

	if( ! XAllocColor( display , DefaultColormap( display , screen) , xcolor))
	{
		/* try to find closest color */
		return  alloc_closest_xcolor_pseudo( display , screen ,
				red , green , blue , xcolor);
	}

	return  1 ;
}

int
ml_unload_xcolor(
	Display *  display ,
	int  screen ,
	XColor *  xcolor
	)
{
	u_long  pixel[1] ;

	pixel[0] = xcolor->pixel ;
	
	/* XXX planes argument 0 is ok ? */
	XFreeColors( display , DefaultColormap( display , screen) , pixel , 1 , 0) ;

	return  1 ;
}

int
ml_get_xcolor_rgb(
	u_short *  red ,
	u_short *  green ,
	u_short *  blue ,
	XColor *  xcolor
	)
{
	*red = xcolor->red ;
	*blue = xcolor->blue ;
	*green = xcolor->green ;

	return  1 ;
}

#endif
