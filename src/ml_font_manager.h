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

	ml_font_custom_t *  normal_font_custom ;
	ml_font_custom_t *  aa_font_custom ;
	
	u_int  font_size ;

	KIK_MAP( ml_font)  font_cache_table ;

	ml_font_t *  usascii_font ;
	mkf_charset_t  usascii_font_cs ;

	int8_t  usascii_font_cs_changable ;

	int  (*set_xfont)( ml_font_t * , char * , u_int , u_int , int) ;
	
}  ml_font_manager_t ;


ml_font_manager_t *  ml_font_manager_new( Display *  display ,
	ml_font_custom_t *  normal_font_custom , ml_font_custom_t *  aa_font_custom ,
	u_int  font_size , mkf_charset_t  usascii_font_cs , int  usascii_font_cs_changable) ;
	
int  ml_font_manager_delete( ml_font_manager_t *  font_man) ;

int  ml_font_manager_use_aa( ml_font_manager_t *  font_man) ;

int  ml_font_manager_unuse_aa( ml_font_manager_t *  font_man) ;

ml_font_t *  ml_get_font( ml_font_manager_t *  font_man , ml_font_attr_t  fontattr) ;

ml_font_t *  ml_get_usascii_font( ml_font_manager_t *  font_man) ;

u_int  ml_col_width( ml_font_manager_t *  font_man) ;

u_int  ml_line_height( ml_font_manager_t *  font_man) ;

u_int  ml_line_height_to_baseline( ml_font_manager_t *  font_man) ;

int  ml_font_manager_usascii_font_cs_changed( ml_font_manager_t *  font_man ,
	mkf_charset_t  usascii_font_cs) ;

int  ml_change_font_size( ml_font_manager_t *  font_man , u_int  font_size) ;

int  ml_larger_font( ml_font_manager_t *  font_man) ;

int  ml_smaller_font( ml_font_manager_t *  font_man) ;

XFontSet  ml_get_fontset( ml_font_manager_t *  font_man) ;


#endif
