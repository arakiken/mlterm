/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_koi8.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_koi8.c"

#else

ef_map_func(8bits, ef_map_koi8_r_to_ucs4, 16) ef_map_func(8bits, ef_map_koi8_u_to_ucs4, 16)
ef_map_func(8bits, ef_map_koi8_t_to_ucs4, 16)

ef_map_func(8bits, ef_map_ucs4_to_koi8_r, 32)
ef_map_func(8bits, ef_map_ucs4_to_koi8_u, 32)
ef_map_func(8bits, ef_map_ucs4_to_koi8_t, 32)

#endif
