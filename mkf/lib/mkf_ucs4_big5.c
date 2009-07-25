/*
 *	$Id$
 */

#include  "mkf_ucs4_big5.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_big5.c"

#else

mkf_map_func( "mkf_zh", mkf_map_big5_to_ucs4, 16)
mkf_map_func( "mkf_zh", mkf_map_hkscs_to_ucs4, 16)

mkf_map_func( "mkf_zh", mkf_map_ucs4_to_big5, 32)
mkf_map_func( "mkf_zh", mkf_map_ucs4_to_hkscs, 32)

#endif
