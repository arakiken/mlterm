/*
 *	$Id$
 */

#ifndef  __ML_FONT_MANAGER_H__
#define  __ML_FONT_MANAGER_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>

#include  "ml_font.h"
#include  "ml_font_custom.h"
#include  "ml_char_encoding.h"


KIK_MAP_TYPEDEF( ml_font , ml_font_attr_t , ml_font_t *) ;

typedef struct  ml_font_manager
{
	Display *  display ;

	ml_font_custom_t *  font_custom ;

	/* for fixed column width */
	ml_font_custom_t *  normal_font_custom ;
	ml_font_custom_t *  aa_font_custom ;		/* may be NULL */
	
	/* for variable column width */
	ml_font_custom_t *  v_font_custom ;
	ml_font_custom_t *  vaa_font_custom ;		/* may be NULL */

	/* for vertical view */
	ml_font_custom_t *  t_font_custom ;
	ml_font_custom_t *  taa_font_custom ;		/* may be NULL */

	/* this ml_font_manager_t object local customization */
	ml_font_custom_t *  local_font_custom ;

	u_int  font_size ;
	u_int  line_space ;
	
	KIK_MAP( ml_font)  font_cache_table ;

	ml_font_t *  usascii_font ;
	mkf_charset_t  usascii_font_cs ;

	int8_t  usascii_font_cs_changable ;
	int8_t  use_multi_col_char ;
	
	u_int8_t  step_in_changing_font_size ;
	
	int  (*set_xfont)( ml_font_t * , char * , u_int , u_int , int) ;
	
}  ml_font_manager_t ;


ml_font_manager_t *  ml_font_manager_new( Display *  display ,
	ml_font_custom_t *  normal_font_custom , ml_font_custom_t *  v_font_custom ,
	ml_font_custom_t *  t_font_custom ,
	ml_font_custom_t *  aa_font_custom , ml_font_custom_t *  vaa_font_custom ,
	ml_font_custom_t *  taa_font_custom ,
	u_int  font_size , u_int  line_space , mkf_charset_t  usascii_font_cs ,
	int  usascii_font_cs_changable , int  use_multi_col_char ,
	int  step_in_changing_font_size) ;
	
int  ml_font_manager_delete( ml_font_manager_t *  font_man) ;

int  ml_font_manager_change_font_present( ml_font_manager_t *  font_man , ml_font_present_t  font_present) ;

int  ml_font_manager_set_local_font_name( ml_font_manager_t *  font_man ,
	ml_font_attr_t  font_attr , char *  font_name , u_int  font_size) ;

ml_font_t *  ml_get_font( ml_font_manager_t *  font_man , ml_font_attr_t  fontattr) ;

ml_font_t *  ml_get_usascii_font( ml_font_manager_t *  font_man) ;

int  ml_font_manager_reload( ml_font_manager_t *  font_man) ;

u_int  ml_col_width( ml_font_manager_t *  font_man) ;

u_int  ml_line_height( ml_font_manager_t *  font_man) ;

u_int  ml_line_height_to_baseline( ml_font_manager_t *  font_man) ;

u_int  ml_line_top_margin( ml_font_manager_t *  font_man) ;

u_int  ml_line_bottom_margin( ml_font_manager_t *  font_man) ;

int  ml_font_manager_usascii_font_cs_changed( ml_font_manager_t *  font_man ,
	mkf_charset_t  usascii_font_cs) ;

int  ml_change_font_size( ml_font_manager_t *  font_man , u_int  font_size) ;

int  ml_larger_font( ml_font_manager_t *  font_man) ;

int  ml_smaller_font( ml_font_manager_t *  font_man) ;

int  ml_use_multi_col_char( ml_font_manager_t *  font_man) ;

int  ml_unuse_multi_col_char( ml_font_manager_t *  font_man) ;

XFontSet  ml_get_fontset( ml_font_manager_t *  font_man) ;


#endif
