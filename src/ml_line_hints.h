/*
 *	update: <2001/11/11(17:52:28)>
 *	$Id$
 */

#ifndef  __ML_LINE_HINTS_H__
#define  __ML_LINE_HINTS_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_cycle_index.h>


typedef struct  ml_line_hints
{
	struct
	{
		int  beg_of_line ;
		u_int  len ;
		u_int  width ;
		
	} *  lines ;

	kik_cycle_index_t *  index ;
	
} ml_line_hints_t ;


int  ml_line_hints_init( ml_line_hints_t *  hints , u_int  num_of_lines) ;

int  ml_line_hints_final( ml_line_hints_t *  hints) ;

int  ml_line_hints_reset( ml_line_hints_t *  hints) ;

int  ml_line_hints_set( ml_line_hints_t *  hints , int  beg_of_line ,
	u_int  len , u_int  width , int  at) ;
	
int  ml_line_hints_add( ml_line_hints_t *  hints , int  beg_of_line , u_int  len , u_int  width) ;

int  ml_line_hints_at( ml_line_hints_t *  hints , int *  beg_of_line , u_int *  len ,
	u_int *  width , int  at) ;

int  ml_get_beg_of_line_hint( ml_line_hints_t *  hints , int  at) ;

u_int  ml_get_line_len_hint( ml_line_hints_t *  hints , int  at) ;

u_int  ml_get_line_width_hint( ml_line_hints_t *  hints , int  at) ;

u_int  ml_get_num_of_lines_by_hints( ml_line_hints_t *  hints) ;


#endif
