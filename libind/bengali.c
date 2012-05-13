/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/bengali.table"

struct tabl *
libind_get_table(
	unsigned int *  table_size
	)
{
	*table_size = sizeof( iscii_bengali_table) / sizeof( struct tabl) ;

	return  iscii_bengali_table ;
}
