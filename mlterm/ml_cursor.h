/*
 *	$Id$
 */

#ifndef  __ML_CURSOR_H__
#define  __ML_CURSOR_H__


#include  "ml_model.h"


typedef struct  ml_cursor
{
	/*
	 * XXX
	 * Following members are modified directly in ml_logical_visual.c
	 * without ml_cursor_xxx() functions.
	 */

	/*
	 * public (readonly)
	 */
	int  row ;
	int  char_index ;
	int  col ;
	int  col_in_char ;

	/*
	 * private
	 */
	int  saved_row ;
	int  saved_char_index ;
	int  saved_col ;
	int8_t  is_saved ;

	ml_model_t *  model ;
	
} ml_cursor_t ;


int  ml_cursor_init( ml_cursor_t *  cursor , ml_model_t *  model) ;

int  ml_cursor_final( ml_cursor_t *  cursor) ;

int  ml_cursor_goto_by_char( ml_cursor_t *  cursor , int  char_index , int  row) ;

int  ml_cursor_moveh_by_char( ml_cursor_t *  cursor , int  char_index) ;

int  ml_cursor_goto_by_col( ml_cursor_t *  cursor , int  col , int  row) ;

int  ml_cursor_moveh_by_col( ml_cursor_t *  cursor , int  col) ;

int  ml_cursor_goto_home( ml_cursor_t *  cursor) ;

int  ml_cursor_goto_beg_of_line( ml_cursor_t *  cursor) ;

int  ml_cursor_reset_col_in_char( ml_cursor_t *  cursor) ;

int  ml_cursor_go_forward( ml_cursor_t *  cursor) ;

int  ml_cursor_cr_lf( ml_cursor_t *  cursor) ;

ml_line_t *  ml_get_cursor_line( ml_cursor_t *  cursor) ;

ml_char_t *  ml_get_cursor_char( ml_cursor_t *  cursor) ;

int  ml_cursor_char_is_cleared( ml_cursor_t *  cursor) ;

int  ml_cursor_left_chars_in_line_are_cleared( ml_cursor_t *  cursor) ;

int  ml_cursor_save( ml_cursor_t *  cursor) ;

int  ml_cursor_restore( ml_cursor_t *  cursor) ;

#ifdef DEBUG

void  ml_cursor_dump( ml_cursor_t *  cursor) ;

#endif

#endif

