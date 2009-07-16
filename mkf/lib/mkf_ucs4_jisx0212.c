/*
 *	$Id$
 */

#include  "mkf_ucs4_jisx0212.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_jisx0212.c"

#else

mkf_map_func( "mkf_jajp", mkf_map_jisx0212_1990_to_ucs4, 16)

mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_jisx0212_1990, 32)

#endif
