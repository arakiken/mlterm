/*
 *	$Id$
 */

#ifndef  __ML_EDIT_INTERN_H__
#define  __ML_EDIT_INTERN_H__


#include  <kiklib/kik_debug.h>

#include  "ml_edit.h"


#define  CURSOR_LINE( edit)  (ml_model_get_line( &(edit)->model,(edit)->cursor.row))
#define  CURSOR_CHAR( edit)  (&CURSOR_LINE(edit)->chars[(edit)->cursor.char_index])


int  ml_edit_clear_line( ml_edit_t *  edit , int  row , int  char_index) ;

int  ml_edit_clear_lines( ml_edit_t *  edit , int  start , u_int  size) ;

int  ml_edit_copy_lines( ml_edit_t *  edit , int  dst_row , int  src_row , u_int  size ,
	int  mark_changed) ;


#endif
