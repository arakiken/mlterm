/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_jisx0213.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_jisx0213.c"

#else

ef_map_func(jajp, ef_map_jisx0213_2000_1_to_ucs4, 16)
    ef_map_func(jajp, ef_map_jisx0213_2000_2_to_ucs4, 16)

        ef_map_func(jajp, ef_map_ucs4_to_jisx0213_2000_1, 32)
            ef_map_func(jajp, ef_map_ucs4_to_jisx0213_2000_2, 32)

#endif
