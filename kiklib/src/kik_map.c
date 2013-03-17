/*
 *	$Id$
 */

#include  "kik_map.h"

#include  <string.h>	/* strcmp */


/* --- global functions --- */

int
kik_map_rehash(
	int  hash_key ,
	u_int  pair_size
	)
{
	return  (hash_key + 1) % pair_size ;
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
