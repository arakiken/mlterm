/*
 *	$Id$
 */

#include  "mkf_ucs4_cp1251.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_cp1251.c"

#else

mkf_map_func( "mkf_8bits", mkf_map_cp1251_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1251, 32)

#endif
