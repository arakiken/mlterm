/*
 *	$Id$
 */

#include  "x_color_manager.h"

#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */


/* --- static functions --- */

static int
unload_all_colors(
	x_color_manager_t *  color_man
	)
{
	ml_color_t  color ;

	for( color = 0 ; color < MAX_COLORS ; color ++)
	{
		if( color_man->is_loaded[color])
		{
			x_unload_xcolor( color_man->display , color_man->screen ,
				&color_man->colors[color]) ;
			x_unload_xcolor( color_man->display , color_man->screen ,
				&color_man->colors[color]) ;
			color_man->is_loaded[color] = 0 ;
		}
	}

	return  1 ;
}


/* --- global functions --- */

x_color_manager_t *
x_color_manager_new(
	Display *  display ,
	int  screen ,
	x_color_custom_t *  color_custom ,
	char *  fg_color ,
	char *  bg_color
	)
{
	x_color_manager_t *  color_man ;

	if( ( color_man = malloc( sizeof( x_color_manager_t))) == NULL)
	{
		return  NULL ;
	}
	
	memset( color_man , 0 , sizeof( x_color_manager_t)) ;

	color_man->display = display ;
	color_man->screen = screen ;

	color_man->color_custom = color_custom ;
	color_man->fade_ratio = 100 ;
	color_man->is_reversed = 0 ;

	if( fg_color == NULL)
	{
		fg_color = "black" ;
	}

	if( bg_color == NULL)
	{
		bg_color = "white" ;
	}
	
	color_man->fg_color = strdup( fg_color) ;
	color_man->bg_color = strdup( bg_color) ;
	
	return  color_man ;
}

int
x_color_manager_delete(
	x_color_manager_t *  color_man
	)
{
	unload_all_colors( color_man) ;

	free( color_man->fg_color) ;
	free( color_man->bg_color) ;

	free( color_man) ;

	return  1 ;
}

int
x_color_manager_set_fg_color(
	x_color_manager_t *  color_man ,
	char *  fg_color
	)
{
	color_man->fg_color = strdup( fg_color) ;

	return  1 ;
}

int
x_color_manager_set_bg_color(
	x_color_manager_t *  color_man ,
	char *  bg_color
	)
{
	color_man->bg_color = strdup( bg_color) ;

	return  1 ;
}

x_color_t *
x_get_color(
	x_color_manager_t *  color_man ,
	ml_color_t  color
	)
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;
	char *  name ;
	int  is_highlighted ;

	if( ( color & ML_BOLD_COLOR_MASK))
	{
		color &= ~ML_BOLD_COLOR_MASK ;
		is_highlighted = 1 ;
	}
	else
	{
		is_highlighted = 0 ;
	}
	
	if( color_man->is_reversed)
	{
		if( color == ML_FG_COLOR)
		{
			color = ML_BG_COLOR ;
		}
		else if( color == ML_BG_COLOR)
		{
			color = ML_FG_COLOR ;
		}
	}

	if( color_man->is_loaded[color])
	{
		if( is_highlighted)
		{
			return  &color_man->highlighted_colors[color] ;
		}
		else
		{
			return  &color_man->colors[color] ;
		}
	}

	if( color == ML_FG_COLOR)
	{
		name = color_man->fg_color ;
	}
	else if( color == ML_BG_COLOR)
	{
		name = color_man->bg_color ;
	}
	else
	{
		name = ml_get_color_name( color) ;
	}
	
	if( x_color_custom_get_rgb( color_man->color_custom , &red , &green , &blue , name))
	{
		if( x_load_rgb_xcolor( color_man->display , color_man->screen ,
			&color_man->colors[color] , red * 90 / 100 , green * 90 / 100 , blue * 90 / 100))
		{
			if( x_load_rgb_xcolor( color_man->display , color_man->screen ,
				&color_man->highlighted_colors[color] , red , green , blue))
			{
				goto  found ;
			}
			else
			{
				x_unload_xcolor( color_man->display , color_man->screen ,
					&color_man->colors[color]) ;
			}
		}
	}

	if( x_load_named_xcolor( color_man->display , color_man->screen ,
		&color_man->highlighted_colors[color] , name))
	{
		x_get_xcolor_rgb( &red , &green , &blue , &color_man->highlighted_colors[color]) ;
	
		if( x_load_rgb_xcolor( color_man->display , color_man->screen ,
			&color_man->colors[color] , red * 90 / 100 , green * 90 / 100 , blue * 90 / 100))
		{
			goto  found ;
		}
		else
		{
			x_unload_xcolor( color_man->display , color_man->screen ,
				&color_man->highlighted_colors[color]) ;
		}
	}

	kik_msg_printf( " Not enough colors available. %s setting problematic.\n" , name) ;

	return  NULL ;

found:
	if( color_man->fade_ratio < 100)
	{
		if( ! x_xcolor_fade( color_man->display , color_man->screen ,
			&color_man->colors[color] , color_man->fade_ratio) ||
			! x_xcolor_fade( color_man->display , color_man->screen ,
			&color_man->highlighted_colors[color] , color_man->fade_ratio))
		{
			return  NULL ;
		}
	}
	
	color_man->is_loaded[color] = 1 ;

	return  &color_man->colors[color] ;
}

x_color_t *
x_get_highligted_color(
	x_color_manager_t *  color_man ,
	ml_color_t  color
	)
{
	return  NULL ;
}

char *
x_get_fg_color_name(
	x_color_manager_t *  color_man
	)
{
	return  color_man->fg_color ;
}

char *
x_get_bg_color_name(
	x_color_manager_t *  color_man
	)
{
	return  color_man->bg_color ;
}

int
x_color_manager_fade(
	x_color_manager_t *  color_man ,
	u_int  fade_ratio	/* valid value is 0 - 99 */
	)
{
	if( fade_ratio >= 100)
	{
		return  0 ;
	}
	
	unload_all_colors( color_man) ;
	color_man->fade_ratio = fade_ratio ;

	return  1 ;
}

int
x_color_manager_unfade(
	x_color_manager_t *  color_man
	)
{
	unload_all_colors( color_man) ;
	color_man->fade_ratio = 100 ;

	return  1 ;
}

int
x_color_manager_reverse_video(
	x_color_manager_t *  color_man
	)
{
	if( color_man->is_reversed)
	{
		return  0 ;
	}
	
	color_man->is_reversed = 1 ;

	return  1 ;
}

int
x_color_manager_restore_video(
	x_color_manager_t *  color_man
	)
{
	if( ! color_man->is_reversed)
	{
		return  0 ;
	}
	
	color_man->is_reversed = 0 ;

	return  1 ;
}
