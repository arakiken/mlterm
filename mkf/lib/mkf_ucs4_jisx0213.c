/*
 *	$Id$
 */

#include  "mkf_ucs4_jisx0213.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_jisx0213.c"

#else

mkf_map_func( "mkf_jajp", mkf_map_jisx0213_2000_1_to_ucs4, 16)
mkf_map_func( "mkf_jajp", mkf_map_jisx0213_2000_2_to_ucs4, 16)

mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_jisx0213_2000_1, 32)
mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_jisx0213_2000_2, 32)

#endif
