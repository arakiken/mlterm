/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/punjabi.table"

struct tabl *
libind_get_table(
	unsigned int *  table_size
	)
{
	*table_size = sizeof( iscii_punjabi_table) / sizeof( struct tabl) ;

	return  iscii_punjabi_table ;
}
