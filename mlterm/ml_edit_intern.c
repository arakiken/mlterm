/*
 *	$Id$
 */

#include  "ml_edit_intern.h"

#include  <kiklib/kik_debug.h>


/* --- global functions --- */

int
ml_edit_clear_line(
	ml_edit_t *  edit ,
	int  row ,
	int  char_index
	)
{
	ml_line_t *  line ;
	
	if( row > ml_model_end_row( &edit->model))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " row %d is over END_ROW %d. nothing is cleared.\n" ,
			row , ml_model_end_row( &edit->model)) ;
	#endif

		return  1 ;
	}

	line = ml_model_get_line( &edit->model , row) ;

	ml_line_clear( line , char_index) ;
	
	if( row == edit->cursor.row)
	{
		if( edit->cursor.char_index > char_index)
		{
			edit->cursor.char_index = char_index ;
			edit->cursor.col = ml_convert_char_index_to_col( line , char_index , 0) ;
		}
	}

	return  1 ;
}

/*
 * used in ml_edit/ml_edit_scroll
 */
int
ml_edit_clear_lines(
	ml_edit_t *  edit ,
	int  beg_row ,
	u_int  size
	)
{
	int  count ;

	if( size == 0)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " size %d should be larger than 0.\n" , size) ;
	#endif
	
		return  0 ;
	}

	if( beg_row > ml_model_end_row( &edit->model))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " line %d is already empty.\n" , beg_row) ;
	#endif
		
		return  0 ;
	}

	ml_edit_clear_line( edit , beg_row , 0) ;

	if( beg_row + size < edit->model.num_of_filled_rows)
	{
		/*
		 * there will still be some lines after the cleared lines.
		 */
		 
		for( count = 1 ; count < size ; count ++)
		{
			ml_edit_clear_line( edit , beg_row + count , 0) ;
		}

		if( beg_row <= edit->cursor.row && edit->cursor.row <= beg_row + size - 1)
		{
			edit->cursor.char_index = 0 ;
			edit->cursor.col = 0 ;
		}
	}
	else
	{
		ml_model_shrink_boundary( &edit->model , edit->model.num_of_filled_rows - beg_row - 1) ;
	
		if( edit->cursor.row >= beg_row)
		{
			edit->cursor.row = beg_row ;
			edit->cursor.char_index = 0 ;
			edit->cursor.col = 0 ;
		}
	}

	return  1 ;
}
