/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/kannada.table"

struct tabl *
libind_get_table(
	u_int *  table_size
	)
{
	*table_size = sizeof( iscii_kannada_table) / sizeof( struct tabl) ;

	return  iscii_kannada_table ;
}
