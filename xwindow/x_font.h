/*
 *	$Id$
 */

#ifndef  __X_FONT_H__
#define  __X_FONT_H__


/* This must be included ahead of Xft.h on XFree86-4.0.x or before. */
#include  <X11/Xlib.h>

#ifdef  ANTI_ALIAS
#include  <X11/Xft/Xft.h>
#endif

#include  <kiklib/kik_types.h>	/* u_int */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */
#include  <ml_font.h>

#include  "x_decsp_font.h"


typedef enum x_font_present
{
	FONT_VAR_WIDTH = 0x01 ,
	FONT_AA = 0x02 ,
	FONT_VERTICAL = 0x04 ,

} x_font_present_t ;

typedef struct x_font
{
	/*
	 * private
	 */
	Display *  display ;
	
	ml_font_t  id ;

	/*
	 * public(readonly)
	 */
	int8_t  is_vertical ;
	int8_t  is_var_col_width ;
	
#ifdef  ANTI_ALIAS
	XftFont *  xft_font ;
#endif
	XFontStruct *  xfont ;
	x_decsp_font_t *  decsp_font ;

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
	
} x_font_t ;


int  x_compose_dec_special_font(void) ;


x_font_t *  x_font_new( Display *  display , ml_font_t  id , x_font_present_t  font_present ,
	char *  fontname , u_int  fontsize , u_int  col_width , int  use_medium_for_bold) ;

int  x_font_delete( x_font_t *  font) ;

int  x_font_set_font_present( x_font_t *  font , x_font_present_t  font_present) ;

int  x_font_load_xft_font( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;
	
int  x_font_load_xfont( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;

int  x_change_font_cols( x_font_t *  font , u_int  cols) ;

u_int  x_calculate_char_width( x_font_t *  font , u_char *  ch , size_t  len , mkf_charset_t  cs) ;

#ifdef  DEBUG
int  x_font_dump( x_font_t *  font) ;
#endif


#endif
