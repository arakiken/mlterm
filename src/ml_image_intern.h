/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_INTERN_H__
#define  __ML_IMAGE_INTERN_H__


#include  <kiklib/kik_debug.h>

#include  "ml_image.h"


#define  CURSOR_LINE( image)  (ml_imgmdl_get_line( &(image)->model,(image)->cursor.row))
#define  CURSOR_CHAR( image)  (&CURSOR_LINE(image)->chars[(image)->cursor.char_index])


int  ml_image_clear_line( ml_image_t *  image , int  row , int  char_index) ;

int  ml_image_clear_lines( ml_image_t *  image , int  start , u_int  size) ;

int  ml_image_copy_lines( ml_image_t *  image , int  dst_row , int  src_row , u_int  size ,
	int  mark_changed) ;


#endif
