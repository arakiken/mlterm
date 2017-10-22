/* -*- c-basic-offset:2; tab-width:2; indent-tabs-mode:nil -*- */

#include "ef_ucs4_iscii.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_iscii.c"

#else

ef_map_func(8bits, ef_map_ucs4_to_iscii, 32)

ef_map_func(8bits, ef_map_iscii_assamese_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_bengali_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_gujarati_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_hindi_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_kannada_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_malayalam_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_oriya_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_punjabi_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_tamil_to_ucs4, 16)
ef_map_func(8bits, ef_map_iscii_telugu_to_ucs4, 16)

#endif
