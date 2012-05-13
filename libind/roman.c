/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/roman.table"

struct tabl *
libind_get_table(
	unsigned int *  table_size
	)
{
	*table_size = sizeof( iscii_roman_table) / sizeof( struct tabl) ;

	return  iscii_roman_table ;
}
