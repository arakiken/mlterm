/*
 *	$Id$
 */

#ifndef  __ML_EDIT_SCROLL_H__
#define  __ML_EDIT_SCROLL_H__


#include  "ml_edit.h"


int  ml_edsl_scroll_upward( ml_edit_t *  edit , u_int  size) ;

int  ml_edsl_scroll_downward( ml_edit_t *  edit , u_int  size) ;

#if  0
int  ml_edsl_scroll_upward_in_all( ml_edit_t *  edit , u_int  size) ;

int  ml_edsl_scroll_downward_in_all( ml_edit_t *  edit , u_int  size) ;
#endif

int  ml_is_scroll_upperlimit( ml_edit_t *  edit , int  row) ;

int  ml_is_scroll_lowerlimit( ml_edit_t *  edit , int  row) ;

int  ml_edsl_delete_line( ml_edit_t *  edit) ;

int  ml_edsl_insert_new_line( ml_edit_t *  edit) ;


#endif
