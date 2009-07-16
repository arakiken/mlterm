/*
 *	$Id$
 */

#include  "mkf_ucs4_gbk.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_gbk.c"

#else

mkf_map_func( "mkf_zh", mkf_map_gbk_to_ucs4, 16)

mkf_map_func( "mkf_zh", mkf_map_ucs4_to_gbk, 32)

#endif
