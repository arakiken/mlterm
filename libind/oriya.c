/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "indian.h"
#include "table/oriya.table"

struct tabl *libind_get_table(unsigned int *table_size) {
  *table_size = sizeof(iscii_oriya_table) / sizeof(struct tabl);

  return iscii_oriya_table;
}
