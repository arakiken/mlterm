/*
 *	$Id$
 */

#include  "ml_edit.h"

#include  <string.h>		/* memmove/memset */
#include  <kiklib/kik_mem.h>	/* alloca */
#include  <kiklib/kik_debug.h>
#include  <kiklib/kik_util.h>
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#include  "ml_edit_util.h"
#include  "ml_edit_scroll.h"


#if  0
#define  __DEBUG
#endif

#if  0
#define  CURSOR_DEBUG
#endif

#if  0
#define  COMPAT_XTERM
#endif

/*
 * ml_edit_t::tab_stops
 * e.g.)
 * 1 line = 40 columns
 * => tab_stops = u_int8_t * 5 (40bits)
 * => Check tab_stops bits if you want to know whether a column is set tab stop or not.
 */
#define  TAB_STOPS_SIZE(edit)  (((edit)->model.num_of_cols - 1) / 8 + 1)

#define  reset_wraparound_checker(edit)  ((edit)->wraparound_ready_line = NULL)

#define  MARGIN_IS_ENABLED(edit) \
	( (edit)->use_margin && \
	  ( 0 < (edit)->hmargin_beg || (edit)->hmargin_end + 1 < (edit)->model.num_of_cols))

#define  CURSOR_IS_INSIDE_HMARGIN(edit) \
	( (edit)->hmargin_beg <= (edit)->cursor.col && (edit)->cursor.col <= (edit)->hmargin_end)

#define  CURSOR_IS_INSIDE_VMARGIN(edit) \
	( (edit)->vmargin_beg <= (edit)->cursor.row && (edit)->cursor.row <= (edit)->vmargin_end)


/* --- static functions --- */

/*
 * Insert chars within a line.
 * The cursor must be inside the left and right margins. (The caller of this
 * function must check it in advance.)
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
	u_int  num_of_cols ;
	u_int  filled_len ;
	u_int  filled_cols ;
	u_int  last_index ;
	u_int  cols_after ;
	u_int  cols ;
	int  cursor_col ;
	int  count ;
	ml_line_t *  cursor_line ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif

	cursor_line = CURSOR_LINE(edit) ;

	buf_len = edit->model.num_of_cols ;

#ifndef  COMPAT_XTERM
	if( edit->cursor.col > edit->hmargin_end)
	{
		num_of_cols = edit->model.num_of_cols ;
	}
	else
#endif
	{
		num_of_cols = edit->hmargin_end + 1 ;
	}

	if( ( buffer = ml_str_alloca( buf_len)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif
	
		return  0 ;
	}

	filled_len = 0 ;
	filled_cols = 0 ;
	cursor_col = edit->cursor.col ;

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

		filled_cols += count ;
		cols_after = ml_char_cols( CURSOR_CHAR(edit)) - count ;
		cursor_col -= count ;
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

	for( count = 0 ; count < num_of_ins_chars ; count ++)
	{
		cols = ml_char_cols( &ins_chars[count]) ;
		if( cursor_col + filled_cols + cols > num_of_cols)
		{
			break ;
		}

		ml_char_copy( &buffer[filled_len ++] , &ins_chars[count]) ;
		filled_cols += cols ;
	}

	if( edit->cursor.char_index + filled_len == num_of_cols)
	{
		/* cursor position doesn't proceed. */
		last_index = filled_len - 1 ;
	}
	else
	{
		last_index = filled_len ;
	}

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
			/* + 1 is for ml_sp_ch() */
			if( cursor_col + filled_cols + 1 > num_of_cols)
			{
				goto  line_full ;
			}

			ml_char_copy( &buffer[filled_len++] , ml_sp_ch()) ;
		}
		filled_cols += count ;
	}
	else
	{
		cols = ml_char_cols( CURSOR_CHAR(edit)) ;
		if( cursor_col + filled_cols + cols > num_of_cols)
		{
			goto  line_full ;
		}

		ml_char_copy( &buffer[filled_len++] , CURSOR_CHAR(edit)) ;
		filled_cols += cols ;
	}

	/*
	 * after cursor(excluding cursor)
	 */

	for( count = edit->cursor.char_index + 1 ;
		count < cursor_line->num_of_filled_chars ;
		count ++)
	{
		cols = ml_char_cols( ml_char_at( cursor_line , count)) ;
		if( cursor_col + filled_cols + cols > num_of_cols)
		{
			break ;
		}

		ml_char_copy( &buffer[filled_len ++] , ml_char_at( cursor_line , count)) ;
		filled_cols += cols ;
	}

line_full:
	/*
	 * Updating current line and cursor.
	 */

	ml_line_overwrite( cursor_line , edit->cursor.char_index ,
		buffer , filled_len , filled_cols) ;

	ml_str_final( buffer , buf_len) ;

	if( do_move_cursor)
	{
		ml_cursor_moveh_by_char( &edit->cursor , edit->cursor.char_index + last_index) ;
	}
	else if( edit->cursor.col_in_char)
	{
		ml_cursor_moveh_by_char( &edit->cursor ,
			edit->cursor.char_index + edit->cursor.col_in_char) ;
	}

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif

	return  1 ;
}

static int
horizontal_tabs(
	ml_edit_t *  edit ,
	u_int  num ,
	int  is_forward
	)
{
	int  col ;
	u_int  count ;

	if( edit->cursor.col < edit->hmargin_beg)
	{
		ml_cursor_goto_by_col( &edit->cursor , edit->hmargin_beg , edit->cursor.row) ;
	}
	else if( edit->cursor.col > edit->hmargin_end)
	{
		ml_cursor_goto_by_col( &edit->cursor , edit->hmargin_end , edit->cursor.row) ;
	}

	col = edit->cursor.col ;

	for( count = 0 ; count < num ; count ++)
	{
		while( 1)
		{
			if( is_forward)
			{
				if( col >= edit->hmargin_end)
				{
					return  1 ;
				}

				col ++ ;
				ml_edit_go_forward( edit , WRAPAROUND) ;
			}
			else
			{
				if( col <= 0)
				{
					return  1 ;
				}

				col -- ;
				ml_edit_go_back( edit , WRAPAROUND) ;
			}

			if( edit->tab_stops[col / 8] & ( 1 << (7 - col % 8)))
			{
				break ;
			}
		}
	}

	return  1 ;
}

static int
copy_area(
	ml_edit_t *  edit ,
	int  src_col ,
	int  src_row ,
	u_int  num_of_copy_cols ,
	u_int  num_of_copy_rows ,
	int  dst_col ,
	int  dst_row
	)
{
	u_int  count ;
	ml_line_t *  src_line ;
	ml_line_t *  dst_line ;
	int  src_char_index ;
	int  dst_char_index ;
	u_int  src_cols_rest ;
	u_int  src_cols_after ;
	u_int  dst_cols_rest ;
	u_int  num_of_src_chars ;
	u_int  num_of_src_cols ;

	for( count = 0 ; count < num_of_copy_rows ; count++)
	{
		int  srow ;
		int  drow ;

		if( src_row < dst_row)
		{
			srow = src_row + num_of_copy_rows - count - 1 ;
			drow = dst_row + num_of_copy_rows - count - 1 ;
		}
		else
		{
			srow = src_row + count ;
			drow = dst_row + count ;
		}

		if( ! (src_line = ml_edit_get_line( edit , srow)) ||
		    ! (dst_line = ml_edit_get_line( edit , drow)))
		{
			continue ;
		}

		/* Beginning of src line */

		src_char_index = ml_convert_col_to_char_index( src_line ,
					&src_cols_rest , src_col , BREAK_BOUNDARY) ;
		if( src_char_index >= src_line->num_of_filled_chars)
		{
			src_cols_after = num_of_copy_cols ;
		}
		else if( src_cols_rest > 0)
		{
			src_cols_after = ml_char_cols( src_line->chars + src_char_index) -
						src_cols_rest ;
			src_char_index ++ ;
		}
		else
		{
			src_cols_after = 0 ;
		}

		/* Beginning of dst line */

		dst_char_index = ml_convert_col_to_char_index( dst_line ,
					&dst_cols_rest , dst_col , 0) ;

		/* Fill rest at the beginning */

		if( dst_cols_rest + src_cols_after > 0)
		{
			ml_line_fill( dst_line , ml_sp_ch() , dst_char_index ,
				dst_cols_rest + src_cols_after) ;
			if( src_char_index >= src_line->num_of_filled_chars)
			{
				continue ;
			}

			dst_char_index += (dst_cols_rest + src_cols_after) ;
		}

		/* End of src line */

		num_of_src_chars = ml_convert_col_to_char_index( src_line ,
					&src_cols_rest , /* original value is replaced. */
					src_col + num_of_copy_cols - 1 , 0)
					+ 1 - src_char_index ;
		if( src_cols_rest == 0)
		{
			src_cols_rest = ml_char_cols( src_line->chars +
						src_char_index + num_of_src_chars - 1) - 1 ;
			num_of_src_chars -- ;
		}
		else
		{
			src_cols_rest = 0 ;
		}
		num_of_src_cols = num_of_copy_cols - src_cols_after - src_cols_rest ;

		/* Copy src to dst */

		if( num_of_src_cols > 0)
		{
			ml_line_overwrite( dst_line , dst_char_index ,
				src_line->chars + src_char_index ,
				num_of_src_chars , num_of_src_cols) ;
		}

		/* Fill rest at the end */

		if( src_cols_rest > 0)
		{
			ml_line_fill( dst_line , ml_sp_ch() ,
				dst_char_index + num_of_src_chars ,
				src_cols_rest) ;
		}
	}

	return  1 ;
}

static int
erase_area(
	ml_edit_t *  edit ,
	int  col ,
	int  row ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	u_int  count ;
	ml_line_t *  line ;
	int  char_index ;
	u_int  cols_rest ;

	for( count = 0 ; count < num_of_rows ; count++)
	{
		if( ! (line = ml_edit_get_line( edit , row + count)))
		{
			continue ;
		}

		char_index = ml_convert_col_to_char_index( line , &cols_rest , col ,
					BREAK_BOUNDARY) ;

		if( char_index >= line->num_of_filled_chars && ! edit->use_bce)
		{
			continue ;
		}

		if( cols_rest > 0)
		{
			ml_line_fill( line , ml_sp_ch() , char_index , cols_rest) ;
			char_index += cols_rest ;
		}

		ml_line_fill( line , edit->use_bce ? &edit->bce_ch : ml_sp_ch() ,
				char_index , num_of_cols) ;
	}

	return  1 ;
}

static int
scroll_downward_region(
	ml_edit_t *  edit ,
	u_int  size ,
	int  is_cursor_beg
	)
{
	int  vmargin_beg ;

	if( is_cursor_beg)
	{
		if( edit->cursor.row < edit->vmargin_beg)
		{
			return  0 ;
		}

		vmargin_beg = edit->cursor.row ;
	}
	else
	{
		vmargin_beg = edit->vmargin_beg ;
	}

	/*
	 * XXX
	 * CURSOR_IS_INSIDE_HMARGIN(edit) disables vim to scroll the right side of
	 * vertically splitted window.
	 */
	if( /* CURSOR_IS_INSIDE_HMARGIN(edit) && */
	    edit->cursor.row >= vmargin_beg &&
	    edit->cursor.row <= edit->vmargin_end)
	{
		if( size > edit->vmargin_end - vmargin_beg + 1)
		{
			size = edit->vmargin_end - vmargin_beg + 1 ;
		}
		else
		{
			copy_area( edit , edit->hmargin_beg , vmargin_beg ,
				edit->hmargin_end - edit->hmargin_beg + 1 ,
				edit->vmargin_end - vmargin_beg + 1 - size ,
				edit->hmargin_beg , vmargin_beg + size) ;
		}

		erase_area( edit , edit->hmargin_beg , vmargin_beg ,
			edit->hmargin_end - edit->hmargin_beg + 1 , size) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
scroll_upward_region(
	ml_edit_t *  edit ,
	u_int  size ,
	int  is_cursor_beg
	)
{
	int  vmargin_beg ;

	if( is_cursor_beg)
	{
		if( edit->cursor.row < edit->vmargin_beg)
		{
			return  0 ;
		}

		vmargin_beg = edit->cursor.row ;
	}
	else
	{
		vmargin_beg = edit->vmargin_beg ;
	}

	/*
	 * XXX
	 * CURSOR_IS_INSIDE_HMARGIN(edit) disables vim to scroll the right side of
	 * vertical splitted window.
	 */
	if( /* CURSOR_IS_INSIDE_HMARGIN(edit) && */
	    edit->cursor.row >= vmargin_beg &&
	    edit->cursor.row <= edit->vmargin_end)
	{
		if( size > edit->vmargin_end - vmargin_beg + 1)
		{
			size = edit->vmargin_end - vmargin_beg + 1 ;
		}
		else
		{
			copy_area( edit , edit->hmargin_beg , vmargin_beg + size ,
				edit->hmargin_end - edit->hmargin_beg + 1 ,
				edit->vmargin_end - vmargin_beg + 1 - size ,
				edit->hmargin_beg , vmargin_beg) ;
		}

		erase_area( edit , edit->hmargin_beg , edit->vmargin_end + 1 - size ,
			edit->hmargin_end - edit->hmargin_beg + 1 , size) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

static int
apply_relative_origin(
	ml_edit_t *  edit ,
	int *  col ,
	int *  row ,
	u_int *  num_of_cols ,
	u_int *  num_of_rows
	)
{
	if( edit->is_relative_origin)
	{
		if( ( (*row) += edit->vmargin_beg) > edit->vmargin_end ||
		    ( (*col) += edit->hmargin_beg) > edit->hmargin_end)
		{
			return  0 ;
		}

		if( (*row) + (*num_of_rows) > edit->vmargin_end + 1)
		{
			(*num_of_rows) = edit->vmargin_end + 1 - (*row) ;
		}

		if( (*col) + (*num_of_cols) > edit->hmargin_end + 1)
		{
			(*num_of_cols) = edit->hmargin_end + 1 - (*col) ;
		}
	}

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
	int  is_logging ,
	int  use_bce
	)
{
	if( ! ml_model_init( &edit->model , num_of_cols , num_of_rows))
	{
		return  0 ;
	}

	ml_cursor_init( &edit->cursor , &edit->model) ;

	ml_line_assure_boundary( CURSOR_LINE(edit) , 0) ;

	ml_char_init( &edit->bce_ch) ;
	ml_char_copy( &edit->bce_ch , ml_sp_ch()) ;

	edit->use_bce = use_bce ;

	edit->is_logging = is_logging ;

	reset_wraparound_checker( edit) ;

	edit->vmargin_beg = 0 ;
	edit->vmargin_end = ml_model_end_row( &edit->model) ;
	edit->scroll_listener = scroll_listener ;

	edit->use_margin = 0 ;
	edit->hmargin_beg = 0 ;
	edit->hmargin_end = num_of_cols - 1 ;

	if( ( edit->tab_stops = malloc( TAB_STOPS_SIZE(edit))) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc failed.\n") ;
	#endif
	
		return  0 ;
	}

	ml_edit_set_tab_size( edit , tab_size) ;
	
	edit->is_relative_origin = 0 ;
	edit->is_auto_wrap = 1 ;

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
ml_edit_clone(
	ml_edit_t *  dst_edit ,
	ml_edit_t *  src_edit
	)
{
	u_int  row ;
	u_int  num_of_rows ;
	ml_line_t *  src_line ;
	ml_line_t *  dst_line ;

	memcpy( ((char*)dst_edit) + sizeof(ml_model_t) ,
		((char*)src_edit) + sizeof(ml_model_t) ,
		sizeof(ml_edit_t) - sizeof(ml_model_t)) ;

	if( ! ( dst_edit->tab_stops = malloc( TAB_STOPS_SIZE(src_edit))))
	{
		return  0 ;
	}
	memcpy( dst_edit->tab_stops , src_edit->tab_stops , TAB_STOPS_SIZE(src_edit)) ;

	dst_edit->cursor.model = &dst_edit->model ;

	num_of_rows = ml_edit_get_rows( src_edit) ;

	if( ! ml_model_init( &dst_edit->model , ml_edit_get_cols( src_edit) , num_of_rows))
	{
		free( dst_edit->tab_stops) ;

		return  0 ;
	}

	for( row = 0 ; row < num_of_rows ; row++)
	{
		dst_line = ml_edit_get_line( dst_edit , row) ;

		if( ( src_line = ml_edit_get_line( src_edit , row)) ==
		    src_edit->wraparound_ready_line)
		{
			dst_edit->wraparound_ready_line = dst_line ;
		}

		ml_line_copy( dst_line , src_line) ;
	}

	return  1 ;
}

int
ml_edit_resize(
	ml_edit_t *  edit ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	u_int  old_filled_rows ;
	u_int  old_cols ;
	u_int  slide ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif

	if( ( old_filled_rows = ml_model_get_num_of_filled_rows( &edit->model)) > num_of_rows)
	{
		if( edit->is_logging && edit->scroll_listener->receive_scrolled_out_line)
		{
			int  count ;

			for( count = 0 ; count < old_filled_rows - num_of_rows ; count++)
			{
				(*edit->scroll_listener->receive_scrolled_out_line)(
					edit->scroll_listener->self ,
					ml_model_get_line( &edit->model , count)) ;
			}
		}
	}

	old_cols = edit->model.num_of_cols ;

	if( ! ml_model_resize( &edit->model , &slide , num_of_cols , num_of_rows))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " ml_model_resize() failed.\n") ;
	#endif
		
		return  0 ;
	}

	if( slide > edit->cursor.row)
	{
		ml_cursor_goto_home( &edit->cursor) ;
		ml_line_assure_boundary( CURSOR_LINE(edit) , 0) ;
	}
	else
	{
		edit->cursor.row -= slide ;

		if( num_of_cols < old_cols)
		{
			if( edit->cursor.col >= num_of_cols)
			{
				edit->cursor.col = num_of_cols - 1 ;
				edit->cursor.char_index =
					ml_convert_col_to_char_index( CURSOR_LINE(edit) ,
						&edit->cursor.col_in_char , edit->cursor.col , 0) ;
			}
		}
	}

	reset_wraparound_checker( edit) ;
	
	edit->vmargin_beg = 0 ;
	edit->vmargin_end = ml_model_end_row( &edit->model) ;

	edit->use_margin = 0 ;
	edit->hmargin_beg = 0 ;
	edit->hmargin_end = num_of_cols - 1 ;

	free( edit->tab_stops) ;

	if( ( edit->tab_stops = malloc( TAB_STOPS_SIZE(edit))) == NULL)
	{
		return  0 ;
	}

	ml_edit_set_tab_size( edit , edit->tab_size) ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
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

#ifdef  COMPAT_XTERM
	if( ! CURSOR_IS_INSIDE_HMARGIN(edit))
	{
		return  ml_edit_overwrite_chars( edit , ins_chars , num_of_ins_chars) ;
	}
	else
#endif
	{
		return  insert_chars( edit , ins_chars , num_of_ins_chars , 1) ;
	}
}

int
ml_edit_insert_blank_chars(
	ml_edit_t *  edit ,
	u_int  num_of_blank_chars
	)
{
	int  count ;
	ml_char_t *  blank_chars ;
	ml_char_t *  sp_ch ;

	if( ! CURSOR_IS_INSIDE_HMARGIN(edit))
	{
		return  0 ;
	}

	reset_wraparound_checker( edit) ;

	if( ( blank_chars = ml_str_alloca( num_of_blank_chars)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " alloca() failed.\n") ;
	#endif

		return  0 ;
	}

	if( edit->use_bce)
	{
		/*
		 * If bce_ch is not used here, vttest 11.4.5 "If your terminal
		 * has the ANSI 'Insert Character' function..." will fail.
		 */
		sp_ch = &edit->bce_ch ;
	}
	else
	{
		sp_ch = ml_sp_ch() ;
	}

	for( count = 0 ; count < num_of_blank_chars ; count ++)
	{
		ml_char_copy( &blank_chars[count] , sp_ch) ;
	}

	ml_str_final( blank_chars , num_of_blank_chars) ;

	/* cursor will not moved. */
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
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  num_of_cols ;
	u_int  filled_len ;
	ml_line_t *  line ;
	int  beg ;
	u_int  cols ;
	int  new_char_index ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif

	buf_len = num_of_ow_chars + edit->model.num_of_cols ;

	if( edit->cursor.col > edit->hmargin_end)
	{
		num_of_cols = edit->model.num_of_cols ;
	}
	else
	{
		num_of_cols = edit->hmargin_end + 1 ;
	}

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

	/*
	 * overwriting
	 */

	beg = 0 ;
	count = 0 ;
	cols = 0 ;

	while( 1)
	{
		u_int  _cols ;

		_cols = ml_char_cols( &buffer[count]) ;

		if( edit->cursor.col + cols + _cols > num_of_cols ||
		    ( edit->wraparound_ready_line &&
		      edit->cursor.col + cols + _cols == num_of_cols))
		{
			ml_line_overwrite( line , edit->cursor.char_index , &buffer[beg] ,
				count - beg , cols) ;

			if( ! edit->is_auto_wrap)
			{
				break ;
			}

			ml_line_set_continued_to_next( line , 1) ;

			if( edit->cursor.row + 1 > edit->vmargin_end)
			{
				if( MARGIN_IS_ENABLED(edit) ?
				    ! scroll_upward_region( edit , 1 , 0) :
				    ! ml_edsl_scroll_upward( edit , 1))
				{
					return  0 ;
				}

				/*
				 * If edit->cursor.row == edit->vmargin_end in this situation,
				 * vmargin_beg == vmargin_end.
				 */
				if( edit->cursor.row + 1 <= edit->vmargin_end)
				{
					edit->cursor.row ++ ;
				}
			}
			else
			{
				edit->cursor.row ++ ;
			}

			if( edit->hmargin_beg > 0)
			{
				ml_cursor_goto_by_col( &edit->cursor , edit->hmargin_beg ,
					edit->cursor.row) ;
			}
			else
			{
				edit->cursor.char_index = edit->cursor.col = 0 ;
			}

			/* Reset edit->wraparound_ready_line because it is not cursor line now. */
			reset_wraparound_checker( edit) ;

			beg = count ;
			cols = _cols ;
			line = CURSOR_LINE(edit) ;
		}
		else
		{
			cols += _cols ;
		}

		if( ++ count >= filled_len)
		{
			break ;
		}
	}

	new_char_index = edit->cursor.char_index + count - beg ;

	if( edit->cursor.col + cols >= num_of_cols &&
		edit->wraparound_ready_line != line)
	{
		/*
		 * Don't use ml_line_end_char_index() instead of
		 * new_char_index --, because num_of_cols is not
		 * ml_model::num_of_cols but is ml_edit_t::hmargin_end + 1.
		 */
		new_char_index -- ;
		edit->wraparound_ready_line = line ;
	}
	else
	{
		reset_wraparound_checker( edit) ;
	}

	ml_line_overwrite( line , edit->cursor.char_index , &buffer[beg] ,
		count - beg , cols) ;
	ml_line_set_continued_to_next( line , 0) ;

	ml_cursor_moveh_by_char( &edit->cursor , new_char_index) ;

	ml_str_final( buffer , buf_len) ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif
	
	return  1 ;
}

/*
 * deleting cols within a line.
 */
int
ml_edit_delete_cols(
	ml_edit_t *  edit ,
	u_int  del_cols
	)
{
	int  char_index ;
	ml_char_t *  buffer ;
	u_int  buf_len ;
	u_int  filled_len ;
	ml_line_t *  cursor_line ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif

	reset_wraparound_checker( edit) ;

	cursor_line = CURSOR_LINE(edit) ;

	/* XXX del_cols should be converted to del_chars */
	if( edit->cursor.char_index + del_cols >= cursor_line->num_of_filled_chars)
	{
		/* no need to overwrite */

		ml_edit_clear_line_to_right( edit) ;	/* Considering BCE */

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

		if( del_cols >= cols_after)
		{
			del_cols -= cols_after ;
		}
		else
		{
			del_cols = 0 ;

			/*
			 * padding spaces after cursor.
			 */
			for( count = 0 ; count < cols_after - del_cols ; count ++)
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

	/* after cursor(excluding cursor) + del_cols */

	if( del_cols)
	{
		u_int  cols ;

		cols = ml_char_cols( ml_char_at( cursor_line , char_index ++)) ;

		if( MARGIN_IS_ENABLED(edit))
		{
			u_int  count ;
			u_int  copy_len ;

			if( ! CURSOR_IS_INSIDE_HMARGIN(edit))
			{
				return  0 ;
			}

			while( cols < del_cols &&
			       edit->cursor.col + cols < edit->hmargin_end + 1 &&
			       char_index < cursor_line->num_of_filled_chars)
			{
				cols += ml_char_cols( ml_char_at( cursor_line , char_index ++)) ;
			}

			copy_len = 0 ;
			while( edit->cursor.col + cols < edit->hmargin_end + 1 &&
			       char_index + copy_len < cursor_line->num_of_filled_chars)
			{
				cols += ml_char_cols( ml_char_at( cursor_line ,
							char_index + (copy_len ++))) ;
			}

			ml_str_copy( buffer + filled_len ,
				ml_char_at( cursor_line , char_index) , copy_len) ;
			filled_len += copy_len ;
			char_index += copy_len ;

			for( count = 0 ; count < del_cols ; count++)
			{
				ml_char_copy( buffer + (filled_len ++) ,
					edit->use_bce ? &edit->bce_ch : ml_sp_ch()) ;
			}
		}
		else
		{
			while( cols < del_cols && char_index < cursor_line->num_of_filled_chars)
			{
				cols += ml_char_cols( ml_char_at( cursor_line , char_index ++)) ;
			}
		}
	}

	ml_str_copy( buffer + filled_len , ml_char_at( cursor_line , char_index) ,
		cursor_line->num_of_filled_chars - char_index) ;
	filled_len += (cursor_line->num_of_filled_chars - char_index) ;

	if( filled_len > 0)
	{
		/*
		 * overwriting.
		 */

		ml_edit_clear_line_to_right( edit) ;	/* Considering BCE */
		ml_line_overwrite( cursor_line , edit->cursor.char_index ,
			buffer , filled_len , ml_str_cols( buffer , filled_len)) ;
	}
	else
	{
		ml_line_reset( cursor_line) ;
	}

	ml_str_final( buffer , buf_len) ;

	if( edit->cursor.col_in_char)
	{
		ml_cursor_moveh_by_char( &edit->cursor ,
			edit->cursor.char_index + edit->cursor.col_in_char) ;
	}

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif
	
	return  1 ;
}

int
ml_edit_clear_cols(
	ml_edit_t *  edit ,
	u_int  cols
	)
{
	ml_line_t *  cursor_line ;

	reset_wraparound_checker( edit) ;
	
	if( edit->cursor.col + cols >= edit->model.num_of_cols)
	{
		return  ml_edit_clear_line_to_right( edit) ;
	}
	
	cursor_line = CURSOR_LINE(edit) ;

	if( edit->cursor.col_in_char)
	{
		ml_line_fill( cursor_line , ml_sp_ch() ,
			edit->cursor.char_index , edit->cursor.col_in_char) ;

		ml_cursor_char_is_cleared( &edit->cursor) ;
	}

	ml_line_fill( cursor_line , edit->use_bce ? &edit->bce_ch : ml_sp_ch() ,
		edit->cursor.char_index , cols) ;

	return  1 ;
}

int
ml_edit_insert_new_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( MARGIN_IS_ENABLED(edit))
	{
		return  scroll_downward_region( edit , 1 , 1) ;
	}
	else
	{
		return  ml_edsl_insert_new_line( edit) ;
	}
}

int
ml_edit_delete_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( MARGIN_IS_ENABLED(edit))
	{
		return  scroll_upward_region( edit , 1 , 1) ;
	}
	else
	{
		return  ml_edsl_delete_line( edit) ;
	}
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
		ml_line_fill( cursor_line , edit->use_bce ? &edit->bce_ch : ml_sp_ch() ,
			edit->cursor.char_index , edit->cursor.col_in_char) ;
		ml_cursor_char_is_cleared( &edit->cursor) ;
	}

	if( edit->use_bce)
	{
		ml_line_clear_with( cursor_line , edit->cursor.char_index , &edit->bce_ch) ;
	}
	else
	{
		ml_line_clear( CURSOR_LINE(edit) , edit->cursor.char_index) ;
	}

	return  1 ;
}

int
ml_edit_clear_line_to_left(
	ml_edit_t *  edit
	)
{
	ml_line_t *  cursor_line ;

	reset_wraparound_checker( edit) ;

	cursor_line = CURSOR_LINE(edit) ;

	ml_line_fill( cursor_line , edit->use_bce ? &edit->bce_ch : ml_sp_ch() ,
		0 , edit->cursor.col + 1) ;

	ml_cursor_left_chars_in_line_are_cleared( &edit->cursor) ;
	
	return  1 ;
}

int
ml_edit_clear_below(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( ! ml_edit_clear_line_to_right( edit))
	{
		return  0 ;
	}

	if( edit->use_bce)
	{
		int  row ;

		for( row = edit->cursor.row + 1 ; row < edit->model.num_of_rows ; row ++)
		{
			ml_line_clear_with( ml_model_get_line( &edit->model , row) ,
				0 , &edit->bce_ch) ;
		}

		return  1 ;
	}
	else
	{
		return  ml_edit_clear_lines( edit , edit->cursor.row + 1 ,
				edit->model.num_of_rows - edit->cursor.row - 1) ;
	}
}

int
ml_edit_clear_above(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( ! ml_edit_clear_line_to_left( edit))
	{
		return  0 ;
	}

	if( edit->use_bce)
	{
		int  row ;

		for( row = 0 ; row < edit->cursor.row ; row ++)
		{
			ml_line_clear_with( ml_model_get_line( &edit->model , row) ,
				0 , &edit->bce_ch) ;
		}

		return  1 ;
	}
	else
	{
		return  ml_edit_clear_lines( edit , 0 , edit->cursor.row) ;
	}
}

int
ml_edit_set_vmargin(
	ml_edit_t *  edit ,
	int  beg ,
	int  end
	)
{
	/*
	 * for compatibility with xterm:
	 *
	 *   1. if beg and end is -1, use default.
	 *   2. if beg and end are smaller than 0, ignore the sequence.
	 *   3. if end is not larger than beg, ignore the sequence.
	 *   4. if beg and end are out of window, ignore the sequence.
	 *
	 *   (default = full size of window)
	 */

	if ( beg == -1)
	{
		beg = 0;
	}

	if ( end == -1)
	{
		end = ml_model_end_row( &edit->model) ;
	}

	if ( beg < 0 || end < 0)
	{
		return  0 ;
	}

	if ( beg >= end)
	{
		return  0 ;
	}

	if ( beg >= edit->model.num_of_rows && end >= edit-> model.num_of_rows)
	{
		return  0 ;
	}

	if( beg >= edit->model.num_of_rows)
	{
		beg = ml_model_end_row( &edit->model) ;
	}

	if( end >= edit->model.num_of_rows)
	{
		end = ml_model_end_row( &edit->model) ;
	}

	edit->vmargin_beg = beg ;
	edit->vmargin_end = end ;

	return  1 ;
}

int
ml_edit_scroll_leftward(
	ml_edit_t *  edit ,
	u_int  size
	)
{
	int  orig_mode ;

	orig_mode = edit->is_relative_origin ;
	edit->is_relative_origin = 1 ;	/* Compatible with RLogin */

#if  0
	if( ! edit->is_relative_origin)
	{
		ml_edit_copy_area( edit , size , 0 ,
			ml_edit_get_cols( edit) - size , ml_edit_get_rows( edit) ,
			0 , 0) ;
		ml_edit_erase_area( edit ,
			ml_edit_get_cols( edit) - size , 0 ,
			size , ml_edit_get_rows( edit)) ;
	}
	else
#endif
	{
		ml_edit_copy_area( edit , size , 0 ,
			edit->hmargin_end - edit->hmargin_beg + 1 - size ,
			edit->vmargin_end - edit->vmargin_beg + 1 ,
			0 , 0) ;
		ml_edit_erase_area( edit ,
			edit->hmargin_end - edit->hmargin_beg + 1 - size , 0 ,
			size , edit->vmargin_end - edit->vmargin_beg + 1) ;
	}

	edit->is_relative_origin = orig_mode ;

	return  1 ;
}

int
ml_edit_scroll_rightward(
	ml_edit_t *  edit ,
	u_int  size
	)
{
	int  orig_mode ;

	orig_mode = edit->is_relative_origin ;
	edit->is_relative_origin = 1 ;	/* Compatible with RLogin */

#if  0
	if( ! edit->is_relative_origin)
	{
		ml_edit_copy_area( edit , 0 , 0 ,
			ml_edit_get_cols( edit) - size , ml_edit_get_rows( edit) ,
			size , 0) ;
		ml_edit_erase_area( edit , 0 , 0 ,
			size , ml_edit_get_rows( edit)) ;
	}
	else
#endif
	{
		ml_edit_copy_area( edit , 0 , 0 ,
			edit->hmargin_end - edit->hmargin_beg + 1 - size ,
			edit->vmargin_end - edit->vmargin_beg + 1 ,
			size , 0) ;
		ml_edit_erase_area( edit , 0 , 0 ,
			size , edit->vmargin_end - edit->vmargin_beg + 1) ;
	}

	edit->is_relative_origin = orig_mode ;

	return  1 ;
}

int
ml_edit_scroll_leftward_from_cursor(
	ml_edit_t *  edit ,
	u_int  width
	)
{
	int  src ;
	u_int  height ;

	if( ! CURSOR_IS_INSIDE_HMARGIN(edit) ||
	    ! CURSOR_IS_INSIDE_VMARGIN(edit))
	{
		return  0 ;
	}

	height = edit->vmargin_end - edit->vmargin_beg + 1 ;

	if( ( src = edit->cursor.col + width) <= edit->hmargin_end)
	{
		copy_area( edit ,
			src , edit->vmargin_beg ,
			edit->hmargin_end - src + 1 , height ,
			edit->cursor.col , edit->vmargin_beg) ;
	}
	else
	{
		width = edit->hmargin_end - edit->cursor.col + 1 ;
	}

	erase_area( edit , edit->hmargin_end - width + 1 , edit->vmargin_beg ,
		width , height) ;

	return  1 ;
}

int
ml_edit_scroll_rightward_from_cursor(
	ml_edit_t *  edit ,
	u_int  width
	)
{
	int  dst ;
	u_int  height ;

	if( ! CURSOR_IS_INSIDE_HMARGIN(edit) ||
	    ! CURSOR_IS_INSIDE_VMARGIN(edit))
	{
		return  0 ;
	}

	height = edit->vmargin_end - edit->vmargin_beg + 1 ;

	if( ( dst = edit->cursor.col + width) <= edit->hmargin_end)
	{
		copy_area( edit ,
			edit->cursor.col , edit->vmargin_beg ,
			edit->hmargin_end - dst + 1 , height ,
			dst , edit->vmargin_beg) ;
	}
	else
	{
		width = edit->hmargin_end - edit->cursor.col + 1 ;
	}

	erase_area( edit , edit->hmargin_beg , edit->vmargin_beg ,
		width , height) ;

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

	if( MARGIN_IS_ENABLED(edit))
	{
		scroll_upward_region( edit , size , 0) ;
	}
	else
	{
		ml_edsl_scroll_upward( edit , size) ;
	}

	ml_cursor_goto_by_col( &edit->cursor , cursor_col , cursor_row) ;

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

	if( MARGIN_IS_ENABLED(edit))
	{
		scroll_downward_region( edit , size , 0) ;
	}
	else
	{
		ml_edsl_scroll_downward( edit , size) ;
	}

	ml_cursor_goto_by_col( &edit->cursor , cursor_col , cursor_row) ;

	return  1 ;
}

int
ml_edit_set_use_hmargin(
	ml_edit_t *  edit ,
	int  use
	)
{
	if( use <= 0)
	{
		edit->use_margin = 0 ;
		edit->hmargin_beg = 0 ;
		edit->hmargin_end = edit->model.num_of_cols - 1 ;
	}
	else
	{
		edit->use_margin = 1 ;
	}

	if( use >= 0)
	{
		ml_edit_goto_home( edit) ;
	}

	return  1 ;
}

int
ml_edit_set_hmargin(
	ml_edit_t *  edit ,
	int  beg ,
	int  end
	)
{
	if( edit->use_margin && 0 <= beg && beg < end && end < edit->model.num_of_cols)
	{
		edit->hmargin_beg = beg ;
		edit->hmargin_end = end ;

		ml_edit_goto_home( edit) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_edit_forward_tabs(
	ml_edit_t *  edit ,
	u_int  num
	)
{
	return  horizontal_tabs( edit , num , 1) ;
}

int
ml_edit_backward_tabs(
	ml_edit_t *  edit ,
	u_int  num
	)
{
	return  horizontal_tabs( edit , num , 0) ;
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

int
ml_edit_goto_beg_of_line(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	if( edit->hmargin_beg > 0 && edit->cursor.col >= edit->hmargin_beg)
	{
		return  ml_cursor_goto_by_col( &edit->cursor ,
				edit->hmargin_beg , edit->cursor.row) ;
	}
	else
	{
		return  ml_cursor_goto_beg_of_line( &edit->cursor) ;
	}
}

/*
 * Note that this function ignores edit->is_relative_origin.
 */
int
ml_edit_goto_home(
	ml_edit_t *  edit
	)
{
	reset_wraparound_checker( edit) ;

	ml_cursor_goto_home( &edit->cursor) ;

	return  1 ;
}

int
ml_edit_go_forward(
	ml_edit_t *  edit ,
	int  flag		/* WARPAROUND | SCROLL */
	)
{
	u_int  num_of_cols ;

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif

	if( CURSOR_IS_INSIDE_HMARGIN(edit))
	{
		num_of_cols = edit->hmargin_end + 1 ;
	}
	else
	{
		num_of_cols = edit->model.num_of_cols ;
	}

	reset_wraparound_checker( edit) ;

	if( edit->cursor.col + 1 >= num_of_cols)
	{
		if( ! ( flag & WRAPAROUND))
		{
			return  0 ;
		}

		if( ml_is_scroll_lowerlimit( edit , edit->cursor.row))
		{
			if( ! ( flag & SCROLL) ||
			    ( MARGIN_IS_ENABLED(edit) ?
			      ! scroll_upward_region( edit , 1 , 0) :
			      ! ml_edsl_scroll_upward( edit , 1)) )
			{
				return  0 ;
			}
		}

		ml_cursor_cr_lf( &edit->cursor) ;
	}
	else if( ! ml_cursor_go_forward( &edit->cursor))
	{
		ml_line_break_boundary( CURSOR_LINE(edit) , 1) ;
		ml_cursor_go_forward( &edit->cursor) ;
	}
	
#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
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
	ml_cursor_dump( &edit->cursor) ;
#endif

	if( edit->wraparound_ready_line)
	{
		reset_wraparound_checker( edit) ;

	#if 0 /* removed for sf.net BTS #1048321 -seiichi */
		return  1 ;
	#endif
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
	 
	if( edit->cursor.char_index == 0 || edit->cursor.char_index == edit->hmargin_beg)
	{
		if( ! ( flag & WRAPAROUND))
		{
			return  0 ;
		}

		if( ml_is_scroll_upperlimit( edit , edit->cursor.row))
		{
			if( ! ( flag & SCROLL) ||
			    ( MARGIN_IS_ENABLED(edit) ?
			      ! scroll_downward_region( edit , 1 , 0) :
			      ! ml_edsl_scroll_downward( edit , 1)) )
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
	ml_cursor_dump( &edit->cursor) ;
#endif

	return  1 ;
}

int
ml_edit_go_upward(
	ml_edit_t *  edit ,
	int  flag		/* SCROLL */
	)
{
#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif
	
	reset_wraparound_checker( edit) ;
	
	if( ml_is_scroll_upperlimit( edit , edit->cursor.row))
	{
		if( ! ( flag & SCROLL) ||
		    ( MARGIN_IS_ENABLED(edit) ?
		      ! scroll_downward_region( edit , 1 , 0) :
		      ! ml_edit_scroll_downward( edit , 1)) )
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cursor cannot go upward(reaches scroll lower limit).\n") ;
		#endif

			return  0 ;
		}
	}
	else
	{
		if( ! ml_cursor_goto_by_col( &edit->cursor , edit->cursor.col , edit->cursor.row - 1))
		{
			return  0 ;
		}
	}
	
#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif
	
	return  1 ;
}

int
ml_edit_go_downward(
	ml_edit_t *  edit ,
	int  flag		/* SCROLL */
	)
{
#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif
	
	reset_wraparound_checker( edit) ;
	
	if( ml_is_scroll_lowerlimit( edit , edit->cursor.row))
	{
		if( ! ( flag & SCROLL) ||
		    ( MARGIN_IS_ENABLED(edit) ?
		      ! scroll_upward_region( edit , 1 , 0) :
		      ! ml_edit_scroll_upward( edit , 1)) )
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG
				" cursor cannot go downward(reaches scroll lower limit).\n") ;
		#endif

			return  0 ;
		}
	}
	else
	{
		if( ! ml_cursor_goto_by_col( &edit->cursor , edit->cursor.col , edit->cursor.row + 1))
		{
			return  0 ;
		}
	}

#ifdef  CURSOR_DEBUG
	ml_cursor_dump( &edit->cursor) ;
#endif
	
	return  1 ;
}

int
ml_edit_goto(
	ml_edit_t *  edit ,
	int  col ,
	int  row
	)
{
	reset_wraparound_checker( edit) ;

	if( edit->is_relative_origin)
	{
		if( ( row += edit->vmargin_beg) > edit->vmargin_end)
		{
			row = edit->vmargin_end ;
		}

		if( ( col += edit->hmargin_beg) > edit->hmargin_end)
		{
			col = edit->hmargin_end ;
		}
	}
	
	return  ml_cursor_goto_by_col( &edit->cursor , col , row) ;
}

int
ml_edit_set_relative_origin(
	ml_edit_t *  edit
	)
{
	edit->is_relative_origin = 1 ;

	return  1 ;
}

int
ml_edit_set_absolute_origin(
	ml_edit_t *  edit
	)
{
	edit->is_relative_origin = 0 ;

	return  1 ;
}

int
ml_edit_set_auto_wrap(
	ml_edit_t *  edit ,
	int  flag
	)
{
	edit->is_auto_wrap = flag ;

	return  1 ;
}

int
ml_edit_set_bce_fg_color(
	ml_edit_t *  edit ,
	ml_color_t  fg_color
	)
{
	return  ml_char_set_fg_color( &edit->bce_ch , fg_color) ;
}

int
ml_edit_set_bce_bg_color(
	ml_edit_t *  edit ,
	ml_color_t  bg_color
	)
{
	return  ml_char_set_bg_color( &edit->bce_ch , bg_color) ;
}

int
ml_edit_save_cursor(
	ml_edit_t *  edit
	)
{
	return  ml_cursor_save( &edit->cursor) ;
}

int
ml_edit_restore_cursor(
	ml_edit_t *  edit
	)
{
	if( ml_cursor_restore( &edit->cursor))
	{
		reset_wraparound_checker(edit) ;

		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
ml_edit_fill_area(
	ml_edit_t *  edit ,
	ml_char_t *  ch ,
	int  col ,
	int  row ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	int  char_index ;
	u_int  cols_rest ;
	ml_line_t *  line ;

	if( ! apply_relative_origin( edit , &col , &row , &num_of_cols , &num_of_rows))
	{
		return  1 ;
	}

	for( ; num_of_rows > 0 ; num_of_rows --)
	{
		line = ml_model_get_line( &edit->model , row++) ;

		char_index = ml_convert_col_to_char_index( line , &cols_rest , col ,
				BREAK_BOUNDARY) ;
		if( cols_rest > 0)
		{
			ml_line_fill( line , ml_sp_ch() , char_index , cols_rest) ;
			char_index += cols_rest ;
		}

		ml_line_fill( line , ch , char_index , num_of_cols) ;
	}

	return  1 ;
}

int
ml_edit_copy_area(
	ml_edit_t *  edit ,
	int  src_col ,
	int  src_row ,
	u_int  num_of_copy_cols ,
	u_int  num_of_copy_rows ,
	int  dst_col ,
	int  dst_row
	)
{
	if( edit->is_relative_origin)
	{
		if( (src_row += edit->vmargin_beg) > edit->vmargin_end ||
		    (dst_row += edit->vmargin_beg) > edit->vmargin_end ||
		    (src_col += edit->hmargin_beg) > edit->hmargin_end ||
		    (dst_col += edit->hmargin_beg) > edit->hmargin_end)
		{
			return  1 ;
		}

		if( src_row + num_of_copy_rows > edit->vmargin_end + 1)
		{
			num_of_copy_rows = edit->vmargin_end + 1 - src_row ;
		}

		if( dst_row + num_of_copy_rows > edit->vmargin_end + 1)
		{
			num_of_copy_rows = edit->vmargin_end + 1 - dst_row ;
		}

		if( src_col + num_of_copy_cols > edit->hmargin_end + 1)
		{
			num_of_copy_cols = edit->hmargin_end + 1 - src_col ;
		}

		if( dst_col + num_of_copy_cols > edit->hmargin_end + 1)
		{
			num_of_copy_cols = edit->hmargin_end + 1 - dst_col ;
		}
	}

	return	copy_area( edit , src_col , src_row , num_of_copy_cols ,
			num_of_copy_rows , dst_col , dst_row) ;
}

int
ml_edit_erase_area(
	ml_edit_t *  edit ,
	int  col ,
	int  row ,
	u_int  num_of_cols ,
	u_int  num_of_rows
	)
{
	if( ! apply_relative_origin( edit , &col , &row , &num_of_cols , &num_of_rows))
	{
		return  1 ;
	}

	return  erase_area( edit , col , row , num_of_cols , num_of_rows) ;
}

int
ml_edit_change_attr_area(
	ml_edit_t *  edit ,
	int  col ,
	int  row ,
	u_int  num_of_cols ,
	u_int  num_of_rows ,
	void (*func)( ml_char_t * , int , int , int , int) ,
	int  attr
	)
{
	u_int  count ;
	ml_line_t *  line ;
	int  char_index ;
	int  end_char_index ;
	u_int  cols_rest ;
	int  bold ;
	int  underlined ;
	int  blinking ;
	int  reversed ;

	if( attr == 0)
	{
		bold = underlined = blinking = reversed = -1 ;
	}
	else
	{
		bold = underlined = blinking = reversed = 0 ;

		if( attr == 1)
		{
			bold = 1 ;
		}
		else if( attr == 4)
		{
			underlined = 1 ;
		}
		else if( attr == 5)
		{
			blinking = 1 ;
		}
		else if( attr == 7)
		{
			reversed = 1 ;
		}
		else if( attr == 22)
		{
			bold = -1 ;
		}
		else if( attr == 24)
		{
			underlined = -1 ;
		}
		else if( attr == 25)
		{
			blinking = -1 ;
		}
		else if( attr == 27)
		{
			reversed = -1 ;
		}
		else
		{
			return  0 ;
		}
	}

	if( ! apply_relative_origin( edit , &col , &row , &num_of_cols , &num_of_rows))
	{
		return  1 ;
	}

	for( count = 0 ; count < num_of_rows ; count++)
	{
		if( count == 1 && ! edit->use_rect_attr_select)
		{
			int  old_col ;

			old_col = col ;
			col = edit->is_relative_origin ? edit->hmargin_beg : 0 ;
			num_of_cols += (old_col - col) ;
		}

		if( ! (line = ml_edit_get_line( edit , row + count)))
		{
			continue ;
		}

		char_index = ml_convert_col_to_char_index( line , &cols_rest , col ,
					BREAK_BOUNDARY) ;
		if( char_index >= line->num_of_filled_chars && attr > 7)
		{
			continue ;
		}

		if( cols_rest > 0)
		{
			char_index ++ ;
		}

		if( edit->use_rect_attr_select || count + 1 == num_of_rows)
		{
			end_char_index = ml_convert_col_to_char_index( line , NULL ,
						col + num_of_cols - 1 , BREAK_BOUNDARY) ;
		}
		else
		{
			end_char_index = ml_convert_col_to_char_index( line , NULL ,
						edit->is_relative_origin ?
							edit->hmargin_end :
							ml_edit_get_cols( edit) - 1 ,
						BREAK_BOUNDARY) ;
		}

		ml_line_assure_boundary( line , end_char_index) ;

		ml_line_set_modified( line , char_index , end_char_index) ;

		for( ; char_index <= end_char_index ; char_index++)
		{
			(*func)( ml_char_at( line , char_index) ,
				bold , underlined , blinking , reversed) ;
		}
	}

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
		" ===> dumping edit...[cursor(index)%d (col)%d (row)%d]\n" ,
		edit->cursor.char_index , edit->cursor.col , edit->cursor.row) ;

	for( row = 0 ; row < edit->model.num_of_rows ; row++)
	{
		int  char_index ;

		line = ml_model_get_line( &edit->model , row) ;

		if( ml_line_is_modified( line))
		{
			if( ml_line_is_cleared_to_end( line))
			{
				kik_msg_printf( "!%.2d-END" ,
					ml_line_get_beg_of_modified( line)) ;
			}
			else
			{
				kik_msg_printf( "!%.2d-%.2d" ,
					ml_line_get_beg_of_modified( line) ,
					ml_line_get_end_of_modified( line)) ;
			}
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

				ml_char_dump( ml_char_at( line , char_index)) ;

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

	for( row = 0 ; row < edit->model.num_of_rows ; row ++)
	{
		kik_msg_printf( "(%.2d)%d " ,
			row , ml_line_is_modified( ml_model_get_line( &edit->model , row))) ;
	}

	kik_msg_printf( "\n") ;
}

#endif
