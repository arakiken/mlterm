/*
 *	update: <2001/7/11(12:59:07)>
 *	$Id$
 */

#ifndef  __ML_IMAGE_SCROLL_H__
#define  __ML_IMAGE_SCROLL_H__


#include  "ml_image.h"


int  ml_image_set_scroll_region( ml_image_t *  image , int  beg , int  end) ;

int  ml_image_scroll_upward( ml_image_t *  image , u_int  size) ;

int  ml_image_scroll_downward( ml_image_t *  image , u_int  size) ;

int  ml_image_insert_new_line( ml_image_t *  image) ;

int  ml_image_delete_line( ml_image_t *  image) ;

inline int  ml_is_scroll_upperlimit( ml_image_t *  image , int  row) ;

inline int  ml_is_scroll_lowerlimit( ml_image_t *  image , int  row) ;


#endif
