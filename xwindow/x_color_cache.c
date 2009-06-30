/*
 *	$Id$
 */

#include  "x_color_cache.h"

#include  <stdio.h>


/* --- static variables --- */

static x_color_cache_t **  color_caches ;
static u_int  num_of_caches ;


/* --- static functions --- */

static x_color_t *
get_cached_256_xcolor(
	x_color_cache_t *  color_cache ,
	ml_color_t  color
	)
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;
	char *  name ;

	if( ! IS_256_COLOR(color))
	{
		return  NULL ;
	}

	if( color_cache->color_256 == color)
	{
		return  &color_cache->xcolor_256 ;
	}

	if( ( ( name = ml_get_color_name( color)) &&
		x_color_config_get_rgb( color_cache->color_config , &red , &green , &blue , name) )
		  || ml_get_color_rgb( color, &red, &green, &blue))
	{
		x_color_t  xcolor ;

		if( ! x_load_rgb_xcolor( color_cache->display, color_cache->screen, &xcolor,
				red, green, blue))
		{
			return  NULL ;
		}
		
		if( color_cache->fade_ratio < 100)
		{
			if( ! x_xcolor_fade( color_cache->display , color_cache->screen ,
				&xcolor , color_cache->fade_ratio))
			{
				return  NULL ;
			}
		}

		if( IS_256_COLOR( color_cache->color_256))
		{
			x_unload_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->xcolor_256) ;
		}

		color_cache->xcolor_256 = xcolor ;
		color_cache->color_256 = color ;

	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " new color %x %x\n", color, xcolor.pixel) ;
	#endif

		return  &color_cache->xcolor_256 ;
	}

	return  NULL ;
}

static x_color_t *
get_cached_vtsys_xcolor(
	x_color_cache_t *  color_cache ,
	ml_color_t  color
	)
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;
	char *  name ;
	char *  tag ;

	if( ! IS_VTSYS_COLOR(color))
	{
		return  NULL ;
	}
	
	if( color_cache->is_loaded[color])
	{
		return  &color_cache->xcolors[color] ;
	}

	if( ( tag = ml_get_color_name( color)) == NULL)
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " ml_get_color_name failed.\n") ;
	#endif
	
		return  NULL ;
	}

	if( strncmp( tag, "hl_", 3) == 0)
	{
		name = tag + 3 ;
	}
	else
	{
		name = tag ;
	}
	
	if( x_color_config_get_rgb( color_cache->color_config , &red , &green , &blue , tag))
	{
		if( x_load_rgb_xcolor( color_cache->display , color_cache->screen ,
			&color_cache->xcolors[color] , red , green , blue))
		{
			goto  found ;
		}
	}

	if( x_load_named_xcolor( color_cache->display , color_cache->screen ,
		&color_cache->xcolors[color] , name))
	{
		if( color < MAX_BASIC_VTSYS_COLORS)
		{
			x_get_xcolor_rgb( &red , &green , &blue , &color_cache->xcolors[color]) ;

			x_unload_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->xcolors[color]) ;
			
			if( x_load_rgb_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->xcolors[color] ,
				red * 90 / 100 , green * 90 / 100 , blue * 90 / 100))
			{
				goto  found ;
			}
		}
		else
		{
			goto  found ;
		}
	}

	return  NULL ;

found:
	if( color_cache->fade_ratio < 100)
	{
		if( ! x_xcolor_fade( color_cache->display , color_cache->screen ,
			&color_cache->xcolors[color] , color_cache->fade_ratio))
		{
			return  NULL ;
		}
	}
	
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " new color %x %x\n",
		color, color_cache->xcolors[color].pixel) ;
#endif

	color_cache->is_loaded[color] = 1 ;

	return  &color_cache->xcolors[color] ;
}


/* --- global functions --- */

x_color_cache_t *
x_acquire_color_cache(
	Display *  display,
	int  screen,
	x_color_config_t *  color_config,
	u_int8_t  fade_ratio
	)
{
	int  count ;
	x_color_cache_t *  color_cache ;
	void *  p ;

	for( count = 0 ; count < num_of_caches ; count ++)
	{
		if( color_caches[count]->display == display &&
			color_caches[count]->screen == screen &&
			color_caches[count]->color_config == color_config &&
			color_caches[count]->fade_ratio == fade_ratio)
		{
			color_caches[count]->ref_count ++ ;
			
			return  color_caches[count] ;
		}
	}

	if( ( p = realloc( color_caches , sizeof( x_color_cache_t*) * (num_of_caches + 1)))
		== NULL)
	{
		return  NULL ;
	}

	color_caches = p ;

	if( ( color_cache = malloc( sizeof( x_color_cache_t))) == NULL)
	{
		return  NULL ;
	}
	
	memset( color_cache->is_loaded , 0 , sizeof( color_cache->is_loaded)) ;
	
	color_cache->display = display ;
	color_cache->screen = screen ;

	color_cache->color_config = color_config ;
	color_cache->fade_ratio = 100 ;

	color_cache->color_256 = ML_UNKNOWN_COLOR ;

	if( ! x_load_named_xcolor( color_cache->display , color_cache->screen ,
		&color_cache->black , "black"))
	{
		free( color_cache) ;
		
		return  NULL ;
	}

	color_cache->ref_count = 1 ;

	color_caches[num_of_caches++] = color_cache ;

	return  color_cache ;
}

int
x_release_color_cache(
	x_color_cache_t *  color_cache
	)
{
	int  count ;
	
	if( -- color_cache->ref_count > 0)
	{
		return  1 ;
	}

	for( count = 0 ; count < num_of_caches ; count ++)
	{
		if( color_caches[count] == color_cache)
		{
			color_caches[count] = color_caches[--num_of_caches] ;

			x_color_cache_unload( color_cache) ;
			x_unload_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->black) ;

			free( color_cache) ;

			if( num_of_caches == 0)
			{
				free( color_caches) ;
				color_caches = NULL ;
			}
			
			return  1 ;
		}
	}

	return  0 ;
}

int
x_color_cache_unload(
	x_color_cache_t *  color_cache
	)
{
	ml_color_t  color ;

	for( color = 0 ; color < MAX_VTSYS_COLORS ; color ++)
	{
		if( color_cache->is_loaded[color])
		{
			x_unload_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->xcolors[color]) ;
			color_cache->is_loaded[color] = 0 ;
		}
	}

	if( IS_256_COLOR(color_cache->color_256))
	{
		x_unload_xcolor( color_cache->display , color_cache->screen ,
			&color_cache->xcolor_256) ;
		color_cache->color_256 = ML_UNKNOWN_COLOR ;
	}

	return  1 ;
}

int
x_color_cache_unload_all(void)
{
	int  count ;

	for( count = 0 ; count < num_of_caches ; count++)
	{
		x_color_cache_unload( color_caches[count]) ;
	}

	return  1 ;
}

int
x_load_xcolor(
	x_color_cache_t *  color_cache ,
	x_color_t *  xcolor,
	char *  name
	)
{
	u_short  red ;
	u_short  green ;
	u_short  blue ;

	if( x_color_config_get_rgb( color_cache->color_config , &red , &green , &blue , name))
	{
		if( x_load_rgb_xcolor( color_cache->display , color_cache->screen ,
			xcolor , red , green , blue))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " new color %x\n",
				xcolor->pixel) ;
		#endif
		
			return  1 ;
		}
	}

	if( x_load_named_xcolor( color_cache->display , color_cache->screen , xcolor , name))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " new color %x\n",
			xcolor->pixel) ;
	#endif
	
		return  1 ;
	}

	return  0 ;
}

/* Always return non-null value. */
x_color_t *
x_get_cached_xcolor(
	x_color_cache_t *  color_cache ,
	ml_color_t  color
	)
{
	x_color_t *  xcolor ;
	
	if( ( xcolor = get_cached_vtsys_xcolor( color_cache, color)))
	{
		return  xcolor ;
	}
	
	if( ( xcolor = get_cached_256_xcolor( color_cache, color)))
	{
		return  xcolor ;
	}

	kik_msg_printf( " Loading color 0x%x failed. Using black color instead.\n", color) ;

	return  &color_cache->black ;
}
