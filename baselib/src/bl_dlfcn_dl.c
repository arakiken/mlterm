/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_dlfcn.h"

#include <stdio.h>  /* NULL */
#include <string.h> /* strlen */

#include "bl_mem.h" /* alloca() */

#include <dlfcn.h>

/* --- global functions --- */

bl_dl_handle_t bl_dl_open(const char *dirpath, const char *name) {
  char *path;
  void *ret;

  if ((path = alloca(strlen(dirpath) + strlen(name) + 7)) == NULL) {
    return NULL;
  }

  /*
   * libfoo.so --> foo.so --> libfoo.sl --> foo.sl
   */

  sprintf(path, "%slib%s.so", dirpath, name);
  if ((ret = dlopen(path, RTLD_LAZY))) {
    return (bl_dl_handle_t)ret;
  }

#ifndef __APPLE__
  sprintf(path, "%slib%s.sl", dirpath, name);
  if ((ret = dlopen(path, RTLD_LAZY))) {
    return (bl_dl_handle_t)ret;
  }

  sprintf(path, "%s%s.so", dirpath, name);
  if ((ret = dlopen(path, RTLD_LAZY))) {
    return (bl_dl_handle_t)ret;
  }

  sprintf(path, "%s%s.sl", dirpath, name);
  if ((ret = dlopen(path, RTLD_LAZY))) {
    return (bl_dl_handle_t)ret;
  }
#else
  {
    const char *p;

    /* XXX Hack */
    if ((strcmp((p = dirpath + strlen(dirpath) - 4), "mef/") == 0 ||
         strcmp((p -= 3), "mlterm/") == 0) &&
        (path = alloca(21 + strlen(p) + 3 + strlen(name) + 3 + 1))) {
      sprintf(path, "@executable_path/lib/%slib%s.so", p, name);
    } else {
      return NULL;
    }

    if ((ret = dlopen(path, RTLD_LAZY))) {
      return (bl_dl_handle_t)ret;
    }
  }
#endif

  return NULL;
}

int bl_dl_close(bl_dl_handle_t handle) { return dlclose(handle); }

void *bl_dl_func_symbol(bl_dl_handle_t handle, const char *symbol) { return dlsym(handle, symbol); }

int bl_dl_is_module(const char *name) {
  size_t len;

  if (!name) {
    return 0;
  }

  if ((len = strlen(name)) < 3) {
    return 0;
  }

  if (strcmp(&name[len - 3], ".so") == 0 || strcmp(&name[len - 3], ".sl") == 0) {
    return 1;
  }

  return 0;
}
