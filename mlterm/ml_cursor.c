/*
 *	$Id$
 */

#include  "ml_cursor.h"


#include  <kiklib/kik_debug.h>


/* --- static functions --- */

static int
cursor_goto(
	ml_cursor_t *  cursor ,
	int  col_or_idx ,
	int  row ,
	int  is_by_col
	)
{
	int  char_index ;
	int  cols_rest ;
	ml_line_t *  line ;

	if( row > ml_model_end_row( cursor->model))
	{
		/* round row to end of row */
		row = ml_model_end_row( cursor->model) ;
	}

	if( ( line = ml_model_get_line( cursor->model , row)) == NULL)
	{
		return  0 ;
	}

	if( is_by_col)
	{
		char_index = ml_convert_col_to_char_index( line , &cols_rest , col_or_idx , BREAK_BOUNDARY) ;
	}
	else
	{
		char_index = col_or_idx ;
		cols_rest = 0 ;
	}

	if( ! ml_line_assure_boundary( line , char_index))
	{
	#ifdef  DEBUG
		kik_warn_printf(
			KIK_DEBUG_TAG " cursor cannot goto char index %d(line length is %d)\n" ,
			char_index , line->num_of_filled_chars) ;
	#endif

		char_index = ml_line_end_char_index(line) ;
		
		/* In this case, cols_rest is always 0. */
	}

	cursor->char_index = char_index ;
	cursor->row = row ;
	cursor->col_in_char = cols_rest ;
	cursor->col = ml_convert_char_index_to_col( ml_model_get_line( cursor->model , cursor->row) ,
			cursor->char_index , 0) + cursor->col_in_char ;

	return  1 ;
}


/* --- global functions --- */

int
ml_cursor_init(
	ml_cursor_t *  cursor ,
	ml_model_t *  model
	)
{
	cursor->model = model ;

	cursor->row = 0 ;
	cursor->char_index = 0 ;
	cursor->col = 0 ;
	cursor->col_in_char = 0 ;
	cursor->saved_row = 0 ;
	cursor->saved_char_index = 0 ;
	cursor->saved_col = 0 ;
	cursor->is_saved = 0 ;
	
	return  1 ;
}

int
ml_cursor_final(
	ml_cursor_t *  cursor
	)
{
	/* Do nothing */
	
	return  1 ;
}

int
ml_cursor_goto_by_char(
	ml_cursor_t *  cursor ,
	int  char_index ,
	int  row
	)
{
	return  cursor_goto( cursor , char_index , row , 0) ;
}

/* Move horizontally */
int
ml_cursor_moveh_by_char(
	ml_cursor_t *  cursor ,
	int  char_index
	)
{
	return  cursor_goto( cursor , char_index , cursor->row , 0) ;
}

int
ml_cursor_goto_by_col(
	ml_cursor_t *  cursor ,
	int  col ,
	int  row
	)
{
	return  cursor_goto( cursor , col , row , 1) ;
}

/* Move horizontally */
int
ml_cursor_moveh_by_col(
	ml_cursor_t *  cursor ,
	int  col
	)
{
	return  cursor_goto( cursor , col , cursor->row , 1) ;
}

int
ml_cursor_goto_home(
	ml_cursor_t *  cursor
	)
{
	cursor->row = 0 ;
	cursor->char_index = 0 ;
	cursor->col = 0 ;
	cursor->col_in_char = 0 ;

	return  1 ;
}

int
ml_cursor_goto_beg_of_line(
	ml_cursor_t *  cursor
	)
{
	cursor->char_index = 0 ;
	cursor->col = 0 ;
	cursor->col_in_char = 0 ;

	return  1 ;
}

int
ml_cursor_reset_col_in_char(
	ml_cursor_t *  cursor
	)
{
	cursor->col = ml_convert_char_index_to_col( ml_get_cursor_line( cursor) , cursor->char_index , 0) ;
	cursor->col_in_char = 0 ;

	return  1 ;
}

int
ml_cursor_go_forward(
	ml_cursor_t *  cursor
	)
{
	/* full width char check. */
	if( cursor->col_in_char + 1 < ml_char_cols( ml_get_cursor_char(cursor)))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " cursor is at 2nd byte of multi byte char.\n") ;
	#endif

		cursor->col ++ ;
		cursor->col_in_char ++ ;

		return  1 ;
	}
	else if( cursor->char_index < ml_line_end_char_index( ml_get_cursor_line(cursor)))
	{
		cursor->char_index ++ ;
		ml_cursor_reset_col_in_char( cursor) ;

		return  1 ;
	}
	else
	{
		/* Can't go forward in this line anymore. */

		return  0 ;
	}
}

int
ml_cursor_cr_lf(
	ml_cursor_t *  cursor
	)
{
	if( cursor->model->num_of_rows <= cursor->row + 1)
	{
		return  0 ;
	}
	
	cursor->row ++ ;
	cursor->char_index = 0 ;
	cursor->col = 0 ;

	if( ! ml_line_assure_boundary( ml_get_cursor_line(cursor) , 0))
	{
		kik_error_printf( "Could cause unexpected behavior.\n") ;
		return  0 ;
	}

	return  1 ;
}

ml_line_t *
ml_get_cursor_line(
	ml_cursor_t *  cursor
	)
{
	return  ml_model_get_line( cursor->model , cursor->row) ;
}

ml_char_t *
ml_get_cursor_char(
	ml_cursor_t *  cursor
	)
{

	return  ml_model_get_line( cursor->model , cursor->row)->chars + cursor->char_index ;
}

int
ml_cursor_char_is_cleared(
	ml_cursor_t *  cursor
	)
{
	cursor->char_index += cursor->col_in_char ;
	cursor->col_in_char = 0 ;

	return  1 ;
}

int
ml_cursor_left_chars_in_line_are_cleared(
	ml_cursor_t *  cursor
	)
{
	cursor->char_index = cursor->col ;
	cursor->col_in_char = 0 ;

	return  1 ;
}

int
ml_cursor_save(
	ml_cursor_t *  cursor
	)
{
	cursor->saved_col = cursor->col ;
	cursor->saved_char_index = cursor->char_index ;
	cursor->saved_row = cursor->row ;
	cursor->is_saved = 1 ;

	return  1 ;
}

int
ml_cursor_restore(
	ml_cursor_t *  cursor
	)
{
	if( ! cursor->is_saved)
	{
		return  0 ;
	}
	
	if( ! ml_cursor_goto_by_col( cursor , cursor->saved_col , cursor->saved_row))
	{
		return  0 ;
	}
	
	return  1 ;
}

#ifdef  DEBUG

void
ml_cursor_dump(
	ml_cursor_t *  cursor
	)
{
	kik_msg_printf( "Cursor position => CH_IDX %d COL %d(+%d) ROW %d.\n" ,
		cursor->char_index , cursor->col , cursor->col_in_char , cursor->row) ;
}

#endif
