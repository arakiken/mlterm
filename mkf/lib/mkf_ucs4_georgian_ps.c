/*
 *	$Id$
 */

#include  "mkf_ucs4_georgian_ps.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_georgian_ps.c"

#else

mkf_map_func( "mkf_8bits", mkf_map_georgian_ps_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_georgian_ps, 32)

#endif
