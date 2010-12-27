/*
 *	$Id$
 */

#include  "ml_edit_util.h"

#include  <kiklib/kik_debug.h>


/* --- global functions --- */

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
		return  0 ;
	}

	if( beg_row > ml_model_end_row( &edit->model))
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " line %d is already empty.\n" , beg_row) ;
	#endif
		
		return  0 ;
	}

	if( edit->use_bce)
	{
		for( count = 0 ; count < size ; count ++)
		{
			ml_line_fill( ml_model_get_line( &edit->model , beg_row + count) ,
				&edit->bce_ch , 0 , edit->model.num_of_cols) ;
		}
	}
	else
	{
		for( count = 0 ; count < size ; count ++)
		{
			ml_line_reset( ml_model_get_line( &edit->model , beg_row + count)) ;
		}
	}

	if( beg_row <= edit->cursor.row && edit->cursor.row <= beg_row + size - 1)
	{
		u_int  brk_size ;

		if( ( brk_size = ml_line_break_boundary( CURSOR_LINE(edit) , edit->cursor.col + 1)) == 0)
		{
		#ifdef  DEBUG
			kik_warn_printf( KIK_DEBUG_TAG " critical error.\n") ;
		#endif

			edit->cursor.char_index = edit->cursor.col = 0 ;
		}
		else
		{
			edit->cursor.char_index = edit->cursor.col = brk_size - 1 ;
		}
		
		edit->cursor.col_in_char = 0 ;
	}

	return  1 ;
}
