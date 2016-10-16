/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_SB_MODE_H__
#define __UI_SB_MODE_H__

typedef enum ui_sb_mode {
  SBM_NONE,
  SBM_LEFT,
  SBM_RIGHT,
  SBM_AUTOHIDE,

  SBM_MAX

} ui_sb_mode_t;

ui_sb_mode_t ui_get_sb_mode_by_name(char* name);

char* ui_get_sb_mode_name(ui_sb_mode_t mode);

#endif
