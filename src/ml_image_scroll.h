/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_SCROLL_H__
#define  __ML_IMAGE_SCROLL_H__


#include  "ml_image.h"


int  ml_imgscrl_scroll_upward( ml_image_t *  image , u_int  size) ;

int  ml_imgscrl_scroll_downward( ml_image_t *  image , u_int  size) ;

inline int  ml_is_scroll_upperlimit( ml_image_t *  image , int  row) ;

inline int  ml_is_scroll_lowerlimit( ml_image_t *  image , int  row) ;

int  ml_imgscrl_delete_line( ml_image_t *  image) ;

int  ml_imgscrl_insert_new_line( ml_image_t *  image) ;


#endif
