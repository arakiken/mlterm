/*
 *	$Id$
 */

#include  "ml_screen.h"

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
	mlx_term_screen_t *  screen ,
	int *  char_index ,
	int *  row ,
	int  n
	)
{
	int  count ;
	ml_cursor_t  cursor ;
	int  result ;

	cursor = screen->edit->cursor ;

	for( count = 0 ; count < n ; count ++)
	{
		if( ! ml_cursor_go_back( screen->edit , WRAPAROUND))
		{
			result = 0 ;

			goto  end ;
		}
	}

	*char_index = screen->edit->cursor.char_index ;
	*row = screen->edit->cursor.row ;

	result = 1 ;

end:
	screen->edit->cursor = cursor ;

	return  result ;
}

#else

static int
get_n_prev_char_pos(
	ml_screen_t *  screen ,
	int *  char_index ,
	int *  row ,
	int  n
	)
{
	int  count ;

	*char_index = screen->edit->cursor.char_index ;
	*row = screen->edit->cursor.row ;

	for( count = 0 ; count < n ; count ++)
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
 * mlx_sb_term_screen will override this function.
 */
static void
receive_scrolled_out_line(
	void *  p ,
	ml_line_t *  line
	)
{
	ml_screen_t *  screen ;

	screen = p ;

	if( screen->logvis)
	{
		(*screen->logvis->visual_line)( screen->logvis , line) ;
	}
	
	ml_log_add( &screen->logs , line) ;

	if( screen->screen_listener && screen->screen_listener->line_scrolled_out)
	{
		(*screen->screen_listener->line_scrolled_out)( screen->screen_listener->self) ;
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
	ml_screen_t *  screen ;

	screen = p ;

	if( ! screen->screen_listener)
	{
		return  0 ;
	}

	return  (*screen->screen_listener->window_scroll_upward_region)(
			screen->screen_listener->self , beg_row , end_row , size) ;
}

static int
window_scroll_downward_region(
	void *  p ,
	int  beg_row ,
	int  end_row ,
	u_int  size
	)
{
	ml_screen_t *  screen ;

	screen = p ;

	if( ! screen->screen_listener)
	{
		return  0 ;
	}

	return  (*screen->screen_listener->window_scroll_downward_region)(
			screen->screen_listener->self , beg_row , end_row , size) ;
}

static int
reverse_or_restore_color(
	ml_screen_t *  screen ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row ,
	int (*func)( ml_line_t * , int)
	)
{
	int  char_index ;
	int  row ;
	ml_line_t *  line ;
	u_int  size_except_spaces ;
	int  beg_except_spaces ;

	if( ( line = ml_screen_get_line( screen , beg_row)) == NULL || ml_line_is_empty( line))
	{
		return  0 ;
	}

	size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line) ;
	beg_except_spaces = ml_line_beg_char_index_except_spaces( line) ;

	row = beg_row ;
	if( beg_row == end_row)
	{
		for( char_index = K_MAX(beg_char_index,beg_except_spaces) ;
			char_index < K_MIN(end_char_index + 1,size_except_spaces) ; char_index ++)
		{
			(*func)( line , char_index) ;
		}
	}
	else if( beg_row < end_row)
	{
		if( ml_line_is_rtl( line))
		{
			for( char_index = beg_except_spaces ; char_index <= beg_char_index ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		else
		{
			for( char_index = K_MAX(beg_char_index,beg_except_spaces) ;
				char_index < size_except_spaces ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}

		for( row ++ ; row < end_row ; row ++)
		{
			if( ( line = ml_screen_get_line( screen , row)) == NULL ||
				ml_line_is_empty( line))
			{
				return  0 ;
			}

			size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line) ;
			beg_except_spaces = ml_line_beg_char_index_except_spaces( line) ;
			
			for( char_index = beg_except_spaces ;
				char_index < size_except_spaces ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		
		if( ( line = ml_screen_get_line( screen , row)) == NULL ||
			ml_line_is_empty( line))
		{
			return  0 ;
		}

		size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line) ;
		beg_except_spaces = ml_line_beg_char_index_except_spaces( line) ;

		if( ml_line_is_rtl( line))
		{
			for( char_index = K_MAX(end_char_index,beg_except_spaces) ;
				char_index < size_except_spaces ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		else
		{
			for( char_index = beg_except_spaces ;
				char_index < K_MIN(end_char_index + 1,size_except_spaces) ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
	}

	return  1 ;
}

static u_int
check_or_copy_region(
	ml_screen_t *  screen ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
        int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	ml_line_t *  line ;
	int  count ;
	u_int  size_except_spaces ;
	int  beg_except_spaces ;
	int  beg_limit ;
	int  end_limit ;

	/* u_int => int */
	beg_limit = - ( ml_get_num_of_logged_lines( &screen->logs)) ;
	end_limit = ml_model_end_row( &screen->edit->model) ;

	if( beg_row < beg_limit)
	{
		beg_row = beg_limit ;
	}
	
	if( end_row > end_limit)
	{
		end_row = end_limit ;
	}
	
	if( ( line = ml_screen_get_line( screen , beg_row)) == NULL)
	{
		return  0 ;
	}

	if( beg_char_index >= (size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line)))
	{
		do
		{
			if( ++ beg_row >= end_row ||
				( line = ml_screen_get_line( screen , beg_row)) == NULL)
			{
				return  0 ;
			}
		}
		while( ( size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line)) == 0) ;

		if( ml_line_is_rtl( line))
		{
			beg_char_index = size_except_spaces - 1 ;
		}
		else
		{
			beg_char_index = 0 ;
		}
	}

	beg_except_spaces = ml_line_beg_char_index_except_spaces( line) ;

	if( beg_row == end_row)
	{
		count = K_MIN(end_char_index + 1,size_except_spaces) -
				K_MAX(beg_char_index,beg_except_spaces) ;
		if( chars)
		{
			ml_line_copy_str( line , chars , beg_char_index , count) ;
		}
	}
	else if( beg_row < end_row)
	{
		int  row ;

		if( ml_line_is_rtl( line))
		{
			count = (K_MAX(beg_char_index,beg_except_spaces) - beg_except_spaces + 1) ;
			if( chars)
			{
				ml_line_copy_str( line , chars , beg_except_spaces , count) ;
			}
		}
		else
		{
			count = (size_except_spaces - K_MAX(beg_char_index,beg_except_spaces)) ;
			if( chars)
			{
				ml_line_copy_str( line , chars , beg_char_index , count) ;
			}
		}

		if( ! ml_line_is_continued_to_next( line))
		{
			if( chars)
			{
				ml_char_copy( &chars[count] , &screen->nl_ch) ;
			}
			count ++ ;
		}

		for( row = beg_row + 1 ; row < end_row ; row ++)
		{
			line = ml_screen_get_line( screen , row) ;

			size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line) ;
			beg_except_spaces = ml_line_beg_char_index_except_spaces( line) ;

			if( chars)
			{
				ml_line_copy_str( line , &chars[count] , beg_except_spaces ,
					size_except_spaces - beg_except_spaces) ;
			}
			count += (size_except_spaces - beg_except_spaces) ;

			if( ! ml_line_is_continued_to_next( line))
			{
				if( chars)
				{
					ml_char_copy( &chars[count] , &screen->nl_ch) ;
				}
				count ++ ;
			}
		}

		line = ml_screen_get_line( screen , row) ;

		size_except_spaces = ml_get_num_of_filled_chars_except_spaces( line) ;
		beg_except_spaces = ml_line_beg_char_index_except_spaces( line) ;

		if( ml_line_is_rtl( line))
		{
			if( chars)
			{
				ml_line_copy_str( line , &chars[count] ,
					K_MAX(end_char_index,beg_except_spaces) ,
					size_except_spaces - K_MAX(end_char_index,beg_except_spaces)) ;
			}
			count += (size_except_spaces - K_MAX(end_char_index,beg_except_spaces)) ;
		}
		else
		{
			if( chars)
			{
				ml_line_copy_str( line , &chars[count] , beg_except_spaces ,
					K_MIN(end_char_index + 1,size_except_spaces)) ;
			}
			count += K_MIN(end_char_index + 1,size_except_spaces) ;
		}
	}
	#ifdef  DEBUG
	else
	{
		kik_warn_printf( KIK_DEBUG_TAG " copy region is illegal. nothing is copyed.\n") ;
	}
	#endif

	return  count ;
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


ml_screen_t *
ml_screen_new(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  num_of_log_lines ,
	int  use_bce
	)
{
	ml_screen_t *  screen ;
	
	if( ( screen = malloc( sizeof( ml_screen_t))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  NULL ;
	}
	
	screen->screen_listener = NULL ;
	
	screen->logvis = screen->container_logvis = NULL ;

	screen->edit_scroll_listener.self = screen ;
	screen->edit_scroll_listener.receive_upward_scrolled_out_line = receive_scrolled_out_line ;
	screen->edit_scroll_listener.window_scroll_upward_region = window_scroll_upward_region ;
	screen->edit_scroll_listener.window_scroll_downward_region = window_scroll_downward_region ;

	if( ! ml_edit_init( &screen->normal_edit , &screen->edit_scroll_listener ,
		cols , rows , tab_size , 1))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_edit_init(normal_edit) failed.\n") ;
	#endif
	
		goto  error1 ;
	}

	if( ! ml_edit_init( &screen->alt_edit , &screen->edit_scroll_listener ,
		cols , rows , tab_size , 0))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_edit_init(alt_edit) failed.\n") ;
	#endif
	
		goto  error2 ;
	}

	screen->edit = &screen->normal_edit ;

	if( ! ml_log_init( &screen->logs , num_of_log_lines))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_log_init failed.\n") ;
	#endif
	
		goto  error3 ;
	}

	ml_char_init( &screen->nl_ch) ;
	ml_char_set( &screen->nl_ch , "\n" , 1 , US_ASCII , 0 , 0 , ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;

	screen->backscroll_rows = 0 ;
	screen->is_backscroll_mode = 0 ;

	screen->use_dynamic_comb = 0 ;
	screen->use_bce = use_bce ;
	screen->is_cursor_visible = 1 ;

	return  screen ;

error3:
	ml_edit_final( &screen->normal_edit) ;
error2:
	ml_edit_final( &screen->alt_edit) ;
error1:
	free( screen) ;

	return  NULL ;
}

int
ml_screen_delete(
	ml_screen_t *  screen
	)
{
	/*
	 * this should be done before ml_edit_final() since termscr->logvis refers
	 * to ml_edit_t and may have some data structure for it.
	 */
	if( screen->logvis)
	{
		(*screen->logvis->logical)( screen->logvis) ;
		(*screen->logvis->delete)( screen->logvis) ;
	}

	ml_edit_final( &screen->normal_edit) ;
	ml_edit_final( &screen->alt_edit) ;

	ml_log_final( &screen->logs) ;

	ml_char_final( &screen->nl_ch) ;

	free( screen) ;

	return  1 ;
}

int
ml_screen_set_listener(
	ml_screen_t *  screen ,
	ml_screen_event_listener_t *  screen_listener
	)
{
	screen->screen_listener = screen_listener ;

	return  1 ;
}

int
ml_screen_resize(
	ml_screen_t *  screen ,
	u_int  cols ,
	u_int  rows
	)
{
	ml_edit_resize( &screen->normal_edit , cols , rows) ;
	ml_edit_resize( &screen->alt_edit , cols , rows) ;

	return  1 ;
}

int
ml_screen_use_bce(
	ml_screen_t *  screen
	)
{
	screen->use_bce = 1 ;

	return  1 ;
}

int
ml_screen_unuse_bce(
	ml_screen_t *  screen
	)
{
	screen->use_bce = 0 ;

	return  1 ;
}

int
ml_screen_set_bce_fg_color(
	ml_screen_t *  screen ,
	ml_color_t  fg_color
	)
{
	return  ml_char_set_fg_color( &screen->edit->bce_ch , fg_color) ;
}

int
ml_screen_set_bce_bg_color(
	ml_screen_t *  screen ,
	ml_color_t  bg_color
	)
{
	return  ml_char_set_bg_color( &screen->edit->bce_ch , bg_color) ;
}

int
ml_screen_cursor_col(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_col( screen->edit) ;
}

int
ml_screen_cursor_char_index(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_char_index( screen->edit) ;
}

int
ml_screen_cursor_row(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_row( screen->edit) ;
}

int
ml_screen_cursor_row_in_screen(
	ml_screen_t *  screen
	)
{
	int  row ;

	row = ml_cursor_row( screen->edit) ;
	
	if( screen->backscroll_rows > 0)
	{
		if( ( row += screen->backscroll_rows) >= ml_edit_get_rows( screen->edit))
		{
			return  -1 ;
		}
	}

	return  row ;
}

int
ml_screen_highlight_cursor(
	ml_screen_t *  screen
	)
{
	if( screen->is_cursor_visible)
	{
		return  ml_highlight_cursor( screen->edit) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_screen_unhighlight_cursor(
	ml_screen_t *  screen
	)
{
	return  ml_unhighlight_cursor( screen->edit) ;
}

u_int
ml_screen_get_cols(
	ml_screen_t *  screen
	)
{
	return  ml_edit_get_cols( screen->edit) ;
}

u_int
ml_screen_get_rows(
	ml_screen_t *  screen
	)
{
	return  ml_edit_get_rows( screen->edit) ;
}

u_int
ml_screen_get_logical_cols(
	ml_screen_t *  screen
	)
{
	if( screen->logvis)
	{
		return  (*screen->logvis->logical_cols)( screen->logvis) ;
	}
	else
	{
		return  ml_edit_get_cols( screen->edit) ;
	}
}

u_int
ml_screen_get_logical_rows(
	ml_screen_t *  screen
	)
{
	if( screen->logvis)
	{
		return  (*screen->logvis->logical_rows)( screen->logvis) ;
	}
	else
	{
		return  ml_edit_get_rows( screen->edit) ;
	}
}

u_int
ml_screen_get_log_size(
	ml_screen_t *  screen
	)
{
	return  ml_get_log_size( &screen->logs) ;
}

u_int
ml_screen_change_log_size(
	ml_screen_t *  screen ,
	u_int  log_size
	)
{
	return  ml_change_log_size( &screen->logs , log_size) ;
}

u_int
ml_screen_get_num_of_logged_lines(
	ml_screen_t *  screen
	)
{
	return  ml_get_num_of_logged_lines( &screen->logs) ;
}

int
ml_screen_convert_scr_row_to_abs(
	ml_screen_t *  screen ,
	int  row
	)
{
	return  row - screen->backscroll_rows ;
}

ml_line_t *
ml_screen_get_line(
	ml_screen_t *  screen ,
	int  row
	)
{
	if( row < 0 - (int)ml_get_num_of_logged_lines( &screen->logs))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " row %d is over the beg of screen.\n" , row) ;
	#endif
	
		return  NULL ;
	}
	else if( row >= (int)ml_edit_get_rows( screen->edit))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " row %d is over the end of screen.\n" , row) ;
	#endif
	
		return  NULL ;
	}

	if( row < 0)
	{
		return  ml_log_get( &screen->logs , ROW_IN_LOGS( screen , row)) ;
	}
	else
	{
		return  ml_edit_get_line( screen->edit , row) ;
	}
}

ml_line_t *
ml_screen_get_line_in_screen(
	ml_screen_t *  screen ,
	int  row
	)
{
	if( screen->is_backscroll_mode && screen->backscroll_rows > 0)
	{
		int  abs_row ;

		if( row < 0 - (int)ml_get_num_of_logged_lines( &screen->logs))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " row %d is over the beg of screen.\n" , row) ;
		#endif
			
			return  NULL ;
		}
		else if( row >= (int)ml_edit_get_rows( screen->edit))
		{
		#ifdef  __DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " row %d is over the end of screen.\n" , row) ;
		#endif
		
			return  NULL ;
		}

		abs_row = row - screen->backscroll_rows ;

		if( abs_row < 0)
		{
			return  ml_log_get( &screen->logs , ROW_IN_LOGS( screen , abs_row)) ;
		}
		else
		{
			return  ml_edit_get_line( screen->edit , abs_row) ;
		}
	}
	else
	{
		return  ml_edit_get_line( screen->edit , row) ;
	}
}

ml_line_t *
ml_screen_get_cursor_line(
	ml_screen_t *  screen
	)
{
	return  ml_edit_get_line( screen->edit , ml_cursor_row( screen->edit)) ;
}

int
ml_screen_set_modified_all(
	ml_screen_t *  screen
	)
{
	int  row ;
	ml_line_t *  line ;

	for( row = 0 ; row < ml_edit_get_rows( screen->edit) ; row ++)
	{
		if( ( line = ml_screen_get_line_in_screen( screen , row)) == NULL)
		{
			return  0 ;
		}
		
		ml_line_set_modified_all( line) ;
	}

	return  1 ;
}

int
ml_screen_add_logical_visual(
	ml_screen_t *  screen ,
	ml_logical_visual_t *  logvis
	)
{
	(*logvis->init)( logvis , screen->edit) ;

	if( screen->logvis)
	{
		if( screen->container_logvis == NULL &&
			( screen->container_logvis = ml_logvis_container_new()) == NULL)
		{
			return  0 ;
		}

		ml_logvis_container_add( screen->container_logvis , screen->logvis) ;
		ml_logvis_container_add( screen->container_logvis , logvis) ;

		screen->logvis = screen->container_logvis ;
	}
	else
	{
		screen->logvis = logvis ;
	}

	return  1 ;
}

int
ml_screen_delete_logical_visual(
	ml_screen_t *  screen
	)
{
	if( screen->logvis)
	{
		(*screen->logvis->logical)( screen->logvis) ;
		(*screen->logvis->delete)( screen->logvis) ;
		screen->logvis = NULL ;
		screen->container_logvis = NULL ;
	}

	return  1 ;
}

int
ml_screen_render(
	ml_screen_t *  screen
	)
{
	if( screen->logvis)
	{
		return  (*screen->logvis->render)( screen->logvis) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_screen_visual(
	ml_screen_t *  screen
	)
{
	if( screen->logvis)
	{
		return  (*screen->logvis->visual)( screen->logvis) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_screen_logical(
	ml_screen_t *  screen
	)
{
	if( screen->logvis)
	{
		return  (*screen->logvis->logical)( screen->logvis) ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_is_backscroll_mode(
	ml_screen_t *  screen
	)
{
	return  screen->is_backscroll_mode ;
}

int
ml_enter_backscroll_mode(
	ml_screen_t *  screen
	)
{
	screen->is_backscroll_mode = 1 ;
	screen->backscroll_rows = 0 ;

	return  1 ;
}

int
ml_exit_backscroll_mode(
	ml_screen_t *  screen
	)
{
	screen->is_backscroll_mode = 0 ;
	screen->backscroll_rows = 0 ;

	return  1 ;
}

int
ml_screen_backscroll_to(
	ml_screen_t *  screen ,
	int  row
	)
{
	ml_line_t *  line ;
	int  count ;
	
	if( ! screen->is_backscroll_mode)
	{
		return  0 ;
	}

	if( row > 0)
	{
		screen->backscroll_rows = 0 ;
	}
	else
	{
		screen->backscroll_rows = abs( row) ;
	}

	for( count = 0 ; count < ml_edit_get_rows( screen->edit) ; count ++)
	{
		if( ( line = ml_screen_get_line_in_screen( screen , count)) == NULL)
		{
			return  0 ;
		}

		ml_line_set_modified_all( line) ;
	}

	return  1 ;
}

int
ml_screen_backscroll_upward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	ml_line_t *  line ;
	int  count ;

	if( ! screen->is_backscroll_mode)
	{
		return  0 ;
	}

	if( screen->backscroll_rows < size)
	{
		size = screen->backscroll_rows ;
	}

	screen->backscroll_rows -= size ;

	if( ! screen->screen_listener ||
		! (*screen->screen_listener->window_scroll_upward_region)(
			screen->screen_listener->self ,
			0 , ml_edit_get_rows( screen->edit) - 1 , size))
	{
		for( count = 0 ; count < ml_edit_get_rows( screen->edit) - size ; count++)
		{
			if( ( line = ml_screen_get_line_in_screen( screen , count)) == NULL)
			{
				break ;
			}
			
			ml_line_set_modified_all( line) ;
		}
	}

	for( count = ml_edit_get_rows( screen->edit) - size ;
		count < ml_edit_get_rows( screen->edit) ; count++)
	{
		if( ( line = ml_screen_get_line_in_screen( screen , count)) == NULL)
		{
			break ;
		}
		
		ml_line_set_modified_all( line) ;
	}
	
	return  1 ;
}

int
ml_screen_backscroll_downward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	ml_line_t *  line ;
	int  count ;
	
	if( ! screen->is_backscroll_mode)
	{
		return  0 ;
	}

	if( ml_get_num_of_logged_lines( &screen->logs) < screen->backscroll_rows + size)
	{
		size = ml_get_num_of_logged_lines( &screen->logs) - screen->backscroll_rows ;
	}

	screen->backscroll_rows += size ;

	if( !screen->screen_listener ||
		! (*screen->screen_listener->window_scroll_downward_region)(
			screen->screen_listener->self ,
			0 , ml_edit_get_rows( screen->edit) - 1 , size))
	{
		for( count = size ; count < ml_edit_get_rows( screen->edit) ; count++)
		{
			if( ( line = ml_screen_get_line_in_screen( screen , count)) == NULL)
			{
				break ;
			}
			
			ml_line_set_modified_all( line) ;
		}
	}
	
	for( count = 0 ; count < size ; count ++)
	{
		if( ( line = ml_screen_get_line_in_screen( screen , count)) == NULL)
		{
			break ;
		}
		
		ml_line_set_modified_all( line) ;
	}

	return  1 ;
}

u_int
ml_screen_get_tab_size(
	ml_screen_t *  screen
	)
{
	return  screen->edit->tab_size ;
}

int
ml_screen_set_tab_size(
	ml_screen_t *  screen ,
	u_int  tab_size
	)
{
	return  ml_edit_set_tab_size( screen->edit , tab_size) ;
}

int
ml_screen_reverse_color(
	ml_screen_t *  screen ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  reverse_or_restore_color( screen , beg_char_index , beg_row ,
		end_char_index , end_row , ml_line_reverse_color) ;
}

int
ml_screen_restore_color(
	ml_screen_t *  screen ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  reverse_or_restore_color( screen , beg_char_index , beg_row ,
		end_char_index , end_row , ml_line_restore_color) ;
}

u_int
ml_screen_copy_region(
	ml_screen_t *  screen ,
	ml_char_t *  chars ,
	u_int  num_of_chars ,
        int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  check_or_copy_region( screen , chars , num_of_chars , beg_char_index , beg_row ,
			end_char_index , end_row) ;
}

u_int
ml_screen_get_region_size(
	ml_screen_t *  screen ,
	int  beg_char_index ,
	int  beg_row ,
	int  end_char_index ,
	int  end_row
	)
{
	return  check_or_copy_region( screen , NULL , 0 , beg_char_index , beg_row ,
			end_char_index , end_row) ;
}

int
ml_screen_get_line_region(
	ml_screen_t *  screen ,
	int *  beg_row ,
	int *  end_char_index ,
	int *  end_row ,
	int  base_row
	)
{
	int  row ;
	ml_line_t *  line ;
	ml_line_t *  next_line ;

	/*
	 * finding the end of line.
	 */
	row = base_row ;

	if( ( line = ml_screen_get_line( screen , row)) == NULL || ml_line_is_empty( line))
	{
		return  0 ;
	}

	while( 1)
	{
		if( ! ml_line_is_continued_to_next( line))
		{
			break ;
		}

		if( ( next_line = ml_screen_get_line( screen , row + 1)) == NULL ||
			ml_line_is_empty( next_line))
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
		if( ( line = ml_screen_get_line( screen , row - 1)) == NULL ||
			ml_line_is_empty( line) ||
			! ml_line_is_continued_to_next( line))
		{
			break ;
		}

		row -- ;
	}

	*beg_row = row ;

	return  1 ;
}

int
ml_screen_get_word_region(
	ml_screen_t *  screen ,
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
	ml_line_t *  line ;
	ml_line_t *  base_line ;
	ml_char_t *  ch ;

	if( ( base_line = ml_screen_get_line( screen , base_row)) == NULL ||
		ml_line_is_empty( base_line))
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
			if( ( line = ml_screen_get_line( screen , row - 1)) == NULL ||
				ml_line_is_empty( line) || ! ml_line_is_continued_to_next( line))
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
			if( ! ml_line_is_continued_to_next( line) ||
				( line = ml_screen_get_line( screen , row + 1)) == NULL ||
				ml_line_is_empty( line))
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
ml_screen_get_n_prev_char(
	ml_screen_t *  screen ,
	int  n
	)
{
	int  char_index ;
	int  row ;
	ml_char_t *  ch ;
	ml_line_t *  line ;

	if( ! get_n_prev_char_pos( screen , &char_index , &row , 1))
	{
		return  NULL ;
	}

	if( ( line = ml_edit_get_line( screen->edit , row)) == NULL)
	{
		return  NULL ;
	}

	if( ( ch = ml_line_get_char( line , char_index)) == NULL)
	{
		return  NULL ;
	}

	return  ch ;
}

int
ml_screen_combine_with_prev_char(
	ml_screen_t *  screen ,
	u_char *  bytes ,
	size_t  ch_size ,
	mkf_charset_t  cs ,
	int  is_biwidth ,
	int  is_comb ,
	ml_color_t  fg_color ,
	ml_color_t  bg_color ,
	int  is_bold ,
	int  is_underlined
	)
{
	int  char_index ;
	int  row ;
	ml_char_t *  ch ;
	ml_line_t *  line ;

	if( ! get_n_prev_char_pos( screen , &char_index , &row , 1))
	{
		return  0 ;
	}

	if( ( line = ml_edit_get_line( screen->edit , row)) == NULL)
	{
		return  0 ;
	}

	if( ( ch = ml_line_get_char( line , char_index)) == NULL)
	{
		return  0 ;
	}
	
	if( ! ml_char_combine( ch , bytes , ch_size , cs , is_biwidth , is_comb ,
		fg_color , bg_color , is_bold , is_underlined))
	{
		return  0 ;
	}
	
	ml_line_set_modified( line , char_index , char_index , 0) ;
	
	return  1 ;
}

int
ml_screen_insert_chars(
	ml_screen_t *  screen ,
	ml_char_t *  chars ,
	u_int  len
	)
{
	return  ml_edit_insert_chars( screen->edit , chars , len) ;
}

int
ml_screen_insert_blank_chars(
	ml_screen_t *  screen ,
	u_int  len
	)
{
	return  ml_edit_insert_blank_chars( screen->edit , len) ;
}

int
ml_screen_vertical_tab(
	ml_screen_t *  screen
	)
{
	return  ml_edit_vertical_tab( screen->edit) ;
}

int
ml_screen_set_tab_stop(
	ml_screen_t *  screen
	)
{
	return  ml_edit_set_tab_stop( screen->edit) ;
}

int
ml_screen_clear_tab_stop(
	ml_screen_t *  screen
	)
{
	return  ml_edit_clear_tab_stop( screen->edit) ;
}

int
ml_screen_clear_all_tab_stops(
	ml_screen_t *  screen
	)
{
	return  ml_edit_clear_all_tab_stops( screen->edit) ;
}

int
ml_screen_insert_new_lines(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	int  count ;
		
	for( count = 0 ; count < size ; count ++)
	{
		ml_edit_insert_new_line( screen->edit) ;
	}

	return  1 ;
}

int
ml_screen_line_feed(
	ml_screen_t *  screen
	)
{	
	return  ml_cursor_go_downward( screen->edit , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_screen_overwrite_chars(
	ml_screen_t *  screen ,
	ml_char_t *  chars ,
	u_int  len
	)
{
	return  ml_edit_overwrite_chars( screen->edit , chars , len) ;
}

int
ml_screen_delete_cols(
	ml_screen_t *  screen ,
	u_int  len
	)
{
	if( screen->use_bce)
	{
		return  ml_edit_delete_cols_bce( screen->edit , len) ;
	}
	else
	{
		return  ml_edit_delete_cols( screen->edit , len) ;
	}
}

int
ml_screen_clear_cols(
	ml_screen_t *  screen ,
	u_int  cols
	)
{
	if( screen->use_bce)
	{
		return  ml_edit_clear_cols_bce( screen->edit , cols) ;
	}
	else
	{
		return  ml_edit_clear_cols( screen->edit , cols) ;
	}
}

int
ml_screen_delete_lines(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_edit_delete_line( screen->edit))
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
ml_screen_clear_line_to_right(
	ml_screen_t *  screen
	)
{
	if( screen->use_bce)
	{
		return  ml_edit_clear_line_to_right_bce( screen->edit) ;
	}
	else
	{
		return  ml_edit_clear_line_to_right( screen->edit) ;
	}
}

int
ml_screen_clear_line_to_left(
	ml_screen_t *  screen
	)
{
	if( screen->use_bce)
	{
		return  ml_edit_clear_line_to_left_bce( screen->edit) ;
	}
	else
	{
		return  ml_edit_clear_line_to_left( screen->edit) ;
	}
}

int
ml_screen_clear_below(
	ml_screen_t *  screen
	)
{	
	if( screen->use_bce)
	{
		return  ml_edit_clear_below_bce( screen->edit) ;
	}
	else
	{
		return  ml_edit_clear_below( screen->edit) ;
	}
}

int
ml_screen_clear_above(
	ml_screen_t *  screen
	)
{
	if( screen->use_bce)
	{
		return  ml_edit_clear_above_bce( screen->edit) ;
	}
	else
	{
		return  ml_edit_clear_above( screen->edit) ;
	}
}

int
ml_screen_set_scroll_region(
	ml_screen_t *  screen ,
	int  beg ,
	int  end
	)
{
	return  ml_edit_set_scroll_region( screen->edit , beg , end) ;
}

int
ml_screen_scroll_upward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	return  ml_cursor_go_downward( screen->edit , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_screen_scroll_downward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	return  ml_cursor_go_upward( screen->edit , SCROLL | BREAK_BOUNDARY) ;
}

int
ml_screen_go_forward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_cursor_go_forward( screen->edit , BREAK_BOUNDARY))
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
ml_screen_go_back(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_cursor_go_back( screen->edit , 0))
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
ml_screen_go_upward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	int  count ;
	
	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_cursor_go_upward( screen->edit , BREAK_BOUNDARY))
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
ml_screen_go_downward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	int  count ;
	
	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_cursor_go_downward( screen->edit , BREAK_BOUNDARY))
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
ml_screen_goto_beg_of_line(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_goto_beg_of_line( screen->edit) ;
}

int
ml_screen_go_horizontally(
	ml_screen_t *  screen ,
	int  col
	)
{
	return  ml_screen_goto( screen , col , ml_cursor_row( screen->edit)) ;
}

int
ml_screen_go_vertically(
	ml_screen_t *  screen ,
	int  row
	)
{
	return  ml_screen_goto( screen , ml_cursor_col( screen->edit) , row) ;
}

int
ml_screen_goto_home(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_goto_home( screen->edit) ;
}

int
ml_screen_goto(
	ml_screen_t *  screen ,
	int  col ,
	int  row
	)
{
	return  ml_cursor_goto( screen->edit , col , row , BREAK_BOUNDARY) ;
}

int
ml_screen_save_cursor(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_save( screen->edit) ;
}

int
ml_screen_restore_cursor(
	ml_screen_t *  screen
	)
{
	return  ml_cursor_restore( screen->edit) ;
}

int
ml_screen_cursor_visible(
	ml_screen_t *  screen
	)
{
	if( screen->is_cursor_visible)
	{
		return  1 ;
	}
	
	screen->is_cursor_visible = 1 ;

	return  1 ;
}

int
ml_screen_cursor_invisible(
	ml_screen_t *  screen
	)
{
	if( ! screen->is_cursor_visible)
	{
		return  1 ;
	}
	
	ml_unhighlight_cursor( screen->edit) ;
	
	screen->is_cursor_visible = 0 ;

	return  1 ;
}

int
ml_screen_use_normal_edit(
	ml_screen_t *  screen
	)
{
	screen->edit = &screen->normal_edit ;

	if( screen->logvis)
	{
		(*screen->logvis->init)( screen->logvis , screen->edit) ;
	}

	ml_edit_set_modified_all( screen->edit) ;
	
	return  1 ;
}

int
ml_screen_use_alternative_edit(
	ml_screen_t *  screen
	)
{
	screen->edit = &screen->alt_edit ;

	ml_screen_goto_home( screen) ;
	ml_screen_clear_below( screen) ;

	if( screen->logvis)
	{
		(*screen->logvis->init)( screen->logvis , screen->edit) ;
	}

	ml_edit_set_modified_all( screen->edit) ;
	
	return  1 ;
}

int
ml_screen_fill_all_with_e(
	ml_screen_t *  screen
	)
{
	ml_char_t  e_ch ;

	ml_char_init( &e_ch) ;
	ml_char_set( &e_ch , "E" , 1 , US_ASCII , 0 , 0 , ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;

	ml_edit_fill_all( screen->edit , &e_ch) ;

	ml_char_final( &e_ch) ;

	return  1 ;
}
