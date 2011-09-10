/*
 *	$Id$
 */

#include  "mkf_ucs4_uhc.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_uhc.c"

#else

mkf_map_func( kokr, mkf_map_uhc_to_ucs4, 16)

mkf_map_func( kokr, mkf_map_ucs4_to_uhc, 32)

#endif
