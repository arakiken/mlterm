/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "indian.h"
#include "table/inscript.table"

struct a2i_tabl *libind_get_table(unsigned int *table_size) {
  *table_size = sizeof(isciikey_inscript_table) / sizeof(struct a2i_tabl);

  return isciikey_inscript_table;
}
