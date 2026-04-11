/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_bel_mode.h"

#include <string.h>        /* strcmp */
#include <pobl/bl_types.h> /* u_int */
#include <pobl/bl_util.h>  /* bl_match_str_in_table */

/* --- static variables --- */

/* Order of this table must be same as ui_bel_mode_t. */
static char *bel_mode_name_table[] = { "none", "sound", "visual", "sound|visual" };

/* --- global functions --- */

ui_bel_mode_t ui_get_bel_mode_by_name(const char *name) {
  int mode;

  if ((mode = bl_match_str_in_table(bel_mode_name_table, BEL_MODE_MAX, name)) == -1) {
    /* default value */
    return BEL_SOUND;
  }

  return mode;
}

char *ui_get_bel_mode_name(ui_bel_mode_t mode) {
  if ((u_int)mode >= BEL_MODE_MAX) {
    /* default value */
    mode = BEL_SOUND;
  }

  return bel_mode_name_table[mode];
}
