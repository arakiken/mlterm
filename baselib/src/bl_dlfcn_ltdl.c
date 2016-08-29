/*
 *	$Id$
 */

#include "bl_dlfcn.h"

#include <stdio.h>  /* NULL */
#include <string.h> /* strlen */

#include "bl_mem.h" /* alloca() */

#include <ltdl.h>

static int ready_sym_table = 0;

/* --- global functions --- */

bl_dl_handle_t bl_dl_open(const char *dirpath, const char *name) {
  lt_dlhandle handle;
  char *path;

  if (!ready_sym_table) {
    LTDL_SET_PRELOADED_SYMBOLS();
    ready_sym_table = 1;
  }

  if (lt_dlinit()) {
    return NULL;
  }

  if ((path = alloca(strlen(dirpath) + strlen(name) + 4)) == NULL) {
    return NULL;
  }

  /*
   * libfoo -> foo
   */

  sprintf(path, "%slib%s", dirpath, name);
  if ((handle = lt_dlopenext(path))) {
    return (bl_dl_handle_t)handle;
  }

  sprintf(path, "%s%s", dirpath, name);
  if ((handle = lt_dlopenext(path))) {
    return (bl_dl_handle_t)handle;
  }

  return NULL;
}

int bl_dl_close(bl_dl_handle_t handle) {
  int ret;

  ret = lt_dlclose((lt_dlhandle)handle);

  lt_dlexit();

  return ret;
}

void *bl_dl_func_symbol(bl_dl_handle_t handle, const char *symbol) {
  return lt_dlsym((lt_dlhandle)handle, symbol);
}

int bl_dl_is_module(const char *name) {
  size_t len;

  if (!name) {
    return 0;
  }

  if ((len = strlen(name)) < 3) {
    return 0;
  }

  if (strcmp(&name[len - 3], ".la") == 0) {
    return 1;
  }

  return 0;
}
