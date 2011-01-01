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
	x_display_t *  disp
	)
{
	u_int  count ;
	x_color_cache_256_t *  cache ;

	for( count = 0 ; count < num_of_caches ; count++)
	{
		if( color_caches[count]->disp == disp &&
			color_caches[count]->cache_256)
		{
			color_caches[count]->cache_256->ref_count ++ ;
			
			return  color_caches[count]->cache_256 ;
		}
	}

	if( ( cache = calloc( 1 , sizeof( x_color_cache_256_t))) == NULL)
	{
		return  NULL ;
	}

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
	u_int8_t  alpha ;
	char *  name ;

	if( ! IS_256_COLOR(color))
	{
		return  NULL ;
	}

	if( ! color_cache->cache_256 &&
	    ! ( color_cache->cache_256 = acquire_color_cache_256( color_cache->disp)))
	{
		return  NULL ;
	}

	if( color_cache->cache_256->is_loaded[color - 16])
	{
		return  &color_cache->cache_256->xcolors[color - 16] ;
	}

	alpha = 0xff ;
	if( ( ( name = ml_get_color_name( color)) &&
	      x_color_config_get_rgb( color_cache->color_config ,
					&red , &green , &blue , &alpha , name) )
	    || ml_get_color_rgb( color , &red , &green , &blue))
	{
		if( ! x_load_rgb_xcolor( color_cache->disp ,
			&color_cache->cache_256->xcolors[color - 16] , red , green , blue , alpha))
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

		color_cache->cache_256->is_loaded[color - 16] = 1 ;

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
	u_int8_t  alpha ;
	char *  tag ;
	char *  name ;

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

	/* Hack for compatibility with xterm vtsys colors. */
	if( ML_RED <= color && color <= (ML_BLACK|ML_BOLD_COLOR_MASK))
	{
		char *  name_tbl[] = {
			"#CD0000" ,	/* ML_RED */
			"#00CD00" ,
			"#CDCD00" ,
			"#0000EE" ,
			"#CD00CD" ,
			"#00CDCD" ,
			"#E5E5E5" ,	/* ML_WHITE */
			"#7F7F7F" , } ;	/* ML_BLACK|ML_BOLD_COLOR_MASK */

		name = name_tbl[color - ML_RED] ;
	}
	else if( color == (ML_BLUE|ML_BOLD_COLOR_MASK))
	{
		name = "#5C5CFF" ;
	}
	else if( strncmp( tag, "hl_", 3) == 0)
	{
		name = tag + 3 ;
	}
	else
	{
		name = tag ;
	}
	
	if( x_color_config_get_rgb( color_cache->color_config ,
					&red , &green , &blue , &alpha , tag))
	{
		if( x_load_rgb_xcolor( color_cache->disp ,
			&color_cache->xcolors[color] , red , green , blue , alpha))
		{
			goto  found ;
		}
	}

	if( x_load_named_xcolor( color_cache->disp ,
		&color_cache->xcolors[color] , name))
	{
		goto  found ;
	}

	return  NULL ;

found:
	if( color_cache->fade_ratio < 100)
	{
		if( ! x_xcolor_fade( color_cache->disp ,
			&color_cache->xcolors[color] , color_cache->fade_ratio))
		{
			return  NULL ;
		}
	}
	
#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " new color %x %s red %x green %x blue %x\n",
		color, name , color_cache->xcolors[color].red , color_cache->xcolors[color].green ,
		color_cache->xcolors[color].blue) ;
#endif

	color_cache->is_loaded[color] = 1 ;

	return  &color_cache->xcolors[color] ;
}


/* --- global functions --- */

x_color_cache_t *
x_acquire_color_cache(
	x_display_t *  disp ,
	x_color_config_t *  color_config,
	u_int8_t  fade_ratio
	)
{
	u_int  count ;
	x_color_cache_t *  color_cache ;
	void *  p ;

	for( count = 0 ; count < num_of_caches ; count ++)
	{
		if( color_caches[count]->disp == disp &&
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
	memset( color_cache->is_loaded , 0 , sizeof( color_cache->is_loaded)) ;

	color_cache->disp = disp ;
	color_cache->cache_256 = NULL ;

	color_cache->color_config = color_config ;
	color_cache->fade_ratio = fade_ratio ;

	if( ! x_load_rgb_xcolor( color_cache->disp , &color_cache->black ,
					0 , 0 , 0 , 0xff))
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
			x_unload_xcolor( color_cache->disp , &color_cache->black) ;

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
		if( color_cache->is_loaded[color])
		{
			x_unload_xcolor( color_cache->disp , &color_cache->xcolors[color]) ;
			color_cache->is_loaded[color] = 0 ;
		}
	}

	if( color_cache->cache_256 && -- color_cache->cache_256->ref_count == 0)
	{
		x_color_cache_256_t *  cache ;

		cache = color_cache->cache_256 ;
		for( color = 0 ; color < sizeof(cache->xcolors) / sizeof(cache->xcolors[0]) ;
			color++)
		{
			if( cache->is_loaded[color])
			{
				x_unload_xcolor( color_cache->disp , &cache->xcolors[color]) ;
				color_cache->is_loaded[color] = 0 ;
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
	u_int8_t  alpha ;

	if( x_color_config_get_rgb( color_cache->color_config ,
					&red , &green , &blue , &alpha , name))
	{
		if( x_load_rgb_xcolor( color_cache->disp , xcolor , red , green , blue , alpha))
		{
		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " new color %x\n",
				xcolor->pixel) ;
		#endif
		
			goto  found ;
		}
	}

	if( x_load_named_xcolor( color_cache->disp , xcolor , name))
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
		if( ! x_xcolor_fade( color_cache->disp , xcolor , color_cache->fade_ratio))
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
