/*
 *	$Id$
 */

#ifndef  __ML_CONFIG_MENU_H__
#define  __ML_CONFIG_MENU_H__


#include  <kiklib/kik_types.h>		/* u_int */

#include  "ml_color.h"
#include  "ml_encoding.h"
#include  "ml_mod_meta_mode.h"
#include  "ml_bel_mode.h"


typedef struct  ml_config_menu_event_listener
{
	void *  self ;
	
	void (*change_encoding)( void * , ml_encoding_type_t) ;
	void (*change_fg_color)( void * , ml_color_t) ;
	void (*change_bg_color)( void * , ml_color_t) ;
	void (*change_tab_size)( void * , u_int) ;
	void (*change_log_size)( void * , u_int) ;
	void (*change_font_size)( void * , u_int) ;
	void (*change_mod_meta_mode)( void * , ml_mod_meta_mode_t) ;
	void (*change_bel_mode)( void * , ml_bel_mode_t) ;
	void (*change_char_combining_flag)( void * , int) ;
	void (*change_pre_conv_xct_to_ucs_flag)( void * , int) ;
	void (*change_transparent_flag)( void * , int) ;
	void (*change_aa_flag)( void * , int) ;
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

} ml_config_menu_t ;


int  ml_config_menu_init( ml_config_menu_t *  config_menu , char *  command_path ,
	ml_config_menu_event_listener_t *  config_menu_listener) ;

int  ml_config_menu_final( ml_config_menu_t *  config_menu) ;

int  ml_config_menu_start( ml_config_menu_t *  config_menu , int  orig_x , int  orig_y ,
	ml_encoding_type_t  orig_encoding , ml_color_t  orig_fg_color , ml_color_t  orig_bg_color ,
	u_int  orig_tabsize , u_int  orig_logsize , u_int  orig_fontsize , u_int  orig_min_fontsize ,
	u_int  orig_max_fontsize , ml_mod_meta_mode_t  orig_mod_meta_mode , ml_bel_mode_t  orig_bel_mode ,
	int  orig_is_combining_char , int  orig_pre_conv_xct_to_ucs4 , int  orig_is_transparent ,
	int  orig_is_aa , int  orig_is_bidi , char *  orig_xim , char *  orig_locale) ;


#endif
