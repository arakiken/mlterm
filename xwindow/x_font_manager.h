/*
 *	$Id$
 */

#ifndef  __X_FONT_MANAGER_H__
#define  __X_FONT_MANAGER_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>

#include  "x_font.h"
#include  "x_font_custom.h"
#include  "ml_char_encoding.h"


KIK_MAP_TYPEDEF( x_font , ml_font_t , x_font_t *) ;

typedef struct  x_font_manager
{
	Display *  display ;

	x_font_custom_t *  font_custom ;

	/* for fixed column width */
	x_font_custom_t *  normal_font_custom ;
	x_font_custom_t *  aa_font_custom ;		/* may be NULL */
	
	/* for variable column width */
	x_font_custom_t *  v_font_custom ;
	x_font_custom_t *  vaa_font_custom ;		/* may be NULL */

	/* for vertical view */
	x_font_custom_t *  t_font_custom ;
	x_font_custom_t *  taa_font_custom ;		/* may be NULL */

	/* this x_font_manager_t object local customization */
	x_font_custom_t *  local_font_custom ;

	u_int  font_size ;
	
	KIK_MAP( x_font)  font_cache_table ;

	struct
	{
		ml_font_t  font ;
		x_font_t *  xfont ;
		
	} prev_cache ;

	x_font_t *  usascii_font ;
	mkf_charset_t  usascii_font_cs ;

	int8_t  usascii_font_cs_changable ;
	int8_t  use_multi_col_char ;
	
	u_int8_t  step_in_changing_font_size ;
	
	int  (*load_xfont)( x_font_t * , char * , u_int , u_int , int) ;
	
}  x_font_manager_t ;


x_font_manager_t *  x_font_manager_new( Display *  display ,
	x_font_custom_t *  normal_font_custom , x_font_custom_t *  v_font_custom ,
	x_font_custom_t *  t_font_custom ,
	x_font_custom_t *  aa_font_custom , x_font_custom_t *  vaa_font_custom ,
	x_font_custom_t *  taa_font_custom ,
	x_font_present_t  font_present , u_int  font_size , mkf_charset_t  usascii_font_cs ,
	int  usascii_font_cs_changable , int  use_multi_col_char ,
	int  step_in_changing_font_size) ;
	
int  x_font_manager_delete( x_font_manager_t *  font_man) ;

int  x_font_manager_change_font_present( x_font_manager_t *  font_man ,
	x_font_present_t  font_present) ;

x_font_present_t  x_font_manager_get_font_present( x_font_manager_t *  font_man) ;

int  x_font_manager_set_local_font_name( x_font_manager_t *  font_man ,
	ml_font_t  font , char *  font_name , u_int  font_size) ;

x_font_t *  x_get_font( x_font_manager_t *  font_man , ml_font_t  fontattr) ;

x_font_t *  x_get_usascii_font( x_font_manager_t *  font_man) ;

int  x_font_manager_reload( x_font_manager_t *  font_man) ;

int  x_font_manager_usascii_font_cs_changed( x_font_manager_t *  font_man ,
	mkf_charset_t  usascii_font_cs) ;

int  x_change_font_size( x_font_manager_t *  font_man , u_int  font_size) ;

int  x_larger_font( x_font_manager_t *  font_man) ;

int  x_smaller_font( x_font_manager_t *  font_man) ;

int  x_use_multi_col_char( x_font_manager_t *  font_man) ;

int  x_unuse_multi_col_char( x_font_manager_t *  font_man) ;

XFontSet  x_get_fontset( x_font_manager_t *  font_man) ;

mkf_charset_t  x_get_usascii_font_cs( ml_char_encoding_t  encoding) ;


#endif
