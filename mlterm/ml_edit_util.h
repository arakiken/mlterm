/*
 *	$Id$
 */

#ifndef  __ML_EDIT_UTIL_H__
#define  __ML_EDIT_UTIL_H__


#include  <kiklib/kik_debug.h>

#include  "ml_edit.h"


#define  CURSOR_LINE( edit)  ml_get_cursor_line( &(edit)->cursor)
#define  CURSOR_CHAR( edit)  ml_get_cursor_char( &(edit)->cursor)


int  ml_edit_clear_lines( ml_edit_t *  edit , int  start , u_int  size) ;

int  ml_edit_copy_lines( ml_edit_t *  edit , int  dst_row , int  src_row , u_int  size ,
	int  mark_changed) ;


#endif
