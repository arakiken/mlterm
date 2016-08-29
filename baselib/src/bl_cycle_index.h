/*
 *	$Id$
 */

#ifndef __BL_CYCLE_INDEX_H__
#define __BL_CYCLE_INDEX_H__

#include "bl_types.h" /* size_t */

typedef struct bl_cycle_index {
  int start;
  int next;
  int is_init;

  u_int size;

} bl_cycle_index_t;

bl_cycle_index_t* bl_cycle_index_new(u_int size);

int bl_cycle_index_delete(bl_cycle_index_t* cycle);

int bl_cycle_index_reset(bl_cycle_index_t* cycle);

int bl_cycle_index_change_size(bl_cycle_index_t* cycle, u_int new_size);

u_int bl_get_cycle_index_size(bl_cycle_index_t* cycle);

u_int bl_get_filled_cycle_index(bl_cycle_index_t* cycle);

int bl_cycle_index_of(bl_cycle_index_t* cycle, int at);

int bl_next_cycle_index(bl_cycle_index_t* cycle);

#endif
