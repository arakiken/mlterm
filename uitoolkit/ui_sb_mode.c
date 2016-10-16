/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_sb_mode.h"

#include <string.h>
#include <pobl/bl_types.h> /* u_int */

/* --- static variables --- */

/* Order of this table must be same as ui_sb_mode_t. */
static char* sb_mode_name_table[] = {
    "none", "left", "right", "autohide",
};

/* --- global functions --- */

ui_sb_mode_t ui_get_sb_mode_by_name(char* name) {
#ifndef USE_CONSOLE
  ui_sb_mode_t mode;

  for (mode = 0; mode < SBM_MAX; mode++) {
    if (strcmp(sb_mode_name_table[mode], name) == 0) {
      return mode;
    }
  }
#endif

  /* default value */
  return SBM_NONE;
}

char* ui_get_sb_mode_name(ui_sb_mode_t mode) {
  if ((u_int)mode >= SBM_MAX) {
    /* default value */
    mode = SBM_NONE;
  }

  return sb_mode_name_table[mode];
}
