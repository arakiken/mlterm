/*
 *	$Id$
 */

#ifndef  __ML_RENDER_HINT_H__
#define  __ML_RENDER_HINT_H__


#include  <kiklib/kik_types.h>
#include  <kiklib/kik_cycle_index.h>


typedef struct  ml_render_hint
{
	struct
	{
		int  beg_of_line ;
		u_int  len ;
		u_int  width ;
		
	} *  lines ;

	kik_cycle_index_t *  index ;

} ml_render_hint_t ;


int  ml_render_hint_init( ml_render_hint_t *  hints , u_int  num_of_lines) ;

int  ml_render_hint_final( ml_render_hint_t *  hints) ;

int  ml_render_hint_reset( ml_render_hint_t *  hints) ;

int  ml_render_hint_change_size( ml_render_hint_t *  hints , u_int  num_of_lines) ;

int  ml_render_hint_add( ml_render_hint_t *  hints , int  beg_of_line , u_int  len , u_int  width) ;

int  ml_render_hint_at( ml_render_hint_t *  hints , int *  beg_of_line , u_int *  len ,
	u_int *  width , int  at) ;

u_int  ml_get_num_of_lines_by_hints( ml_render_hint_t *  hints) ;


#endif
