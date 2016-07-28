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
typedef struct _FcCharSet *  fc_charset_ptr_t ;
typedef struct _FcPattern *  fc_pattern_ptr_t ;

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

#if  defined(USE_WIN32GUI)
	Font  fid ;
	mkf_conv_t *  conv ;
#elif  defined(USE_QUARTZ)
	void *  cg_font ;
	u_int  size ;
#else
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT)
	xft_font_ptr_t  xft_font ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_CAIRO)
	cairo_scaled_font_ptr_t  cairo_font ;
	struct
	{
		fc_charset_ptr_t  charset ;
		void *  next ;

	} *  compl_fonts ;
	fc_pattern_ptr_t  pattern ;
#endif
#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
	XFontStruct *  xfont ;
#endif
#endif

	x_decsp_font_ptr_t  decsp_font ;

#ifdef  USE_OT_LAYOUT
	/* ot_font == NULL and use_ot_layout == true is possible in ISO10646_UCS4_1_V font. */
	void *  ot_font ;
#ifdef  USE_WIN32GUI
	u_int16_t  size ;	/* font size */
#endif
	int8_t  ot_font_not_found ;
	int8_t  use_ot_layout ;
#endif

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
	int8_t  double_draw_gap ;
	int8_t  size_attr ;

} x_font_t ;


int  x_compose_dec_special_font(void) ;

x_font_t *  x_font_new( Display *  display , ml_font_t  id , int  size_attr ,
	x_type_engine_t  type_engine ,
	x_font_present_t  font_present , const char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold , u_int  letter_space) ;

int  x_font_delete( x_font_t *  font) ;

int  x_font_set_font_present( x_font_t *  font , x_font_present_t  font_present) ;

int  x_font_load_xft_font( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;
	
int  x_font_load_xfont( x_font_t *  font , char *  fontname , u_int  fontsize ,
	u_int  col_width , int  use_medium_for_bold) ;

int  x_change_font_cols( x_font_t *  font , u_int  cols) ;

u_int  x_calculate_char_width( x_font_t *  font , u_int32_t  ch ,
	mkf_charset_t  cs , int *  draw_alone) ;

int  x_font_has_ot_layout_table( x_font_t *  font) ;

u_int  x_convert_text_to_glyphs( x_font_t *  font , u_int32_t *  shaped , u_int  shaped_len ,
	int8_t *  offsets , u_int8_t *  widths , u_int32_t *  cmapped ,
	u_int32_t *  src , u_int  src_len , const char *  script , const char *  features) ;

#ifdef  USE_XLIB
char **  x_font_get_encoding_names( mkf_charset_t  cs) ;

/* For mlterm-libvte */
void  x_font_set_dpi_for_fc( double  dpi) ;
#else
#define  x_font_get_encoding_names(cs)  (0)
#endif

#ifdef  SUPPORT_POINT_SIZE_FONT
void  x_font_use_point_size( int  use) ;
#else
#define  x_font_use_point_size(bool)  (0)
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XFT) || defined(USE_TYPE_CAIRO)
int  x_use_cp932_ucs_for_xft(void) ;

u_int32_t  x_convert_to_xft_ucs4( u_int32_t  ch , mkf_charset_t  cs) ;
#endif

#if  ! defined(NO_DYNAMIC_LOAD_TYPE) || defined(USE_TYPE_XCORE)
size_t  x_convert_ucs4_to_utf16( u_char *  utf16 , u_int32_t  ucs4) ;
#endif

#ifdef  DEBUG
int  x_font_dump( x_font_t *  font) ;
#endif


#endif
