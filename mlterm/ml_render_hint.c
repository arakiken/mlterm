/*
 *	$Id$
 */

#include  "ml_render_hint.h"

#include  <kiklib/kik_mem.h>
#include  <kiklib/kik_debug.h>


/* --- global functions --- */

int
ml_render_hint_init(
	ml_render_hint_t *  hints ,
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
ml_render_hint_final(
	ml_render_hint_t *  hints
	)
{
	free( hints->lines) ;

	kik_cycle_index_delete( hints->index) ;

	return  1 ;
}

int
ml_render_hint_reset(
	ml_render_hint_t *  hints
	)
{
	return  kik_cycle_index_reset( hints->index) ;
}

int
ml_render_hint_change_size(
	ml_render_hint_t *   hints ,
	u_int  num_of_lines
	)
{
	if( kik_get_cycle_index_size( hints->index) == num_of_lines)
	{
		return  1 ;
	}

	free( hints->lines) ;
	
	if( ( hints->lines = malloc( sizeof( *hints->lines) * num_of_lines)) == NULL)
	{
	#ifdef  DEBUG
		kik_warn_printf( KIK_DEBUG_TAG " malloc() failed.\n") ;
	#endif

		return  0 ;
	}
	
	return  kik_cycle_index_change_size( hints->index , num_of_lines) ;
}

int
ml_render_hint_add(
	ml_render_hint_t *  hints ,
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
ml_render_hint_at(
	ml_render_hint_t *  hints ,
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

	if( beg_of_line)
	{
		*beg_of_line = hints->lines[at].beg_of_line ;
	}

	if( len)
	{
		*len = hints->lines[at].len ;
	}

	if( width)
	{
		*width = hints->lines[at].width ;
	}

	return  1 ;
}

u_int
ml_get_num_of_lines_by_hints(
	ml_render_hint_t *  hints
	)
{
	return  kik_get_filled_cycle_index( hints->index) ;
}
