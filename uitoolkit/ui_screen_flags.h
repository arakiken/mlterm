/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

/* Flags (enums) for ui_screen.c */

#ifndef __UI_SCREEN_FLAGS_H__
#define __UI_SCREEN_FLAGS_H__

typedef enum ui_mod_meta_mode {
  MOD_META_NONE = 0x0,
  MOD_META_OUTPUT_ESC,
  MOD_META_SET_MSB,

  MOD_META_MODE_MAX

} ui_mod_meta_mode_t;

typedef enum ui_dnd_escape_mode {
  DND_ESCAPE_NONE = 0x0,
  DND_ESCAPE_BACKSLASH,
  DND_ESCAPE_QUOTE,
  DND_ESCAPE_DOUBLE_QUOTE,

  DND_ESCAPE_MODE_MAX

} ui_dnd_escape_mode_t;

ui_mod_meta_mode_t ui_get_mod_meta_mode_by_name(const char *name);

char *ui_get_mod_meta_mode_name(ui_mod_meta_mode_t mode);

ui_dnd_escape_mode_t ui_get_dnd_escape_mode_by_name(const char *name);

char *ui_get_dnd_escape_mode_name(ui_dnd_escape_mode_t mode);

#endif
