/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_mod_meta_mode.h"

#include <string.h> /* strcmp */

/* --- static variables --- */

/* Order of this table must be same as ui_mod_meta_mode_t. */
static char *mod_meta_mode_name_table[] = {
    "none", "esc", "8bit",
};

/* --- global functions --- */

ui_mod_meta_mode_t ui_get_mod_meta_mode_by_name(char *name) {
  ui_mod_meta_mode_t mode;

  for (mode = 0; mode < MOD_META_MODE_MAX; mode++) {
    if (strcmp(mod_meta_mode_name_table[mode], name) == 0) {
      return mode;
    }
  }

  /* default value */
  return MOD_META_NONE;
}

char *ui_get_mod_meta_mode_name(ui_mod_meta_mode_t mode) {
  if (mode < 0 || MOD_META_MODE_MAX <= mode) {
    /* default value */
    mode = MOD_META_NONE;
  }

  return mod_meta_mode_name_table[mode];
}
