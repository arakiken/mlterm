/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __BL_DLFCN_H__
#define __BL_DLFCN_H__

#include "bl_def.h"

typedef void* bl_dl_handle_t;

bl_dl_handle_t bl_dl_open(const char* dirpath, const char* name);

int bl_dl_close(bl_dl_handle_t handle);

void* bl_dl_func_symbol(bl_dl_handle_t handle, const char* symbol);

int bl_dl_is_module(const char* name);

int bl_dl_close_at_exit(bl_dl_handle_t handle);

void bl_dl_close_all(void);

#endif
