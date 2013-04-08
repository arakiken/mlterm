/*
 *	$Id$
 */

#ifndef  __X_FONT_MANAGER_H__
#define  __X_FONT_MANAGER_H__


#include  "x.h"

#include  <kiklib/kik_types.h>
#include  <ml_char_encoding.h>

#include  "x_font_cache.h"


typedef struct  x_font_manager
{
	x_font_cache_t *  font_cache ;
	
	x_font_config_t *  font_config ;

	u_int8_t  step_in_changing_font_size ;
	int8_t  use_bold_font ;

}  x_font_manager_t ;


x_font_manager_t *  x_font_manager_new( Display *  display , x_type_engine_t  type_engine ,
	x_font_present_t  font_present , u_int  font_size , mkf_charset_t  usascii_font_cs ,
	int  use_multi_col_char , u_int  step_in_changing_font_size , u_int  letter_space ,
	int  use_bold_font) ;
	
int  x_font_manager_delete( x_font_manager_t *  font_man) ;

x_font_t *  x_get_font( x_font_manager_t *  font_man , ml_font_t  fontattr) ;

x_font_t *  x_get_usascii_font( x_font_manager_t *  font_man) ;

int  x_font_manager_usascii_font_cs_changed( x_font_manager_t *  font_man ,
	mkf_charset_t  usascii_font_cs) ;

int  x_change_font_present( x_font_manager_t *  font_man , x_type_engine_t  type_engine ,
				x_font_present_t  font_present) ;

x_type_engine_t  x_get_type_engine( x_font_manager_t *  font_man) ;

x_font_present_t  x_get_font_present( x_font_manager_t *  font_man) ;

int  x_change_font_size( x_font_manager_t *  font_man , u_int  font_size) ;

int  x_larger_font( x_font_manager_t *  font_man) ;

int  x_smaller_font( x_font_manager_t *  font_man) ;

u_int  x_get_font_size( x_font_manager_t *  font_man) ;

int  x_set_use_multi_col_char( x_font_manager_t *  font_man , int  flag) ;

#define  x_is_using_multi_col_char( font_man)  \
		((font_man)->font_cache->use_multi_col_char)

int  x_set_letter_space( x_font_manager_t *  font_man , u_int  letter_space) ;

#define  x_get_letter_space( font_man)  ((font_man)->font_cache->letter_space)

int  x_set_use_bold_font( x_font_manager_t *  font_man , int  use_bold_font) ;

#define  x_is_using_bold_font( font_man)  ((font_man)->use_bold_font)

XFontSet  x_get_fontset( x_font_manager_t *  font_man) ;

mkf_charset_t  x_get_usascii_font_cs( ml_char_encoding_t  encoding) ;


#endif
