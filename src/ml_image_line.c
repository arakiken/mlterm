/*
 *	update: <2001/11/26(02:32:46)>
 *	$Id$
 */

#include  "ml_image_line.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>		/* strdup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>


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

int
ml_imgline_reset(
	ml_image_line_t *  line
	)
{
	line->num_of_filled_chars = 0 ;
	line->num_of_filled_cols = 0 ;
	line->is_continued_to_next = 0 ;

	ml_imgline_update_change_char_index( line , 0 , 0 , 1) ;

	return 1 ;
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

/*
 * this function copys change_{beg|end}_char_index/is_modified member as it is.
 */
void
ml_imgline_copy_line(
	ml_image_line_t *  dst ,
	ml_image_line_t *  src
	)
{
	memcpy( dst->chars , src->chars , sizeof( ml_char_t) * src->num_of_filled_chars) ;
	
	dst->num_of_filled_chars = src->num_of_filled_chars ;
	dst->num_of_filled_cols = src->num_of_filled_cols ;

	dst->change_beg_char_index = src->change_beg_char_index ;
	dst->change_end_char_index = src->change_end_char_index ;
	dst->is_modified = src->is_modified ;
	dst->is_continued_to_next = src->is_continued_to_next ;
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
