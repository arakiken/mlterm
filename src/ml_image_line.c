/*
 *	$Id$
 */

#include  "ml_image_line.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>		/* strdup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>

#include  "ml_bidi.h"


/* --- static variables --- */

static char *  word_separators = " ,.:;/@" ;
static int  separators_are_allocated = 0 ;


/* --- static functions --- */

static int
is_word_separator(
	ml_char_t *  ch
	)
{
	char *  p ;
	char  c ;

	if( ml_char_cs(ch) != US_ASCII)
	{
		return  0 ;
	}

	p = word_separators ;
	c = ml_char_bytes(ch)[0] ;

	while( *p)
	{
		if( c == *p)
		{
			return  1 ;
		}

		p ++ ;
	}

	return  0 ;
}


/* --- global functions --- */

int
ml_set_word_separators(
	char *  seps
	)
{
	if( separators_are_allocated)
	{
		free( word_separators) ;
	}
	else
	{
		separators_are_allocated = 1 ;
	}
	
	word_separators = strdup( seps) ;

	return  1 ;
}

int
ml_free_word_separators(void)
{
	if( separators_are_allocated)
	{
		free( word_separators) ;
		separators_are_allocated = 0 ;
	}

	return  1 ;
}


u_int
ml_get_cols_of(
	ml_char_t *  chars ,
	u_int  len
	)
{
	int  counter ;
	u_int  cols ;

	cols = 0 ;

	for( counter = 0 ; counter < len ; counter ++)
	{
		cols += ml_char_cols( &chars[counter]) ;
	}

	return  cols ;
}


int
ml_imgline_init(
	ml_image_line_t *  line ,
	u_int  num_of_chars
	)
{
	memset( line , 0 , sizeof( ml_image_line_t)) ;

	if( ( line->chars = ml_str_new( num_of_chars)) == NULL)
	{
		return  0 ;
	}

	line->num_of_chars = num_of_chars ;

	return  1 ;
}

int
ml_imgline_clone(
	ml_image_line_t *  clone ,
	ml_image_line_t *  orig ,
	u_int  num_of_chars
	)
{
	ml_imgline_init( clone , num_of_chars) ;
	
	if( ml_imgline_is_using_bidi( orig))
	{
		ml_imgline_use_bidi( clone) ;
	}

	ml_imgline_copy_line( clone , orig) ;

	return  1 ;
}

int
ml_imgline_final(
	ml_image_line_t *  line
	)
{
	if( line->chars)
	{
		ml_str_delete( line->chars , line->num_of_chars) ;
	}
	
	free( line->visual_order) ;

	return  1 ;
}

int
ml_imgline_reset(
	ml_image_line_t *  line
	)
{
	if( line->num_of_filled_chars > 0)
	{
		ml_imgline_update_change_char_index( line , 0 , 0 , 1) ;
	}

	line->num_of_filled_chars = 0 ;
	line->num_of_filled_cols = 0 ;
	line->visual_order_len = 0 ;
	line->is_continued_to_next = 0 ;
	line->is_bidi = 0 ;

	return 1 ;
}

int
ml_imgline_clear(
	ml_image_line_t *  line ,
	ml_char_t *  sp_ch
	)
{
	ml_imgline_reset( line) ;
	
	ml_char_copy( &line->chars[0] , sp_ch) ;
	line->num_of_filled_chars = 1 ;
	
	return  1 ;
}

void
ml_imgline_update_change_char_index(
	ml_image_line_t *  line ,
	int  beg_char_index ,
	int  end_char_index ,
	int  is_cleared_to_end
	)
{
	if( beg_char_index > end_char_index)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " beg_char_index %d > end_char_index %d\n" ,
			beg_char_index , end_char_index) ;
	#endif
	
		return ;
	}

	if( line->is_modified)
	{
		if( beg_char_index < line->change_beg_char_index)
		{
			line->change_beg_char_index = beg_char_index ;
		}

		if( end_char_index > line->change_end_char_index)
		{
			line->change_end_char_index = end_char_index ;
		}

		line->is_cleared_to_end |= is_cleared_to_end ;
	}
	else
	{
		line->is_modified = 1 ;
		line->change_beg_char_index = beg_char_index ;
		line->change_end_char_index = end_char_index ;
		line->is_cleared_to_end = is_cleared_to_end ;
	}
}

void
ml_imgline_set_modified(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		ml_imgline_update_change_char_index( line , 0 , 0 , 1) ;
	}
	else
	{
		ml_imgline_update_change_char_index( line , 0 , END_CHAR_INDEX(*line) , 1) ;
	}
}

void
ml_imgline_is_updated(
	ml_image_line_t *  line
	)
{
	line->is_modified = 0 ;
}

int
ml_imgline_get_beg_of_modified(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		return  0 ;
	}
	else
	{
		return  K_MIN(line->change_beg_char_index,END_CHAR_INDEX(*line)) ;
	}
}

u_int
ml_imgline_get_num_of_redrawn_chars(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		return  0 ;
	}
	else
	{
		return  K_MIN(line->change_end_char_index,END_CHAR_INDEX(*line)) -
			K_MIN(line->change_beg_char_index,END_CHAR_INDEX(*line)) + 1 ;
	}
}

int
ml_convert_char_index_to_x(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	int  counter ;
	int  x ;

	if( char_index > END_CHAR_INDEX(*line))
	{
		char_index = END_CHAR_INDEX(*line) ;
	}

	/*
	 * excluding the last char width.
	 */
	x = 0 ;
	for( counter = 0 ; counter < char_index ; counter ++)
	{
		x += ml_char_width( &line->chars[counter]) ;
	}

	return  x ;
}

int
ml_convert_x_to_char_index(
	ml_image_line_t *  line ,
	u_int *  x_rest ,
	int  x
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < END_CHAR_INDEX(*line) ; counter ++)
	{
		if( x < ml_char_width( &line->chars[counter]))
		{
			break ;
		}

		x -= ml_char_width( &line->chars[counter]) ;
	}

	if( x_rest != NULL)
	{
		*x_rest = x ;
	}

	return  counter ;
}

int
ml_imgline_reverse_color(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( char_index > END_CHAR_INDEX(*line))
	{
		return  0 ;
	}

	ml_char_reverse_color( &line->chars[char_index]) ;

	ml_imgline_update_change_char_index( line , char_index , char_index , 0) ;

	return  1 ;
}

int
ml_imgline_restore_color(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( char_index > END_CHAR_INDEX(*line))
	{
		return  0 ;
	}

	ml_char_restore_color( &line->chars[char_index]) ;

	ml_imgline_update_change_char_index( line , char_index , char_index , 0) ;

	return  1 ;
}

int
ml_imgline_copy_line(
	ml_image_line_t *  dst ,
	ml_image_line_t *  src
	)
{
	u_int  copy_len ;

	copy_len = K_MIN(src->num_of_filled_chars,dst->num_of_chars) ;
	
	ml_imgline_copy_str( src , dst->chars , 0 , copy_len) ;

	dst->num_of_filled_chars = copy_len ;

	if( copy_len == src->num_of_filled_chars)
	{
		dst->num_of_filled_cols = src->num_of_filled_cols ;
	}
	else
	{
		dst->num_of_filled_cols = ml_get_cols_of( dst->chars , dst->num_of_filled_chars) ;
	}

	dst->change_beg_char_index = src->change_beg_char_index ;
	dst->change_end_char_index = src->change_end_char_index ;
	dst->is_modified = src->is_modified ;
	dst->is_continued_to_next = src->is_continued_to_next ;
	dst->is_bidi = src->is_bidi ;

	if( dst->visual_order)
	{
		if( copy_len == src->num_of_filled_chars && src->visual_order)
		{
			memcpy( dst->visual_order , src->visual_order , sizeof( u_int16_t) * copy_len) ;
			dst->visual_order_len = src->visual_order_len ;
		}
		else
		{
			ml_imgline_render_bidi( dst) ;
		}
	}
	else
	{
		dst->visual_order_len = 0 ;
	}

	return  1 ;
}

int
ml_imgline_share(
	ml_image_line_t *  dst ,
	ml_image_line_t *  src
	)
{
	dst->chars = src->chars ;
	dst->num_of_filled_chars = src->num_of_filled_chars ;
	dst->num_of_filled_cols = src->num_of_filled_cols ;
	
	dst->visual_order = src->visual_order ;
	dst->visual_order_len = src->visual_order_len ;

	dst->change_beg_char_index = src->change_beg_char_index ;
	dst->change_end_char_index = src->change_end_char_index ;
	dst->is_modified = src->is_modified ;
	dst->is_continued_to_next = src->is_continued_to_next ;
	dst->is_bidi = src->is_bidi ;

	return  1 ;
}

int
ml_imgline_is_empty(
	ml_image_line_t *  line
	)
{
	return  (line->num_of_filled_chars == 0) ;
}

u_int
ml_get_num_of_filled_chars_except_end_space(
	ml_image_line_t *  line
	)
{
	ml_char_t *  ch ;
	int  char_index ;

	for( char_index = END_CHAR_INDEX(*line) ; char_index >= 0 ; char_index --)
	{
		ch = &line->chars[char_index] ;
		
		if( ml_char_size( ch) != 1 || ml_char_bytes( ch)[0] != ' ')
		{
			return  char_index + 1 ;
		}
	}

	return  0 ;
}

int
ml_imgline_get_word_pos(
	ml_image_line_t *  line ,
	int *  beg_char_index ,
	int *  end_char_index ,
	int  char_index
	)
{
	int  orig_char_index ;
	ml_char_t *  ch ;

	if( is_word_separator(&line->chars[char_index]))
	{
		*beg_char_index = char_index ;
		*end_char_index = char_index ;

		return  1 ;
	}
	
	orig_char_index = char_index ;

	/*
	 * search the beg of word
	 */
	while( 1)
	{
		ch = &line->chars[char_index] ;

		if( is_word_separator(ch))
		{
			*beg_char_index = char_index + 1 ;

			break ;
		}

		if( char_index == 0)
		{
			*beg_char_index = 0 ;
			
			break ;
		}
		else
		{
			char_index -- ;
		}
	}

	/*
	 * search the end of word.
	 */
	char_index = orig_char_index ;
	while( 1)
	{
		ch = &line->chars[char_index] ;

		if( is_word_separator(ch))
		{
			*end_char_index = char_index - 1 ;

			break ;
		}

		if( char_index == END_CHAR_INDEX(*line))
		{
			*end_char_index = char_index ;

			break ;
		}
		else
		{
			char_index ++ ;
		}
	}

	return  1 ;
}

int
ml_imgline_is_using_bidi(
	ml_image_line_t *  line
	)
{
	if( line->visual_order)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_imgline_use_bidi(
	ml_image_line_t *  line
	)
{
	if( line->visual_order)
	{
		return  1 ;
	}

	if( ( line->visual_order = malloc( sizeof( u_int16_t) * line->num_of_chars)) == NULL)
	{
		return  0 ;
	}

	line->visual_order_len = 0 ;

	return  1 ;
}

int
ml_imgline_unuse_bidi(
	ml_image_line_t *  line
	)
{
	free( line->visual_order) ;
	line->visual_order = NULL ;
	line->visual_order_len = 0 ;

	return  1 ;
}

int
ml_imgline_render_bidi(
	ml_image_line_t *  line
	)
{
	int  len ;

	if( ! line->visual_order || line->is_bidi)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " render failed. visual_order is %p and is_bidi is %d\n" ,
			line->visual_order , line->is_bidi) ;
	#endif
	
		return  0 ;
	}
	else if( line->num_of_filled_chars == 0 ||
		(len = ml_get_num_of_filled_chars_except_end_space( line)) == 0)
	{
		return  1 ;
	}

	line->visual_order_len = len ;
	ml_bidi( line->visual_order , line->chars , line->visual_order_len) ;

	return  1 ;
}

int
ml_imgline_start_bidi(
	ml_image_line_t *  line
	)
{
	int  counter ;
	ml_char_t *  src ;
	
	if( ! line->visual_order)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " visual bidi failed..\n") ;
	#endif

		return  0 ;
	}
	
	if( line->is_bidi)
	{
		return  1 ;
	}

	if( line->visual_order_len == 0)
	{
		line->is_bidi = 1 ;

		return  1 ;
	}

	if( ( src = ml_str_alloca( line->visual_order_len)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( src , line->chars , line->visual_order_len) ;

	for( counter = 0 ; counter < line->visual_order_len ; counter ++)
	{
	#ifdef  DEBUG
		if( line->visual_order[counter] >= line->visual_order_len)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" visual order(%d) of %d is illegal.\n" ,
				line->visual_order[counter] , counter) ;
				
			abort() ;
		}
	#endif
	
		ml_char_copy( &line->chars[line->visual_order[counter]] , &src[counter]) ;
	}

	ml_imgline_set_modified( line) ;

	line->is_bidi = 1 ;

	return  1 ;
}

int
ml_imgline_stop_bidi(
	ml_image_line_t *  line
	)
{
	int  counter ;
	ml_char_t *  src ;
	
	if( ! line->visual_order || ! line->is_bidi)
	{
		return  1 ;
	}

	if( line->visual_order_len == 0)
	{
		line->is_bidi = 0 ;

		return  1 ;
	}

	if( ( src = ml_str_alloca( line->visual_order_len)) == NULL)
	{
		return  0 ;
	}

	ml_str_copy( src , line->chars , line->visual_order_len) ;

	for( counter = 0 ; counter < line->visual_order_len ; counter ++)
	{
	#ifdef  DEBUG
		if( line->visual_order[counter] >= line->visual_order_len)
		{
			kik_debug_printf( KIK_DEBUG_TAG
				" visual order(%d) of %d is illegal.\n" ,
				line->visual_order[counter] , counter) ;

			abort() ;
		}
	#endif
		
		ml_char_copy( &line->chars[counter] , &src[line->visual_order[counter]]) ;
	}

	ml_str_final( src , line->visual_order_len) ;

	line->is_bidi = 0 ;

	/*
	 * !! Notice !!
	 * is_modified is as it is , which should not be touched here.
	 */

	return  1 ;
}

int
ml_convert_char_index_normal_to_bidi(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( ! line->visual_order)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " visual_order is NULL\n") ;
	#endif
	
		return  char_index ;
	}
	
	if( 0 <= char_index && char_index < line->visual_order_len)
	{
		return  line->visual_order[char_index] ;
	}
	else
	{
		return  char_index ;
	}
}

int
ml_convert_char_index_bidi_to_normal(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	u_int  counter ;

	if( ! line->visual_order)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " visual_order is NULL\n") ;
	#endif
	
		return  char_index ;
	}
	
	for( counter = 0 ; counter < line->visual_order_len ; counter ++)
	{
		if( char_index == line->visual_order[counter])
		{
			return  counter ;
		}
	}

	return  char_index ;
}

int
ml_imgline_copy_str(
	ml_image_line_t *  line ,
	ml_char_t *  dst ,
	int  beg ,
	u_int  len
	)
{
	if( ! line->is_bidi)
	{
		return  ml_str_copy( dst , &line->chars[beg] , len) ;
	}
	else
	{
		/*
		 * XXX  adhoc !
		 */
		 
		int *  flags ;
		int  bidi_pos ;
		int  norm_pos ;
		int  dst_pos ;

		if( ( flags = alloca( sizeof( int) * line->visual_order_len)) == NULL)
		{
			return  0 ;
		}

		memset( flags , 0 , sizeof( int) * line->visual_order_len) ;

		for( bidi_pos = beg ; bidi_pos < beg + len ; bidi_pos ++)
		{
			for( norm_pos = 0 ; norm_pos < line->visual_order_len ; norm_pos ++)
			{
				if( line->visual_order[norm_pos] == bidi_pos)
				{
					flags[norm_pos] = 1 ;
				}
			}
		}

		for( dst_pos = norm_pos = 0 ; norm_pos < line->visual_order_len ; norm_pos ++)
		{
			if( flags[norm_pos])
			{
				ml_char_copy( &dst[dst_pos ++] ,
					&line->chars[line->visual_order[norm_pos]]) ;
			}
		}

		return  1 ;
	}
}
