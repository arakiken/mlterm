/*
 *	$Id$
 */

/*
 * ANTI_ALIAS macro should be concerned.
 */

#ifndef  __ML_FONT_H__
#define  __ML_FONT_H__


#include  <X11/Xlib.h>
#include  <kiklib/kik_types.h>	/* u_int */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */

#ifdef  ANTI_ALIAS
#include  <X11/Xft/Xft.h>
#endif

#include  "ml_decsp_font.h"


#define  RESET_FONT_THICKNESS(cs)  ((cs) &= ~(FONT_MEDIUM | FONT_BOLD))
#define  RESET_FONT_SLANT(cs)      ((cs) &= ~(FONT_ROMAN | FONT_ITALIC))
#define  RESET_FONT_WIDTH(cs)      ((cs) &= ~(FONT_NORMAL))

#define  DEFAULT_FONT_ATTR(cs)  ((cs) | FONT_MEDIUM | FONT_ROMAN | FONT_NORMAL)


typedef enum ml_font_present
{
	FONT_VAR_WIDTH = 0x01 ,
	FONT_AA = 0x02 ,
	FONT_VERTICAL = 0x04 ,

} ml_font_present_t ;

typedef enum ml_font_attr
{
	/* 0x00 - MAX_CHARSET(0x7ff) is reserved for mkf_charset_t */

	/* 0x1000 - 0x2000 is reserved for font thickness */
	FONT_MEDIUM = 0x1000u ,
	FONT_BOLD   = 0x2000u ,

	/* 0x4000 - 0x8000 is reserved for font slant */
	FONT_ROMAN  = 0x4000u ,
	FONT_ITALIC = 0x8000u ,
	
	/* 0x10000 - 0x10000 is reserved for font width */
	FONT_NORMAL = 0x10000u ,

	/* 0x20000 - 0x20000 is reserved for font */
	FONT_BIWIDTH = 0x20000u ,

} ml_font_attr_t ;

typedef struct  ml_font
{
	/*
	 * private
	 */
	Display *  display ;

	/*
	 * public(readonly)
	 */
	ml_font_attr_t  attr ;

	int8_t  is_var_col_width ;
	
	/*
	 * private
	 */
	int8_t  is_vertical ;


	/*
	 * public(readonly)
	 */
#ifdef  ANTI_ALIAS
	XftFont *  xft_font ;
#endif
	XFontStruct *  xfont ;
	ml_decsp_font_t *  decsp_font ;

	/*
	 * these members are never zero.
	 */
	u_int  cols ;
	u_int  width ;
	u_int  height ;
	u_int  height_to_baseline ;

	/* this is not zero only when is_proportional is true and xfont is set. */
	int  x_off ;

	int8_t  is_double_drawing ;
	int8_t  is_proportional ;
	
} ml_font_t ;


int  ml_compose_dec_special_font(void) ;


ml_font_t *  ml_font_new( Display *  display , ml_font_attr_t  attr) ;

int  ml_font_delete( ml_font_t *  font) ;

int  ml_font_set_font_present( ml_font_t *  font , ml_font_present_t  font_present) ;

int  ml_font_set_xft_font( ml_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;
	
int  ml_font_set_xfont( ml_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;

mkf_charset_t  ml_font_cs( ml_font_t *  font) ;

int  ml_change_font_cs( ml_font_t *  font , mkf_charset_t  cs) ;

int  ml_change_font_cols( ml_font_t *  font , u_int  cols) ;

u_int  ml_calculate_char_width( ml_font_t *  font , u_char *  ch , size_t  len , mkf_charset_t  cs) ;

#ifdef  DEBUG
int  ml_font_dump( ml_font_t *  font) ;
#endif


#endif
