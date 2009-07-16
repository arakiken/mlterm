/*
 *	$Id$
 */

#include  "mkf_ucs4_cns11643.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_cns11643.c"

#else

mkf_map_func( "mkf_zh", mkf_map_cns11643_1992_1_to_ucs4, 16)
mkf_map_func( "mkf_zh", mkf_map_cns11643_1992_2_to_ucs4, 16)
mkf_map_func( "mkf_zh", mkf_map_cns11643_1992_3_to_ucs4, 16)

mkf_map_func( "mkf_zh", mkf_map_ucs4_to_cns11643_1992_1, 32)
mkf_map_func( "mkf_zh", mkf_map_ucs4_to_cns11643_1992_2, 32)
mkf_map_func( "mkf_zh", mkf_map_ucs4_to_cns11643_1992_3, 32)

#endif
