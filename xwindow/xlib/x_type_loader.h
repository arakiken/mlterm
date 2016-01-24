/*
 *	$Id$
 */

#ifndef  __X_TYPE_LOADER_H__
#define  __X_TYPE_LOADER_H__


#include  "../x_font.h"
#include  "../x_window.h"


typedef enum  x_type_id
{
	TYPE_API_COMPAT_CHECK ,
	X_WINDOW_SET_TYPE ,
	X_WINDOW_DRAW_STRING8 ,
	X_WINDOW_DRAW_STRING32 ,
	X_WINDOW_RESIZE ,
	X_SET_FONT ,
	X_UNSET_FONT ,
	X_CALCULATE_CHAR_WIDTH ,
	X_WINDOW_SET_CLIP ,
	X_WINDOW_UNSET_CLIP ,
	X_SET_OT_FONT ,
	X_CONVERT_TEXT_TO_GLYPHS ,
	MAX_TYPE_FUNCS ,

} x_type_id_t ;


#define  TYPE_API_VERSION  0x01
#define  TYPE_API_COMPAT_CHECK_MAGIC			\
	 (((TYPE_API_VERSION & 0x0f) << 28) |		\
	  ((sizeof( x_font_t) & 0xff) << 20) |		\
	  ((sizeof( x_window_t) & 0xff) << 12))


void *  x_load_type_xft_func( x_type_id_t  id) ;

void *  x_load_type_cairo_func( x_type_id_t  id) ;


#endif
