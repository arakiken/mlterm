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
	ml_char_init( &edit->prev_recv_ch) ;

	return ;
}

/*
 * if len is over num_of_filled_chars of end line, *char_index points over it.
 */
static ml_line_t *
get_pos(
	ml_edit_t *  edit ,
	int *  row ,
	int *  char_index ,
	int  start_row ,
	int  start_char_index ,
	int  end_row ,
	u_int  len
	)
{
	ml_line_t *  line ;

	*char_index = start_char_index + len ;

	for( *row = start_row ; *row < end_row ; (*row) ++)
	{
		line = ml_model_get_line( &edit->model , *row) ;
		
		if( *char_index < line->num_of_filled_chars)
		{
			return  NULL ;
		}

		*char_index -= line->num_of_filled_chars ;
	}

	line = ml_model_get_line( &edit->model , end_row) ;

	if( ml_line_get_num_of_filled_cols( line) == edit->model.num_of_cols &&
		*char_index > ml_line_end_char_index( line))
	{
		*char_index = ml_line_end_char_index( line) ;
		
		return  line ;
	}

	return  NULL ;
}

static int
render_chars(
	ml_edit_t *  edit ,
	int  start_row ,
	ml_char_t *  chars ,
	u_int  num_of_chars
	)
{
	int  bol ;		/* index of the beginning of line */
	u_int  row ;
	u_int  line_cols ;
	int  count ;
	u_int  scroll_size ;

	if( start_row <= edit->scroll_region_end)
	{
		ml_render_hint_change_size( &edit->render_hint ,
			edit->scroll_region_end - edit->scroll_region_beg + 1) ;
	}
	else
	{
		ml_render_hint_change_size( &edit->render_hint ,
			edit->model.num_of_rows - edit->scroll_region_end - 1) ;
	}
	
	ml_render_hint_reset( &edit->render_hint) ;
	
	count = 0 ;
	bol = 0 ;
	row = start_row ;
	scroll_size = 0 ;
	line_cols = 0 ;	
	while( 1)
	{
		if( count > bol &&
			line_cols + ml_char_cols( &chars[count]) > edit->model.num_of_cols)
		{
			/*
			 * line length is 'count - bol' for the character at 'count'
			 * position to be excluded.
			 */
			ml_render_hint_add( &edit->render_hint , bol , count - bol , line_cols) ;

			if( row ++ >= edit->scroll_region_end)
			{
				scroll_size ++ ;
			}
			
			bol = count ;
			line_cols = 0 ;
		}
		else
		{
			line_cols += ml_char_cols( &chars[count]) ;

			if( ++ count >= num_of_chars)
			{
				ml_render_hint_add( &edit->render_hint , bol , count - bol , line_cols) ;

				if( scroll_size)
				{
					if( ! ml_edsl_scroll_upward( edit , scroll_size))
					{
					#ifdef  DEBUG
						kik_warn_printf( KIK_DEBUG_TAG
							" ml_edsl_scroll_upward failed.\n") ;
					#endif
					}
				}

				return  1 ;
			}
		}
	}
}

static int
overwrite_lines(
	ml_edit_t *  edit ,
	int *  cursor_index ,
	ml_char_t *  chars ,
	u_int  num_of_chars
	)
{
	int  count ;
	int  current_row ;
	int  beg_char_index ;
	u_int  num_of_lines ;
	ml_line_t *  line ;
	int  beg_of_line ;
	u_int  len ;
	u_int  cols ;
	
	if( ! render_chars( edit , edit->cursor.row , chars , num_of_chars))
	{
		return  0 ;
	}

	num_of_lines = ml_get_num_of_lines_by_hints( &edit->render_hint) ;

#ifdef  DEBUG
	if( num_of_lines > edit->model.num_of_rows)
	{
		kik_warn_printf( KIK_DEBUG_TAG
			" ml_get_num_of_lines_by_hints() returns too large value.\n") ;
		
		num_of_lines = edit->model.num_of_rows ;
	}
#endif

	count = 0 ;
	current_row = edit->cursor.row ;

	/* all changes should happen after cursor */
	beg_char_index = edit->cursor.char_index ;

	while( ml_render_hint_at( &edit->render_hint , &beg_of_line , &len , &cols , count))
	{
		if( count == 0)
		{
			/*
			 * adjusting cursor_index
			 * excluding scrolled out characters.
			 */
			*cursor_index -= beg_of_line ;
		}

		line = ml_model_get_line( &edit->model , current_row) ;
		
		if( ++ count == num_of_lines)
		{
			ml_line_overwrite( line , beg_char_index , &chars[beg_of_line] , len , cols) ;
			ml_line_unset_continued_to_next( line) ;

			break ;
		}
		else
		{
			ml_line_overwrite_all( line , beg_char_index ,
				&chars[beg_of_line] , len , cols) ;
			ml_line_set_continued_to_next( line) ;
			
			current_row ++ ;
		}

		beg_char_index = 0 ;
	}

	if( current_row >= edit->model.num_of_filled_rows)
	{
		edit->model.num_of_filled_rows = current_row + 1 ;
	}

	return  1 ;
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
	
	edit->cursor.col =
		ml_convert_char_index_to_col( CURSOR_LINE(edit) , edit->cursor.char_index , 0)
		+ cols_rest ;

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
	int  cursor_index ;
	int  new_char_index ;
	int  new_row ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	u_int  filled_cols ;
	int  cols_rest ;
	int  cols_after ;
	int  count ;
	ml_line_t *  wraparound ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

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
	 
	if( edit->cursor.char_index > 0)
	{
		ml_str_copy( &buffer[filled_len] , CURSOR_LINE(edit)->chars , edit->cursor.char_index) ;
		filled_len += edit->cursor.char_index ;
	}

	if( edit->wraparound_ready_line)
	{
		ml_char_copy( &buffer[filled_len++] , &edit->prev_recv_ch) ;
	}
	
	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;
	if( cols_rest)
	{
	#ifdef  DEBUG
		if( ml_char_cols( CURSOR_CHAR(edit)) <= cols_rest)
		{
			kik_warn_printf( KIK_DEBUG_TAG " illegal cols_rest.\n") ;
		}
	#endif
	
		/*
		 * padding spaces for former half of cursor.
		 */
		for( count = 0 ; count < cols_rest ; count ++)
		{
			ml_char_copy( &buffer[filled_len ++] , ml_sp_ch()) ;
		}
		
		cols_after = ml_char_cols( CURSOR_CHAR(edit)) - cols_rest ;
	}
	else
	{
		cols_after = 0 ;
	}

	if( ! do_move_cursor)
	{
		cursor_index = filled_len ;
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

	if( do_move_cursor)
	{
		cursor_index = filled_len ;
	}
	
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
			count < CURSOR_LINE(edit)->num_of_filled_chars ;
			count ++)
		{
			if( filled_cols + ml_char_cols( &CURSOR_LINE(edit)->chars[count]) >
				edit->model.num_of_cols)
			{
				break ;
			}

			ml_char_copy( &buffer[filled_len ++] , &CURSOR_LINE(edit)->chars[count]) ;
			filled_cols += ml_char_cols( &CURSOR_LINE(edit)->chars[count]) ;
		}
	}

line_full:
	
	/*
	 * overwriting.
	 */

	ml_line_overwrite_all( CURSOR_LINE(edit) , edit->cursor.char_index , buffer ,
		filled_len , filled_cols) ;

	ml_str_final( buffer , buf_len) ;

	reset_wraparound_checker( edit) ;

	/* wraparound_ready_line member is set in this function */
	if( ( wraparound = get_pos( edit , &new_row , &new_char_index , edit->cursor.row , 0 ,
				edit->cursor.row , cursor_index)))
	{
		/* for wraparound line checking */
		if( num_of_ins_chars == 1)
		{
			edit->wraparound_ready_line = wraparound ;
			ml_char_copy( &edit->prev_recv_ch , ins_chars) ;
		}
	}
	
	cursor_goto_by_char_index( edit , new_char_index , new_row , BREAK_BOUNDARY) ;

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
	ml_line_t *  line ;
	
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

	line = CURSOR_LINE(edit) ;

	if( use_bce)
	{
		ml_line_fill( line , &edit->bce_ch , edit->cursor.char_index , cols) ;
	}
	else
	{
		ml_line_fill( line , ml_sp_ch() , edit->cursor.char_index , cols) ;
	}

	return  1 ;
}

static int
clear_line_to_left(
	ml_edit_t *  edit ,
	int  use_bce
	)
{
	ml_char_t *  sp_ch ;
	int  count ;
	u_int  num_of_sp ;
	ml_char_t *  buf ;
	u_int  buf_size ;

	reset_wraparound_checker( edit) ;

	if( use_bce)
	{
		sp_ch = &edit->bce_ch ;
	}
	else
	{
		sp_ch = ml_sp_ch() ;
	}

	/* XXX cursor char will be cleared , is this ok? */
	buf_size = CURSOR_LINE(edit)->num_of_filled_chars - (edit->cursor.char_index + 1) ;
	if( buf_size > 0)
	{
		u_int  padding ;
		u_int  filled ;
		
		if( ml_char_cols( CURSOR_CHAR(edit)) > 1)
		{
			ml_convert_col_to_char_index( CURSOR_LINE(edit) , &padding ,
				edit->cursor.col , 0) ;
			buf_size += padding ;
		}
		else
		{
			padding = 0 ;
		}

		if( ( buf = ml_str_new( buf_size)) == NULL)
		{
			return  0 ;
		}

		filled = 0 ;

		if( padding)
		{
			/*
			 * padding spaces.
			 */
			for( count = 0 ; count < padding ; count ++)
			{
				ml_char_copy( &buf[filled++] , sp_ch) ;
			}
		}

		/* excluding cursor char */
		ml_str_copy( &buf[filled] , CURSOR_CHAR(edit) + 1 , buf_size - padding) ;
	}

	num_of_sp = edit->cursor.col / ml_char_cols( sp_ch) ;
	edit->cursor.col = 0 ;
	for( count = 0 ; count < num_of_sp ; count ++)
	{
		ml_char_copy( &CURSOR_LINE(edit)->chars[count] , sp_ch) ;
		edit->cursor.col += ml_char_cols( sp_ch) ;
	}

	edit->cursor.char_index = count ;

	/* char on cursor */
	ml_char_copy( &CURSOR_LINE(edit)->chars[count++] , sp_ch) ;

	if( buf_size > 0)
	{
		ml_str_copy( &CURSOR_LINE(edit)->chars[count] , buf , buf_size) ;
		
		CURSOR_LINE(edit)->num_of_filled_chars = count + buf_size ;

		ml_str_delete( buf , buf_size) ;
	}

	ml_line_set_modified( CURSOR_LINE(edit) , 0 , edit->cursor.char_index , 0) ;

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
	edit->cursor.orig_fg = ML_BG_COLOR ;
	edit->cursor.orig_bg = ML_FG_COLOR ;
	edit->cursor.is_highlighted = 0 ;
	edit->cursor.saved_row = 0 ;
	edit->cursor.saved_char_index = 0 ;
	edit->cursor.saved_col = 0 ;
	edit->cursor.is_saved = 0 ;

	edit->is_logging = is_logging ;

	reset_wraparound_checker( edit) ;
	
	edit->scroll_region_beg = 0 ;
	edit->scroll_region_end = edit->model.num_of_rows - 1 ;
	edit->scroll_listener = scroll_listener ;

	if( ( edit->tab_stops = malloc( TAB_STOPS_SIZE(edit))) == NULL)
	{
		return  0 ;
	}
	ml_edit_set_tab_size( edit , tab_size) ;

	ml_render_hint_init( &edit->render_hint , edit->model.num_of_rows) ;

	return  1 ;
}

int
ml_edit_final(
	ml_edit_t *  edit
	)
{
	ml_model_final( &edit->model) ;

	ml_render_hint_final( &edit->render_hint) ;

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

	ml_render_hint_change_size( &edit->render_hint , edit->model.num_of_rows) ;

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
	int  cursor_index ;
	int  new_char_index ;
	int  new_row ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	int  cols_rest ;
	ml_line_t *  wraparound ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	buf_len = num_of_ow_chars + edit->model.num_of_cols ;

	if( edit->wraparound_ready_line)
	{
		buf_len ++ ;
	}
	
	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/* before cursor(excluding cursor) */
	if( edit->cursor.char_index > 0)
	{
		ml_str_copy( &buffer[filled_len] , CURSOR_LINE(edit)->chars , edit->cursor.char_index) ;
		filled_len += edit->cursor.char_index ;
	}
	
	if( edit->wraparound_ready_line)
	{
		ml_char_copy( &buffer[filled_len++] , &edit->prev_recv_ch) ;
	}
	
	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;
	if( cols_rest)
	{
		int  count ;

		/*
		 * padding spaces before cursor.
		 */
		for( count = 0 ; count < cols_rest ; count ++)
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

	/* overwriting lines with scrolling */
	overwrite_lines( edit , &cursor_index , buffer , filled_len) ;

	ml_str_final( buffer , buf_len) ;

	reset_wraparound_checker( edit) ;

	/* wraparound_ready_line member is set in this function */
	if( ( wraparound = get_pos( edit , &new_row , &new_char_index , edit->cursor.row , 0 ,
		ml_get_num_of_lines_by_hints( &edit->render_hint) + edit->cursor.row - 1 , cursor_index)))
	{
		/* for wraparound line checking */
		if( num_of_ow_chars == 1)
		{
			edit->wraparound_ready_line = wraparound ;
			ml_char_copy( &edit->prev_recv_ch , ow_chars) ;
		}
	}
	
	cursor_goto_by_char_index( edit , new_char_index , new_row , BREAK_BOUNDARY) ;

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
	int  cursor_char_index ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	u_int  cols ;
	int  cols_rest ;

#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " [cursor(index)%d (col)%d (row)%d] ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	reset_wraparound_checker( edit) ;

	if( edit->cursor.row == ml_model_end_row( &edit->model) &&
		edit->cursor.char_index == ml_line_end_char_index( CURSOR_LINE(edit)))
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
	 
	buf_len = CURSOR_LINE(edit)->num_of_filled_chars ;

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;

	/* before cursor(excluding cursor) */
	if( edit->cursor.char_index > 0)
	{
		ml_str_copy( &buffer[filled_len] , CURSOR_LINE(edit)->chars , edit->cursor.char_index) ;
		filled_len += edit->cursor.char_index ;
	}

	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;
	if( cols_rest)
	{
		int  cols_after ;
		int  count ;

	#ifdef  DEBUG
		if( ml_char_cols( CURSOR_CHAR(edit)) <= cols_rest)
		{
			kik_warn_printf( KIK_DEBUG_TAG " illegal cols_rest.\n") ;
		}
	#endif
	
		cols_after = ml_char_cols( CURSOR_CHAR(edit)) - cols_rest ;
		
		/*
		 * padding spaces before cursor.
		 */
		for( count = 0 ; count < cols_rest ; count ++)
		{
			ml_char_copy( &buffer[filled_len ++] , ml_sp_ch()) ;
		}

		cursor_char_index = filled_len ;

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
	}
	else
	{
		cursor_char_index = filled_len ;
	}

	/* after cursor(excluding cursor) + delete_cols */

	char_index = filled_len ;
	
	if( delete_cols)
	{
		cols = ml_char_cols( &CURSOR_LINE(edit)->chars[char_index++]) ;
		while( cols < delete_cols && char_index <= ml_line_end_char_index( CURSOR_LINE(edit)))
		{
			cols += ml_char_cols( &CURSOR_LINE(edit)->chars[char_index++]) ;
		}
	}

	ml_str_copy( &buffer[filled_len] , &CURSOR_LINE(edit)->chars[char_index] ,
		CURSOR_LINE(edit)->num_of_filled_chars - char_index) ;
	filled_len += (CURSOR_LINE(edit)->num_of_filled_chars - char_index) ;

	if( filled_len > 0)
	{
		/*
		 * overwriting.
		 */

		ml_line_overwrite_all( CURSOR_LINE(edit) , edit->cursor.char_index ,
			buffer , filled_len , ml_str_cols( buffer , filled_len)) ;
	}
	else
	{
		ml_line_clear( CURSOR_LINE(edit) , 0) ;
	}

	ml_str_final( buffer , buf_len) ;

	cursor_goto_by_char_index( edit , cursor_char_index , edit->cursor.row , BREAK_BOUNDARY) ;

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
	ml_line_t *  line ;
	int  char_index ;
	int  col ;
	
	if( ! ml_edit_delete_cols( edit , delete_cols))
	{
		return  0 ;
	}

	line = CURSOR_LINE(edit) ;
	
	char_index = ml_line_end_char_index( line) + 1 ;

	for( col = ml_line_get_num_of_filled_cols( line) ; col < edit->model.num_of_cols ; col ++)
	{
		ml_char_copy( &line->chars[char_index++] , &edit->bce_ch) ;
	}

	line->num_of_filled_chars = char_index ;

	ml_line_set_modified( line , edit->cursor.char_index , ml_line_end_char_index( line) , 0) ;
	
	return  1 ;
}

int
ml_edit_clear_cols(
	ml_edit_t *  edit ,
	u_int  cols
	)
{
	return  clear_cols( edit , cols , 0) ;
}

int
ml_edit_clear_cols_bce(
	ml_edit_t *  edit ,
	u_int  cols
	)
{
	return  clear_cols( edit , cols , 1) ;
}

int
ml_edit_insert_new_line(
	ml_edit_t *  edit
	)
{
	return  ml_edsl_insert_new_line( edit) ;
}

int
ml_edit_delete_line(
	ml_edit_t *  edit
	)
{
	return  ml_edsl_delete_line( edit) ;
}

int
ml_edit_clear_line_to_right(
	ml_edit_t *  edit
	)
{
	int  cols_rest ;

	reset_wraparound_checker( edit) ;

	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;
	if( cols_rest)
	{
		int  count ;

		if( edit->cursor.char_index + cols_rest >= CURSOR_LINE(edit)->num_of_filled_chars)
		{
			u_int  size ;
			u_int  actual_size ;

			size = edit->cursor.char_index + cols_rest -
				CURSOR_LINE(edit)->num_of_filled_chars + 1 ;

			actual_size = ml_line_break_boundary( CURSOR_LINE(edit) , size) ;
			if( actual_size < size)
			{
			#ifdef  DEBUG
				kik_warn_printf( KIK_DEBUG_TAG " ml_line_break_boundary() failed.\n") ;
			#endif
			
				if( size - actual_size > cols_rest)
				{
					cols_rest = 0 ;
				}
				else
				{
					cols_rest -= (size - actual_size) ;
				}
			}
		}

		/*
		 * padding spaces.
		 *
		 * if cols_rest is over 1 , the next char of cursor is overwritten(cleared) by sp_ch ,
		 * but in any event it will be cleared in ml_edit_clear_line() , so it doesn't do harm.
		 */
		for( count = 0 ; count < cols_rest ; count ++)
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
	ml_line_t *  line ;

	reset_wraparound_checker( edit) ;

	char_index = edit->cursor.char_index ;
	line = CURSOR_LINE(edit) ;
	
	for( cols = edit->cursor.col ; cols < edit->model.num_of_cols ; cols ++)
	{
		ml_char_copy( &line->chars[char_index++] , &edit->bce_ch) ;
	}

	line->num_of_filled_chars = char_index ;

	ml_line_set_modified( line , edit->cursor.char_index , ml_line_end_char_index( line) , 0) ;
	
	return  1 ;
}

int
ml_edit_clear_line_to_left(
	ml_edit_t *  edit
	)
{
	return  clear_line_to_left( edit , 0) ;
}

int
ml_edit_clear_line_to_left_bce(
	ml_edit_t *  edit
	)
{
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
			edit->model.num_of_cols / ml_char_cols(&edit->bce_ch)) ;
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

	for( row = 0 ; row <= edit->cursor.row - 1 ; row ++)
	{
		ml_line_fill( ml_model_get_line( &edit->model , row) , &edit->bce_ch , 0 ,
			edit->model.num_of_cols / ml_char_cols(&edit->bce_ch)) ;
	}

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
ml_edit_vertical_tab(
	ml_edit_t *  edit
	)
{
	int  col ;
	
	reset_wraparound_checker( edit) ;

	for( col = edit->cursor.col + 1 ; col < edit->model.num_of_cols - 1 ; col ++)
	{
		if( edit->tab_stops[col / 8] & ( 1 << (7 - col % 8)))
		{
			break ;
		}
	}

	cursor_goto_by_col( edit , col , edit->cursor.row , BREAK_BOUNDARY) ;
	
	return  1 ;
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
ml_cursor_is_beg_of_line(
	ml_edit_t *  edit
	)
{
	return  (edit->cursor.row == 0) ;
}

int
ml_cursor_goto_beg_of_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;
	
	edit->cursor.char_index = 0 ;
	edit->cursor.col = 0 ;

	return  1 ;
}

int
ml_cursor_goto_home(
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
ml_cursor_goto_end(
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
ml_cursor_go_forward(
	ml_edit_t *  edit ,
	int  flag		/* WARPAROUND | SCROLL | BREAK_BOUNDARY */
	)
{
	int  cols_rest ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going forward from char index %d col %d row %d, then ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	reset_wraparound_checker( edit) ;

	/*
	 * full width char check.
	 */

	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest ,
		K_MIN(edit->model.num_of_cols - 1,edit->cursor.col + 1) , flag & BREAK_BOUNDARY) ;
	if( cols_rest)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif

		edit->cursor.col ++ ;

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

#ifdef  CURSOR_DEBUG
	kik_msg_printf( "-> char index %d col %d row %d\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	return  1 ;
}

int
ml_cursor_go_back(
	ml_edit_t *  edit ,
	int  flag		/* WRAPAROUND | SCROLL */
	)
{
	int  cols_rest ;
	
#ifdef  CURSOR_DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " going back from char index %d col %d row %d ->" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	reset_wraparound_checker( edit) ;
	
	/*
	 * full width char check.
	 */
	 
	ml_convert_col_to_char_index( CURSOR_LINE(edit) , &cols_rest , edit->cursor.col , 0) ;
	if( cols_rest)
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif
	
		edit->cursor.col -- ;

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

	edit->cursor.col =
		ml_convert_char_index_to_col( CURSOR_LINE(edit) , edit->cursor.char_index , 0) ;
	edit->cursor.col += (ml_char_cols( CURSOR_CHAR(edit)) - 1) ;

#ifdef  CURSOR_DEBUG
	kik_msg_printf( " to char index %d col %d row %d\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;
#endif

	return  1 ;
}

int
ml_cursor_go_upward(
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
ml_cursor_go_downward(
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
ml_cursor_goto(
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
				line->change_beg_char_index , line->change_end_char_index) ;
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
