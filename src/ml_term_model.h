/*
 *	$Id$
 */

#ifndef  __ML_TERM_MODEL_H__
#define  __ML_TERM_MODEL_H__


#include  <kiklib/kik_types.h>		/* int8_t */

#include  "ml_image.h"
#include  "ml_logs.h"
#include  "ml_logical_visual.h"


typedef struct  ml_term_model_event_listener
{
	void *  self ;
	
	int  (*window_scroll_upward_region)( void * , int , int , u_int) ;
	int  (*window_scroll_downward_region)( void * , int , int , u_int) ;
	void  (*scrolled_out_line_received)( void *) ;

}  ml_term_model_event_listener_t ;

typedef struct  ml_term_model
{
	/* public(readonly) */
	ml_image_t *  image ;

	/*
	 * private
	 */

	ml_image_t  normal_image ;
	ml_image_t  alt_image ;

	ml_image_scroll_event_listener_t  image_scroll_listener ;
	
	ml_logs_t  logs ;

	ml_logical_visual_t *  logvis ;
	ml_logical_visual_t *  container_logvis ;

	ml_term_model_event_listener_t *  termmdl_listener ;

	ml_char_t  nl_ch ;
	
	u_int  backscroll_rows ;
	int8_t  is_backscroll_mode ;

} ml_term_model_t ;


int  ml_set_word_separators( char *  seps) ;

int  ml_free_word_separators(void) ;


ml_term_model_t *  ml_term_model_new( ml_term_model_event_listener_t *  termmdl_listener ,
	u_int  cols , u_int  rows , ml_char_t *  sp_ch , ml_char_t *  nl_ch ,
	u_int  tab_size , u_int  num_of_log_lines) ;
	
int  ml_term_model_delete( ml_term_model_t *  termmdl) ;

int  ml_term_model_resize( ml_term_model_t *  termmdl , u_int  cols , u_int  rows) ;

u_int  ml_term_model_get_cols( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_get_rows( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_get_logical_cols( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_get_logical_rows( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_get_log_size( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_change_log_size( ml_term_model_t *  termmdl , u_int  log_size) ;

u_int  ml_term_model_get_num_of_logged_lines( ml_term_model_t *  termmdl) ;

int  ml_convert_row_to_scr_row( ml_term_model_t *  termmdl , int  row) ;

int  ml_convert_row_to_abs_row( ml_term_model_t *  termmdl , int  row) ;

ml_image_line_t *  ml_term_model_get_line( ml_term_model_t *  termmdl , int  row) ;

ml_image_line_t *  ml_term_model_get_line_in_screen( ml_term_model_t *  termmdl , int  row) ;

ml_image_line_t *  ml_term_model_get_line_in_all( ml_term_model_t *  termmdl , int  row) ;

ml_image_line_t *  ml_term_model_cursor_line( ml_term_model_t *  termmdl) ;

int  ml_term_model_delete_logical_visual( ml_term_model_t *  termmdl) ;

int  ml_term_model_add_logical_visual( ml_term_model_t *  termmdl , ml_logical_visual_t *  logvis) ;

int  ml_term_model_render( ml_term_model_t *  termmdl) ;

int  ml_term_model_visual( ml_term_model_t *  termmdl) ;

int  ml_term_model_logical( ml_term_model_t *  termmdl) ;

int  ml_is_backscroll_mode( ml_term_model_t *  termmdl) ;

int  ml_set_backscroll_mode( ml_term_model_t *  termmdl) ;

int  ml_unset_backscroll_mode( ml_term_model_t *  termmdl) ;

void  ml_term_model_set_modified_all( ml_term_model_t *  termmdl) ;


int  ml_term_model_backscroll_to( ml_term_model_t *  termmdl , int  row) ;

int  ml_term_model_backscroll_upward( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_backscroll_downward( ml_term_model_t *  termmdl , u_int  size) ;

u_int  ml_term_model_copy_region( ml_term_model_t *  termmdl , ml_char_t *  chars ,
	u_int  num_of_chars , int  beg_char_index , int  beg_row , int  end_char_index , int  end_row) ;

u_int  ml_term_model_get_region_size( ml_term_model_t *  termmdl , int  beg_char_index ,
	int  beg_row , int  end_char_index , int  end_row) ;

int  ml_term_model_get_line_region( ml_term_model_t *  termmdl , int *  beg_row ,
	int *  end_char_index , int *  end_row , int  base_row) ;

int  ml_term_model_get_word_region( ml_term_model_t *  termmdl , int *  beg_char_index ,
	int *  beg_row , int *  end_char_index , int *  end_row , int  base_char_index , int  base_row) ;


ml_char_t *  ml_term_model_get_n_prev_char( ml_term_model_t *  termmdl , int  n) ;

int  ml_term_model_combine_with_prev_char( ml_term_model_t *  termmdl , u_char *  bytes ,
	size_t  ch_size , ml_font_t *  font , ml_font_decor_t  font_decor , ml_color_t  fg_color ,
	ml_color_t  bg_color , int  is_comb) ;

int  ml_term_model_insert_chars( ml_term_model_t *  termmdl , ml_char_t *  chars , u_int  len) ;

int  ml_term_model_insert_blank_chars( ml_term_model_t *  termmdl , u_int  len) ;

int  ml_term_model_vertical_tab( ml_term_model_t *  termmdl) ;

int  ml_term_model_set_tab_stop( ml_term_model_t *  termmdl) ;

int  ml_term_model_clear_tab_stop( ml_term_model_t *  termmdl) ;

int  ml_term_model_clear_all_tab_stops( ml_term_model_t *  termmdl) ;

int  ml_term_model_insert_new_lines( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_line_feed( ml_term_model_t *  termmdl) ;

int  ml_term_model_overwrite_chars( ml_term_model_t *  termmdl , ml_char_t *  chars , u_int  len) ;

int  ml_term_model_delete_cols( ml_term_model_t *  termmdl , u_int  len) ;

int  ml_term_model_delete_lines( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_clear_line_to_right( ml_term_model_t *  termmdl) ;

int  ml_term_model_clear_line_to_left( ml_term_model_t *  termmdl) ;

int  ml_term_model_clear_below( ml_term_model_t *  termmdl) ;

int  ml_term_model_clear_above( ml_term_model_t *  termmdl) ;

int  ml_term_model_set_scroll_region( ml_term_model_t *  termmdl , int  beg , int  end) ;

int  ml_term_model_scroll_upward( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_scroll_downward( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_go_forward( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_go_back( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_go_upward( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_go_downward( ml_term_model_t *  termmdl , u_int  size) ;

int  ml_term_model_goto_beg_of_line( ml_term_model_t *  termmdl) ;

int  ml_term_model_go_horizontally( ml_term_model_t *  termmdl , int  col) ;

int  ml_term_model_go_vertically( ml_term_model_t *  termmdl , int  row) ;

int  ml_term_model_goto_home( ml_term_model_t *  termmdl) ;

int  ml_term_model_goto( ml_term_model_t *  termmdl , int  col , int  row) ;

int  ml_term_model_cursor_col( ml_term_model_t *  termmdl) ;

int  ml_term_model_cursor_char_index( ml_term_model_t *  termmdl) ;

int  ml_term_model_cursor_row( ml_term_model_t *  termmdl) ;

int  ml_term_model_save_cursor( ml_term_model_t *  termmdl) ;

int  ml_term_model_restore_cursor( ml_term_model_t *  termmdl) ;

int  ml_term_model_highlight_cursor( ml_term_model_t *  termmdl) ;

int  ml_term_model_unhighlight_cursor( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_get_tab_size( ml_term_model_t *  termmdl) ;

u_int  ml_term_model_set_tab_size( ml_term_model_t *  termmdl , u_int  tab_size) ;

int  ml_term_model_use_normal_image( ml_term_model_t *  termmdl) ;

int  ml_term_model_use_alternative_image( ml_term_model_t *  termmdl) ;

int  ml_term_model_fill_all( ml_term_model_t *  termmdl , ml_char_t *  ch) ;


#endif
