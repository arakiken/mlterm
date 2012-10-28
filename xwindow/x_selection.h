/*
 *	$Id$
 */

#ifndef  __X_SELECTION_H__
#define  __X_SELECTION_H__


#include  <kiklib/kik_types.h>		/* u_int */
#include  <ml_str.h>


typedef enum
{
	SEL_CHAR = 1 ,
	SEL_WORD = 2 ,
	SEL_LINE = 3 ,

} x_sel_type_t ;

typedef struct  x_sel_event_listener
{
	void *  self ;
	int  (*select_in_window)( void * , ml_char_t ** , u_int *, int , int , int , int) ;
	void  (*reverse_color)( void * , int , int , int , int) ;
	void  (*restore_color)( void * , int , int , int , int) ;

} x_sel_event_listener_t ;

typedef struct  x_selection
{
	x_sel_event_listener_t *  sel_listener ;

	ml_char_t *  sel_str ;
	u_int  sel_len ;

	/*
	 * Be careful that value of col must be munis in rtl line.
	 * +-----------------------------+
	 * |          a  a  a  a  a  a  a|<= RTL line
	 *           -1 -2 -3 -4 -5 -6 -7 <= index
	 */

	int  base_col_l ;
	int  base_row_l ;
	int  base_col_r ;
	int  base_row_r ;
	int  beg_col ;
	int  beg_row ;
	int  end_col ;
	int  end_row ;
	int  lock_col ;
	int  lock_row ;

	int  prev_col ;
	int  prev_row ;

	int8_t  is_selecting ;	/* x_sel_type_t is stored */
	int8_t  is_reversed ;
	int8_t  is_locked ;

}  x_selection_t ;


int  x_sel_init( x_selection_t *  sel , x_sel_event_listener_t *  listener) ;

int  x_sel_final( x_selection_t *  sel) ;

int  x_start_selection( x_selection_t *  sel , int  col_l , int  row_l ,
		int  col_r , int  row_r , x_sel_type_t  type) ;

int  x_selecting( x_selection_t *  sel , int  col , int  row) ;

int  x_stop_selecting( x_selection_t *  sel) ;

int  x_restore_selected_region_color_except_logs( x_selection_t *  sel) ;

int  x_reverse_selected_region_color_except_logs( x_selection_t *  sel) ;

int  x_restore_selected_region_color( x_selection_t *  sel) ;

int  x_reverse_selected_region_color( x_selection_t *  sel) ;

int  x_sel_clear( x_selection_t *  sel) ;

int  x_selected_region_is_changed( x_selection_t *  sel , int  col , int  row , u_int  base) ;

int  x_sel_line_scrolled_out( x_selection_t *  sel , int  min_row) ;

#define  x_is_selecting( sel)  ((sel)->is_selecting)

#define  x_sel_is_reversed( sel)  ((sel)->is_reversed)

int  x_is_after_sel_right_base_pos( x_selection_t *  sel , int  col , int  row) ;

int  x_is_before_sel_left_base_pos( x_selection_t *  sel , int  col , int  row) ;

int  x_sel_lock( x_selection_t *  sel) ;


#endif
