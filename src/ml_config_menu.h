/*
 *	$Id$
 */

#ifndef  __ML_CONFIG_MENU_H__
#define  __ML_CONFIG_MENU_H__


#include  <kiklib/kik_types.h>		/* u_int/pid_t */

#include  "ml_color.h"
#include  "ml_char_encoding.h"
#include  "ml_mod_meta_mode.h"
#include  "ml_bel_mode.h"
#include  "ml_font.h"


typedef struct  ml_config_menu_session
{
	pid_t  pid ;
	int  fd ;
	
	ml_char_encoding_t  encoding ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	u_int  tabsize ;
	u_int  logsize ;
	u_int  fontsize ;
	u_int  min_fontsize ;
	u_int  max_fontsize ;
	ml_mod_meta_mode_t  mod_meta_mode ;
	ml_bel_mode_t  bel_mode ;
	ml_font_present_t  font_present ;
	char *  xim ;
	char *  locale ;
	int8_t  is_combining_char ;
	int8_t  copy_paste_via_ucs ;
	int8_t  is_transparent ;
	int8_t  use_bidi ;
	
} ml_config_menu_session_t ;

typedef struct  ml_config_menu_event_listener
{
	void *  self ;
	
	void (*change_encoding)( void * , ml_char_encoding_t) ;
	void (*change_fg_color)( void * , ml_color_t) ;
	void (*change_bg_color)( void * , ml_color_t) ;
	void (*change_tab_size)( void * , u_int) ;
	void (*change_log_size)( void * , u_int) ;
	void (*change_font_size)( void * , u_int) ;
	void (*change_mod_meta_mode)( void * , ml_mod_meta_mode_t) ;
	void (*change_bel_mode)( void * , ml_bel_mode_t) ;
	void (*change_char_combining_flag)( void * , int) ;
	void (*change_copy_paste_via_ucs_flag)( void * , int) ;
	void (*change_transparent_flag)( void * , int) ;
	void (*change_font_present)( void * , ml_font_present_t) ;
	void (*change_bidi_flag)( void * , int) ;
	void (*change_xim)( void * , char * , char *) ;
	
	void (*larger_font_size)( void *) ;
	void (*smaller_font_size)( void *) ;

	void (*change_wall_picture)( void * , char *) ;
	void (*unset_wall_picture)( void *) ;

} ml_config_menu_event_listener_t ;

typedef struct  ml_config_menu
{
	char *  command_path ;
	ml_config_menu_event_listener_t *  config_menu_listener ;
	ml_config_menu_session_t *  session ;

} ml_config_menu_t ;


int  ml_config_menu_init( ml_config_menu_t *  config_menu , char *  command_path ,
	ml_config_menu_event_listener_t *  config_menu_listener) ;

int  ml_config_menu_final( ml_config_menu_t *  config_menu) ;

int  ml_config_menu_start( ml_config_menu_t *  config_menu , int  x , int  y ,
	ml_char_encoding_t  orig_encoding , ml_color_t  orig_fg_color , ml_color_t  orig_bg_color ,
	u_int  orig_tabsize , u_int  orig_logsize , u_int  orig_fontsize , u_int  orig_min_fontsize ,
	u_int  orig_max_fontsize , ml_mod_meta_mode_t  orig_mod_meta_mode , ml_bel_mode_t  orig_bel_mode ,
	int  orig_is_combining_char , int  orig_copy_paste_via_ucs , int  orig_is_transparent ,
	ml_font_present_t  orig_font_present , int  orig_is_bidi ,
	char *  orig_xim , char *  orig_locale) ;


#endif
