/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_jisx0208.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_jisx0208.c"

#else

ef_map_func(jajp, ef_map_jisx0208_1983_to_ucs4, 16)
    ef_map_func(jajp, ef_map_jisx0208_nec_ext_to_ucs4, 16)
        ef_map_func(jajp, ef_map_jisx0208_necibm_ext_to_ucs4, 16)
            ef_map_func(jajp, ef_map_sjis_ibm_ext_to_ucs4, 16)

                ef_map_func(jajp, ef_map_ucs4_to_jisx0208_1983, 32)
                    ef_map_func(jajp, ef_map_ucs4_to_jisx0208_nec_ext, 32)
                        ef_map_func(jajp, ef_map_ucs4_to_jisx0208_necibm_ext, 32)
                            ef_map_func(jajp, ef_map_ucs4_to_sjis_ibm_ext, 32)

#endif
