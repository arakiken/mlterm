/*
 *	$Id$
 */

/*
 * Managing Terminal Window.
 */

#ifndef  __ML_TERM_SCREEN_H__
#define  __ML_TERM_SCREEN_H__


#include  <mkf/mkf_parser.h>

#include  "ml_image.h"
#include  "ml_window.h"
#include  "ml_logs.h"
#include  "ml_selection.h"
#include  "ml_backscroll.h"
#include  "ml_char_encoding.h"
#include  "ml_font_manager.h"
#include  "ml_color.h"
#include  "ml_keymap.h"
#include  "ml_termcap.h"
#include  "ml_config_menu.h"
#include  "ml_mod_meta_mode.h"
#include  "ml_bel_mode.h"
#include  "ml_pty.h"


typedef struct  ml_pty_encoding_event_listener
{
	void *  self ;

	int  (*encoding_changed)( void * , ml_char_encoding_t) ;
	ml_char_encoding_t  (*encoding)( void *) ;
	size_t  (*convert)( void * , u_char *  dst , size_t  len , mkf_parser_t *) ;
	int  (*init)( void * , int) ;
	
} ml_pty_encoding_event_listener_t ;

typedef struct  ml_system_event_listener
{
	void *  self ;

	void  (*open_pty)( void *) ;
	void  (*close_pty)( void * , ml_window_t *) ;
	
	/* for debug */
	void  (*exit)( void * , int) ;

} ml_system_event_listener_t ;

typedef struct  ml_screen_scroll_event_listener
{
	void *  self ;

	void  (*bs_mode_exited)( void *) ;
	void  (*bs_mode_entered)( void *) ;
	void  (*scrolled_upward)( void * , u_int) ;
	void  (*scrolled_downward)( void * , u_int) ;
	void  (*log_size_changed)( void * , u_int) ;
	void  (*line_height_changed)( void * , u_int) ;
	void  (*transparent_state_changed)( void * , int) ;

} ml_screen_scroll_event_listener_t ;

typedef struct  ml_term_screen
{
	ml_window_t  window ;
	
	ml_image_t *  image ;
	
	ml_image_t  normal_image ;
	ml_image_t  alt_image ;
	
	ml_logs_t  logs ;
	ml_selection_t  sel ;
	ml_bs_image_t  bs_image ;
	ml_config_menu_t  config_menu ;

	ml_sel_event_listener_t  sel_listener ;
	ml_image_scroll_event_listener_t  image_scroll_listener ;
	ml_xim_event_listener_t  xim_listener ;
	ml_bs_event_listener_t  bs_listener ;
	ml_config_menu_event_listener_t  config_menu_listener ;

	ml_pty_t *  pty ;
	
	ml_font_manager_t *  font_man ;

	ml_keymap_t *  keymap ;
	ml_termcap_t *  termcap ;

	u_int  mod_meta_mask ;
	ml_mod_meta_mode_t  mod_meta_mode ;

	ml_bel_mode_t  bel_mode ;

	ml_pty_encoding_event_listener_t *  encoding_listener ;
	ml_system_event_listener_t *  system_listener ;
	ml_screen_scroll_event_listener_t *  screen_scroll_listener ;

	mkf_parser_t *  xct_parser ;
	mkf_parser_t *  utf8_parser ;
	
	mkf_parser_t *  ml_str_parser ;
	mkf_conv_t *  utf8_conv ;
	mkf_conv_t *  xct_conv ;
	
	int  scroll_cache_rows ;
	int  scroll_cache_boundary_start ;
	int  scroll_cache_boundary_end ;

	char *  pic_file_path ;

	int8_t  copy_paste_via_ucs ;
	int8_t  is_reverse ;
	int8_t  is_cursor_visible ;
	int8_t  is_app_keypad ;
	int8_t  is_app_cursor_keys ;
	int8_t  is_mouse_pos_sending ;
	int8_t  xim_open_in_startup ;
	int8_t  is_aa ;
	int8_t  use_bidi ;
	
} ml_term_screen_t ;


ml_term_screen_t *  ml_term_screen_new( u_int  cols , u_int  rows ,
	ml_font_manager_t *  font_man , ml_color_table_t  color_table ,
	ml_keymap_t *  keymap , ml_termcap_t *  termcap ,
	u_int  num_of_log_lines , u_int  tab_size , int  use_xim ,
	int  xim_open_in_startup , ml_mod_meta_mode_t  mod_meta_mode , ml_bel_mode_t  bel_mode ,
	int  prefer_utf8_selection , char *  pic_file_path , int  use_transbg , int  is_aa ,
	int  use_bidi , int  big5_buggy , char *  conf_menu_path) ;

int  ml_term_screen_delete( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_pty( ml_term_screen_t *  termscr , ml_pty_t *  pty) ;

int  ml_set_encoding_listener( ml_term_screen_t *  termscr ,
	ml_pty_encoding_event_listener_t *  encoding_listener) ;
	
int  ml_set_system_listener( ml_term_screen_t *  termscr ,
	ml_system_event_listener_t *  system_listener) ;

int  ml_set_screen_scroll_listener( ml_term_screen_t *  termscr ,
	ml_screen_scroll_event_listener_t *  screen_scroll_listener) ;
	
int  ml_convert_row_to_y( ml_term_screen_t *  termscr , int  row) ;

int  ml_convert_y_to_row( ml_term_screen_t *  termscr , int *  y_rest , int  y) ;

int  ml_term_screen_set_scroll_region( ml_term_screen_t *  termscr , int  beg , int  end) ;

int  ml_term_screen_scroll_upward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_scroll_downward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_scroll_to( ml_term_screen_t *  termscr , int  row) ;

int  ml_term_screen_redraw_image( ml_term_screen_t *  termscr) ;

int  ml_term_screen_highlight_cursor( ml_term_screen_t *  termscr) ;

int  ml_term_screen_unhighlight_cursor( ml_term_screen_t *  termscr) ;

int  ml_term_screen_combine_with_prev_char( ml_term_screen_t *  termscr , u_char *  bytes ,
	size_t  ch_size , ml_font_t *  font , ml_font_decor_t  font_decor ,
	ml_color_t  fg_color , ml_color_t  bg_color) ;

ml_font_t *  ml_term_screen_get_font( ml_term_screen_t *  termscr , ml_font_attr_t  attr) ;

int  ml_term_screen_render_bidi( ml_term_screen_t *  termscr) ;

int  ml_term_screen_start_bidi( ml_term_screen_t *  termscr) ;

int  ml_term_screen_stop_bidi( ml_term_screen_t *  termscr) ;


/*
 * for VT100 commands.
 */
 
int  ml_term_screen_insert_chars( ml_term_screen_t *  termscr , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_term_screen_insert_blank_chars( ml_term_screen_t *  termscr , u_int  len) ;

int  ml_term_screen_vertical_tab( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_tab_stop( ml_term_screen_t *  termscr) ;

int  ml_term_screen_clear_tab_stop( ml_term_screen_t *  termscr) ;

int  ml_term_screen_clear_all_tab_stops( ml_term_screen_t *  termscr) ;
	
int  ml_term_screen_insert_new_lines( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_line_feed( ml_term_screen_t *  termscr) ;

int  ml_term_screen_overwrite_chars( ml_term_screen_t *  termscr ,
	ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_term_screen_delete_cols( ml_term_screen_t *  termscr , u_int  delete_len) ;

int  ml_term_screen_delete_lines( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_clear_line_to_right( ml_term_screen_t *  termscr) ;

int  ml_term_screen_clear_line_to_left( ml_term_screen_t *  termscr) ;

int  ml_term_screen_clear_below( ml_term_screen_t *  termscr) ;

int  ml_term_screen_clear_above( ml_term_screen_t *  termscr) ;

int  ml_term_screen_scroll_image_upward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_scroll_image_downward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_go_forward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_go_back( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_go_upward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_go_downward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_goto_beg_of_line( ml_term_screen_t *  termscr) ;

int  ml_term_screen_goto_end_of_line( ml_term_screen_t *  termscr) ;

int  ml_term_screen_goto_home( ml_term_screen_t *  termscr) ;

int  ml_term_screen_go_horizontally( ml_term_screen_t *  termscr , int  col) ;

int  ml_term_screen_go_vertically( ml_term_screen_t *  termscr , int  row) ;

int  ml_term_screen_goto( ml_term_screen_t *  termscr , int  col , int  row) ;

int  ml_term_screen_save_cursor( ml_term_screen_t *  termscr) ;

int  ml_term_screen_restore_cursor( ml_term_screen_t *  termscr) ;

int  ml_term_screen_is_beg_of_line( ml_term_screen_t *  termscr) ;

int  ml_term_screen_is_end_of_line( ml_term_screen_t *  termscr) ;

u_int ml_term_screen_get_cols( ml_term_screen_t *  termscr) ;

u_int ml_term_screen_get_rows( ml_term_screen_t *  termscr) ;

int  ml_term_screen_bel( ml_term_screen_t *  termscr) ;

int  ml_term_screen_reverse_video( ml_term_screen_t *  termscr) ;

int  ml_term_screen_restore_video( ml_term_screen_t *  termscr) ;

int  ml_term_screen_resize_columns( ml_term_screen_t *  termscr , u_int  cols) ;

int  ml_term_screen_set_app_keypad( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_normal_keypad( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_app_cursor_keys( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_normal_cursor_keys( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_mouse_pos_sending( ml_term_screen_t *  termscr) ;

int  ml_term_screen_unset_mouse_pos_sending( ml_term_screen_t *  termscr) ;

int  ml_term_screen_use_normal_image( ml_term_screen_t *  termscr) ;

int  ml_term_screen_use_alternative_image( ml_term_screen_t *  termscr) ;

int  ml_term_screen_cursor_visible( ml_term_screen_t *  termscr) ;

int  ml_term_screen_cursor_invisible( ml_term_screen_t *  termscr) ;

int  ml_term_screen_send_device_attr( ml_term_screen_t *  termscr) ;

int  ml_term_screen_report_device_status( ml_term_screen_t *  termscr) ;

int  ml_term_screen_report_cursor_position( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_window_name( ml_term_screen_t *  termscr , u_char *  name) ;

int  ml_term_screen_set_icon_name( ml_term_screen_t *  termscr , u_char *  name) ;

int  ml_term_screen_fill_all_with_e( ml_term_screen_t *  termscr) ;


#endif
