/*
 *	update: <2001/11/26(17:06:59)>
 *	$Id$
 */

#ifndef  __ML_COLOR_H__
#define  __ML_COLOR_H__


/* this must be included before including Xft.h(before XFree86-4.0.x) */
#include  <X11/Xlib.h>

#ifdef  ANTI_ALIAS
#include  <X11/Xft/Xft.h>
#endif


/* used in ml_color_manager.c/ml_window.c */
#define  MAX_ACTUAL_COLORS MLC_FG_COLOR


#ifdef  ANTI_ALIAS
typedef XftColor **  ml_color_table_t ;
#else
typedef XColor **  ml_color_table_t ;
#endif


typedef enum  ml_color
{
	MLC_UNKNOWN_COLOR = -1 ,

	/*
	 * 11 colors are registered.
	 * max color must be under 0xf.(see ml_char.h)
	 */
	 
	MLC_BLACK = 0 ,
	MLC_RED ,
	MLC_GREEN ,
	MLC_YELLOW ,
	MLC_BLUE ,
	MLC_MAGENTA ,
	MLC_CYAN ,
	MLC_WHITE ,
	MLC_GRAY ,
	MLC_LIGHTGRAY ,
	MLC_PRIVATE_FG_COLOR ,
	MLC_PRIVATE_BG_COLOR ,

	/* extra */
	MLC_FG_COLOR ,
	MLC_BG_COLOR ,

	MAX_COLORS ,

} ml_color_t ;


ml_color_table_t  ml_color_table_dup( ml_color_table_t  color_table) ;

char *  ml_get_color_name( ml_color_t  color) ;

ml_color_t  ml_get_color( char *  name) ;


#endif
