/*
 *	$Id$
 */

#ifndef  __KIK_MAP_H__
#define  __KIK_MAP_H__


#include  <string.h>		/* memset */

#include  "kik_types.h"		/* size_t */
#include  "kik_debug.h"
#include  "kik_mem.h"


#define  DEFAULT_MAP_SIZE  128


#define  KIK_PAIR( name)  __ ## name ## _pair_t

#define  KIK_PAIR_TYPEDEF( name , key_type , val_type) \
typedef struct  __ ## name ## _pair \
{ \
	int  is_filled ; \
	key_type  key ; \
	val_type  value ; \
	\
} *  __ ## name ## _pair_t


#define  KIK_MAP( name)  __ ## name ## _map_t

#define  KIK_MAP_TYPEDEF( name , key_type , val_type) \
KIK_PAIR_TYPEDEF( name , key_type , val_type) ; \
typedef struct  __ ## name ## _map \
{ \
	KIK_PAIR( name) pairs ; \
	KIK_PAIR( name) *  pairs_array ; \
	u_int  map_size ; \
	u_int  filled_size ; \
	int  (*hash_func)( key_type , u_int) ; \
	int  (*compare_func)( key_type , key_type) ; \
	\
} *  __ ## name ## _map_t


#define  kik_map_new_with_size( key_type , val_type , map , __hash_func , __compare_func , size) \
{ \
	if( ( map = malloc( sizeof( *(map)))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_map_new().\n") ; \
		abort() ; \
	} \
	\
	if( ( (map)->pairs = malloc( size * sizeof( *(map)->pairs))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_map_new().\n") ; \
		abort() ; \
	} \
	memset( (map)->pairs , 0 , size * sizeof( *(map)->pairs)) ; \
	\
	if( ( (map)->pairs_array = malloc( size * sizeof(void*))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_map_new().\n") ; \
		abort() ; \
	} \
	memset( (map)->pairs_array , 0 , size * sizeof(void*)) ; \
	\
	(map)->map_size = size ; \
	(map)->filled_size = 0 ; \
	(map)->hash_func = __hash_func ; \
	(map)->compare_func = __compare_func ; \
}

#define  kik_map_new( key_type , val_type , map , __hash_func , __compare_func) \
	kik_map_new_with_size( key_type , val_type , map , __hash_func , __compare_func , DEFAULT_MAP_SIZE)

/*
 * the deletion of pair->key/pair->value should be done by users of kik_map.
 */
#define  kik_map_delete( map) \
{ \
	free( map->pairs) ; \
	free( map->pairs_array) ; \
	free( map) ; \
}

#define  kik_map_get( result , map , __key , __pair_p) \
{ \
	int  hash_key ; \
	int  count ; \
	\
	__pair_p = NULL ; \
	result = 0 ; \
	\
	hash_key = (*(map)->hash_func)( __key , (map)->map_size) ; \
	for( count = 0 ; count < (map)->map_size ; count ++) \
	{ \
		if( (map)->pairs[hash_key].is_filled && \
			(*(map)->compare_func)( __key , (map)->pairs[hash_key].key)) \
		{ \
			__pair_p = &(map)->pairs[hash_key] ; \
			result = 1 ; \
			\
			break ; \
		} \
		\
		hash_key = kik_map_rehash( hash_key , (map)->map_size) ; \
	} \
}


#if  0

#define  kik_map_dump_size(map_size,new_size) \
	kik_debug_printf( "reallocating map size from %d to %d.\n" , map_size , new_size)

#else

#define  kik_map_dump_size(map_size,new_size)

#endif


#define  kik_map_set( result , map , __key , __value) \
{ \
	int  hash_key ; \
	int  count ; \
	\
	result = 0 ; \
	\
	while( 1) \
	{ \
		hash_key = (*(map)->hash_func)( __key , (map)->map_size) ; \
		for( count = 0 ; count < (map)->map_size ; count ++) \
		{ \
			if( ! (map)->pairs[hash_key].is_filled) \
			{ \
				(map)->pairs[hash_key].key = __key ; \
				(map)->pairs[hash_key].value = __value ; \
				(map)->pairs[hash_key].is_filled = 1 ; \
				(map)->pairs_array[(map)->filled_size ++] = &(map)->pairs[hash_key] ; \
				result = 1 ; \
				\
				break ; \
			} \
			\
			hash_key = kik_map_rehash( hash_key , (map)->map_size) ; \
		} \
		\
		if( result == 1) \
		{ \
			/* exiting while(1) loop */ \
			break ; \
		} \
		else \
		{ \
			/* \
			 * expanding map by DEFAULT_MAP_SIZE \
			 */ \
			\
			u_int  new_size ; \
			int  array_index ; \
			\
			new_size = (map)->map_size + DEFAULT_MAP_SIZE ; \
			\
			kik_map_dump_size((map)->map_size,new_size) ; \
			\
			if( ( (map)->pairs = realloc( (map)->pairs , new_size * sizeof( *(map)->pairs))) == NULL) \
			{ \
				kik_error_printf( "realloc() failed in kik_map_set().\n") ; \
				abort() ; \
			} \
			\
			if( ( (map)->pairs_array = realloc( (map)->pairs_array , new_size * sizeof(void*))) == NULL) \
			{ \
				kik_error_printf( "realloc() failed in kik_map_set().\n") ; \
				abort() ; \
			} \
			\
			array_index = 0 ; \
			for( count = 0 ; count < (map)->map_size ; count ++) \
			{ \
				if( (map)->pairs[count].is_filled) \
				{ \
					(map)->pairs_array[array_index++] = &(map)->pairs[count] ; \
				} \
			} \
			\
			(map)->map_size = new_size ; \
		} \
	} \
}

#define  kik_map_erase( result , map , __key) \
{ \
	int  hash_key ; \
	int  count ; \
	\
	result = 0 ; \
	\
	hash_key = (*(map)->hash_func)( __key , (map)->map_size) ; \
	for( count = 0 ; count < (map)->map_size ; count ++) \
	{ \
		if( (map)->pairs[hash_key].is_filled && \
			(*(map)->compare_func)( __key , (map)->pairs[hash_key].key)) \
		{ \
			int  count2 ; \
			\
			for( count2 = 0 ; count2 < (map)->filled_size ; count2 ++) \
			{ \
				if( (map)->pairs_array[count2] == &(map)->pairs[hash_key]) \
				{ \
					if( count2 + 1 < (map)->filled_size) \
					{ \
						/* moving the last element to the pos of the erased */ \
						(map)->pairs_array[count2] = \
							(map)->pairs_array[(map)->filled_size - 1] ; \
					} \
					(map)->filled_size -- ; \
					\
					break ; \
				} \
			} \
			\
			(map)->pairs[hash_key].is_filled = 0 ; \
			result = 1 ; \
			\
			break ; \
		} \
		\
		hash_key = kik_map_rehash( hash_key , (map)->map_size) ; \
	} \
	\
	if( result == 1) \
	{ \
		/* \
		 * if (map)->filled_size is (DEFAULT_MAP_SIZE * 2) smaller than the map size , \
		 * the map size is (DEFAULT_MAP_SIZE) shrinked.
		 * the difference(DEFAULT_MAP_SIZE) is buffered to reduce calling realloc().
		 */ \
		if( (map)->filled_size + (DEFAULT_MAP_SIZE * 2) < (map)->map_size) \
		{ \
			/* \
			 * shrinking map by DEFAULT_MAP_SIZE \
			 */ \
			\
			u_int  new_size ; \
			int  array_index ; \
			\
			new_size = (map)->map_size - DEFAULT_MAP_SIZE ; \
			\
			kik_map_dump_size((map)->map_size,new_size) ; \
			\
			if( ( (map)->pairs = realloc( (map)->pairs , new_size * sizeof( *(map)->pairs))) == NULL) \
			{ \
				kik_error_printf( "realloc() failed in kik_map_set().\n") ; \
				abort() ; \
			} \
			\
			if( ( (map)->pairs_array = realloc( (map)->pairs_array , new_size * sizeof(void*))) == NULL) \
			{ \
				kik_error_printf( "realloc() failed in kik_map_set().\n") ; \
				abort() ; \
			} \
			\
			array_index = 0 ; \
			for( count = 0 ; count < (map)->map_size ; count ++) \
			{ \
				if( (map)->pairs[count].is_filled) \
				{ \
					(map)->pairs_array[array_index++] = &(map)->pairs[count] ; \
				} \
			} \
			\
			(map)->map_size = new_size ; \
		} \
	} \
}

#define  kik_map_get_pairs_array( map , array , size) \
{ \
	array = (map)->pairs_array ; \
	size = (map)->filled_size ; \
}


int  kik_map_rehash( int  hash_key , u_int  size) ;


/*
 * preparing useful hash functions.
 */

int  kik_map_hash_str( char *  key , u_int  size) ;

int  kik_map_hash_int( int  key , u_int  size) ;


/*
 * preparing useful compare functions.
 */
int  kik_map_compare_str( char *  key1 , char *  key2) ;

int  kik_map_compare_int( int  key1 , int  key2) ;


#endif
