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
#include  "ml_logical_visual.h"
#include  "ml_sb_mode.h"
#include  "ml_font.h"
#include  "ml_iscii.h"


typedef struct  ml_config_menu_session
{
	pid_t  pid ;
	int  fd ;
	
	ml_char_encoding_t  encoding ;
	ml_iscii_lang_t  iscii_lang ;
	ml_color_t  fg_color ;
	ml_color_t  bg_color ;
	ml_color_t  sb_fg_color ;
	ml_color_t  sb_bg_color ;
	u_int  tabsize ;
	u_int  logsize ;
	u_int  fontsize ;
	u_int  line_space ;
	u_int  min_fontsize ;
	u_int  max_fontsize ;
	u_int  screen_width_ratio ;
	u_int  screen_height_ratio ;
	ml_mod_meta_mode_t  mod_meta_mode ;
	ml_bel_mode_t  bel_mode ;
	ml_font_present_t  font_present ;
	ml_vertical_mode_t  vertical_mode ;
	ml_sb_mode_t  sb_mode ;
	char *  xim ;
	char *  locale ;
	char *  sb_view_name ;
	char *  wall_pic ;
	int8_t  is_combining_char ;
	int8_t  copy_paste_via_ucs ;
	int8_t  is_transparent ;
	int8_t  use_bidi ;
	int8_t  bidi_base_dir_is_rtl ;
	int8_t  use_multi_col_char ;
	u_int8_t  brightness ;
	u_int8_t  fade_ratio ;
	
} ml_config_menu_session_t ;

typedef struct  ml_config_menu_event_listener
{
	void *  self ;
	
	void (*change_char_encoding)( void * , ml_char_encoding_t) ;
	void (*change_iscii_lang)( void * , ml_iscii_lang_t) ;
	void (*change_fg_color)( void * , ml_color_t) ;
	void (*change_bg_color)( void * , ml_color_t) ;
	void (*change_sb_fg_color)( void * , ml_color_t) ;
	void (*change_sb_bg_color)( void * , ml_color_t) ;
	void (*change_tab_size)( void * , u_int) ;
	void (*change_log_size)( void * , u_int) ;
	void (*change_font_size)( void * , u_int) ;
	void (*change_line_space)( void * , u_int) ;
	void (*change_screen_width_ratio)( void * , u_int) ;
	void (*change_screen_height_ratio)( void * , u_int) ;
	void (*change_mod_meta_mode)( void * , ml_mod_meta_mode_t) ;
	void (*change_bel_mode)( void * , ml_bel_mode_t) ;
	void (*change_vertical_mode)( void * , ml_vertical_mode_t) ;
	void (*change_sb_mode)( void * , ml_sb_mode_t) ;
	void (*change_char_combining_flag)( void * , int) ;
	void (*change_copy_paste_via_ucs_flag)( void * , int) ;
	void (*change_transparent_flag)( void * , int) ;
	void (*change_font_present)( void * , ml_font_present_t) ;
	void (*change_multi_col_char_flag)( void * , int) ;
	void (*change_bidi_flag)( void * , int) ;
	void (*change_bidi_base_dir)( void * , int) ;
	void (*change_fade_ratio)( void * , u_int) ;
	void (*change_brightness)( void * , u_int) ;
	void (*change_sb_view)( void * , char *) ;
	void (*change_xim)( void * , char * , char *) ;
	void (*change_wall_picture)( void * , char *) ;
	
	void (*larger_font_size)( void *) ;
	void (*smaller_font_size)( void *) ;

	void (*full_reset)( void *) ;

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
	ml_char_encoding_t  orig_encoding , ml_iscii_lang_t  orig_iscii_lang ,
	ml_color_t  orig_fg_color , ml_color_t  orig_bg_color ,
	ml_color_t  orig_sb_fg_color , ml_color_t  orig_sb_bg_color ,
	u_int  orig_tabsize , u_int  orig_logsize , u_int  orig_fontsize , u_int  orig_min_fontsize ,
	u_int  orig_max_fontsize , u_int  orig_line_space ,
	u_int  orig_screen_width_ratio , u_int  orig_screen_height_ratio ,
	ml_mod_meta_mode_t  orig_mod_meta_mode , ml_bel_mode_t  orig_bel_mode ,
	ml_vertical_mode_t  orig_vertical_mode , ml_sb_mode_t  orig_sb_mode ,
	int  orig_is_combining_char , int  orig_copy_paste_via_ucs , int  orig_is_transparent ,
	u_int  orig_brightness , u_int  orig_fade_ratio , ml_font_present_t  orig_font_present ,
	int  orig_use_multi_col_char , int  orig_use_bidi , int  orig_bidi_base_dir_is_rtl ,
	char *  orig_sb_view_name , char *  orig_xim , char *  orig_locale , char *  orig_wall_pic) ;


#endif
