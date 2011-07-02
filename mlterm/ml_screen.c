/*
 *	$Id$
 */

#include  "ml_screen.h"

#include  <stdlib.h>		/* abs */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_mem.h>	/* malloc/free */
#include  <kiklib/kik_str.h>	/* strdup */
#include  <kiklib/kik_util.h>	/* K_MIN */

#include  "ml_char_encoding.h"
#include  "ml_str_parser.h"


#define  ROW_IN_LOGS( screen , row) ( ml_get_num_of_logged_lines( &(screen)->logs) + row)

#if  1
#define  EXIT_BS_AT_BOTTOM
#endif


/* --- static variables --- */

static char *  word_separators = " .,:;/|@()[]{}" ;


/* --- static functions --- */

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
 * callbacks of ml_edit_scroll_event_listener_t.
 */

/*
 * Don't operate ml_model_t in this function because ml_model_t is not scrolled yet.
 * Operate ml_model_t in scrolled_out_lines_finished().
 */ 
static void
receive_scrolled_out_line(
	void *  p ,
	ml_line_t *  line
	)
{
	ml_screen_t *  screen ;

	screen = p ;

	if( screen->screen_listener && screen->screen_listener->line_scrolled_out)
	{
		(*screen->screen_listener->line_scrolled_out)( screen->screen_listener->self) ;
	}
	
	if( screen->logvis)
	{
		(*screen->logvis->visual_line)( screen->logvis , line) ;
	}
	else
	{
		line->num_of_filled_chars = ml_line_get_num_of_filled_chars_except_spaces( line) ;
	}
	
	ml_log_add( &screen->logs , line) ;

	if( ml_screen_is_backscrolling( screen) == BSM_STATIC)
	{
		screen->backscroll_rows ++ ;
	}

	if( screen->search)
	{
		screen->search->row -- ;
	}
}

static void
scrolled_out_lines_finished(
	void *  p
	)
{
	ml_screen_t *  screen ;

	screen = p ;
	
	if( ml_screen_is_backscrolling( screen) == BSM_VOLATILE)
	{
		ml_screen_set_modified_all( screen) ;
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

	if( screen->is_backscrolling)
	{
		/*
		 * Not necessary to scrolling window. If backscroll_mode is BSM_VOLATILE,
		 * ml_screen_set_modified_all() in scrolled_out_lines_finished() later.
		 */
		return  1 ;
	}

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

	if( screen->is_backscrolling)
	{
		/*
		 * Not necessary to scrolling window. If backscroll_mode is BSM_VOLATILE,
		 * ml_screen_set_modified_all() in scrolled_out_lines_finished() later.
		 */
		return  1 ;
	}
	
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
	int  beg_regarding_rtl ;

	row = end_row ;
	
	while( ( line = ml_screen_get_line( screen , row)) == NULL || ml_line_is_empty( line))
	{
		if( -- row < 0 && abs(row) > ml_get_num_of_logged_lines( &screen->logs))
		{
			return  0 ;
		}
	}

	if( row < end_row)
	{
		if( ml_line_is_rtl( line))
		{
			end_char_index = ml_line_beg_char_index_regarding_rtl( line) ;
		}
		else
		{
			if( ( end_char_index = ml_line_get_num_of_filled_chars_except_spaces( line)) > 0)
			{
				end_char_index -- ;
			}
		}
		
		end_row = row ;
	}

	row = beg_row ;

	while( 1)
	{
		while( ( line = ml_screen_get_line( screen , row)) == NULL || ml_line_is_empty( line))
		{
			if( ++ row > end_row)
			{
				return  0 ;
			}
		}

		size_except_spaces = ml_line_get_num_of_filled_chars_except_spaces( line) ;
		beg_regarding_rtl = ml_line_beg_char_index_regarding_rtl( line) ;

		if( row > beg_row)
		{
			if( ml_line_is_rtl( line))
			{
				beg_char_index = K_MAX(size_except_spaces,1) - 1 ;
			}
			else
			{
				beg_char_index = beg_regarding_rtl ;
			}
		}
		else if( beg_char_index < 0 || beg_char_index >= size_except_spaces)
		{
			if( ++ row > end_row)
			{
				return  0 ;
			}

			continue ;
		}

		break ;
	}
	
	if( row == end_row)
	{
		if( end_char_index < 0)
		{
			end_char_index = beg_regarding_rtl ;
		}
		else if( end_char_index >= size_except_spaces)
		{
			end_char_index = K_MAX(size_except_spaces,1) - 1 ;
		}
		
		if( ml_line_is_rtl( line))
		{
			for( char_index = K_MAX(end_char_index,beg_regarding_rtl) ;
				char_index < K_MIN(beg_char_index + 1,size_except_spaces) ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		else
		{
			for( char_index = K_MAX(beg_char_index,beg_regarding_rtl) ;
				char_index < K_MIN(end_char_index + 1,size_except_spaces) ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
	}
	else if( row < end_row)
	{
		if( ml_line_is_rtl( line))
		{
			for( char_index = beg_regarding_rtl ; char_index <= beg_char_index ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		else
		{
			for( char_index = K_MAX(beg_char_index,beg_regarding_rtl) ;
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
				continue ;
			}

			size_except_spaces = ml_line_get_num_of_filled_chars_except_spaces( line) ;
			beg_regarding_rtl = ml_line_beg_char_index_regarding_rtl( line) ;
			
			for( char_index = beg_regarding_rtl ;
				char_index < size_except_spaces ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		
		if( ( line = ml_screen_get_line( screen , row)) == NULL ||
			ml_line_is_empty( line))
		{
			return  1 ;
		}

		size_except_spaces = ml_line_get_num_of_filled_chars_except_spaces( line) ;
		beg_regarding_rtl = ml_line_beg_char_index_regarding_rtl( line) ;

		if( end_char_index < 0)
		{
			end_char_index = beg_regarding_rtl ;
		}
		else if( end_char_index >= size_except_spaces)
		{
			end_char_index = K_MAX(size_except_spaces,1) - 1 ;
		}
		
		if( ml_line_is_rtl( line))
		{
			for( char_index = K_MAX(end_char_index,beg_regarding_rtl) ;
				char_index < size_except_spaces ; char_index ++)
			{
				(*func)( line , char_index) ;
			}
		}
		else
		{
			for( char_index = beg_regarding_rtl ;
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
        int  beg_char_index ,		/* can be over size_except_spaces */
	int  beg_row ,
	int  end_char_index ,		/* can be over size_except_spaces */
	int  end_row
	)
{
	ml_line_t *  line ;
	int  count ;
	u_int  size_except_spaces ;
	int  beg_regarding_rtl ;
	int  row ;

	row = end_row ;
	
	while( ( line = ml_screen_get_line( screen , row)) == NULL || ml_line_is_empty( line))
	{
		if( -- row < 0 && abs(row) > ml_get_num_of_logged_lines( &screen->logs))
		{
			return  0 ;
		}
	}

	if( row < end_row)
	{
		if( ml_line_is_rtl( line))
		{
			end_char_index = ml_line_beg_char_index_regarding_rtl( line) ;
		}
		else
		{
			if( ( end_char_index = ml_line_get_num_of_filled_chars_except_spaces( line)) > 0)
			{
				end_char_index -- ;
			}
		}
		
		end_row = row ;
	}

	row = beg_row ;

	while( 1)
	{
		while( ( line = ml_screen_get_line( screen , row)) == NULL || ml_line_is_empty( line))
		{
			if( ++ row > end_row)
			{
				return  0 ;
			}
		}

		size_except_spaces = ml_line_get_num_of_filled_chars_except_spaces( line) ;
		beg_regarding_rtl = ml_line_beg_char_index_regarding_rtl( line) ;

		if( row > beg_row)
		{
			if( ml_line_is_rtl( line))
			{
				beg_char_index = K_MAX(size_except_spaces,1) - 1 ;
			}
			else
			{
				beg_char_index = beg_regarding_rtl ;
			}
		}
		else if( beg_char_index < 0 || beg_char_index >= size_except_spaces)
		{
			if( ++ row > end_row)
			{
				return  0 ;
			}

			continue ;
		}

		break ;
	}
		
	if( row == end_row)
	{
		if( end_char_index < 0)
		{
			end_char_index = beg_regarding_rtl ;
		}
		else if( end_char_index >= size_except_spaces)
		{
			end_char_index = K_MAX(size_except_spaces,1) - 1 ;
		}
		
		if( ml_line_is_rtl( line))
		{
			count = K_MIN(beg_char_index + 1,size_except_spaces) -
					K_MAX(end_char_index,beg_regarding_rtl) ;
			if( chars)
			{
				ml_line_copy_logical_str( line , chars , end_char_index , count) ;
			}
		}
		else
		{
			count = K_MIN(end_char_index + 1,size_except_spaces) -
					K_MAX(beg_char_index,beg_regarding_rtl) ;
			if( chars)
			{
				ml_line_copy_logical_str( line , chars , beg_char_index , count) ;
			}
		}
	}
	else if( row < end_row)
	{
		if( ml_line_is_rtl( line))
		{
			count = (K_MIN(beg_char_index + 1,size_except_spaces) - beg_regarding_rtl) ;
			if( chars)
			{
				ml_line_copy_logical_str( line , chars ,
					beg_regarding_rtl , count) ;
			}
		}
		else
		{
			count = (size_except_spaces - K_MAX(beg_char_index,beg_regarding_rtl)) ;
			if( chars)
			{
				ml_line_copy_logical_str( line , chars ,
					K_MAX(beg_char_index,beg_regarding_rtl) , count) ;
			}
		}

		if( ! ml_line_is_continued_to_next( line))
		{
			if( chars)
			{
				ml_char_copy( &chars[count] , ml_nl_ch()) ;
			}
			count ++ ;
		}

		for( row ++ ; row < end_row ; row ++)
		{
			line = ml_screen_get_line( screen , row) ;

			size_except_spaces = ml_line_get_num_of_filled_chars_except_spaces( line) ;
			beg_regarding_rtl = ml_line_beg_char_index_regarding_rtl( line) ;

			if( chars)
			{
				ml_line_copy_logical_str( line , &chars[count] ,
					beg_regarding_rtl ,
					size_except_spaces - beg_regarding_rtl) ;
			}
			count += (size_except_spaces - beg_regarding_rtl) ;

			if( ! ml_line_is_continued_to_next( line))
			{
				if( chars)
				{
					ml_char_copy( &chars[count] , ml_nl_ch()) ;
				}
				count ++ ;
			}
		}

		line = ml_screen_get_line( screen , row) ;

		size_except_spaces = ml_line_get_num_of_filled_chars_except_spaces( line) ;
		beg_regarding_rtl = ml_line_beg_char_index_regarding_rtl( line) ;

		if( end_char_index < 0)
		{
			end_char_index = beg_regarding_rtl ;
		}
		else if( end_char_index >= size_except_spaces)
		{
			end_char_index = K_MAX(size_except_spaces,1) - 1 ;
		}
		
		if( ml_line_is_rtl( line))
		{
			if( chars)
			{
				ml_line_copy_logical_str( line , &chars[count] ,
					K_MAX(end_char_index,beg_regarding_rtl) ,
					size_except_spaces -
						K_MAX(end_char_index,beg_regarding_rtl)) ;
			}
			count += (size_except_spaces - K_MAX(end_char_index,beg_regarding_rtl)) ;
		}
		else
		{
			if( chars)
			{
				ml_line_copy_logical_str( line , &chars[count] ,
					beg_regarding_rtl ,
					K_MIN(end_char_index + 1,size_except_spaces) -
						beg_regarding_rtl) ;
			}
			count += K_MIN(end_char_index + 1,size_except_spaces) ;
		}
	}
	else
	{
		count = 0;
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " copy region is illegal. nothing is copyed.\n") ;
	#endif
	}

	return  count ;
}


/* --- global functions --- */

int
ml_set_word_separators(
	char *  seps
	)
{
	static char *  default_word_separators ;

	if( default_word_separators)
	{
		free( word_separators) ;

		if( seps == NULL || *seps == '\0')
		{
			/* Fall back to default. */
			word_separators = default_word_separators ;

			return  1 ;
		}
	}
	else if( seps == NULL || *seps == '\0')
	{
		/* Not changed */
		return  1 ;
	}
	else
	{
		default_word_separators = word_separators ;
	}

	word_separators = strdup( seps) ;

	return  1 ;
}


ml_screen_t *
ml_screen_new(
	u_int  cols ,
	u_int  rows ,
	u_int  tab_size ,
	u_int  num_of_log_lines ,
	int  use_bce ,
	ml_bs_mode_t  bs_mode
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
	screen->edit_scroll_listener.receive_scrolled_out_line = receive_scrolled_out_line ;
	screen->edit_scroll_listener.scrolled_out_lines_finished = scrolled_out_lines_finished ;
	screen->edit_scroll_listener.window_scroll_upward_region = window_scroll_upward_region ;
	screen->edit_scroll_listener.window_scroll_downward_region = window_scroll_downward_region ;

	if( ! ml_edit_init( &screen->normal_edit , &screen->edit_scroll_listener ,
		cols , rows , tab_size , 1 , use_bce))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_edit_init(normal_edit) failed.\n") ;
	#endif
	
		goto  error1 ;
	}

	if( ! ml_edit_init( &screen->alt_edit , &screen->edit_scroll_listener ,
		cols , rows , tab_size , 0 , use_bce))
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

	screen->search = NULL ;

	screen->backscroll_rows = 0 ;
	screen->backscroll_mode = bs_mode ;
	screen->is_backscrolling = 0 ;

	screen->use_dynamic_comb = 0 ;
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

	ml_screen_search_final( screen) ;

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
	if( row < -(int)ml_get_num_of_logged_lines( &screen->logs))
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
	if( screen->is_backscrolling && screen->backscroll_rows > 0)
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
		if( ( line = ml_screen_get_line_in_screen( screen , row)))
		{
			ml_line_set_modified_all( line) ;
		}
	}

	return  1 ;
}

int
ml_screen_add_logical_visual(
	ml_screen_t *  screen ,
	ml_logical_visual_t *  logvis
	)
{
	(*logvis->init)( logvis , &screen->edit->model , &screen->edit->cursor) ;

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

		return  1 ;
	}
	else
	{
		return  0 ;
	}
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
		return  1 ;
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
		return  1 ;
	}
}

int
ml_set_backscroll_mode(
	ml_screen_t *  screen ,
	ml_bs_mode_t   mode
	)
{
	if( mode != BSM_VOLATILE && mode != BSM_STATIC)
	{
		return  0 ;
	}
	
	screen->backscroll_mode = mode ;

	if( screen->is_backscrolling)
	{
		screen->is_backscrolling = mode ;
	}
	
	return  1 ;
}

int
ml_enter_backscroll_mode(
	ml_screen_t *  screen
	)
{
	screen->is_backscrolling = screen->backscroll_mode ;

	return  1 ;
}

int
ml_exit_backscroll_mode(
	ml_screen_t *  screen
	)
{
	screen->is_backscrolling = 0 ;
	screen->backscroll_rows = 0 ;

	ml_screen_set_modified_all( screen) ;

	return  1 ;
}

int
ml_screen_backscroll_to(
	ml_screen_t *  screen ,
	int  row
	)
{
	ml_line_t *  line ;
	u_int  count ;
	
	if( ! screen->is_backscrolling)
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
			break ;
		}

		ml_line_set_modified_all( line) ;
	}

#ifdef  EXIT_BS_AT_BOTTOM
	if( screen->backscroll_rows == 0)
	{
		ml_exit_backscroll_mode( screen) ;
	}
#endif

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

	if( ! screen->is_backscrolling)
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
	
#ifdef  EXIT_BS_AT_BOTTOM
	if( screen->backscroll_rows == 0)
	{
		ml_exit_backscroll_mode( screen) ;
	}
#endif

	return  1 ;
}

int
ml_screen_backscroll_downward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	ml_line_t *  line ;
	u_int  count ;
	
	if( ! screen->is_backscrolling)
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
	int  flag ;

	if( ( base_line = ml_screen_get_line( screen , base_row)) == NULL ||
		ml_line_is_empty( base_line))
	{
		return  0 ;
	}

	if( is_word_separator( ml_char_at( base_line , base_char_index)))
	{
		*beg_char_index = base_char_index ;
		*end_char_index = base_char_index ;
		*beg_row = base_row ;
		*end_row = base_row ;

		return  1 ;
	}

	flag = ml_char_is_biwidth( ml_char_at( base_line , base_char_index)) ;
	
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
		
		ch = ml_char_at( line , char_index) ;

		if( is_word_separator(ch) || flag != ml_char_is_biwidth( ch))
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
		
		ch = ml_char_at( line , char_index) ;

		if( is_word_separator(ch) || flag != ml_char_is_biwidth( ch))
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

	if( ( ch = ml_char_at( line , char_index)) == NULL)
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

	if( ( ch = ml_char_at( line , char_index)) == NULL)
	{
		return  0 ;
	}
	
	if( ! ml_char_combine( ch , bytes , ch_size , cs , is_biwidth , is_comb ,
		fg_color , bg_color , is_bold , is_underlined))
	{
		return  0 ;
	}
	
	ml_line_set_modified( line , char_index , char_index) ;
	
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
ml_screen_vertical_forward_tabs(
	ml_screen_t *  screen ,
	u_int  num
	)
{
	return  ml_edit_vertical_forward_tabs( screen->edit , num) ;
}

int
ml_screen_vertical_backward_tabs(
	ml_screen_t *  screen ,
	u_int  num
	)
{
	return  ml_edit_vertical_backward_tabs( screen->edit , num) ;
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
	u_int  count ;

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
	return  ml_edit_go_downward( screen->edit , SCROLL) ;
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
	return  ml_edit_delete_cols( screen->edit , len) ;
}

int
ml_screen_clear_cols(
	ml_screen_t *  screen ,
	u_int  cols
	)
{
	return  ml_edit_clear_cols( screen->edit , cols) ;
}

int
ml_screen_delete_lines(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	u_int  count ;

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
	return  ml_edit_clear_line_to_right( screen->edit) ;
}

int
ml_screen_clear_line_to_left(
	ml_screen_t *  screen
	)
{
	return  ml_edit_clear_line_to_left( screen->edit) ;
}

int
ml_screen_clear_below(
	ml_screen_t *  screen
	)
{
	return  ml_edit_clear_below( screen->edit) ;
}

int
ml_screen_clear_above(
	ml_screen_t *  screen
	)
{
	return  ml_edit_clear_above( screen->edit) ;
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
ml_screen_index(
	ml_screen_t *  screen
	)
{
	return  ml_edit_go_downward( screen->edit , SCROLL) ;
}

int
ml_screen_reverse_index(
	ml_screen_t *  screen
	)
{
	return  ml_edit_go_upward( screen->edit , SCROLL) ;
}

int
ml_screen_scroll_upward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	return  ml_edit_scroll_upward( screen->edit , size) ;
}

int
ml_screen_scroll_downward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	return  ml_edit_scroll_downward( screen->edit , size) ;
}

int
ml_screen_go_forward(
	ml_screen_t *  screen ,
	u_int  size
	)
{
	u_int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_edit_go_forward( screen->edit , 0))
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
	u_int  count ;

	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_edit_go_back( screen->edit , 0))
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
	u_int  count ;
	
	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_edit_go_upward( screen->edit , 0))
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
	u_int  count ;
	
	for( count = 0 ; count < size ; count ++)
	{
		if( ! ml_edit_go_downward( screen->edit , 0))
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
	return  ml_edit_goto_beg_of_line( screen->edit) ;
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
	return  ml_edit_goto_home( screen->edit) ;
}

int
ml_screen_goto(
	ml_screen_t *  screen ,
	int  col ,
	int  row
	)
{
	return  ml_edit_goto( screen->edit , col , row) ;
}

int
ml_screen_set_relative_origin(
	ml_screen_t *  screen
	)
{
	return  ml_edit_set_relative_origin( screen->edit) ;
}

int
ml_screen_set_absolute_origin(
	ml_screen_t *  screen
	)
{
	return  ml_edit_set_absolute_origin( screen->edit) ;
}

int
ml_screen_set_auto_wrap(
	ml_screen_t *  screen
	)
{
	return  ml_edit_set_auto_wrap( screen->edit) ;
}

int
ml_screen_unset_auto_wrap(
	ml_screen_t *  screen
	)
{
	return  ml_edit_unset_auto_wrap( screen->edit) ;
}

int
ml_screen_save_cursor(
	ml_screen_t *  screen
	)
{
	return  ml_edit_save_cursor( screen->edit) ;
}

int
ml_screen_restore_cursor(
	ml_screen_t *  screen
	)
{
	return  ml_edit_restore_cursor( screen->edit) ;
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
	
	screen->is_cursor_visible = 0 ;

	return  1 ;
}

int
ml_screen_is_cursor_visible(
	ml_screen_t *  screen
	)
{
	return screen->is_cursor_visible ;
}

int
ml_screen_is_alternative_edit(
	ml_screen_t *  screen
        )
{
	if( screen->edit == &(screen->alt_edit) )
	{
		return  1 ;
	}

	return  0 ;
}

int
ml_screen_use_normal_edit(
	ml_screen_t *  screen
	)
{
	screen->edit = &screen->normal_edit ;

	if( screen->logvis)
	{
		(*screen->logvis->init)( screen->logvis , &screen->edit->model , &screen->edit->cursor) ;
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
		(*screen->logvis->init)( screen->logvis , &screen->edit->model , &screen->edit->cursor) ;
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

ml_bs_mode_t
ml_screen_is_backscrolling(
	ml_screen_t *  screen
	)
{
	if( screen->is_backscrolling == BSM_STATIC)
	{
		if( screen->backscroll_rows < ml_get_log_size( &screen->logs))
		{
			return  BSM_STATIC ;
		}
		else
		{
			return  BSM_VOLATILE ;
		}
	}
	else
	{
		return  screen->is_backscrolling ;
	}
}

int
ml_screen_search_init(
	ml_screen_t *  screen ,
	int (*match)( size_t * , size_t * , void * , u_char * , int)
	)
{
	if( screen->search)
	{
		return  0 ;
	}

	if( ! ( screen->search = malloc( sizeof( *screen->search))))
	{
		return  0 ;
	}

	screen->search->match = match ;

	ml_screen_search_reset_position( screen) ;

	return  1 ;
}

int
ml_screen_search_final(
	ml_screen_t *  screen
	)
{
	free( screen->search) ;
	screen->search = NULL ;

	return  1 ;
}

int
ml_screen_search_reset_position(
	ml_screen_t *  screen
	)
{
	if( ! screen->search)
	{
		return  0 ;
	}

	/* char_index == -1 has special meaning. */
	screen->search->char_index = -1 ;
	screen->search->row = -1 ;

	return  1 ;
}

/*
 * It is assumed that this function is called in *visual* context.
 *
 * XXX
 * It is not supported to match text in multiple lines.
 */
int
ml_screen_search_find(
	ml_screen_t *  screen ,
	int *  beg_char_index ,		/* visual position is returned */
	int *  beg_row ,		/* visual position is returned */
	int *  end_char_index ,		/* visual position is returned */
	int *  end_row ,		/* visual position is returned */
	void *  regex ,
	int  backward
	)
{
	ml_char_t *  line_str ;
	mkf_parser_t *  parser ;
	mkf_conv_t *  conv ;
	u_char *  buf ;
	size_t  buf_len ;
	ml_line_t *  line ;
	int  step ;
	int  res ;

	if( ! screen->search)
	{
		return  0 ;
	}

	if( ! ( line_str = ml_str_alloca( ml_screen_get_logical_cols( screen))))
	{
		return  0 ;
	}

	buf_len = ml_screen_get_logical_cols( screen) * MLCHAR_UTF_MAX_SIZE + 1 ;
	if( ! ( buf = alloca( buf_len)))
	{
		return  0 ;
	}

	if( ! (parser = ml_str_parser_new()))
	{
		return  0 ;
	}
	
	if( ! (conv = ml_conv_new( ML_UTF8)))
	{
		(*parser->delete)( parser) ;

		return  0 ;
	}

	/* char_index == -1 has special meaning. */
	if( screen->search->char_index == -1)
	{
		screen->search->char_index = ml_screen_cursor_char_index( screen) ;
		screen->search->row = ml_screen_cursor_row( screen) ;
	}

	*beg_char_index = screen->search->char_index ;
	*beg_row = screen->search->row ;
	step = (backward ? -1 : 1) ;

#ifdef  DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " Search start from %d %d\n" ,
			*beg_char_index , *beg_row) ;
#endif

	res = 0 ;

	for( ; (line = ml_screen_get_line( screen , *beg_row)) ; (*beg_row) += step)
	{
		size_t  line_len ;
		size_t  match_beg ;
		size_t  match_len ;

		if( ( line_len = ml_line_get_num_of_filled_chars_except_spaces( line)) == 0)
		{
			continue ;
		}

		/*
		 * Visual => Logical
		 */
		ml_line_copy_logical_str( line , line_str , 0 , line_len) ;

		(*parser->init)( parser) ;
		if( backward)
		{
			ml_str_parser_set_str( parser , line_str ,
				(*beg_row != screen->search->row) ?
					line_len : K_MIN(*beg_char_index + 1,line_len)) ;
			*beg_char_index = 0 ;
		}
		else
		{
			if( *beg_row != screen->search->row)
			{
				*beg_char_index = 0 ;
			}
			else if( line_len <= *beg_char_index)
			{
				continue ;
			}

			ml_str_parser_set_str( parser , line_str + (*beg_char_index) ,
					line_len - *beg_char_index) ;
		}

		(*conv->init)( conv) ;
		*(buf + (*conv->convert)( conv , buf , buf_len - 1 , parser)) = '\0' ;

		if( (*screen->search->match)( &match_beg , &match_len ,
					regex , buf , backward))
		{
			size_t  count ;
			size_t  comb_size ;
			int  beg ;
			int  end ;
			int  meet_pos ;

			ml_get_combining_chars( line_str + (*beg_char_index) , &comb_size) ;

			for( count = 0 ; count < match_beg ; count++)
			{
				/* Ignore 2nd and following bytes. */
				if( (buf[count] & 0xc0) != 0x80)
				{
					if( comb_size > 0)
					{
						comb_size -- ;
					}
					else if( (++ (*beg_char_index)) >= line_len - 1)
					{
						break ;
					}
					else
					{
						ml_get_combining_chars(
							line_str + (*beg_char_index) ,
							&comb_size) ;
					}
				}
			}

			*end_char_index = (*beg_char_index) - 1 ;
			for( ; count < match_beg + match_len ; count++)
			{
				/* Ignore 2nd and following bytes. */
				if( (buf[count] & 0xc0) != 0x80)
				{
					if( comb_size > 0)
					{
						comb_size -- ;
					}
					else if( (++ (*end_char_index)) >= line_len - 1)
					{
						break ;
					}
					else
					{
						ml_get_combining_chars(
							line_str + (*end_char_index) ,
							&comb_size) ;
					}
				}
			}

			if( *end_char_index < *beg_char_index)
			{
				continue ;
			}

			*end_row = *beg_row ;

			if( backward)
			{
				if( *beg_char_index > 0)
				{
					screen->search->char_index = *beg_char_index - 1 ;
					screen->search->row = *beg_row ;
				}
				else
				{
					screen->search->char_index =
						ml_screen_get_logical_cols( screen) - 1 ;
					screen->search->row = *beg_row - 1 ;
				}
			}
			else
			{
				screen->search->char_index = *beg_char_index + 1 ;
				screen->search->row = *beg_row ;
			}

		#ifdef  DEBUG
			kik_debug_printf( KIK_DEBUG_TAG " Search position x %d y %d\n" ,
					screen->search->char_index , screen->search->row) ;
		#endif

			/*
			 * Logical => Visual
			 *
			 * XXX Incomplete
			 * Current implementation have problems like this.
			 *  (logical)RRRLLLNNN => (visual)NNNLLLRRR
			 *  Searching LLLNNN =>           ^^^^^^ hits but only NNNL is reversed.
			 */
			meet_pos = 0 ;
			beg = ml_line_convert_logical_char_index_to_visual(
					line , *beg_char_index , &meet_pos) ;
			end = ml_line_convert_logical_char_index_to_visual(
					line , *end_char_index , &meet_pos) ;
			
			if( beg > end)
			{
				*beg_char_index = end ;
				*end_char_index = beg ;
			}
			else
			{
				*beg_char_index = beg ;
				*end_char_index = end ;
			}

			if( ml_line_is_rtl( line))
			{
				int  char_index ;

				/* XXX for x_selection */
				char_index = -(*beg_char_index) ;
				*beg_char_index = -(*end_char_index) ;
				*end_char_index = char_index ;
			}

			res = 1 ;

			break ;
		}
	}

	(*parser->delete)( parser) ;
	(*conv->delete)( conv) ;
	ml_str_final( line_str , ml_screen_get_logical_cols( screen)) ;

	return  res ;
}
