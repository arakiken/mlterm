/*
 *	update: <2001/11/16(14:21:55)>
 *	$Id$
 */

#ifndef  __ML_TERM_MANAGER_H__
#define  __ML_TERM_MANAGER_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_conf.h>

#include  "ml_pty.h"
#include  "ml_vt100_parser.h"
#include  "ml_font_manager.h"
#include  "ml_color_manager.h"
#include  "ml_window_manager.h"
#include  "ml_keymap.h"
#include  "ml_termcap.h"
#include  "ml_encoding.h"


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
#ifdef  ANTI_ALIAS
	ml_font_custom_t  aa_font_custom ;
#endif
	ml_keymap_t  keymap ;
	ml_termcap_t  termcap ;

	ml_system_event_listener_t  system_listener ;
	
	int  x ;
	int  y ;
	u_int  cols ;
	u_int  rows ;
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
	u_int  tab_size ;
	int  use_scrollbar ;
	char *  scrollbar_view_name ;
	int  use_login_shell ;
	int  use_xim ;
	int  is_aa ;
	int  xim_open_in_startup ;
	ml_mod_meta_mode_t  mod_meta_mode ;
	int  unicode_to_other_cs ;
	int  all_cs_to_unicode ;
	int  conv_to_generic_iso2022 ;
	int  pre_conv_xct_to_ucs ;
	int  col_size_a ;
	char *  pic_file_path ;
	int  use_transbg ;
	char *  conf_menu_path ;
	ml_encoding_type_t  encoding ;
	char *  cmd_path ;
	char **  cmd_argv ;
	u_int  num_of_startup_terms ;
	
	ml_term_t  terms[5] ;
	u_int  num_of_terms ;
	
	/* lower 5 bits are used */
	int  dead_mask ;

} ml_term_manager_t ;


int  ml_term_manager_init( ml_term_manager_t *  term_man , int  argc , char **  argv) ;
	
int  ml_term_manager_final( ml_term_manager_t *  term_man) ;

void  ml_term_manager_event_loop( ml_term_manager_t *  term_man) ;


#endif
