/*
 *	$Id$
 */

#ifndef  __ML_COLOR_H__
#define  __ML_COLOR_H__


#include  <kiklib/kik_types.h>


#define  MAX_VTSYS_COLORS  16
#define  MAX_BASIC_VTSYS_COLORS  8

/* same as 0 <= color <= 0x7 */
#define  IS_VTSYS_BASE_COLOR(color)  ((unsigned int)(color) <= 0x7)
/* same as 0 <= color <= 0xf */
#define  IS_VTSYS_COLOR(color)  ((unsigned int)(color) <= 0xf)
#define  IS_256_COLOR(color)  (0x10 <= (color) && (color) <= 0xff)
#define  IS_VALID_COLOR_EXCEPT_FG_BG(color)  ((unsigned int)(color) <= 0xff)
#define  IS_FG_BG_COLOR(color)  (0x100 <= (color) && (color) <= 0x101)
#define  IS_ALT_COLOR(color)  (0x102 <= (color))


typedef enum  ml_color
{
	ML_UNKNOWN_COLOR = -1 ,

	/*
	 * Don't change this order, which ml_vt100_parser.c(change_char_attr etc) and
	 * x_color_cache.c etc depend on.
	 */
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
	ML_BG_COLOR = 0x101 ,

	ML_BOLD_COLOR = 0x102 ,
	ML_ITALIC_COLOR = 0x103 ,
	ML_UNDERLINE_COLOR = 0x104 ,
	ML_BLINKING_COLOR = 0x105 ,
	ML_CROSSED_OUT_COLOR = 0x106 ,

} ml_color_t ;


int  ml_color_config_init(void) ;

int  ml_color_config_final(void) ;

int  ml_customize_color_file( char *  color , char *  rgb , int  save) ;


char *  ml_get_color_name( ml_color_t  color) ;

ml_color_t  ml_get_color( const char *  name) ;

int  ml_get_color_rgba( ml_color_t  color, u_int8_t *  red, u_int8_t *  green,
		u_int8_t *  blue, u_int8_t *  alpha) ;

int  ml_color_parse_rgb_name( u_int8_t *  red, u_int8_t *  green, u_int8_t *  blue,
	u_int8_t *  alpha, const char *  name) ;

ml_color_t  ml_get_closest_color( u_int8_t  red , u_int8_t  green , u_int8_t  blue) ;


#endif
