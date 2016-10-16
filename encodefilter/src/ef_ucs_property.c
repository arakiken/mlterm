/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs_property.h"

#ifndef REMOVE_PROPERTY_TABLE
#include "table/ef_ucs_property.table"
#endif

/* '(i) | 0x2' is done in order for the result of BIT_SHIFT_32() to be over 0.
 */
#define BIT_SHIFT_32(i) ((((i) | 0x2) >> 1) & 0x7fffffff)
#define DEFAULT_INTERVAL BIT_SHIFT_32(sizeof(ucs_property_table) / sizeof(ucs_property_table[0]))

#if 0
#define SELF_TEST
#endif

#ifdef SELF_TEST
static int debug_count;
#endif

/* --- global functions --- */

ef_property_t ef_get_ucs_property(u_int32_t ucs) {
#ifndef REMOVE_PROPERTY_TABLE
  u_int32_t idx;
  u_int32_t interval;

  interval = DEFAULT_INTERVAL;
  idx = interval;

#ifdef SELF_TEST
  debug_count = 0;
#endif

  while (1) {
#ifdef SELF_TEST
    debug_count++;
#endif

    if (ucs < ucs_property_table[idx].first) {
      /*
       * If idx == 0, 'ucs < ucs_property_table[idx].first'
       * is always false because ucs_property_table[0].first
       * is 0. So following 'idx - 1' is never minus value.
       */
      if (ucs_property_table[idx - 1].last < ucs) {
        return 0;
      } else {
        interval = BIT_SHIFT_32(interval);
      }
      idx -= interval;
    } else if (ucs_property_table[idx].last < ucs) {
      /*
       * If idx == max value
       * ( sizeof(ucs_property_table)/sizeof(ucs_property_table[0]) ),
       * 'ucs_property_table[idx].last < ucs' is always false because
       * ucs_property_table[max].last is 0xffffffff.
       * So following 'idx + 1' is never over max value.
       */
      if (ucs < ucs_property_table[idx + 1].first) {
        return 0;
      } else {
        interval = BIT_SHIFT_32(interval);
      }
      idx += interval;
    } else {
      return ucs_property_table[idx].prop;
    }
  }
#else
  return 0;
#endif
}

#ifdef SELF_TEST
int main(void) {
  u_int32_t ucs;

  for (ucs = 0; ucs <= 0x10ffff; ucs++) {
    ef_property_t prop = ef_get_ucs_property(ucs);

    printf("UCS %x => PROP %x (Loop %d)\n", ucs, prop, debug_count);
  }
}
#endif
