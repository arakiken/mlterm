/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_MOD_META_MODE_H__
#define __UI_MOD_META_MODE_H__

typedef enum ui_mod_meta_mode {
  MOD_META_NONE = 0x0,
  MOD_META_OUTPUT_ESC,
  MOD_META_SET_MSB,

  MOD_META_MODE_MAX

} ui_mod_meta_mode_t;

ui_mod_meta_mode_t ui_get_mod_meta_mode_by_name(char* name);

char* ui_get_mod_meta_mode_name(ui_mod_meta_mode_t mode);

#endif
