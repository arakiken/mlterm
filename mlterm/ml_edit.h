/*
 *	$Id$
 */

#ifndef  __ML_EDIT_H__
#define  __ML_EDIT_H__


#include  <kiklib/kik_types.h>

#include  "ml_str.h"
#include  "ml_line.h"
#include  "ml_model.h"
#include  "ml_cursor.h"


typedef struct ml_edit_scroll_event_listener
{
	void *  self ;

	void (*receive_scrolled_out_line)( void * , ml_line_t *) ;
	void (*scrolled_out_lines_finished)( void *) ;
	
	int  (*window_scroll_upward_region)( void * , int , int , u_int) ;
	int  (*window_scroll_downward_region)( void * , int , int , u_int) ;

} ml_edit_scroll_event_listener_t ;

typedef struct  ml_edit
{
	ml_model_t  model ;
	ml_cursor_t  cursor ;

	u_int  tab_size ;
	u_int8_t *  tab_stops ;

	ml_char_t  bce_ch ;

	/* used for line overlapping */
	ml_line_t *  wraparound_ready_line ;

	int  scroll_region_beg ;
	int  scroll_region_end ;

	ml_edit_scroll_event_listener_t *  scroll_listener ;

	int8_t  is_logging ;
	int8_t  is_relative_origin ;
	int8_t  is_auto_wrap ;
	int8_t  use_bce ;

} ml_edit_t ;


int  ml_edit_init( ml_edit_t *  edit , ml_edit_scroll_event_listener_t *  scroll_listener ,
	u_int  num_of_cols , u_int  num_of_rows , u_int  tab_size , int  is_logging ,
	int  use_bce) ;

int  ml_edit_final( ml_edit_t *  edit) ;

int  ml_edit_resize( ml_edit_t *  edit , u_int  num_of_cols , u_int  num_of_rows) ;

int  ml_edit_insert_chars( ml_edit_t *  edit , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_edit_insert_blank_chars( ml_edit_t *  edit , u_int  num_of_blank_chars) ;

int  ml_edit_overwrite_chars( ml_edit_t *  edit , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_edit_delete_cols( ml_edit_t *  edit , u_int  delete_cols) ;

int  ml_edit_clear_cols( ml_edit_t *  edit , u_int  cols) ;

int  ml_edit_insert_new_line( ml_edit_t *  edit) ;

int  ml_edit_delete_line( ml_edit_t *  edit) ;

int  ml_edit_clear_line_to_right( ml_edit_t *  edit) ;

int  ml_edit_clear_line_to_left( ml_edit_t *  edit) ;

int  ml_edit_clear_below( ml_edit_t *  edit) ;

int  ml_edit_clear_above( ml_edit_t *  edit) ;

int  ml_edit_fill_all( ml_edit_t *  edit , ml_char_t *  ch) ;
	
int  ml_edit_set_scroll_region( ml_edit_t *  edit , int  beg , int  end) ;

int  ml_edit_scroll_upward( ml_edit_t *  edit , u_int  size) ;

int  ml_edit_scroll_downward( ml_edit_t *  edit , u_int  size) ;

int  ml_edit_vertical_forward_tabs( ml_edit_t *  edit , u_int  num) ;

int  ml_edit_vertical_backward_tabs( ml_edit_t *  edit , u_int  num) ;

#define  ml_edit_get_tab_size( edit)  ((edit)->tab_size)

int  ml_edit_set_tab_size( ml_edit_t *  edit , u_int  tab_size) ;

int  ml_edit_set_tab_stop( ml_edit_t *  edit) ;

int  ml_edit_clear_tab_stop( ml_edit_t *  edit) ;

int  ml_edit_clear_all_tab_stops( ml_edit_t *  edit) ;

#define  ml_edit_get_line( edit , row)  (ml_model_get_line( &(edit)->model , row))

int  ml_edit_set_modified_all( ml_edit_t *  edit) ;

#define  ml_edit_get_cols( edit)  ((edit)->model.num_of_cols)

#define  ml_edit_get_rows( edit)  ((edit)->model.num_of_rows)

int  ml_edit_go_forward( ml_edit_t *  edit , int  flag) ;

int  ml_edit_go_back( ml_edit_t *  edit , int  flag) ;

int  ml_edit_go_upward( ml_edit_t *  edit , int  flag) ;

int  ml_edit_go_downward( ml_edit_t *  edit , int  flag) ;

int  ml_edit_goto_beg_of_line( ml_edit_t *  edit) ;

int  ml_edit_goto_home( ml_edit_t *  edit) ;

int  ml_edit_goto( ml_edit_t *  edit , int  col , int  row) ;

int  ml_edit_set_relative_origin( ml_edit_t *  edit) ;

int  ml_edit_set_absolute_origin( ml_edit_t *  edit) ;

int  ml_edit_set_auto_wrap( ml_edit_t *  edit , int  flag) ;

int  ml_edit_set_bce_fg_color( ml_edit_t *  edit , ml_color_t  fg_color) ;

int  ml_edit_set_bce_bg_color( ml_edit_t *  edit , ml_color_t  bg_color) ;

int  ml_edit_save_cursor( ml_edit_t *  edit) ;

int  ml_edit_restore_cursor( ml_edit_t *  edit) ;

#define  ml_cursor_char_index( edit)  ((edit)->cursor.char_index)

#define  ml_cursor_col( edit)  ((edit)->cursor.col)

#define  ml_cursor_row( edit)  ((edit)->cursor.row)

#ifdef  DEBUG

void  ml_edit_dump( ml_edit_t *  edit) ;

void  ml_edit_dump_updated( ml_edit_t *  edit) ;

#endif


#endif
