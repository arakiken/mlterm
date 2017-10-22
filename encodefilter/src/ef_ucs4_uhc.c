/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_uhc.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_uhc.c"

#else

ef_map_func(kokr, ef_map_uhc_to_ucs4, 16)

ef_map_func(kokr, ef_map_ucs4_to_uhc, 32)

#endif
