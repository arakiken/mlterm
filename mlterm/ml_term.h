/*
 *	$Id$
 */

/*
 * !! Notice !!
 * Don't provide any methods modifying ml_model_t and ml_logs_t states
 * unless these are logicalized in advance.
 */
 
#ifndef  __ML_TERM_H__
#define  __ML_TERM_H__


#include  "ml_pty.h"
#include  "ml_vt100_parser.h"
#include  "ml_screen.h"
#include  "ml_config_menu.h"


typedef struct ml_pty_event_listener
{
	void *  self ;
	void  (*closed)( void *) ;
	
} ml_pty_event_listener_t ;

typedef struct ml_term
{
	/*
	 * private
	 */
	ml_pty_t *  pty ;
	ml_pty_event_listener_t *  pty_listener ;
	
	ml_vt100_parser_t *  parser ;
	ml_screen_t *  screen ;
	ml_config_menu_t  config_menu ;

	/*
	 * public(read/write)
	 */
	ml_shape_t *  shape ;
	ml_iscii_lang_type_t  iscii_lang_type ;
	ml_iscii_lang_t  iscii_lang ;
	ml_vertical_mode_t  vertical_mode ;
	
	int8_t  use_bidi ;
	int8_t  use_dynamic_comb ;

	/*
	 * private
	 */
	char *  win_name ;
	char *  icon_name ;

	int8_t  is_auto_encoding ;
	
	int8_t  is_mouse_pos_sending ;
	int8_t  is_app_keypad ;
	int8_t  is_app_cursor_keys ;

	int8_t  is_attached ;

} ml_term_t ;


ml_term_t *  ml_term_new( u_int  cols , u_int  rows , u_int  tab_size , u_int  log_size ,
	ml_char_encoding_t  encoding , int  is_auto_encoding , ml_unicode_font_policy_t  policy ,
	int  col_size_a , int  use_char_combining , int  use_multi_col_char , int  use_bidi ,
	int  use_bce , int  use_dynamic_comb , ml_bs_mode_t  bs_mode ,
	ml_vertical_mode_t  vertical_mode , ml_iscii_lang_type_t  iscii_lang_type) ;

int  ml_term_delete( ml_term_t *  term) ;

int  ml_term_open_pty( ml_term_t *  term , char *  cmd_path , char **  argv , char **  env , char *  host) ;

int  ml_term_attach( ml_term_t *  term , ml_xterm_event_listener_t *  xterm_listener ,
	ml_config_event_listener_t *  config_listener , ml_screen_event_listener_t *  screen_listener ,
	ml_pty_event_listener_t *  pty_listner) ;

int  ml_term_detach( ml_term_t *  term) ;

int  ml_term_is_attached( ml_term_t *  term) ;

int  ml_term_parse_vt100_sequence( ml_term_t *  term) ;

int  ml_term_change_encoding( ml_term_t *  term , ml_char_encoding_t  encoding) ;

ml_char_encoding_t  ml_term_get_encoding( ml_term_t *  term) ;

int  ml_term_set_auto_encoding( ml_term_t *  term , int  is_auto_encoding) ;

int  ml_term_is_auto_encoding( ml_term_t *  term) ;

int  ml_term_set_unicode_font_policy( ml_term_t *  term , ml_unicode_font_policy_t  policy) ;

size_t  ml_term_convert_to( ml_term_t *  term , u_char *  dst , size_t  len , mkf_parser_t *  parser) ;

int  ml_term_init_encoding_parser( ml_term_t *  term) ;

int  ml_term_init_encoding_conv( ml_term_t *  term) ;

int  ml_term_enable_logging_vt_seq( ml_term_t *  term) ;

int  ml_term_get_pty_fd( ml_term_t *  term) ;

char *  ml_term_get_slave_name( ml_term_t *  term) ;

pid_t  ml_term_get_child_pid( ml_term_t *  term) ;

size_t  ml_term_write( ml_term_t *  term , u_char *  buf , size_t  len , int  to_menu) ;

int  ml_term_flush( ml_term_t *  term) ;

int  ml_term_resize( ml_term_t *  term , u_int  cols , u_int  rows) ;

int  ml_term_cursor_col( ml_term_t *  term) ;

int  ml_term_cursor_char_index( ml_term_t *  term) ;

int  ml_term_cursor_row( ml_term_t *  term) ;

int  ml_term_cursor_row_in_screen( ml_term_t *  term) ;

int  ml_term_unhighlight_cursor( ml_term_t *  term) ;

u_int  ml_term_get_cols( ml_term_t *  term) ;

u_int  ml_term_get_rows( ml_term_t *  term) ;

u_int  ml_term_get_logical_cols( ml_term_t *  term) ;

u_int  ml_term_get_logical_rows( ml_term_t *  term) ;

u_int  ml_term_get_log_size( ml_term_t *  term) ;

int  ml_term_change_log_size( ml_term_t *  term , u_int  log_size) ;

u_int  ml_term_get_num_of_logged_lines( ml_term_t *  term) ;

int  ml_term_convert_scr_row_to_abs( ml_term_t *  term , int  row) ;

ml_line_t *  ml_term_get_line( ml_term_t *  term , int  row) ;

ml_line_t *  ml_term_get_line_in_screen( ml_term_t *  term , int  row) ;

ml_line_t *  ml_term_get_cursor_line( ml_term_t *  term) ;

int  ml_term_set_modified_lines_in_screen( ml_term_t *  term , u_int  beg , u_int  end) ;

int  ml_term_set_modified_lines( ml_term_t *  term , u_int  beg , u_int  end) ;

int  ml_term_set_modified_all( ml_term_t *  term) ;

int  ml_term_update_special_visual( ml_term_t *  term) ;

ml_bs_mode_t  ml_term_is_backscrolling( ml_term_t *  term) ;

int  ml_term_set_backscroll_mode( ml_term_t *  term , ml_bs_mode_t  mode) ;

int  ml_term_enter_backscroll_mode( ml_term_t *  term) ;

int  ml_term_exit_backscroll_mode( ml_term_t *  term) ;

int  ml_term_backscroll_to( ml_term_t *  term , int  row) ;

int  ml_term_backscroll_upward( ml_term_t *  term , u_int  size) ;

int  ml_term_backscroll_downward( ml_term_t *  term , u_int  size) ;

u_int  ml_term_get_tab_size( ml_term_t *  term) ;

int  ml_term_set_tab_size( ml_term_t *  term , u_int  size) ;

int  ml_term_reverse_color( ml_term_t *  term , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

int  ml_term_restore_color( ml_term_t *  term , int  beg_char_index , int  beg_row ,
	int  end_char_index , int  end_row) ;

u_int  ml_term_copy_region( ml_term_t *  term , ml_char_t *  chars ,
	u_int  num_of_chars , int  beg_char_index , int  beg_row , int  end_char_index , int  end_row) ;

u_int  ml_term_get_region_size( ml_term_t *  term , int  beg_char_index ,
	int  beg_row , int  end_char_index , int  end_row) ;

int  ml_term_get_line_region( ml_term_t *  term , int *  beg_row ,
	int *  end_char_index , int *  end_row , int  base_row) ;

int  ml_term_get_word_region( ml_term_t *  term , int *  beg_char_index ,
	int *  beg_row , int *  end_char_index , int *  end_row , int  base_char_index , int  base_row) ;

int  ml_term_set_char_combining_flag( ml_term_t *  term , int  flag) ;

int  ml_term_is_using_char_combining( ml_term_t *  term) ;

int  ml_term_set_multi_col_char_flag( ml_term_t *  term , int  flag) ;

int  ml_term_is_using_multi_col_char( ml_term_t *  term) ;

int  ml_term_set_mouse_report( ml_term_t *  term , int  flag) ;

int  ml_term_is_mouse_pos_sending( ml_term_t *  term) ;

int  ml_term_set_app_keypad( ml_term_t *  term , int  flag) ;

int  ml_term_is_app_keypad( ml_term_t *  term) ;

int  ml_term_set_app_cursor_keys( ml_term_t *  term , int  flag) ;

int  ml_term_is_app_cursor_keys( ml_term_t *  term) ;

int  ml_term_set_window_name( ml_term_t *  term , char *  name) ;

int  ml_term_set_icon_name( ml_term_t *  term , char *  name) ;

char *  ml_term_window_name( ml_term_t *  term) ;

char *  ml_term_icon_name( ml_term_t *  term) ;

int  ml_term_start_config_menu( ml_term_t *  term , char *  cmd_path , int  x , int  y , char *  display) ;


#endif
