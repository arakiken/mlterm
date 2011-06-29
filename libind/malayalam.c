/*
 *	$Id: $
 */

#include  "indian.h"
#include  "table/malayalam.table"

struct tabl *
libind_get_table(
	u_int *  table_size
	)
{
	*table_size = sizeof( iscii_malayalam_table) / sizeof( struct tabl) ;

	return  iscii_malayalam_table ;
}
