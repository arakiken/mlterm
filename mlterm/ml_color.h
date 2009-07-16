/*
 *	$Id$
 */

#ifndef  __ML_COLOR_H__
#define  __ML_COLOR_H__


#include  <kiklib/kik_types.h>


#define  MAX_VTSYS_COLORS  16
#define  MAX_BASIC_VTSYS_COLORS  8

#define  IS_VTSYS_COLOR(color)  (0x0 <= (color) && (color) <= 0xf)
#define  IS_256_COLOR(color)  (0x10 <= (color) && (color) <= 0xff)
#define  IS_VALID_COLOR(color)  (0x0 <= (color) && (color) <= 0x101)


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

	ML_BOLD_COLOR_MASK = 0x8 ,

	/*
	 * 0x8 - 0xf: bold vt colors.
	 */
	
	/*
	 * 0x10 - 0xff: 240 colors.
	 */

	ML_FG_COLOR = 0x100 ,
	ML_BG_COLOR = 0x101
	
} ml_color_t ;


/* For VT color only(ignore ML_BOLD_COLOR_MASK) */

char *  ml_get_color_name( ml_color_t  color) ;

ml_color_t  ml_get_color( char *  name) ;


int  ml_get_color_rgb( ml_color_t  color, u_int8_t *  red, u_int8_t *  green, u_int8_t *  blue) ;

int  ml_change_color_rgb( ml_color_t  color, u_int8_t  red, u_int8_t  green, u_int8_t  blue) ;

int  ml_color_parse_rgb_name( u_int8_t *  red, u_int8_t *  green, u_int8_t *  blue, char *  name) ;


#endif
