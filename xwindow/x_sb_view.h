/*
 *	$Id$
 */

/*
 * !! Notice !!
 * this file must be kept as independent to specific systems as possible.
 * so u_xxx which may not defined must not be used.
 */
 
#ifndef  __X_SB_VIEW_H__
#define  __X_SB_VIEW_H__


#include  <X11/Xlib.h>


typedef struct  x_sb_view
{
	Display *  display ;
	int  screen ;
	Window  window ;
	GC  gc ;
	unsigned int  height ;
	
	void (*get_geometry_hints)( struct  x_sb_view * , unsigned int *  width ,
		unsigned int *  top_margin , unsigned int *  bottom_margin ,
		int *  up_button_y , unsigned int *  up_button_height ,
		int *  down_button_y , unsigned int *  down_button_height) ;
	void (*get_default_color)( struct x_sb_view * , char **  fg_color , char **  bg_color) ;
	
	void (*realized)( struct  x_sb_view *  , Display * , int  screen , Window ,
		GC , unsigned int  height) ;
	void (*resized)( struct x_sb_view * , Window , unsigned int  height) ;
	void (*delete)( struct  x_sb_view *) ;
	
	void (*draw_decoration)( struct  x_sb_view *) ;
	void (*draw_scrollbar)( struct  x_sb_view * , int  bar_top_y , unsigned int  bar_height) ;
	
	void (*up_button_pressed)( struct  x_sb_view *) ;
	void (*down_button_pressed)( struct  x_sb_view *) ;
	void (*up_button_released)( struct  x_sb_view *) ;
	void (*down_button_released)( struct  x_sb_view *) ;

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
	u_int  rc_num ;
	u_int  use_count ;
} x_sb_view_conf_t ;


#endif
