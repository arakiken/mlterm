/*
 *	update: <2001/11/26(02:32:57)>
 *	$Id$
 */

#include  "ml_line_hints.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/* --- global functions --- */

int
ml_line_hints_init(
	ml_line_hints_t *  hints ,
	u_int  num_of_lines
	)
{
	if( ( hints->lines = malloc( sizeof( *hints->lines) * num_of_lines)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}

	if( ( hints->index = kik_cycle_index_new( num_of_lines)) == NULL)
	{
		return  0 ;
	}
	
	return  1 ;
}

int
ml_line_hints_final(
	ml_line_hints_t *  hints
	)
{
	free( hints->lines) ;

	kik_cycle_index_delete( hints->index) ;

	return  1 ;
}

int
ml_line_hints_reset(
	ml_line_hints_t *  hints
	)
{
	return  kik_cycle_index_reset( hints->index) ;
}

int
ml_line_hints_add(
	ml_line_hints_t *  hints ,
	int  beg_of_line ,
	u_int  len ,
	u_int  width
	)
{
	int  at ;

	at = kik_next_cycle_index( hints->index) ;
	
	hints->lines[ at].beg_of_line = beg_of_line ;
	hints->lines[ at].len = len ;
	hints->lines[ at].width = width ;

	return  1 ;
}

int
ml_line_hints_set(
	ml_line_hints_t *  hints ,
	int  beg_of_line ,
	u_int  len ,
	u_int  width ,
	int  at
	)
{
	if( ( at = kik_cycle_index_of( hints->index , at)) == -1)
	{
		return  0 ;
	}
	
	hints->lines[ at].beg_of_line = beg_of_line ;
	hints->lines[ at].len = len ;
	hints->lines[ at].width = width ;

	return  1 ;
}

int
ml_line_hints_at(
	ml_line_hints_t *  hints ,
	int *  beg_of_line ,
	u_int *  len ,
	u_int *  width ,
	int  at
	)
{
	if( ( at = kik_cycle_index_of( hints->index , at)) == -1)
	{
		return  0 ;
	}

	*beg_of_line = hints->lines[at].beg_of_line ;
	*len = hints->lines[at].len ;
	*width = hints->lines[at].width ;

	return  1 ;
}

int
ml_get_beg_of_line_hint(
	ml_line_hints_t *  hints ,
	int  at
	)
{
	if( ( at = kik_cycle_index_of( hints->index , at)) == -1)
	{
		return  0 ;
	}
	
	return  hints->lines[at].beg_of_line ;
}

u_int
ml_get_line_len_hint(
	ml_line_hints_t *  hints ,
	int  at
	)
{
	if( ( at = kik_cycle_index_of( hints->index , at)) == -1)
	{
		return  0 ;
	}

	return  hints->lines[at].len ;
}

u_int
ml_get_line_width_hint(
	ml_line_hints_t *  hints ,
	int  at
	)
{
	if( ( at = kik_cycle_index_of( hints->index , at)) == -1)
	{
		return  0 ;
	}

	return  hints->lines[at].width ;
}

u_int
ml_get_num_of_lines_by_hints(
	ml_line_hints_t *  hints
	)
{
	return  kik_get_filled_cycle_index( hints->index) ;
}
