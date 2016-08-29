/*
 *	$Id: $
 */

#include "indian.h"
#include "table/assamese.table"

struct tabl* libind_get_table(unsigned int* table_size) {
  *table_size = sizeof(iscii_assamese_table) / sizeof(struct tabl);

  return iscii_assamese_table;
}
