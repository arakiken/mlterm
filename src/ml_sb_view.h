/*
 *	$Id$
 */

/*
 * !! Notice !!
 * this file must be kept as independent to specific systems as possible.
 * so u_xxx which may not defined must not be used.
 */
 
#ifndef  __ML_SB_VIEW_H__
#define  __ML_SB_VIEW_H__


#include  <X11/Xlib.h>


typedef struct  ml_sb_view
{
	Display *  display ;
	int  screen ;
	Window  window ;
	GC  gc ;
	unsigned int  height ;
	
	void (*get_geometry_hints)( struct  ml_sb_view * , unsigned int *  width ,
		unsigned int *  top_margin , unsigned int *  bottom_margin ,
		int *  up_button_y , unsigned int *  up_button_height ,
		int *  down_button_y , unsigned int *  down_button_height) ;
	void (*get_default_color)( struct ml_sb_view * , char **  fg_color , char **  bg_color) ;
	
	void (*realized)( struct  ml_sb_view *  , Display * , int  screen , Window ,
		GC , unsigned int  height) ;
	void (*resized)( struct ml_sb_view * , Window , unsigned int  height) ;
	void (*delete)( struct  ml_sb_view *) ;
	
	void (*draw_decoration)( struct  ml_sb_view *) ;
	void (*draw_scrollbar)( struct  ml_sb_view * , int  bar_top_y , unsigned int  bar_height) ;
	
	void (*up_button_pressed)( struct  ml_sb_view *) ;
	void (*down_button_pressed)( struct  ml_sb_view *) ;
	void (*up_button_released)( struct  ml_sb_view *) ;
	void (*down_button_released)( struct  ml_sb_view *) ;

} ml_sb_view_t ;


#endif
