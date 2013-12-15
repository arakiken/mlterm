/*
 *	$Id$
 */

#ifndef  __X_FONT_CACHE_H__
#define  __X_FONT_CACHE_H__


#include  "x.h"

#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>
#include  <mkf/mkf_charset.h>

#include  "x_font_config.h"


KIK_MAP_TYPEDEF( x_font , ml_font_t , x_font_t *) ;

typedef struct  x_font_cache
{
	/*
	 * Public(readonly)
	 */
	Display *  display ;
	u_int  font_size ;
	mkf_charset_t  usascii_font_cs ;
	x_font_config_t *  font_config ;
	int8_t  use_multi_col_char ;
	u_int8_t  letter_space ;

	x_font_t *  usascii_font ;

	/*
	 * Private
	 */
	KIK_MAP( x_font)  xfont_table ;
	struct
	{
		ml_font_t  font ;
		x_font_t *  xfont ;
		
	} prev_cache ;
	
	u_int  ref_count ;
	
} x_font_cache_t ;


void  x_set_use_leftward_double_drawing( int  use) ;

x_font_cache_t *  x_acquire_font_cache( Display *  display , u_int  font_size ,
	mkf_charset_t  usascii_font_cs , x_font_config_t *  font_config ,
	int  use_multi_col_char , u_int  letter_space) ;

int  x_release_font_cache( x_font_cache_t *  font_cache) ;

int  x_font_cache_unload( x_font_cache_t *  font_cache) ;

int  x_font_cache_unload_all(void) ;

x_font_t *  x_font_cache_get_xfont( x_font_cache_t *  font_cache , ml_font_t  font) ;

char *  x_get_font_name_list_for_fontset( x_font_cache_t *  font_cache) ;


#endif
