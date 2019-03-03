/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_dialog.h"

#include "bl_debug.h"

/* --- static variables --- */

static int (*callback)(bl_dialog_style_t, const char *);

/* --- global functions --- */

void bl_dialog_set_callback(int (*cb)(bl_dialog_style_t, const char *)) {
  callback = cb;
}

#if 0
int bl_dialog_set_exec_file(bl_dialog_style_t style, const char *path) { return 1; }
#endif

int bl_dialog(bl_dialog_style_t style, const char *msg) {
  int ret;

  if (callback && (ret = (*callback)(style, msg)) != -1) {
    return ret;
  } else {
    bl_msg_printf("%s\n", msg);

    return -1;
  }
}
