/*
 *	$Id$
 */

#ifndef  __ML_VT100_COMMAND_H__
#define  __ML_VT100_COMMAND_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "ml_term_screen.h"


int  ml_vt100_cmd_start( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_stop( ml_term_screen_t *  termscr) ;


/*
 * VT100 commands.
 */

int  ml_vt100_cmd_combine_with_prev_char( ml_term_screen_t *  termscr , u_char *  bytes ,
	size_t  ch_size , ml_font_t *  font , ml_font_decor_t  font_decor ,
	ml_color_t  fg_color , ml_color_t  bg_color) ;

int  ml_vt100_cmd_insert_chars( ml_term_screen_t *  termscr , ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_vt100_cmd_insert_blank_chars( ml_term_screen_t *  termscr , u_int  len) ;

int  ml_vt100_cmd_vertical_tab( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_tab_stop( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_clear_tab_stop( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_clear_all_tab_stops( ml_term_screen_t *  termscr) ;
	
int  ml_vt100_cmd_insert_new_lines( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_line_feed( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_overwrite_chars( ml_term_screen_t *  termscr ,
	ml_char_t *  chars , u_int  num_of_chars) ;

int  ml_vt100_cmd_delete_cols( ml_term_screen_t *  termscr , u_int  delete_len) ;

int  ml_vt100_cmd_delete_lines( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_clear_line_to_right( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_clear_line_to_left( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_clear_below( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_clear_above( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_scroll_region( ml_term_screen_t *  termscr , int  beg , int  end) ;

int  ml_vt100_cmd_scroll_upward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_scroll_downward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_go_forward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_go_back( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_go_upward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_go_downward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_vt100_cmd_goto_beg_of_line( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_goto_end_of_line( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_goto_home( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_go_horizontally( ml_term_screen_t *  termscr , int  col) ;

int  ml_vt100_cmd_go_vertically( ml_term_screen_t *  termscr , int  row) ;

int  ml_vt100_cmd_goto( ml_term_screen_t *  termscr , int  col , int  row) ;

int  ml_vt100_cmd_save_cursor( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_restore_cursor( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_bel( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_reverse_video( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_restore_video( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_resize_columns( ml_term_screen_t *  termscr , u_int  cols) ;

int  ml_vt100_cmd_set_app_keypad( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_normal_keypad( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_app_cursor_keys( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_normal_cursor_keys( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_mouse_pos_sending( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_unset_mouse_pos_sending( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_use_normal_image( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_use_alternative_image( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_cursor_visible( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_cursor_invisible( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_send_device_attr( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_report_device_status( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_report_cursor_position( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_window_name( ml_term_screen_t *  termscr , u_char *  name) ;

int  ml_vt100_cmd_set_icon_name( ml_term_screen_t *  termscr , u_char *  name) ;

int  ml_vt100_cmd_fill_all_with_e( ml_term_screen_t *  termscr) ;

int  ml_vt100_cmd_set_config( ml_term_screen_t *  termscr , char *  key , char *  value) ;

int  ml_vt100_cmd_get_config( ml_term_screen_t *  termscr , char *  key) ;


#endif
