/*
 *	$Id$
 */

#ifndef  __X_COLOR_CACHE_H__
#define  __X_COLOR_CACHE_H__


#include  <ml_color.h>
#include  "x_color.h"
#include  "x_color_config.h"


typedef struct x_color_cache_256
{
	x_color_t  xcolors[240] ;
	u_int8_t  is_loaded[240] ;

	u_int  ref_count ;

} x_color_cache_256_t ;

typedef struct x_color_cache
{
	x_display_t *  disp ;

	x_color_t  xcolors[16] ;
	u_int8_t  is_loaded[16] ;

	x_color_cache_256_t *  cache_256 ;

	x_color_t  black ;
	
	x_color_config_t *  color_config ;

	u_int8_t  fade_ratio ;

	u_int16_t  ref_count ;		/* 0 - 65535 */

} x_color_cache_t ;


x_color_cache_t *  x_acquire_color_cache( x_display_t *  disp ,
	x_color_config_t *  color_config, u_int8_t  fade_ratio) ;

int  x_release_color_cache( x_color_cache_t *  color_cache) ;

int  x_color_cache_unload( x_color_cache_t *  color_cache) ;

int  x_color_cache_unload_all(void) ;

int  x_load_xcolor( x_color_cache_t *  color_cache , x_color_t *  xcolor, char *  name) ;

x_color_t *  x_get_cached_xcolor( x_color_cache_t *  color_cache , ml_color_t  color) ;


#endif
