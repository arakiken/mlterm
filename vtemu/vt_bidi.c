/*
 *	$Id$
 */

#include "vt_bidi.h"

#include <string.h> /* strcmp */

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

/* Order of this table must be same as vt_bidi_mode_t. */
static char* bidi_mode_name_table[] = {
    "normal", "left", "right",
};

/* --- global functions --- */

vt_bidi_mode_t vt_get_bidi_mode(const char* name) {
  vt_bidi_mode_t mode;

  for (mode = 0; mode < BIDI_MODE_MAX; mode++) {
    if (strcmp(bidi_mode_name_table[mode], name) == 0) {
      return mode;
    }
  }

  /* default value */
  return BIDI_NORMAL_MODE;
}

char* vt_get_bidi_mode_name(vt_bidi_mode_t mode) {
  if ((u_int)mode >= BIDI_MODE_MAX) {
    /* default value */
    mode = BIDI_NORMAL_MODE;
  }

  return bidi_mode_name_table[mode];
}
