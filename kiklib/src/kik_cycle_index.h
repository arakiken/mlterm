/*
 *	$Id$
 */

#ifndef  __KIK_CYCLE_INDEX_H__
#define  __KIK_CYCLE_INDEX_H__


#include  "kik_types.h"		/* size_t */


typedef struct  kik_cycle_index
{
	int  start ;
	int  next ;
	int  is_init ;

	u_int  size ;

} kik_cycle_index_t ;


kik_cycle_index_t *  kik_cycle_index_new( u_int   size) ;

int  kik_cycle_index_delete( kik_cycle_index_t *  cycle) ;
	
int  kik_cycle_index_reset( kik_cycle_index_t *  cycle) ;

int  kik_cycle_index_change_size( kik_cycle_index_t *  cycle , u_int  new_size) ;

u_int  kik_get_cycle_index_size( kik_cycle_index_t *  cycle) ;

u_int  kik_get_filled_cycle_index( kik_cycle_index_t *  cycle) ;

int  kik_cycle_index_of( kik_cycle_index_t *  cycle , int  at) ;

int  kik_next_cycle_index( kik_cycle_index_t *  cycle) ;


#endif
