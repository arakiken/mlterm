/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/roman.table"

struct tabl *
libind_get_table(
	u_int *  table_size
	)
{
	*table_size = sizeof( iscii_roman_table) / sizeof( struct tabl) ;

	return  iscii_roman_table ;
}
