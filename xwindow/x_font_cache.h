/*
 *	$Id$
 */

#ifndef  __X_FONT_CACHE_H__
#define  __X_FONT_CACHE_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_map.h>
#include  <kiklib/kik_types.h>
#include  <mkf/mkf_charset.h>

#include  "x_font_custom.h"


KIK_MAP_TYPEDEF( x_font , ml_font_t , x_font_t *) ;

typedef struct  x_font_cache
{
	/*
	 * Public(readonly)
	 */
	Display *  display ;
	u_int  font_size ;
	mkf_charset_t  usascii_font_cs ;
	x_font_custom_t *  font_custom ;
	int8_t  use_multi_col_char ;

	x_font_t *  usascii_font ;

	/*
	 * Private
	 */
	KIK_MAP( x_font)  xfont_table ;
	u_int  ref_count ;
	
} x_font_cache_t ;


x_font_cache_t *  x_acquire_font_cache( Display *  display , u_int  font_size ,
	mkf_charset_t  usascii_font_cs , x_font_custom_t *  font_custom , int  use_multi_col_char) ;

int  x_release_font_cache( x_font_cache_t *  font_cache) ;

x_font_t *  x_font_cache_get_xfont( x_font_cache_t *  font_cache , ml_font_t  font) ;

u_int  x_font_cache_get_all_font_names( x_font_cache_t *  font_cache , char ***  fontnames) ;


#endif
