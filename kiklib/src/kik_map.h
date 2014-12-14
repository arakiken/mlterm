/*
 *	$Id$
 */

#ifndef  __KIK_MAP_H__
#define  __KIK_MAP_H__


#include  <string.h>		/* memset */

#include  "kik_types.h"		/* size_t */
#include  "kik_debug.h"
#include  "kik_mem.h"


#define  DEFAULT_MAP_SIZE  16


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
	if( ( map = malloc( sizeof( *(map)))) == NULL || \
	    ( (map)->pairs = calloc( size , sizeof( *(map)->pairs))) == NULL) \
	{ \
		kik_error_printf( "malloc() failed in kik_map_new().\n") ; \
		abort() ; \
	} \
	\
	(map)->pairs_array = NULL ; \
	(map)->map_size = size ; \
	(map)->filled_size = 0 ; \
	if( __hash_func == kik_map_hash_int) \
	{ \
		if( size & (size - 1)) \
		{ \
			(map)->hash_func = kik_map_hash_int ; \
		} \
		else \
		{ \
			/* new_size == 2^n */ \
			(map)->hash_func = kik_map_hash_int_fast ; \
		} \
	} \
	else \
	{ \
		(map)->hash_func = __hash_func ; \
	} \
	(map)->compare_func = __compare_func ; \
}

#define  kik_map_new( key_type , val_type , map , __hash_func , __compare_func) \
	kik_map_new_with_size( key_type , val_type , map , __hash_func , __compare_func , DEFAULT_MAP_SIZE)

/*
 * the deletion of pair->key/pair->value should be done by users of kik_map.
 */
#define  kik_map_delete( map) \
{ \
	free( (map)->pairs) ; \
	free( (map)->pairs_array) ; \
	free( map) ; \
}

#define  kik_map_get( map , __key , __pair_p) \
{ \
	int  __hash_key ; \
	u_int  __count ; \
	\
	__pair_p = NULL ; \
	\
	__hash_key = (*(map)->hash_func)( __key , (map)->map_size) ; \
	for( __count = 0 ; __count < (map)->map_size ; __count ++) \
	{ \
		if( (map)->pairs[__hash_key].is_filled && \
			(*(map)->compare_func)( __key , (map)->pairs[__hash_key].key)) \
		{ \
			__pair_p = &(map)->pairs[__hash_key] ; \
			\
			break ; \
		} \
		\
		__hash_key = kik_map_rehash( __hash_key , (map)->map_size) ; \
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
	int  __hash_key ; \
	u_int  __count ; \
	\
	result = 0 ; \
	\
	if( (map)->map_size == (map)->filled_size) \
	{ \
		/* \
		 * Expanding map by DEFAULT_MAP_SIZE \
		 */ \
		\
		u_int  __new_size ; \
		void *  __new ; \
		\
		__new_size = (map)->map_size + DEFAULT_MAP_SIZE ; \
		\
		kik_map_dump_size((map)->map_size,__new_size) ; \
		\
		if( ( __new = calloc( __new_size , sizeof( *(map)->pairs)))) \
		{ \
			void *  __old ; \
			\
			__old = (map)->pairs ; \
			\
			if( (map)->hash_func == kik_map_hash_int || \
			    (map)->hash_func == kik_map_hash_int_fast) \
			{ \
				if( __new_size & (__new_size - 1)) \
				{ \
					(map)->hash_func = kik_map_hash_int ; \
				} \
				else \
				{ \
					/* __new_size == 2^n */ \
					(map)->hash_func = kik_map_hash_int_fast ; \
				} \
			} \
			\
			/* reconstruct (map)->pairs since map_size is changed. */ \
			for( __count = 0 ; __count < (map)->map_size ; __count ++) \
			{ \
				if( (map)->pairs[__count].is_filled) \
				{ \
					void *  dst ; \
					\
					__hash_key = (*(map)->hash_func)( \
							(map)->pairs[__count].key , \
							__new_size) ; \
					\
					(map)->pairs = __new ; \
					while( (map)->pairs[__hash_key].is_filled) \
					{ \
						__hash_key = kik_map_rehash( __hash_key , \
								__new_size) ; \
					} \
					\
					dst = &(map)->pairs[__hash_key] ; \
					(map)->pairs = __old ; \
					memcpy( dst , &(map)->pairs[__count] , \
						sizeof( *(map)->pairs)) ; \
				} \
			} \
			\
			free( __old) ; \
			(map)->pairs = __new ; \
			(map)->map_size = __new_size ; \
		} \
	} \
	\
	__hash_key = (*(map)->hash_func)( __key , (map)->map_size) ; \
	for( __count = 0 ; __count < (map)->map_size ; __count ++) \
	{ \
		if( ! (map)->pairs[__hash_key].is_filled) \
		{ \
			(map)->pairs[__hash_key].key = __key ; \
			(map)->pairs[__hash_key].value = __value ; \
			(map)->pairs[__hash_key].is_filled = 1 ; \
			(map)->filled_size ++ ; \
			\
			free( (map)->pairs_array) ; \
			(map)->pairs_array = NULL ; \
			\
			result = 1 ; \
			\
			break ; \
		} \
		\
		__hash_key = kik_map_rehash( __hash_key , (map)->map_size) ; \
	} \
}

#define  __kik_map_erase_simple( result , map , __key) \
	int  __hash_key ; \
	u_int  __count ; \
	\
	result = 0 ; \
	\
	__hash_key = (*(map)->hash_func)( __key , (map)->map_size) ; \
	for( __count = 0 ; __count < (map)->map_size ; __count ++) \
	{ \
		if( (map)->pairs[__hash_key].is_filled && \
			(*(map)->compare_func)( __key , (map)->pairs[__hash_key].key)) \
		{ \
			(map)->pairs[__hash_key].is_filled = 0 ; \
			(map)->filled_size -- ; \
			\
			free( (map)->pairs_array) ; \
			(map)->pairs_array = NULL ; \
			\
			result = 1 ; \
			\
			break ; \
		} \
		\
		__hash_key = kik_map_rehash( __hash_key , (map)->map_size) ; \
	}

/*
 * Not shrink map.
 */
#define  kik_map_erase_simple( result , map , __key) \
{ \
	__kik_map_erase_simple( result , map , __key) ; \
}

/*
 * Shrink map.
 */
#define  kik_map_erase( result , map , __key) \
{ \
	__kik_map_erase_simple( result , map , __key) ; \
	\
	/* \
	 * __hash_key and __count are declared in __kik_map_erase_simple(). \
	 */ \
	\
	if( result == 1 && \
	    /* \
	     * if (map)->filled_size is (DEFAULT_MAP_SIZE * 2) smaller than the map size , \
	     * the map size is (DEFAULT_MAP_SIZE) shrinked. \
	     * the difference(DEFAULT_MAP_SIZE) is buffered to reduce calling realloc(). \
	     */ \
	    (map)->filled_size + (DEFAULT_MAP_SIZE * 2) < (map)->map_size) \
	{ \
		/* \
		 * shrinking map by DEFAULT_MAP_SIZE \
		 */ \
		\
		u_int  __new_size ; \
		void *  __old ; \
		void *  __new ; \
		u_int  __count ; \
		\
		__new_size = (map)->map_size - DEFAULT_MAP_SIZE ; \
		\
		kik_map_dump_size((map)->map_size,__new_size) ; \
		\
		if( ( __new = calloc( __new_size , sizeof( *(map)->pairs)))) \
		{ \
			__old = (map)->pairs ; \
			\
			if( (map)->hash_func == kik_map_hash_int || \
			    (map)->hash_func == kik_map_hash_int_fast) \
			{ \
				if( __new_size & (__new_size - 1)) \
				{ \
					(map)->hash_func = kik_map_hash_int ; \
				} \
				else \
				{ \
					/* __new_size == 2^n */ \
					(map)->hash_func = kik_map_hash_int_fast ; \
				} \
			} \
			\
			/* reconstruct (map)->pairs since map_size is changed. */ \
			for( __count = 0 ; __count < (map)->map_size ; __count ++) \
			{ \
				if( (map)->pairs[__count].is_filled) \
				{ \
					void *  dst ; \
					\
					__hash_key = (*(map)->hash_func)( \
							(map)->pairs[__count].key , \
							__new_size) ; \
					\
					(map)->pairs = __new ; \
					while( (map)->pairs[__hash_key].is_filled) \
					{ \
						__hash_key = kik_map_rehash( __hash_key , \
								__new_size) ; \
					} \
					\
					dst = &(map)->pairs[__hash_key] ; \
					(map)->pairs = __old ; \
					memcpy( dst , &(map)->pairs[__count] , \
						sizeof( *(map)->pairs)) ; \
				} \
			} \
			\
			free( __old) ; \
			(map)->pairs = __new ; \
			(map)->map_size = __new_size ; \
		} \
	} \
}

#define  kik_map_get_pairs_array( map , array , size) \
{ \
	size = (map)->filled_size ; \
	\
	if( ( array = (map)->pairs_array) == NULL) \
	{ \
		if( ( array = calloc( size , sizeof(void*))) == NULL) \
		{ \
			size = 0 ; \
		} \
		else \
		{ \
			int  __array_count ; \
			u_int  __count ; \
			\
			__array_count = 0 ; \
			for( __count = 0 ; __count < (map)->map_size ; __count ++) \
			{ \
				if( (map)->pairs[__count].is_filled) \
				{ \
					array[__array_count++] = &(map)->pairs[__count] ; \
				} \
			} \
		} \
		\
		(map)->pairs_array = array ; \
	} \
}


int  kik_map_rehash( int  hash_key , u_int  size) ;


/*
 * preparing useful hash functions.
 */

int  kik_map_hash_str( char *  key , u_int  size) ;

int  kik_map_hash_int( int  key , u_int  size) ;

int  kik_map_hash_int_fast( int  key , u_int  size) ;


/*
 * preparing useful compare functions.
 */
int  kik_map_compare_str( char *  key1 , char *  key2) ;

int  kik_map_compare_str_nocase( char *  key1 , char *  key2) ;

int  kik_map_compare_int( int  key1 , int  key2) ;


#endif
