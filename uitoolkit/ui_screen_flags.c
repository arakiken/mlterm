/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_screen_flags.h"

#include <string.h> /* strcmp */
#include <pobl/bl_types.h> /* u_int */
#include <pobl/bl_util.h> /* bl_match_str_in_table */

/* --- static variables --- */

/* Order of this table must be same as ui_mod_meta_mode_t. */
static char *mod_meta_mode_name_table[] = { "none", "esc", "8bit" };

static char *dnd_escape_mode_name_table[] = { "none", "backslash", "quote", "doublequote" };

/* --- global functions --- */

ui_mod_meta_mode_t ui_get_mod_meta_mode_by_name(const char *name) {
  int mode;

  if ((mode = bl_match_str_in_table(mod_meta_mode_name_table, MOD_META_MODE_MAX, name)) == -1) {
    /* default value */
    return MOD_META_NONE;
  }

  return mode;
}

char *ui_get_mod_meta_mode_name(ui_mod_meta_mode_t mode) {
  if ((u_int)mode >= MOD_META_MODE_MAX) {
    /* default value */
    mode = MOD_META_NONE;
  }

  return mod_meta_mode_name_table[mode];
}

ui_dnd_escape_mode_t ui_get_dnd_escape_mode_by_name(const char *name) {
  int mode;

  if ((mode = bl_match_str_in_table(dnd_escape_mode_name_table, DND_ESCAPE_MODE_MAX, name)) == -1) {
    /* default value */
    return DND_ESCAPE_BACKSLASH;
  }

  return mode;
}

char *ui_get_dnd_escape_mode_name(ui_dnd_escape_mode_t mode) {
  if ((u_int)mode >= DND_ESCAPE_MODE_MAX) {
    /* default value */
    mode = DND_ESCAPE_NONE;
  }

  return dnd_escape_mode_name_table[mode];
}
