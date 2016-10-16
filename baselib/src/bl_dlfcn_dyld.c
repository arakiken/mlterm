/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_dlfcn.h"

#include <stdio.h>  /* NULL */
#include <string.h> /* strlen */

#include "bl_mem.h" /* alloca() */
#include "bl_slist.h"
#ifdef DEBUG
#include "bl_debug.h"
#endif

#include <mach-o/dyld.h>

typedef struct loaded_module {
  bl_dl_handle_t handle;
  char* dirpath;
  char* name;

  u_int ref_count;

  struct loaded_module* next;

} loaded_module_t;

/* --- static functions --- */

static loaded_module_t* module_list = NULL;

/* --- global functions --- */

bl_dl_handle_t bl_dl_open(const char* dirpath, const char* name) {
  NSObjectFileImage file_image;
  NSObjectFileImageReturnCode ret;
  loaded_module_t* module;
  bl_dl_handle_t handle;
  char* path;

  module = module_list;
  while (module) {
    if (strcmp(module->dirpath, dirpath) == 0 && strcmp(module->name, name) == 0) {
      module->ref_count++;
      return module->handle;
    }

    module = bl_slist_next(module);
  }

  if (!(module = malloc(sizeof(loaded_module_t)))) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " malloc failed.\n");
#endif
    return NULL;
  }

  module->dirpath = strdup(dirpath);
  module->name = strdup(name);
  module->ref_count = 0;

  if ((path = alloca(strlen(dirpath) + strlen(name) + 7)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif
    return NULL;
  }

  /*
   * libfoo.so --> foo.so
   */

  sprintf(path, "%slib%s.so", dirpath, name);

  if ((ret = NSCreateObjectFileImageFromFile(path, &file_image)) != NSObjectFileImageSuccess) {
    sprintf(path, "%s%s.so", dirpath, name);
    if ((ret = NSCreateObjectFileImageFromFile(path, &file_image)) != NSObjectFileImageSuccess) {
      goto error;
    }
  }

  handle = (bl_dl_handle_t)NSLinkModule(file_image, path, NSLINKMODULE_OPTION_BINDNOW);

  if (!handle) {
    goto error;
  }

  bl_slist_insert_head(module_list, module);
  module->handle = handle;
  module->ref_count++;

  return handle;

error:
  if (module) {
    free(module->dirpath);
    free(module->name);
    free(module);
  }

  return NULL;
}

int bl_dl_close(bl_dl_handle_t handle) {
  loaded_module_t* module;

  if (!module_list) {
    return 1;
  }

  module = module_list;
  while (module) {
    if (module->handle == handle) {
      module->ref_count--;

      if (module->ref_count) {
        return 0;
      }

      break;
    }

    module = bl_slist_next(module);
  }

  if (NSUnLinkModule((NSModule)module->handle, NSUNLINKMODULE_OPTION_NONE) == 0) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " NSUnLinkModule() failed.\n");
#endif
    return 1;
  }

  bl_slist_remove(module_list, module);
  free(module->dirpath);
  free(module->name);
  free(module);

  return 0;
}

void* bl_dl_func_symbol(bl_dl_handle_t unused, const char* symbol) {
  NSSymbol nssymbol = NULL;
  char* symbol_name;

  if ((symbol_name = alloca(strlen(symbol) + 2)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " alloca() failed.\n");
#endif
    return NULL;
  }

  sprintf(symbol_name, "_%s", symbol);

  if (!NSIsSymbolNameDefined(symbol_name)) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " NSIsSymbolNameDefined() failed. [symbol_name: %s]\n",
                   symbol_name);
#endif
    return NULL;
  }

  if ((nssymbol = NSLookupAndBindSymbol(symbol_name)) == NULL) {
#ifdef DEBUG
    bl_warn_printf(BL_DEBUG_TAG " NSLookupAndBindSymbol() failed. [symbol_name: %s]\n",
                   symbol_name);
#endif
    return NULL;
  }

  return NSAddressOfSymbol(nssymbol);
}

int bl_dl_is_module(const char* name) {
  size_t len;

  if (!name) {
    return 0;
  }

  if ((len = strlen(name)) < 3) {
    return 0;
  }

  if (strcmp(&name[len - 3], ".so") == 0) {
    return 1;
  }

  return 0;
}
