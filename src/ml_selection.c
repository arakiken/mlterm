/*
 *	$Id$
 */

#include  "ml_selection.h"

#include  <stdio.h>
#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


#if  0
#define  __DEBUG
#endif


/* --- static functions --- */

static void
reset_sel_region(
	ml_selection_t *  sel
	)
{
	sel->base_col_r = 0 ;
	sel->base_row_r = 0 ;
	sel->base_col_l = 0 ;
	sel->base_row_l = 0 ;
	sel->beg_col = 0 ;
	sel->beg_row = 0 ;
	sel->end_col = 0 ;
	sel->end_row = 0 ;
}

static int
update_sel_region(
	ml_selection_t *  sel ,
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
			kik_debug_printf( "HELO\n") ;
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
		sel->sel_listener->reverse_color( sel->sel_listener->self ,
			rv_beg_col , rv_beg_row , rv_end_col , rv_end_row) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " reversing %d %d %d %d\n" , rv_beg_col , rv_beg_row ,
			rv_end_col , rv_end_row) ;
	#endif
	}

	if( do_restore)
	{
		sel->sel_listener->restore_color( sel->sel_listener->self ,
			rs_beg_col , rs_beg_row , rs_end_col , rs_end_row) ;

	#ifdef  __DEBUG
		kik_debug_printf( KIK_DEBUG_TAG " restoring %d %d %d %d\n" , rs_beg_col , rs_beg_row ,
			rs_end_col , rs_end_row) ;
	#endif
	}

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " current region %d %d %d %d\n" ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;
#endif
	
	return  1 ;
}


/* --- global functions --- */

int
ml_sel_init(
	ml_selection_t *  sel ,
	ml_sel_event_listener_t *  sel_listener
	)
{
	sel->sel_listener = sel_listener ;
	sel->base_col_r = 0 ;
	sel->base_row_r = 0 ;
	sel->base_col_l = 0 ;
	sel->base_row_l = 0 ;
	sel->beg_col = 0 ;
	sel->beg_row = 0 ;
	sel->end_col = 0 ;
	sel->end_row = 0 ;
	sel->prev_col = 0 ;
	sel->prev_row = 0 ;
	
	sel->is_reversed = 0 ;
	sel->is_selecting = 0 ;
	sel->is_owner = 0 ;
	
	sel->sel_str = NULL ;
	sel->sel_len = 0 ;

	return  1 ;
}

int
ml_sel_final(
	ml_selection_t *  sel
	)
{
	if( sel->sel_str)
	{
		ml_str_delete( sel->sel_str , sel->sel_len) ;
	}
			
	return  1 ;
}

int
ml_start_selection(
	ml_selection_t *  sel ,
	int  col_l ,
	int  row_l ,
	int  col_r ,
	int  row_r
	)
{
	sel->is_owner = 1 ;
	sel->is_reversed = 1 ;
	sel->is_selecting = 1 ;

	sel->base_col_r = col_r ;
	sel->base_row_r = row_r ;
	sel->base_col_l = col_l ;
	sel->base_row_l = row_l ;
	sel->beg_col = col_r ;
	sel->end_col = col_r ;
	sel->beg_row = row_r ;
	sel->end_row = row_r ;
	sel->prev_col = col_r ;
	sel->prev_row = row_r ;

	sel->sel_listener->reverse_color( sel->sel_listener->self ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;

#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selection started from %d %d\n" , col , row) ;
#endif
	
	return  1 ;
}

int
ml_selecting(
	ml_selection_t *  sel ,
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

	return  1 ;
}

int
ml_stop_selecting(
	ml_selection_t *  sel
	)
{
#ifdef  __DEBUG
	kik_debug_printf( KIK_DEBUG_TAG " selection stops.\n") ;
#endif

	if( ! sel->is_selecting)
	{
		return  0 ;
	}

	if( sel->sel_str)
	{
		ml_str_delete( sel->sel_str , sel->sel_len) ;
	}

	sel->is_selecting = 0 ;

	if( ! sel->sel_listener->select_in_window( sel->sel_listener->self ,
		&sel->sel_str , &sel->sel_len , sel->beg_col , sel->beg_row , sel->end_col , sel->end_row))
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
ml_sel_clear(
	ml_selection_t *  sel
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
	}
	
	sel->is_owner = 0 ;

	return  1 ;
}

int
ml_restore_selected_region_color(
	ml_selection_t *  sel
	)
{
	if( ! sel->is_reversed)
	{
		return  0 ;
	}

	ml_stop_selecting( sel) ;

	sel->sel_listener->restore_color( sel->sel_listener->self ,
		sel->beg_col , sel->beg_row , sel->end_col , sel->end_row) ;

	sel->is_reversed = 0 ;

	reset_sel_region( sel) ;
	
	return  1 ;
}

int
ml_selected_region_is_changed(
	ml_selection_t *  sel ,
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
ml_is_after_sel_right_base_pos(
	ml_selection_t *  sel ,
	int  col ,
	int  row
	)
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

int
ml_is_before_sel_left_base_pos(
	ml_selection_t *  sel ,
	int  col ,
	int  row
	)
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
