/*
 *	$Id$
 */

#include  "mkf_ucs4_jisx0208.h"

#include  "mkf_map_loader.h"


/* --- global functions --- */

#ifdef  NO_DYNAMIC_LOAD_TABLE

#include  "../libtbl/mkf_ucs4_jisx0208.c"

#else

mkf_map_func( "mkf_jajp", mkf_map_jisx0208_1983_to_ucs4, 16)
mkf_map_func( "mkf_jajp", mkf_map_jisx0208_nec_ext_to_ucs4, 16)
mkf_map_func( "mkf_jajp", mkf_map_jisx0208_necibm_ext_to_ucs4, 16)
mkf_map_func( "mkf_jajp", mkf_map_sjis_ibm_ext_to_ucs4, 16)

mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_jisx0208_1983, 32)
mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_jisx0208_nec_ext, 32)
mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_jisx0208_necibm_ext, 32)
mkf_map_func( "mkf_jajp", mkf_map_ucs4_to_sjis_ibm_ext, 32)

#endif
