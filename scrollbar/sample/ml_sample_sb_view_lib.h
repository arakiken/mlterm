/*
 *	$Id$
 */

#ifndef  __ML_SAMPLE_SB_VIEW_LIB_H__
#define  __ML_SAMPLE_SB_VIEW_LIB_H__


#include  <ml_sb_view.h>


typedef struct  sample_sb_view
{
	ml_sb_view_t  view ;

	GC  gc ;
	
	Pixmap  arrow_up ;
	Pixmap  arrow_up_dent ;
	Pixmap  arrow_down ;
	Pixmap  arrow_down_dent ;

} sample_sb_view_t ;


Pixmap  ml_get_icon_pixmap( ml_sb_view_t *  view , GC  gc_intern ,
	char **  data , unsigned int  width , unsigned int  height) ;


#endif
