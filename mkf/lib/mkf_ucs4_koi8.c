/*
 *	$Id$
 */

#include  "mkf_ucs4_koi8.h"

#include  "mkf_tblfunc_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_koi8.c"

#else

mkf_map_func( 8bits, mkf_map_koi8_r_to_ucs4, 16)
mkf_map_func( 8bits, mkf_map_koi8_u_to_ucs4, 16)
mkf_map_func( 8bits, mkf_map_koi8_t_to_ucs4, 16)

mkf_map_func( 8bits, mkf_map_ucs4_to_koi8_r, 32)
mkf_map_func( 8bits, mkf_map_ucs4_to_koi8_u, 32)
mkf_map_func( 8bits, mkf_map_ucs4_to_koi8_t, 32)

#endif
