/*
 *	$Id$
 */

#ifndef  __X_SAMPLE_SB_VIEW_LIB_H__
#define  __X_SAMPLE_SB_VIEW_LIB_H__


#include  <x_sb_view.h>


typedef struct  sample_sb_view
{
	x_sb_view_t  view ;

	GC  gc ;
	
	Pixmap  arrow_up ;
	Pixmap  arrow_up_dent ;
	Pixmap  arrow_down ;
	Pixmap  arrow_down_dent ;

} sample_sb_view_t ;


Pixmap  x_get_icon_pixmap( x_sb_view_t *  view , GC  gc_intern ,
	char **  data , unsigned int  width , unsigned int  height) ;


#endif
