/*
 *	$Id$
 */

/*
 * !! Notice !!
 * This file must be kept as independent to specific systems as possible.
 * So types like u_xxx which may not be defined in some environments must
 * not be used here.
 */
 
#ifndef  __X_SB_VIEW_H__
#define  __X_SB_VIEW_H__


#include  "x.h"


typedef struct x_display *  __x_display_ptr_t ;
typedef struct x_window *  __x_window_ptr_t ;

typedef struct  x_sb_view
{
	Display *  display ;
	int  screen ;
	Window  window ;
	GC  gc ;	/* If you change gc values in x_sb_view, restore them before return. */
	unsigned int  height ;
	
	/*
	 * Set 1 when create x_sb_view_t.
	 * x_sb_view_t of version 0 doesn't have this 'version' member, so
	 * x_sb_view_t->version designates x_sb_view->get_geometry_hints actually.
	 * It is assumed that x_sb_view_t->version of version 0 is not 1.
	 */
	int  version ;
	
	void (*get_geometry_hints)( struct  x_sb_view * , unsigned int *  width ,
		unsigned int *  top_margin , unsigned int *  bottom_margin ,
		int *  up_button_y , unsigned int *  up_button_height ,
		int *  down_button_y , unsigned int *  down_button_height) ;
	void (*get_default_color)( struct x_sb_view * ,
		char **  fg_color , char **  bg_color) ;

	/* Win32: GC is None. */	
	void (*realized)( struct  x_sb_view *  , Display * , int  screen , Window ,
		GC , unsigned int  height) ;
	void (*resized)( struct x_sb_view * , Window , unsigned int  height) ;
	void (*color_changed)( struct x_sb_view * , int) ;
	void (*delete)( struct  x_sb_view *) ;

	/*
	 * Win32: x_sb_view_t::gc is set by x_scrollbar.c before following draw_XXX
	 *        functions is called.
	 */
	
	/* drawing bar only. */	
	void (*draw_scrollbar)( struct  x_sb_view * ,
		int  bar_top_y , unsigned int  bar_height) ;
	/* drawing background of bar. */
	void (*draw_background)( struct  x_sb_view * , int , unsigned int) ;
	void (*draw_up_button)( struct  x_sb_view * , int) ;
	void (*draw_down_button)( struct  x_sb_view * , int) ;

	/* x_scrollbar sets this after x_*_sb_view_new(). */
	__x_window_ptr_t  win ;

} x_sb_view_t ;

typedef struct x_sb_view_rc
{
	char *  key ;
	char *  value ;
	
}  x_sb_view_rc_t ;

typedef struct  x_sb_view_conf
{
	char *  sb_name ;
	char *  engine_name ;
	char *  dir ;
	x_sb_view_rc_t *  rc ;
	unsigned int  rc_num ;
	unsigned int  use_count ;

	int  (*load_image)( __x_display_ptr_t  disp , char *  path ,
		/* u_int32_t */ unsigned int **  cardinal ,
		Pixmap *  pixmap , Pixmap *  mask ,
		unsigned int *  width , unsigned int *  height) ;

} x_sb_view_conf_t ;


#endif
