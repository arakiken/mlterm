/*
 *	$Id$
 */

#include  "ml_backscroll.h"

#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>


#define  ROW_IN_LOGS(bs_image,row) \
	( ml_get_num_of_logged_lines((bs_image)->logs) + row)

#if  0
#define  __DEBUG
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
ml_bs_init(
	ml_bs_image_t *  bs_image ,
	ml_image_t *  image ,
	ml_logs_t *  logs ,
	ml_bs_event_listener_t *  bs_listener ,
	ml_char_t *  nl_ch
	)
{
	bs_image->image = image ;
	bs_image->logs = logs ;
	bs_image->bs_listener = bs_listener ;

	bs_image->backscroll_rows = 0 ;
	bs_image->is_backscroll_mode = 0 ;

	ml_char_init( &bs_image->nl_ch) ;
	ml_char_copy( &bs_image->nl_ch , nl_ch) ;

	return  1 ;
}

int
ml_bs_final(
	ml_bs_image_t *  bs_image
	)
{
	ml_char_final( &bs_image->nl_ch) ;
	
	return  1 ;
}

int
ml_bs_change_image(
	ml_bs_image_t *  bs_image ,
	ml_image_t *  image
	)
{
	bs_image->image = image ;

	return  1 ;
}

int
ml_is_backscroll_mode(
	ml_bs_image_t *  bs_image
	)
{
	return  bs_image->is_backscroll_mode ;
}

void
ml_set_backscroll_mode(
	ml_bs_image_t *  bs_image
	)
{
	bs_image->is_backscroll_mode = 1 ;
}

void
ml_unset_backscroll_mode(
	ml_bs_image_t *  bs_image
	)
{
	bs_image->is_backscroll_mode = 0 ;
	bs_image->backscroll_rows = 0 ;
}
	
int
ml_convert_row_to_bs_row(
	ml_bs_image_t *  bs_image ,
	int  row
	)
{
	return row - bs_image->backscroll_rows ;
}

ml_image_line_t *
ml_bs_get_image_line_in_all(
	ml_bs_image_t *  bs_image ,
	int  row
	)
{
	u_int  num_of_filled_lines ;

	num_of_filled_lines = ml_get_num_of_logged_lines( bs_image->logs) ;

	if( num_of_filled_lines < bs_image->backscroll_rows)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " backscroll is overflowed.\n") ;
	#endif

		return  NULL ;
	}
	else if( row == ml_image_get_rows( bs_image->image))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " row %d is at the end of screen.\n" , row) ;
	#endif
	
		return  NULL ;
	}

	if( row < 0)
	{
		return  ml_log_get( bs_image->logs , ROW_IN_LOGS(bs_image,row)) ;
	}
	else
	{
		return  ml_image_get_line( bs_image->image , row) ;
	}
}

ml_image_line_t *
ml_bs_get_image_line_in_screen(
	ml_bs_image_t *  bs_image ,
	int  row_in_scr
	)
{
	if( bs_image->is_backscroll_mode && bs_image->backscroll_rows > 0)
	{
		u_int  num_of_filled_lines ;

		num_of_filled_lines = ml_get_num_of_logged_lines( bs_image->logs) ;

		if( num_of_filled_lines < bs_image->backscroll_rows)
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " backscroll is overflowed.\n") ;
		#endif
			
			return  NULL ;
		}
		else if( row_in_scr == ml_image_get_rows( bs_image->image))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " row %d is at the end of screen.\n") ;
		#endif
		
			return  NULL ;
		}

		if( row_in_scr < bs_image->backscroll_rows)
		{
			return  ml_log_get( bs_image->logs ,
				ROW_IN_LOGS(bs_image,row_in_scr - bs_image->backscroll_rows)) ;
		}
		else
		{
			return  ml_image_get_line( bs_image->image ,
				row_in_scr - bs_image->backscroll_rows) ;
		}
	}
	else
	{
		return  ml_image_get_line( bs_image->image , row_in_scr) ;
	}
}

int
ml_bs_scroll_to(
	ml_bs_image_t *  bs_image ,
	int  beg_row
	)
{
	int  counter ;
	ml_image_line_t *  line ;

	if( ! bs_image->is_backscroll_mode)
	{
		return  0 ;
	}

	if( beg_row > 0)
	{
		bs_image->backscroll_rows = 0 ;
	}
	else
	{
		bs_image->backscroll_rows = abs( beg_row) ;
	}

	for( counter = 0 ; counter < ml_image_get_rows( bs_image->image) ; counter ++)
	{
		if( ( line = ml_bs_get_image_line_in_screen( bs_image , counter)) == NULL)
		{
			return  0 ;
		}

		ml_imgline_set_modified_all( line) ;
	}
	
	return  1 ;
}

int
ml_bs_scroll_upward(
	ml_bs_image_t *  bs_image ,
	u_int  size
	)
{
	int  counter ;

	if( ! bs_image->is_backscroll_mode)
	{
		return  0 ;
	}

	if( bs_image->backscroll_rows < size)
	{
		size = bs_image->backscroll_rows ;
	}
	
	bs_image->backscroll_rows -= size ;
	
	if( ! bs_image->bs_listener->window_scroll_upward( bs_image->bs_listener->self , size))
	{
		for( counter = 0 ; counter < ml_image_get_rows( bs_image->image) - size ; counter++)
		{
			ml_imgline_set_modified_all( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
		}
	}

	for( counter = ml_image_get_rows( bs_image->image) - size ;
		counter < ml_image_get_rows( bs_image->image) ; counter++)
	{
		ml_imgline_set_modified_all( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
	}
	
	return  1 ;
}

int
ml_bs_scroll_downward(
	ml_bs_image_t *  bs_image ,
	u_int  size
	)
{
	int  counter ;
	
	if( ! bs_image->is_backscroll_mode)
	{
		return  0 ;
	}

	if( ml_get_num_of_logged_lines( bs_image->logs) < bs_image->backscroll_rows + size)
	{
		size = ml_get_num_of_logged_lines( bs_image->logs) - bs_image->backscroll_rows ;
	}
	
	bs_image->backscroll_rows += size ;

	if( ! bs_image->bs_listener->window_scroll_downward( bs_image->bs_listener->self , size))
	{
		for( counter = size ; counter < ml_image_get_rows( bs_image->image) ; counter++)
		{
			ml_imgline_set_modified_all( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
		}
	}
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		ml_imgline_set_modified_all( ml_bs_get_image_line_in_screen( bs_image , counter)) ;
	}
	
	return  1 ;
}

u_int
ml_bs_copy_region(
	ml_bs_image_t *  bs_image ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
        int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_image_line_t *  line ;
	int  counter ;
	u_int  size_except_end_space ;
	int  beg_row_in_all ;
	int  end_row_in_all ;

	/* u_int => int */
	beg_row_in_all = -(ml_get_num_of_logged_lines((bs_image)->logs)) ;
	end_row_in_all = bs_image->image->num_of_filled_rows - 1 ;

	if( beg_row < beg_row_in_all)
	{
		return  0 ;
	}
	
	while( 1)
	{
		line = ml_bs_get_image_line_in_all( bs_image , beg_row) ;

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

		if( beg_char_index < size_except_end_space)
		{
			break ;
		}
		
		beg_row ++ ;
		beg_char_index = 0 ;
		
		if( beg_row > end_row_in_all)
		{
			return  0 ;
		}
	}
	
	if( end_row > end_row_in_all)
	{
		end_row = end_row_in_all ;
	}
	
	counter = 0 ;
	
	if( beg_row == end_row)
	{
		size_except_end_space = K_MIN(end_char_index + 1,size_except_end_space) ;

		ml_imgline_copy_str( line , &chars[counter] , beg_char_index ,
			size_except_end_space - beg_char_index) ;
		counter += (size_except_end_space - beg_char_index) ;
	}
	else if( beg_row < end_row)
	{
		int  row ;

		ml_imgline_copy_str( line , &chars[counter] , beg_char_index ,
				size_except_end_space - beg_char_index) ;
		counter += (size_except_end_space - beg_char_index) ;

		if( ! line->is_continued_to_next)
		{
			ml_char_copy( &chars[counter++] , &bs_image->nl_ch) ;
		}
		
		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			line = ml_bs_get_image_line_in_all( bs_image , row) ;

			size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

			ml_imgline_copy_str( line , &chars[counter] , 0 , size_except_end_space) ;
			counter += size_except_end_space ;

			if( ! line->is_continued_to_next)
			{
				ml_char_copy( &chars[counter++] , &bs_image->nl_ch) ;
			}
		}

		line = ml_bs_get_image_line_in_all( bs_image , row) ;

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

		ml_imgline_copy_str( line , &chars[counter] , 0 ,
			K_MIN(end_char_index + 1,size_except_end_space)) ;
		counter += K_MIN(end_char_index + 1,size_except_end_space) ;
	}
	#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " copy region is illegal. nothing is copyed.\n") ;
	}
	#endif

	return  counter ;
}

u_int
ml_bs_get_region_size(
	ml_bs_image_t *  bs_image ,
        int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_image_line_t *  line ;
	u_int  region_size ;
	u_int  size_except_end_space ;
	int  beg_row_in_all ;
	int  end_row_in_all ;

	/* u_int => int */
	beg_row_in_all = -(ml_get_num_of_logged_lines((bs_image)->logs)) ;
	end_row_in_all = bs_image->image->num_of_filled_rows - 1 ;

	if( beg_row < beg_row_in_all)
	{
		return  0 ;
	}
	
	while( 1)
	{
		line = ml_bs_get_image_line_in_all( bs_image , beg_row) ;

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

		if( beg_char_index < size_except_end_space)
		{
			break ;
		}
		
		beg_row ++ ;
		beg_char_index = 0 ;
		
		if( beg_row > end_row_in_all)
		{
			return  0 ;
		}
	}
	
	if( end_row > end_row_in_all)
	{
		end_row = end_row_in_all ;
	}
	
	if( beg_row == end_row)
	{
		region_size = K_MIN(end_char_index + 1,size_except_end_space) - beg_char_index ;
	}
	else if( beg_row < end_row)
	{
		int  row ;

		region_size = size_except_end_space - beg_char_index ;

		if( ! line->is_continued_to_next)
		{
			/* for NL */
			region_size ++ ;
		}

		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			line = ml_bs_get_image_line_in_all( bs_image , row) ;

			region_size += ml_get_num_of_filled_chars_except_end_space( line) ;

			if( ! line->is_continued_to_next)
			{
				/* for NL */
				region_size ++ ;
			}
		}

		line = ml_bs_get_image_line_in_all( bs_image , row) ;

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
		region_size += (K_MIN(end_char_index + 1,size_except_end_space)) ;
	}
	else
	{
		region_size = 0 ;
	}

	return  region_size ;
}

int
ml_bs_get_line_region(
	ml_bs_image_t *  bs_image ,
	int *  beg_row ,
	int *  end_char_index ,
	int *  end_row ,
	int  base_row
	)
{
	int  row ;
	ml_image_line_t *  line ;
	ml_image_line_t *  next_line ;

	/*
	 * finding the end of line.
	 */
	row = base_row ;

	if( ( line = ml_bs_get_image_line_in_all( bs_image , row)) == NULL || ml_imgline_is_empty( line))
	{
		return  0 ;
	}

	while( 1)
	{
		if( ! line->is_continued_to_next)
		{
			break ;
		}

		if( ( next_line = ml_bs_get_image_line_in_all( bs_image , row + 1)) == NULL ||
			ml_imgline_is_empty( next_line))
		{
			break ;
		}

		line = next_line ;
		row ++ ;
	}

	*end_char_index = line->num_of_filled_chars - 1 ;
	*end_row = row ;

	/*
	 * finding the beginning of line.
	 */
	row = base_row ;
	
	while( 1)
	{
		if( ( line = ml_bs_get_image_line_in_all( bs_image , row - 1)) == NULL ||
			ml_imgline_is_empty( line) ||
			! line->is_continued_to_next)
		{
			break ;
		}

		row -- ;
	}

	*beg_row = row ;

	return  1 ;
}

int
ml_bs_get_word_region(
	ml_bs_image_t *  bs_image ,
	int *  beg_char_index ,
	int *  beg_row ,
	int *  end_char_index ,
	int *  end_row ,
	int  base_char_index ,
	int  base_row
	)
{
	int  row ;
	int  char_index ;
	ml_image_line_t *  line ;
	ml_image_line_t *  base_line ;
	ml_char_t *  ch ;

	if( ( base_line = ml_bs_get_image_line_in_all( bs_image , base_row)) == NULL ||
		ml_imgline_is_empty( base_line))
	{
		return  0 ;
	}

	if( is_word_separator(&base_line->chars[base_char_index]))
	{
		*beg_char_index = base_char_index ;
		*end_char_index = base_char_index ;
		*beg_row = base_row ;
		*end_row = base_row ;

		return  1 ;
	}
	
	/*
	 * search the beg of word
	 */
	row = base_row ;
	char_index = base_char_index ;
	line = base_line ;
	
	while( 1)
	{
		if( char_index == 0)
		{
			if( ( line = ml_bs_get_image_line_in_all( bs_image , row - 1)) == NULL ||
				ml_imgline_is_empty( line) || ! line->is_continued_to_next)
			{
				*beg_char_index = char_index ;
				
				break ;
			}

			row -- ;
			char_index = line->num_of_filled_chars - 1 ;
		}
		else
		{
			char_index -- ;
		}
		
		ch = &line->chars[char_index] ;

		if( is_word_separator(ch))
		{
			*beg_char_index = char_index + 1 ;
			
			break ;
		}
	}

	*beg_row = row ;

	/*
	 * search the end of word.
	 */
	row = base_row ;
	char_index = base_char_index ;
	line = base_line ;
	
	while( 1)
	{
		if( char_index == line->num_of_filled_chars - 1)
		{
			if( ! line->is_continued_to_next ||
				( line = ml_bs_get_image_line_in_all( bs_image , row + 1)) == NULL ||
				ml_imgline_is_empty( line))
			{
				*end_char_index = char_index ;
				
				break ;
			}
			
			row ++ ;
			char_index = 0 ;
		}
		else
		{
			char_index ++ ;
		}
		
		ch = &line->chars[char_index] ;

		if( is_word_separator(ch))
		{
			*end_char_index = char_index - 1 ;

			break ;
		}
	}

	*end_row = row ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selected word region: %d %d => %d %d %d %d\n" ,
		base_char_index , base_row , *beg_char_index , *beg_row , *end_char_index , *end_row) ;
#endif

	return  1 ;
}
