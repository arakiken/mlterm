/*
 *	$Id$
 */

#include  "kik_map.h"

#include  <string.h>	/* strcmp */


/* --- global functions --- */

int
kik_map_rehash(
	int  hash_key ,
	u_int  size
	)
{
	if( ++ hash_key >= size)
	{
		return  0 ;
	}
	else
	{
		return  hash_key ;
	}
}

int
kik_map_hash_str(
	char *  key ,
	u_int  size
	)
{
	int  hash_key ;

	hash_key = 0 ;
	
	while( *key)
	{
		hash_key += *key ++ ;
	}

	return  hash_key % size ;
}

int
kik_map_hash_int(
	int  key ,
	u_int  size
	)
{
	return  key % size ;
}

int
kik_map_hash_int_fast(
	int  key ,
	u_int  size	/* == 2^n */
	)
{
	return  key & (size - 1) ;
}

int
kik_map_compare_str(
	char *  key1 ,
	char *  key2
	)
{
	return  (strcmp( key1 , key2) == 0) ;
}

int
kik_map_compare_str_nocase(
	char *  key1 ,
	char *  key2
	)
{
	return  (strcasecmp( key1 , key2) == 0) ;
}

int
kik_map_compare_int(
	int  key1 ,
	int  key2
	)
{
	return  (key1 == key2) ;
}


#ifdef  __DEBUG

#include  <stdio.h>	/* printf */

/* Macros in kik_map.h use kik_error_printf and kik_debug_printf. */
#define  kik_error_printf  printf
#define  kik_debug_printf  printf

#undef  DEFAULT_MAP_SIZE
#define DEFAULT_MAP_SIZE 2

KIK_MAP_TYPEDEF( test , int , char *) ;

int
main(void)
{
	KIK_MAP( test)  map ;
	KIK_PAIR( test)  pair ;
	int  result ;
	int  key ;
	char *  table[] = { "a" , "b" , "c" , "d" , "e" , "f" , "g" } ;

	kik_map_new_with_size( int , char * , map , kik_map_hash_int , kik_map_compare_int , 2) ;

	for( key = 0 ; key < sizeof(table) / sizeof(table[0]) ; key++)
	{
		kik_map_set( result , map , key , table[key]) ;
	}

	printf( "MAP SIZE %d / FILLED %d\n" , map->map_size , map->filled_size) ;

	for( key = 0 ; key < sizeof(table) / sizeof(table[0]) ; key++)
	{
		kik_map_get( result , map , key , pair) ;
		if( result)
		{
			printf( "%d %s\n" , key , pair->value) ;
		}
		else
		{
			printf( "The value of the key %d is not found\n" , key) ;
		}
	}

	for( key = 0 ; key < sizeof(table) / sizeof(table[0]) - 2 ; key++)
	{
		printf( "KEY %d is erased.\n" , key) ;
		kik_map_erase( result , map , key) ;
	}

	printf( "MAP SIZE %d / FILLED %d\n" , map->map_size , map->filled_size) ;

	for( key = 0 ; key < sizeof(table) / sizeof(table[0]) ; key++)
	{
		kik_map_get( result , map , key , pair) ;
		if( result)
		{
			printf( "%d %s\n" , key , pair->value) ;
		}
		else
		{
			printf( "The value of the key %d is not found\n" , key) ;
		}
	}

	kik_map_delete( map) ;

	return  1 ;
}

#endif
