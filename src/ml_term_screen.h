/*
 *	$Id$
 */

#ifndef  __ML_TERM_SCREEN_H__
#define  __ML_TERM_SCREEN_H__


#include  <mkf/mkf_parser.h>
#include  <kiklib/kik_types.h>		/* u_int/int8_t/size_t */

#include  "ml_image.h"
#include  "ml_logical_visual.h"
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
#include  "ml_sb_mode.h"
#include  "ml_pty.h"
#include  "ml_shaping.h"
#include  "ml_iscii.h"


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
	void  (*change_fg_color)( void * , ml_color_t) ;
	ml_color_t  (*fg_color)( void *) ;
	void  (*change_bg_color)( void * , ml_color_t) ;
	ml_color_t  (*bg_color)( void *) ;
	void  (*change_view)( void * , char *) ;
	char *  (*view_name)( void *) ;
	void  (*transparent_state_changed)( void * , int , ml_picture_modifier_t *) ;
	ml_sb_mode_t  (*sb_mode)( void *) ;
	void  (*change_sb_mode)( void * , ml_sb_mode_t) ;

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

	/*
	 * encoding proper aux
	 */
	ml_logical_visual_t *  logvis ;
	ml_shape_t *  shape ;
	ml_iscii_lang_t  iscii_lang ;
	ml_iscii_state_t  iscii_state ;

	u_int  mod_meta_mask ;
	ml_mod_meta_mode_t  mod_meta_mode ;

	ml_bel_mode_t  bel_mode ;

	ml_font_present_t  font_present ;

	ml_vertical_mode_t  vertical_mode ;
	
	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;

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
	ml_picture_modifier_t  pic_mod ;

	u_int8_t  brightness ;
	u_int8_t  fade_ratio ;
	int8_t  is_focused ;
	int8_t  copy_paste_via_ucs ;
	int8_t  is_reverse ;
	int8_t  is_cursor_visible ;
	int8_t  is_app_keypad ;
	int8_t  is_app_cursor_keys ;
	int8_t  is_mouse_pos_sending ;
	int8_t  xim_open_in_startup ;
	int8_t  use_bidi ;
	int8_t  use_vertical_cursor ;
	
} ml_term_screen_t ;


ml_term_screen_t *  ml_term_screen_new( u_int  cols , u_int  rows ,
	ml_font_manager_t *  font_man , ml_color_table_t  color_table ,
	u_int  brightness , u_int  fade_ratio ,
	ml_keymap_t *  keymap , ml_termcap_t *  termcap ,
	u_int  num_of_log_lines , u_int  tab_size ,
	u_int  screen_width_ratio , u_int  screen_height_ratio ,
	int  xim_open_in_startup , ml_mod_meta_mode_t  mod_meta_mode ,
	ml_bel_mode_t  bel_mode , int  prefer_utf8_selection , char *  pic_file_path ,
	int  use_transbg , ml_font_present_t  font_present , int  use_bidi ,
	ml_vertical_mode_t  vertical_mode , int  use_vertical_cursor , int  big5_buggy ,
	char *  conf_menu_path , ml_iscii_lang_t  iscii_lang) ;

int  ml_term_screen_delete( ml_term_screen_t *  termscr) ;

ml_picture_modifier_t *  ml_term_screen_get_picture_modifier( ml_term_screen_t *  termscr) ;

int  ml_term_screen_set_pty( ml_term_screen_t *  termscr , ml_pty_t *  pty) ;

int  ml_set_system_listener( ml_term_screen_t *  termscr ,
	ml_system_event_listener_t *  system_listener) ;

int  ml_set_encoding_listener( ml_term_screen_t *  termscr ,
	ml_pty_encoding_event_listener_t *  encoding_listener) ;

int  ml_set_screen_scroll_listener( ml_term_screen_t *  termscr ,
	ml_screen_scroll_event_listener_t *  screen_scroll_listener) ;

	
int  ml_term_screen_scroll_upward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_scroll_downward( ml_term_screen_t *  termscr , u_int  size) ;

int  ml_term_screen_scroll_to( ml_term_screen_t *  termscr , int  row) ;


u_int  ml_term_screen_get_cols( ml_term_screen_t *  termscr) ;

u_int  ml_term_screen_get_rows( ml_term_screen_t *  termscr) ;


int  ml_term_screen_start_vt100_cmd( ml_term_screen_t *  termscr) ;

int  ml_term_screen_stop_vt100_cmd( ml_term_screen_t *  termscr) ;

ml_font_t *  ml_term_screen_get_font( ml_term_screen_t *  termscr , ml_font_attr_t  attr) ;


#endif
