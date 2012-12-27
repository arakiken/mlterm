/*
 *	$Id$
 */

#ifndef  __X_PICTURE_H__
#define  __X_PICTURE_H__


#include  <kiklib/kik_types.h>		/* u_int16_t */
#include  <ml_term.h>

#include  "x.h"				/* XA_PIXMAP */
#include  "x_window.h"


#define  PICTURE_CHARSET  0x1ff


typedef struct x_picture_modifier
{
	u_int16_t  brightness ;		/* 0 - 65535 */
	u_int16_t  contrast ;		/* 0 - 65535 */
	u_int16_t  gamma ;		/* 0 - 65535 */

	u_int8_t  alpha ;		/* 0 - 255 */
	u_int8_t  blend_red ;
	u_int8_t  blend_green ;
	u_int8_t  blend_blue ;

} x_picture_modifier_t ;

typedef struct x_picture
{
	Display *  display ;
	x_picture_modifier_t *  mod ;
	char *  file_path ;
	u_int  width ;
	u_int  height ;

	Pixmap  pixmap ;

	u_int  ref_count ;

} x_picture_t ;

typedef struct x_icon_picture
{
	x_display_t *  disp ;
	char *  file_path ;

	Pixmap  pixmap ;
	Pixmap  mask ;
	u_int32_t *  cardinal ;

	u_int  ref_count ;
	
} x_icon_picture_t ;

typedef struct x_inline_picture
{
	Pixmap  pixmap ;
	char *  file_path ;
	u_int  width ;
	u_int  height ;
	Display *  display ;
	ml_term_t *  term ;
	u_int  col_width ;
	u_int  line_height ;

} x_inline_picture_t ;


int  x_picture_display_opened( Display *  display) ;

int  x_picture_display_closed( Display *  display) ;

int  x_picture_modifiers_equal( x_picture_modifier_t *  a , x_picture_modifier_t *  b) ;

#define  x_picture_modifier_is_normal(pic_mod) (x_picture_modifiers_equal((pic_mod), NULL))

x_picture_t *  x_acquire_bg_picture( x_window_t *  win , x_picture_modifier_t *  mod ,
			char *  file_path) ;

int  x_release_picture( x_picture_t *  pic) ;

x_icon_picture_t *  x_acquire_icon_picture( x_display_t *  disp , char *  file_path) ;

int  x_release_icon_picture( x_icon_picture_t *  pic) ;

int  x_load_inline_picture( x_display_t *  disp , char *  file_path ,
	u_int *  width , u_int *  height , u_int  col_width , u_int  line_height ,
	ml_term_t *  term) ;

x_inline_picture_t *  x_get_inline_picture( int  idx) ;


#endif
