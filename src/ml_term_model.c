/*
 *	$Id$
 */

#include  "ml_term_model.h"

#include  <stdlib.h>		/* abs */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* K_MIN */


#define  ROW_IN_LOGS( model , row) \
	( ml_get_num_of_logged_lines( &(model)->logs) + row)


/* --- static variables --- */

static char *  word_separators = " ,.:;/@" ;
static int  separators_are_allocated = 0 ;


/* --- static functions --- */

#if  0

static int
get_n_prev_char_pos(
	ml_term_screen_t *  termmdl ,
	int *  char_index ,
	int *  row ,
	int  n
	)
{
	int  counter ;
	ml_cursor_t  cursor ;
	int  result ;

	cursor = termmdl->image->cursor ;

	for( counter = 0 ; counter < n ; counter ++)
	{
		if( ! ml_cursor_go_back( termmdl->image , WRAPAROUND))
		{
			result = 0 ;

			goto  end ;
		}
	}

	*char_index = termmdl->image->cursor.char_index ;
	*row = termmdl->image->cursor.row ;

	result = 1 ;

end:
	termmdl->image->cursor = cursor ;

	return  result ;
}

#else

static int
get_n_prev_char_pos(
	ml_term_model_t *  termmdl ,
	int *  char_index ,
	int *  row ,
	int  n
	)
{
	int  counter ;

	*char_index = termmdl->image->cursor.char_index ;
	*row = termmdl->image->cursor.row ;

	for( counter = 0 ; counter < n ; counter ++)
	{
		if( *char_index == 0)
		{
			return  0 ;
		}

		(*char_index) -- ;
	}

	return  1 ;
}

#endif	

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

/*
 * ml_sb_term_screen will override this function.
 */
static void
receive_scrolled_out_line(
	void *  p ,
	ml_image_line_t *  line
	)
{
	ml_term_model_t *  termmdl ;

	termmdl = p ;

	if( termmdl->logvis)
	{
		(*termmdl->logvis->visual_line)( termmdl->logvis , line) ;
	}
	
	ml_log_add( &termmdl->logs , line) ;

	if( termmdl->termmdl_listener->scrolled_out_line_received)
	{
		(*termmdl->termmdl_listener->scrolled_out_line_received)(
			termmdl->termmdl_listener->self) ;
	}
}

static int
window_scroll_upward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_term_model_t *  termmdl ;

	termmdl = p ;

	return  (*termmdl->termmdl_listener->window_scroll_upward_region)(
			termmdl->termmdl_listener->self , beg_row , end_row , size) ;
}

static int
window_scroll_downward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_term_model_t *  termmdl ;

	termmdl = p ;

	return  (*termmdl->termmdl_listener->window_scroll_downward_region)(
			termmdl->termmdl_listener->self , beg_row , end_row , size) ;
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


ml_term_model_t *
ml_term_model_new(
	ml_term_model_event_listener_t *  termmdl_listener ,
	u_int  cols ,
	u_int  rows ,
	ml_char_t *  sp_ch ,
	ml_char_t *  nl_ch ,
	u_int  tab_size ,
	u_int  num_of_log_lines ,
	int  use_bce
	)
{
	ml_term_model_t *  termmdl ;
	
	if( ( termmdl = malloc( sizeof( ml_term_model_t))) == NULL)
	{
		return  NULL ;
	}
	
	termmdl->termmdl_listener = termmdl_listener ;
	
	termmdl->logvis = termmdl->container_logvis = NULL ;

	termmdl->image_scroll_listener.self = termmdl ;
	termmdl->image_scroll_listener.receive_upward_scrolled_out_line = receive_scrolled_out_line ;
	termmdl->image_scroll_listener.window_scroll_upward_region = window_scroll_upward_region ;
	termmdl->image_scroll_listener.window_scroll_downward_region = window_scroll_downward_region ;

	if( ! ml_image_init( &termmdl->normal_image , &termmdl->image_scroll_listener ,
		cols , rows , sp_ch , tab_size , 1))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_image_init(normal_image) failed.\n") ;
	#endif
	
		goto  error1 ;
	}

	if( ! ml_image_init( &termmdl->alt_image , &termmdl->image_scroll_listener ,
		cols , rows , sp_ch , tab_size , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_image_init(alt_image) failed.\n") ;
	#endif
	
		goto  error2 ;
	}

	termmdl->image = &termmdl->normal_image ;

	if( ! ml_log_init( &termmdl->logs , num_of_log_lines))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_log_init failed.\n") ;
	#endif
	
		goto  error3 ;
	}

	ml_char_init( &termmdl->nl_ch) ;
	ml_char_copy( &termmdl->nl_ch , nl_ch) ;

	termmdl->backscroll_rows = 0 ;
	termmdl->is_backscroll_mode = 0 ;

	termmdl->use_bce = use_bce ;

	return  termmdl ;

error3:
	ml_image_final( &termmdl->normal_image) ;
error2:
	ml_image_final( &termmdl->alt_image) ;
error1:
	free( termmdl) ;

	return  NULL ;
}

int
ml_term_model_delete(
	ml_term_model_t *  termmdl
	)
{
	/*
	 * this should be done before ml_image_final() since termscr->logvis refers
	 * to ml_image_t and may have some data structure for it.
	 */
	if( termmdl->logvis)
	{
		(*termmdl->logvis->logical)( termmdl->logvis) ;
		(*termmdl->logvis->delete)( termmdl->logvis) ;
	}

	ml_image_final( &termmdl->normal_image) ;
	ml_image_final( &termmdl->alt_image) ;

	ml_log_final( &termmdl->logs) ;

	ml_char_final( &termmdl->nl_ch) ;

	free( termmdl) ;

	return  1 ;
}

int
ml_term_model_resize(
	ml_term_model_t *  termmdl ,
	u_int  cols ,
	u_int  rows
	)
{
	ml_image_resize( &termmdl->normal_image , cols , rows) ;
	ml_image_resize( &termmdl->alt_image , cols , rows) ;

	return  1 ;
}

int
ml_term_model_use_bce(
	ml_term_model_t *  termmdl
	)
{
	termmdl->use_bce = 1 ;

	return  1 ;
}

int
ml_term_model_unuse_bce(
	ml_term_model_t *  termmdl
	)
{
	termmdl->use_bce = 0 ;

	return  1 ;
}

int
ml_term_model_set_bce_fg_color(
	ml_term_model_t *  termmdl ,
	ml_color_t  fg_color
	)
{
	return  ml_char_set_fg_color( &termmdl->image->bce_ch , fg_color) ;
}

int
ml_term_model_set_bce_bg_color(
	ml_term_model_t *  termmdl ,
	ml_color_t  bg_color
	)
{
	return  ml_char_set_bg_color( &termmdl->image->bce_ch , bg_color) ;
}

u_int
ml_term_model_get_cols(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_get_cols( termmdl->image) ;
}

u_int
ml_term_model_get_rows(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_get_rows( termmdl->image) ;
}

u_int
ml_term_model_get_logical_cols(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->logvis)
	{
		return  (*termmdl->logvis->logical_cols)( termmdl->logvis) ;
	}
	else
	{
		return  ml_image_get_cols( termmdl->image) ;
	}
}

u_int
ml_term_model_get_logical_rows(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->logvis)
	{
		return  (*termmdl->logvis->logical_rows)( termmdl->logvis) ;
	}
	else
	{
		return  ml_image_get_rows( termmdl->image) ;
	}
}

u_int
ml_term_model_get_log_size(
	ml_term_model_t *  termmdl
	)
{
	return  ml_get_log_size( &termmdl->logs) ;
}

u_int
ml_term_model_change_log_size(
	ml_term_model_t *  termmdl ,
	u_int  log_size
	)
{
	return  ml_change_log_size( &termmdl->logs , log_size) ;
}

u_int
ml_term_model_get_num_of_logged_lines(
	ml_term_model_t *  termmdl
	)
{
	return  ml_get_num_of_logged_lines( &termmdl->logs) ;
}

int
ml_convert_row_to_scr_row(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	if( termmdl->backscroll_rows > 0)
	{
		if( ( row += termmdl->backscroll_rows) >= ml_image_get_rows( termmdl->image))
		{
			return  -1 ;
		}
	}

	return  row ;
}

int
ml_convert_row_to_abs_row(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	return  row - termmdl->backscroll_rows ;
}

ml_image_line_t *
ml_term_model_get_line(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	return  ml_image_get_line( termmdl->image , row) ;
}

ml_image_line_t *
ml_term_model_get_line_in_screen(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	if( termmdl->is_backscroll_mode && termmdl->backscroll_rows > 0)
	{
		int  abs_row ;

		if( row < 0 - (int)ml_get_num_of_logged_lines( &termmdl->logs))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " row %d is over the beg of screen.\n" , row) ;
		#endif
			
			return  NULL ;
		}
		else if( row >= (int)ml_image_get_rows( termmdl->image))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " row %d is over the end of screen.\n" , row) ;
		#endif
		
			return  NULL ;
		}

		abs_row = row - termmdl->backscroll_rows ;

		if( abs_row < 0)
		{
			return  ml_log_get( &termmdl->logs , ROW_IN_LOGS( termmdl , abs_row)) ;
		}
		else
		{
			return  ml_image_get_line( termmdl->image , abs_row) ;
		}
	}
	else
	{
		return  ml_image_get_line( termmdl->image , row) ;
	}
}

ml_image_line_t *
ml_term_model_get_line_in_all(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	if( row < 0 - (int)ml_get_num_of_logged_lines( &termmdl->logs))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " row %d is over the beg of screen.\n" , row) ;
	#endif
	
		return  NULL ;
	}
	else if( row >= (int)ml_image_get_rows( termmdl->image))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " row %d is over the end of screen.\n" , row) ;
	#endif
	
		return  NULL ;
	}

	if( row < 0)
	{
		return  ml_log_get( &termmdl->logs , ROW_IN_LOGS( termmdl , row)) ;
	}
	else
	{
		return  ml_image_get_line( termmdl->image , row) ;
	}
}

ml_image_line_t *
ml_term_model_cursor_line(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_get_line( termmdl->image , ml_cursor_row( termmdl->image)) ;
}

void
ml_term_model_set_modified_all(
	ml_term_model_t *  termmdl
	)
{
	ml_image_set_modified_all( termmdl->image) ;
}

int
ml_term_model_delete_logical_visual(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->logvis)
	{
		(*termmdl->logvis->logical)( termmdl->logvis) ;
		(*termmdl->logvis->delete)( termmdl->logvis) ;
		termmdl->logvis = NULL ;
	}

	return  1 ;
}

int
ml_term_model_add_logical_visual(
	ml_term_model_t *  termmdl ,
	ml_logical_visual_t *  logvis
	)
{
	if( termmdl->logvis)
	{
		if( termmdl->container_logvis == NULL &&
			( termmdl->container_logvis = ml_logvis_container_new( termmdl->image)) == NULL)
		{
			return  0 ;
		}

		ml_logvis_container_add( termmdl->container_logvis , termmdl->logvis) ;
		ml_logvis_container_add( termmdl->container_logvis , logvis) ;

		termmdl->logvis = termmdl->container_logvis ;
	}
	else
	{
		termmdl->logvis = logvis ;
	}

	return  1 ;
}

int
ml_term_model_render(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->logvis)
	{
		return  (*termmdl->logvis->render)( termmdl->logvis) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_term_model_visual(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->logvis)
	{
		return  (*termmdl->logvis->visual)( termmdl->logvis) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_term_model_logical(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->logvis)
	{
		return  (*termmdl->logvis->logical)( termmdl->logvis) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_is_backscroll_mode(
	ml_term_model_t *  termmdl
	)
{
	return  termmdl->is_backscroll_mode ;
}

int
ml_set_backscroll_mode(
	ml_term_model_t *  termmdl
	)
{
	termmdl->is_backscroll_mode = 1 ;
	termmdl->backscroll_rows = 0 ;

	return  1 ;
}

int
ml_unset_backscroll_mode(
	ml_term_model_t *  termmdl
	)
{
	termmdl->is_backscroll_mode = 0 ;
	termmdl->backscroll_rows = 0 ;

	return  1 ;
}

int
ml_term_model_backscroll_to(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	ml_image_line_t *  line ;
	int  counter ;
	
	if( ! termmdl->is_backscroll_mode)
	{
		return  0 ;
	}

	if( row > 0)
	{
		termmdl->backscroll_rows = 0 ;
	}
	else
	{
		termmdl->backscroll_rows = abs( row) ;
	}

	for( counter = 0 ; counter < ml_image_get_rows( termmdl->image) ; counter ++)
	{
		if( ( line = ml_term_model_get_line_in_screen( termmdl , counter)) == NULL)
		{
			return  0 ;
		}

		ml_imgline_set_modified_all( line) ;
	}

	return  1 ;
}

int
ml_term_model_backscroll_upward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	ml_image_line_t *  line ;
	int  counter ;

	if( ! termmdl->is_backscroll_mode)
	{
		return  0 ;
	}

	if( termmdl->backscroll_rows < size)
	{
		size = termmdl->backscroll_rows ;
	}

	termmdl->backscroll_rows -= size ;

	if( ! (*termmdl->termmdl_listener->window_scroll_upward_region)(
			termmdl->termmdl_listener->self ,
			0 , ml_image_get_rows( termmdl->image) - 1 , size))
	{
		for( counter = 0 ; counter < ml_image_get_rows( termmdl->image) - size ; counter++)
		{
			if( ( line = ml_term_model_get_line_in_screen( termmdl , counter)) == NULL)
			{
				break ;
			}
			
			ml_imgline_set_modified_all( line) ;
		}
	}

	for( counter = ml_image_get_rows( termmdl->image) - size ;
		counter < ml_image_get_rows( termmdl->image) ; counter++)
	{
		if( ( line = ml_term_model_get_line_in_screen( termmdl , counter)) == NULL)
		{
			break ;
		}
		
		ml_imgline_set_modified_all( line) ;
	}
	
	return  1 ;
}

int
ml_term_model_backscroll_downward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	ml_image_line_t *  line ;
	int  counter ;
	
	if( ! termmdl->is_backscroll_mode)
	{
		return  0 ;
	}

	if( ml_get_num_of_logged_lines( &termmdl->logs) < termmdl->backscroll_rows + size)
	{
		size = ml_get_num_of_logged_lines( &termmdl->logs) - termmdl->backscroll_rows ;
	}

	termmdl->backscroll_rows += size ;

	if( ! (*termmdl->termmdl_listener->window_scroll_downward_region)(
		termmdl->termmdl_listener->self ,
		0 , ml_image_get_rows( termmdl->image) - 1 , size))
	{
		for( counter = size ; counter < ml_image_get_rows( termmdl->image) ; counter++)
		{
			if( ( line = ml_term_model_get_line_in_screen( termmdl , counter)) == NULL)
			{
				break ;
			}
			
			ml_imgline_set_modified_all( line) ;
		}
	}
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ( line = ml_term_model_get_line_in_screen( termmdl , counter)) == NULL)
		{
			break ;
		}
		
		ml_imgline_set_modified_all( line) ;
	}

	return  1 ;
}

u_int
ml_term_model_copy_region(
	ml_term_model_t *  termmdl ,
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
	beg_row_in_all = - ( ml_get_num_of_logged_lines( &termmdl->logs)) ;
	end_row_in_all = ml_imgmdl_end_row( &termmdl->image->model) ;

	if( beg_row < beg_row_in_all)
	{
		return  0 ;
	}
	
	while( 1)
	{
		line = ml_term_model_get_line_in_all( termmdl , beg_row) ;

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
		size_except_end_space = K_MIN( end_char_index + 1 , size_except_end_space) ;

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

		if( ! ml_imgline_is_continued_to_next( line))
		{
			ml_char_copy( &chars[counter++] , &termmdl->nl_ch) ;
		}
		
		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			line = ml_term_model_get_line_in_all( termmdl , row) ;

			size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;

			ml_imgline_copy_str( line , &chars[counter] , 0 , size_except_end_space) ;
			counter += size_except_end_space ;

			if( ! ml_imgline_is_continued_to_next( line))
			{
				ml_char_copy( &chars[counter++] , &termmdl->nl_ch) ;
			}
		}

		line = ml_term_model_get_line_in_all( termmdl , row) ;

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
ml_term_model_get_region_size(
	ml_term_model_t *  termmdl ,
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
	beg_row_in_all = -(ml_get_num_of_logged_lines( &termmdl->logs)) ;
	end_row_in_all = ml_imgmdl_end_row( &termmdl->image->model) ;

	if( beg_row < beg_row_in_all)
	{
		return  0 ;
	}
	
	while( 1)
	{
		line = ml_term_model_get_line_in_all( termmdl , beg_row) ;

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
		region_size = K_MIN( end_char_index + 1 , size_except_end_space) - beg_char_index ;
	}
	else if( beg_row < end_row)
	{
		int  row ;

		region_size = size_except_end_space - beg_char_index ;

		if( ! ml_imgline_is_continued_to_next( line))
		{
			/* for NL */
			region_size ++ ;
		}

		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			line = ml_term_model_get_line_in_all( termmdl , row) ;

			region_size += ml_get_num_of_filled_chars_except_end_space( line) ;

			if( ! ml_imgline_is_continued_to_next( line))
			{
				/* for NL */
				region_size ++ ;
			}
		}

		line = ml_term_model_get_line_in_all( termmdl , row) ;

		size_except_end_space = ml_get_num_of_filled_chars_except_end_space( line) ;
		region_size += (K_MIN( end_char_index + 1 , size_except_end_space)) ;
	}
	else
	{
		region_size = 0 ;
	}

	return  region_size ;
}

int
ml_term_model_get_line_region(
	ml_term_model_t *  termmdl ,
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

	if( ( line = ml_term_model_get_line_in_all( termmdl , row)) == NULL || ml_imgline_is_empty( line))
	{
		return  0 ;
	}

	while( 1)
	{
		if( ! ml_imgline_is_continued_to_next( line))
		{
			break ;
		}

		if( ( next_line = ml_term_model_get_line_in_all( termmdl , row + 1)) == NULL ||
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
		if( ( line = ml_term_model_get_line_in_all( termmdl , row - 1)) == NULL ||
			ml_imgline_is_empty( line) ||
			! ml_imgline_is_continued_to_next( line))
		{
			break ;
		}

		row -- ;
	}

	*beg_row = row ;

	return  1 ;
}

int
ml_term_model_get_word_region(
	ml_term_model_t *  termmdl ,
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

	if( ( base_line = ml_term_model_get_line_in_all( termmdl , base_row)) == NULL ||
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
			if( ( line = ml_term_model_get_line_in_all( termmdl , row - 1)) == NULL ||
				ml_imgline_is_empty( line) || ! ml_imgline_is_continued_to_next( line))
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
			if( ! ml_imgline_is_continued_to_next( line) ||
				( line = ml_term_model_get_line_in_all( termmdl , row + 1)) == NULL ||
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


/*
 * VT100 commands
 *
 * !! Notice !!
 * These functions are called under logical order mode.
 */

ml_char_t *
ml_term_model_get_n_prev_char(
	ml_term_model_t *  termmdl ,
	int  n
	)
{
	int  char_index ;
	int  row ;
	ml_char_t *  ch ;
	ml_image_line_t *  line ;

	if( ! get_n_prev_char_pos( termmdl , &char_index , &row , 1))
	{
		return  NULL ;
	}

	if( ( line = ml_image_get_line( termmdl->image , row)) == NULL)
	{
		return  NULL ;
	}

	if( ( ch = ml_imgline_get_char( line , char_index)) == NULL)
	{
		return  NULL ;
	}

	return  ch ;
}

int
ml_term_model_combine_with_prev_char(
	ml_term_model_t *  termmdl ,
	u_char *  bytes ,
	size_t  ch_size ,
	ml_font_t *  font ,
	ml_font_decor_t  font_decor ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_comb
	)
{
	int  char_index ;
	int  row ;
	ml_char_t *  ch ;
	ml_image_line_t *  line ;

	if( ! get_n_prev_char_pos( termmdl , &char_index , &row , 1))
	{
		return  0 ;
	}

	if( ( line = ml_image_get_line( termmdl->image , row)) == NULL)
	{
		return  0 ;
	}

	if( ( ch = ml_imgline_get_char( line , char_index)) == NULL)
	{
		return  0 ;
	}
	
	if( ! ml_char_combine( ch , bytes , ch_size , font , font_decor ,
				fg_color , bg_color , is_comb))
	{
		return  0 ;
	}
	
	ml_imgline_set_modified( line , char_index , char_index , 0) ;
	
	return  1 ;
}

int
ml_term_model_insert_chars(
	ml_term_model_t *  termmdl ,
	ml_char_t *  chars ,
	u_int  len
	)
{
	return  ml_image_insert_chars( termmdl->image , chars , len) ;
}

int
ml_term_model_insert_blank_chars(
	ml_term_model_t *  termmdl ,
	u_int  len
	)
{
	return  ml_image_insert_blank_chars( termmdl->image , len) ;
}

int
ml_term_model_vertical_tab(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_vertical_tab( termmdl->image) ;
}

int
ml_term_model_set_tab_stop(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_set_tab_stop( termmdl->image) ;
}

int
ml_term_model_clear_tab_stop(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_clear_tab_stop( termmdl->image) ;
}

int
ml_term_model_clear_all_tab_stops(
	ml_term_model_t *  termmdl
	)
{
	return  ml_image_clear_all_tab_stops( termmdl->image) ;
}

int
ml_term_model_insert_new_lines(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	int  counter ;
		
	for( counter = 0 ; counter < size ; counter ++)
	{
		ml_image_insert_new_line( termmdl->image) ;
	}

	return  1 ;
}

int
ml_term_model_line_feed(
	ml_term_model_t *  termmdl
	)
{	
	return  ml_cursor_go_downward( termmdl->image , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_term_model_overwrite_chars(
	ml_term_model_t *  termmdl ,
	ml_char_t *  chars ,
	u_int  len
	)
{
	return  ml_image_overwrite_chars( termmdl->image , chars , len) ;
}

int
ml_term_model_delete_cols(
	ml_term_model_t *  termmdl ,
	u_int  len
	)
{	
	return  ml_image_delete_cols( termmdl->image , len) ;
}

int
ml_term_model_delete_lines(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_image_delete_line( termmdl->image))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " deleting nonexisting lines.\n") ;
		#endif
			
			return  0 ;
		}
	}
	
	return  1 ;
}

int
ml_term_model_clear_line_to_right(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->use_bce)
	{
		return  ml_image_clear_line_to_right_bce( termmdl->image) ;
	}
	else
	{
		return  ml_image_clear_line_to_right( termmdl->image) ;
	}
}

int
ml_term_model_clear_line_to_left(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->use_bce)
	{
		return  ml_image_clear_line_to_left_bce( termmdl->image) ;
	}
	else
	{
		return  ml_image_clear_line_to_left( termmdl->image) ;
	}
}

int
ml_term_model_clear_below(
	ml_term_model_t *  termmdl
	)
{	
	if( termmdl->use_bce)
	{
		return  ml_image_clear_below_bce( termmdl->image) ;
	}
	else
	{
		return  ml_image_clear_below( termmdl->image) ;
	}
}

int
ml_term_model_clear_above(
	ml_term_model_t *  termmdl
	)
{
	if( termmdl->use_bce)
	{
		return  ml_image_clear_above_bce( termmdl->image) ;
	}
	else
	{
		return  ml_image_clear_above( termmdl->image) ;
	}
}

int
ml_term_model_set_scroll_region(
	ml_term_model_t *  termmdl ,
	int  beg ,
	int  end
	)
{
	return  ml_image_set_scroll_region( termmdl->image , beg , end) ;
}

int
ml_term_model_scroll_upward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	return  ml_cursor_go_downward( termmdl->image , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_term_model_scroll_downward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	return  ml_cursor_go_upward( termmdl->image , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_term_model_go_forward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_forward( termmdl->image , BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go forward any more.\n") ;
		#endif
		
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_term_model_go_back(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	int  counter ;

	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_back( termmdl->image , 0))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go back any more.\n") ;
		#endif
					
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_term_model_go_upward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_upward( termmdl->image , BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go upward any more.\n") ;
		#endif
				
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_term_model_go_downward(
	ml_term_model_t *  termmdl ,
	u_int  size
	)
{
	int  counter ;
	
	for( counter = 0 ; counter < size ; counter ++)
	{
		if( ! ml_cursor_go_downward( termmdl->image , BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot go downward any more.\n") ;
		#endif
		
			return  0 ;
		}
	}

	return  1 ;
}

int
ml_term_model_goto_beg_of_line(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_goto_beg_of_line( termmdl->image) ;
}

int
ml_term_model_go_horizontally(
	ml_term_model_t *  termmdl ,
	int  col
	)
{
	return  ml_term_model_goto( termmdl , col , ml_cursor_row( termmdl->image)) ;
}

int
ml_term_model_go_vertically(
	ml_term_model_t *  termmdl ,
	int  row
	)
{
	return  ml_term_model_goto( termmdl , ml_cursor_col( termmdl->image) , row) ;
}

int
ml_term_model_goto_home(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_goto_home( termmdl->image) ;
}

int
ml_term_model_goto(
	ml_term_model_t *  termmdl ,
	int  col ,
	int  row
	)
{
	return  ml_cursor_goto( termmdl->image , col , row , BREAK_BOUNDARY) ;
}

int
ml_term_model_cursor_col(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_col( termmdl->image) ;
}

int
ml_term_model_cursor_char_index(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_char_index( termmdl->image) ;
}

int
ml_term_model_cursor_row(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_row( termmdl->image) ;
}

int
ml_term_model_save_cursor(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_save( termmdl->image) ;
}

int
ml_term_model_restore_cursor(
	ml_term_model_t *  termmdl
	)
{
	return  ml_cursor_restore( termmdl->image) ;
}

int
ml_term_model_highlight_cursor(
	ml_term_model_t *  termmdl
	)
{
	return  ml_highlight_cursor( termmdl->image) ;
}

int
ml_term_model_unhighlight_cursor(
	ml_term_model_t *  termmdl
	)
{
	return  ml_unhighlight_cursor( termmdl->image) ;
}

u_int
ml_term_model_get_tab_size(
	ml_term_model_t *  termmdl
	)
{
	return  termmdl->image->tab_size ;
}

u_int
ml_term_model_set_tab_size(
	ml_term_model_t *  termmdl ,
	u_int  tab_size
	)
{
	return  ml_image_set_tab_size( termmdl->image , tab_size) ;
}

int
ml_term_model_use_normal_image(
	ml_term_model_t *  termmdl
	)
{
	termmdl->image = &termmdl->normal_image ;

	if( termmdl->logvis)
	{
		(*termmdl->logvis->change_image)( termmdl->logvis , termmdl->image) ;
	}

	ml_image_set_modified_all( termmdl->image) ;
	
	return  1 ;
}

int
ml_term_model_use_alternative_image(
	ml_term_model_t *  termmdl
	)
{
	termmdl->image = &termmdl->alt_image ;

	ml_term_model_goto_home( termmdl) ;
	ml_term_model_clear_below( termmdl) ;

	if( termmdl->logvis)
	{
		(*termmdl->logvis->change_image)( termmdl->logvis , termmdl->image) ;
	}

	ml_image_set_modified_all( termmdl->image) ;
	
	return  1 ;
}

int
ml_term_model_fill_all(
	ml_term_model_t *  termmdl ,
	ml_char_t *  ch
	)
{
	return  ml_image_fill_all( termmdl->image , ch) ;
}
