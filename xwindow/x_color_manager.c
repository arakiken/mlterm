/*
 *	$Id$
 */

#include  "x_color_manager.h"

#include  <stdio.h>		/* sprintf */
#include  <string.h>		/* memset */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */


enum
{
	_FG_COLOR = 0x0 ,
	_BG_COLOR = 0x1 ,
	_CUR_FG_COLOR = 0x2 ,
	_CUR_BG_COLOR = 0x3 ,
	MAX_SYS_COLORS = 0x4 ,
} ;


/* --- static variables --- */

static char *  default_fg_color = "black" ;
static char *  default_bg_color = "white" ;


/* --- static functions --- */

static int
sys_color_set(
	x_color_manager_t *   color_man,
	char *  name,
	int  color
	)
{
	free( color_man->sys_colors[color].name) ;
	
	if( color_man->sys_colors[color].is_loaded)
	{
		x_unload_xcolor( color_man->color_cache->disp ,
			&color_man->sys_colors[color].xcolor) ;
		color_man->sys_colors[color].is_loaded = 0 ;
	}

	if( name == NULL)
	{
		if( color == _CUR_FG_COLOR || color == _CUR_BG_COLOR)
		{
			color_man->sys_colors[color].name = NULL ;
		}
		else if( color == _FG_COLOR)
		{
			color_man->sys_colors[color].name = strdup( default_fg_color) ;
		}
		else /* if( color == _BG_COLOR) */
		{
			color_man->sys_colors[color].name = strdup( default_bg_color) ;
		}
	}
	else
	{
		color_man->sys_colors[color].name = strdup( name) ;
	}

	return  1 ;
}


/* --- global functions --- */

x_color_manager_t *
x_color_manager_new(
	x_display_t *  disp ,
	x_color_config_t *  color_config ,
	char *  fg_color ,	/* can be NULL(If NULL, use "black".) */
	char *  bg_color ,	/* can be NULL(If NULL, use "white".) */
	char *  cursor_fg_color , /* can be NULL(If NULL, use reversed one of the char color.) */
	char *  cursor_bg_color	  /* can be NULL(If NULL, use reversed one of the char color.) */
	)
{
	int  count ;
	x_color_manager_t *  color_man ;

	if( ( color_man = malloc( sizeof( x_color_manager_t))) == NULL)
	{
		return  NULL ;
	}

	if( ( color_man->color_cache = x_acquire_color_cache( disp , color_config , 100))
					== NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " x_aquire_color_cache failed.\n") ;
	#endif
	
		free( color_man) ;
		
		return  NULL ;
	}

	color_man->alt_color_cache = NULL ;

	if( fg_color == NULL)
	{
		fg_color = default_fg_color ;
	}
	
	if( bg_color == NULL)
	{
		bg_color = default_bg_color ;
	}
	
	color_man->sys_colors[_FG_COLOR].name = strdup( fg_color) ;
	color_man->sys_colors[_BG_COLOR].name = strdup( bg_color) ;
	
	if( cursor_fg_color)
	{
		color_man->sys_colors[_CUR_FG_COLOR].name = strdup( cursor_fg_color) ;
	}
	else
	{
		color_man->sys_colors[_CUR_FG_COLOR].name = NULL ;
	}
	
	if( cursor_bg_color)
	{
		color_man->sys_colors[_CUR_BG_COLOR].name = strdup( cursor_bg_color) ;
	}
	else
	{
		color_man->sys_colors[_CUR_BG_COLOR].name = NULL ;
	}

	for( count = 0 ; count < MAX_SYS_COLORS ; count++)
	{
		color_man->sys_colors[count].is_loaded = 0 ;
	}

	color_man->alpha = 0xff ;
	
	color_man->is_reversed = 0 ;
		
	return  color_man ;
}

int
x_color_manager_delete(
	x_color_manager_t *  color_man
	)
{
	int  count ;

	for( count = 0 ; count < MAX_SYS_COLORS ; count++)
	{
		free( color_man->sys_colors[count].name) ;
		
		if( color_man->sys_colors[count].is_loaded)
		{
			x_unload_xcolor( color_man->color_cache->disp ,
				&color_man->sys_colors[count].xcolor) ;
			color_man->sys_colors[count].is_loaded = 0 ;
		}
	}

	x_release_color_cache( color_man->color_cache) ;

	if( color_man->alt_color_cache)
	{
		x_release_color_cache( color_man->alt_color_cache) ;
	}
	
	free( color_man) ;
	
	return  1 ;
}

int
x_color_manager_set_fg_color(
	x_color_manager_t *  color_man ,
	char *  name
	)
{
	return  sys_color_set( color_man, name, _FG_COLOR) ;
}

int
x_color_manager_set_bg_color(
	x_color_manager_t *  color_man ,
	char *  name
	)
{
	return  sys_color_set( color_man, name, _BG_COLOR) ;
}

int
x_color_manager_set_cursor_fg_color(
	x_color_manager_t *  color_man ,
	char *  name
	)
{
	return  sys_color_set( color_man, name, _CUR_FG_COLOR) ;
}

int
x_color_manager_set_cursor_bg_color(
	x_color_manager_t *  color_man ,
	char *  name
	)
{
	return  sys_color_set( color_man, name, _CUR_BG_COLOR) ;
}

char *
x_color_manager_get_fg_color(
	x_color_manager_t *  color_man
	)
{
	return  color_man->sys_colors[_FG_COLOR].name ;
}

char *
x_color_manager_get_bg_color(
	x_color_manager_t *  color_man
	)
{
	return  color_man->sys_colors[_BG_COLOR].name ;
}

char *
x_color_manager_get_cursor_fg_color(
	x_color_manager_t *  color_man
	)
{
	return  color_man->sys_colors[_CUR_FG_COLOR].name ;
}

char *
x_color_manager_get_cursor_bg_color(
	x_color_manager_t *  color_man
	)
{
	return  color_man->sys_colors[_CUR_BG_COLOR].name ;
}

x_color_t *
x_get_xcolor(
	x_color_manager_t *  color_man ,
	ml_color_t  color
	)
{
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

	if( color == ML_FG_COLOR)
	{
		if( ! color_man->sys_colors[_FG_COLOR].is_loaded)
		{
			if( ! x_load_xcolor( color_man->color_cache ,
				&color_man->sys_colors[_FG_COLOR].xcolor ,
				color_man->sys_colors[_FG_COLOR].name))
			{
				return  &color_man->color_cache->black ;
			}

			color_man->sys_colors[_FG_COLOR].is_loaded = 1 ;
		}

		return  &color_man->sys_colors[_FG_COLOR].xcolor ;
	}
	else if( color == ML_BG_COLOR)
	{
		if( ! color_man->sys_colors[_BG_COLOR].is_loaded)
		{
			if( ! x_load_xcolor( color_man->color_cache ,
				&color_man->sys_colors[_BG_COLOR].xcolor ,
				color_man->sys_colors[_BG_COLOR].name))
			{
				return  &color_man->color_cache->black ;
			}

			if( color_man->alpha < 0xff)
			{
				u_int8_t  red ;
				u_int8_t  green ;
				u_int8_t  blue ;
				u_int8_t  alpha ;

				x_get_xcolor_rgb( &red , &green , &blue , &alpha ,
					&color_man->sys_colors[_BG_COLOR].xcolor) ;

				/*
				 * If alpha of bg color is already less than 0xff,
				 * default alpha value is not applied.
				 */
				if( alpha == 0xff)
				{
					x_load_rgb_xcolor( color_man->color_cache->disp ,
						&color_man->sys_colors[_BG_COLOR].xcolor ,
						red , green , blue , color_man->alpha) ;
				}
			}

			color_man->sys_colors[_BG_COLOR].is_loaded = 1 ;
		}

		return  &color_man->sys_colors[_BG_COLOR].xcolor ;
	}
	else
	{
		return  x_get_cached_xcolor( color_man->color_cache , color) ;
	}
}

/*
 * If fading status is changed, 1 is returned.
 */
int
x_color_manager_fade(
	x_color_manager_t *  color_man ,
	u_int  fade_ratio	/* valid value is 0 - 99 */
	)
{
	x_color_cache_t *  color_cache ;
	int  count ;

	if( fade_ratio >= 100)
	{
		return  0 ;
	}

	if( fade_ratio == color_man->color_cache->fade_ratio)
	{
		return  0 ;
	}
	
	if( color_man->alt_color_cache && fade_ratio == color_man->alt_color_cache->fade_ratio)
	{
		color_cache = color_man->alt_color_cache ;
		color_man->alt_color_cache = color_man->color_cache ;
	}
	else
	{
		if( ( color_cache = x_acquire_color_cache( color_man->color_cache->disp ,
					color_man->color_cache->color_config, fade_ratio)) == NULL)
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " x_aquire_color_cache failed.\n") ;
		#endif
			return  0 ;
		}

		if( color_man->color_cache->fade_ratio == 100)
		{
			if( color_man->alt_color_cache)
			{
				x_release_color_cache( color_man->alt_color_cache) ;
			}

			color_man->alt_color_cache = color_man->color_cache ;
		}
	}
		
	color_man->color_cache = color_cache ;

	for( count = 0 ; count < MAX_SYS_COLORS ; count++)
	{
		if( color_man->sys_colors[count].is_loaded)
		{
			x_unload_xcolor( color_man->color_cache->disp ,
				&color_man->sys_colors[count].xcolor) ;
			color_man->sys_colors[count].is_loaded = 0 ;
		}
	}
	
	return  1 ;
}

/*
 * If fading status is changed, 1 is returned.
 */
int
x_color_manager_unfade(
	x_color_manager_t *  color_man
	)
{
	x_color_cache_t *  color_cache ;
	int  count ;

	if( color_man->alt_color_cache == NULL || color_man->color_cache->fade_ratio == 100)
	{
		return  0 ;
	}

	color_cache = color_man->alt_color_cache ;
	color_man->alt_color_cache = color_man->color_cache ;
	color_man->color_cache = color_cache ;

	for( count = 0 ; count < MAX_SYS_COLORS ; count++)
	{
		if( color_man->sys_colors[count].is_loaded)
		{
			x_unload_xcolor( color_man->color_cache->disp ,
				&color_man->sys_colors[count].xcolor) ;
			color_man->sys_colors[count].is_loaded = 0 ;
		}
	}

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

/*
 * Swap the color of ML_BG_COLOR <=> that of cursor fg color.
 * Deal ML_BG_COLOR as cursor fg color.
 */
int
x_color_manager_adjust_cursor_fg_color(
	x_color_manager_t *  color_man
	)
{
	struct sys_color  tmp_color ;

	if( ! color_man->sys_colors[_CUR_FG_COLOR].name)
	{
		return  0 ;
	}

	tmp_color = color_man->sys_colors[_BG_COLOR] ;
	color_man->sys_colors[_BG_COLOR] = color_man->sys_colors[_CUR_FG_COLOR] ;
	color_man->sys_colors[_CUR_FG_COLOR] = tmp_color ;

	return  1 ;
}

/*
 * Swap the color of ML_FG_COLOR <=> that of cursor bg color.
 * Deal ML_FG_COLOR as cursor bg color.
 */
int
x_color_manager_adjust_cursor_bg_color(
	x_color_manager_t *  color_man
	)
{
	struct sys_color  tmp_color ;

	if( ! color_man->sys_colors[_CUR_BG_COLOR].name)
	{
		return  0 ;
	}

	tmp_color = color_man->sys_colors[_FG_COLOR] ;
	color_man->sys_colors[_FG_COLOR] = color_man->sys_colors[_CUR_BG_COLOR] ;
	color_man->sys_colors[_CUR_BG_COLOR] = tmp_color ;
	
	return  1 ;
}

/*
 * Unload system colors.
 */
int
x_color_manager_unload(
	x_color_manager_t *  color_man
	)
{
	int  count ;
	
	for( count = 0 ; count < MAX_SYS_COLORS ; count++)
	{
		if( color_man->sys_colors[count].is_loaded)
		{
			x_unload_xcolor( color_man->color_cache->disp ,
				&color_man->sys_colors[count].xcolor) ;
			color_man->sys_colors[count].is_loaded = 0 ;
		}
	}

	return  1 ;
}

int
x_color_manager_change_alpha(
	x_color_manager_t *  color_man ,
	u_int8_t  alpha
	)
{
	if(
	#ifndef  USE_WIN32GUI
		color_man->color_cache->disp->depth != 32 ||
	#endif
		alpha == color_man->alpha)
	{
		return  0 ;
	}

	if( color_man->sys_colors[_BG_COLOR].is_loaded)
	{
		x_unload_xcolor( color_man->color_cache->disp ,
			&color_man->sys_colors[_BG_COLOR].xcolor) ;
		color_man->sys_colors[_BG_COLOR].is_loaded = 0 ;
	}

	color_man->alpha = alpha ;

	return  1 ;
}
