/*
 *	$Id$
 */

#include  "ml_image_line.h"

#include  <stdio.h>		/* fprintf */
#include  <string.h>		/* memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>

#include  "ml_bidi.h"


#ifdef  DEBUG

#define  END_CHAR_INDEX(line) \
	( (line)->num_of_filled_chars == 0 && \
		kik_warn_printf( "END_CHAR_INDEX()[" __FUNCTION__  "] num_of_filled_chars is 0.\n") ? \
		0 : (line)->num_of_filled_chars - 1 )

#else

#define  END_CHAR_INDEX(line)  ((line)->num_of_filled_chars - 1)

#endif


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

/*
 * !! Notice !!
 * cloning the very same including bidi parameters.
 */
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

	if( line->visual_order)
	{
		free( line->visual_order) ;
	}

	return  1 ;
}

/*
 * return: actually broken rows.
 */
u_int
ml_imgline_break_boundary(
	ml_image_line_t *  line ,
	u_int  size ,
	ml_char_t *  sp_ch
	)
{
	int  counter ;

	if( line->num_of_filled_chars + size > line->num_of_chars)
	{
		/* over line length */

	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " it failed to break from col %d by size %d" ,
			END_CHAR_INDEX(line) , size) ;
	#endif

		size = line->num_of_chars - line->num_of_filled_chars ;

	#ifdef  DEBUG
		fprintf( stderr , " ... size modified -> %d\n" , size) ;
	#endif
	}

	if( size == 0)
	{
		/* nothing is done */
		
		return  0 ;
	}

	/* padding spaces */
	for( counter = line->num_of_filled_chars ; counter < line->num_of_filled_chars + size ;
		counter ++)
	{
		ml_char_copy( &line->chars[counter] , sp_ch) ;
	}

	line->num_of_filled_chars += size ;
	line->num_of_filled_cols += ( ml_char_cols(sp_ch) * size) ;

	/*
	 * change char index is not updated , because space has no glyph.
	 */

	return  size ;
}

int
ml_imgline_reset(
	ml_image_line_t *  line
	)
{
	if( line->num_of_filled_chars > 0)
	{
		ml_imgline_set_modified( line , 0 , 0 , 1) ;
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
	int  char_index ,
	ml_char_t *  sp_ch
	)
{
	if( char_index == 0)
	{
		ml_imgline_reset( line) ;

		ml_char_copy( &line->chars[0] , sp_ch) ;
		line->num_of_filled_chars = 1 ;
		line->num_of_filled_cols = 1 ;
	}
	else
	{
		if( char_index >= line->num_of_filled_chars)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " nothing is cleared.\n") ;
		#endif

			return  1 ;
		}

		ml_char_copy( &line->chars[char_index] , sp_ch) ;
		line->num_of_filled_chars = char_index + 1 ;
		line->num_of_filled_cols = ml_str_cols( line->chars , line->num_of_filled_chars) ;

		ml_imgline_set_modified( line , char_index , END_CHAR_INDEX(line) , 1) ;
	}
	
	return  1 ;
}

int
ml_imgline_overwrite_chars(
	ml_image_line_t *  line ,
	int  change_char_index ,
	ml_char_t *  chars ,
	u_int  len ,
	u_int  cols ,
	ml_char_t *  sp_ch
	)
{
	int  counter ;
	int  char_index ;
	u_int  cols_rest ;
	u_int  padding ;
	u_int  new_len ;
	u_int  copy_len ;

	if( cols >= line->num_of_filled_cols || ml_imgline_is_empty( line))
	{
		return  ml_imgline_overwrite_all( line , change_char_index , chars , len , cols) ;
	}

	/* trying to delete from pos 0 to the pos just before this char_index */
	char_index = ml_convert_col_to_char_index( line , &cols_rest , cols , 0) ;

	if( char_index == END_CHAR_INDEX(line) && cols_rest >= 1)
	{
		return  ml_imgline_overwrite_all( line , change_char_index , chars , len , cols) ;
	}

	if( 1 <= cols_rest && cols_rest < ml_char_cols( &line->chars[char_index]))
	{
		padding = ml_char_cols( &line->chars[char_index]) - cols_rest ;
		char_index ++ ;
	}
	else
	{
		padding = 0 ;
	}

	copy_len = line->num_of_filled_chars - char_index ;
	new_len = len + padding + copy_len ;
	if( new_len > line->num_of_chars)
	{
	#ifdef DEBUG
		kik_warn_printf(
			KIK_DEBUG_TAG " line length %d(ow %d copy %d) after overwriting is overflowed\n" ,
			new_len , len , copy_len) ;
	#endif
		
		new_len = line->num_of_chars ;

	#ifdef  DEBUG
		if( new_len < padding + copy_len)
		{
			kik_error_printf( KIK_DEBUG_TAG "\n") ;

			abort() ;
		}
	#endif
		
		len = new_len - padding - copy_len ;

	#ifdef  DEBUG
		fprintf( stderr , " ... modified -> new_len %d , ow len %d\n" , new_len , len) ;
	#endif
	}

	ml_str_copy( &line->chars[len + padding] , &line->chars[char_index] , copy_len) ;

	for( counter = 0 ; counter < padding ; counter ++)
	{
		ml_char_copy( &line->chars[len + counter] , sp_ch) ;
	}

	ml_str_copy( line->chars , chars , len) ;

	line->num_of_filled_chars = new_len ;
	
	/*
	 * reculculating width...
	 */
	 
	for( counter = len ; counter <= END_CHAR_INDEX(line) ; counter ++)
	{
		cols += ml_char_cols( &line->chars[counter]) ;
	}

	if( line->num_of_filled_cols > cols)
	{
		ml_imgline_set_modified( line , change_char_index ,
			K_MAX(change_char_index,END_CHAR_INDEX(line)) , 1) ;
	}
	else
	{
		ml_imgline_set_modified( line , change_char_index ,
			K_MAX(change_char_index,len - 1) , 0) ;
	}

	line->num_of_filled_cols = cols ;

	return  1 ;
}

int
ml_imgline_overwrite_all(
	ml_image_line_t *  line ,
	int  change_char_index ,
	ml_char_t *  chars ,
	int  len ,
	u_int  cols
	)
{
	ml_str_copy( line->chars , chars , len) ;

	line->num_of_filled_chars = len ;

	if( line->num_of_filled_cols > cols)
	{
		ml_imgline_set_modified( line , change_char_index , END_CHAR_INDEX(line) , 1) ;
	}
	else
	{
		ml_imgline_set_modified( line , change_char_index , END_CHAR_INDEX(line) , 0) ;
	}
	
	line->num_of_filled_cols = cols ;

	return  1 ;
}

int
ml_imgline_fill_all(
	ml_image_line_t *  line ,
	ml_char_t *  ch ,
	u_int  num
	)
{
	int  char_index ;
	int  cols ;

	num = K_MIN(num,line->num_of_chars) ;

	cols = 0 ;
	for( char_index = 0 ; char_index < num ; char_index ++)
	{
		ml_char_copy( &line->chars[char_index] , ch) ;
		cols += ml_char_cols( ch) ;
	}

	line->num_of_filled_chars = char_index ;
	line->num_of_filled_cols = cols ;

	ml_imgline_set_modified_all( line) ;

	return  1 ;
}

void
ml_imgline_set_modified(
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
ml_imgline_set_modified_all(
	ml_image_line_t *  line
	)
{
	if( ml_imgline_is_empty( line))
	{
		ml_imgline_set_modified( line , 0 , 0 , 1) ;
	}
	else
	{
		ml_imgline_set_modified( line , 0 , END_CHAR_INDEX(line) , 1) ;
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
		return  K_MIN(line->change_beg_char_index,END_CHAR_INDEX(line)) ;
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
		return  K_MIN(line->change_end_char_index,END_CHAR_INDEX(line)) -
			K_MIN(line->change_beg_char_index,END_CHAR_INDEX(line)) + 1 ;
	}
}

int
ml_convert_char_index_to_col(
	ml_image_line_t *  line ,
	int  char_index ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	int  counter ;
	int  col ;

	if( line->num_of_filled_chars == 0)
	{
		return  0 ;
	}

#ifdef  DEBUG
	if( char_index >= line->num_of_chars)
	{
		/* this must never happen */
		
		kik_warn_printf( KIK_DEBUG_TAG
			" char index %d is larger than num_of_chars(%d)\n" ,
			char_index , line->num_of_chars) ;
			
		abort() ;
	}
#endif
	
	col = 0 ;

	if( (flag & BREAK_BOUNDARY) && line->num_of_filled_chars <= char_index)
	{
		for( counter = 0 ; counter < line->num_of_filled_chars ; counter ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &line->chars[counter]) == 0)
			{
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
			
				continue ;
			}
		#endif
		
			col += ml_char_cols( &line->chars[counter]) ;
		}

		col += (char_index - counter) ;
	}
	else
	{
		/*
		 * excluding the width of the last char.
		 */
		for( counter = 0 ; counter < K_MIN(char_index,END_CHAR_INDEX(line)) ; counter ++)
		{
		#ifdef  DEBUG
			if( ml_char_cols( &line->chars[counter]) == 0)
			{
				kik_warn_printf( KIK_DEBUG_TAG " ml_char_cols returns 0.\n") ;
				
				continue ;
			}
		#endif

			col += ml_char_cols( &line->chars[counter]) ;
		}
	}

	return  col ;
}

int
ml_convert_col_to_char_index(
	ml_image_line_t *  line ,
	int *  cols_rest ,
	int  col ,
	int  flag	/* BREAK_BOUNDARY */
	)
{
	int  char_index ;

	if( line->num_of_filled_chars == 0)
	{
		return  0 ;
	}

#ifdef  DEBUG
	if( col >= line->num_of_chars)
	{
		/* this must never happen */
		
		kik_warn_printf( KIK_DEBUG_TAG " col %d is larger than num_of_chars(%d)" ,
			col , line->num_of_chars) ;

		abort() ;
	}
#endif

	char_index = 0 ;
	
	for( ; char_index < END_CHAR_INDEX(line) ; char_index ++)
	{
		if( col < ml_char_cols( &line->chars[char_index]))
		{
			goto  end ;
		}

		col -= ml_char_cols( &line->chars[char_index]) ;
	}
	
	if( col < ml_char_cols( &line->chars[char_index]))
	{
		goto  end ;
	}
	else if( flag & BREAK_BOUNDARY)
	{
		col -= ml_char_cols( &line->chars[char_index++]) ;
		
		while( col > 0)
		{
			col -- ;
			char_index ++ ;
		}
	}

end:
	if( cols_rest != NULL)
	{
		*cols_rest = col ;
	}

	return  char_index ;
}

int
ml_convert_char_index_to_x(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	int  counter ;
	int  x ;

	if( char_index > END_CHAR_INDEX(line))
	{
		char_index = END_CHAR_INDEX(line) ;
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
	
	for( counter = 0 ; counter < END_CHAR_INDEX(line) ; counter ++)
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
	if( char_index > END_CHAR_INDEX(line))
	{
		return  0 ;
	}

	ml_char_reverse_color( &line->chars[char_index]) ;

	ml_imgline_set_modified( line , char_index , char_index , 0) ;

	return  1 ;
}

int
ml_imgline_restore_color(
	ml_image_line_t *  line ,
	int  char_index
	)
{
	if( char_index > END_CHAR_INDEX(line))
	{
		return  0 ;
	}

	ml_char_restore_color( &line->chars[char_index]) ;

	ml_imgline_set_modified( line , char_index , char_index , 0) ;

	return  1 ;
}

/*
 * !! Notice !!
 * this copys src parameters to dst if at all possible.
 * if visual_order of dst is NULL , this ignores bidi related parameters.
 */
int
ml_imgline_copy_line(
	ml_image_line_t *  dst ,
	ml_image_line_t *  src
	)
{
	u_int  copy_len ;

	copy_len = K_MIN(src->num_of_filled_chars,dst->num_of_chars) ;

	/* getting normalized str */
	ml_imgline_copy_str( src , dst->chars , 0 , copy_len) ;

	dst->num_of_filled_chars = copy_len ;

	if( copy_len == src->num_of_filled_chars)
	{
		dst->num_of_filled_cols = src->num_of_filled_cols ;
	}
	else
	{
		dst->num_of_filled_cols = ml_str_cols( dst->chars , dst->num_of_filled_chars) ;
	}

	dst->is_modified = src->is_modified ;
	dst->change_beg_char_index = K_MIN(src->change_beg_char_index,dst->num_of_chars) ;
	dst->change_end_char_index = K_MIN(src->change_end_char_index,dst->num_of_chars) ;
	dst->is_cleared_to_end = src->is_cleared_to_end ;

	dst->is_continued_to_next = src->is_continued_to_next ;
	dst->is_bidi = 0 ;

	if( ml_imgline_is_using_bidi( src) && ml_imgline_is_using_bidi( dst))
	{
		ml_imgline_render_bidi( dst) ;

		if( src->is_bidi)
		{
			ml_imgline_start_bidi( dst) ;
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

inline int
ml_imgline_end_char_index(
	ml_image_line_t *  line
	)
{
	if( line->num_of_filled_chars == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " num_of_filled_chars is 0.\n") ;
	#endif
	
		return  0 ;
	}
	else
	{
		return  line->num_of_filled_chars - 1 ;
	}
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

	for( char_index = END_CHAR_INDEX(line) ; char_index >= 0 ; char_index --)
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

		if( char_index == END_CHAR_INDEX(line))
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
	#ifdef  __DEBUG
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

	if( line->is_modified)
	{
		ml_imgline_set_modified_all( line) ;
	}

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
