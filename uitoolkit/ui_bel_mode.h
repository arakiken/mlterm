/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __UI_BEL_MODE_H__
#define __UI_BEL_MODE_H__

typedef enum ui_bel_mode {
  BEL_NONE = 0x0,
  BEL_SOUND = 0x1,
  BEL_VISUAL = 0x2,
  /* BEL_SOUND|BEL_VISUAL */

  BEL_MODE_MAX = 0x4

} ui_bel_mode_t;

ui_bel_mode_t ui_get_bel_mode_by_name(char *name);

char *ui_get_bel_mode_name(ui_bel_mode_t mode);

#endif
