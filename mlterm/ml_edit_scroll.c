/*
 *	$Id$
 */

#include  "ml_edit_scroll.h"

#include  <kiklib/kik_util.h>

#include  "ml_edit_util.h"


/* --- static functions --- */

/*
 * src and dst may overlap
 */
static int
copy_lines(
	ml_edit_t *  edit ,
	int  dst_row ,
	int  src_row ,
	u_int  size ,
	int  mark_changed
	)
{
	int  count ;
	ml_line_t *  src_line ;
	ml_line_t *  dst_line ;
	
	if( size == 0 || dst_row == src_row)
	{
		return  1 ;
	}

	if( src_row + size > edit->model.num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" copying %d lines from %d row is over edit->model.num_of_rows(%d)" ,
			size , src_row , edit->model.num_of_rows) ;
	#endif

		size = edit->model.num_of_rows - src_row ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size modified -> %d.\n" , size) ;
	#endif
	}
	
	if( dst_row + size > edit->model.num_of_rows)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG
			" copying %d lines to %d row is over edit->model.num_of_rows(%d)" ,
			size , dst_row , edit->model.num_of_rows) ;
	#endif

		size = edit->model.num_of_rows - dst_row ;

	#ifdef  DEBUG
		kik_msg_printf( " ... size modified -> %d.\n" , size) ;
	#endif
	}

	if( dst_row < src_row)
	{
		for( count = 0 ; count < size ; count ++)
		{
			dst_line = ml_model_get_line( &edit->model , dst_row + count) ;
			src_line = ml_model_get_line( &edit->model , src_row + count) ;
			
			ml_line_copy_line( dst_line , src_line) ;
			if( mark_changed)
			{
				ml_line_set_modified_all( dst_line) ;
			}
		}
	}
	else
	{
		for( count = size - 1 ; count >= 0 ; count --)
		{
			dst_line = ml_model_get_line( &edit->model , dst_row + count) ;
			src_line = ml_model_get_line( &edit->model , src_row + count) ;
			
			ml_line_copy_line( dst_line , src_line) ;
			if( mark_changed)
			{
				ml_line_set_modified_all( dst_line) ;
			}
		}
	}

	return  1 ;
}

static int
clear_lines_to_eol(
	ml_edit_t *  edit ,
	int  beg_row ,
	u_int  size
	)
{
	int  count ;

	ml_edit_clear_lines( edit , beg_row , size) ;

	for( count = 0 ; count < size ; count ++)
	{
		ml_line_set_modified_all( ml_model_get_line( &edit->model , beg_row + count)) ;
	}

	return  1 ;
}

static int
scroll_upward_region(
	ml_edit_t *  edit ,
	int  boundary_beg ,
	int  boundary_end ,
	u_int  size
	)
{
	int  count ;
	int  window_is_scrolled ;

	if( boundary_beg + size > boundary_end)
	{
		/*
		 * all lines within boundary are scrolled out.
		 */

		if( edit->is_logging && edit->scroll_listener->receive_scrolled_out_line)
		{
			for( count = boundary_beg ; count < boundary_end ; count ++)
			{
				(*edit->scroll_listener->receive_scrolled_out_line)(
					edit->scroll_listener->self ,
					ml_model_get_line( &edit->model , count)) ;
			}
		}

		edit->cursor.row = boundary_beg ;
		edit->cursor.col = 0 ;
		edit->cursor.char_index = 0 ;

		(*edit->scroll_listener->scrolled_out_lines_finished)(
			edit->scroll_listener->self) ;

		return  clear_lines_to_eol( edit , boundary_beg , boundary_end - boundary_beg + 1) ;
	}

	/*
	 * scrolling up in window.
	 *
	 * !! Notice !!
	 * This should be done before ml_edit_t data structure is chanegd
	 * for the listener object to clear existing cache.
	 */
#if  0
	kik_debug_printf( " SCROLL UP region %d %d size %d\n" ,
		boundary_beg , boundary_end , size) ;
#endif
	window_is_scrolled = (*edit->scroll_listener->window_scroll_upward_region)(
				edit->scroll_listener->self , boundary_beg , boundary_end , size) ;
	 
	/*
	 * handing over scrolled out lines , and calculating scrolling beg/end y positions.
	 */

	if( edit->is_logging && edit->scroll_listener->receive_scrolled_out_line)
	{
		for( count = boundary_beg ; count < boundary_beg + size ; count ++)
		{
			(*edit->scroll_listener->receive_scrolled_out_line)(
				edit->scroll_listener->self ,
				ml_model_get_line( &edit->model , count)) ;
		}
	}

	/*
	 * resetting cursor position.
	 */
	 
	if( boundary_beg <= edit->cursor.row && edit->cursor.row <= boundary_end)
	{
		if( edit->cursor.row < boundary_beg + size)
		{
			edit->cursor.row = boundary_beg ;
			edit->cursor.char_index = 0 ;
			edit->cursor.col = 0 ;
		}
		else
		{
			edit->cursor.row -= size ;
		}
	}
	
	/*
	 * scrolling up in edit.
	 */
	 
	if( boundary_beg == 0 && boundary_end == ml_model_end_row( &edit->model))
	{
		ml_model_scroll_upward( &edit->model , size) ;
	}
	else
	{
		copy_lines( edit , boundary_beg , boundary_beg + size ,
			boundary_end - (boundary_beg + size) + 1 , 0) ;
	}
	
	if( ! window_is_scrolled)
	{
		int  count ;

		ml_edit_clear_lines( edit , boundary_end - size + 1 , size) ;

		for( count = boundary_beg ; count <= boundary_end ; count ++)
		{
			ml_line_set_modified_all( ml_model_get_line( &edit->model , count)) ;
		}
	}
	else
	{
		clear_lines_to_eol( edit , boundary_end - size + 1 , size) ;
	}

	/*
	 * This must be called after ml_model_scroll_upward() because scrolled_out_lines_finished()
	 * can change ml_model_t.
	 */
	(*edit->scroll_listener->scrolled_out_lines_finished)( edit->scroll_listener->self) ;
	
	return  1 ;
}

static int
scroll_downward_region(
	ml_edit_t *  edit ,
	int  boundary_beg ,
	int  boundary_end ,
	u_int  size
	)
{
	int  window_is_scrolled ;

	if( boundary_beg + size > boundary_end)
	{
		/*
		 * all lines within boundary are scrolled out.
		 */

		edit->cursor.row = boundary_end ;
		edit->cursor.col = 0 ;
		edit->cursor.char_index = 0 ;
		
		return  clear_lines_to_eol( edit , boundary_beg , boundary_end - boundary_beg + 1) ;
	}

	/*
	 * scrolling down in window.
	 *
	 * !! Notice !!
	 * This should be done before ml_edit_t data structure is chanegd
	 * for the listener object to clear existing cache.
	 */
#if  0
	kik_debug_printf( " SCROLL DOWN region %d %d size %d\n" ,
		boundary_beg , boundary_end , size) ;
#endif
	window_is_scrolled = (*edit->scroll_listener->window_scroll_downward_region)(
				edit->scroll_listener->self , boundary_beg , boundary_end , size) ;
	
	/*
	 * resetting cursor position.
	 */
	if( boundary_beg <= edit->cursor.row && edit->cursor.row <= boundary_end)
	{
		if( edit->cursor.row + size >= boundary_end + 1)
		{
			edit->cursor.row = boundary_end ;
			edit->cursor.char_index = 0 ;
			edit->cursor.col = 0 ;
		}
		else
		{
			edit->cursor.row += size ;
		}
	}
	
	/*
	 * scrolling down in edit.
	 */
	
	if( boundary_beg == 0 && boundary_end == ml_model_end_row( &edit->model))
	{
		ml_model_scroll_downward( &edit->model , size) ;
	}
	else
	{
		copy_lines( edit , boundary_beg + size , boundary_beg ,
			(boundary_end - size) - boundary_beg + 1 , 0) ;
	}

	if( ! window_is_scrolled)
	{
		int  count ;
		
		ml_edit_clear_lines( edit , boundary_beg , size) ;

		for( count = boundary_beg ; count <= boundary_end ; count ++)
		{
			ml_line_set_modified_all( ml_model_get_line( &edit->model , count)) ;
		}
	}
	else
	{
		clear_lines_to_eol( edit , boundary_beg , size) ;
	}

	return  1 ;
}


/* --- global functions --- */

int
ml_edsl_scroll_upward(
	ml_edit_t *  edit ,
	u_int  size
	)
{
#if  0
	/*
	 * XXX
	 * Can this cause unexpected result ?
	 */
	if( edit->scroll_region_beg > edit->cursor.row || edit->cursor.row > edit->scroll_region_end)
	{
		return  0 ;
	}
#endif

	return  scroll_upward_region( edit , edit->scroll_region_beg , edit->scroll_region_end , size) ;
}

int
ml_edsl_scroll_downward(
	ml_edit_t *  edit ,
	u_int  size
	)
{
#if  0
	/*
	 * XXX
	 * Can this cause unexpected result ?
	 */
	if( edit->scroll_region_beg > edit->cursor.row || edit->cursor.row > edit->scroll_region_end)
	{
		return  0 ;
	}
#endif
	
	return  scroll_downward_region( edit , edit->scroll_region_beg , edit->scroll_region_end , size) ;
}

/*
 * XXX
 * not used for now.
 */
#if  0
int
ml_edsl_scroll_upward_in_all(
	ml_edit_t *  edit ,
	u_int  size
	)
{
	return  scroll_upward_region( edit , 0 , edit->model.num_of_rows - 1 , size) ;
}

int
ml_edsl_scroll_downward_in_all(
	ml_edit_t *  edit ,
	u_int  size
	)
{
	return  scroll_downward_region( edit , 0 , edit->model.num_of_rows - 1 , size) ;
}
#endif

int
ml_is_scroll_upperlimit(
	ml_edit_t *  edit ,
	int  row
	)
{
	return  (row == edit->scroll_region_beg) ;
}

int
ml_is_scroll_lowerlimit(
	ml_edit_t *  edit ,
	int  row
	)
{
	return  (row == edit->scroll_region_end) ;
}

int
ml_edsl_insert_new_line(
	ml_edit_t *  edit
	)
{
	int  start_row ;
	int  start_col ;
	int  end_row ;

	if( edit->cursor.row < edit->scroll_region_beg ||
	    edit->scroll_region_end < edit->cursor.row)
	{
		return  0 ;
	}

	start_row = edit->cursor.row ;
	start_col = edit->cursor.col ;
	end_row = edit->scroll_region_end ;

	scroll_downward_region( edit , start_row , end_row , 1) ;
	ml_cursor_goto_by_col( &edit->cursor , start_col , start_row) ;
	ml_edit_clear_line_to_right( edit) ;

	return  1 ;
}

int
ml_edsl_delete_line(
	ml_edit_t *  edit
	)
{
	int  start_row ;
	int  start_col ;
	int  end_row ;
	int  is_logging ;

	if( edit->cursor.row < edit->scroll_region_beg ||
	    edit->scroll_region_end < edit->cursor.row)
	{
		return  0 ;
	}

	is_logging = edit->is_logging ;
	edit->is_logging = 0 ;

	start_row = edit->cursor.row ;
	start_col = edit->cursor.col ;
	end_row = edit->scroll_region_end ;

	scroll_upward_region( edit , start_row , end_row , 1) ;
	ml_edit_clear_lines( edit , end_row , 1) ;
	ml_cursor_goto_by_col( &edit->cursor , start_col , start_row) ;

	edit->is_logging = is_logging ;

	return  1 ;
}
