/*
 *	$Id$
 */

#ifndef  __ML_SELECTION_H__
#define  __ML_SELECTION_H__


#include  "ml_image.h"


typedef struct  ml_sel_event_listener
{
	void *  self ;
	int  (*select_in_window)( void * , ml_char_t ** , u_int *, int , int , int , int) ;
	void  (*reverse_color)( void * , int , int , int , int) ;
	void  (*restore_color)( void * , int , int , int , int) ;

} ml_sel_event_listener_t ;


typedef struct  ml_selection
{
	ml_sel_event_listener_t *  sel_listener ;

	ml_char_t *  sel_str ;
	u_int  sel_len ;

	int  base_col_l ;
	int  base_row_l ;
	int  base_col_r ;
	int  base_row_r ;
	int  beg_col ;
	int  beg_row ;
	int  end_col ;
	int  end_row ;

	int  prev_col ;
	int  prev_row ;
	
	int  is_selecting ;
	int  is_reversed ;
	int  is_owner ;

}  ml_selection_t ;


int  ml_sel_init( ml_selection_t *  sel , ml_sel_event_listener_t *  listener) ;

int  ml_sel_final( ml_selection_t *  sel) ;

int  ml_start_selection( ml_selection_t *  sel , int  col_l , int  row_l , int  col_r , int  row_r) ;

int  ml_selecting( ml_selection_t *  sel , int  col , int  row) ;

int  ml_stop_selecting( ml_selection_t *  sel) ;

int  ml_restore_selected_region_color( ml_selection_t *  sel) ;

int  ml_sel_clear( ml_selection_t *  sel) ;

int  ml_selected_region_is_changed( ml_selection_t *  sel , int  col , int  row , u_int  base) ;

int  ml_sel_upward_scrolled_out( ml_selection_t *  sel , u_int  height) ;


#endif
