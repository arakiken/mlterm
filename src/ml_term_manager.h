/*
 *	$Id$
 */

/*
 * ANTI_ALIAS macro should be concerned.
 */

#ifndef  __ML_TERM_MANAGER_H__
#define  __ML_TERM_MANAGER_H__


#include  <kiklib/kik_types.h>	/* u_int */

#include  "ml_pty.h"
#include  "ml_vt100_parser.h"
#include  "ml_font_manager.h"
#include  "ml_color_manager.h"
#include  "ml_window_manager.h"
#include  "ml_term_screen.h"	/* ml_system_event_listener_t ... */


typedef struct ml_term
{
	ml_pty_t *  pty ;
	ml_window_t *  root_window ;
	ml_vt100_parser_t *  vt100_parser ;
	ml_font_manager_t *  font_man ;

} ml_term_t ;

typedef struct  ml_term_manager
{
	ml_window_manager_t  win_man ;
	ml_color_manager_t  color_man ;
	ml_font_custom_t  normal_font_custom ;
	ml_font_custom_t  v_font_custom ;
	ml_font_custom_t  t_font_custom ;
#ifdef  ANTI_ALIAS
	ml_font_custom_t  aa_font_custom ;
	ml_font_custom_t  vaa_font_custom ;
	ml_font_custom_t  taa_font_custom ;
#endif
	ml_keymap_t  keymap ;
	ml_termcap_t  termcap ;

	ml_system_event_listener_t  system_listener ;
	
	int  x ;
	int  y ;
	int  geom_hint ;
	u_int  cols ;
	u_int  rows ;
	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;
	char *  app_name ;
	char *  title ;
	char *  icon_name ;
	char *  term_type ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	ml_color_t  sb_fg_color ;
	ml_color_t  sb_bg_color ;
	u_int  num_of_log_lines ;
	u_int  font_size ;
	u_int  line_space ;
	u_int  tab_size ;
	char *  scrollbar_view_name ;
	char *  pic_file_path ;
	char *  conf_menu_path ;
	ml_iscii_lang_t  iscii_lang ;
	ml_mod_meta_mode_t  mod_meta_mode ;
	ml_bel_mode_t  bel_mode ;
	ml_sb_mode_t  sb_mode ;
	u_int  num_of_startup_terms ;
	u_int  col_size_a ;
	ml_char_encoding_t  encoding ;
	ml_font_present_t  font_present ;
	ml_vertical_mode_t  vertical_mode ;

	u_int8_t  step_in_changing_font_size ;
	u_int8_t  brightness ;
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
	
	char *  cmd_path ;
	char **  cmd_argv ;
	
	ml_term_t *  terms ;
	u_int  max_terms ;
	u_int  num_of_terms ;
	
	/* 32 is the very max of terminals */
	u_int32_t  dead_mask ;

} ml_term_manager_t ;


int  ml_term_manager_init( ml_term_manager_t *  term_man , int  argc , char **  argv) ;

int  ml_term_manager_final( ml_term_manager_t *  term_man) ;

void  ml_term_manager_event_loop( ml_term_manager_t *  term_man) ;


#endif
