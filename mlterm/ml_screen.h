/*
 *	$Id$
 */

#ifndef  __ML_SCREEN_H__
#define  __ML_SCREEN_H__


#include  <kiklib/kik_types.h>		/* int8_t */

#include  "ml_edit.h"
#include  "ml_logs.h"
#include  "ml_logical_visual.h"


typedef struct  ml_screen_event_listener
{
	void *  self ;
	
	int  (*window_scroll_upward_region)( void * , int , int , u_int) ;
	int  (*window_scroll_downward_region)( void * , int , int , u_int) ;
	void  (*line_scrolled_out)( void *) ;

}  ml_screen_event_listener_t ;

typedef enum  ml_bs_mode
{
	BSM_NONE = 0x0 ,
	BSM_VOLATILE ,
	BSM_STATIC ,

} ml_bs_mode_t ;

typedef struct  ml_screen
{
	/* public(readonly) */
	ml_edit_t *  edit ;

	/*
	 * private
	 */

	ml_edit_t  normal_edit ;
	ml_edit_t  alt_edit ;

	ml_edit_scroll_event_listener_t  edit_scroll_listener ;
	
	ml_logs_t  logs ;

	ml_logical_visual_t *  logvis ;
	ml_logical_visual_t *  container_logvis ;

	ml_screen_event_listener_t *  screen_listener ;

	u_int  backscroll_rows ;
	ml_bs_mode_t  backscroll_mode ;
	int8_t  is_backscrolling ;

	int8_t  use_dynamic_comb ;	/* public */
	int8_t  use_bce ;
	int8_t  is_cursor_visible ;

} ml_screen_t ;


int  ml_set_word_separators( char *  seps) ;

int  ml_free_word_separators(void) ;


ml_screen_t *  ml_screen_new( u_int  cols , u_int  rows , u_int  tab_size ,
	u_int  num_of_log_lines , int  use_bce , ml_bs_mode_t  bs_mode) ;

int  ml_screen_delete( ml_screen_t *  screen) ;

int  ml_screen_set_listener( ml_screen_t *  screen , ml_screen_event_listener_t *  screen_listener) ;

int  ml_screen_resize( ml_screen_t *  screen , u_int  cols , u_int  rows) ;

int  ml_screen_use_bce( ml_screen_t *  screen) ;

int  ml_screen_unuse_bce( ml_screen_t *  screen) ;

int  ml_screen_set_bce_fg_color( ml_screen_t *  screen , ml_color_t  fg_color) ;

int  ml_screen_set_bce_bg_color( ml_screen_t *  screen , ml_color_t  bg_color) ;

int  ml_screen_cursor_col( ml_screen_t *  screen) ;

int  ml_screen_cursor_char_index( ml_screen_t *  screen) ;

int  ml_screen_cursor_row( ml_screen_t *  screen) ;

int  ml_screen_cursor_row_in_screen( ml_screen_t *  screen) ;

int  ml_screen_highlight_cursor( ml_screen_t *  screen) ;

int  ml_screen_unhighlight_cursor( ml_screen_t *  screen) ;

u_int  ml_screen_get_cols( ml_screen_t *  screen) ;

u_int  ml_screen_get_rows( ml_screen_t *  screen) ;

u_int  ml_screen_get_logical_cols( ml_screen_t *  screen) ;

u_int  ml_screen_get_logical_rows( ml_screen_t *  screen) ;

u_int  ml_screen_get_log_size( ml_screen_t *  screen) ;

u_int  ml_screen_change_log_size( ml_screen_t *  screen , u_int  log_size) ;

u_int  ml_screen_get_num_of_logged_lines( ml_screen_t *  screen) ;

int  ml_screen_convert_scr_row_to_abs( ml_screen_t *  screen , int  row) ;

ml_line_t *  ml_screen_get_line( ml_screen_t *  screen , int  row) ;

ml_line_t *  ml_screen_get_line_in_screen( ml_screen_t *  screen , int  row) ;

ml_line_t *  ml_screen_get_cursor_line( ml_screen_t *  screen) ;

int  ml_screen_set_modified_all( ml_screen_t *  screen) ;

int  ml_screen_add_logical_visual( ml_screen_t *  screen , ml_logical_visual_t *  logvis) ;

int  ml_screen_delete_logical_visual( ml_screen_t *  screen) ;

int  ml_screen_render( ml_screen_t *  screen) ;

int  ml_screen_visual( ml_screen_t *  screen) ;

int  ml_screen_logical( ml_screen_t *  screen) ;

ml_bs_mode_t  ml_is_backscroll_mode( ml_screen_t *  screen) ;

int  ml_set_backscroll_mode( ml_screen_t *  screen , ml_bs_mode_t  mode) ;

int  ml_enter_backscroll_mode( ml_screen_t *  screen) ;

int  ml_exit_backscroll_mode( ml_screen_t *  screen) ;

int  ml_screen_backscroll_to( ml_screen_t *  screen , int  row) ;

int  ml_screen_backscroll_upward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_backscroll_downward( ml_screen_t *  screen , u_int  size) ;

u_int  ml_screen_get_tab_size( ml_screen_t *  screen) ;

int  ml_screen_set_tab_size( ml_screen_t *  screen , u_int  tab_size) ;

int  ml_screen_restore_color( ml_screen_t *  screen , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

int  ml_screen_reverse_color( ml_screen_t *  screen , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

u_int  ml_screen_copy_region( ml_screen_t *  screen , ml_char_t *  chars ,
	u_int  num_of_chars , int  beg_char_index , int  beg_row , int  end_char_index , int  end_row) ;

u_int  ml_screen_get_region_size( ml_screen_t *  screen , int  beg_char_index ,
	int  beg_row , int  end_char_index , int  end_row) ;

int  ml_screen_get_line_region( ml_screen_t *  screen , int *  beg_row ,
	int *  end_char_index , int *  end_row , int  base_row) ;

int  ml_screen_get_word_region( ml_screen_t *  screen , int *  beg_char_index ,
	int *  beg_row , int *  end_char_index , int *  end_row , int  base_char_index , int  base_row) ;


/*
 * VT100 commands
 */
 
ml_char_t *  ml_screen_get_n_prev_char( ml_screen_t *  screen , int  n) ;

int  ml_screen_combine_with_prev_char( ml_screen_t *  screen , u_char *  bytes ,
	size_t  ch_size , mkf_charset_t  cs , int  is_biwidth , int  is_comb ,
	ml_color_t  fg_color , ml_color_t  bg_color , int  is_bold , int  is_underlined) ;

int  ml_screen_insert_chars( ml_screen_t *  screen , ml_char_t *  chars , u_int  len) ;

int  ml_screen_insert_blank_chars( ml_screen_t *  screen , u_int  len) ;

int  ml_screen_vertical_forward_tabs( ml_screen_t *  screen , u_int  num) ;

int  ml_screen_vertical_backward_tabs( ml_screen_t *  screen , u_int  num) ;

int  ml_screen_set_tab_stop( ml_screen_t *  screen) ;

int  ml_screen_clear_tab_stop( ml_screen_t *  screen) ;

int  ml_screen_clear_all_tab_stops( ml_screen_t *  screen) ;

int  ml_screen_insert_new_lines( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_line_feed( ml_screen_t *  screen) ;

int  ml_screen_overwrite_chars( ml_screen_t *  screen , ml_char_t *  chars , u_int  len) ;

int  ml_screen_delete_cols( ml_screen_t *  screen , u_int  len) ;

int  ml_screen_delete_lines( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_clear_cols( ml_screen_t *  screen , u_int  cols) ;

int  ml_screen_clear_line_to_right( ml_screen_t *  screen) ;

int  ml_screen_clear_line_to_left( ml_screen_t *  screen) ;

int  ml_screen_clear_below( ml_screen_t *  screen) ;

int  ml_screen_clear_above( ml_screen_t *  screen) ;

int  ml_screen_set_scroll_region( ml_screen_t *  screen , int  beg , int  end) ;

int  ml_screen_index( ml_screen_t *  screen) ;

int  ml_screen_reverse_index( ml_screen_t *  screen) ;

int  ml_screen_scroll_upward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_scroll_downward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_forward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_back( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_upward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_go_downward( ml_screen_t *  screen , u_int  size) ;

int  ml_screen_goto_beg_of_line( ml_screen_t *  screen) ;

int  ml_screen_go_horizontally( ml_screen_t *  screen , int  col) ;

int  ml_screen_go_vertically( ml_screen_t *  screen , int  row) ;

int  ml_screen_goto_home( ml_screen_t *  screen) ;

int  ml_screen_goto( ml_screen_t *  screen , int  col , int  row) ;

int  ml_screen_save_cursor( ml_screen_t *  screen) ;

int  ml_screen_restore_cursor( ml_screen_t *  screen) ;

int  ml_screen_cursor_visible( ml_screen_t *  screen) ;

int  ml_screen_cursor_invisible( ml_screen_t *  screen) ;

int  ml_screen_use_normal_edit( ml_screen_t *  screen) ;

int  ml_screen_use_alternative_edit( ml_screen_t *  screen) ;

int  ml_screen_fill_all_with_e( ml_screen_t *  screen) ;


#endif
