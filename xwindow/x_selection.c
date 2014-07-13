/*
 *	$Id$
 */

#include  "x_selection.h"

#include  <string.h>	/* memset */
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>
#include  <ml_str_parser.h>


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static int
update_sel_region(
	x_selection_t *  sel ,
	int  col ,
	int  row
	)
{
	int  rv_beg_col ;
	int  rv_beg_row ;
	int  rv_end_col ;
	int  rv_end_row ;
	int  do_reverse ;
	
	int  rs_beg_col ;
	int  rs_beg_row ;
	int  rs_end_col ;
	int  rs_end_row ;
	int  do_restore ;

	if( sel->is_rect)
	{
		int  conved ;
		int  conved_col ;

		(*sel->sel_listener->restore_color)( sel->sel_listener->self ,
			sel->beg_col , sel->beg_row , sel->end_col , sel->end_row , 1) ;

		if( ( col < 0 && sel->base_col_r >= 0) ||
		    ( col >= 0 && sel->base_col_r < 0))
		{
			conved_col = -col ;
			conved = 1 ;
		}
		else
		{
			conved_col = col ;
			conved = 0 ;
		}

		if( conved_col < sel->base_col_r)
		{
			if( row <= sel->base_row_r)
			{
				sel->beg_col = col ;
				sel->beg_row = row ;
				sel->end_col = sel->base_col_l ;
				sel->end_row = sel->base_row_l ;
			}
			else
			{
				sel->beg_col = conved_col ;
				sel->beg_row = sel->base_row_l ;
				if( conved)
				{
					sel->end_col = -sel->base_col_l ;
				}
				else
				{
					sel->end_col = sel->base_col_l ;
				}
				sel->end_row = row ;
			}
		}
		else
		{
			if( row <= sel->base_row_r)
			{
				if( conved)
				{
					sel->beg_col = -sel->base_col_r ;
				}
				else
				{
					sel->beg_col = sel->base_col_r ;
				}
				sel->beg_row = row ;
				sel->end_col = conved_col ;
				sel->end_row = sel->base_row_r ;
			}
			else
			{
				sel->beg_col = sel->base_col_r ;
				sel->beg_row = sel->base_row_r ;
				sel->end_col = col ;
				sel->end_row = row ;
			}
		}

		(*sel->sel_listener->reverse_color)( sel->sel_listener->self ,
			sel->beg_col , sel->beg_row , sel->end_col , sel->end_row , 1) ;

		return  1 ;
	}

	do_reverse = 0 ;
	do_restore = 0 ;

	if( sel->beg_row > row || (sel->beg_row == row && sel->beg_col > col))
	{
		rv_beg_col = col ;
		rv_beg_row = row ;
		rv_end_col = sel->beg_col ;
		rv_end_row = sel->beg_row ;
		do_reverse = 1 ;

		sel->beg_col = col ;
		sel->beg_row = row ;

		if( sel->end_row > sel->base_row_r ||
			(sel->end_row == sel->base_row_r && sel->end_col >= sel->base_col_r))
		{
			rs_beg_col = sel->base_col_r ;
			rs_beg_row = sel->base_row_r ;
			rs_end_col = sel->end_col ;
			rs_end_row = sel->end_row ;
			do_restore = 1 ;
			
			sel->end_col = sel->base_col_l ;
			sel->end_row = sel->base_row_l ;
		}
	}
	else if((sel->beg_row < row || (sel->beg_row == row && sel->beg_col <= col)) &&
		(sel->end_row > row || (sel->end_row == row && sel->end_col >= col)))
	{
		if( row > sel->base_row_r || (row == sel->base_row_r && col >= sel->base_col_r))
		{
			rs_beg_col = col + 1 ;	/* don't restore col itself */
			rs_beg_row = row ;
			rs_end_col = sel->end_col ;
			rs_end_row = sel->end_row ;
			do_restore = 1 ;

			sel->end_col = col ;
			sel->end_row = row ;
		}
		else if( row < sel->base_row_l || (row == sel->base_row_l && col <= sel->base_col_l))
		{
			rs_beg_col = sel->beg_col ;
			rs_beg_row = sel->beg_row ;
			rs_end_col = col - 1 ;	/* don't restore col itself */
			rs_end_row = row ;
			do_restore = 1 ;

			sel->beg_col = col ;
			sel->beg_row = row ;
		}
	}
	else if( sel->end_row < row || (sel->end_row == row && sel->end_col < col))
	{
		rv_beg_col = sel->end_col ;
		rv_beg_row = sel->end_row ;
		rv_end_col = col ;
		rv_end_row = row ;
		do_reverse = 1 ;

		sel->end_col = col ;
		sel->end_row = row ;
		
		if( sel->beg_row < sel->base_row_l ||
			(sel->beg_row == sel->base_row_l && sel->beg_col <= sel->base_col_l))
		{
			rs_beg_col = sel->beg_col ;
			rs_beg_row = sel->beg_row ;
			rs_end_col = sel->base_col_l ;
			rs_end_row = sel->base_row_l ;
			do_restore = 1 ;
			
			sel->beg_col = sel->base_col_r ;
			sel->beg_row = sel->base_row_r ;
		}
	}

	if( do_reverse)
	{
		(*sel->sel_listener->reverse_color)( sel->sel_listener->self ,
			rv_beg_col , rv_beg_row , rv_end_col , rv_end_row , 0) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " reversing %d %d %d %d\n" ,
			rv_beg_col , rv_beg_row , rv_end_col , rv_end_row) ;
	#endif
	}

	if( do_restore)
	{
		(*sel->sel_listener->restore_color)( sel->sel_listener->self ,
			rs_beg_col , rs_beg_row , rs_end_col , rs_end_row , 0) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " restoring %d %d %d %d\n" ,
			rs_beg_col , rs_beg_row , rs_end_col , rs_end_row) ;
	#endif

		if( sel->is_locked == 1)
		{
			if( ( sel->end_row < sel->lock_row ||
			      ( sel->end_row == sel->lock_row &&
				sel->end_col < sel->lock_col)) )
			{
				(*sel->sel_listener->reverse_color)(
					sel->sel_listener->self ,
					rs_beg_col , rs_beg_row ,
					(sel->end_col = sel->lock_col) ,
					(sel->end_row = sel->lock_row) , 0) ;
			}
		}
		else if( sel->is_locked == -1)
		{
			if( ( sel->beg_row > sel->lock_row ||
			      ( sel->beg_row == sel->lock_row &&
				sel->beg_col > sel->lock_col)) )
			{
				(*sel->sel_listener->reverse_color)(
					sel->sel_listener->self ,
					(sel->beg_col = sel->lock_col) ,
					(sel->beg_row = sel->lock_row) ,
					rs_end_col , rs_end_row , 0) ;
			}
		}
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " current region %d %d %d %d\n" ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;
#endif
	
	return  1 ;
}


/* --- global functions --- */

int
x_sel_init(
	x_selection_t *  sel ,
	x_sel_event_listener_t *  sel_listener
	)
{
	memset( sel , 0 , sizeof(x_selection_t)) ;

	sel->sel_listener = sel_listener ;

	return  1 ;
}

int
x_sel_final(
	x_selection_t *  sel
	)
{
	if( sel->sel_str)
	{
		ml_str_delete( sel->sel_str , sel->sel_len) ;
	}

	return  1 ;
}

int
x_start_selection(
	x_selection_t *  sel ,
	int  col_l ,
	int  row_l ,
	int  col_r ,
	int  row_r ,
	x_sel_type_t  type ,
	int  is_rect
	)
{
	sel->is_reversed = 1 ;
	sel->is_selecting = type ;
	sel->is_rect = is_rect ;

	sel->base_col_r = sel->beg_col = sel->end_col = sel->prev_col = col_r ;
	sel->base_row_r = sel->beg_row = sel->end_row = sel->prev_row = row_r ;
	sel->base_col_l = col_l ;
	sel->base_row_l = row_l ;

	(*sel->sel_listener->reverse_color)( sel->sel_listener->self ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row ,
		sel->is_rect) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selection started => %d %d\n" , sel->beg_col , sel->beg_row) ;
#endif
	
	return  1 ;
}

int
x_selecting(
	x_selection_t *  sel ,
	int  col ,
	int  row
	)
{
	if( ! sel->is_selecting)
	{
		return  0 ;
	}

	sel->prev_col = col ;
	sel->prev_row = row ;

	update_sel_region( sel , col , row) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selecting %d %d => %d %d - %d %d.\n" ,
		col , row , sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;
#endif

	return  1 ;
}

int
x_stop_selecting(
	x_selection_t *  sel
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selection stops => %d %d - %d %d.\n" ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;
#endif

	if( ! sel->is_selecting)
	{
		return  0 ;
	}

	sel->is_selecting = 0 ;
	sel->is_locked = 0 ;

	if( sel->sel_str)
	{
		ml_str_delete( sel->sel_str , sel->sel_len) ;
	}

	if( ! (*sel->sel_listener->select_in_window)( sel->sel_listener->self ,
		&sel->sel_str , &sel->sel_len , sel->beg_col , sel->beg_row ,
		sel->end_col , sel->end_row , sel->is_rect))
	{
	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " select_in_window() failed.\n") ;
	#endif

		sel->sel_str = NULL ;
		sel->sel_len = 0 ;
		
		return  0 ;
	}

	return  1 ;
}

int
x_sel_clear(
	x_selection_t *  sel
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selection is cleared.\n") ;
#endif

	if( sel->is_selecting)
	{
		if( sel->sel_str)
		{
			ml_str_delete( sel->sel_str , sel->sel_len) ;
			sel->sel_str = NULL ;
			sel->sel_len = 0 ;
		}

		sel->is_selecting = 0 ;
		sel->is_locked = 0 ;
	}
	
	return  x_restore_selected_region_color( sel) ;
}

int
x_restore_selected_region_color_except_logs(
	x_selection_t *  sel
	)
{
	int  beg_row ;
	int  beg_col ;
	
	if( ! sel->is_reversed)
	{
		return  0 ;
	}

	if( sel->end_row < 0)
	{
		return  1 ;
	}
	
	if( ( beg_row = sel->beg_row) < 0)
	{
		beg_row = 0 ;
		beg_col = 0 ;
	}
	else
	{
		beg_col = sel->beg_col ;
	}
	
	(*sel->sel_listener->restore_color)( sel->sel_listener->self ,
		beg_col , beg_row , sel->end_col , sel->end_row , sel->is_rect) ;

	return  1 ;
}

int
x_reverse_selected_region_color_except_logs(
	x_selection_t *  sel
	)
{
	int  beg_row ;
	int  beg_col ;
	
	if( ! sel->is_reversed)
	{
		return  0 ;
	}

	if( sel->end_row < 0)
	{
		return  1 ;
	}
	
	if( ( beg_row = sel->beg_row) < 0)
	{
		beg_row = 0 ;
		beg_col = 0 ;
	}
	else
	{
		beg_col = sel->beg_col ;
	}

	(*sel->sel_listener->reverse_color)( sel->sel_listener->self ,
		beg_col , beg_row , sel->end_col , sel->end_row , sel->is_rect) ;

	return  1 ;
}

int
x_restore_selected_region_color(
	x_selection_t *  sel
	)
{
	if( ! sel->is_reversed)
	{
		return  0 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " restore selected region color => %d %d - %d %d.\n" ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;
#endif

	(*sel->sel_listener->restore_color)( sel->sel_listener->self ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row , sel->is_rect) ;

	sel->is_reversed = 0 ;

	return  1 ;
}

/*
 * Not used for now.
 */
#if  0
int
x_reverse_selected_region_color(
	x_selection_t *  sel
	)
{
	if( sel->is_reversed)
	{
		return  0 ;
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " reverse selected region color => %d %d - %d %d.\n" ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;
#endif

	(*sel->sel_listener->reverse_color)( sel->sel_listener->self ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row , sel->is_rect) ;

	sel->is_reversed = 1 ;

	return  1 ;
}
#endif

int
x_sel_line_scrolled_out(
	x_selection_t *  sel ,
	int  min_row
	)
{
	if( ! sel->is_selecting)
	{
		return  0 ;
	}

	if( sel->base_row_l > min_row)
	{
		sel->base_row_l -- ;
	}
	else
	{
		sel->base_col_l = -1 ;
	}

	if( sel->base_row_r > min_row)
	{
		sel->base_row_r -- ;
	}
	else
	{
		sel->base_col_r = 0 ;
	}

	if( sel->is_locked)
	{
		if( sel->lock_row > min_row)
		{
			sel->lock_row -- ;
		}
		else
		{
			sel->lock_col = 0 ;
		}
	}

	if( sel->beg_row > min_row)
	{
		sel->beg_row -- ;
	}
	else
	{
		sel->beg_col = 0 ;
	}

	if( sel->end_row > min_row)
	{
		sel->end_row -- ;
	}
	else
	{
		sel->end_col = 0 ;
	}

	if( sel->prev_row > min_row)
	{
		sel->prev_row -- ;
	}
	else
	{
		sel->prev_col = 0 ;
	}

	return  1 ;
}

int
x_selected_region_is_changed(
	x_selection_t *  sel ,
	int  col ,
	int  row ,
	u_int  base
	)
{
	if( abs( sel->prev_col - col) >= base || abs( sel->prev_row - row) >= base)
	{
		return  1 ;
	}
	else
	{
		return  0 ;
	}
}

int
x_is_after_sel_right_base_pos(
	x_selection_t *  sel ,
	int  col ,
	int  row
	)
{
	if( sel->is_rect)
	{
		return  sel->base_col_r < col ;
	}
	else
	{
		if( sel->base_row_r < row || (sel->base_row_r == row && sel->base_col_r < col))
		{
			return  1 ;
		}
		else
		{
			return  0 ;
		}
	}
}

int
x_is_before_sel_left_base_pos(
	x_selection_t *  sel ,
	int  col ,
	int  row
	)
{
	if( sel->is_rect)
	{
		return  sel->base_col_l > col ;
	}
	else
	{
		if( sel->base_row_l > row || (sel->base_row_l == row && sel->base_col_l > col))
		{
			return  1 ;
		}
		else
		{
			return  0 ;
		}
	}
}

int
x_sel_lock(
	x_selection_t *  sel
	)
{
	if( sel->beg_row < sel->base_row_l ||
	    ( sel->beg_row == sel->base_row_l && sel->beg_col <= sel->base_col_l) )
	{
		/*
		 * (Text surrounded by '*' is selected region. '|' is the base position.)
		 * aaa*bbb*|ccc
		 *     ^
		 *     +---- lock position ("bbb" is always selected.)
		 *
		 * This lock position is usually used in RTL lines.
		 */

		sel->lock_col = sel->beg_col ;
		sel->lock_row = sel->beg_row ;

		sel->is_locked = -1 ;
	}
	else
	{
		/*
		 * (Text surrounded by '*' is selected region. '|' is the base position.)
		 * aaa|*bbb*ccc
		 *        ^
		 *        +---- lock position ("bbb" is always selected.)
		 *
		 * This lock position is usually used in LTR lines.
		 */

		sel->lock_col = sel->end_col ;
		sel->lock_row = sel->end_row ;

		sel->is_locked = 1 ;
	}

	return  1 ;
}
