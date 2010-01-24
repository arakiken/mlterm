/*
 *	$Id$
 */

#ifndef  __X_FONT_H__
#define  __X_FONT_H__


/* X11/Xlib.h must be included ahead of Xft.h on XFree86-4.0.x or before. */
#include  "x.h"

#ifdef  USE_WIN32GUI
#include  <mkf/mkf_conv.h>
#endif

#ifdef  USE_TYPE_XFT
#include  <X11/Xft/Xft.h>
#endif

#include  <kiklib/kik_types.h>	/* u_int */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */
#include  <ml_font.h>

#include  "x_type_engine.h"
#include  "x_decsp_font.h"


typedef enum x_font_present
{
	FONT_VAR_WIDTH = 0x01 ,
	FONT_AA = 0x02 ,
	FONT_VERTICAL = 0x04 ,

	FONT_PRESENT_MAX

} x_font_present_t ;

typedef struct x_font
{
	/*
	 * Private
	 */
	Display *  display ;
	
	ml_font_t  id ;

	/*
	 * Public(readonly)
	 */
#ifdef  USE_TYPE_XFT
	XftFont *  xft_font ;
#endif
#ifdef  USE_WIN32GUI
	Font  fid ;
	mkf_conv_t *  conv ;
#elif   USE_TYPE_XCORE
	XFontStruct *  xfont ;
#endif

	x_decsp_font_t *  decsp_font ;

	/*
	 * These members are never zero.
	 */
	u_int  cols ;
	u_int  width ;
	u_int  height ;
	u_int  height_to_baseline ;

	/* This is not zero only when is_proportional is true and xfont is set. */
	int  x_off ;

	/*
	 * If is_var_col_width is false and is_proportional is true,
	 * characters are drawn one by one. (see {xft_}draw_str())
	 */
	int8_t  is_var_col_width ;
	int8_t  is_proportional ;
	
	int8_t  is_vertical ;
	int8_t  is_double_drawing ;

} x_font_t ;


int  x_compose_dec_special_font(void) ;


x_font_t *  x_font_new( Display *  display , ml_font_t  id , x_type_engine_t  type_engine ,
	x_font_present_t  font_present , const char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;

int  x_font_delete( x_font_t *  font) ;

int  x_font_set_font_present( x_font_t *  font , x_font_present_t  font_present) ;

int  x_font_load_xft_font( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;
	
int  x_font_load_xfont( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;

int  x_change_font_cols( x_font_t *  font , u_int  cols) ;

u_int  x_calculate_char_width( x_font_t *  font ,
		const u_char *  ch , size_t  len , mkf_charset_t  cs) ;

char **  x_font_get_cs_names( mkf_charset_t  cs) ;

#ifdef  DEBUG
int  x_font_dump( x_font_t *  font) ;
#endif


#endif
