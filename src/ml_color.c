/*
 *	$Id$
 */

#include  "ml_color.h"

#include  <string.h>		/* memcpy */
#include  <kiklib/kik_mem.h>


typedef struct  color_table
{
	char *  name ;
	ml_color_t  color ;
	
} color_table_t ;


/* --- static variables --- */

static color_table_t  color_table[] =
{
	{ "black" , MLC_BLACK } ,
	{ "red" , MLC_RED } ,
	{ "green" , MLC_GREEN } ,
	{ "yellow" , MLC_YELLOW } ,
	{ "blue" , MLC_BLUE } ,
	{ "magenta" , MLC_MAGENTA } ,
	{ "cyan" , MLC_CYAN } ,
	{ "white" , MLC_WHITE } ,
	{ "gray" , MLC_GRAY } ,
	{ "lightgray" , MLC_LIGHTGRAY } ,
	{ "pink" , MLC_PINK } ,
	{ "brown" , MLC_BROWN } ,
	{ "priv_fg" , MLC_PRIVATE_FG_COLOR } ,
	{ "priv_bg" , MLC_PRIVATE_BG_COLOR } ,
	
} ;


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

char *
ml_get_color_name(
	ml_color_t  color
	)
{
	int  counter ;
	
	/* MLC_PRIVATE_{FG|BG}_COLOR have no color names. */
	for( counter = 0 ; counter < MLC_PRIVATE_FG_COLOR ; counter ++)
	{
		if( color_table[counter].color == color)
		{
			return  color_table[counter].name ;
		}
	}

	return  NULL ;
}

ml_color_t
ml_get_color(
	char *  name
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < sizeof( color_table) / sizeof( color_table_t) ; counter ++)
	{
		if( strcmp( color_table[counter].name , name) == 0)
		{
			return  color_table[counter].color ;
		}
	}

	return  MLC_UNKNOWN_COLOR ;
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
		return  0 ;
	}
	else
	{
		return  1 ;
	}
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
		return  0 ;
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
		return  0 ;
	}
	else
	{
		return  1 ;
	}
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
		return  0 ;
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
