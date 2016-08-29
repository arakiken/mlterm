/*
 *	$Id: $
 */

#include "indian.h"
#include "table/tamil.table"

struct tabl* libind_get_table(unsigned int* table_size) {
  *table_size = sizeof(iscii_tamil_table) / sizeof(struct tabl);

  return iscii_tamil_table;
}
