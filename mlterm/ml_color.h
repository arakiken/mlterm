/*
 *	$Id$
 */

#ifndef  __ML_COLOR_H__
#define  __ML_COLOR_H__


#define  MAX_COLORS         10
#define  MAX_ACTUAL_COLORS  8


typedef enum  ml_color
{
	ML_UNKNOWN_COLOR = -1 ,

	ML_BLACK = 0x0 ,
	ML_RED = 0x1 ,
	ML_GREEN = 0x2 ,
	ML_YELLOW = 0x3 ,
	ML_BLUE = 0x4 ,
	ML_MAGENTA = 0x5 ,
	ML_CYAN = 0x6 ,
	ML_WHITE = 0x7 ,
	ML_FG_COLOR = 0x8 ,
	ML_BG_COLOR = 0x9 ,

	ML_BOLD_COLOR_MASK = 0x10 ,

} ml_color_t ;


char *  ml_get_color_name( ml_color_t  color) ;


#endif
