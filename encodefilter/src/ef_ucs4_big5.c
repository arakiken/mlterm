/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_big5.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_big5.c"

#else

ef_map_func(zh, ef_map_big5_to_ucs4, 16) ef_map_func(zh, ef_map_hkscs_to_ucs4, 16)

    ef_map_func(zh, ef_map_ucs4_to_big5, 32) ef_map_func(zh, ef_map_ucs4_to_hkscs, 32)

#endif
