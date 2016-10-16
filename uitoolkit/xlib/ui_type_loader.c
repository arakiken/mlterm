/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ui_type_loader.h"

#ifndef NO_DYNAMIC_LOAD_TYPE

#include <stdio.h> /* NULL */
#include <pobl/bl_dlfcn.h>
#include <pobl/bl_debug.h>

#ifndef LIBDIR
#define TYPELIB_DIR "/usr/local/lib/mlterm/"
#else
#define TYPELIB_DIR LIBDIR "/mlterm/"
#endif

/* --- global functions --- */

void* ui_load_type_xft_func(ui_type_id_t id) {
  static void** func_table;
  static int is_tried;

  if (!is_tried) {
    bl_dl_handle_t handle;

    is_tried = 1;

    if ((!(handle = bl_dl_open(TYPELIB_DIR, "type_xft")) &&
         !(handle = bl_dl_open("", "type_xft")))) {
      bl_error_printf("xft: Could not load.\n");

      return NULL;
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Loading libtype_xft.so\n");
#endif

    bl_dl_close_at_exit(handle);

    func_table = bl_dl_func_symbol(handle, "ui_type_xft_func_table");

    if ((u_int32_t)func_table[TYPE_API_COMPAT_CHECK] != TYPE_API_COMPAT_CHECK_MAGIC) {
      bl_dl_close(handle);
      func_table = NULL;
      bl_error_printf("Incompatible type engine API.\n");

      return NULL;
    }
  }

  if (func_table) {
    return func_table[id];
  } else {
    return NULL;
  }
}

void* ui_load_type_cairo_func(ui_type_id_t id) {
  static void** func_table;
  static int is_tried;

  if (!is_tried) {
    bl_dl_handle_t handle;

    is_tried = 1;

    if ((!(handle = bl_dl_open(TYPELIB_DIR, "type_cairo")) &&
         !(handle = bl_dl_open("", "type_cairo")))) {
      bl_error_printf("cairo: Could not load.\n");

      return NULL;
    }

#ifdef DEBUG
    bl_debug_printf(BL_DEBUG_TAG " Loading libtype_cairo.so\n");
#endif

    bl_dl_close_at_exit(handle);

    func_table = bl_dl_func_symbol(handle, "ui_type_cairo_func_table");

    if ((u_int32_t)func_table[TYPE_API_COMPAT_CHECK] != TYPE_API_COMPAT_CHECK_MAGIC) {
      bl_dl_close(handle);
      func_table = NULL;
      bl_error_printf("Incompatible type engine API.\n");

      return NULL;
    }
  }

  if (func_table) {
    return func_table[id];
  } else {
    return NULL;
  }
}

#endif /* NO_DYNAMIC_LOAD_TYPE */
