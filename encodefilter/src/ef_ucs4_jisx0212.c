/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_jisx0212.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_jisx0212.c"

#else

ef_map_func(jajp, ef_map_jisx0212_1990_to_ucs4, 16)

    ef_map_func(jajp, ef_map_ucs4_to_jisx0212_1990, 32)

#endif
