/*
 *	$Id$
 */

#include  "x_font_cache.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */


/* --- static variables --- */

static x_font_cache_t **  font_caches ;
static u_int  num_of_caches ;


/* --- static functions --- */

#ifdef  DEBUG

static void
dump_cached_fonts(
	x_font_cache_t *  font_cache
	)
{
	int  count ;
	u_int  size ;
	KIK_PAIR( x_font) *  f_array ;
	
	kik_warn_printf( KIK_DEBUG_TAG " cached fonts info\n") ;
	kik_map_get_pairs_array( font_cache->xfont_table , f_array , size) ;
	for( count = 0 ; count < size ; count++)
	{
		if( f_array[count]->value != NULL)
		{
			x_font_dump( f_array[count]->value) ;
		}
	}
}

#endif

static int
font_hash(
	ml_font_t  font ,
	u_int  size
	)
{
	return  font % size ;
}

static int
font_compare(
	ml_font_t  key1 ,
	ml_font_t  key2
	)
{
	return  (key1 == key2) ;
}


/* --- global functions --- */

x_font_cache_t *
x_acquire_font_cache(
	Display *  display ,
	u_int  font_size ,
	mkf_charset_t  usascii_font_cs ,
	x_font_config_t *  font_config ,
	int  use_multi_col_char
	)
{
	int  count ;
	x_font_cache_t *  font_cache ;
	void *  p ;
	u_int  beg_font_size ;

	for( count = 0 ; count < num_of_caches ; count ++)
	{
		if( font_caches[count]->display == display &&
			font_caches[count]->font_size == font_size &&
			font_caches[count]->usascii_font_cs == usascii_font_cs &&
			font_caches[count]->font_config == font_config &&
			font_caches[count]->use_multi_col_char == use_multi_col_char)
		{
			font_caches[count]->ref_count ++ ;
			
			return  font_caches[count] ;
		}
	}

	if( ( p = realloc( font_caches , sizeof( x_font_cache_t*) * (num_of_caches + 1))) == NULL)
	{
		return  NULL ;
	}

	font_caches = p ;

	if( ( font_cache = malloc( sizeof( x_font_cache_t))) == NULL)
	{
		return  NULL ;
	}

	font_cache->font_config = font_config ;

	kik_map_new( ml_font_t , x_font_t * , font_cache->xfont_table , font_hash , font_compare) ;
	
	font_cache->display = display ;
	font_cache->font_size = font_size ;
	font_cache->usascii_font_cs = usascii_font_cs ;
	font_cache->use_multi_col_char = use_multi_col_char ;
	font_cache->ref_count = 1 ;

	/*
	 * Initialized all members of x_font_cache_t here.
	 */
	 
	beg_font_size = font_size ;
	
	while( ( font_cache->usascii_font =
			x_font_cache_get_xfont( font_cache , DEFAULT_FONT(US_ASCII))) == NULL)
	{
		if( ++ font_cache->font_size > x_get_max_font_size())
		{
			font_cache->font_size = x_get_min_font_size() ;
		}
		else if( font_cache->font_size == beg_font_size)
		{
			x_release_font_cache( font_cache) ;
			
			return  NULL ;
		}
	}

	return  font_caches[num_of_caches++] = font_cache ;
}

int
x_release_font_cache(
	x_font_cache_t *  font_cache
	)
{
	int  count ;
	
	if( -- font_cache->ref_count > 0)
	{
		return  1 ;
	}

	for( count = 0 ; count < num_of_caches ; count ++)
	{
		if( font_caches[count] == font_cache)
		{
			int  __count ;
			u_int  size ;
			KIK_PAIR( x_font) *  f_array ;

			font_caches[count] = font_caches[--num_of_caches] ;

			kik_map_get_pairs_array( font_cache->xfont_table , f_array , size) ;
			for( __count = 0 ; __count < size ; __count++)
			{
				if( f_array[__count]->value != NULL)
				{
					x_font_delete( f_array[__count]->value) ;
				}
			}

			kik_map_delete( font_cache->xfont_table) ;

			free( font_cache) ;

			if( num_of_caches == 0)
			{
				free( font_caches) ;
				font_caches = NULL ;
			}
			
			return  1 ;
		}
	}

	return  0 ;
}

x_font_t *
x_font_cache_get_xfont(
	x_font_cache_t *  font_cache ,
	ml_font_t  font
	)
{
	int  result ;
	x_font_t *  xfont ;
	KIK_PAIR( x_font)  fn_pair ;
	char *  fontname ;
	int  use_medium_for_bold ;
	u_int  col_width ;

	if( FONT_CS(font) == US_ASCII)
	{
		font &= ~US_ASCII ;
		font |= font_cache->usascii_font_cs ;
	}

	if( font == DEFAULT_FONT(font_cache->usascii_font_cs))
	{
		col_width = 0 ;
	}
	else
	{
		col_width = font_cache->usascii_font->width ;
	}

	kik_map_get( result , font_cache->xfont_table , font , fn_pair) ;
	if( result)
	{
		return  fn_pair->value ;
	}

	use_medium_for_bold = 0 ;
	
	if( ( fontname = x_get_config_font_name( font_cache->font_config , font_cache->font_size , font))
		== NULL)
	{
		if( font & FONT_BOLD)
		{
			if( ( fontname = x_get_config_font_name( font_cache->font_config ,
						font_cache->font_size , font & ~FONT_BOLD)))
			{
				use_medium_for_bold = 1 ;
			}
		}
	}

	if( ( xfont = x_font_new( font_cache->display , font , font_cache->font_config->type_engine ,
			font_cache->font_config->font_present , fontname ,
			font_cache->font_size , col_width , use_medium_for_bold)))
	{
		if( ! font_cache->use_multi_col_char)
		{
			x_change_font_cols( xfont , 1) ;
		}
	}
#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " font for id %x doesn't exist.\n" , font) ;
	}
#endif

	free( fontname) ;
	
	/*
	 * If this font doesn't exist, NULL(which indicates it) is cached.
	 */
	kik_map_set( result , font_cache->xfont_table , font , xfont) ;

#ifdef  DEBUG
	kik_warn_printf( KIK_DEBUG_TAG " font %x for id %x was cached.\n" , xfont , font) ;
#endif

	return  xfont ;
}

char *
x_get_font_name_list_for_fontset(
	x_font_cache_t *  font_cache
	)
{
	char *  font_name_list ;
	char *  p ;
	size_t  list_len ;
	
	if( font_cache->font_config->type_engine != TYPE_XCORE)
	{
		x_font_config_t *  font_config ;

		if( ( font_config = x_acquire_font_config( TYPE_XCORE ,
					font_cache->font_config->font_present & ~FONT_AA)) == NULL)
		{
			font_name_list = NULL ;
		}
		else
		{
			font_name_list = x_get_all_config_font_names( font_config , font_cache->font_size) ;

			x_release_font_config( font_config) ;
		}
	}
	else
	{
		font_name_list = x_get_all_config_font_names( font_cache->font_config ,
					font_cache->font_size) ;
	}

	if( font_name_list)
	{
		list_len = strlen( font_name_list) ;
	}
	else
	{
		list_len = 0 ;
	}

	if( ( p = malloc( list_len + 28 + DIGIT_STR_LEN(font_cache->font_size) + 1))
			== NULL)
	{
		return  font_name_list ;
	}

	if( font_name_list)
	{
		sprintf( p , "%s,-*-*-medium-r-*--%d-*-*-*-*-*" ,
			font_name_list , font_cache->font_size) ;
		free( font_name_list) ;
	}
	else
	{
		sprintf( p , "-*-*-medium-r-*--%d-*-*-*-*-*" ,
			font_cache->font_size) ;
	}

	return  p ;
}
