/*
 *	$Id$
 */

#include "ef_ucs4_cns11643.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_cns11643.c"

#else

ef_map_func(zh, ef_map_cns11643_1992_1_to_ucs4, 16)
    ef_map_func(zh, ef_map_cns11643_1992_2_to_ucs4, 16)
        ef_map_func(zh, ef_map_cns11643_1992_3_to_ucs4, 16)

            ef_map_func(zh, ef_map_ucs4_to_cns11643_1992_1, 32)
                ef_map_func(zh, ef_map_ucs4_to_cns11643_1992_2, 32)
                    ef_map_func(zh, ef_map_ucs4_to_cns11643_1992_3, 32)

#endif
