/*
 *	$Id$
 */

#include  "ml_edit.h"

#include  <string.h>		/* memmove/memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "ml_edit_intern.h"
#include  "ml_edit_scroll.h"
#include  "ml_bidi.h"


#if  0
#define  __DEBUG
#endif

#if  0
#define  CURSOR_DEBUG
#endif

#define  TAB_STOPS_SIZE(edit)  (((edit)->model.num_of_cols - 1) / 8 + 1)


/* --- static functions --- */

static void
reset_wraparound_checker(
	ml_edit_t *  edit
	)
{
	edit->wraparound_ready_line = NULL ;
}

static int
cursor_goto_intern(
	ml_edit_t *  edit ,
	int  col_or_indx ,
	int  row ,
	int  flag ,		/* BREAK_BOUNDARY */
	int  is_col
	)
{
	u_int  brk_size ;
	u_int  actual ;
	int  char_index ;
	int  cols_rest ;
	ml_line_t *  line ;

	if( row > ml_model_end_row( &edit->model))
	{
		if( ! ( flag & BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot goto row %d(total lines are %d)\n" ,
				row , edit->model.num_of_filled_rows) ;
		#endif

			return  0 ;
		}

		brk_size = row - ml_model_end_row( &edit->model) ;
		
		if( ( actual = ml_model_break_boundary( &edit->model , brk_size)) < brk_size)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " cursor cannot goto row %d(total lines are %d)\n" ,
				row , edit->model.num_of_filled_rows) ;
		#endif

			row -= (brk_size - actual) ;
		}
	}

	line = ml_model_get_line( &edit->model , row) ;
	
	if( is_col)
	{
		char_index = ml_convert_col_to_char_index( line , &cols_rest , col_or_indx ,
				BREAK_BOUNDARY) ;
	}
	else
	{
		char_index = col_or_indx ;
		cols_rest = 0 ;
	}

	if( char_index > ml_line_end_char_index( line))
	{
		if( ! ( flag & BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf(
				KIK_DEBUG_TAG " cursor cannot goto char_index %d(line length is %d)\n" ,
				char_index , line->num_of_filled_chars) ;
		#endif
		
			return  0 ;
		}

		brk_size = char_index + cols_rest + 1 - line->num_of_filled_chars ;
		
		if( ( actual = ml_line_break_boundary( line , brk_size)) < brk_size)
		{
		#ifdef  DEBUG
			kik_warn_printf(
				KIK_DEBUG_TAG " cursor cannot goto char index %d(line length is %d)\n" ,
				char_index , line->num_of_filled_chars) ;
		#endif

			char_index -= (brk_size - actual) ;
		}
	}

	edit->cursor.char_index = char_index ;
	edit->cursor.row = row ;
	edit->cursor.col_in_char = cols_rest ;
	edit->cursor.col =
		ml_convert_char_index_to_col( CURSOR_LINE(edit) , edit->cursor.char_index , 0)
		+ edit->cursor.col_in_char ;

	return  1 ;
}

static int
cursor_goto_by_char_index(
	ml_edit_t *  edit ,
	int  char_index ,
	int  row ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	return  cursor_goto_intern( edit , char_index , row , flag , 0) ;
}

static int
cursor_goto_by_col(
	ml_edit_t *  edit ,
	int  col ,
	int  row ,
	int  flag		/* BREAK_BOUNDARY */
	)
{
	if( col >= edit->model.num_of_cols)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " col(%d) is larger than num_of_cols(%d)" ,
			col , edit->model.num_of_cols) ;
	#endif
	
		col = edit->model.num_of_cols - 1 ;
		
	#ifdef  DEBUG
		kik_msg_printf( " ... col modified -> %d\n" , col) ;
	#endif
	}
	
	return  cursor_goto_intern( edit , col , row , flag , 1) ;
}

/*
 * inserting chars within a line.
 */
static int
insert_chars(
	ml_edit_t *  edit ,
	ml_char_t *  ins_chars ,
	u_int  num_of_ins_chars ,
	int  do_move_cursor
	)
{
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	u_int  filled_cols ;
	u_int  last_index ;
	u_int  cols_after ;
	int  count ;
	ml_line_t *  cursor_line ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	cursor_line = CURSOR_LINE(edit) ;

	buf_len = edit->model.num_of_cols ;
	
	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/*
	 * before cursor(excluding cursor)
	 */
	 
	if( edit->cursor.col_in_char)
	{
	#ifdef  DEBUG
		if( ml_char_cols( CURSOR_CHAR(edit)) <= edit->cursor.col_in_char)
		{
			kik_warn_printf( KIK_DEBUG_TAG " illegal col_in_char.\n") ;
		}
	#endif
	
		/*
		 * padding spaces for former half of cursor.
		 */
		for( count = 0 ; count < edit->cursor.col_in_char ; count ++)
		{
			ml_char_copy( &buffer[filled_len ++] , ml_sp_ch()) ;
		}

		cols_after = ml_char_cols( CURSOR_CHAR(edit)) - edit->cursor.col_in_char ;
	}
	else
	{
		cols_after = 0 ;
	}

	/*
	 * chars are appended one by one below since the line may be full.
	 */
	 
	/*
	 * inserted chars
	 */
	
	filled_cols = ml_str_cols( buffer , filled_len) ;
	
	for( count = 0 ; count < num_of_ins_chars ; count ++)
	{
		if( filled_cols + ml_char_cols( &ins_chars[count]) > edit->model.num_of_cols)
		{
			break ;
		}

		ml_char_copy( &buffer[filled_len ++] , &ins_chars[count]) ;
		filled_cols += ml_char_cols( &ins_chars[count]) ;
	}

	last_index = filled_len ;

	if( filled_cols < edit->model.num_of_cols)
	{
		/*
		 * cursor char
		 */
		 
		if( cols_after)
		{
			/*
			 * padding spaces for latter half of cursor.
			 */
			for( count = 0 ; count < cols_after ; count ++)
			{
				if( filled_cols + ml_char_cols( ml_sp_ch()) > edit->model.num_of_cols)
				{
					goto  line_full ;
				}

				ml_char_copy( &buffer[filled_len++] , ml_sp_ch()) ;
				filled_cols += ml_char_cols( ml_sp_ch()) ;
			}
		}
		else
		{
			if( filled_cols + ml_char_cols( CURSOR_CHAR(edit)) > edit->model.num_of_cols)
			{
				goto  line_full ;
			}

			ml_char_copy( &buffer[filled_len++] , CURSOR_CHAR(edit)) ;
			filled_cols += ml_char_cols( CURSOR_CHAR(edit)) ;
		}

		/*
		 * after cursor(excluding cursor)
		 */
		 
		for( count = edit->cursor.char_index + 1 ;
			count < cursor_line->num_of_filled_chars ;
			count ++)
		{
			if( filled_cols + ml_char_cols( &cursor_line->chars[count]) >
				edit->model.num_of_cols)
			{
				break ;
			}

			ml_char_copy( &buffer[filled_len ++] , &cursor_line->chars[count]) ;
			filled_cols += ml_char_cols( &cursor_line->chars[count]) ;
		}
	}

line_full:
	
	/*
	 * overwriting.
	 */

	if( edit->cursor.char_index + filled_len > edit->model.num_of_cols)
	{
		filled_len = edit->model.num_of_cols - edit->cursor.char_index ;
	}

	ml_line_overwrite( cursor_line , edit->cursor.char_index ,
		buffer , filled_len , filled_cols) ;

	ml_str_final( buffer , buf_len) ;

	if( do_move_cursor)
	{
		cursor_goto_by_char_index( edit , edit->cursor.char_index + last_index ,
			edit->cursor.row , BREAK_BOUNDARY) ;
	}
	else if( edit->cursor.col_in_char)
	{
		cursor_goto_by_char_index( edit , edit->cursor.char_index + edit->cursor.col_in_char ,
			edit->cursor.row , BREAK_BOUNDARY) ;
	}

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	return  1 ;
}

static int
clear_cols(
	ml_edit_t *  edit ,
	u_int  cols ,
	int  use_bce
	)
{
	ml_line_t *  cursor_line ;
	
	if( edit->cursor.col + cols >= edit->model.num_of_cols)
	{
		if( use_bce)
		{
			return  ml_edit_clear_line_to_right_bce( edit) ;
		}
		else
		{
			return  ml_edit_clear_line_to_right( edit) ;
		}
	}

	cursor_line = CURSOR_LINE(edit) ;

	if( edit->cursor.col_in_char)
	{
		ml_line_fill( cursor_line , ml_sp_ch() , edit->cursor.char_index ,
			edit->cursor.col_in_char) ;
		edit->cursor.char_index += edit->cursor.col_in_char ;
	}

	if( use_bce)
	{
		ml_line_fill( cursor_line , &edit->bce_ch , edit->cursor.char_index , cols) ;
	}
	else
	{
		ml_line_fill( cursor_line , ml_sp_ch() , edit->cursor.char_index , cols) ;
	}

	return  1 ;
}

static int
clear_line_to_left(
	ml_edit_t *  edit ,
	int  use_bce
	)
{
	ml_line_t *  cursor_line ;
	ml_char_t *  sp_ch ;

	cursor_line = CURSOR_LINE(edit) ;

	if( use_bce)
	{
		sp_ch = &edit->bce_ch ;
	}
	else
	{
		sp_ch = ml_sp_ch() ;
	}

	edit->cursor.char_index = edit->cursor.col ;
		
	ml_line_fill( cursor_line , sp_ch , 0 , edit->cursor.char_index + 1) ;

	return  1 ;
}

static int
vertical_tabs(
	ml_edit_t *  edit ,
	u_int  num ,
	int  is_forward
	)
{
	int  col ;
	int  count ;
	
	reset_wraparound_checker( edit) ;

	col = edit->cursor.col ;
	
	for( count = 0 ; count < num ; count ++)
	{
		while( 1)
		{
			if( is_forward)
			{
				if( col >= edit->model.num_of_cols - 1)
				{
					goto  end ;
				}

				col ++ ;
			}
			else
			{
				if( col <= 0)
				{
					goto  end ;
				}

				col -- ;
			}

			if( edit->tab_stops[col / 8] & ( 1 << (7 - col % 8)))
			{
				break ;
			}
		}
	}

end:
	cursor_goto_by_col( edit , col , edit->cursor.row , BREAK_BOUNDARY) ;
	
	return  1 ;
}


/* --- global functions --- */

int
ml_edit_init(
	ml_edit_t *  edit ,
	ml_edit_scroll_event_listener_t *  scroll_listener ,
	u_int  num_of_cols ,
	u_int  num_of_rows ,
	u_int  tab_size ,
	int  is_logging
	)
{
	if( num_of_cols == 0 || num_of_rows == 0)
	{
		return  0 ;
	}

	ml_char_init( &edit->bce_ch) ;
	ml_char_set( &edit->bce_ch , " " , 1 , US_ASCII , 0 , 0 , ML_FG_COLOR , ML_BG_COLOR , 0 , 0) ;

	if( ! ml_model_init( &edit->model , num_of_cols , num_of_rows))
	{
		return  0 ;
	}
	
	ml_model_break_boundary( &edit->model , 1) ;
	
	edit->cursor.row = 0 ;
	edit->cursor.char_index = 0 ;
	edit->cursor.col = 0 ;
	edit->cursor.col_in_char = 0 ;
	edit->cursor.orig_fg = ML_BG_COLOR ;
	edit->cursor.orig_bg = ML_FG_COLOR ;
	edit->cursor.is_highlighted = 0 ;
	edit->cursor.saved_row = 0 ;
	edit->cursor.saved_char_index = 0 ;
	edit->cursor.saved_col = 0 ;
	edit->cursor.is_saved = 0 ;

	edit->is_logging = is_logging ;

	edit->wraparound_ready_line = NULL ;
	
	edit->scroll_region_beg = 0 ;
	edit->scroll_region_end = edit->model.num_of_rows - 1 ;
	edit->scroll_listener = scroll_listener ;

	if( ( edit->tab_stops = malloc( TAB_STOPS_SIZE(edit))) == NULL)
	{
		return  0 ;
	}
	ml_edit_set_tab_size( edit , tab_size) ;

	return  1 ;
}

int
ml_edit_final(
	ml_edit_t *  edit
	)
{
	ml_model_final( &edit->model) ;

	free( edit->tab_stops) ;

	ml_char_final( &edit->bce_ch) ;

	return  1 ;
}

int
ml_edit_resize(
	ml_edit_t *  edit ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	if( num_of_cols == 0 || num_of_rows == 0)
	{
		return  0 ;
	}
	
	if( num_of_cols < edit->model.num_of_cols)
	{
		if( edit->cursor.col >= num_of_cols)
		{
			edit->cursor.col = num_of_cols - 1 ;
			edit->cursor.char_index =
				ml_convert_col_to_char_index(
					CURSOR_LINE(edit) , NULL , edit->cursor.col , 0) ;
		}
	}

	if( num_of_rows < edit->model.num_of_filled_rows)
	{
		if( edit->cursor.row < edit->model.num_of_filled_rows - num_of_rows)
		{
			edit->cursor.row = 0 ;
			edit->cursor.char_index = 0 ;
			edit->cursor.col = 0 ;
		}
		else
		{
			edit->cursor.row -= (edit->model.num_of_filled_rows - num_of_rows) ;
		}
	}

	if( ! ml_model_resize( &edit->model , num_of_cols , num_of_rows))
	{
		return  0 ;
	}
	
	edit->wraparound_ready_line = NULL ;
	
	edit->scroll_region_beg = 0 ;
	edit->scroll_region_end = edit->model.num_of_rows - 1 ;

	free( edit->tab_stops) ;
	if( ( edit->tab_stops = malloc( TAB_STOPS_SIZE(edit))) == NULL)
	{
		return  0 ;
	}
	ml_edit_set_tab_size( edit , edit->tab_size) ;

	if( edit->cursor.row > edit->model.num_of_rows)
	{
		edit->cursor.row = ml_model_end_row( &edit->model) ;
		edit->cursor.char_index = ml_line_end_char_index( CURSOR_LINE(edit)) ;
		edit->cursor.col = ml_convert_char_index_to_col( CURSOR_LINE(edit) ,
					edit->cursor.char_index , 0) ;
	}
	else if( edit->cursor.col > edit->model.num_of_cols)
	{
		edit->cursor.char_index = ml_line_end_char_index( CURSOR_LINE(edit)) ;
		edit->cursor.col = ml_convert_char_index_to_col( CURSOR_LINE(edit) ,
					edit->cursor.char_index , 0) ;
	}

#ifdef  __DEBUG
	kik_msg_printf( "-> char index %d row %d\n" ,
		edit->cursor.char_index , edit->cursor.row) ;
#endif

	return  1 ;
}

int
ml_edit_insert_chars(
	ml_edit_t *  edit ,
	ml_char_t *  ins_chars ,
	u_int  num_of_ins_chars
	)
{
	reset_wraparound_checker( edit) ;

	return  insert_chars( edit , ins_chars , num_of_ins_chars , 1) ;
}

int
ml_edit_insert_blank_chars(
	ml_edit_t *  edit ,
	u_int  num_of_blank_chars
	)
{
	int  count ;
	ml_char_t *  blank_chars ;

	reset_wraparound_checker( edit) ;

	if( ( blank_chars = ml_str_alloca( num_of_blank_chars)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  0 ;
	}

	for( count = 0 ; count < num_of_blank_chars ; count ++)
	{
		ml_char_copy( &blank_chars[count] , ml_sp_ch()) ;
	}

	ml_str_final( blank_chars , num_of_blank_chars) ;

	/* the cursor will not moved. */
	return  insert_chars( edit , blank_chars , num_of_blank_chars , 0) ;
}

int
ml_edit_overwrite_chars(
	ml_edit_t *  edit ,
	ml_char_t *  ow_chars ,
	u_int  num_of_ow_chars
	)
{
	int  count ;
	int  cursor_index ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	ml_line_t *  line ;
	int  beg ;
	u_int  cols ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	buf_len = num_of_ow_chars + edit->model.num_of_cols ;

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	line = CURSOR_LINE(edit) ;
	filled_len = 0 ;

	/* before cursor(excluding cursor) */
	
	if( edit->cursor.col_in_char)
	{
		int  count ;

		/*
		 * padding spaces before cursor.
		 */
		for( count = 0 ; count < edit->cursor.col_in_char ; count ++)
		{
			ml_char_copy( &buffer[filled_len ++] , ml_sp_ch()) ;
		}
	}
	
	/* appending overwriting chars */
	ml_str_copy( &buffer[filled_len] , ow_chars , num_of_ow_chars) ;
	filled_len += num_of_ow_chars ;
	
	cursor_index = filled_len ;

	/*
	 * overwriting
	 */

	beg = 0 ;
	count = 0 ;
	cols = 0 ;

	while( 1)
	{
		if( edit->cursor.col + cols + ml_char_cols( &buffer[count]) > edit->model.num_of_cols ||
			(edit->wraparound_ready_line &&
			edit->cursor.col + cols + ml_char_cols( &buffer[count]) == edit->model.num_of_cols))
		{
			ml_line_overwrite( line , edit->cursor.char_index , &buffer[beg] ,
				count - beg , cols) ;
			ml_line_set_continued_to_next( line) ;

			beg = count ;
			cols = 0 ;
			
			edit->cursor.char_index = edit->cursor.col = 0 ;
			if( edit->cursor.row + 1 > edit->scroll_region_end)
			{
				if( ! ml_edsl_scroll_upward( edit , 1))
				{
				#ifdef  DEBUG
					kik_warn_printf( KIK_DEBUG_TAG
						" ml_edsl_scroll_upward failed.\n") ;
				#endif
				}

				/*
				 * If edit->cursor.row == edit->scroll_region_end in this situation,
				 * scroll_region_beg == scroll_region_end.
				 */
				if( edit->cursor.row + 1 <= edit->scroll_region_end)
				{
					edit->cursor.row ++ ;
				}
			}
			else
			{
				ml_model_assure_boundary( &edit->model , ++ edit->cursor.row) ;
			}

			line = CURSOR_LINE(edit) ;
		}
		else
		{
			cols += ml_char_cols( &buffer[count]) ;

			if( ++ count >= filled_len)
			{
				if( edit->cursor.col + cols >= edit->model.num_of_cols &&
					edit->wraparound_ready_line != line)
				{
					edit->wraparound_ready_line = line ;
				}
				else
				{
					reset_wraparound_checker( edit) ;
				}
				
				ml_line_overwrite( line , edit->cursor.char_index , &buffer[beg] ,
					count - beg , cols) ;
				ml_line_unset_continued_to_next( line) ;

				cursor_goto_by_char_index( edit ,
					edit->cursor.char_index + count - beg ,
					edit->cursor.row , BREAK_BOUNDARY) ;

				break ;
			}
		}
	}

	ml_str_final( buffer , buf_len) ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif
	
	return  1 ;
}

/*
 * deleting chars within a line.
 */
int
ml_edit_delete_cols(
	ml_edit_t *  edit ,
	u_int  delete_cols
	)
{
	int  char_index ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	ml_line_t *  cursor_line ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	reset_wraparound_checker( edit) ;

	cursor_line = CURSOR_LINE(edit) ;

	if( edit->cursor.row == ml_model_end_row( &edit->model) &&
		edit->cursor.char_index == ml_line_end_char_index( cursor_line))
	{
		/* if you are in the end of edit , nothing is deleted. */
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" deletion is requested in the end of edit(%d,%d) , nothing is deleted.\n" ,
			edit->cursor.char_index , edit->cursor.row) ;
	#endif
		
		return  1 ;
	}
	
	/*
	 * collecting chars after cursor line.
	 */
	 
	buf_len = cursor_line->num_of_filled_chars ;

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/* before cursor(including cursor) */

	if( edit->cursor.col_in_char)
	{
		int  cols_after ;
		int  count ;

	#ifdef  DEBUG
		if( ml_char_cols( CURSOR_CHAR(edit)) <= edit->cursor.col_in_char)
		{
			kik_warn_printf( KIK_DEBUG_TAG " illegal col_in_char.\n") ;
		}
	#endif
	
		cols_after = ml_char_cols( CURSOR_CHAR(edit)) - edit->cursor.col_in_char ;
		
		/*
		 * padding spaces before cursor.
		 */
		for( count = 0 ; count < edit->cursor.col_in_char ; count ++)
		{
			ml_char_copy( &buffer[filled_len ++] , ml_sp_ch()) ;
		}

		if( delete_cols >= cols_after)
		{
			delete_cols -= cols_after ;
		}
		else
		{
			delete_cols = 0 ;

			/*
			 * padding spaces after cursor.
			 */
			for( count = 0 ; count < cols_after - delete_cols ; count ++)
			{
				ml_char_copy( &buffer[filled_len ++] , ml_sp_ch()) ;
			}
		}
		
		char_index = edit->cursor.char_index + 1 ;
	}
	else
	{
		char_index = edit->cursor.char_index ;
	}

	/* after cursor(excluding cursor) + delete_cols */

	if( delete_cols)
	{
		u_int  cols ;
		
		cols = ml_char_cols( &cursor_line->chars[char_index ++]) ;
		while( cols < delete_cols && char_index <= ml_line_end_char_index( cursor_line))
		{
			cols += ml_char_cols( &cursor_line->chars[char_index ++]) ;
		}
	}

	ml_str_copy( &buffer[filled_len] , &cursor_line->chars[char_index] ,
		cursor_line->num_of_filled_chars - char_index) ;
	filled_len += (cursor_line->num_of_filled_chars - char_index) ;

	if( filled_len > 0)
	{
		/*
		 * overwriting.
		 */

		ml_line_clear( cursor_line , edit->cursor.char_index) ;
		ml_line_overwrite( cursor_line , edit->cursor.char_index ,
			buffer , filled_len , ml_str_cols( buffer , filled_len)) ;
	}
	else
	{
		ml_line_clear( cursor_line , 0) ;
	}

	ml_str_final( buffer , buf_len) ;

	if( edit->cursor.col_in_char)
	{
		cursor_goto_by_char_index( edit , edit->cursor.char_index + edit->cursor.col_in_char ,
			edit->cursor.row , BREAK_BOUNDARY) ;
	}

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> [cursor(index)%d (col)%d (row)%d]\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif
	
	return  1 ;
}

int
ml_edit_delete_cols_bce(
	ml_edit_t *  edit ,
	u_int  delete_cols
	)
{
	ml_line_t *  cursor_line ;
	int  char_index ;
	int  col ;
	
	if( ! ml_edit_delete_cols( edit , delete_cols))
	{
		return  0 ;
	}

	cursor_line = CURSOR_LINE(edit) ;
	
	char_index = ml_line_end_char_index( cursor_line) + 1 ;

	for( col = ml_line_get_num_of_filled_cols( cursor_line) ; col < edit->model.num_of_cols ; col ++)
	{
		ml_char_copy( &cursor_line->chars[char_index++] , &edit->bce_ch) ;
	}

	cursor_line->num_of_filled_chars = char_index ;

	ml_line_set_modified( cursor_line , edit->cursor.char_index ,
		ml_line_end_char_index( cursor_line)) ;
	
	return  1 ;
}

int
ml_edit_clear_cols(
	ml_edit_t *  edit ,
	u_int  cols
	)
{
	reset_wraparound_checker( edit) ;
	
	return  clear_cols( edit , cols , 0) ;
}

int
ml_edit_clear_cols_bce(
	ml_edit_t *  edit ,
	u_int  cols
	)
{
	reset_wraparound_checker( edit) ;
	
	return  clear_cols( edit , cols , 1) ;
}

int
ml_edit_insert_new_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;
	
	return  ml_edsl_insert_new_line( edit) ;
}

int
ml_edit_delete_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;
	
	return  ml_edsl_delete_line( edit) ;
}

int
ml_edit_clear_line_to_right(
	ml_edit_t *  edit
	)
{
	ml_line_t *  cursor_line ;

	reset_wraparound_checker( edit) ;

	cursor_line = CURSOR_LINE(edit) ;

	if( edit->cursor.col_in_char)
	{
		int  count ;

		if( edit->cursor.char_index + edit->cursor.col_in_char >= cursor_line->num_of_filled_chars)
		{
			u_int  size ;
			u_int  actual_size ;

			size = edit->cursor.char_index + edit->cursor.col_in_char -
				cursor_line->num_of_filled_chars + 1 ;

			actual_size = ml_line_break_boundary( cursor_line , size) ;
			if( actual_size < size)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_line_break_boundary() failed.\n") ;
			#endif
			
				if( size - actual_size > edit->cursor.col_in_char)
				{
					edit->cursor.col_in_char = 0 ;
				}
				else
				{
					edit->cursor.col_in_char -= (size - actual_size) ;
				}
			}
		}

		/*
		 * padding spaces.
		 *
		 * if edit->cursor.col_in_char is over 1 , the next char of cursor is
		 * overwritten(cleared) by sp_ch, but in any event it will be cleared
		 * in ml_edit_clear_line() , so it doesn't do harm.
		 */
		for( count = 0 ; count < edit->cursor.col_in_char ; count ++)
		{
			ml_char_copy( CURSOR_CHAR(edit) , ml_sp_ch()) ;
			edit->cursor.char_index ++ ;
		}
	}
	
	return  ml_edit_clear_line( edit , edit->cursor.row , edit->cursor.char_index) ;
}

int
ml_edit_clear_line_to_right_bce(
	ml_edit_t *  edit
	)
{
	int  cols ;
	int  char_index ;
	ml_line_t *  cursor_line ;

	reset_wraparound_checker( edit) ;

	cursor_line = CURSOR_LINE(edit) ;
	
	if( edit->cursor.col_in_char)
	{
		ml_line_fill( cursor_line , ml_sp_ch() , edit->cursor.char_index ,
			edit->cursor.col_in_char) ;
		edit->cursor.char_index += edit->cursor.col_in_char ;
	}

	char_index = edit->cursor.char_index ;

	for( cols = ml_str_cols( cursor_line->chars , edit->cursor.char_index) ;
		cols < edit->model.num_of_cols ; cols ++)
	{
		ml_char_copy( &cursor_line->chars[char_index++] , &edit->bce_ch) ;
	}

	cursor_line->num_of_filled_chars = char_index ;

	ml_line_set_modified( cursor_line , edit->cursor.char_index ,
		ml_line_end_char_index( cursor_line)) ;

	return  1 ;
}

int
ml_edit_clear_line_to_left(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	return  clear_line_to_left( edit , 0) ;
}

int
ml_edit_clear_line_to_left_bce(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	return  clear_line_to_left( edit , 1) ;
}

int
ml_edit_clear_below(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( ! ml_edit_clear_line_to_right( edit) ||
		! ml_edit_clear_lines( edit , edit->cursor.row + 1 ,
			edit->model.num_of_filled_rows - (edit->cursor.row + 1)))
	{
		return  0 ;
	}

	return  1 ;
}

int
ml_edit_clear_below_bce(
	ml_edit_t *  edit
	)
{
	int  row ;
	
	reset_wraparound_checker( edit) ;

	if( ! ml_edit_clear_line_to_right_bce( edit))
	{
		return  0 ;
	}

	for( row = edit->cursor.row + 1 ; row < edit->model.num_of_rows ; row ++)
	{
		ml_line_fill( ml_model_get_line( &edit->model , row) , &edit->bce_ch , 0 ,
			edit->model.num_of_cols) ;
	}

	edit->model.num_of_filled_rows = edit->model.num_of_rows ;

	return  1 ;
}

int
ml_edit_clear_above(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( ! ml_edit_clear_lines( edit , 0 , edit->cursor.row) ||
		! ml_edit_clear_line_to_left( edit))
	{
		return  0 ;
	}

	return  1 ;
}

int
ml_edit_clear_above_bce(
	ml_edit_t *  edit
	)
{
	int  row ;
	
	reset_wraparound_checker( edit) ;

	if( ! ml_edit_clear_line_to_left_bce( edit))
	{
		return  0 ;
	}

	for( row = 0 ; row < edit->cursor.row ; row ++)
	{
		ml_line_fill( ml_model_get_line( &edit->model , row) , &edit->bce_ch , 0 ,
			edit->model.num_of_cols) ;
	}

	return  1 ;
}

int
ml_edit_fill_all(
	ml_edit_t *  edit ,
	ml_char_t *  ch
	)
{
	int  row ;
	
	for( row = 0 ; row < edit->model.num_of_rows ; row ++)
	{
		ml_line_fill( ml_model_get_line( &edit->model , row) , ch , 0 ,
			edit->model.num_of_cols / ml_char_cols(ch)) ;
	}

	edit->model.num_of_filled_rows = edit->model.num_of_rows ;

	return  1 ;
}

int
ml_edit_set_scroll_region(
	ml_edit_t *  edit ,
	int  beg ,
	int  end
	)
{
	if( 0 > beg)
	{
		beg = 0 ;
	}
	else if( beg >= edit->model.num_of_rows)
	{
		beg = edit->model.num_of_rows - 1 ;
	}

	if( 0 > end)
	{
		end = 0 ;
	}
	else if( end >= edit->model.num_of_rows)
	{
		end = edit->model.num_of_rows - 1 ;
	}

	if( beg > end)
	{
		/* illegal */
		
		return  0 ;
	}

	edit->scroll_region_beg = beg ;
	edit->scroll_region_end = end ;

	/* cursor position is reset(the same behavior of xterm 4.0.3, kterm 6.2.0 or so) */
	edit->cursor.row = 0 ;
	edit->cursor.col = 0 ;
	edit->cursor.char_index = 0 ;

	return  1 ;
}

int
ml_edit_scroll_upward(
	ml_edit_t *  edit ,
	u_int  size
	)
{
	int  cursor_row ;
	int  cursor_col ;
	
	cursor_row = edit->cursor.row ;
	cursor_col = edit->cursor.col ;
	
	if( ! ml_edsl_scroll_upward( edit , size))
	{
		return  0 ;
	}
	
	cursor_goto_by_col( edit , cursor_col , cursor_row , BREAK_BOUNDARY) ;

	return  1 ;
}

int
ml_edit_scroll_downward(
	ml_edit_t *  edit ,
	u_int  size
	)
{
	int  cursor_row ;
	int  cursor_col ;
	
	cursor_row = edit->cursor.row ;
	cursor_col = edit->cursor.col ;

	if( ! ml_edsl_scroll_downward( edit , size))
	{
		return  0 ;
	}
	
	cursor_goto_by_col( edit , cursor_col , cursor_row , BREAK_BOUNDARY) ;

	return  1 ;
}

int
ml_edit_vertical_forward_tabs(
	ml_edit_t *  edit ,
	u_int  num
	)
{
	return  vertical_tabs( edit , num , 1) ;
}

int
ml_edit_vertical_backward_tabs(
	ml_edit_t *  edit ,
	u_int  num
	)
{
	return  vertical_tabs( edit , num , 0) ;
}

int
ml_edit_set_tab_size(
	ml_edit_t *  edit ,
	u_int  tab_size
	)
{
	int  col ;
	u_int8_t *  tab_stops ;
	
	if( tab_size == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " tab size 0 is not acceptable.\n") ;
	#endif
	
		return  0 ;
	}

	ml_edit_clear_all_tab_stops( edit) ;
	
	col = 0 ;
	tab_stops = edit->tab_stops ;
	
	while( 1)
	{
		if( col % tab_size == 0)
		{
			(*tab_stops) |= ( 1 << (7 - col % 8)) ;
		}

		col ++ ;

		if( col >= edit->model.num_of_cols)
		{
			tab_stops ++ ;

			break ;
		}
		else if( col % 8 == 0)
		{
			tab_stops ++ ;
		}
	}

#ifdef  __DEBUG
	{
		int  i ;

		kik_debug_printf( KIK_DEBUG_TAG " tab stops =>\n") ;

		for( i = 0 ; i < TAB_STOPS_SIZE(edit) ; i ++)
		{
			kik_msg_printf( "%x " , edit->tab_stops[i]) ;
		}
		kik_msg_printf( "\n") ;
	}
#endif
	
	edit->tab_size = tab_size ;

	return  1 ;
}

int
ml_edit_set_tab_stop(
	ml_edit_t *  edit
	)
{
	edit->tab_stops[ edit->cursor.col / 8] |= (1 << (7 - edit->cursor.col % 8)) ;
	
	return  1 ;
}

int
ml_edit_clear_tab_stop(
	ml_edit_t *  edit
	)
{
	edit->tab_stops[ edit->cursor.col / 8] &= ~(1 << (7 - edit->cursor.col % 8)) ;
	
	return  1 ;
}

int
ml_edit_clear_all_tab_stops(
	ml_edit_t *  edit
	)
{
	memset( edit->tab_stops , 0 , TAB_STOPS_SIZE(edit)) ;
	
	return  1 ;
}

ml_line_t *
ml_edit_get_line(
	ml_edit_t *  edit ,
	int  row
	)
{
	return  ml_model_get_line( &edit->model , row) ;
}

int
ml_edit_set_modified_all(
	ml_edit_t *  edit
	)
{
	int  count ;

	for( count = 0 ; count < edit->model.num_of_rows ; count ++)
	{
		ml_line_set_modified_all( ml_model_get_line( &edit->model , count)) ;
	}

	return  1 ;
}

u_int
ml_edit_get_cols(
	ml_edit_t *  edit
	)
{
	return  edit->model.num_of_cols ;
}

u_int
ml_edit_get_rows(
	ml_edit_t *  edit
	)
{
	return  edit->model.num_of_rows ;
}

int
ml_cursor_is_beg_of_line(
	ml_edit_t *  edit
	)
{
	return  (edit->cursor.row == 0) ;
}

int
ml_edit_goto_beg_of_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;
	
	edit->cursor.char_index = 0 ;
	edit->cursor.col = 0 ;

	return  1 ;
}

int
ml_edit_goto_home(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;
	
	edit->cursor.char_index = 0 ;
	edit->cursor.col = 0 ;
	edit->cursor.row = 0 ;

	return  1 ;
}

int
ml_edit_goto_end(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;
	
	edit->cursor.row = ml_model_end_row( &edit->model) ;
	edit->cursor.char_index = 0 ;
	edit->cursor.col = 0 ;

	return  1 ;
}

int
ml_edit_go_forward(
	ml_edit_t *  edit ,
	int  flag		/* WARPAROUND | SCROLL | BREAK_BOUNDARY */
	)
{
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going forward from char index %d col %d row %d, then ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	reset_wraparound_checker( edit) ;

	/*
	 * full width char check.
	 */

	if( edit->cursor.col_in_char + 1 < ml_char_cols( CURSOR_CHAR(edit)))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif

		edit->cursor.col ++ ;
		edit->cursor.col_in_char ++ ;

		return  1 ;
	}

	/*
	 * moving forward.
	 */
	 
	if( edit->cursor.char_index == ml_line_end_char_index( CURSOR_LINE(edit)))
	{
		if( ml_line_get_num_of_filled_cols( CURSOR_LINE(edit)) < edit->model.num_of_cols &&
			flag & BREAK_BOUNDARY)
		{
			if( ml_line_break_boundary( CURSOR_LINE(edit) , 1))
			{
				edit->cursor.char_index ++ ;

				goto  end ;
			}
		}
		
		if( ! ( flag & WRAPAROUND))
		{
			return  0 ;
		}

		if( ml_is_scroll_lowerlimit( edit , edit->cursor.row))
		{
			if( ! ( flag & SCROLL))
			{
				return  0 ;
			}

			if( ! ml_edsl_scroll_upward( edit , 1))
			{
				return  0 ;
			}
		}
		
		if( edit->cursor.row == ml_model_end_row( &edit->model))
		{
			if( ! ( flag & BREAK_BOUNDARY))
			{
				return  0 ;
			}

			if( ! ml_model_break_boundary( &edit->model , 1))
			{
				return  0 ;
			}
		}
		
		edit->cursor.row ++ ;
		edit->cursor.char_index = 0 ;
	}
	else
	{
		edit->cursor.char_index ++ ;
	}

end:
	edit->cursor.col =
		ml_convert_char_index_to_col( CURSOR_LINE(edit) , edit->cursor.char_index , 0) ;
	edit->cursor.col_in_char = 0 ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> char index %d col %d row %d\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	return  1 ;
}

int
ml_edit_go_back(
	ml_edit_t *  edit ,
	int  flag		/* WRAPAROUND | SCROLL */
	)
{
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going back from char index %d col %d row %d ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	if( edit->wraparound_ready_line)
	{
		reset_wraparound_checker( edit) ;

		return  1 ;
	}
	else
	{
		reset_wraparound_checker( edit) ;
	}
	
	/*
	 * full width char check.
	 */
	 
	if( edit->cursor.col_in_char)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif
	
		edit->cursor.col -- ;
		edit->cursor.col_in_char -- ;

		return  1 ;
	}

	/*
	 * moving backward.
	 */
	 
	if( edit->cursor.char_index == 0)
	{
		if( ! ( flag & WRAPAROUND))
		{
			return  0 ;
		}

		if( ml_is_scroll_upperlimit( edit , edit->cursor.row))
		{
			if( ! ( flag & SCROLL))
			{
				return  0 ;
			}

			if( ! ml_edsl_scroll_downward( edit , 1))
			{
				return  0 ;
			}
		}
		
		if( edit->cursor.row == 0)
		{
			return  0 ;
		}
		
		edit->cursor.row -- ;
		edit->cursor.char_index = ml_line_end_char_index( CURSOR_LINE( edit)) ;
	}
	else
	{
		edit->cursor.char_index -- ;
	}

	edit->cursor.col_in_char = ml_char_cols( CURSOR_CHAR(edit)) - 1 ;
	edit->cursor.col =
		ml_convert_char_index_to_col( CURSOR_LINE(edit) , edit->cursor.char_index , 0)
		+ edit->cursor.col_in_char ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( " to char index %d col %d row %d\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	return  1 ;
}

int
ml_edit_go_upward(
	ml_edit_t *  edit ,
	int  flag		/* SCROLL | BREAK_BOUNDARY */
	)
{
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going upward from char index %d(col %d) ->" ,
		edit->cursor.char_index , edit->cursor.col) ;
#endif
	
	reset_wraparound_checker( edit) ;
	
	if( ml_is_scroll_upperlimit( edit , edit->cursor.row))
	{
		if( ! ( flag & SCROLL) || ! ( flag & BREAK_BOUNDARY))
		{
			return  0 ;
		}

		if( ! ml_edit_scroll_downward( edit , 1))
		{
			return  0 ;
		}
	}
	else
	{
		if( ! cursor_goto_by_col( edit , edit->cursor.col , edit->cursor.row - 1 ,
			flag & BREAK_BOUNDARY))
		{
			return  0 ;
		}
	}
	
#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> upward to char index %d (col %d)\n" ,
		edit->cursor.char_index , edit->cursor.col) ;
#endif
	
	return  1 ;
}

int
ml_edit_go_downward(
	ml_edit_t *  edit ,
	int  flag		/* SCROLL | BREAK_BOUNDARY */
	)
{
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going downward from char index %d col %d row %d ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif
	
	reset_wraparound_checker( edit) ;
	
	if( ml_is_scroll_lowerlimit( edit , edit->cursor.row))
	{
		if( ! ( flag & (SCROLL)) || ! ( flag & BREAK_BOUNDARY))
		{
		#ifdef  DEBUG
			kik_warn_printf(
				KIK_DEBUG_TAG " cursor cannot go downward(reaches scroll lower limit).\n") ;
		#endif

			return  0 ;
		}

		if( ! ml_edit_scroll_upward( edit , 1))
		{
			return  0 ;
		}
	}
	else
	{
		if( ! cursor_goto_by_col( edit , edit->cursor.col , edit->cursor.row + 1 ,
			flag & BREAK_BOUNDARY))
		{
			return  0 ;
		}
	}

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> downward to char_index %d col %d row %d\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif
	
	return  1 ;
}

int
ml_edit_goto(
	ml_edit_t *  edit ,
	int  col ,
	int  row ,
	int  flag
	)
{
	reset_wraparound_checker( edit) ;

	if( row < 0 || edit->model.num_of_rows <= row)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " row %d is over num_of_rows %d" ,
			row , edit->model.num_of_rows) ;
	#endif

		row = edit->model.num_of_rows - 1 ;

	#ifdef  DEBUG
		kik_msg_printf( " ... modified to %d.\n" , row) ;
	#endif
	}
	
	if( col < 0 || edit->model.num_of_cols <= col)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " col %d is over num_of_cols %d" ,
			col , edit->model.num_of_cols) ;
	#endif

		col = edit->model.num_of_cols - 1 ;

	#ifdef  DEBUG
		kik_msg_printf( " ... modified to %d.\n" , col) ;
	#endif
	}

	if( ! cursor_goto_by_col( edit , col , row , flag))
	{
		return  0 ;
	}

	edit->cursor.col = col ;

	return  1 ;
}

int
ml_cursor_save(
	ml_edit_t *  edit
	)
{
	edit->cursor.saved_col = edit->cursor.col ;
	edit->cursor.saved_char_index = edit->cursor.char_index ;
	edit->cursor.saved_row = edit->cursor.row ;
	edit->cursor.is_saved = 1 ;

	return  1 ;
}

int
ml_cursor_restore(
	ml_edit_t *  edit
	)
{
	if( ! edit->cursor.is_saved)
	{
		return  0 ;
	}
	
	if( cursor_goto_by_col( edit , edit->cursor.saved_col , edit->cursor.saved_row ,
		BREAK_BOUNDARY) == 0)
	{
		return  0 ;
	}
	
	edit->cursor.is_saved = 0 ;

	return  1 ;
}

int
ml_highlight_cursor(
	ml_edit_t *  edit
	)
{
	if( edit->cursor.is_highlighted)
	{
		/* already highlighted */
		
		return  0 ;
	}

	edit->cursor.orig_fg = ml_char_fg_color( CURSOR_CHAR(edit)) ;
	ml_char_set_fg_color( CURSOR_CHAR(edit) , ML_BG_COLOR) ;
	
	edit->cursor.orig_bg = ml_char_bg_color( CURSOR_CHAR(edit)) ;
	ml_char_set_bg_color( CURSOR_CHAR(edit) , ML_FG_COLOR) ;

	edit->cursor.is_highlighted = 1 ;

	return  1 ;
}

int
ml_unhighlight_cursor(
	ml_edit_t *  edit
	)
{
	if( ! edit->cursor.is_highlighted)
	{
		/* already highlighted */
		
		return  0 ;
	}
	
	ml_char_set_fg_color( CURSOR_CHAR(edit) , edit->cursor.orig_fg) ;
	ml_char_set_bg_color( CURSOR_CHAR(edit) , edit->cursor.orig_bg) ;

	edit->cursor.is_highlighted = 0 ;
	
	return  1 ;
}

int
ml_cursor_char_index(
	ml_edit_t *  edit
	)
{
	return  edit->cursor.char_index ;
}

int
ml_cursor_col(
	ml_edit_t *  edit
	)
{
	return  edit->cursor.col ;
}

int
ml_cursor_row(
	ml_edit_t *  edit
	)
{
	return  edit->cursor.row ;
}

/*
 * for debugging.
 */

#ifdef  DEBUG

void
ml_edit_dump(
	ml_edit_t *  edit
	)
{
	int  row ;
	ml_line_t *  line ;
	
	kik_debug_printf( KIK_DEBUG_TAG
		" ===> dumping edit...[(filled rows)%d] [cursor(index)%d (col)%d (row)%d]\n" ,
		edit->model.num_of_filled_rows , edit->cursor.char_index , edit->cursor.col ,
		edit->cursor.row) ;

	for( row = 0 ; row < edit->model.num_of_rows ; row++)
	{
		int  char_index ;

		line = ml_model_get_line( &edit->model , row) ;

		if( ml_line_is_modified( line))
		{
			kik_msg_printf( "!%.2d-%.2d" ,
				ml_line_get_beg_of_modified( line) ,
				ml_line_get_end_of_modified( line)) ;
		}
		else
		{
			kik_msg_printf( "      ") ;
		}
		
		kik_msg_printf( "[%.2d %.2d]" , line->num_of_filled_chars ,
			ml_line_get_num_of_filled_cols( line)) ;
			
		if( line->num_of_filled_chars > 0)
		{
			for( char_index = 0 ; char_index < line->num_of_filled_chars ;
				char_index ++)
			{
				if( edit->cursor.row == row && edit->cursor.char_index == char_index)
				{
					kik_msg_printf( "**") ;
				}

				ml_char_dump( &line->chars[char_index]) ;

				if( edit->cursor.row == row && edit->cursor.char_index == char_index)
				{
					kik_msg_printf( "**") ;
				}
			}
		}
		
		kik_msg_printf( "\n") ;
	}

	kik_debug_printf( KIK_DEBUG_TAG " <=== end of edit.\n\n") ;
}

void
ml_edit_dump_updated(
	ml_edit_t *  edit
	)
{
	int  row ;

	for( row = 0 ; row < edit->model.num_of_filled_rows ; row ++)
	{
		kik_msg_printf( "(%.2d)%d " ,
			row , ml_line_is_modified( ml_model_get_line( &edit->model , row))) ;
	}

	kik_msg_printf( "\n") ;
}

#endif
