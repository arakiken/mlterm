/*
 *	$Id$
 */

#include  "x_font_cache.h"

#include  <stdio.h>		/* sprintf */
#include  <kiklib/kik_util.h>	/* DIGIT_STR_LEN */


/* --- static variables --- */

static x_font_cache_t **  font_caches ;
static u_int  num_of_caches ;
static int  leftward_double_drawing ;


/* --- static functions --- */

#ifdef  DEBUG

static void
dump_cached_fonts(
	x_font_cache_t *  font_cache
	)
{
	u_int  count ;
	u_int  size ;
	KIK_PAIR( x_font) *  f_array ;
	
	kik_debug_printf( KIK_DEBUG_TAG " cached fonts info\n") ;
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

/*
 * Call this function after init all members except font_table
 */
static int
init_usascii_font(
	x_font_cache_t *  font_cache
	)
{
	u_int  beg_font_size ;
	
	beg_font_size = font_cache->font_size ;
	while( ( font_cache->usascii_font = x_font_cache_get_xfont( font_cache ,
					NORMAL_FONT_OF(font_cache->usascii_font_cs))) == NULL)
	{
		if( ++ font_cache->font_size > x_get_max_font_size())
		{
			font_cache->font_size = x_get_min_font_size() ;
		}
		else if( font_cache->font_size == beg_font_size)
		{
			return  0 ;
		}
	}

	return  1 ;
}

static KIK_MAP( x_font)
xfont_table_new(void)
{
	KIK_MAP( x_font)  xfont_table ;
	
	kik_map_new_with_size( ml_font_t , x_font_t * , xfont_table ,
		kik_map_hash_int , kik_map_compare_int , 16) ;

	return  xfont_table ;
}

static int
xfont_table_delete(
	KIK_MAP( x_font)  xfont_table
	)
{
	int  count ;
	u_int  size ;
	KIK_PAIR( x_font) *  f_array ;

	kik_map_get_pairs_array( xfont_table , f_array , size) ;
	
	for( count = 0 ; count < size ; count ++)
	{
		if( f_array[count]->value != NULL)
		{
			x_font_delete( f_array[count]->value) ;
		}
	}

	kik_map_delete( xfont_table) ;

	return  1 ;
}


/* --- global functions --- */

void
x_set_use_leftward_double_drawing(
	int  use
	)
{
	leftward_double_drawing = use ;
}

x_font_cache_t *
x_acquire_font_cache(
	Display *  display ,
	u_int  font_size ,
	mkf_charset_t  usascii_font_cs ,
	x_font_config_t *  font_config ,
	int  use_multi_col_char ,
	u_int  letter_space
	)
{
	int  count ;
	x_font_cache_t *  font_cache ;
	void *  p ;

	for( count = 0 ; count < num_of_caches ; count ++)
	{
		if( font_caches[count]->display == display &&
			font_caches[count]->font_size == font_size &&
			font_caches[count]->usascii_font_cs == usascii_font_cs &&
			font_caches[count]->font_config == font_config &&
			font_caches[count]->use_multi_col_char == use_multi_col_char &&
			font_caches[count]->letter_space == letter_space)
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

	font_cache->xfont_table = xfont_table_new() ;

	font_cache->display = display ;
	font_cache->font_size = font_size ;
	font_cache->usascii_font_cs = usascii_font_cs ;
	font_cache->use_multi_col_char = use_multi_col_char ;
	font_cache->letter_space = letter_space ;
	font_cache->ref_count = 1 ;

	font_cache->prev_cache.font = 0 ;
	font_cache->prev_cache.xfont = NULL ;

	if( ! init_usascii_font( font_cache))
	{
		xfont_table_delete( font_cache->xfont_table) ;
		free( font_cache) ;

		return  NULL ;
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
			font_caches[count] = font_caches[--num_of_caches] ;
			
			xfont_table_delete( font_cache->xfont_table) ;
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

int
x_font_cache_unload(
	x_font_cache_t *  font_cache
	)
{
	/*
	 * Discarding existing cache.
	 */
	xfont_table_delete( font_cache->xfont_table) ;
	font_cache->usascii_font = NULL ;
	font_cache->prev_cache.font = 0 ;
	font_cache->prev_cache.xfont = NULL ;

	/*
	 * Creating new cache.
	 */
	font_cache->xfont_table = xfont_table_new() ;

	if( ! init_usascii_font( font_cache))
	{
	#ifdef  DEBUG
		kik_debug_printf( KIK_DEBUG_TAG
		" x_font_cache_unload failed. Should x_release_font_cache this font cache.\n") ;
	#endif
	
		return  0 ;
	}
	
	return  1 ;
}

int
x_font_cache_unload_all(void)
{
	int  count ;

	for( count = 0 ; count < num_of_caches ; count++)
	{
		x_font_cache_unload( font_caches[count]) ;
	}

	return  1 ;
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
	int  size_attr ;

	if( FONT_CS(font) == US_ASCII)
	{
		font &= ~US_ASCII ;
		font |= font_cache->usascii_font_cs ;
	}

	if( font_cache->prev_cache.xfont && font_cache->prev_cache.font == font)
	{
		return  font_cache->prev_cache.xfont ;
	}

	kik_map_get( font_cache->xfont_table , font , fn_pair) ;
	if( fn_pair)
	{
		return  fn_pair->value ;
	}

	if( font == NORMAL_FONT_OF(font_cache->usascii_font_cs))
	{
		col_width = 0 ;
	}
	else
	{
		col_width = font_cache->usascii_font->width ;
	}

	use_medium_for_bold = 0 ;

	if( ( fontname = x_get_config_font_name( font_cache->font_config ,
				font_cache->font_size , font)) == NULL)
	{
		ml_font_t  next_font ;
		int  scalable ;

		next_font = font ;

	#if ! defined(USE_WIN32GUI) && ! defined(USE_FRAMEBUFFER)
		if( font_cache->font_config->type_engine == TYPE_XCORE)
		{
			/*
			 * If the type engine doesn't support scalable fonts,
			 * medium weight font (drawn doubly) is used for bold.
			 */
			scalable = 0 ;
		}
		else
	#endif
		{
			/*
			 * If the type engine supports scalable fonts,
			 * the face of medium weight / r slant font is used
			 * for bold and italic.
			 */
			scalable = 1 ;
		}

		while( next_font & (FONT_BOLD|FONT_ITALIC))
		{
			if( next_font & FONT_BOLD)
			{
				next_font &= ~FONT_BOLD ;
			}
			else /* if( next_font & FONT_ITALIC) */
			{
				if( ! scalable)
				{
					break ;
				}

				next_font &= ~FONT_ITALIC ;
			}

			if( ( fontname = x_get_config_font_name( font_cache->font_config ,
						font_cache->font_size , next_font)))
			{
				if( ( font & FONT_BOLD) && ! scalable)
				{
					use_medium_for_bold = 1 ;
				}

				goto  found ;
			}
		}
	}

found:
	size_attr = SIZE_ATTR_OF(font) ;

	if( ( xfont = x_font_new( font_cache->display , FONT_WITHOUT_SIZE_ATTR(font) , size_attr ,
				font_cache->font_config->type_engine ,
				font_cache->font_config->font_present , fontname ,
				font_cache->font_size , col_width , use_medium_for_bold ,
				font_cache->letter_space)) ||
	    ( size_attr &&
	      ( xfont = x_font_new( font_cache->display ,
				NORMAL_FONT_OF(font_cache->usascii_font_cs) , size_attr ,
				font_cache->font_config->type_engine ,
				font_cache->font_config->font_present , fontname ,
				font_cache->font_size , col_width , use_medium_for_bold ,
				font_cache->letter_space))))
	{
		if( ! font_cache->use_multi_col_char)
		{
			x_change_font_cols( xfont , 1) ;
		}

		if( xfont->double_draw_gap && leftward_double_drawing)
		{
			xfont->double_draw_gap = -1 ;
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

	font_cache->prev_cache.font = font ;
	font_cache->prev_cache.xfont = xfont ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Font %x for id %x was cached.%s\n" ,
		xfont , font , use_medium_for_bold ? "(medium font is used for bold.)" : "") ;
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
