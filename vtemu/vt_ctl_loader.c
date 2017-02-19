/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "vt_ctl_loader.h"

#ifndef NO_DYNAMIC_LOAD_CTL

#include <stdio.h> /* NULL */
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_debug.h>

#ifndef LIBDIR
#define CTLLIB_DIR "/usr/local/lib/mlterm/"
#else
#define CTLLIB_DIR LIBDIR "/mlterm/"
#endif

/* --- global functions --- */

void *vt_load_ctl_bidi_func(vt_ctl_bidi_id_t id) {
  static void **func_table;
  static int is_tried;

  if (!is_tried) {
    bl_dl_handle_t handle;

    is_tried = 1;

    if ((!(handle = bl_dl_open(CTLLIB_DIR, "ctl_bidi")) &&
         !(handle = bl_dl_open("", "ctl_bidi")))) {
      bl_error_printf("BiDi: Could not load.\n");

      return NULL;
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Loading libctl_bidi.so\n");
#endif

    func_table = bl_dl_func_symbol(handle, "vt_ctl_bidi_func_table");

    if ((u_int32_t)func_table[CTL_BIDI_API_COMPAT_CHECK] != CTL_API_COMPAT_CHECK_MAGIC) {
      bl_dl_close(handle);
      func_table = NULL;
      bl_error_printf("Incompatible BiDi rendering API.\n");

      return NULL;
    }
  }

  if (func_table) {
    return func_table[id];
  } else {
    return NULL;
  }
}

void *vt_load_ctl_iscii_func(vt_ctl_iscii_id_t id) {
  static void **func_table;
  static int is_tried;

  if (!is_tried) {
    bl_dl_handle_t handle;

    is_tried = 1;

    if ((!(handle = bl_dl_open(CTLLIB_DIR, "ctl_iscii")) &&
         !(handle = bl_dl_open("", "ctl_iscii")))) {
      bl_error_printf("iscii: Could not load.\n");

      return NULL;
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Loading libctl_iscii.so\n");
#endif

    func_table = bl_dl_func_symbol(handle, "vt_ctl_iscii_func_table");

    if ((u_int32_t)func_table[CTL_ISCII_API_COMPAT_CHECK] != CTL_API_COMPAT_CHECK_MAGIC) {
      bl_dl_close(handle);
      func_table = NULL;
      bl_error_printf("Incompatible indic rendering API.\n");

      return NULL;
    }
  }

  if (func_table) {
    return func_table[id];
  } else {
    return NULL;
  }
}

#endif /* NO_DYNAMIC_LOAD_CTL */
