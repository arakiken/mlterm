/*
 *	$Id$
 */

#include  "mkf_ucs4_cp125x.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_cp125x.c"

#else

mkf_map_func( "mkf_8bits", mkf_map_cp1250_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1251_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1252_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1253_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1254_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1255_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1256_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1257_to_ucs4, 16)

mkf_map_func( "mkf_8bits", mkf_map_cp1258_to_ucs4, 16)


mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1250, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1251, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1252, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1253, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1254, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1255, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1256, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1257, 32)

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_cp1258, 32)


#endif
