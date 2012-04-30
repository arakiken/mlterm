/*
 *	$Id$
 */

#ifndef  __X_PICTURE_H__
#define  __X_PICTURE_H__


#include  <kiklib/kik_types.h>		/* u_int16_t */

#include  "x.h"				/* XA_PIXMAP */
#include  "x_window.h"


#define  x_picture_modifier_is_normal(pic_mod) (x_picture_modifiers_equal((pic_mod), NULL))


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

#ifdef  ENABLE_SIXEL
typedef struct x_picture_manager
{
	struct
	{
		x_picture_t *  pic ;
		int  x ;
		int  y ;

	} *  pics ;

	u_int  num_of_pics ;

} x_picture_manager_t ;
#endif	/* ENABLE_SIXEL */


int  x_picture_display_opened( Display *  display) ;

int  x_picture_display_closed( Display *  display) ;

int  x_picture_modifiers_equal( x_picture_modifier_t *  a , x_picture_modifier_t *  b) ;

x_picture_t *  x_acquire_bg_picture( x_window_t *  win , x_picture_modifier_t *  mod ,
			char *  file_path) ;

#ifdef  ENABLE_SIXEL
x_picture_t * x_acquire_picture( x_display_t *  disp ,
			char *  file_path , u_int  width , u_int  height) ;
#endif

int  x_release_picture( x_picture_t *  pic) ;

x_icon_picture_t *  x_acquire_icon_picture( x_display_t *  disp , char *  file_path) ;

int  x_release_icon_picture( x_icon_picture_t *  pic) ;

#ifdef  ENABLE_SIXEL
x_picture_manager_t *  x_picture_manager_new(void) ;

int  x_picture_manager_delete( x_picture_manager_t *  pic_man) ;

int  x_picture_manager_add( x_picture_manager_t *  pic_man ,
			x_picture_t *  pic , int  x , int  y) ;

int  x_picture_manager_remove( x_picture_manager_t *  pic_man ,
			int  x , int  y , int  width , int  height) ;

int  x_picture_manager_redraw( x_picture_manager_t *  pic_man , x_window_t *  win ,
			int  x , int  y , u_int  width , u_int  height) ;

int  x_picture_manager_scroll( x_picture_manager_t *  pic_man ,
			int  beg_y , int  end_y , int  height) ;
#endif


#endif
