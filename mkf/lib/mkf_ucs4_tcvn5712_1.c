/*
 *	$Id$
 */

#include "mkf_ucs4_tcvn5712_1.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_tcvn5712_1.c"

#else

/*
 * not compatible with ISO2022.
 * at the present time , not used.
 */

mkf_map_func( "mkf_8bits", mkf_map_ucs4_to_tcvn5712_1_1993, 32)

mkf_map_func( "mkf_8bits", mkf_map_tcvn5712_1_1992_to_ucs4, 16)

#endif
