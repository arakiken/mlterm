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

#include  <kiklib/kik_types.h>	/* u_int */
#include  <mkf/mkf_charset.h>	/* mkf_charset_t */
#include  <ml_font.h>

#include  "x_type_engine.h"


typedef enum x_font_present
{
	FONT_VAR_WIDTH = 0x1 ,
	FONT_VERTICAL = 0x2 ,
	FONT_AA = 0x4 ,
	FONT_NOAA = 0x8 ,	/* Don't specify with FONT_AA */

} x_font_present_t ;

typedef struct _XftFont *  xft_font_ptr_t ;
typedef struct _cairo_scaled_font *  cairo_scaled_font_ptr_t ;

/* defined in xlib/x_decsp_font.h */
typedef struct x_decsp_font *  x_decsp_font_ptr_t ;

typedef struct x_font
{
	/*
	 * Private
	 */
	Display *  display ;
	
	/*
	 * Public(readonly)
	 */
	ml_font_t  id ;

#ifdef  USE_WIN32GUI
	Font  fid ;
	mkf_conv_t *  conv ;
#else
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	xft_font_ptr_t  xft_font ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	cairo_scaled_font_ptr_t  cairo_font ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	XFontStruct *  xfont ;
#endif
#endif

	x_decsp_font_ptr_t  decsp_font ;

	/*
	 * These members are never zero.
	 */
	u_int8_t  cols ;
	u_int8_t  width ;
	u_int8_t  height ;
	u_int8_t  ascent ;

	/* This is not zero only when is_proportional is true and xfont is set. */
	int8_t  x_off ;

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
	u_int  col_width , int  use_medium_for_bold , u_int  letter_space) ;

int  x_font_delete( x_font_t *  font) ;

int  x_font_set_font_present( x_font_t *  font , x_font_present_t  font_present) ;

int  x_font_load_xft_font( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;
	
int  x_font_load_xfont( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;

int  x_change_font_cols( x_font_t *  font , u_int  cols) ;

u_int  x_calculate_char_width( x_font_t *  font ,
		const u_char *  ch , size_t  len , mkf_charset_t  cs) ;

char **  x_font_get_encoding_names( mkf_charset_t  cs) ;

void  x_font_use_point_size_for_fc( int  bool) ;

void  x_font_set_dpi_for_fc( double  dpi) ;

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
int  x_use_cp932_ucs_for_xft(void) ;

int  x_convert_to_xft_ucs4( u_char *  ucs4_bytes , const u_char *  src_bytes , size_t  src_size ,
	mkf_charset_t  cs) ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
size_t  x_convert_ucs4_to_utf16( u_char *  utf16 , u_char *  ucs) ;
#endif

#ifdef  USE_FRAMEBUFFER
u_char *  x_get_bitmap( XFontStruct *  xfont , u_char *  ch , size_t  len) ;

int  x_get_bitmap_cell( XFontStruct *  xfont , u_char *  bitmap , int  x , int  y) ;
#endif

#ifdef  DEBUG
int  x_font_dump( x_font_t *  font) ;
#endif


#endif
