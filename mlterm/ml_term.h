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


typedef struct ml_term
{
	/*
	 * private
	 */
	ml_pty_ptr_t  pty ;
	ml_pty_event_listener_t *  pty_listener ; /* pool until pty opened. */
	
	ml_vt100_parser_t *  parser ;
	ml_screen_t *  screen ;
	ml_config_menu_t  config_menu ;

	/*
	 * public(read/write)
	 */
	ml_shape_t *  shape ;

	/*
	 * private
	 */
	ml_vertical_mode_t  vertical_mode ;
	ml_bidi_mode_t  bidi_mode ;

#ifdef  MULTI_WINDOWS_PER_PTY
	char *  win_id ;
#endif
	char *  win_name ;
	char *  icon_name ;
	char *  icon_path ;

	int8_t  use_bidi ;
	int8_t  use_ind ;
	int8_t  use_dynamic_comb ;

	int8_t  is_auto_encoding ;

	int8_t  is_attached ;

} ml_term_t ;


ml_term_t *  ml_term_new( u_int  cols , u_int  rows , u_int  tab_size , u_int  log_size ,
	ml_char_encoding_t  encoding , int  is_auto_encoding , ml_unicode_policy_t  policy ,
	int  col_size_a , int  use_char_combining , int  use_multi_col_char , int  use_bidi ,
	ml_bidi_mode_t  bidi_mode , int  use_ind , int  use_bce , int  use_dynamic_comb ,
	ml_bs_mode_t  bs_mode , ml_vertical_mode_t  vertical_mode) ;

int  ml_term_delete( ml_term_t *  term) ;

int  ml_term_zombie( ml_term_t *  term) ;

int  ml_term_open_pty( ml_term_t *  term , const char *  cmd_path , char ** argv , char **  env ,
	const char *  host , const char *  pass , const char *  pubkey , const char *  privkey) ;

int  ml_term_plug_pty( ml_term_t *  term , ml_pty_ptr_t  pty) ;

int  ml_term_attach( ml_term_t *  term , ml_xterm_event_listener_t *  xterm_listener ,
	ml_config_event_listener_t *  config_listener ,
	ml_screen_event_listener_t *  screen_listener ,
	ml_pty_event_listener_t *  pty_listner) ;

int  ml_term_detach( ml_term_t *  term) ;

#define  ml_term_is_attached( term)  ((term)->is_attached)

#define  ml_term_parse_vt100_sequence( term)  ml_parse_vt100_sequence( (term)->parser)

#define  ml_term_change_encoding( term , encoding) \
		ml_vt100_parser_change_encoding( (term)->parser , encoding)

#define  ml_term_get_encoding( term)  ml_vt100_parser_get_encoding( (term)->parser)

int  ml_term_set_auto_encoding( ml_term_t *  term , int  is_auto_encoding) ;

#define  ml_term_is_auto_encoding( term)  ((term)->is_auto_encoding)

#define  ml_term_set_unicode_policy( term , policy) \
		ml_vt100_parser_set_unicode_policy( (term)->parser , policy)

int  ml_term_set_use_bidi( ml_term_t *  term , int  flag) ;

#define  ml_term_is_using_bidi( term)  ((term)->use_bidi)

int  ml_term_set_bidi_mode( ml_term_t *  term , ml_bidi_mode_t  mode) ;

#define  ml_term_get_bidi_mode( term)  ((term)->bidi_mode)

int  ml_term_set_use_ind( ml_term_t *  term , int  flag) ;

#define  ml_term_is_using_ind( term)  ((term)->use_ind)

int  ml_term_set_vertical_mode( ml_term_t *  term , ml_vertical_mode_t  mode) ;

#define  ml_term_get_vertical_mode( term)  ((term)->vertical_mode)

int  ml_term_set_use_dynamic_comb( ml_term_t *  term , int  flag) ;

#define  ml_term_is_using_dynamic_comb( term)  ((term)->use_dynamic_comb)

#define  ml_term_convert_to( term , dst , len , _parser) \
		ml_vt100_parser_convert_to( (term)->parser , dst , len , _parser)

#define  ml_term_init_encoding_parser( term)  ml_init_encoding_parser( (term)->parser)

#define  ml_term_init_encoding_conv( term)  ml_init_encoding_conv( (term)->parser)

int  ml_term_get_master_fd( ml_term_t *  term) ;

int  ml_term_get_slave_fd( ml_term_t *  term) ;

char *  ml_term_get_slave_name( ml_term_t *  term) ;

pid_t  ml_term_get_child_pid( ml_term_t *  term) ;

size_t  ml_term_write( ml_term_t *  term , u_char *  buf , size_t  len , int  to_menu) ;

int  ml_term_flush( ml_term_t *  term) ;

/* Must be called in visual context. */
#define  ml_term_write_loopback( term , buf , len) \
		ml_parse_vt100_write_loopback( (term)->parser , buf , len)

#ifdef  MULTI_WINDOWS_PER_PTY
#define  ml_term_set_readable( term , is_readable) \
		((term)->pty && ml_pty_set_readable( (term)->pty , is_readable))

#define  ml_term_is_readable( term)  ((term)->pty && ml_pty_is_readable( (term)->pty))
#endif

int  ml_term_resize( ml_term_t *  term , u_int  cols , u_int  rows) ;

#define  ml_term_cursor_col( term)  ml_screen_cursor_col( (term)->screen)

#define  ml_term_cursor_char_index( term)  ml_screen_cursor_char_index( (term)->screen)

#define  ml_term_cursor_row( term)  ml_screen_cursor_row( (term)->screen)

#define  ml_term_cursor_row_in_screen( term)  ml_screen_cursor_row_in_screen( (term)->screen)

int  ml_term_unhighlight_cursor( ml_term_t *  term , int  revert_visual) ;

#define  ml_term_get_cols( term)  ml_screen_get_cols( (term)->screen)

#define  ml_term_get_rows( term)  ml_screen_get_rows( (term)->screen)

#define  ml_term_get_logical_cols( term)  ml_screen_get_logical_cols( (term)->screen)

#define  ml_term_get_logical_rows( term)  ml_screen_get_logical_rows( (term)->screen)

#define  ml_term_get_log_size( term)  ml_screen_get_log_size( (term)->screen)

#define  ml_term_change_log_size( term , log_size) \
		ml_screen_change_log_size( (term)->screen , log_size)

#define  ml_term_get_num_of_logged_lines( term)  ml_screen_get_num_of_logged_lines( (term)->screen)

#define  ml_term_convert_scr_row_to_abs( term , row) \
		ml_screen_convert_scr_row_to_abs( (term)->screen , row)

#define  ml_term_get_line( term , row)  ml_screen_get_line( term->screen , row)

#define  ml_term_get_line_in_screen( term , row) \
		ml_screen_get_line_in_screen( (term)->screen , row)

#define  ml_term_get_cursor_line( term)  ml_screen_get_cursor_line( (term)->screen)

#define  ml_term_is_cursor_visible( term)  ml_screen_is_cursor_visible( (term)->screen)

#if  0
int  ml_term_set_modified_region( ml_term_t *  term ,
	int  beg_char_index , int  beg_row , u_int  nchars , u_int  nrows) ;

int  ml_term_set_modified_region_in_screen( ml_term_t *  term ,
	int  beg_char_index , int  beg_row , u_int  nchars , u_int  nrows) ;
#endif

int  ml_term_set_modified_lines( ml_term_t *  term , u_int  beg , u_int  end) ;

int  ml_term_set_modified_lines_in_screen( ml_term_t *  term , u_int  beg , u_int  end) ;

int  ml_term_set_modified_all_lines_in_screen( ml_term_t *  term) ;

int  ml_term_updated_all( ml_term_t *  term) ;

int  ml_term_update_special_visual( ml_term_t *  term) ;

#define  ml_term_logical_visual_is_reversible( term) \
		ml_screen_logical_visual_is_reversible( (term)->screen)

#define  ml_term_is_backscrolling( term)  ml_screen_is_backscrolling( (term)->screen)

#define  ml_term_set_backscroll_mode( term , mode)  ml_set_backscroll_mode( (term)->screen , mode)

int  ml_term_enter_backscroll_mode( ml_term_t *  term) ;

#define  ml_term_exit_backscroll_mode( term)  ml_exit_backscroll_mode( (term)->screen)

#define  ml_term_backscroll_to( term , row)  ml_screen_backscroll_to( (term)->screen , row)

#define  ml_term_backscroll_upward( term , size) \
		ml_screen_backscroll_upward( (term)->screen , size)

#define  ml_term_backscroll_downward( term , size) \
		ml_screen_backscroll_downward( (term)->screen , size)

#define  ml_term_get_tab_size( term)  ml_screen_get_tab_size( (term)->screen)

#define  ml_term_set_tab_size( term , tab_size)  ml_screen_set_tab_size( (term)->screen , tab_size)

#define  ml_term_reverse_color( term , beg_char_index , beg_row , end_char_index , end_row) \
		ml_screen_reverse_color( (term)->screen , beg_char_index , beg_row , \
			end_char_index , end_row)

#define  ml_term_restore_color( term , beg_char_index , beg_row , end_char_index , end_row) \
		ml_screen_restore_color( (term)->screen , beg_char_index , beg_row , \
			end_char_index , end_row)

#define  ml_term_copy_region( term , chars , num_of_chars , beg_char_index , beg_row , \
		end_char_index , end_row) \
		ml_screen_copy_region( (term)->screen , chars , num_of_chars , \
			beg_char_index , beg_row , end_char_index , end_row)

#define  ml_term_get_region_size( term , beg_char_index , beg_row , end_char_index , end_row) \
		ml_screen_get_region_size( (term)->screen , beg_char_index , beg_row , \
			end_char_index , end_row)

#define  ml_term_get_line_region( term , beg_row , end_char_index , end_row , base_row) \
		ml_screen_get_line_region( (term)->screen , beg_row , end_char_index , \
			end_row , base_row)

#define  ml_term_get_word_region( term , beg_char_index , beg_row , end_char_index , end_row , \
		base_char_index , base_row) \
		ml_screen_get_word_region( (term)->screen , beg_char_index , beg_row , \
			end_char_index , end_row , base_char_index , base_row)

#define  ml_term_set_col_size_of_width_a( term , col_size_a) \
		ml_vt100_parser_set_col_size_of_width_a( (term)->parser , col_size_a)

#define  ml_term_get_col_size_of_width_a( term) \
		ml_vt100_parser_get_col_size_of_width_a( (term)->parser)

#define  ml_term_set_use_char_combining( term , flag) \
		ml_vt100_parser_set_use_char_combining( (term)->parser , flag)

#define  ml_term_is_using_char_combining( term) \
		ml_vt100_parser_is_using_char_combining((term)->parser)

#define  ml_term_set_use_multi_col_char( term , flag) \
		ml_vt100_parser_set_use_multi_col_char( (term)->parser , flag)

#define  ml_term_is_using_multi_col_char( term) \
		ml_vt100_parser_is_using_multi_col_char((term)->parser)

#define  ml_term_is_logging_vt_seq( term)  ml_vt100_parser_is_logging_vt_seq( (term)->parser)

#define  ml_term_set_logging_vt_seq( term , flag) \
		ml_vt100_parser_set_logging_vt_seq( (term)->parser , flag)

#define  ml_term_get_mouse_report_mode( term) \
		ml_vt100_parser_get_mouse_report_mode((term)->parser)

#define  ml_term_is_extended_mouse_report_mode( term) \
		ml_vt100_parser_is_extended_mouse_report_mode((term)->parser)

#define  ml_term_is_app_keypad( term) \
		ml_vt100_parser_is_app_keypad((term)->parser)

#define  ml_term_is_app_cursor_keys( term) \
		ml_vt100_parser_is_app_cursor_keys((term)->parser)

#define  ml_term_is_app_escape( term)  ml_vt100_parser_is_app_escape((term)->parser)

#ifdef  MULTI_WINDOWS_PER_PTY
int  ml_term_set_window_id( ml_term_t *  term , char *  name) ;

#define  ml_term_window_id( term)  ((term)->win_id)
#endif

int  ml_term_set_window_name( ml_term_t *  term , char *  name) ;

int  ml_term_set_icon_name( ml_term_t *  term , char *  name) ;

int  ml_term_set_icon_path( ml_term_t *  term , char *  path) ;

#define  ml_term_window_name( term)  ((term)->win_name)

#define  ml_term_icon_name( term)  ((term)->icon_name)

#define  ml_term_icon_path( term)  ((term)->icon_path)

#define  ml_term_is_bracketed_paste_mode( term) \
		ml_vt100_parser_is_bracketed_paste_mode((term)->parser)

int  ml_term_start_config_menu( ml_term_t *  term , char *  cmd_path ,
		int  x , int  y , char *  display) ;

#define  ml_term_search_init( term , match)  ml_screen_search_init( (term)->screen , match)

#define  ml_term_search_final( term)  ml_screen_search_final( (term)->screen)

#define  ml_term_search_reset_position( term)  ml_screen_search_reset_position( (term)->screen)

#define  ml_term_search_find( term , beg_char_index , beg_row , \
		end_char_index , end_row , regex , backward) \
		ml_screen_search_find( (term)->screen , beg_char_index , beg_row , \
			end_char_index , end_row , regex , backward)


/*
 * Defined in ml_term_manager.c, but declared here because x_screen.c uses
 * following functions.
 */
int  ml_set_auto_restart_cmd( char *  cmd) ;

#ifdef  MULTI_WINDOWS_PER_PTY
int  ml_term_sync_size( ml_term_t *  term , u_int  cols , u_int  rows) ;
#endif


#endif
