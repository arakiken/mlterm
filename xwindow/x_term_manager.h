/*
 *	$Id$
 */

/*
 * ANTI_ALIAS macro should be concerned.
 */

#ifndef  __X_TERM_MANAGER_H__
#define  __X_TERM_MANAGER_H__


#include  <kiklib/kik_types.h>	/* u_int */
#include  <ml_term.h>

#include  "x_display.h"
#include  "x_font_manager.h"
#include  "x_color_manager.h"
#include  "x_window_manager.h"
#include  "x_screen.h"	/* x_system_event_listener_t */
#include  "x_termcap.h"


typedef struct x_term
{
	x_display_t *  display ;
	
	x_window_t *  root_window ;
	x_font_manager_t *  font_man ;
	x_color_manager_t *  color_man ;

	ml_term_t *  term ;

} x_term_t ;

typedef struct x_config
{
	int  x ;
	int  y ;
	int  geom_hint ;
	u_int  cols ;
	u_int  rows ;
	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;
	u_int  font_size ;
	u_int  num_of_log_lines ;
	u_int  line_space ;
	u_int  tab_size ;
	ml_iscii_lang_t  iscii_lang ;
	x_mod_meta_mode_t  mod_meta_mode ;
	x_bel_mode_t  bel_mode ;
	x_sb_mode_t  sb_mode ;
	u_int  col_size_a ;
	ml_char_encoding_t  encoding ;
	x_font_present_t  font_present ;
	ml_vertical_mode_t  vertical_mode ;

	char *  disp_name ;
	char *  app_name ;
	char *  title ;
	char *  icon_name ;
	char *  term_type ;
	char *  scrollbar_view_name ;
	char *  pic_file_path ;
	char *  conf_menu_path ;
	char *  fg_color ;
	char *  bg_color ;
	char *  sb_fg_color ;
	char *  sb_bg_color ;
	char *  cmd_path ;
	char **  cmd_argv ;
	
	u_int8_t  step_in_changing_font_size ;
	u_int16_t  brightness ;
	u_int8_t  fade_ratio ;
	int8_t  use_scrollbar ;
	int8_t  use_login_shell ;
	int8_t  xim_open_in_startup ;
	int8_t  use_bidi ;
	int8_t  big5_buggy ;
	int8_t  iso88591_font_for_usascii ;
	int8_t  not_use_unicode_font ;
	int8_t  only_use_unicode_font ;
	int8_t  copy_paste_via_ucs ;
	int8_t  use_transbg ;
	int8_t  use_char_combining ;
	int8_t  use_multi_col_char ;
	int8_t  use_vertical_cursor ;
	int8_t  use_extended_scroll_shortcut ;
	int8_t  use_dynamic_comb ;

} x_config_t ;

typedef struct  x_term_manager
{
	x_display_t **  displays ;
	u_int  num_of_displays ;
	
	x_term_t *  terms ;
	u_int  max_terms ;
	u_int  num_of_terms ;
	u_int  num_of_startup_terms ;
	
	/* 32 is the very max of terminals */
	u_int32_t  dead_mask ;

	x_system_event_listener_t  system_listener ;

	x_config_t  conf ;
	
	x_font_custom_t  normal_font_custom ;
	x_font_custom_t  v_font_custom ;
	x_font_custom_t  t_font_custom ;
#ifdef  ANTI_ALIAS
	x_font_custom_t  aa_font_custom ;
	x_font_custom_t  vaa_font_custom ;
	x_font_custom_t  taa_font_custom ;
#endif
	x_color_custom_t  color_custom ;
	x_keymap_t  keymap ;
	x_termcap_t  termcap ;

	int  sock_fd ;
	
	int8_t  is_genuine_daemon ;
	
} x_term_manager_t ;


int  x_term_manager_init( x_term_manager_t *  term_man , int  argc , char **  argv) ;

int  x_term_manager_final( x_term_manager_t *  term_man) ;

void  x_term_manager_event_loop( x_term_manager_t *  term_man) ;


#endif
