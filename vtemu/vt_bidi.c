/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_bidi.h"

#include <string.h> /* strcmp */
#include <pobl/bl_types.h> /* u_int */
#include <pobl/bl_util.h>  /* bl_match_str_in_table */

#if 0
#define __DEBUG
#endif

/* --- static variables --- */

/* Order of this table must be same as vt_bidi_mode_t. */
static char *bidi_mode_name_table[] = { "normal", "left", "right" };

/* --- global functions --- */

vt_bidi_mode_t vt_get_bidi_mode_by_name(const char *name) {
  int mode;

  if ((mode = bl_match_str_in_table(bidi_mode_name_table, BIDI_MODE_MAX, name)) == -1) {
    /* default value */
    return BIDI_NORMAL_MODE;
  }

  return mode;
}

char *vt_get_bidi_mode_name(vt_bidi_mode_t mode) {
  if ((u_int)mode >= BIDI_MODE_MAX) {
    /* default value */
    mode = BIDI_NORMAL_MODE;
  }

  return bidi_mode_name_table[mode];
}
