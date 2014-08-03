/*
 *	$Id$
 */

#ifndef  __X_SCREEN_H__
#define  __X_SCREEN_H__


#include  <stdio.h>			/* FILE */
#include  <mkf/mkf_parser.h>
#include  <kiklib/kik_types.h>		/* u_int/int8_t/size_t */
#include  <ml_term.h>

#include  "x_window.h"
#include  "x_selection.h"
#include  "x_shortcut.h"
#include  "x_termcap.h"
#include  "x_mod_meta_mode.h"
#include  "x_bel_mode.h"
#include  "x_sb_mode.h"
#include  "x_im.h"
#include  "x_picture.h"


typedef struct x_screen *  x_screen_ptr_t ;

typedef struct x_system_event_listener
{
	void *  self ;

	void  (*open_screen)( void * , x_screen_ptr_t) ;
	void  (*close_screen)( void * , x_screen_ptr_t) ;
	void  (*open_pty)( void * , x_screen_ptr_t , char *) ;
	void  (*next_pty)( void * , x_screen_ptr_t) ;
	void  (*prev_pty)( void * , x_screen_ptr_t) ;
	void  (*close_pty)( void * , x_screen_ptr_t , char *) ;
	
	void  (*pty_closed)( void * , x_screen_ptr_t) ;

	int  (*mlclient)( void * , x_screen_ptr_t , char * , FILE *) ;

	void  (*font_config_updated)(void) ;
	void  (*color_config_updated)(void) ;

	/* for debug */
	void  (*exit)( void * , int) ;

} x_system_event_listener_t ;

typedef struct  x_screen_scroll_event_listener
{
	void *  self ;

	void  (*bs_mode_entered)( void *) ;
	void  (*bs_mode_exited)( void *) ;
	void  (*scrolled_upward)( void * , u_int) ;
	void  (*scrolled_downward)( void * , u_int) ;
	void  (*scrolled_to)( void * , int) ;
	void  (*log_size_changed)( void * , u_int) ;
	void  (*line_height_changed)( void * , u_int) ;
	void  (*change_fg_color)( void * , char *) ;
	char *  (*fg_color)( void *) ;
	void  (*change_bg_color)( void * , char *) ;
	char *  (*bg_color)( void *) ;
	void  (*change_view)( void * , char *) ;
	char *  (*view_name)( void *) ;
	void  (*transparent_state_changed)( void * , int , x_picture_modifier_t *) ;
	x_sb_mode_t  (*sb_mode)( void *) ;
	void  (*change_sb_mode)( void * , x_sb_mode_t) ;
	void  (*term_changed)( void * , u_int , u_int) ;

} x_screen_scroll_event_listener_t ;

typedef struct  x_screen
{
	x_window_t  window ;

	x_font_manager_t *  font_man ;
	
	x_color_manager_t *  color_man ;

	ml_term_t *  term ;
	
	x_selection_t  sel ;

	ml_screen_event_listener_t  screen_listener ;
	ml_xterm_event_listener_t  xterm_listener ;
	ml_config_event_listener_t  config_listener ;
	ml_pty_event_listener_t  pty_listener ;

	x_sel_event_listener_t  sel_listener ;
	x_xim_event_listener_t  xim_listener ;
	x_im_event_listener_t  im_listener ;

	x_shortcut_t *  shortcut ;
	x_termcap_entry_t *  termcap ;

	char *  input_method ;
	x_im_t *  im ;
	int  is_preediting ;
	u_int  im_preedit_beg_row ;
	u_int  im_preedit_end_row ;

	char *  mod_meta_key ;
	x_mod_meta_mode_t  mod_meta_mode ;
	u_int  mod_meta_mask ;
	u_int  mod_ignore_mask ;

	x_bel_mode_t  bel_mode ;

	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;

	x_system_event_listener_t *  system_listener ;
	x_screen_scroll_event_listener_t *  screen_scroll_listener ;

	mkf_parser_t *  xct_parser ;
	mkf_parser_t *  utf_parser ;	/* UTF8 in X, UTF16 in Win32. */
	
	mkf_parser_t *  ml_str_parser ;
	mkf_conv_t *  utf_conv ;	/* UTF8 in X, UTF16 in Win32. */
	mkf_conv_t *  xct_conv ;
	
	int  scroll_cache_rows ;
	int  scroll_cache_boundary_start ;
	int  scroll_cache_boundary_end ;

	char *  pic_file_path ;
	x_picture_modifier_t  pic_mod ;
	x_picture_t *  bg_pic ;

	x_icon_picture_t *  icon ;

	int16_t  prev_inline_pic ;

	char  prev_mouse_report_seq[5] ;

	u_int8_t  fade_ratio ;
	u_int8_t  line_space ;
	int8_t  receive_string_via_ucs ;
	int8_t  use_vertical_cursor ;
	int8_t  use_extended_scroll_shortcut ;
	int8_t  borderless ;
	int8_t  font_or_color_config_updated ;	/* 0x1 = font updated, 0x2 = color updated */
	int8_t  blink_wait ;
	int8_t  blink_cursor ;
	int8_t  hide_underline ;
	int8_t  processing_vtseq ;
	int8_t  anim_wait ;

} x_screen_t ;


void  x_exit_backscroll_by_pty( int  flag) ;

void  x_allow_change_shortcut( int  flag) ;

void  x_set_mod_meta_prefix( char *  prefix) ;

#ifdef  USE_IM_CURSOR_COLOR
void  x_set_im_cursor_color( char *  color) ;
#endif

x_screen_t *  x_screen_new( ml_term_t *  term , x_font_manager_t *  font_man ,
	x_color_manager_t *  color_man , x_termcap_entry_t *  termcap ,
	u_int  brightness , u_int contrast , u_int gamma , u_int  alpha ,
	u_int  fade_ratio , x_shortcut_t *  shortcut ,
	u_int  screen_width_ratio , u_int  screen_height_ratio ,
	char *  mod_meta_key , x_mod_meta_mode_t  mod_meta_mode ,
	x_bel_mode_t  bel_mode , int  receive_string_via_ucs , char *  pic_file_path ,
	int  use_transbg , int  use_vertical_cursor , int  big5_buggy ,
	int  use_extended_scroll_shortcut , int  borderless , u_int  line_space ,
	char *  input_method , int  allow_osc52 , int  blink_cursor ,
	int  hmargin , int  vmargin , int  hide_underline) ;

int  x_screen_delete( x_screen_t *  screen) ;

int  x_screen_attach( x_screen_t *  screen , ml_term_t *  term) ;

int  x_screen_attached( x_screen_t *  screen) ;

ml_term_t *  x_screen_detach( x_screen_t *  screen) ;

void  x_set_system_listener( x_screen_t *  screen ,
	x_system_event_listener_t *  system_listener) ;

void  x_set_screen_scroll_listener( x_screen_t *  screen ,
	x_screen_scroll_event_listener_t *  screen_scroll_listener) ;

	
int  x_screen_scroll_upward( x_screen_t *  screen , u_int  size) ;

int  x_screen_scroll_downward( x_screen_t *  screen , u_int  size) ;

int  x_screen_scroll_to( x_screen_t *  screen , int  row) ;


u_int  x_col_width( x_screen_t *  screen) ;

u_int  x_line_height( x_screen_t *  screen) ;

u_int  x_line_ascent( x_screen_t *  screen) ;

u_int  x_line_top_margin( x_screen_t *  screen) ;

u_int  x_line_bottom_margin( x_screen_t *  screen) ;


int  x_screen_exec_cmd( x_screen_t *  screen , char *  cmd) ;

int  x_screen_set_config( x_screen_t *  screen , char *  dev , char *  key , char *  value) ;


int  x_screen_reset_view( x_screen_t *  screen) ;


x_picture_modifier_t *  x_screen_get_picture_modifier( x_screen_t *  screen) ;


#endif
