/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "bl_dlfcn.h"

#include <stdio.h>  /* NULL */

/* --- global functions --- */

/*
 * dummy codes
 */

bl_dl_handle_t bl_dl_open(const char *dirpath, const char *name) { return NULL; }

void bl_dl_close(bl_dl_handle_t handle) {}

void *bl_dl_func_symbol(bl_dl_handle_t handle, const char *symbol) { return NULL; }

int bl_dl_is_module(const char *name) { return 0; }
