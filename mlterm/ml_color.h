/*
 *	$Id$
 */

#ifndef  __ML_COLOR_H__
#define  __ML_COLOR_H__


#include  <kiklib/kik_types.h>


#define  MAX_VTSYS_COLORS  16
#define  MAX_BASIC_VTSYS_COLORS  8
#define  MAX_256_COLORS  240
#define  MAX_EXT_COLORS  240
#define  MAX_256EXT_COLORS  (MAX_256_COLORS + MAX_EXT_COLORS)

/* same as 0 <= color <= 0x7 */
#define  IS_VTSYS_BASE_COLOR(color)  ((unsigned int)(color) <= 0x7)
/* same as 0 <= color <= 0xf */
#define  IS_VTSYS_COLOR(color)  ((unsigned int)(color) <= 0xf)
#define  IS_256_COLOR(color)  (0x10 <= (color) && (color) <= 0xff)
#define  IS_EXT_COLOR(color)  (0x100 <= (color) && (color) <= 0x1ef)
#define  IS_256EXT_COLOR(color)  (0x10 <= (color) && (color) <= 0x1ef)
#define  IS_VALID_COLOR_EXCEPT_SPECIAL_COLORS(color)  ((unsigned int)(color) <= 0x1ef)
#define  IS_FG_BG_COLOR(color)  (0x1f0 <= (color) && (color) <= 0x1f1)
#define  IS_ALT_COLOR(color)  (0x1f2 <= (color))
#define  EXT_COLOR_TO_INDEX(color)  ((color) - MAX_VTSYS_COLORS - MAX_256_COLORS)
#define  INDEX_TO_EXT_COLOR(color)  ((color) + MAX_VTSYS_COLORS + MAX_256_COLORS)
#define  COLOR_DISTANCE( diff_r , diff_g , diff_b) \
		((diff_r) * (diff_r) * 9 + (diff_g) * (diff_g) * 30 + (diff_b) * (diff_b))
/* no one may notice the difference (4[2^3/2]*4*9+4*4*30+4*4) */
#define  COLOR_DISTANCE_THRESHOLD  640


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

	/*
	 * 0x100 - 0x1ef: 241-480 colors.
	 */

	ML_FG_COLOR = 0x1f0 ,
	ML_BG_COLOR = 0x1f1 ,

	ML_BOLD_COLOR = 0x1f2 ,
	ML_ITALIC_COLOR = 0x1f3 ,
	ML_UNDERLINE_COLOR = 0x1f4 ,
	ML_BLINKING_COLOR = 0x1f5 ,
	ML_CROSSED_OUT_COLOR = 0x1f6 ,

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

int  ml_ext_color_is_changed( ml_color_t  color) ;


#endif
