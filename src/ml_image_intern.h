/*
 *	$Id$
 */

#ifndef  __ML_IMAGE_INTERN_H__
#define  __ML_IMAGE_INTERN_H__


#include  <kiklib/kik_debug.h>

#include  "ml_image.h"


#define  IMAGE_LINE_INTERN(image,row) \
	((image)->lines[(\
		(image)->beg_row + (row) >= (image)->num_of_rows ? \
			((image)->beg_row + (row)) - (image)->num_of_rows : \
			(image)->beg_row + (row) \
		)])
		

#ifdef  __DEBUG

/* this does range check (XXX this doesn't confirm to ANSI C) */
#define  IMAGE_LINE(image,row) \
	( (row) < 0 && kik_warn_printf( KIK_DEBUG_TAG " row %d is less than 0.\n" , row) ? \
		IMAGE_LINE_INTERN(image,0) : \
		(image)->num_of_rows <= row && \
		kik_warn_printf( KIK_DEBUG_TAG " row %d is over ml_image_t::num_of_rows.\n" , row) ? \
		IMAGE_LINE_INTERN(image,image->num_of_rows) : IMAGE_LINE_INTERN(image,row))

#else

/* !! NOTICE !! this doesn't do range check. */
#define  IMAGE_LINE(image,row) IMAGE_LINE_INTERN(image,row)

#endif


#ifdef  DEBUG

#define  END_ROW(image)  ((image)->num_of_filled_rows - 1)

#else

#define  END_ROW(image) \
	((image)->num_of_filled_rows < 1 && \
		kik_warn_printf( KIK_DEBUG_TAG " num_of_filled_rows is less than 1.\n") ? \
		0 : (image)->num_of_filled_rows - 1)

#endif


#define  CURSOR_LINE( image)  (IMAGE_LINE(image,(image)->cursor.row))

#define  CURSOR_CHAR( image) (CURSOR_LINE(image).chars[(image)->cursor.char_index])

#define  END_LINE( image)  (IMAGE_LINE(image,END_ROW(image)))


int  ml_image_clear_line( ml_image_t *  image , int  row , int  char_index) ;

int  ml_image_clear_lines( ml_image_t *  image , int  start , u_int  size) ;

int  ml_image_copy_lines( ml_image_t *  image , int  dst_row , int  src_row , u_int  size ,
	int  mark_changed) ;


#endif
