/*
 *	$Id$
 */

#include "ef_ucs4_georgian_ps.h"

#include "ef_tblfunc_loader.h"

/* --- global functions --- */

#ifdef NO_DYNAMIC_LOAD_TABLE

#include "../module/ef_ucs4_georgian_ps.c"

#else

ef_map_func(8bits, ef_map_georgian_ps_to_ucs4, 16)

    ef_map_func(8bits, ef_map_ucs4_to_georgian_ps, 32)

#endif
