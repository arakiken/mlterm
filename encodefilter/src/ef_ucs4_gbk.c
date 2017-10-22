/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_gbk.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_gbk.c"

#else

ef_map_func(zh, ef_map_gbk_to_ucs4, 16)

ef_map_func(zh, ef_map_ucs4_to_gbk, 32)

#endif
