/*
 *	$Id$
 */

#include  "x_color_cache.h"

#include  <stdio.h>


/* --- static variables --- */

static x_color_cache_t **  color_caches ;
static u_int  num_of_caches ;


/* --- static functions --- */

static x_color_cache_256_t *
acquire_color_cache_256(
	Display *  display ,
	int  screen
	)
{
	u_int  count ;
	x_color_cache_256_t *  cache ;

	for( count = 0 ; count < num_of_caches ; count++)
	{
		if( color_caches[count]->display == display &&
			color_caches[count]->screen == screen &&
			color_caches[count]->cache_256)
		{
			color_caches[count]->cache_256->ref_count ++ ;
			
			return  color_caches[count]->cache_256 ;
		}
	}

	if( ( cache = malloc( sizeof( x_color_cache_256_t))) == NULL)
	{
		return  NULL ;
	}

	memset( cache->xcolors , 0 , sizeof( cache->xcolors)) ;
	cache->ref_count = 1 ;

	return  cache ;
}

static x_color_t *
get_cached_256_xcolor(
	x_color_cache_t *  color_cache ,
	ml_color_t  color
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	char *  name ;

	if( ! IS_256_COLOR(color))
	{
		return  NULL ;
	}

	if( ! color_cache->cache_256 &&
	    ! ( color_cache->cache_256 =
			acquire_color_cache_256( color_cache->display , color_cache->screen)))
	{
		return  NULL ;
	}

	if( color_cache->cache_256->xcolors[color - 16].is_loaded)
	{
		return  &color_cache->cache_256->xcolors[color - 16] ;
	}

	if( ( ( name = ml_get_color_name( color)) &&
		x_color_config_get_rgb( color_cache->color_config , &red , &green , &blue , name) )
		  || ml_get_color_rgb( color, &red, &green, &blue))
	{
		if( ! x_load_rgb_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->cache_256->xcolors[color - 16] , red , green , blue))
		{
			return  NULL ;
		}

		/*
		 * 16-255 colors ignore color_cache->fade_ratio.
		 */
		
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " new color %x %x\n" ,
			color , color_cache->cache_256->xcolors[color - 16].pixel) ;
	#endif

		return  &color_cache->cache_256->xcolors[color - 16] ;
	}

	return  NULL ;
}

static x_color_t *
get_cached_vtsys_xcolor(
	x_color_cache_t *  color_cache ,
	ml_color_t  color
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;
	char *  name ;
	char *  tag ;

	if( ! IS_VTSYS_COLOR(color))
	{
		return  NULL ;
	}
	
	if( color_cache->xcolors[color].is_loaded)
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
	u_int  count ;
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
	
	memset( color_cache->xcolors , 0 , sizeof( color_cache->xcolors)) ;
	
	color_cache->display = display ;
	color_cache->screen = screen ;
	color_cache->cache_256 = NULL ;

	color_cache->color_config = color_config ;
	color_cache->fade_ratio = fade_ratio ;

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
	u_int  count ;
	
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

	for( color = 0 ;
	     color < sizeof(color_cache->xcolors) / sizeof(color_cache->xcolors[0]) ; color ++)
	{
		if( color_cache->xcolors[color].is_loaded)
		{
			x_unload_xcolor( color_cache->display , color_cache->screen ,
				&color_cache->xcolors[color]) ;
		}
	}

	if( color_cache->cache_256 && -- color_cache->cache_256->ref_count == 0)
	{
		x_color_cache_256_t *  cache ;

		cache = color_cache->cache_256 ;
		for( color = 0 ; color < sizeof(cache->xcolors) / sizeof(cache->xcolors[0]) ;
			color++)
		{
			if( cache->xcolors[color].is_loaded)
			{
				x_unload_xcolor( color_cache->display , color_cache->screen ,
					&cache->xcolors[color]) ;
			}
		}

		free( cache) ;
		color_cache->cache_256 = NULL ;
	}

	return  1 ;
}

int
x_color_cache_unload_all(void)
{
	u_int  count ;

	for( count = 0 ; count < num_of_caches ; count++)
	{
		x_color_cache_unload( color_caches[count]) ;
	}

	return  1 ;
}

/* Not cached */
int
x_load_xcolor(
	x_color_cache_t *  color_cache ,
	x_color_t *  xcolor,
	char *  name
	)
{
	u_int8_t  red ;
	u_int8_t  green ;
	u_int8_t  blue ;

	if( x_color_config_get_rgb( color_cache->color_config , &red , &green , &blue , name))
	{
		if( x_load_rgb_xcolor( color_cache->display , color_cache->screen ,
			xcolor , red , green , blue))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " new color %x\n",
				xcolor->pixel) ;
		#endif
		
			goto  found ;
		}
	}

	if( x_load_named_xcolor( color_cache->display , color_cache->screen , xcolor , name))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " new color %x\n",
			xcolor->pixel) ;
	#endif
	
		goto  found ;
	}

	return  0 ;
	
found:
	if( color_cache->fade_ratio < 100)
	{
		if( ! x_xcolor_fade( color_cache->display , color_cache->screen ,
			xcolor , color_cache->fade_ratio))
		{
			return  0 ;
		}
	}
	
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " new color %s %x\n", name , xcolor->pixel) ;
#endif

	return  1 ;
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
