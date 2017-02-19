/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_dlfcn.h"

#include <stdio.h>  /* NULL */
#include <string.h> /* strlen */

#include "bl_mem.h" /* alloca() */
#if defined(__CYGWIN__) || defined(__MSYS__)
#include "bl_path.h" /* bl_conv_to_win32_path */
#endif

#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0502 /* for SetDllDirectory */
#include <windows.h>
#include <winbase.h> /* SetDllDirectory */

/* --- static functions --- */

static int initialized;

/* --- global functions --- */

bl_dl_handle_t bl_dl_open(const char *dirpath, const char *name) {
  HMODULE module;
  char *path;
#if defined(__CYGWIN__) || defined(__MSYS__)
  /* MAX_PATH which is 260 (3+255+1+1) is defined in win32 alone. */
  char winpath[MAX_PATH];
#endif

#ifdef SetDllDirectory /* Defined in winbase.h as SetDllDirectoryA or \
                          SetDllDirectoryW */
  if (!initialized) {
    SetDllDirectory(""); /* Don't load library at CWD. */
    initialized = 1;
  }
#endif

  if ((path = alloca(strlen(dirpath) + strlen(name) + 8)) == NULL) {
    return NULL;
  }

#if defined(__CYGWIN__)
  sprintf(path, "%scyg%s.dll", dirpath, name);
  if (bl_conv_to_win32_path(path, winpath, sizeof(winpath)) < 0) {
    goto next_step;
  }
  path = winpath;
#elif defined(__MSYS__)
  sprintf(path, "%slib%s.dll", dirpath, name);
  if (bl_conv_to_win32_path(path, winpath, sizeof(winpath)) < 0) {
    goto next_step;
  }
  path = winpath;
#else
  sprintf(path, "%slib%s.dll", dirpath, name);

  if ((module = LoadLibraryA(path))) {
    return (bl_dl_handle_t)module;
  }

  /* Assume cygwin(-mno-cygwin) */
  sprintf(path, "%scyg%s.dll", dirpath, name);
#endif

  if ((module = LoadLibraryA(path))) {
    return (bl_dl_handle_t)module;
  }

#if defined(__CYGWIN__) || defined(__MSYS__)
next_step:
  sprintf(path, "%s%s.dll", dirpath, name);
  if (bl_conv_to_win32_path(path, winpath, sizeof(winpath)) < 0) {
    return NULL;
  }
  path = winpath;
#else
  sprintf(path, "%s%s.dll", dirpath, name);
#endif

  if ((module = LoadLibraryA(path))) {
    return (bl_dl_handle_t)module;
  }

  return NULL;
}

int bl_dl_close(bl_dl_handle_t handle) { return FreeLibrary((HMODULE)handle); }

void *bl_dl_func_symbol(bl_dl_handle_t handle, const char *symbol) {
  return GetProcAddress((HMODULE)handle, symbol);
}

int bl_dl_is_module(const char *name) {
  size_t len;

  if (!name) {
    return 0;
  }

  if ((len = strlen(name)) < 4) {
    return 0;
  }

  if (strcmp(&name[len - 4], ".dll") == 0) {
    return 1;
  }

  return 0;
}
