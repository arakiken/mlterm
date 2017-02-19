/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_bel_mode.h"

#include <string.h>        /* strcmp */
#include <pobl/bl_types.h> /* u_int */

/* --- static variables --- */

/* Order of this table must be same as ui_bel_mode_t. */
static char *bel_mode_name_table[] = {"none", "sound", "visual", "sound|visual"};

/* --- global functions --- */

ui_bel_mode_t ui_get_bel_mode_by_name(char *name) {
  ui_bel_mode_t mode;

  for (mode = 0; mode < BEL_MODE_MAX; mode++) {
    if (strcmp(bel_mode_name_table[mode], name) == 0) {
      return mode;
    }
  }

  /* default value */
  return BEL_SOUND;
}

char *ui_get_bel_mode_name(ui_bel_mode_t mode) {
  if ((u_int)mode >= BEL_MODE_MAX) {
    /* default value */
    mode = BEL_SOUND;
  }

  return bel_mode_name_table[mode];
}
