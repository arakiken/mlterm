/*
 *	$Id$
 */

#ifndef  __X_MAIN_CONFIG_H__
#define  __X_MAIN_CONFIG_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_conf.h>
#include  <ml_term.h>

#include  "x_sb_screen.h"


typedef struct x_main_config
{
	/*
	 * Public (read only)
	 */

	int  x ;
	int  y ;
	int  geom_hint ;
	u_int  cols ;
	u_int  rows ;
	u_int  font_size ;
	u_int  tab_size ;
	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;
	u_int  num_of_log_lines ;
	x_mod_meta_mode_t  mod_meta_mode ;
	x_bel_mode_t  bel_mode ;
	x_sb_mode_t  sb_mode ;
	ml_char_encoding_t  encoding ;
	int  is_auto_encoding ;
	x_type_engine_t  type_engine ;
	x_font_present_t  font_present ;
	ml_bidi_mode_t  bidi_mode ;
	ml_vertical_mode_t  vertical_mode ;
	ml_bs_mode_t  bs_mode ;
	ml_unicode_policy_t  unicode_policy ;
	u_int  parent_window ;

	char *  disp_name ;
	char *  app_name ;
	char *  title ;
	char *  icon_name ;
	char *  term_type ;
	char *  scrollbar_view_name ;
	char *  pic_file_path ;
	/* BACKWARD COMPAT (3.1.7 or before) */
#if  1
	char *  shortcut_strs[4] ;
#endif
	char *  fg_color ;
	char *  bg_color ;
	char *  cursor_fg_color ;
	char *  cursor_bg_color ;
	char *  bd_color ;
	char *  it_color ;
	char *  ul_color ;
	char *  bl_color ;
	char *  co_color ;
	char *  sb_fg_color ;
	char *  sb_bg_color ;
	char *  mod_meta_key ;
	char *  icon_path ;
	char *  input_method ;
	char *  init_str ;
	char *  bidi_separators ;
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	char **  server_list ;
	char *  default_server ;
#endif
#ifdef  USE_LIBSSH2
	char *  public_key ;
	char *  private_key ;
#endif
	char *  cmd_path ;
	char **  cmd_argv ;
	
	u_int16_t  brightness ;
	u_int16_t  contrast ;
	u_int16_t  gamma ;
	u_int8_t  col_size_of_width_a ;
	u_int8_t  step_in_changing_font_size ;
	u_int8_t  alpha ;
	u_int8_t  fade_ratio ;
	u_int8_t  line_space ;
	u_int8_t  letter_space ;
	int8_t  use_scrollbar ;
	int8_t  use_login_shell ;
	int8_t  use_bidi ;
	int8_t  use_ind ;
	int8_t  big5_buggy ;
	int8_t  iso88591_font_for_usascii ;
	int8_t  receive_string_via_ucs ;
	int8_t  use_transbg ;
	int8_t  use_char_combining ;
	int8_t  use_multi_col_char ;
	int8_t  use_vertical_cursor ;
	int8_t  use_extended_scroll_shortcut ;
	int8_t  borderless ;
	int8_t  use_dynamic_comb ;
	int8_t  logging_vt_seq ;
	int8_t  allow_osc52 ;
	int8_t  blink_cursor ;
	u_int8_t  hmargin ;
	u_int8_t  vmargin ;
	int8_t  hide_underline ;
	int8_t  use_bold_font ;
	int8_t  use_italic_font ;
	int8_t  use_local_echo ;
	int8_t  use_x11_forwarding ;
	int8_t  use_auto_detect ;
	int8_t  unlimit_log_size ;
	int8_t  alt_color_mode ;
#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
	int8_t  show_dialog ;
#endif

} x_main_config_t ;


int  x_prepare_for_main_config( kik_conf_t *  conf) ;

int  x_main_config_init( x_main_config_t *  main_config , kik_conf_t *  conf , int  argc , char **  argv) ;

int  x_main_config_final( x_main_config_t *  main_config) ;

#if  defined(USE_WIN32API) || defined(USE_LIBSSH2)
int  x_main_config_add_to_server_list( x_main_config_t *  main_config , char *  server) ;
#endif


#endif
