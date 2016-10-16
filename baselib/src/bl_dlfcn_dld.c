/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_dlfcn.h"

#include <stdio.h>  /* NULL */
#include <string.h> /* strlen */

#include "bl_mem.h" /* alloca() */

#include <dl.h>

/* --- global functions --- */

bl_dl_handle_t bl_dl_open(const char *dirpath, const char *name) {
  char *path;
  shl_t handle;

  if ((path = alloca(strlen(dirpath) + strlen(name) + 7)) == NULL) {
    return NULL;
  }

  /*
   * libfoo.sl --> foo.sl
   */

  sprintf(path, "%slib%s.sl", dirpath, name);
  if ((handle = shl_load(path, BIND_DEFERRED, 0x0))) {
    return (bl_dl_handle_t)handle;
  }

  sprintf(path, "%s%s.sl", dirpath, name);
  if ((handle = shl_load(path, BIND_DEFERRED, 0x0))) {
    return (bl_dl_handle_t)handle;
  }

  return NULL;
}

int bl_dl_close(bl_dl_handle_t handle) { return shl_unload((shl_t)handle); }

void *bl_dl_func_symbol(bl_dl_handle_t handle, const char *symbol) {
  void *func;

  if (shl_findsym((shl_t *)&handle, symbol, TYPE_PROCEDURE, &func) == -1) {
    return NULL;
  }

  return func;
}

int bl_dl_is_module(const char *name) {
  size_t len;

  if (!name) {
    return 0;
  }

  if ((len = strlen(name)) < 3) {
    return 0;
  }

  if (strcmp(&name[len - 3], ".sl") == 0) {
    return 1;
  }

  return 0;
}
