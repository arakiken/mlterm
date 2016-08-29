/*
 *	$Id$
 */

#include "ef_ucs4_viscii.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_viscii.c"

#else

ef_map_func(8bits, ef_map_viscii_to_ucs4, 16)

    ef_map_func(8bits, ef_map_ucs4_to_viscii, 32)

#endif
