/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_tcvn5712_1.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_tcvn5712_1.c"

#else

/*
 * not compatible with ISO2022.
 * at the present time , not used.
 */

ef_map_func(8bits, ef_map_ucs4_to_tcvn5712_1_1993, 32)

ef_map_func(8bits, ef_map_tcvn5712_1_1992_to_ucs4, 16)

#endif
