/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_DIALOG_H__

typedef enum {
  BL_DIALOG_OKCANCEL = 0,
  BL_DIALOG_ALERT = 1,

} bl_dialog_style_t;

int bl_dialog_set_callback(int (*callback)(bl_dialog_style_t, const char *));

int bl_dialog(bl_dialog_style_t style, const char *msg);

#endif
