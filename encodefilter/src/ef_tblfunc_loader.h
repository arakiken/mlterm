/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#ifndef __EF_TBLFUNC_LOADER_H__
#define __EF_TBLFUNC_LOADER_H__

#include <pobl/bl_types.h>
#include <pobl/bl_dlfcn.h>

#include "ef_char.h"

#ifndef LIBDIR
#define MEFLIB_DIR "/usr/local/lib/mef/"
#else
#define MEFLIB_DIR LIBDIR "/mef/"
#endif

#ifdef DLFCN_NONE
#ifndef NO_DYNAMIC_LOAD_TABLE
#define NO_DYNAMIC_LOAD_TABLE
#endif
#endif

#ifndef NO_DYNAMIC_LOAD_TABLE

#define ef_map_func(libname, funcname, bits)                                    \
  int funcname(ef_char_t *ch, u_int##bits##_t ucscode) {                        \
    static int (*_##funcname)(ef_char_t *, u_int##bits##_t);                    \
    if (!_##funcname && !(_##funcname = ef_load_##libname##_func(#funcname))) { \
      return 0;                                                                  \
    }                                                                            \
    return (*_##funcname)(ch, ucscode);                                          \
  }

#define ef_map_func2(libname, funcname)                                         \
  int funcname(ef_char_t *dst_ch, ef_char_t *src_ch) {                         \
    static int (*_##funcname)(ef_char_t *, ef_char_t *);                       \
    if (!_##funcname && !(_##funcname = ef_load_##libname##_func(#funcname))) { \
      return 0;                                                                  \
    }                                                                            \
    return (*_##funcname)(dst_ch, src_ch);                                       \
  }

void *ef_load_8bits_func(const char *symname);

void *ef_load_jajp_func(const char *symname);

void *ef_load_kokr_func(const char *symname);

void *ef_load_zh_func(const char *symname);

#endif /* NO_DYNAMIC_LOAD_TABLE */

#endif
